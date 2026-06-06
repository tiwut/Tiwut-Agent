#include "agent.hpp"
#include "tools.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <climits>
#include <nlohmann/json.hpp>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <pwd.h>

Agent::Agent(const Config& config) : config_(config), client_(config) {}

std::string Agent::getSystemPrompt() const {
    return R"(You are Tiwut Agent (ta), a powerful terminal-based autonomous AI assistant running on Linux.
You have FULL control over the host system via a set of tools. You are a highly proactive, intelligent, and autonomous engineer. Your goal is to fully solve tasks directly on the host computer — never tell the user to do it themselves, never give up early.

== RESPONSE FORMAT ==
You MUST respond with a SINGLE valid JSON object only. No markdown, no backtick fences, no prose outside the JSON.

Schema:
{
  "thought": "Brief explanation of your reasoning",
  "action": "tool_name",        <- omit if outputting final_answer
  "args": { ... },              <- required when action is present
  "final_answer": "message"     <- omit unless task is 100% complete
}

CRITICAL JSON RULES:
- No literal newlines inside JSON string values — use \n instead.
- No literal tabs inside JSON string values — use \t instead.
- No backtick (`) characters anywhere in JSON output.
- No markdown code fences (``` or ```) anywhere in the response.
- Escape ALL double quotes inside string values with \".
- The entire response must start with { and end with } and be parseable.
- When writing multi-line file content, use \n escape sequences ONLY.

== AVAILABLE TOOLS ==

1. execute_command — Run a shell command.
   Args: {"cmd": "string"}
   Output: Full stdout+stderr of the command.

2. write_file — Write or overwrite a file.
   Args: {"path": "/absolute/path/to/file", "content": "file content with \\n for newlines"}
   IMPORTANT: Always use absolute paths (starting with /). Never use ~ in paths — expand manually.

3. read_file — Read a file.
   Args: {"path": "/absolute/path/to/file"}

4. list_directory — List a directory.
   Args: {"path": "/absolute/path"}

5. search_files — Find files matching a regex pattern.
   Args: {"path": "/absolute/path", "pattern": "regex"}

6. capture_screen — Take a screenshot.
   Args: {"output_path": "/absolute/path/screenshot.png"}

7. control_gui — Simulate mouse/keyboard input.
   Args: {"action": "click"|"move"|"type"|"key", "args": {"x":int, "y":int, "text":"str", "key":"str"}}

8. web_search — Search the web.
   Args: {"query": "string"}

9. generate_image — Generate an AI image and save it.
   Args: {"prompt": "string", "output_path": "/absolute/path/image.png"}

10. ask_user — Ask the user a question when truly necessary.
    Args: {"prompt": "string"}

== EXECUTION RULES ==

1. One tool per turn. Wait for tool output before proceeding.
2. NEVER give up when a tool fails. Instead:
   a. Try installing missing utilities: execute_command with sudo apt install -y / pip install / pip3 install
   b. Try an alternative command or approach (curl, wget, python3 inline script, etc.)
   c. Write a custom script with write_file and run it with execute_command
   d. Try at least 3 different approaches before asking the user
3. Before creating any files or projects, use ask_user to confirm the target directory.
4. Always use absolute paths when writing or reading files. Use execute_command to find $HOME if needed.
5. When writing code to a file, use \n for newlines in the JSON content string — never raw newlines.
6. After writing a file, verify it exists with list_directory or read_file.
7. After running a command, check the output carefully. If there's an error, fix it and retry.
8. If a tool says the action succeeded, verify the result independently before claiming the task is done.
9. Keep a mental log of what you tried. Don't repeat the same failing action.
10. When the task is fully complete and verified, output final_answer.
)";
}

static std::string sanitizeJsonString(const std::string& json) {
    std::string result;
    result.reserve(json.size() + 256);
    bool inside_string = false;
    bool escaped = false;

    for (size_t i = 0; i < json.size(); ++i) {
        unsigned char c = (unsigned char)json[i];

        if (escaped) {
            
            escaped = false;
            result += (char)c;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            result += (char)c;
            continue;
        }

        if (c == '"') {
            inside_string = !inside_string;
            result += '"';
            continue;
        }

        if (inside_string) {
            
            if (c == '\n') {
                result += "\\n";
            } else if (c == '\r') {
                result += "\\r";
            } else if (c == '\t') {
                result += "\\t";
            } else if (c == '\b') {
                result += "\\b";
            } else if (c == '\f') {
                result += "\\f";
            } else if (c == '`') {
                result += "'"; 
            } else if (c < 0x20) {
                
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                result += buf;
            } else {
                result += (char)c;
            }
        } else {
            
            result += (char)c;
        }
    }
    return result;
}

