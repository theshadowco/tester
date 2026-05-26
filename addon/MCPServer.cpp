#include "MCPServer.h"
#include "corestrings.h"
#include "types.h"
#ifdef __linux__
#include <X11/Xlib.h>
#endif
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <system_error>
#include <thread>
#include <utf8.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace {
std::chrono::seconds pendingCallTimeout () {
	constexpr auto defaultTimeout = std::chrono::seconds ( 120 );
	const auto raw = std::getenv ( "TESTER_MCP_WAIT_TIMEOUT_SECONDS" );
	if ( raw == nullptr ) {
		return defaultTimeout;
	}
	char* end = nullptr;
	errno = 0;
	const auto seconds = std::strtol ( raw, &end, 10 );
	if ( errno != 0 || end == raw || *end != '\0' || seconds <= 0 ) {
		return defaultTimeout;
	}
	return std::chrono::seconds ( seconds );
}

const char* runnerInstructions () {
	return "The Runner executes one 1C:Enterprise command at a time. "
				 "Do not call execute_script in parallel with another Runner action. "
				 "If execute_script returns \"Runner is working. Use runner_status "
				 "to check completion.\", or if the MCP client times out, do not "
				 "replay the same script. Poll runner_status until it is no longer "
				 "busy. A completed or failed status includes and consumes the saved "
				 "result of the original execute_script call. If runner_status says "
				 "there is a completed result, retrieve it with runner_status before "
				 "starting another execute_script call. Use a separate read-only "
				 "synchronization script only after the pending result has been "
				 "consumed, and only when the returned result does not already "
				 "provide the current UI state.";
}
}

nlohmann::json MCPServer::PendingCall::start () {
	std::lock_guard lock ( mutex );
	id = strings::getUnique ( 16 );
	reply.reset ();
	payload.clear ();
	timedOut = false;
	waitingResult = true;
	return id;
};

const WCHAR_T* MCPServer::PendingCall::keepPayload ( std::u16string value ) {
	std::lock_guard lock ( mutex );
	payload = std::move ( value );
	return payload.c_str ();
}

void MCPServer::PendingCall::reset () {
	{
		std::lock_guard lock ( mutex );
		id = nullptr;
		reply.reset ();
		payload.clear ();
		waitingResult = false;
		timedOut = false;
	}
	condition.notify_all ();
}

MCPServer::ToolReply MCPServer::PendingCall::wait () {
	ToolReply result;
	{
		std::unique_lock lock ( mutex );
		const auto ready =
				condition.wait_for ( lock, pendingCallTimeout (), [ & ] {
					return reply.has_value () && reply->id == id;
			} );
		if ( !ready ) {
			timedOut = true;
			throw std::runtime_error (
					"Runner is working. Use runner_status to check completion." );
		}
		result = std::move ( *reply );
	}
	reset ();
	return result;
}

bool MCPServer::PendingCall::pushResult ( ToolReply result ) {
	bool accepted = false;
	{
		std::lock_guard lock ( mutex );
		if ( !id.is_null () && ( result.id.is_null () || result.id == id ) ) {
			result.id = id;
			reply = std::move ( result );
			waitingResult = false;
			accepted = true;
		}
	}
	if ( accepted ) {
		condition.notify_all ();
	}
	return accepted;
}

MCPServer::RunnerStatus MCPServer::PendingCall::consumeStatus () {
	RunnerStatus status;
	std::lock_guard lock ( mutex );
	if ( waitingResult ) {
		status.working = true;
		status.status = "busy";
		return status;
	}
	if ( timedOut && reply.has_value () ) {
		status.status = reply->isError ? "failed" : "completed";
		status.reply = std::move ( *reply );
		id = nullptr;
		reply.reset ();
		payload.clear ();
		timedOut = false;
		return status;
	}
	return status;
}

bool MCPServer::PendingCall::readyForNewCall () {
	std::lock_guard lock ( mutex );
	return !waitingResult && !( timedOut && reply.has_value () );
}

struct MCPServer::Call1C {
	Call1C ( MCPServer& server, MCPContext& context, const std::string& name,
					 const nlohmann::json& args )
			: server ( server ), context ( context ), name ( name ), args ( args ),
				lock ( server.mutex, std::try_to_lock ) {
		if ( !lock.owns_lock () ) {
			throw std::runtime_error (
					"Runner is working. Use runner_status to check completion." );
		}
		if ( server.pendingCall.working () ) {
			throw std::runtime_error (
					"Runner is working. Use runner_status to check completion." );
		}
		if ( !server.pendingCall.readyForNewCall () ) {
			throw std::runtime_error (
					"Runner has a completed result. Use runner_status to retrieve it." );
		}
	}

