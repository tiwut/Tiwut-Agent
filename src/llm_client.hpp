#ifndef LLM_CLIENT_HPP
#define LLM_CLIENT_HPP

#include <string>
#include <vector>
#include "config.hpp"
#include <nlohmann/json.hpp>

struct Message {
    std::string role; 
    std::string content;
};

class LLMClient {
public:
    LLMClient(const Config& config);

    
    std::string getResponse(const std::vector<Message>& history, const std::string& systemPrompt);

private:
    Config config_;
    
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string postRequest(const std::string& url, const std::string& payload, const std::vector<std::string>& headers);
    
    
    std::string callOllama(const std::vector<Message>& history, const std::string& systemPrompt);
    std::string callGemini(const std::vector<Message>& history, const std::string& systemPrompt);
    std::string callOpenAI(const std::vector<Message>& history, const std::string& systemPrompt);
};

#endif 
