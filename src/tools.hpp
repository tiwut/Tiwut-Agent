#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class Tools {
public:
    
    static std::string executeCommand(const std::string& cmd);

    
    static std::string writeFile(const std::string& path, const std::string& content);
    static std::string readFile(const std::string& path);
    static std::string listDirectory(const std::string& path);
    static std::string searchFiles(const std::string& path, const std::string& pattern);

    
    static std::string captureScreen(const std::string& outputPath);
    static std::string controlGui(const std::string& action, const nlohmann::json& args);
    static std::string webSearch(const std::string& query);
    static std::string generateImage(const std::string& prompt, const std::string& outputPath);

private:
    
    static std::string runHelper(const std::vector<std::string>& args);
};

#endif 
