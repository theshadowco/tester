#include "httpServer.h"
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

HTTPServer::HTTPServer () : Extender ( L"httpServer" ), waiting ( false ) {
	init ();
	addProcedure ( L"Start", L"Старт", 2,
								 [ & ] ( tVariant* Params ) { return start ( Params ); } );
	addProcedure ( L"Stop", L"Стоп", 0, [ & ] ( tVariant* ) {
		stop ();
		return true;
	} );
	addProcedure ( L"Send", L"Послать", 1, [ & ] ( tVariant* Params ) {
		send ( Params );
		return true;
	} );
}

void HTTPServer::init () {
	server.new_task_queue = [] { return new httplib::ThreadPool ( 1, 1 ); };
	server.set_keep_alive_max_count ( 1 );
	server.Post ( "/", [ this ] ( const httplib::Request& request,
																httplib::Response& response ) {
		{
			std::lock_guard<std::mutex> lock ( router );
			if ( waiting ) {
				response.status = 503;
				response.set_content ( "Runner is busy", "text/plain" );
				return;
			}
			waiting = true;
		}

		auto body = Chars::StringToWide ( request.body );
		auto eventData = Chars::ToWCHAR ( body.c_str () );
		getBaseConnector ()->ExternalEvent ( getExtensionID (), u"",
																				 eventData.get () );

		std::unique_lock<std::mutex> lock ( router );
		const auto ready =
				condition.wait_for ( lock, std::chrono::seconds ( 120 ),
														 [ & ] { return data.has_value (); } );
		auto result = ready ? *data : "Runner is busy";
		waiting = false;
		data.reset ();
		response.set_content ( result, "text/plain" );
	} );
}

HTTPServer::~HTTPServer () {
	stop ();
}

bool HTTPServer::start ( tVariant* Params ) {
	std::string host = Chars::WideToString (
			Chars::WCHARToWide ( Params->pwstrVal, Params->wstrLen ) );
	Params++;
	int port = Params->intVal;
	if ( auto error = startServer ( host, port ) ) {
		SetError ( error->message );
		return false;
	}
	return true;
}

HTTPServer::BindError HTTPServer::lastError () {
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

std::optional<HTTPServer::BindError> HTTPServer::startServer ( std::string host,
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
			error.message =
					"cpp-lib bind_to_port failed (no OS error code available)";
		}
		return error;
	}
	worker = std::jthread ( [ this ] { server.listen_after_bind (); } );
	server.wait_until_ready ();
	if ( server.is_running () ) {
		return std::nullopt;
	}
	return lastError ();
}

void HTTPServer::stop () {
	{
		std::lock_guard lock ( router );
		if ( waiting && !data.has_value () ) {
			data = "server stopped";
		}
	}
	condition.notify_one ();

	std::lock_guard lock ( mutex );
	server.stop ();
	if ( worker.joinable () ) {
		worker.join ();
	}
}

void HTTPServer::send ( tVariant* Params ) {
	{
		std::lock_guard lock ( router );
		if ( !waiting ) {
			return;
		}
		data = Chars::WideToString (
				Chars::WCHARToWide ( Params->pwstrVal, Params->wstrLen ) );
	}
	condition.notify_one ();
}