static std::string extractJsonBlock(const std::string& input) {
    size_t start = input.find('{');
    if (start == std::string::npos) return "";

    size_t len = input.size();
    int depth = 0;
    bool in_str = false;
    bool esc = false;
    size_t end = std::string::npos;

    for (size_t i = start; i < len; ++i) {
        char c = input[i];
        if (esc) { esc = false; continue; }
        if (c == '\\') { esc = true; continue; }
        if (c == '"') { in_str = !in_str; continue; }
        if (in_str) continue;
        if (c == '{') depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) { end = i; break; }
        }
    }

    if (end == std::string::npos) return "";
    return input.substr(start, end - start + 1);
}

static std::string expandPath(const std::string& path) {
    if (path.empty()) return path;

    
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home) {
            return std::string(home) + path.substr(1);
        }
    }

    
    if (path[0] == '/') return path;

    
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/" + path;
    }

    return path; 
}

static void trimHistory(std::vector<Message>& history, size_t maxMessages = 30) {
    if (history.size() <= maxMessages) return;

    
    Message firstMsg = history[0];
    size_t keep = maxMessages - 1;
    std::vector<Message> trimmed;
    trimmed.reserve(maxMessages);
    trimmed.push_back(firstMsg);
    size_t startIdx = history.size() - keep;
    for (size_t i = startIdx; i < history.size(); ++i) {
        trimmed.push_back(history[i]);
    }
    history = std::move(trimmed);
}

bool Agent::checkPermission(const std::string& toolName, const std::string& details) const {
    if (config_.safety.ask_permission == "never") return true;

    if (config_.safety.ask_permission == "sudo_only") {
        if (toolName != "execute_command") return true;
        if (details.find("sudo ") == std::string::npos) return true;
        
    }

    
    std::cout << "\n\033[31;1m[Security Confirmation Needed]\033[0m\n"
              << "\033[33;1mTool  :\033[0m " << toolName << "\n"
              << "\033[33;1mParams:\033[0m " << details.substr(0, 300)
              << (details.size() > 300 ? "..." : "") << "\n";

    
    char* raw = readline("\033[35mExecute this action? [Y/n]: \033[0m");
    std::string line;
    if (raw) {
        line = raw;
        free(raw);
    }

    return line.empty() || line[0] == 'y' || line[0] == 'Y';
}