	void operator() () {
		const auto message = nlohmann::json {
				{ "id", server.pendingCall.start () },
				{ "name", name },
				{ "args", args },
		}
																		 .dump ();
		std::u16string body;
		utf8::utf8to16 ( message.begin (), message.end (),
										std::back_inserter ( body ) );
		const auto payload = server.pendingCall.keepPayload ( std::move ( body ) );
		auto connector = server.getBaseConnector ();
		if ( connector == nullptr ||
				 !connector->ExternalEvent ( server.getExtensionID (), mcpEventMessage,
																		 payload ) ) {
			server.pendingCall.reset ();
			throw std::runtime_error (
					"ExternalEvent dispatch failed: "
					"could not send message to 1C:Enterprise 8.3. Please try again." );
		}
		auto result = server.pendingCall.wait ();
		if ( result.isError ) {
			server.rpcError ( context, server.replyErrorCode ( result.payload ),
												replyErrorMessage ( result.payload ), context.id,
												httplib::StatusCode::OK_200,
												replyErrorData ( result.payload ) );
		} else {
			server.rpcPackage ( context, context.id, result.payload );
		}
	}

	MCPServer& server;
	MCPContext& context;
	const std::string& name;
	const nlohmann::json& args;
	std::unique_lock<std::mutex> lock;
};

MCPServer::MCPContext::MCPContext ( const httplib::Request& request,
																		httplib::Response& response )
		: request ( request ), response ( response ) {
}

MCPServer::InvalidRPCRequest::InvalidRPCRequest ( const std::string& message,
																									nlohmann::json requestID )
		: std::invalid_argument ( message ), id ( std::move ( requestID ) ) {
}

bool MCPServer::versionSupported ( std::string_view version ) {
	return version == latestVersion || version == fallbackProtocolVersion;
}

std::string MCPServer::adjustVersion ( const std::string& version ) {
	if ( versionSupported ( version ) ) {
		return version;
	}
	return std::string ( latestVersion );
}

std::string MCPServer::createSessionID () {
	return "mcp_" + strings::getUnique ( 32 );
}

std::string MCPServer::authority ( const std::string& url ) {
	const auto protocolEnd = url.find ( "://" );
	if ( protocolEnd == std::string::npos ) {
		return {};
	}
	auto start = protocolEnd + 3;
	auto end = url.find ( '/', start );
	if ( end == std::string::npos ) {
		end = url.size ();
	}
	return url.substr ( start, end - start );
}

bool MCPServer::localhost ( const std::string& host ) {
	return strings::contains ( host, "localhost", "127.0.0.1", "[::1]", "::1" );
}

bool MCPServer::isOriginAllowed ( const httplib::Request& request ) const {
	const auto origin = strings::trim ( request.get_header_value ( "Origin" ) );
	if ( origin.empty () ) {
		return true;
	}
	const auto originAuthority = authority ( origin );
	const auto host = strings::trim ( request.get_header_value ( "Host" ) );
	if ( originAuthority.empty () || host.empty () ) {
		return false;
	}
	return originAuthority == host ||
				 ( localhost ( originAuthority ) && localhost ( host ) );
}

void MCPServer::emptyAnswer ( httplib::Response& response,
															httplib::StatusCode code ) const {
	response.status = code;
	response.set_content ( "", "application/json" );
}

void MCPServer::answer ( httplib::Response& response,
												 const nlohmann::json& data,
												 httplib::StatusCode code ) const {
	response.status = code;
	response.set_content ( data.dump (), "application/json" );
}

void MCPServer::insertSession ( MCPContext& context ) const {
	if ( !context.sessionID.empty () ) {
		context.response.set_header ( sessionHeader, context.sessionID );
	}
	if ( !context.version.empty () ) {
		context.response.set_header ( protocolHeader, context.version );
	}
}

void MCPServer::rpcError ( MCPContext& context, int code,
													 const std::string& message, const nlohmann::json& id,
													 httplib::StatusCode httpCode,
													 const nlohmann::json& data ) const {
	nlohmann::json error;
	error [ "code" ] = code;
	error [ "message" ] = message;
	if ( !data.is_null () ) {
		error [ "data" ] = data;
	}
	nlohmann::json response;
	response [ "jsonrpc" ] = "2.0";
	if ( !id.is_null () ) {
		response [ "id" ] = id;
	}
	response [ "error" ] = error;
	answer ( context.response, response, httpCode );
}

