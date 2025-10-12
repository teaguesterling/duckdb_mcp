#include "server/stdio_server_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/common/exception.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#ifndef _WIN32
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#endif

namespace duckdb {

FdServerTransport::FdServerTransport(int input_fd, int output_fd) 
    : input_fd(input_fd), output_fd(output_fd), owns_fds(false), connected(false) {
#ifdef _WIN32
    throw NotImplementedException("Server transport not supported on Windows yet");
#endif
}

FdServerTransport::FdServerTransport(const string &input_path, const string &output_path) 
    : input_fd(-1), output_fd(-1), owns_fds(true), connected(false) {
#ifdef _WIN32
    throw NotImplementedException("Server transport not supported on Windows yet");
#else
    if (!OpenFileDescriptors(input_path, output_path)) {
        throw IOException("Failed to open file descriptors: " + input_path + ", " + output_path);
    }
#endif
}

FdServerTransport::~FdServerTransport() {
    Disconnect();
}

bool FdServerTransport::Connect() {
    lock_guard<mutex> lock(io_mutex);
    
    if (connected) {
        return true;
    }
    
    // Validate file descriptors
    if (input_fd < 0 || output_fd < 0) {
        return false;
    }
    
    connected = true;
    return true;
}

void FdServerTransport::Disconnect() {
    lock_guard<mutex> lock(io_mutex);
    
#ifndef _WIN32
    if (owns_fds) {
        if (input_fd >= 0 && input_fd != STDIN_FILENO) {
            close(input_fd);
        }
        if (output_fd >= 0 && output_fd != STDOUT_FILENO) {
            close(output_fd);
        }
    }
#endif
    
    input_fd = -1;
    output_fd = -1;
    connected = false;
}

bool FdServerTransport::IsConnected() const {
    return connected;
}

void FdServerTransport::Send(const MCPMessage &message) {
    if (!IsConnected()) {
        throw IOException("Not connected");
    }
    
    lock_guard<mutex> lock(io_mutex);
    
    try {
        string json = message.ToJSON();
        WriteLine(json);
        
        // Flush output
#ifndef _WIN32
        if (output_fd == STDOUT_FILENO) {
            std::cout.flush();
        } else {
            fsync(output_fd);
        }
#else
        std::cout.flush();
#endif
    } catch (const std::exception &e) {
        throw IOException("Failed to send message: " + string(e.what()));
    }
}

MCPMessage FdServerTransport::Receive() {
    if (!IsConnected()) {
        throw IOException("Not connected");
    }
    
    lock_guard<mutex> lock(io_mutex);
    
    try {
        // Wait for input to be available
        while (!HasInputAvailable()) {
            if (!connected) {
                throw IOException("Connection closed");
            }
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        string line = ReadLine();
        if (line.empty()) {
            throw IOException("Received empty message");
        }
        
        return MCPMessage::FromJSON(line);
        
    } catch (const std::exception &e) {
        throw IOException("Failed to receive message: " + string(e.what()));
    }
}

MCPMessage FdServerTransport::SendAndReceive(const MCPMessage &message) {
    // For server mode, this doesn't make sense - we send responses, not requests
    throw NotImplementedException("SendAndReceive not supported in server mode");
}

bool FdServerTransport::Ping() {
    return IsConnected();
}

string FdServerTransport::GetConnectionInfo() const {
    if (input_fd == STDIN_FILENO && output_fd == STDOUT_FILENO) {
        return "fd server transport (stdin/stdout)";
    } else {
        return "fd server transport (fds: " + std::to_string(input_fd) + "/" + std::to_string(output_fd) + ")";
    }
}

string FdServerTransport::ReadLine() {
#ifdef _WIN32
    throw NotImplementedException("Server transport not supported on Windows yet");
#else
    string line;
    char buffer[1024];
    
    if (input_fd == STDIN_FILENO) {
        // Use standard getline for stdin
        if (!std::getline(std::cin, line)) {
            throw IOException("Failed to read from stdin");
        }
    } else {
        // Read from file descriptor until newline
        ssize_t bytes_read;
        while ((bytes_read = read(input_fd, buffer, 1)) > 0) {
            if (buffer[0] == '\n') {
                break;
            }
            line += buffer[0];
        }
        if (bytes_read < 0) {
            throw IOException("Failed to read from file descriptor");
        }
    }
    
    return line;
#endif
}

void FdServerTransport::WriteLine(const string &line) {
#ifdef _WIN32
    throw NotImplementedException("Server transport not supported on Windows yet");
#else
    string output = line + "\n";
    
    if (output_fd == STDOUT_FILENO) {
        std::cout << line << std::endl;
    } else {
        ssize_t bytes_written = write(output_fd, output.c_str(), output.length());
        if (bytes_written < 0 || static_cast<size_t>(bytes_written) != output.length()) {
            throw IOException("Failed to write to file descriptor");
        }
    }
#endif
}

bool FdServerTransport::HasInputAvailable() {
#ifdef _WIN32
    return false; // Windows polling not implemented yet
#else
    // Use poll to check if file descriptor has data available
    struct pollfd pfd;
    pfd.fd = input_fd;
    pfd.events = POLLIN;
    
    int result = poll(&pfd, 1, 0); // Non-blocking poll
    return result > 0 && (pfd.revents & POLLIN);
#endif
}

bool FdServerTransport::OpenFileDescriptors(const string &input_path, const string &output_path) {
#ifdef _WIN32
    return false; // Windows file descriptor opening not implemented yet
#else
    // Open input file descriptor
    if (input_path == "/dev/stdin") {
        input_fd = STDIN_FILENO;
    } else {
        input_fd = open(input_path.c_str(), O_RDONLY);
        if (input_fd < 0) {
            return false;
        }
    }
    
    // Open output file descriptor
    if (output_path == "/dev/stdout") {
        output_fd = STDOUT_FILENO;
    } else {
        output_fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) {
            if (input_fd >= 0 && input_fd != STDIN_FILENO) {
                close(input_fd);
            }
            return false;
        }
    }
    
    return true;
#endif
}

} // namespace duckdb