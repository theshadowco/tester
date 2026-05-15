#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
using json = nlohmann::json;
using namespace std::chrono_literals;

constexpr auto protocolVersion = "2025-11-25";
constexpr auto waitTimeoutEnv = "TESTER_MCP_WAIT_TIMEOUT_SECONDS";

std::string getenvOr ( const char* name, const std::string& fallback ) {
	const auto value = std::getenv ( name );
	if ( value == nullptr || *value == '\0' ) {
		return fallback;
	}
	return value;
}

int getenvIntOr ( const char* name, int fallback ) {
	const auto value = std::getenv ( name );
	if ( value == nullptr || *value == '\0' ) {
		return fallback;
	}
	try {
		return std::stoi ( value );
	} catch ( const std::exception& ) {
		return fallback;
	}
}

std::filesystem::path scriptPath ( const char* name ) {
	return std::filesystem::path ( TESTER_SOURCE_DIR ) / "tests" / "files" /
				 "tester.tests" / name;
}

class ChildProcess {
public:
	ChildProcess ( const char* executable, std::vector<std::string> arguments ) {
		pid = fork ();
		if ( pid < 0 ) {
			throw std::runtime_error ( "fork failed" );
		}
		if ( pid == 0 ) {
			setpgid ( 0, 0 );
			std::vector<char*> argv;
			argv.push_back ( const_cast<char*> ( executable ) );
			for ( auto& argument : arguments ) {
				argv.push_back ( argument.data () );
			}
			argv.push_back ( nullptr );
			execv ( executable, argv.data () );
			_exit ( 127 );
		}
		processGroup = pid;
		setpgid ( pid, processGroup );
	}

	ChildProcess ( const ChildProcess& ) = delete;
	ChildProcess& operator= ( const ChildProcess& ) = delete;

	~ChildProcess () {
		terminate ();
	}

	bool running () {
		if ( pid <= 0 ) {
			return false;
		}
		int status = 0;
		const auto result = waitpid ( pid, &status, WNOHANG );
		if ( result == 0 ) {
			return true;
		}
		if ( result == pid ) {
			pid = -1;
		}
		return false;
	}

	void terminate () {
		if ( processGroup <= 0 ) {
			return;
		}
		kill ( -processGroup, SIGTERM );
		for ( auto attempt = 0; attempt < 20; ++attempt ) {
			running ();
			if ( kill ( -processGroup, 0 ) != 0 ) {
				processGroup = -1;
				return;
			}
			std::this_thread::sleep_for ( 100ms );
		}
		kill ( -processGroup, SIGKILL );
		for ( auto attempt = 0; attempt < 20; ++attempt ) {
			running ();
			if ( kill ( -processGroup, 0 ) != 0 ) {
				processGroup = -1;
				return;
			}
			std::this_thread::sleep_for ( 50ms );
		}
		processGroup = -1;
	}

private:
	pid_t pid { -1 };
	pid_t processGroup { -1 };
};

class MCPClient {
public:
	MCPClient ( std::string host, int port )
			: client ( std::move ( host ), port ) {
		client.set_connection_timeout ( 1, 0 );
		client.set_read_timeout ( 30, 0 );
		client.set_write_timeout ( 5, 0 );
	}

	void initialize () {
		const auto response =
				post ( { { "jsonrpc", "2.0" },
								 { "id", nextID () },
								 { "method", "initialize" },
								 { "params",
									 { { "protocolVersion", protocolVersion },
										 { "capabilities", json::object () },
										 { "clientInfo",
											 { { "name", "tester.sys MCP integration test" },
												 { "version", "1.0.0" } } } } } },
							 false );
		if ( response.body.contains ( "error" ) ) {
			throw std::runtime_error ( response.body.dump () );
		}
		sessionID = response.sessionID;
		if ( sessionID.empty () ) {
			throw std::runtime_error (
					"initialize response did not include MCP session" );
		}
		post ( { { "jsonrpc", "2.0" }, { "method", "notifications/initialized" } },
					 true );
	}

	json callTool ( const std::string& name, json arguments = json::object () ) {
		return post ( { { "jsonrpc", "2.0" },
										{ "id", nextID () },
										{ "method", "tools/call" },
										{ "params",
											{ { "name", name }, { "arguments", arguments } } } },
									true )
				.body;
	}

	json executeScript ( const std::filesystem::path& path ) {
		return callTool ( "execute_script", { { "script_path", path.string () } } );
	}

	json runnerStatus () {
		return callTool ( "runner_status" );
	}

private:
	struct RPCResponse {
		json body;
		std::string sessionID;
	};

	int nextID () {
		return ++requestID;
	}

