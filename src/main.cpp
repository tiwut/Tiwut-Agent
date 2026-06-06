#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <readline/readline.h>
#include <readline/history.h>
#include "config.hpp"
#include "agent.hpp"

void printBanner() {
    std::cout << "\033[35;1m"
              << " ___________________________________________________________ \n"
              << "|                          TIWUT               {#} {-} {x}  |\n"
              << "|-----------------------------------------------------------|\n"
              << "|                                                           |\n"
              << "|      ##         #######     ######  #      #  ########### |\n"
              << "|     #  #      ##            #       # #    #       #      |\n"
              << "|    #    #    ##             #____   #  #   #       #      |\n"
              << "|   ########   ##    ######   #       #   #  #       #      |\n"
              << "|  #        #   ##      # #   #       #    # #       #      |\n"
              << "| #          #    ######  #   ######  #      #       #      |\n"
              << "|                                                           |\n"
              << "|                 Pure Code. Total Freedom.                 |\n"
              << "|           ______________________________________          |\n"
              << "|           |                                    |          |\n"
              << "|           | Website : https://tiwut.org/       |          |\n"
              << "|           | GitHub  : https://github.com/tiwut |          |\n"
              << "|           |____________________________________|          |\n"
              << "|___________________________________________________________|\n"
              << "\033[0m";
}

void printHelp() {
    std::cout << "Usage:\n"
              << "  ta                     Start interactive agent session\n"
              << "  ta \"<task/question>\"   Run a single task directly and exit\n"
              << "  ta -h | --help         Show this help information\n\n"
              << "Configuration file: ~/.config/ta/config.yaml\n";
}

int main(int argc, char* argv[]) {
    
    Config config;
    if (!config.load()) {
        std::cerr << "Fatal: Failed to load configuration.\n";
        return 1;
    }
    
    
    if (config.api_provider == "gemini") {
        if (config.api_key == "YOUR_GEMINI_API_KEY" || config.api_key.empty()) {
            std::cerr << "\033[31;1mError: Please update ~/.config/ta/config.yaml with your Gemini API key.\033[0m\n";
            return 1;
        }
    } else if (config.api_provider == "openai") {
        if (config.api_key == "YOUR_OPENAI_API_KEY" || config.api_key.empty()) {
            std::cerr << "\033[31;1mError: Please update ~/.config/ta/config.yaml with your OpenAI API key.\033[0m\n";
            return 1;
        }
    }

    
    if (argc > 1) {
        std::string firstArg = argv[1];
        if (firstArg == "-h" || firstArg == "--help") {
            printHelp();
            return 0;
        }

        
        
        std::string task = argv[1];
        for (int i = 2; i < argc; ++i) {
            task += " ";
            task += argv[i];
        }

        printBanner();
        std::cout << "\033[32;1m[Task]:\033[0m " << task << "\n\n";

        try {
            Agent agent(config);
            agent.run(task);
        } catch (const std::exception& e) {
            std::cerr << "\033[31;1mFatal Error during task execution: " << e.what() << "\033[0m\n";
            return 1;
        }
        return 0;
    }

    
    printBanner();
    std::cout << "Interactive session started. Type 'exit' or 'quit' to end.\n"
              << "Safety Mode: " << config.safety.ask_permission << " (asks before executing commands)\n"
              << "LLM Model  : " << config.model << " (" << config.api_provider << ")\n\n";

    Agent agent(config);

    while (true) {
        char* raw_line = readline("\033[35;1mta> \033[0m");
        if (!raw_line) {
            std::cout << "\nGoodbye!\n";
            break; 
        }

        std::string input(raw_line);
        free(raw_line);

        if (input.empty()) {
            continue;
        }

        add_history(input.c_str());

        if (input == "exit" || input == "quit") {
            std::cout << "Goodbye!\n";
            break;
        }

        try {
            agent.run(input);
        } catch (const std::exception& e) {
            std::cerr << "\033[31;1mError: " << e.what() << "\033[0m\n";
        }
    }

    return 0;
}