std::string Agent::executeTool(const std::string& toolName, const nlohmann::json& args) {
    
    if (toolName == "execute_command") {
        if (!args.contains("cmd")) return "Error: 'cmd' parameter missing.";
        std::string cmd = args["cmd"].get<std::string>();

        if (!checkPermission(toolName, cmd)) {
            return "Error: User denied permission to run: " + cmd;
        }

        std::cout << "\033[32m[Executing] " << cmd << "\033[0m\n";
        std::string result = Tools::executeCommand(cmd);
        std::cout << "\033[37m" << result << "\033[0m\n";

        
        const size_t CMD_MAX = 6000;
        if (result.size() > CMD_MAX) {
            result = result.substr(0, 2500)
                   + "\n\n... [OUTPUT TRUNCATED — " + std::to_string(result.size() - 5000) + " chars omitted] ...\n\n"
                   + result.substr(result.size() - 2500);
        }
        return result;

    
    } else if (toolName == "write_file") {
        if (!args.contains("path") || !args.contains("content"))
            return "Error: 'path' and 'content' are required.";

        std::string path    = expandPath(args["path"].get<std::string>());
        std::string content = args["content"].get<std::string>();

        if (!checkPermission(toolName, "Write → " + path))
            return "Error: User denied write to: " + path;

        std::cout << "\033[32m[Writing File] " << path << "\033[0m\n";
        std::string res = Tools::writeFile(path, content);
        std::cout << "\033[37m" << res << "\033[0m\n";
        return res;

    
    } else if (toolName == "read_file") {
        if (!args.contains("path")) return "Error: 'path' is required.";
        std::string path = expandPath(args["path"].get<std::string>());

        if (!checkPermission(toolName, "Read ← " + path))
            return "Error: User denied read of: " + path;

        std::string content = Tools::readFile(path);
        
        const size_t FILE_MAX = 4000;
        if (content.size() > FILE_MAX) {
            content = content.substr(0, 2000)
                    + "\n\n... [FILE TRUNCATED — " + std::to_string(content.size() - 4000) + " chars omitted] ...\n\n"
                    + content.substr(content.size() - 2000);
        }
        return content;

    
    } else if (toolName == "list_directory") {
        std::string path = args.contains("path") ? expandPath(args["path"].get<std::string>()) : ".";
        if (!checkPermission(toolName, "List: " + path))
            return "Error: User denied listing of: " + path;
        return Tools::listDirectory(path);

    
    } else if (toolName == "search_files") {
        if (!args.contains("pattern")) return "Error: 'pattern' is required.";
        std::string path    = args.contains("path") ? expandPath(args["path"].get<std::string>()) : ".";
        std::string pattern = args["pattern"].get<std::string>();
        if (!checkPermission(toolName, "Search \"" + pattern + "\" in " + path))
            return "Error: User denied search.";
        return Tools::searchFiles(path, pattern);

    
    } else if (toolName == "capture_screen") {
        if (!args.contains("output_path")) return "Error: 'output_path' is required.";
        std::string out = expandPath(args["output_path"].get<std::string>());
        if (!checkPermission(toolName, "Screenshot → " + out))
            return "Error: User denied screenshot.";
        std::cout << "\033[32m[Capturing Screen]\033[0m\n";
        return Tools::captureScreen(out);

    
    } else if (toolName == "control_gui") {
        if (!args.contains("action") || !args.contains("args"))
            return "Error: 'action' and 'args' are required.";
        std::string act       = args["action"].get<std::string>();
        nlohmann::json subArgs = args["args"];
        if (!checkPermission(toolName, "GUI: " + act + " " + subArgs.dump()))
            return "Error: User denied GUI action.";
        std::cout << "\033[32m[GUI] " << act << "\033[0m\n";
        return Tools::controlGui(act, subArgs);

    
    } else if (toolName == "web_search") {
        if (!args.contains("query")) return "Error: 'query' is required.";
        std::string query = args["query"].get<std::string>();
        if (!checkPermission(toolName, "Web search: " + query))
            return "Error: User denied web search.";
        std::cout << "\033[32m[Searching Web] " << query << "\033[0m\n";
        std::string res = Tools::webSearch(query);
        
        const size_t WEB_MAX = 4000;
        if (res.size() > WEB_MAX)
            res = res.substr(0, WEB_MAX) + "\n... [Truncated]";
        return res;

    
    } else if (toolName == "generate_image") {
        if (!args.contains("prompt") || !args.contains("output_path"))
            return "Error: 'prompt' and 'output_path' are required.";
        std::string prompt = args["prompt"].get<std::string>();
        std::string path   = expandPath(args["output_path"].get<std::string>());
        if (!checkPermission(toolName, "Generate image → " + path))
            return "Error: User denied image generation.";
        std::cout << "\033[32m[Generating Image] \"" << prompt << "\" → " << path << "\033[0m\n";
        return Tools::generateImage(prompt, path);

    
    } else if (toolName == "ask_user") {
        if (!args.contains("prompt")) return "Error: 'prompt' is required.";
        std::string p = args["prompt"].get<std::string>();
        std::cout << "\n\033[36;1m[Tiwut Agent]: " << p << "\033[0m\n";

        char* raw = readline("\033[35mYour response: \033[0m");
        std::string resp;
        if (raw) {
            resp = raw;
            free(raw);
            if (!resp.empty()) add_history(resp.c_str());
        }
        return "User Response: " + resp;

    } else {
        return "Error: Unknown tool '" + toolName + "'. Valid tools: execute_command, write_file, read_file, list_directory, search_files, capture_screen, control_gui, web_search, generate_image, ask_user.";
    }
}

