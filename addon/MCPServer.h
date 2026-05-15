#pragma once
#include "extender.h"
#include "types.h"
// X11 headers define macros that break httplib's enum names.
#ifdef Success
#undef Success
#endif
#ifdef None
#undef None
#endif
#include <condition_variable>
#include <httplib.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

class MCPServer : public Extender {
public:
	MCPServer ();
	~MCPServer () override;

private:
	struct Call1C;

	struct BindError {
		enum class Source { none, invalidServer, socket };
		Source source { Source::none };
		int code { 0 };
		std::string message;
	};

	struct Session {
		std::string version;
	};

	struct ToolReply {
		bool isError { false };
		nlohmann::json payload, id;
	};

	struct RunnerStatus {
		bool working { false };
		std::string status { "idle" };
		std::optional<ToolReply> reply;
	};

	struct PendingCall {
		nlohmann::json start ();
		const WCHAR_T* keepPayload ( std::u16string payload );
		void reset ();
		ToolReply wait ();
		bool pushResult ( ToolReply result );
		RunnerStatus consumeStatus ();
		bool readyForNewCall ();
		bool working ();

		nlohmann::json id = nullptr;
		std::optional<ToolReply> reply;
		std::u16string payload;
		std::mutex mutex;
		std::condition_variable condition;
		bool waitingResult { false };
		bool timedOut { false };
	};

	struct MCPContext {
		MCPContext ( const httplib::Request& request, httplib::Response& response );
		const httplib::Request& request;
		httplib::Response& response;
		nlohmann::json body, id;
		std::string method, sessionID, version;
		std::shared_ptr<const Session> session;
	};

	class InvalidRPCRequest : public std::invalid_argument {
	public:
		InvalidRPCRequest ( const std::string& message,
												nlohmann::json requestID = nullptr );
		nlohmann::json id;
	};

	static constexpr auto sessionHeader = "MCP-Session-Id";
	static constexpr auto legacySessionHeader = "Mcp-Session-Id";
	static constexpr auto protocolHeader = "MCP-Protocol-Version";
	static constexpr auto mcpEventMessage = u"MCP";
	static constexpr auto latestVersion = "2025-11-25";
	static constexpr auto fallbackProtocolVersion = "2025-03-26";

	static bool versionSupported ( std::string_view version );
	static std::string adjustVersion ( const std::string& version );
	static std::string createSessionID ();
	static std::string authority ( const std::string& url );
	static bool localhost ( const std::string& host );
	bool isOriginAllowed ( const httplib::Request& request ) const;
	void emptyAnswer ( httplib::Response& response,
										 httplib::StatusCode code ) const;
	void answer ( httplib::Response& response, const nlohmann::json& data,
								httplib::StatusCode code = httplib::StatusCode::OK_200 ) const;
	void insertSession ( MCPContext& context ) const;
	void rpcError ( MCPContext& context, int code, const std::string& message,
									const nlohmann::json& id = nullptr,
									httplib::StatusCode httpCode = httplib::StatusCode::OK_200,
									const nlohmann::json& data = nullptr ) const;
	void rpcPackage ( MCPContext& context, const nlohmann::json& id,
										const nlohmann::json& result ) const;
	void parseRPC ( MCPContext& context ) const;
	std::string requestSessionID ( const httplib::Request& request ) const;
	const nlohmann::json& getParams ( const MCPContext& context ) const;
	static std::string getField ( const nlohmann::json& object,
																const char* name );
	void loadSession ( MCPContext& context );
	std::string initializeVersion ( const MCPContext& context ) const;
	static nlohmann::json initializeResult ( const std::string& version );
	Session createSession ( const std::string& version ) const;
	void initializeMCP ( MCPContext& context );
	nlohmann::json listToolsResult () const;
	static nlohmann::json toolArguments ( const nlohmann::json& params );
	static ToolReply getToolResult ( const std::string& body );
	static std::string replyErrorMessage ( const nlohmann::json& error );
	static int replyErrorCode ( const nlohmann::json& error );
	static nlohmann::json replyErrorData ( const nlohmann::json& error );
	void call1C ( MCPContext& context, const std::string& name,
								const nlohmann::json& args );
	void callTool ( MCPContext& context );
	void handleMCPPost ( const httplib::Request& request,
											 httplib::Response& response );
	void handleMCPGet ( const httplib::Request& request,
											httplib::Response& response );
	void handleMCPDelete ( const httplib::Request& request,
												 httplib::Response& response );

	httplib::Server server;
	std::mutex mutex;
	std::jthread worker;
	types::SafeMap<std::string, Session> sessions;
	PendingCall pendingCall;
	void init ();
	bool start ( tVariant* Params );
	bool send ( tVariant* Params );
	BindError lastError ();
	std::optional<BindError> startServer ( std::string host, int port );
	void stop ();
};
