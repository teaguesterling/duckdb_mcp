#include "protocol/mcp_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb_mcp_logging.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <climits>
#include <cstring>
#include <sys/stat.h>
#endif

namespace duckdb {

StdioTransport::StdioTransport(const StdioConfig &config)
    : config(config), connected(false), process_pid(-1), stdin_fd(-1), stdout_fd(-1), stderr_fd(-1) {
}

StdioTransport::~StdioTransport() {
	if (connected) {
		Disconnect();
	}
}

bool StdioTransport::Connect() {
	lock_guard<mutex> lock(io_mutex);

	if (connected) {
		MCP_LOG_DEBUG("TRANSPORT", "Already connected to %s", config.command_path);
		return true;
	}

	MCP_LOG_INFO("TRANSPORT", "Connecting to MCP server: %s", config.command_path);

	if (!StartProcess()) {
		MCP_LOG_ERROR("TRANSPORT", "Failed to start MCP server process: %s", config.command_path);
		return false;
	}

	connected = true;
	MCP_LOG_INFO("TRANSPORT", "Successfully connected to MCP server: %s", config.command_path);
	return true;
}

void StdioTransport::Disconnect() {
	lock_guard<mutex> lock(io_mutex);

	if (!connected) {
		return;
	}

	MCP_LOG_INFO("TRANSPORT", "Disconnecting from MCP server: %s", config.command_path);
	StopProcess();
	connected = false;
	MCP_LOG_DEBUG("TRANSPORT", "Disconnected from MCP server: %s", config.command_path);
}

bool StdioTransport::IsConnected() const {
	return connected && IsProcessRunning();
}

void StdioTransport::Send(const MCPMessage &message) {
	if (!IsConnected()) {
		MCP_LOG_ERROR("TRANSPORT", "Attempted to send message when not connected to %s", config.command_path);
		throw IOException("Transport not connected");
	}

	try {
		string json = message.ToJSON();
		MCP_LOG_PROTOCOL(true, config.command_path, json);

		MCP_PERF_TIMER("mcp_send", config.command_path);
		WriteToProcess(json + "\n");
	} catch (const std::exception &e) {
		MCP_LOG_ERROR("TRANSPORT", "Failed to send message to %s: %s", config.command_path, e.what());
		throw;
	}
}

MCPMessage StdioTransport::Receive() {
	if (!IsConnected()) {
		MCP_LOG_ERROR("TRANSPORT", "Attempted to receive message when not connected to %s", config.command_path);
		throw IOException("Transport not connected");
	}

	try {
		MCP_PERF_TIMER("mcp_receive", config.command_path);
		string response = ReadFromProcess();

		MCP_LOG_PROTOCOL(false, config.command_path, response);

		return MCPMessage::FromJSON(response);
	} catch (const std::exception &e) {
		MCP_LOG_ERROR("TRANSPORT", "Failed to receive message from %s: %s", config.command_path, e.what());
		throw;
	}
}

MCPMessage StdioTransport::SendAndReceive(const MCPMessage &message) {
	Send(message);
	return Receive();
}

bool StdioTransport::Ping() {
	if (!IsConnected()) {
		return false;
	}

	try {
		auto ping_msg = MCPMessage::CreateRequest(MCPMethods::PING, Value(), Value::BIGINT(1));
		auto response = SendAndReceive(ping_msg);
		return response.IsResponse() && !response.IsError();
	} catch (...) {
		return false;
	}
}

string StdioTransport::GetConnectionInfo() const {
	return "stdio://" + config.command_path + " (pid: " + std::to_string(process_pid) + ")";
}

bool StdioTransport::StartProcess() {
#ifdef _WIN32
	// Windows process creation not implemented yet
	throw NotImplementedException("Stdio transport not supported on Windows yet");
#else
	int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];

	// Helper lambda: create a pipe and set O_CLOEXEC on both ends.
	// Uses pipe2() on Linux (atomic), falls back to pipe()+fcntl on macOS.
	auto create_pipe = [](int pipefd[2]) -> bool {
#ifdef __linux__
		return pipe2(pipefd, O_CLOEXEC) == 0;
#else
		if (pipe(pipefd) == -1) {
			return false;
		}
		fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
		fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
		return true;
#endif
	};

	// Create pipes with O_CLOEXEC to prevent FD leaks to child processes
	if (!create_pipe(stdin_pipe)) {
		return false;
	}