void Agent::run(const std::string& task) {
    history_.push_back({"user", task});
    std::string sysPrompt = getSystemPrompt();

    const int MAX_STEPS        = 30;
    const int MAX_JSON_RETRIES = 4;  
    int step         = 0;
    int jsonFailures = 0;  

    while (step < MAX_STEPS) {
        step++;
        trimHistory(history_, 28); 

        std::cout << "\033[36mThinking... (Step " << step << ")\033[0m\n";
        std::cout.flush();

        
        std::string rawResponse;
        try {
            rawResponse = client_.getResponse(history_, sysPrompt);
        } catch (const std::exception& e) {
            std::cerr << "\033[31;1m[LLM Error] " << e.what() << "\033[0m\n";
            
            if (step <= 2) {
                std::cout << "\033[33mRetrying LLM call...\033[0m\n";
                step--;  
                continue;
            }
            std::cerr << "\033[31;1mFailing after repeated LLM errors.\033[0m\n";
            break;
        }

        
        std::string jsonStr = extractJsonBlock(rawResponse);

        if (jsonStr.empty()) {
            jsonFailures++;
            std::cerr << "\033[31;1m[Parse Error] No JSON block found in model response.\033[0m\n";
            if (jsonFailures < MAX_JSON_RETRIES) {
                history_.push_back({"assistant", rawResponse.substr(0, 500)});
                history_.push_back({"user",
                    "ERROR: Your response did not contain a valid JSON object. "
                    "You MUST respond with a single raw JSON object starting with { and ending with }. "
                    "Do NOT use markdown, do NOT use backticks, do NOT add any text outside the JSON. "
                    "Try again now."
                });
                step--; 
                continue;
            } else {
                
                std::cout << "\033[31;1m[Agent]: Unable to produce valid JSON after "
                          << MAX_JSON_RETRIES << " retries. Raw response:\033[0m\n"
                          << rawResponse.substr(0, 800) << "\n";
                jsonFailures = 0;
                break;
            }
        }

        
        std::string cleanJson = sanitizeJsonString(jsonStr);
        nlohmann::json resJson;
        try {
            resJson = nlohmann::json::parse(cleanJson);
            jsonFailures = 0; 
        } catch (const std::exception& e) {
            jsonFailures++;
            std::cerr << "\033[31;1m[Parse Error] " << e.what() << "\033[0m\n";
            if (jsonFailures < MAX_JSON_RETRIES) {
                history_.push_back({"assistant", jsonStr.substr(0, 400)});
                history_.push_back({"user",
                    std::string("ERROR parsing your JSON: ") + e.what() + ". "
                    "Common mistakes: raw newlines inside string values (use \\n), "
                    "unescaped double quotes inside strings (use \\\"), "
                    "backtick characters (replace with single quote or remove). "
                    "Respond with corrected JSON only."
                });
                step--;
                continue;
            } else {
                std::cout << "\033[31;1m[Agent]: JSON parsing failed " << MAX_JSON_RETRIES
                          << " times, skipping this step.\033[0m\n";
                jsonFailures = 0;
                break;
            }
        }

        
        if (resJson.contains("thought") && resJson["thought"].is_string()) {
            std::cout << "\033[34;1m[Thought]:\033[0m "
                      << resJson["thought"].get<std::string>() << "\n";
        }

        
        if (resJson.contains("final_answer") && resJson["final_answer"].is_string()) {
            std::cout << "\n\033[32;1m[Tiwut Agent]:\033[0m "
                      << resJson["final_answer"].get<std::string>() << "\n\n";
            history_.push_back({"assistant", resJson["final_answer"].get<std::string>()});
            break;
        }

        
        if (resJson.contains("action") && resJson["action"].is_string()) {
            std::string tool = resJson["action"].get<std::string>();
            nlohmann::json args = resJson.contains("args") ? resJson["args"] : nlohmann::json::object();

            
            if (tool == "final_answer") {
                std::string fa;
                if (args.contains("content") && args["content"].is_string())
                    fa = args["content"].get<std::string>();
                else if (args.is_string())
                    fa = args.get<std::string>();
                else
                    fa = args.dump();
                std::cout << "\n\033[32;1m[Tiwut Agent]:\033[0m " << fa << "\n\n";
                break;
            }

            
            history_.push_back({"assistant", jsonStr});

            
            std::string toolResult = executeTool(tool, args);

            
            if (tool != "ask_user" && tool != "execute_command") {
                const std::string& preview = toolResult;
                std::cout << "\033[90m[Tool Result]: "
                          << (preview.size() > 250 ? preview.substr(0, 250) + "..." : preview)
                          << "\033[0m\n";
            }

            
            std::string historyResult = toolResult;
            const size_t HIST_MAX = 3000;
            if (historyResult.size() > HIST_MAX) {
                historyResult = historyResult.substr(0, 1200)
                              + "\n\n... [TRUNCATED " + std::to_string(historyResult.size() - 2400)
                              + " CHARS] ...\n\n"
                              + historyResult.substr(historyResult.size() - 1200);
            }

            history_.push_back({"user", "Tool '" + tool + "' result:\n" + historyResult});

        } else {
            
            std::cerr << "\033[33m[Warning] Response had neither 'action' nor 'final_answer'.\033[0m\n";
            history_.push_back({"assistant", jsonStr});
            history_.push_back({"user",
                "You did not specify an 'action' or 'final_answer'. "
                "If the task is done, set final_answer. Otherwise call the next tool."
            });
        }
    }

    if (step >= MAX_STEPS) {
        std::cout << "\033[31;1m[Agent]: Maximum step limit (" << MAX_STEPS << ") reached.\033[0m\n";
    }
}
