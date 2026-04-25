cmake_minimum_required(VERSION 3.14)
project(aegis_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_executable(aegis_core
    src/main.cpp
    src/telemetry.cpp
    src/json_utils.cpp
    src/udp_socket.cpp
)

target_include_directories(aegis_core PRIVATE include)
target_link_libraries(aegis_core PRIVATE nlohmann_json::nlohmann_json)
