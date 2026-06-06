#include "llm_client.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <chrono>

LLMClient::LLMClient(const Config& config) : config_(config) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

size_t LLMClient::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string LLMClient::postRequest(const std::string& url, const std::string& payload, const std::vector<std::string>& headers) {
    CURL* curl = curl_easy_init();
    std::string responseString;
    
    if (curl) {
        struct curl_slist* chunk = nullptr;
        for (const auto& header : headers) {
            chunk = curl_slist_append(chunk, header.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);
        
        
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); 
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 240L);       

        
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        CURLcode res = CURLE_OK;
        int retryCount = 0;
        const int MAX_RETRIES = 3;
        
        while (retryCount < MAX_RETRIES) {
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                break;
            }
            
            retryCount++;
            if (retryCount < MAX_RETRIES) {
                std::cerr << "\n\033[33m[Warning] LLM API call failed: " << curl_easy_strerror(res) 
                          << ". Retrying in 3 seconds... (Attempt " << retryCount << "/" << MAX_RETRIES << ")\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(3));
                responseString.clear(); 
            }
        }

        if (res != CURLE_OK) {
            std::string err = curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            curl_slist_free_all(chunk);
            throw std::runtime_error("cURL Request failed: " + err);
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(chunk);
    } else {
        throw std::runtime_error("Failed to initialize cURL.");
    }
    
    return responseString;
}

std::string LLMClient::getResponse(const std::vector<Message>& history, const std::string& systemPrompt) {
    if (config_.api_provider == "ollama") {
        return callOllama(history, systemPrompt);
    } else if (config_.api_provider == "gemini") {
        return callGemini(history, systemPrompt);
    } else if (config_.api_provider == "openai") {
        return callOpenAI(history, systemPrompt);
    } else {
        throw std::runtime_error("Unsupported API provider: " + config_.api_provider);
    }
}

std::string LLMClient::callOllama(const std::vector<Message>& history, const std::string& systemPrompt) {
    std::string url = config_.api_url + "/api/chat";
    
    nlohmann::json payload;
    payload["model"] = config_.model;
    payload["stream"] = false;
    payload["format"] = "json";   
    payload["keep_alive"] = "10m"; 

    nlohmann::json messages = nlohmann::json::array();
    
    
    if (!systemPrompt.empty()) {
        nlohmann::json systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = systemPrompt;
        messages.push_back(systemMsg);
    }
    
    
    for (const auto& msg : history) {
        nlohmann::json m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        messages.push_back(m);
    }
    payload["messages"] = messages;

    std::vector<std::string> headers = { "Content-Type: application/json" };
    
    std::string rawResponse = postRequest(url, payload.dump(), headers);
    
    try {
        nlohmann::json resJson = nlohmann::json::parse(rawResponse);
        if (resJson.contains("message") && resJson["message"].contains("content")) {
            return resJson["message"]["content"].get<std::string>();
        } else {
            throw std::runtime_error("Unexpected Ollama response structure: " + rawResponse);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Ollama parsing error: " + std::string(e.what()) + ". Response: " + rawResponse);
    }
}

std::string LLMClient::callGemini(const std::vector<Message>& history, const std::string& systemPrompt) {
    if (config_.api_key.empty() || config_.api_key == "YOUR_GEMINI_API_KEY") {
        throw std::runtime_error("Gemini API key is not configured in ~/.config/ta/config.yaml");
    }
    
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" + config_.model + ":generateContent?key=" + config_.api_key;
    
    nlohmann::json payload;
    
    
    if (!systemPrompt.empty()) {
        nlohmann::json systemInstruction;
        systemInstruction["parts"] = nlohmann::json::array({ {{"text", systemPrompt}} });
        payload["systemInstruction"] = systemInstruction;
    }
    
    
    nlohmann::json contents = nlohmann::json::array();
    for (const auto& msg : history) {
        nlohmann::json content;
        
        content["role"] = (msg.role == "assistant" ? "model" : "user");
        content["parts"] = nlohmann::json::array({ {{"text", msg.content}} });
        contents.push_back(content);
    }
    payload["contents"] = contents;
    
    
    nlohmann::json generationConfig;
    generationConfig["responseMimeType"] = "application/json";
    payload["generationConfig"] = generationConfig;

    std::vector<std::string> headers = { "Content-Type: application/json" };
    
    std::string rawResponse = postRequest(url, payload.dump(), headers);
    
    try {
        nlohmann::json resJson = nlohmann::json::parse(rawResponse);
        if (resJson.contains("candidates") && !resJson["candidates"].empty() &&
            resJson["candidates"][0].contains("content") &&
            resJson["candidates"][0]["content"].contains("parts") &&
            !resJson["candidates"][0]["content"]["parts"].empty()) {
            return resJson["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        } else {
            
            if (resJson.contains("error")) {
                throw std::runtime_error("Gemini API Error: " + resJson["error"]["message"].get<std::string>());
            }
            throw std::runtime_error("Unexpected Gemini response structure: " + rawResponse);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Gemini parsing error: " + std::string(e.what()) + ". Response: " + rawResponse);
    }
}

std::string LLMClient::callOpenAI(const std::vector<Message>& history, const std::string& systemPrompt) {
    if (config_.api_key.empty() || config_.api_key == "YOUR_OPENAI_API_KEY") {
        throw std::runtime_error("OpenAI API key is not configured in ~/.config/ta/config.yaml");
    }
    
    std::string url = "https://api.openai.com/v1/chat/completions";
    
    nlohmann::json payload;
    payload["model"] = config_.model;
    
    
    nlohmann::json responseFormat;
    responseFormat["type"] = "json_object";
    payload["response_format"] = responseFormat;

    nlohmann::json messages = nlohmann::json::array();
    
    
    if (!systemPrompt.empty()) {
        nlohmann::json systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = systemPrompt;
        messages.push_back(systemMsg);
    }
    
    
    for (const auto& msg : history) {
        nlohmann::json m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        messages.push_back(m);
    }
    payload["messages"] = messages;

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + config_.api_key
    };
    
    std::string rawResponse = postRequest(url, payload.dump(), headers);
    
    try {
        nlohmann::json resJson = nlohmann::json::parse(rawResponse);
        if (resJson.contains("choices") && !resJson["choices"].empty() &&
            resJson["choices"][0].contains("message") &&
            resJson["choices"][0]["message"].contains("content")) {
            return resJson["choices"][0]["message"]["content"].get<std::string>();
        } else {
            
            if (resJson.contains("error")) {
                throw std::runtime_error("OpenAI API Error: " + resJson["error"]["message"].get<std::string>());
            }
            throw std::runtime_error("Unexpected OpenAI response structure: " + rawResponse);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("OpenAI parsing error: " + std::string(e.what()) + ". Response: " + rawResponse);
    }
}