	if (!create_pipe(stdout_pipe)) {
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		return false;
	}

	if (!create_pipe(stderr_pipe)) {
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		return false;
	}

	process_pid = fork();

	if (process_pid == -1) {
		// Fork failed
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		close(stderr_pipe[0]);
		close(stderr_pipe[1]);
		return false;
	}

	if (process_pid == 0) {
		// Child process

		// Redirect stdin, stdout, stderr
		dup2(stdin_pipe[0], STDIN_FILENO);
		dup2(stdout_pipe[1], STDOUT_FILENO);
		dup2(stderr_pipe[1], STDERR_FILENO);

		// Close all FDs > STDERR to prevent leaking parent's file descriptors
		// (pipe2 O_CLOEXEC handles the pipe FDs, but other inherited FDs need closing)
		long max_fd = sysconf(_SC_OPEN_MAX);
		if (max_fd < 0) {
			max_fd = 1024; // Reasonable default
		}
		for (int fd = STDERR_FILENO + 1; fd < max_fd; fd++) {
			close(fd);
		}

		// Set working directory
		if (!config.working_directory.empty()) {
			if (chdir(config.working_directory.c_str()) != 0) {
				_exit(127);
			}
		}

		// Build a sanitized environment instead of inheriting the parent's.
		// Only pass through a minimal safe set of variables plus user-configured ones.
		vector<string> env_strings;

		// Safe passthrough variables from parent environment
		static const char *safe_vars[] = {"HOME", "USER", "LANG", "TZ", "PATH", "TERM", "SHELL", nullptr};
		for (const char **var = safe_vars; *var != nullptr; var++) {
			const char *val = getenv(*var);
			if (val) {
				env_strings.push_back(string(*var) + "=" + val);
			}
		}

		// Dangerous environment variable keys that must be blocked
		// (even if user tries to set them via config.environment)
		static const char *blocked_keys[] = {"LD_PRELOAD",
		                                     "LD_LIBRARY_PATH",
		                                     "LD_AUDIT",
		                                     "DYLD_INSERT_LIBRARIES",
		                                     "DYLD_LIBRARY_PATH",
		                                     "DYLD_FRAMEWORK_PATH",
		                                     nullptr};

		// Add user-supplied environment variables (from config), blocking dangerous ones
		for (const auto &env_pair : config.environment) {
			bool is_blocked = false;
			for (const char **bk = blocked_keys; *bk != nullptr; bk++) {
				if (env_pair.first == *bk) {
					is_blocked = true;
					break;
				}
			}
			if (!is_blocked) {
				env_strings.push_back(env_pair.first + "=" + env_pair.second);
			}
		}

		// Build envp array for execve
		vector<char *> envp;
		for (auto &s : env_strings) {
			envp.push_back(const_cast<char *>(s.c_str()));
		}
		envp.push_back(nullptr);

		// Prepare arguments
		vector<char *> args;
		args.push_back(const_cast<char *>(config.command_path.c_str()));
		for (const auto &arg : config.arguments) {
			args.push_back(const_cast<char *>(arg.c_str()));
		}
		args.push_back(nullptr);

		// Resolve command path: execve does not search PATH, so we must resolve it manually
		string resolved_command = config.command_path;
		if (!config.command_path.empty() && config.command_path[0] != '/') {
			// Search PATH for the command
			const char *path_env = getenv("PATH");
			if (path_env) {
				string path_str(path_env);
				size_t start = 0;
				bool found = false;
				while (start < path_str.size()) {
					size_t end = path_str.find(':', start);
					if (end == string::npos) {
						end = path_str.size();
					}
					string dir = path_str.substr(start, end - start);
					string candidate = dir + "/" + config.command_path;
					struct stat st;
					if (stat(candidate.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
						resolved_command = candidate;
						found = true;
						break;
					}
					start = end + 1;
				}
				if (!found) {
					_exit(127); // Command not found
				}
			}
		}

		// Execute command with sanitized environment
		execve(resolved_command.c_str(), args.data(), envp.data());

		// If we get here, exec failed
		_exit(127);
	}

	// Parent process

	// Close child ends of pipes
	close(stdin_pipe[0]);
	close(stdout_pipe[1]);
	close(stderr_pipe[1]);

	// Store parent ends
	stdin_fd = stdin_pipe[1];
	stdout_fd = stdout_pipe[0];
	stderr_fd = stderr_pipe[0];

	// Set non-blocking mode
	fcntl(stdout_fd, F_SETFL, O_NONBLOCK);
	fcntl(stderr_fd, F_SETFL, O_NONBLOCK);

	// Give the child process a moment to start and potentially fail
	usleep(100000); // 100ms

	// Check if process is still running (if it failed quickly, it might be dead)
	if (!IsProcessRunning()) {
		return false;
	}

	return true;
#endif
}

void StdioTransport::StopProcess() {
#ifdef _WIN32
	// Windows process termination not implemented yet
#else
	if (process_pid > 0) {
		// Close file descriptors
		if (stdin_fd >= 0) {
			close(stdin_fd);
			stdin_fd = -1;
		}
		if (stdout_fd >= 0) {
			close(stdout_fd);
			stdout_fd = -1;
		}
		if (stderr_fd >= 0) {
			close(stderr_fd);
			stderr_fd = -1;
		}

		// Send SIGTERM for graceful shutdown
		kill(process_pid, SIGTERM);

		// Wait briefly for process to exit gracefully
		usleep(100000); // 100ms

		int status;
		int wait_result = waitpid(process_pid, &status, WNOHANG);
		if (wait_result == 0) {
			// Process still running after SIGTERM - force kill
			kill(process_pid, SIGKILL);
			// Blocking wait to reap the zombie
			waitpid(process_pid, &status, 0);
		}

		process_pid = -1;
	}
#endif
}

bool StdioTransport::IsProcessRunning() const {
#ifdef _WIN32
	return false; // Windows process checking not implemented yet
#else
	if (process_pid <= 0) {
		return false;
	}

	int status;
	int result = waitpid(process_pid, &status, WNOHANG);

	return result == 0; // Process is still running
#endif
}

void StdioTransport::WriteToProcess(const string &data) {
#ifdef _WIN32
	throw NotImplementedException("Stdio transport not supported on Windows yet");
#else
	if (stdin_fd < 0) {
		throw IOException("Process stdin not available");
	}

	ssize_t written = write(stdin_fd, data.c_str(), data.length());
	if (written != static_cast<ssize_t>(data.length())) {
		throw IOException("Failed to write to process stdin");
	}
#endif
}

string StdioTransport::ReadFromProcess() {
#ifdef _WIN32
	throw NotImplementedException("Stdio transport not supported on Windows yet");
#else
	if (stdout_fd < 0) {
		throw IOException("Process stdout not available");
	}

	if (!WaitForData()) {
		throw IOException("Timeout waiting for process response");
	}

	char buffer[4096];
	string result;

	while (true) {
		ssize_t bytes_read = read(stdout_fd, buffer, sizeof(buffer) - 1);

		if (bytes_read > 0) {
			buffer[bytes_read] = '\0';
			result += buffer;

			// Check if we have a complete line
			if (result.find('\n') != string::npos) {
				break;
			}
		} else if (bytes_read == 0) {
			// EOF
			break;
		} else {
			// Error or would block
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				throw IOException("Error reading from process stdout");
			}
			break;
		}
	}

