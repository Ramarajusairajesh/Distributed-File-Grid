cmake_minimum_required(VERSION 3.10)
project(Distributed-File-Grid)

set(CMAKE_CXX_STANDARD 17)

add_executable(Distributed-File-Grid src/main.cpp)

target_link_libraries(Distributed-File-Grid pthread)