void MCPServer::rpcPackage ( MCPContext& context, const nlohmann::json& id,
														 const nlohmann::json& result ) const {
	nlohmann::json response;
	response [ "jsonrpc" ] = "2.0";
	response [ "id" ] = id;
	response [ "result" ] = result;
	answer ( context.response, response );
}

void MCPServer::parseRPC ( MCPContext& context ) const {
	context.body = nlohmann::json::parse ( context.request.body );
	if ( !context.body.is_object () ||
			 context.body.value ( "jsonrpc", "" ) != "2.0" ) {
		throw InvalidRPCRequest ( "Invalid JSON-RPC request" );
	}
	const auto rpcID = context.body.find ( "id" );
	if ( rpcID != context.body.end () ) {
		const auto& value = *rpcID;
		if ( !( value.is_string () || value.is_number () || value.is_null () ) ) {
			throw InvalidRPCRequest ( "Invalid JSON-RPC request" );
		}
		context.id = value;
	}
	const auto methodField = context.body.find ( "method" );
	if ( methodField == context.body.end () || !methodField->is_string () ) {
		throw InvalidRPCRequest ( "Invalid JSON-RPC request", context.id );
	}
	context.method = methodField->get<std::string> ();
	if ( context.method.empty () ) {
		throw InvalidRPCRequest ( "Invalid JSON-RPC request", context.id );
	}
}

std::string
MCPServer::requestSessionID ( const httplib::Request& request ) const {
	auto sessionID = strings::trim ( request.get_header_value ( sessionHeader ) );
	if ( sessionID.empty () ) {
		sessionID =
				strings::trim ( request.get_header_value ( legacySessionHeader ) );
	}
	return sessionID;
}

const nlohmann::json& MCPServer::getParams ( const MCPContext& context ) const {
	auto params = context.body.find ( "params" );
	if ( params == context.body.end () ) {
		throw std::invalid_argument ( "Missing params" );
	}
	if ( !params->is_object () ) {
		throw std::invalid_argument ( "params must be an object" );
	}
	return *params;
}

std::string MCPServer::getField ( const nlohmann::json& object,
																	const char* name ) {
	const auto field = object.find ( name );
	if ( field == object.end () ) {
		throw std::invalid_argument ( std::string ( name ) + " is required" );
	}
	if ( !field->is_string () ) {
		throw std::invalid_argument ( std::string ( name ) + " must be a string" );
	}
	return field->get<std::string> ();
}

void MCPServer::loadSession ( MCPContext& context ) {
	context.sessionID = requestSessionID ( context.request );
	if ( context.sessionID.empty () ) {
		throw std::invalid_argument ( "Missing MCP-Session-Id header" );
	}
	context.session = sessions.get ( context.sessionID );
	if ( !context.session ) {
		throw std::runtime_error ( "Unknown MCP session" );
	}
	const auto requestVersion =
			strings::trim ( context.request.get_header_value ( protocolHeader ) );
	if ( requestVersion.empty () ) {
		context.version = context.session->version;
		return;
	}
	if ( !versionSupported ( requestVersion ) ) {
		throw std::invalid_argument ( "Unsupported MCP protocol version" );
	}
	if ( requestVersion != context.session->version ) {
		throw std::invalid_argument ( "MCP protocol version mismatch" );
	}
	context.version = requestVersion;
}

std::string MCPServer::initializeVersion ( const MCPContext& context ) const {
	const auto& params = getParams ( context );
	return adjustVersion ( getField ( params, "protocolVersion" ) );
}

nlohmann::json MCPServer::initializeResult ( const std::string& version ) {
	nlohmann::json result;
	result [ "protocolVersion" ] = version;
	result [ "capabilities" ] = { { "tools", { { "listChanged", false } } } };
	result [ "serverInfo" ] = { { "name", "tester" },
															{ "version", "1.0.0" },
															{ "description",
																"MCP tools for interactive communication with "
																"1C:Enterprise 8.3 applications" } };
	result [ "instructions" ] = runnerInstructions ();
	return result;
}

MCPServer::Session
MCPServer::createSession ( const std::string& version ) const {
	Session data;
	data.version = version;
	return data;
}

