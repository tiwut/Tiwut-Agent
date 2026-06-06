#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

struct SafetyConfig {
    std::string ask_permission; 
};

struct Config {
    std::string api_provider;      
    std::string api_key;           
    std::string api_url;           
    std::string model;             
    SafetyConfig safety;           
    std::string web_search_engine; 

    
    bool load();
    
    
    std::string getConfigPath() const;
};

#endif 
