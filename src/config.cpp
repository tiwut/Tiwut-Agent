#include "config.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

std::string Config::getConfigPath() const {
    const char* home = std::getenv("HOME");
    if (!home) {
        return "config.yaml"; 
    }
    return std::string(home) + "/.config/ta/config.yaml";
}

bool Config::load() {
    std::string configPath = getConfigPath();
    fs::path p(configPath);
    
    
    if (!fs::exists(p)) {
        
        try {
            fs::create_directories(p.parent_path());
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not create config directory: " << e.what() << std::endl;
        }

        
        std::ofstream outFile(configPath);
        if (outFile.is_open()) {
            outFile << "# Tiwut Agent Config File\n"
                    << "# Configure API keys and agent behaviors here\n\n"
                    << "api_provider: gemini       # options: gemini, ollama, openai\n"
                    << "api_key: \"YOUR_GEMINI_API_KEY\"  # Only required for gemini or openai\n"
                    << "api_url: \"http://localhost:11434\" # Ollama default API url\n"
                    << "model: gemini-1.5-flash    # default model (e.g., gemini-1.5-flash, llama3, gpt-4o)\n\n"
                    << "safety:\n"
                    << "  ask_permission: always   # options: always, never, sudo_only\n\n"
                    << "web_search_engine: duckduckgo # default web search engine\n";
            outFile.close();
            std::cout << "\n\033[33m[Config] Created default configuration file at: " << configPath << "\033[0m\n"
                      << "\033[33m[Config] Please open this file and configure your API key or Ollama connection details before using.\033[0m\n\n";
        } else {
            std::cerr << "Error: Could not write default configuration to " << configPath << std::endl;
            return false;
        }
    }

    
    try {
        YAML::Node doc = YAML::LoadFile(configPath);
        
        if (doc["api_provider"]) api_provider = doc["api_provider"].as<std::string>();
        else api_provider = "gemini";
        
        if (doc["api_key"]) api_key = doc["api_key"].as<std::string>();
        else api_key = "";
        
        if (doc["api_url"]) api_url = doc["api_url"].as<std::string>();
        else api_url = "http://localhost:11434";
        
        if (doc["model"]) model = doc["model"].as<std::string>();
        else model = "gemini-1.5-flash";
        
        if (doc["safety"] && doc["safety"]["ask_permission"]) {
            safety.ask_permission = doc["safety"]["ask_permission"].as<std::string>();
        } else {
            safety.ask_permission = "always";
        }
        
        if (doc["web_search_engine"]) web_search_engine = doc["web_search_engine"].as<std::string>();
        else web_search_engine = "duckduckgo";

    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration from " << configPath << ": " << e.what() << std::endl;
        return false;
    }

    return true;
}
