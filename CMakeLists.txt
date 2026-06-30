cmake_minimum_required(VERSION 3.16)

project(graphmeta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(CURL REQUIRED)

add_executable(graphmeta src/basic_cached.cpp)
add_executable(graphsend src/advanced.cpp)

target_link_libraries(graphmeta PRIVATE
    nlohmann_json::nlohmann_json
    yaml-cpp
    CURL::libcurl
)

target_link_libraries(graphsend PRIVATE
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

set_target_properties(graphmeta PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_target_properties(graphsend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