void MCPServer::initializeMCP ( MCPContext& context ) {
	try {
		context.version = initializeVersion ( context );
		auto session = createSession ( context.version );
		context.sessionID = createSessionID ();
		sessions.insert ( context.sessionID, session );
		rpcPackage ( context, context.id, initializeResult ( context.version ) );
		insertSession ( context );
	} catch ( const std::exception& error ) {
		context.sessionID.clear ();
		context.version.clear ();
		rpcError ( context, -32602, error.what (), context.id );
	}
}

nlohmann::json MCPServer::listToolsResult () const {
	nlohmann::json tools = nlohmann::json::array ();
	tools.push_back (
			{ { "name", "execute_script" },
				{ "description",
					"Execute a 1C:Enterprise 8.3 BSL script file through the local "
					"Runner service. Pass exactly one argument: the absolute "
					"filesystem path to an existing .bsl file. The Runner is "
					"synchronous and can execute only one command at a time, so do "
					"not call this tool in parallel with other Runner actions. If "
					"this tool reports that the Runner is still working, or the MCP "
					"client times out, do not replay the same state-changing script. "
					"Poll runner_status until it is no longer busy. If it returns "
					"completed or failed, consume that saved result and continue from "
					"the actual state reported there. "
					"Run a read-only synchronization script such as "
					"GetActiveWindowControls() only after runner_status is no longer "
					"busy and only if no fresh UI state was returned." },
				{ "inputSchema",
					{ { "type", "object" },
						{ "properties",
							{ { "script_path",
									{ { "type", "string" },
										{ "title", "BSL Script Path" },
										{ "description",
												"Absolute path to the .bsl script file to execute. "
												"The file must already exist and use the .bsl "
												"extension. Example: "
												"/full/path/to/script.bsl" },
											{ "minLength", 1 },
											{ "examples", nlohmann::json::array (
																				{ "/full/path/to/script.bsl" } ) } } } } },
						{ "required", { "script_path" } },
						{ "additionalProperties", false } } } } );
	tools.push_back (
			{ { "name", "runner_status" },
				{ "description",
					"Check whether the local Runner service is currently executing a "
					"command. This tool is read-only and accepts no arguments. After "
					"execute_script reports that the Runner is still working, poll "
					"this tool until status is completed or failed; that response "
					"contains and consumes the saved result or error from the original "
					"script. If status is idle, there is no pending saved result." },
				{ "inputSchema",
					{ { "type", "object" },
						{ "properties", nlohmann::json::object () },
						{ "additionalProperties", false } } },
				{ "outputSchema",
					{ { "type", "object" },
							{ "properties",
								{ { "working", { { "type", "boolean" } } },
									{ "status",
										{ { "type", "string" },
											{ "enum",
												nlohmann::json::array (
														{ "busy", "idle", "completed", "failed" } ) } } },
									{ "result", nlohmann::json::object () },
									{ "error", nlohmann::json::object () } } },
							{ "required", nlohmann::json::array ( { "working", "status" } ) },
							{ "additionalProperties", false } } },
				{ "annotations", { { "readOnlyHint", true } } } } );
	return { { "tools", tools } };
}

nlohmann::json MCPServer::toolArguments ( const nlohmann::json& params ) {
	const auto arguments = params.find ( "arguments" );
	if ( arguments == params.end () ) {
		return nlohmann::json::object ();
	}
	if ( !arguments->is_object () ) {
		throw std::invalid_argument ( "tools/call arguments must be an object" );
	}
	return *arguments;
}

MCPServer::ToolReply MCPServer::getToolResult ( const std::string& body ) {
	const auto parsed = nlohmann::json::parse ( body );
	if ( !parsed.is_object () ) {
		throw std::invalid_argument ( "MCP reply payload must be a JSON object" );
	}
	ToolReply result;
	const auto id = parsed.find ( "id" );
	if ( id == parsed.end () || id->is_null () ) {
		result.id = nullptr;
	} else if ( id->is_array () || id->is_object () ) {
		throw std::invalid_argument ( "MCP reply payload id must be scalar" );
	} else {
		result.id = *id;
	}
	const auto response = parsed.find ( "result" );
	if ( response != parsed.end () ) {
		result.payload = *response;
		return result;
	}
	const auto error = parsed.find ( "error" );
	if ( error != parsed.end () ) {
		result.isError = true;
		result.payload = *error;
		return result;
	}
	result.payload = {
			{ "content",
				nlohmann::json::array ( { { { "type", "text" },
																	 { "text", parsed.dump () } } } ) },
			{ "structuredContent", parsed },
			{ "isError", parsed.value ( "success", true ) == false },
	};
	return result;
}

