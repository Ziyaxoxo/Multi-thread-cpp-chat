cmake_minimum_required(VERSION 3.10)
project(ChatApp)

# Use modern C++
set(CMAKE_CXX_STANDARD 17)

# Find the Boost system library (required for Asio)
find_package(Boost REQUIRED COMPONENTS system)

# Build the Server
add_executable(server server.cpp)
target_link_libraries(server Boost::system)

# Build the Client
add_executable(client client.cpp)
target_link_libraries(client Boost::system)