	StringUtil::RTrim(result);
	return result;
#endif
}

bool StdioTransport::WaitForData(int timeout_ms) {
#ifdef _WIN32
	return false; // Windows polling not implemented yet
#else
	struct pollfd pfd;
	pfd.fd = stdout_fd;
	pfd.events = POLLIN;

	int result = poll(&pfd, 1, timeout_ms);
	return result > 0 && (pfd.revents & POLLIN);
#endif
}

// TCP Transport placeholder implementations
TCPTransport::TCPTransport(const TCPConfig &config) : config(config) {
}

bool TCPTransport::Connect() {
	// Placeholder - Phase 2 implementation
	throw NotImplementedException("TCP transport not implemented yet");
}

void TCPTransport::Disconnect() {
	connected = false;
}

bool TCPTransport::IsConnected() const {
	return connected;
}

void TCPTransport::Send(const MCPMessage &message) {
	throw NotImplementedException("TCP transport not implemented yet");
}

MCPMessage TCPTransport::Receive() {
	throw NotImplementedException("TCP transport not implemented yet");
}

MCPMessage TCPTransport::SendAndReceive(const MCPMessage &message) {
	throw NotImplementedException("TCP transport not implemented yet");
}

bool TCPTransport::Ping() {
	return false;
}

string TCPTransport::GetConnectionInfo() const {
	return "tcp://" + config.host + ":" + std::to_string(config.port);
}

} // namespace duckdb