std::string MCPServer::replyErrorMessage ( const nlohmann::json& error ) {
	if ( error.is_object () ) {
		const auto message = error.find ( "message" );
		if ( message != error.end () && message->is_string () ) {
			return message->get<std::string> ();
		}
	}
	return "Tool execution failed";
}

int MCPServer::replyErrorCode ( const nlohmann::json& error ) {
	if ( error.is_object () ) {
		const auto code = error.find ( "code" );
		if ( code != error.end () && code->is_number_integer () ) {
			return code->get<int> ();
		}
	}
	return -32000;
}

nlohmann::json MCPServer::replyErrorData ( const nlohmann::json& error ) {
	if ( error.is_object () ) {
		const auto data = error.find ( "data" );
		if ( data != error.end () ) {
			return *data;
		}
	}
	return nullptr;
}

void MCPServer::callTool ( MCPContext& context ) {
	const auto& params = getParams ( context );
	const auto name = getField ( params, "name" );
	const auto args = toolArguments ( params );
	if ( name == "execute_script" ) {
		Call1C ( *this, context, name, args ) ();
	} else if ( name == "runner_status" ) {
		if ( !args.empty () ) {
			throw std::invalid_argument ( "runner_status accepts no arguments" );
		}
		const auto status = pendingCall.consumeStatus ();
		nlohmann::json structured =
				{ { "working", status.working }, { "status", status.status } };
		if ( status.reply.has_value () ) {
			structured [ status.reply->isError ? "error" : "result" ] =
					status.reply->payload;
		}
		rpcPackage ( context, context.id,
								 { { "content",
										 nlohmann::json::array ( { { { "type", "text" },
																							 { "text", structured.dump () } } } ) },
									 { "structuredContent", structured },
									 { "isError", false } } );
	} else {
		throw std::invalid_argument ( "Unknown tool" );
	}
}

void MCPServer::handleMCPPost ( const httplib::Request& request,
																httplib::Response& response ) {
	MCPContext context ( request, response );
	if ( !isOriginAllowed ( request ) ) {
		rpcError ( context, -32000, "Invalid Origin", nullptr,
							 httplib::StatusCode::Forbidden_403 );
		return;
	}
	try {
		parseRPC ( context );
	} catch ( const nlohmann::json::parse_error& error ) {
		rpcError ( context, -32700, error.what (), nullptr,
							 httplib::StatusCode::BadRequest_400 );
		return;
	} catch ( const InvalidRPCRequest& error ) {
		rpcError ( context, -32600, error.what (), error.id,
							 httplib::StatusCode::BadRequest_400 );
		return;
	}
	if ( context.method == "initialize" ) {
		initializeMCP ( context );
		return;
	}
	try {
		loadSession ( context );
	} catch ( const std::invalid_argument& error ) {
		rpcError ( context, -32000, error.what (), context.id,
							 httplib::StatusCode::BadRequest_400 );
		insertSession ( context );
		return;
	} catch ( const std::runtime_error& error ) {
		rpcError ( context, -32000, error.what (), context.id,
							 httplib::StatusCode::NotFound_404 );
		return;
	}
	try {
		if ( context.id.is_null () ||
				 context.method == "notifications/initialized" ) {
			emptyAnswer ( response, httplib::StatusCode::Accepted_202 );
		} else if ( context.method == "ping" ) {
			rpcPackage ( context, context.id, nlohmann::json::object () );
		} else if ( context.method == "tools/list" ) {
			rpcPackage ( context, context.id, listToolsResult () );
		} else if ( context.method == "tools/call" ) {
			callTool ( context );
		} else {
			rpcError ( context, -32601, "Method not found", context.id );
		}
	} catch ( const std::invalid_argument& error ) {
		rpcError ( context, -32602, error.what (), context.id );
	} catch ( const std::exception& error ) {
		rpcError ( context, -32000, error.what (), context.id );
	}
	insertSession ( context );
}

void MCPServer::handleMCPGet ( const httplib::Request& request,
															 httplib::Response& response ) {
	const auto status = isOriginAllowed ( request )
													? httplib::StatusCode::MethodNotAllowed_405
													: httplib::StatusCode::Forbidden_403;
	emptyAnswer ( response, status );
}

