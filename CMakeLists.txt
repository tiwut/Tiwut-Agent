cmake_minimum_required(VERSION 3.16)
project(TiwutAgent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)
endif()

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/llm_client.cpp
    src/tools.cpp
    src/agent.cpp
)

add_executable(ta ${SOURCES})

target_include_directories(ta PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
)

find_library(READLINE_LIBRARY NAMES readline REQUIRED)

if(yaml-cpp_FOUND)
    target_link_libraries(ta PRIVATE ${CURL_LIBRARIES} yaml-cpp ${READLINE_LIBRARY})
else()
    target_include_directories(ta PRIVATE ${YAML_CPP_INCLUDE_DIRS})
    target_link_libraries(ta PRIVATE ${CURL_LIBRARIES} ${YAML_CPP_LIBRARIES} ${READLINE_LIBRARY})
endif()

install(TARGETS ta DESTINATION bin)
