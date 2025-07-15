#include "protocol/mcp_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"

#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

namespace duckdb {

StdioTransport::StdioTransport(const StdioConfig &config) 
    : config(config), connected(false), process_pid(-1), 
      stdin_fd(-1), stdout_fd(-1), stderr_fd(-1) {
}

StdioTransport::~StdioTransport() {
    if (connected) {
        Disconnect();
    }
}

bool StdioTransport::Connect() {
    lock_guard<mutex> lock(io_mutex);
    
    if (connected) {
        return true;
    }
    
    if (!StartProcess()) {
        return false;
    }
    
    connected = true;
    return true;
}

void StdioTransport::Disconnect() {
    lock_guard<mutex> lock(io_mutex);
    
    if (!connected) {
        return;
    }
    
    StopProcess();
    connected = false;
}

bool StdioTransport::IsConnected() const {
    return connected && IsProcessRunning();
}

void StdioTransport::Send(const MCPMessage &message) {
    if (!IsConnected()) {
        throw IOException("Transport not connected");
    }
    
    string json = message.ToJSON();
    
    // Debug logging - TODO: remove this when issue is fixed
    fprintf(stderr, "[MCP-DEBUG] Sending JSON: %s\n", json.c_str());
    
    WriteToProcess(json + "\n");
}

MCPMessage StdioTransport::Receive() {
    if (!IsConnected()) {
        throw IOException("Transport not connected");
    }
    
    string response = ReadFromProcess();
    
    // Debug logging - TODO: remove this when issue is fixed  
    // fprintf(stderr, "[MCP-DEBUG] Raw JSON response: %s\n", response.c_str());
    
    return MCPMessage::FromJSON(response);
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
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    
    // Create pipes
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        return false;
    }
    
    process_pid = fork();
    
    if (process_pid == -1) {
        // Fork failed
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return false;
    }
    
    if (process_pid == 0) {
        // Child process
        
        // Redirect stdin, stdout, stderr
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        // Close pipe ends
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        
        // Set working directory
        if (!config.working_directory.empty()) {
            if (chdir(config.working_directory.c_str()) != 0) {
                // Could log error but continue anyway
            }
        }
        
        // Set environment variables
        for (const auto &env_pair : config.environment) {
            setenv(env_pair.first.c_str(), env_pair.second.c_str(), 1);
        }
        
        // Prepare arguments
        vector<char*> args;
        args.push_back(const_cast<char*>(config.command_path.c_str()));
        for (const auto &arg : config.arguments) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);
        
        // Debug: Log what we're about to execute
        fprintf(stderr, "[MCP-EXEC-DEBUG] Executing command: %s\n", config.command_path.c_str());
        fprintf(stderr, "[MCP-EXEC-DEBUG] Working directory: %s\n", config.working_directory.c_str());
        fprintf(stderr, "[MCP-EXEC-DEBUG] Arguments:\n");
        for (size_t i = 0; i < args.size() - 1; i++) {
            fprintf(stderr, "[MCP-EXEC-DEBUG]   [%zu]: %s\n", i, args[i]);
        }
        fprintf(stderr, "[MCP-EXEC-DEBUG] Environment variables:\n");
        for (const auto &env_pair : config.environment) {
            fprintf(stderr, "[MCP-EXEC-DEBUG]   %s=%s\n", env_pair.first.c_str(), env_pair.second.c_str());
        }
        
        // Execute command
        execvp(config.command_path.c_str(), args.data());
        
        // If we get here, exec failed - log the error
        int exec_errno = errno;
        fprintf(stderr, "[MCP-EXEC-DEBUG] execvp failed: %s (errno: %d)\n", strerror(exec_errno), exec_errno);
        exit(1);
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
        fprintf(stderr, "[MCP-EXEC-DEBUG] Child process died immediately after fork\n");
        
        // Try to read any stderr output from failed child
        char error_buffer[1024];
        ssize_t error_bytes = read(stderr_fd, error_buffer, sizeof(error_buffer) - 1);
        if (error_bytes > 0) {
            error_buffer[error_bytes] = '\0';
            fprintf(stderr, "[MCP-EXEC-DEBUG] Child stderr: %s\n", error_buffer);
        }
        return false;
    }
    
    fprintf(stderr, "[MCP-EXEC-DEBUG] Child process started successfully (pid: %d)\n", process_pid);
    return true;
}

void StdioTransport::StopProcess() {
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
        
        // Terminate process
        kill(process_pid, SIGTERM);
        
        // Wait for process to exit
        int status;
        waitpid(process_pid, &status, WNOHANG);
        
        process_pid = -1;
    }
}

bool StdioTransport::IsProcessRunning() const {
    if (process_pid <= 0) {
        return false;
    }
    
    int status;
    int result = waitpid(process_pid, &status, WNOHANG);
    
    return result == 0; // Process is still running
}

void StdioTransport::WriteToProcess(const string &data) {
    if (stdin_fd < 0) {
        throw IOException("Process stdin not available");
    }
    
    ssize_t written = write(stdin_fd, data.c_str(), data.length());
    if (written != static_cast<ssize_t>(data.length())) {
        throw IOException("Failed to write to process stdin");
    }
}

string StdioTransport::ReadFromProcess() {
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
}

bool StdioTransport::WaitForData(int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = stdout_fd;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, timeout_ms);
    return result > 0 && (pfd.revents & POLLIN);
}

// TCP Transport placeholder implementations
TCPTransport::TCPTransport(const TCPConfig &config) : config(config) {}

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