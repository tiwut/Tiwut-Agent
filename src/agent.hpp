#ifndef AGENT_HPP
#define AGENT_HPP

#include <string>
#include <vector>
#include "config.hpp"
#include "llm_client.hpp"

class Agent {
public:
    Agent(const Config& config);

    
    void run(const std::string& task);

private:
    Config config_;
    LLMClient client_;
    std::vector<Message> history_;

    
    std::string getSystemPrompt() const;
    
    
    bool checkPermission(const std::string& toolName, const std::string& details) const;
    
    
    std::string executeTool(const std::string& toolName, const nlohmann::json& args);
};

#endif 
