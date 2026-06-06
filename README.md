# Tiwut Agent (`ta`)

> **Pure Code. Total Freedom.**
> A terminal-based autonomous AI agent that operates directly on your computer.

```
 ___________________________________________________________
|                          TIWUT               {#} {-} {x}  |
|-----------------------------------------------------------|
|                                                           |
|      ##         #######     ######  #      #  ########### |
|     #  #      ##            #       # #    #       #      |
|    #    #    ##             #____   #  #   #       #      |
|   ########   ##    ######   #       #   #  #       #      |
|  #        #   ##      # #   #       #    # #       #      |
| #          #    ######  #   ######  #      #       #      |
|                                                           |
|                 Pure Code. Total Freedom.                 |
|           ______________________________________          |
|           |                                    |          |
|           | Website : https://tiwut.org/       |          |
|           | GitHub  : https://github.com/tiwut |          |
|           |____________________________________|          |
|___________________________________________________________|
```
---

## What is Tiwut Agent?

`ta` is a native C++17 terminal application that connects any large language model (LLM) — local via **Ollama**, or cloud-based via **Gemini** or **OpenAI** — to your actual Linux system. Instead of just chatting, the agent:

- **Executes shell commands** and reads their output
- **Creates, reads, and modifies files** on disk
- **Searches and navigates** your filesystem
- **Controls your mouse and keyboard** (GUI automation via xdotool)
- **Takes screenshots** of your display
- **Searches the web** (via DuckDuckGo, no API key required)
- **Generates images** using AI (via Pollinations.ai, no API key required)
- **Asks you questions** only when it genuinely cannot proceed on its own

The agent works in an autonomous **ReAct loop**: it thinks, picks a tool, executes it, observes the result, and repeats — until the task is done.

---

## How It Works

```
User types a task
       │
       ▼
  Agent::run()
       │
  ┌────▼──────────────────────────────────────────────┐
  │              ReAct Loop (up to 30 steps)           │
  │                                                    │
  │  1. Build message history + system prompt          │
  │  2. Send to LLM (Ollama / Gemini / OpenAI)         │
  │  3. Receive raw JSON response                      │
  │  4. Extract & sanitize JSON                        │
  │  5. Parse: read "thought", "action", "args"        │
  │  6. Ask user for permission (if safety = always)   │
  │  7. Execute the tool                               │
  │  8. Append result to message history               │
  │  9. Repeat until "final_answer" is returned        │
  └────────────────────────────────────────────────────┘
       │
       ▼
  Print final answer to terminal
```

The LLM never runs code directly — it only **proposes** a structured JSON action. The C++ agent validates the JSON, checks permissions, executes the tool, and feeds the real output back into the next LLM call. This keeps the model grounded in reality.

---

## Architecture Overview

```
Tiwut-Agent/
├── src/
│   ├── main.cpp          — CLI entry point, readline interactive loop
│   ├── agent.cpp         — Core ReAct loop, JSON parsing, tool dispatch
│   ├── agent.hpp         — Agent class declaration
│   ├── llm_client.cpp    — HTTP calls to Ollama / Gemini / OpenAI
│   ├── llm_client.hpp    — LLMClient class declaration
│   ├── tools.cpp         — Low-level tool implementations (fs, shell, GUI bridge)
│   ├── tools.hpp         — Tools namespace declaration
│   ├── config.cpp        — YAML config loader + default file writer
│   └── config.hpp        — Config struct
├── scripts/
│   └── gui_helper.py     — Python bridge: web search, screenshots, GUI, image gen
├── CMakeLists.txt        — CMake build configuration
└── README.md             — This file
```

### Component Roles

| Component | Language | Role |
|---|---|---|
| `main.cpp` | C++ | Parses CLI args, runs readline interactive loop, calls `Agent::run()` |
| `agent.cpp` | C++ | ReAct loop, JSON extraction/sanitization, permission checks, tool dispatch |
| `llm_client.cpp` | C++ | cURL-based HTTP client for Ollama, Gemini, and OpenAI APIs |
| `tools.cpp` | C++ | Shell command execution, file I/O, directory listing, file search; bridges to `gui_helper.py` |
| `gui_helper.py` | Python | Web search (DuckDuckGo), screenshot (GNOME DBus/CLI), GUI automation (xdotool), image generation (Pollinations.ai) |
| `config.cpp` | C++ | Reads `~/.config/ta/config.yaml` via yaml-cpp; writes sensible defaults on first run |

---

## Available Tools

The agent can call any of these 10 tools in each step of its loop:

| Tool | What it does |
|---|---|
| `execute_command` | Runs any shell command; full stdout+stderr returned |
| `write_file` | Creates or overwrites a file at an absolute path |
| `read_file` | Reads a text file and returns its contents |
| `list_directory` | Lists files and folders in a directory |
| `search_files` | Recursively searches a directory by regex filename pattern |
| `capture_screen` | Takes a screenshot (GNOME Wayland via DBus, fallback: `gnome-screenshot`) |
| `control_gui` | Simulates mouse movements, clicks, keyboard typing and keypresses via `xdotool` |
| `web_search` | Queries DuckDuckGo HTML and parses structured results (no API key needed) |
| `generate_image` | Generates an AI image via Pollinations.ai and saves it to disk (no API key needed) |
| `ask_user` | Prompts the user for information via readline in the terminal |

### How a Tool Call Looks Internally

The LLM must respond with a JSON object like:

```json
{
  "thought": "I need to check what OS the user is running.",
  "action": "execute_command",
  "args": { "cmd": "uname -a && cat /etc/os-release" }
}
```

The agent parses this, prompts for confirmation (if required), runs the command, and feeds the output back as a `user` message:

```
Tool 'execute_command' result:
Linux bazzite 6.19.14 ...
```

This output becomes the next input to the LLM so it can reason about what to do next.

---

## Safety System

The safety system is configured in `~/.config/ta/config.yaml` under the `safety:` key.

| Mode | Behavior |
|---|---|
| `always` | Asks for confirmation **before every tool call** — command, file write, web search, etc. |
| `sudo_only` | Only asks for confirmation when the command contains `sudo` |
| `never` | Executes everything autonomously without prompting |

When permission is requested, the agent displays:

```
[Security Confirmation Needed]
Tool  : execute_command
Params: ls -la /home/tiwut
Execute this action? [Y/n]:
```

Pressing **Enter** or **Y** approves. Typing **n** denies the action, and the agent is informed so it can try an alternative approach.

---

## Configuration

The configuration file is auto-created at first run:

```
~/.config/ta/config.yaml
```

```yaml
# Tiwut Agent Config File
api_provider: gemini       # options: gemini, ollama, openai
api_key: "YOUR_GEMINI_API_KEY"  # Only required for gemini or openai
api_url: "http://localhost:11434" # Ollama default API URL
model: gemini-1.5-flash    # Model name (e.g. gemini-1.5-flash, llama3, qwen3, gpt-4o)

safety:
  ask_permission: always   # options: always, never, sudo_only

web_search_engine: duckduckgo
```

### Provider Setup