	RPCResponse post ( const json& body, bool includeSession ) {
		httplib::Headers headers;
		headers.emplace ( "Accept", "application/json" );
		if ( includeSession ) {
			headers.emplace ( "MCP-Session-Id", sessionID );
			headers.emplace ( "MCP-Protocol-Version", protocolVersion );
		}
		const auto result =
				client.Post ( "/mcp", headers, body.dump (), "application/json" );
		if ( !result ) {
			throw std::runtime_error ( "MCP HTTP request failed" );
		}
		if ( result->status != 200 && result->status != 202 ) {
			throw std::runtime_error ( "Unexpected MCP HTTP status " +
																 std::to_string ( result->status ) + ": " +
																 result->body );
		}
		RPCResponse response;
		response.sessionID = result->get_header_value ( "MCP-Session-Id" );
		response.body =
				result->body.empty () ? json::object () : json::parse ( result->body );
		return response;
	}

	httplib::Client client;
	std::string sessionID;
	int requestID { 0 };
};

MCPClient waitForMCPServer ( const std::string& host, int port ) {
	const auto deadline = std::chrono::steady_clock::now () + 45s;
	std::string lastError;
	while ( std::chrono::steady_clock::now () < deadline ) {
		try {
			MCPClient client ( host, port );
			client.initialize ();
			return client;
		} catch ( const std::exception& error ) {
			lastError = error.what ();
			std::this_thread::sleep_for ( 500ms );
		}
	}
	throw std::runtime_error ( "MCP server did not become ready: " + lastError );
}

json structuredContent ( const json& response ) {
	REQUIRE_FALSE ( response.contains ( "error" ) );
	const auto& result = response.at ( "result" );
	if ( result.contains ( "structuredContent" ) ) {
		return result.at ( "structuredContent" );
	}
	const auto text =
			result.at ( "content" ).at ( 0 ).at ( "text" ).get<std::string> ();
	return json::parse ( text );
}

json waitForRunnerStatus ( MCPClient& client, const std::string& expected,
													 std::chrono::seconds timeout ) {
	const auto deadline = std::chrono::steady_clock::now () + timeout;
	json last = json::object ();
	while ( std::chrono::steady_clock::now () < deadline ) {
		last = structuredContent ( client.runnerStatus () );
		if ( last.value ( "status", "" ) == expected ) {
			return last;
		}
		std::this_thread::sleep_for ( 250ms );
	}
	FAIL ( "Timed out waiting for runner_status="
				 << expected << ", last status was " << last.dump () );
	return last;
}

bool responseErrorContains ( const json& response, const std::string& text ) {
	if ( !response.contains ( "error" ) ) {
		return false;
	}
	return response.at ( "error" ).dump ().find ( text ) != std::string::npos;
}

class RunnerFixture {
public:
	RunnerFixture ()
			: process ( "/opt/1cv8/common/1cestart",
									{ "/IBName", "Tester, MCP testing", "/N", "Administrator",
										"/TESTMANAGER" } ) {
	}

	~RunnerFixture () {
		if ( client != nullptr ) {
			try {
				client->executeScript ( scriptPath ( "close.bsl" ) );
			} catch ( const std::exception& ) {
			}
		}
	}

	ChildProcess process;
	std::unique_ptr<MCPClient> client;
};
}

TEST_SUITE ( "mcp_runner" ) {
	TEST_CASE (
			"runner_status reports busy and consumes completed script result" ) {
		const auto runner = std::filesystem::path ( "/opt/1cv8/common/1cestart" );
		if ( !std::filesystem::exists ( runner ) ) {
			MESSAGE ( "Skipping MCP Runner integration test: " << runner
																												 << " does not exist" );
			return;
		}

		const auto waitingScript = scriptPath ( "waiting.bsl" );
		const auto closeScript = scriptPath ( "close.bsl" );
		REQUIRE ( std::filesystem::exists ( waitingScript ) );
		REQUIRE ( std::filesystem::exists ( closeScript ) );

		setenv ( waitTimeoutEnv, "2", 1 );
		RunnerFixture fixture;
		std::this_thread::sleep_for ( 3s );
		const auto host = getenvOr ( "TESTER_MCP_HOST", "localhost" );
		const auto port = getenvIntOr ( "TESTER_MCP_PORT", 8080 );
		fixture.client =
				std::make_unique<MCPClient> ( waitForMCPServer ( host, port ) );

		const auto first = fixture.client->executeScript ( waitingScript );
		REQUIRE ( responseErrorContains ( first, "runner_status" ) );

		const auto second = fixture.client->executeScript ( waitingScript );
		REQUIRE ( responseErrorContains ( second, "runner_status" ) );

		const auto busy = waitForRunnerStatus ( *fixture.client, "busy", 3s );
		CHECK ( busy.at ( "working" ) == true );

		const auto completed =
				waitForRunnerStatus ( *fixture.client, "completed", 60s );
		CHECK ( completed.at ( "working" ) == false );
		REQUIRE ( completed.contains ( "result" ) );
		CHECK ( completed.at ( "result" ).dump ().find ( "test passed" ) !=
						std::string::npos );

		const auto idle = structuredContent ( fixture.client->runnerStatus () );
		CHECK ( idle.at ( "working" ) == false );
		CHECK ( idle.at ( "status" ) == "idle" );
	}
}
