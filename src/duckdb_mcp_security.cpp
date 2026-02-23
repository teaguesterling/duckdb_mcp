#include "duckdb_mcp_security.hpp"
#ifndef __EMSCRIPTEN__
#include "client/mcp_storage_extension.hpp"
#endif
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/common/serializer/buffered_file_reader.hpp"
#include "include/json_utils.hpp"
#ifndef __EMSCRIPTEN__
#include <fstream>
#include <sstream>
#endif
#ifndef _WIN32
#include <climits>
#include <cstdlib>
#endif

namespace duckdb {

void MCPSecurityConfig::SetAllowedCommands(const string &commands) {
	if (servers_locked) {
		throw InvalidInputException("Cannot modify MCP settings: servers are locked");
	}
	if (commands_locked) {
		throw InvalidInputException("Cannot modify allowed MCP commands: commands are immutable once set for security");
	}

	// Parse and set commands
	allowed_commands = ParseDelimitedString(commands, ':');

	// Lock commands immediately after being explicitly set (security requirement).
	// This prevents re-initialization from widening permissions.
	// An empty list means "deny all" rather than "permissive mode".
	commands_locked = true;
}

void MCPSecurityConfig::SetAllowedUrls(const string &urls) {
	if (servers_locked) {
		throw InvalidInputException("Cannot modify MCP settings: servers are locked");
	}
	allowed_urls = ParseDelimitedString(urls, ' ');
}

void MCPSecurityConfig::SetServerFile(const string &file_path) {
	if (servers_locked) {
		throw InvalidInputException("Cannot modify MCP settings: servers are locked");
	}
	server_file = file_path;
}

void MCPSecurityConfig::LockServers(bool lock) {
	servers_locked = lock;
}

void MCPSecurityConfig::SetServingDisabled(bool disabled) {
	serving_disabled = disabled;
}

bool MCPSecurityConfig::IsCommandAllowed(const string &command_path) const {
	// If we're in permissive mode (no security settings configured), allow everything
	if (IsPermissiveMode()) {
		return true;
	}

	// If no allowlist is configured but we're not in permissive mode, allow nothing (secure by default)
	if (allowed_commands.empty()) {
		return false;
	}

	// Security: Ensure this is an executable path only (no arguments)
	// The command_path should not contain spaces or argument separators
	if (StringUtil::Contains(command_path, " ") || StringUtil::Contains(command_path, "\t") ||
	    StringUtil::Contains(command_path, "\n") || StringUtil::Contains(command_path, "\r")) {
		return false;
	}

	// Check against allowlist - exact string match only.
	// Allowlist entries should use the same form (absolute or relative) as the command.
	for (const auto &allowed : allowed_commands) {
		if (command_path == allowed) {
			return true;
		}
	}

	return false;
}

bool MCPSecurityConfig::IsUrlAllowed(const string &url) const {
	// If we're in permissive mode (no security settings configured), allow everything
	if (IsPermissiveMode()) {
		return true;
	}

	// If no allowlist is configured but we're not in permissive mode, allow nothing (secure by default)
	if (allowed_urls.empty()) {
		return false;
	}

	// Check against allowlist
	for (const auto &allowed : allowed_urls) {
		if (StringUtil::StartsWith(url, allowed)) {
			return true;
		}
	}

	return false;
}

bool MCPSecurityConfig::IsPermissiveMode() const {
	// Permissive mode is when NO security settings have been configured
	// (both commands and URLs are empty, and commands haven't been locked)
	return allowed_commands.empty() && allowed_urls.empty() && !commands_locked;
}

void MCPSecurityConfig::ValidateAttachSecurity(const string &command, const vector<string> &args) const {
	if (servers_locked) {
		throw InvalidInputException("Cannot attach MCP servers: servers are locked");
	}

	// If we're in permissive mode, skip security validation but still do basic safety checks
	if (IsPermissiveMode()) {
		// Basic safety checks even in permissive mode
		for (const auto &arg : args) {
			// Prevent dangerous arguments even in permissive mode
			if (StringUtil::Contains(arg, "..") || StringUtil::Contains(arg, "|") || StringUtil::Contains(arg, ";") ||
			    StringUtil::Contains(arg, "&") || StringUtil::Contains(arg, "`") || StringUtil::Contains(arg, "$")) {
				throw InvalidInputException("MCP argument contains potentially unsafe characters: " + arg);
			}
		}
		return; // Allow in permissive mode
	}

	// Check if any commands are configured at all
	if (allowed_commands.empty()) {
		throw InvalidInputException("No MCP commands are allowed. Set allowed_mcp_commands setting first. Example: SET "
		                            "allowed_mcp_commands='python3:/usr/bin/python3'");
	}

	if (!IsCommandAllowed(command)) {
		// Build helpful error message showing what's allowed
		string allowed_list = "";
		for (size_t i = 0; i < allowed_commands.size(); i++) {
			if (i > 0)
				allowed_list += ", ";
			allowed_list += "'" + allowed_commands[i] + "'";
		}
		throw InvalidInputException("MCP command '" + command + "' not allowed. Allowed commands: " + allowed_list +
		                            ". Cannot modify allowed_mcp_commands after first use (security requirement).");
	}

	// Additional validation for command arguments
	for (const auto &arg : args) {
		// Prevent dangerous arguments
		if (StringUtil::Contains(arg, "..") || StringUtil::Contains(arg, "|") || StringUtil::Contains(arg, ";") ||
		    StringUtil::Contains(arg, "&") || StringUtil::Contains(arg, "`") || StringUtil::Contains(arg, "$")) {
			throw InvalidInputException("MCP argument contains potentially unsafe characters: " + arg);
		}
	}
}

vector<string> MCPSecurityConfig::ParseDelimitedString(const string &input, char delimiter) const {
	vector<string> result;
	if (input.empty()) {
		return result;
	}

	stringstream ss(input);
	string item;

	while (getline(ss, item, delimiter)) {
		StringUtil::Trim(item);
		if (!item.empty()) {
			result.push_back(item);
		}
	}

	return result;
}

#ifndef __EMSCRIPTEN__

MCPConnectionParams ParseMCPAttachParams(const AttachInfo &info) {
	MCPConnectionParams params;

	// Check for config file mode first
	if (info.options.find("from_config_file") != info.options.end()) {
		auto config_file_value = info.options.at("from_config_file");
		if (!config_file_value.IsNull()) {
			params.config_file_path = config_file_value.ToString();
			params.server_name = info.path; // The ATTACH path becomes the server name

			// Security: Validate that the config file path matches the configured mcp_server_file.
			// This prevents attackers from using from_config_file to load arbitrary config files.
			auto &sec = MCPSecurityConfig::GetInstance();
			string configured_file = sec.GetServerFile();
#ifndef _WIN32
			char resolved_requested[PATH_MAX];
			char resolved_configured[PATH_MAX];
			// Resolve both paths to canonical form for comparison
			if (realpath(params.config_file_path.c_str(), resolved_requested) &&
			    realpath(configured_file.c_str(), resolved_configured)) {
				if (string(resolved_requested) != string(resolved_configured)) {
					throw InvalidInputException(
					    "from_config_file path does not match configured mcp_server_file. "
					    "Set mcp_server_file to the desired config path first.");
				}
			} else {
				// If either path can't be resolved, compare the raw strings as a fallback
				if (params.config_file_path != configured_file) {
					throw InvalidInputException(
					    "from_config_file path does not match configured mcp_server_file. "
					    "Set mcp_server_file to the desired config path first.");
				}
			}
#else
			// On Windows, fall back to simple string comparison
			if (params.config_file_path != configured_file) {
				throw InvalidInputException(
				    "from_config_file path does not match configured mcp_server_file. "
				    "Set mcp_server_file to the desired config path first.");
			}
#endif

			// Load parameters from config file
			auto config_params = ParseMCPConfigFile(params.config_file_path, params.server_name);

			// Override with any explicitly provided parameters
			if (info.options.find("transport") != info.options.end()) {
				auto transport_value = info.options.at("transport");
				if (!transport_value.IsNull()) {
					config_params.transport = transport_value.ToString();
				}
			}

			return config_params;
		}
	}

	// Try structured parameters with JSON parsing (new format)
	// Note: DuckDB lowercases option names automatically
	if (info.options.find("transport") != info.options.end() || info.options.find("args") != info.options.end() ||
	    info.options.find("cwd") != info.options.end() || info.options.find("env") != info.options.end()) {
		// The path is used literally as command or URL
		params.command = info.path;

		// Parse TRANSPORT parameter (simple string)
		if (info.options.find("transport") != info.options.end()) {
			auto transport_value = info.options.at("transport");
			if (!transport_value.IsNull()) {
				params.transport = transport_value.ToString();
			}
		}

		// Parse ARGS parameter (JSON array or fall back to simple string)
		if (info.options.find("args") != info.options.end()) {
			auto args_value = info.options.at("args");
			if (!args_value.IsNull()) {
				string args_str = args_value.ToString();

				// Check if it starts with '[' - JSON array format
				if (args_str.length() > 0 && args_str[0] == '[') {
					// Parse as JSON array
					yyjson_doc *doc = yyjson_read(args_str.c_str(), args_str.length(), 0);
					if (doc) {
						yyjson_val *root = yyjson_doc_get_root(doc);
						if (yyjson_is_arr(root)) {
							size_t idx, max;
							yyjson_val *arg;
							yyjson_arr_foreach(root, idx, max, arg) {
								if (yyjson_is_str(arg)) {
									params.args.push_back(yyjson_get_str(arg));
								}
							}
						}
						yyjson_doc_free(doc);
					} else {
						throw InvalidInputException("Invalid JSON in ARGS parameter: " + args_str);
					}
				} else {
					// Future: bash-style parsing would go here
					// For now, treat as single argument
					params.args.push_back(args_str);
				}
			}
		}

		// Parse CWD parameter (simple string)
		if (info.options.find("cwd") != info.options.end()) {
			auto cwd_value = info.options.at("cwd");
			if (!cwd_value.IsNull()) {
				params.working_dir = cwd_value.ToString();
			}
		}

		// Parse ENV parameter (JSON object or fall back to simple string)
		if (info.options.find("env") != info.options.end()) {
			auto env_value = info.options.at("env");
			if (!env_value.IsNull()) {
				string env_str = env_value.ToString();

				// Check if it starts with '{' - JSON object format
				if (env_str.length() > 0 && env_str[0] == '{') {
					// Parse as JSON object
					yyjson_doc *doc = yyjson_read(env_str.c_str(), env_str.length(), 0);
					if (doc) {
						yyjson_val *root = yyjson_doc_get_root(doc);
						if (yyjson_is_obj(root)) {
							size_t idx, max;
							yyjson_val *key, *val;
							yyjson_obj_foreach(root, idx, max, key, val) {
								if (yyjson_is_str(key) && yyjson_is_str(val)) {
									params.env[yyjson_get_str(key)] = yyjson_get_str(val);
								}
							}
						}
						yyjson_doc_free(doc);
					} else {
						throw InvalidInputException("Invalid JSON in ENV parameter: " + env_str);
					}
				} else {
					// Future: bash-style parsing would go here (KEY=value KEY2=value2)
					// For now, treat as single environment variable
					auto equals_pos = env_str.find('=');
					if (equals_pos != string::npos) {
						string key = env_str.substr(0, equals_pos);
						string value = env_str.substr(equals_pos + 1);
						params.env[key] = value;
					}
				}
			}
		}
	} else {
		// Default: use path as literal command for stdio transport
		params.command = info.path;
		params.transport = "stdio";
	}

	// CRITICAL: Validate security immediately after parsing (before any connection attempts)
	auto &security = MCPSecurityConfig::GetInstance();
	security.ValidateAttachSecurity(params.command, params.args);

	return params;
}

MCPConnectionParams ParseMCPConfigFile(const string &config_file_path, const string &server_name) {
	MCPConnectionParams params;

	try {
		// Read the .mcp.json file
		std::ifstream file(config_file_path);
		if (!file.is_open()) {
			throw IOException("Could not open MCP config file: " + config_file_path);
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		string json_content = buffer.str();
		file.close();

		// Parse JSON using yyjson
		yyjson_doc *doc = yyjson_read(json_content.c_str(), json_content.length(), 0);
		if (!doc) {
			throw IOException("Invalid JSON in MCP config file: " + config_file_path);
		}

		yyjson_val *root = yyjson_doc_get_root(doc);
		if (!yyjson_is_obj(root)) {
			yyjson_doc_free(doc);
			throw IOException("MCP config file must contain a JSON object");
		}

		// Look for mcpServers object
		yyjson_val *mcp_servers = yyjson_obj_get(root, "mcpServers");
		if (!mcp_servers || !yyjson_is_obj(mcp_servers)) {
			yyjson_doc_free(doc);
			throw IOException("MCP config file must contain 'mcpServers' object");
		}

		// Find the specific server
		yyjson_val *server_config = yyjson_obj_get(mcp_servers, server_name.c_str());
		if (!server_config || !yyjson_is_obj(server_config)) {
			yyjson_doc_free(doc);
			throw IOException("Server '" + server_name + "' not found in MCP config file");
		}

		// Parse command
		yyjson_val *command_val = yyjson_obj_get(server_config, "command");
		if (command_val && yyjson_is_str(command_val)) {
			params.command = yyjson_get_str(command_val);
		} else {
			yyjson_doc_free(doc);
			throw IOException("Server '" + server_name + "' missing required 'command' field");
		}

		// Parse args (optional)
		yyjson_val *args_val = yyjson_obj_get(server_config, "args");
		if (args_val && yyjson_is_arr(args_val)) {
			size_t idx, max;
			yyjson_val *arg;
			yyjson_arr_foreach(args_val, idx, max, arg) {
				if (yyjson_is_str(arg)) {
					params.args.push_back(yyjson_get_str(arg));
				}
			}
		}

		// Parse cwd (optional)
		yyjson_val *cwd_val = yyjson_obj_get(server_config, "cwd");
		if (cwd_val && yyjson_is_str(cwd_val)) {
			params.working_dir = yyjson_get_str(cwd_val);
		}

		// Parse env (optional)
		yyjson_val *env_val = yyjson_obj_get(server_config, "env");
		if (env_val && yyjson_is_obj(env_val)) {
			size_t idx, max;
			yyjson_val *key, *val;
			yyjson_obj_foreach(env_val, idx, max, key, val) {
				if (yyjson_is_str(key) && yyjson_is_str(val)) {
					params.env[yyjson_get_str(key)] = yyjson_get_str(val);
				}
			}
		}

		// Parse transport (optional, defaults to stdio)
		yyjson_val *transport_val = yyjson_obj_get(server_config, "transport");
		if (transport_val && yyjson_is_str(transport_val)) {
			params.transport = yyjson_get_str(transport_val);
		} else {
			params.transport = "stdio"; // Default
		}

		yyjson_doc_free(doc);

		// Set config file parameters for reference
		params.config_file_path = config_file_path;
		params.server_name = server_name;

	} catch (const std::exception &e) {
		throw IOException("Error parsing MCP config file '" + config_file_path + "': " + string(e.what()));
	}

	// CRITICAL: Validate security for config file parameters
	auto &security = MCPSecurityConfig::GetInstance();
	security.ValidateAttachSecurity(params.command, params.args);

	return params;
}

#endif // !__EMSCRIPTEN__

} // namespace duckdb