**Ollama (local, recommended for privacy):**
```yaml
api_provider: ollama
api_url: "http://localhost:11434"
model: gemma4          # or llama3, qwen3, mistral, etc.
```
Requires [Ollama](https://ollama.com/) installed and running. Pull a model with `ollama pull gemma4`.

**Google Gemini:**
```yaml
api_provider: gemini
api_key: "AIza..."
model: gemini-1.5-flash
```
Get an API key from [Google AI Studio](https://aistudio.google.com/).

**OpenAI:**
```yaml
api_provider: openai
api_key: "sk-..."
model: gpt-4o
```

---

## Building from Source

### Dependencies

| Dependency | Purpose | Install (Fedora/Bazzite) | Install (Debian/Ubuntu) |
|---|---|---|---|
| `cmake` ≥ 3.16 | Build system | `sudo dnf install cmake` | `sudo apt install cmake` |
| `g++` / `clang++` (C++17) | Compiler | `sudo dnf install gcc-c++` | `sudo apt install g++` |
| `libcurl` | HTTP requests to LLMs | `sudo dnf install libcurl-devel` | `sudo apt install libcurl4-openssl-dev` |
| `yaml-cpp` | Config file parsing | `sudo dnf install yaml-cpp-devel` | `sudo apt install libyaml-cpp-dev` |
| `readline` | Interactive line editing | `sudo dnf install readline-devel` | `sudo apt install libreadline-dev` |
| `python3` | GUI helper bridge | Usually pre-installed | `sudo apt install python3` |
| `xdotool` *(optional)* | GUI automation | `sudo dnf install xdotool` | `sudo apt install xdotool` |

### Build Steps

```bash
git clone https://github.com/tiwut/Tiwut-Agent.git
cd Tiwut-Agent

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

The binary is at `build/ta`.

### Install System-Wide (optional)

```bash
sudo cmake --install .
# Now you can run 'ta' from anywhere
```

---

## Usage

### Interactive Mode

```bash
./ta
```

Starts a persistent session. Type your task and press Enter. Use ↑/↓ arrow keys to navigate history (powered by GNU Readline).

```
ta> what is my CPU model?
ta> please create a Python web scraper in ~/projects/scraper
ta> take a screenshot and describe what you see
ta> search the web for best Linux file manager 2025
```

Type `exit` or `quit` to leave.

### Single Task Mode

```bash
./ta "what is my IP address?"
./ta "create a hello world C++ app in /tmp/test and compile it"
```

Runs the task and exits when done — useful for scripting.

### Help

```bash
./ta --help
```

---

## Internal Pipeline — Step by Step

Here is exactly what happens when you type a task:

1. **Input** — `main.cpp` reads your input via `readline()` (supports arrow keys, history).

2. **History push** — Your message is added to `history_` as a `{role: "user", content: "..."}` message.

3. **History trim** — `agent.cpp` trims history to the last 28 messages (keeping the first task message), preventing context window overflow.

4. **LLM call** — `llm_client.cpp` constructs a JSON payload with the system prompt + full message history, then POSTs it to the configured API endpoint using cURL.
   - Ollama: `POST /api/chat` with `format: "json"` and `keep_alive: "10m"`
   - Gemini: `POST /v1beta/models/{model}:generateContent` with `responseMimeType: "application/json"`
   - OpenAI: `POST /v1/chat/completions` with `response_format: {type: "json_object"}`

5. **Response extraction** — `agent.cpp` uses a brace-counting parser (`extractJsonBlock`) to find the first valid `{...}` block in the raw response, tolerating any markdown or prose the model might emit before/after.

6. **JSON sanitization** — `sanitizeJsonString` scans every character inside JSON string values and escapes raw newlines (`\n`), carriage returns (`\r`), tabs (`\t`), bell/backspace/formfeed, and backtick characters that would break parsing.

7. **JSON parse** — `nlohmann::json::parse()` parses the cleaned string. On failure, the error and a correction instruction are fed back to the LLM (up to 4 retries per step).

8. **Thought display** — If the JSON contains a `"thought"` key, it is printed in blue to the terminal so you can follow the agent's reasoning.

9. **Final answer check** — If the JSON contains `"final_answer"`, the answer is printed in green and the loop exits.

10. **Tool dispatch** — `executeTool()` matches `"action"` to one of the 10 tool handlers. Each handler:
    - Validates required arguments
    - Expands `~` and relative paths to absolute paths
    - Calls `checkPermission()` (prompts if safety mode requires it)
    - Executes the real operation
    - Caps returned output length (6 KB for commands, 4 KB for files/web) to prevent context overflow

11. **Result feed-back** — The tool output (truncated if needed) is appended to history as a `user` message: `"Tool 'execute_command' result:\n..."`.

12. **Loop** — Back to step 3 for the next reasoning step.

---

## Failsafe & Stability Design

Every feature includes multiple independent failsafes:

### JSON Parsing
- Brace-counter extractor ignores all non-JSON text before/after the object
- Full C0 control character sanitizer prevents parse errors from raw newlines/tabs
- Backtick → single-quote replacement (LLMs often emit these in code)
- Up to **4 retries** with specific error guidance sent back to the model
- Different error messages per failure type (no JSON found vs. parse error)

### LLM Communication
- cURL retry loop: up to **3 attempts** with 3s backoff between retries
- 10s **connection timeout** — fails fast if Ollama is not running
- 240s **total timeout** — accommodates large local models on modest hardware
- `keep_alive: "10m"` on Ollama — prevents model unloading between requests (eliminates cold-start delays)

### File Operations
- `~` expansion using `getenv("HOME")` with `getpwuid()` fallback
- Relative paths resolved against `$HOME`
- Parent directories auto-created with `fs::create_directories`
- All paths converted to absolute before execution

### Context Window Management
- Message history capped at **28 messages** (first task + latest entries)
- Command output capped at **6,000 chars** (head+tail preserved)
- File content capped at **4,000 chars**
- Web search results capped at **4,000 chars**
- Tool results in history capped at **3,000 chars**

### Agent Behavior
- Max **30 steps** per task (prevents infinite loops)
- JSON failures don't count as real steps (step counter not incremented)
- Unknown tool names return an error listing all valid tools (so the model can self-correct)
- `final_answer` hallucinated as an `action` is handled gracefully
- Responses with no `action` and no `final_answer` prompt the model to proceed or conclude

### GUI Automation
- Screenshot: tries GNOME DBus API first, falls back to `gnome-screenshot` CLI
- Image generation: graceful HTTP error handling with informative messages
- Web search: 10s timeout with structured error output

---

## File Reference

| File | Description |
|---|---|
| [`src/main.cpp`](src/main.cpp) | Entry point: argument parsing, banner, readline interactive loop |
| [`src/agent.cpp`](src/agent.cpp) | ReAct loop engine: JSON extraction, sanitization, tool dispatch, history management |
| [`src/agent.hpp`](src/agent.hpp) | `Agent` class interface |
| [`src/llm_client.cpp`](src/llm_client.cpp) | HTTP client for Ollama, Gemini, and OpenAI with retry logic |
| [`src/llm_client.hpp`](src/llm_client.hpp) | `LLMClient` class and `Message` struct |
| [`src/tools.cpp`](src/tools.cpp) | Shell execution, file I/O, directory ops, Python bridge calls |
| [`src/tools.hpp`](src/tools.hpp) | `Tools` namespace declarations |
| [`src/config.cpp`](src/config.cpp) | YAML config loader, default config writer |
| [`src/config.hpp`](src/config.hpp) | `Config` and `SafetyConfig` structs |
| [`scripts/gui_helper.py`](scripts/gui_helper.py) | Python helper: DuckDuckGo search, screenshot, xdotool GUI, Pollinations.ai image gen |
| [`CMakeLists.txt`](CMakeLists.txt) | CMake build: finds curl, yaml-cpp, readline; produces `ta` binary |

---

## Sample
```bash
./ta
 ___________________________________________________________ 
|                          TIWUT               {#} {-} {x}  |
|-----------------------------------------------------------|
|                                                           |
|      ##         #######     ######  #      #  ########### |
|     #  #      ##            #       # #    #       #      |
|    #    #    ##             #____   #  #   #       #      |
|   ########   ##    ######   #       #   #  #       #      |
|  #        #   ##      # #   #       #    # #       #      |
| #          #    ######  #   ######  #      #       #      |
|                                                           |
|                 Pure Code. Total Freedom.                 |
|           ______________________________________          |
|           |                                    |          |
|           | Website : https://tiwut.org/       |          |
|           | GitHub  : https://github.com/tiwut |          |
|           |____________________________________|          |
|___________________________________________________________|
Interactive session started. Type 'exit' or 'quit' to end.
Safety Mode: always (asks before executing commands)
LLM Model  : gemma4 (ollama)

ta> please install all the necessary tools for cpp programming.
Thinking... (Step 1)
[Thought]: The user wants to install all necessary tools for C++ programming. The standard approach on a Linux system is to install the build essentials, which typically include a C++ compiler (g++), make, and related libraries.

I will use the 'execute_command' tool to run 'sudo apt install -y build-essential' as this package usually provides g++, gdb, and other necessary tools.

[Security Confirmation Needed]
Tool  : execute_command
Params: sudo apt update && sudo apt install -y build-essential
Execute this action? [Y/n]: y
[Executing] sudo apt update && sudo apt install -y build-essential

WARNING: apt does not have a stable CLI interface. Use with caution in scripts.

Hit:1 http://deb.debian.org/debian stable-backports InRelease
Hit:2 https://dl.google.com/linux/chrome-stable/deb stable InRelease
Hit:3 http://deb.debian.org/debian stable InRelease
Hit:4 https://deb.nodesource.com/node_22.x nodistro InRelease
Hit:5 http://deb.debian.org/debian stable-updates InRelease
Get:6 http://deb.debian.org/debian-security stable-security InRelease [43.4 kB]
Fetched 43.4 kB in 0s (258 kB/s)
Reading package lists...
Building dependency tree...
Reading state information...
38 packages can be upgraded. Run 'apt list --upgradable' to see them.

WARNING: apt does not have a stable CLI interface. Use with caution in scripts.

Reading package lists...
Building dependency tree...
Reading state information...
build-essential is already the newest version (12.12).
The following packages were automatically installed and are no longer required:
  apparmor       libintl-xs-perl            libterm-readkey-perl
  containerd     libmodule-find-perl        needrestart
  criu           libnet1                    python3-protobuf
  docker-buildx  libproc-processtable-perl  python3-pycriu
  libcompel1     libprotobuf-c1             runc
  libintl-perl   libsort-naturally-perl     tini
Use 'sudo apt autoremove' to remove them.

Summary:
  Upgrading: 0, Installing: 0, Removing: 0, Not Upgrading: 38

Thinking... (Step 2)
[Thought]: The previous command output confirms that 'build-essential' is already installed and is the newest version (12.12). This means the primary requirement for C++ tools has been met. I should inform the user that the tools are installed and provide guidance on how to verify the setup, such as checking the version of 'g++'.

[Security Confirmation Needed]
Tool  : execute_command
Params: g++ --version && cmake --version && make --version
Execute this action? [Y/n]: y
[Executing] g++ --version && cmake --version && make --version
g++ (Debian 14.2.0-19) 14.2.0
Copyright (C) 2024 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

cmake version 3.31.6

CMake suite maintained and supported by Kitware (kitware.com/cmake).
GNU Make 4.4.1
Built for x86_64-pc-linux-gnu
Copyright (C) 1988-2023 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

Thinking... (Step 3)
[Thought]: The output confirms that g++ (Compiler), CMake (Build system generator), and Make (Build automation tool) are all installed and functional. This completes the task of installing necessary tools for C++ programming. I can now provide a final answer to the user.

[Tiwut Agent]: All necessary tools for C++ programming, including the g++ compiler (version 14.2.0), CMake (version 3.31.6), and GNU Make (version 4.4.1), have been successfully installed and verified on the system. You are ready to start developing C++ projects.

ta> can you recommend any other tools?
Thinking... (Step 1)
[Thought]: The user is asking for recommendations for other tools for C++ programming. Since I am running in a terminal-based environment, my recommendations should focus on tools that are command-line driven, improve development efficiency, or handle common C++ tasks like testing, debugging, or code quality analysis.

I should list several key categories of tools:
1. **Build System Enhancements:** CMake was installed, but advanced build management (like Meson or Conan) could be useful.
2. **Code Quality/Analysis:** Linters and Static Analyzers are crucial (e.g., Clang-Tidy, cppcheck).
3. **Testing:** Dedicated testing frameworks (e.g., Google Test/Catch2).
4. **Package Management:** Conan (or vcpkg) for managing dependencies.
5. **Debugging/Profiling:** Valgrind for memory checking.

I will structure my answer by explaining the purpose of each tool and suggesting the next commands needed to install them if they aren't already present, making the answer highly actionable for the user.

[Tiwut Agent]: While 'build-essential' covered the core compilers ('g++'), 'cmake', and 'make', advanced C++ development relies on several complementary tools for code quality, dependency management, and robust testing. I recommend the following tools and how to install them:

1. **Dependency Manager (Conan/vcpkg):**
   * **Purpose:** When your project relies on external libraries (like Boost, OpenCV, etc.), these tools manage the compilation and linking of those dependencies, preventing 'DLL Hell' or complex manual setup.
   * **Recommendation:** Use **Conan** or **vcpkg** (Microsoft's package manager).
   * **Installation (Example for Conan):**
     '''bash
     sudo pip install conan
     '''

2. **Static Code Analyzers (Clang-Tidy, cppcheck):**
   * **Purpose:** These tools analyze your code *without* running it, finding potential bugs, style violations, and non-optimal code structures before you even compile it (shift-left testing).
   * **Recommendation:** **Clang-Tidy** (part of the LLVM suite) is excellent as it integrates well with modern build systems.
   * **Installation (Example):**
     '''bash
     sudo apt install clang-tidy
     '''

3. **Unit Testing Frameworks (Google Test/Catch2):**
   * **Purpose:** Crucial for developing reliable code. Instead of just testing the main program flow, these frameworks allow you to write isolated tests for small functions (unit tests).
   * **Recommendation:** **Google Test (GTest)** is the industry standard.
   * **Installation (Typically via package manager or Conan):**
     '''bash
     # If available via apt: 
     sudo apt install libgtest-dev
     '''

4. **Memory Profiling (Valgrind):**
   * **Purpose:** Catches memory leaks and undefined behavior (like using memory after it has been freed). It runs your compiled program and reports detailed memory usage.
   * **Installation:**
     '''bash
     sudo apt install valgrind
     '''

5. **Modern Build System (Meson/Ninja):**
   * **Purpose:** While CMake is excellent, sometimes pairing it with a faster build backend like **Ninja** (or using Meson) can significantly speed up the compilation step for large projects.
   * **Installation:**
     '''bash
     sudo apt install ninja-build
     '''

**Summary Action Plan:**
For immediate improvements, focus on installing **'valgrind'** for debugging and **'clang-tidy'** for code quality. These provide the most impactful additions to your current setup.

```
