#include "tools.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstdio>
#include <memory>
#include <regex>

namespace fs = std::filesystem;

static std::string escapeArg(const std::string& arg) {
    std::string escaped;
    escaped += "'";
    for (char c : arg) {
        if (c == '\'') {
            escaped += "'\\''"; 
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

std::string Tools::executeCommand(const std::string& cmd) {
    std::string result;
    std::string fullCmd = cmd + " 2>&1";
    
    
    FILE* pipe = popen(("PAGER=cat " + fullCmd).c_str(), "r");
    if (!pipe) {
        return "Error: popen() failed.";
    }
    
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    return result.empty() ? "[Command executed successfully with no output]" : result;
}

std::string Tools::writeFile(const std::string& path, const std::string& content) {
    try {
        fs::path p(path);
        
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
        
        std::ofstream file(p);
        if (!file.is_open()) {
            return "Error: Could not open file for writing: " + path;
        }
        
        file << content;
        file.close();
        return "File written successfully: " + fs::absolute(p).string();
    } catch (const std::exception& e) {
        return "Error writing file: " + std::string(e.what());
    }
}

std::string Tools::readFile(const std::string& path) {
    try {
        fs::path p(path);
        if (!fs::exists(p)) {
            return "Error: File does not exist: " + path;
        }
        if (!fs::is_regular_file(p)) {
            return "Error: Path is not a regular file: " + path;
        }
        
        std::ifstream file(p, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return "Error: Could not open file for reading: " + path;
        }
        
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    } catch (const std::exception& e) {
        return "Error reading file: " + std::string(e.what());
    }
}

std::string Tools::listDirectory(const std::string& path) {
    try {
        fs::path p(path.empty() ? "." : path);
        if (!fs::exists(p)) {
            return "Error: Directory does not exist: " + path;
        }
        if (!fs::is_directory(p)) {
            return "Error: Path is not a directory: " + path;
        }
        
        std::ostringstream ss;
        ss << "Contents of directory " << fs::absolute(p).string() << ":\n";
        
        int count = 0;
        for (const auto& entry : fs::directory_iterator(p)) {
            count++;
            std::string type = entry.is_directory() ? "[DIR]" : "[FILE]";
            ss << "  " << type << " " << entry.path().filename().string();
            if (entry.is_regular_file()) {
                ss << " (" << entry.file_size() << " bytes)";
            }
            ss << "\n";
        }
        
        if (count == 0) {
            ss << "  [Directory is empty]\n";
        }
        
        return ss.str();
    } catch (const std::exception& e) {
        return "Error listing directory: " + std::string(e.what());
    }
}

std::string Tools::searchFiles(const std::string& path, const std::string& pattern) {
    try {
        fs::path startPath(path.empty() ? "." : path);
        if (!fs::exists(startPath)) {
            return "Error: Start path does not exist: " + path;
        }
        
        std::ostringstream ss;
        ss << "Search results for \"" << pattern << "\" under " << fs::absolute(startPath).string() << ":\n";
        
        int matchCount = 0;
        std::regex regexPattern(pattern, std::regex_constants::icase);
        
        for (const auto& entry : fs::recursive_directory_iterator(startPath, fs::directory_options::skip_permission_denied)) {
            std::string filename = entry.path().filename().string();
            std::string relPath = fs::relative(entry.path(), startPath).string();
            
            if (std::regex_search(filename, regexPattern) || std::regex_search(relPath, regexPattern)) {
                matchCount++;
                std::string type = entry.is_directory() ? "[DIR]" : "[FILE]";
                ss << "  " << type << " " << relPath;
                if (entry.is_regular_file()) {
                    try {
                        ss << " (" << entry.file_size() << " bytes)";
                    } catch (...) {}
                }
                ss << "\n";
                if (matchCount >= 100) {
                    ss << "  ... [Truncated after 100 matches]\n";
                    break;
                }
            }
        }
        
        if (matchCount == 0) {
            ss << "  [No matching files or folders found]\n";
        }
        
        return ss.str();
    } catch (const std::exception& e) {
        return "Error searching files: " + std::string(e.what());
    }
}

std::string Tools::runHelper(const std::vector<std::string>& args) {
    
    std::string helperPath = "/home/tiwut/Documents/Dev/Tiwut-Agent/scripts/gui_helper.py";
    
    std::string cmd = "python3 " + escapeArg(helperPath);
    for (const auto& arg : args) {
        cmd += " " + escapeArg(arg);
    }
    
    return executeCommand(cmd);
}

std::string Tools::captureScreen(const std::string& outputPath) {
    return runHelper({"screenshot", outputPath});
}

std::string Tools::controlGui(const std::string& action, const nlohmann::json& args) {
    return runHelper({"gui", action, args.dump()});
}

std::string Tools::webSearch(const std::string& query) {
    return runHelper({"search", query});
}

std::string Tools::generateImage(const std::string& prompt, const std::string& outputPath) {
    return runHelper({"image", prompt, outputPath});
}