void MCPServer::handleMCPDelete ( const httplib::Request& request,
																	httplib::Response& response ) {
	if ( !isOriginAllowed ( request ) ) {
		emptyAnswer ( response, httplib::StatusCode::Forbidden_403 );
		return;
	}
	const auto sessionID = requestSessionID ( request );
	if ( sessionID.empty () ) {
		emptyAnswer ( response, httplib::StatusCode::BadRequest_400 );
		return;
	}
	if ( !sessions.has ( sessionID ) ) {
		emptyAnswer ( response, httplib::StatusCode::NotFound_404 );
		return;
	}
	sessions.remove ( sessionID );
	emptyAnswer ( response, httplib::StatusCode::NoContent_204 );
}

MCPServer::MCPServer () : Extender ( L"MCPServer" ) {
	init ();
	addProcedure ( L"Start", L"Старт", 2,
								 [ & ] ( tVariant* Params ) { return start ( Params ); } );
	addProcedure ( L"Stop", L"Стоп", 0, [ & ] ( tVariant* ) {
		stop ();
		return true;
	} );
	addProcedure ( L"Send", L"Послать", 1,
								 [ & ] ( tVariant* Params ) { return send ( Params ); } );
}

void MCPServer::init () {
	server.Post ( "/mcp", [ this ] ( const httplib::Request& request,
																	 httplib::Response& response ) {
		handleMCPPost ( request, response );
	} );
	server.Get ( "/mcp", [ this ] ( const httplib::Request& request,
																	httplib::Response& response ) {
		handleMCPGet ( request, response );
	} );
	server.Delete ( "/mcp", [ this ] ( const httplib::Request& request,
																		 httplib::Response& response ) {
		handleMCPDelete ( request, response );
	} );
}

MCPServer::~MCPServer () {
	stop ();
}

bool MCPServer::start ( tVariant* Params ) {
	std::string host;
	if ( !readString ( Params [ 0 ], host, "host" ) ) {
		return false;
	}
	Params++;
	int port = Params->intVal;
	if ( auto error = startServer ( host, port ) ) {
		SetError ( error->message );
		return false;
	}
	return true;
}

MCPServer::BindError MCPServer::lastError () {
#ifdef _WIN32
	int code = WSAGetLastError ();
	std::error_code ec ( code, std::system_category () );
	return { BindError::Source::socket, code, ec.message () };
#else
	auto code = errno;
	std::error_code error ( code, std::generic_category () );
	return { BindError::Source::socket, code, error.message () };
#endif
}

std::optional<MCPServer::BindError> MCPServer::startServer ( std::string host,
																														 int port ) {
	std::lock_guard lock ( mutex );
	if ( worker.joinable () ) {
		return BindError { BindError::Source::none, 0, "already running" };
	}
	if ( !server.is_valid () ) {
		return BindError { BindError::Source::invalidServer, 0,
											 "server is not valid (SSL cert/key?)" };
	}
	if ( !server.bind_to_port ( host, port ) ) {
		auto error = lastError ();
		if ( error.code == 0 ) {
			error.message = "bind_to_port failed (no OS error code available)";
		}
		return error;
	}
	worker = std::jthread ( [ this ] {
		try {
			server.listen_after_bind ();
		} catch ( const std::exception& error ) {
			SetError ( error.what () );
		} catch ( ... ) {
			SetError ( "Unhandled MCP server listener error" );
		}
	} );
	server.wait_until_ready ();
	if ( server.is_running () ) {
		return std::nullopt;
	}
	return lastError ();
}

void MCPServer::stop () {
	std::lock_guard lock ( mutex );
	server.stop ();
	if ( worker.joinable () ) {
		worker.join ();
	}
	sessions.clear ();
	pendingCall.reset ();
}

bool MCPServer::send ( tVariant* Params ) {
	ToolReply result;
	std::string body;
	if ( !readString ( Params [ 0 ], body, "payload" ) ) {
		return false;
	}
	try {
		result = getToolResult ( body );
	} catch ( const std::exception& error ) {
		ToolReply failed;
		failed.isError = true;
		failed.payload = { { "code", -32000 }, { "message", error.what () } };
		if ( pendingCall.pushResult ( std::move ( failed ) ) ) {
			return true;
		}
		SetError ( error.what () );
		return false;
	}
	if ( pendingCall.pushResult ( result ) ) {
		return true;
	}
	SetError ( "MCP reply does not match the active Runner call" );
	return false;
}

bool MCPServer::PendingCall::working () {
	std::lock_guard lock ( mutex );
	return waitingResult;
}
