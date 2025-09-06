cmake_minimum_required(VERSION 3.10)
project(HeartbeatTests)

# Find required packages
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(GTest REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${Protobuf_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

# Add the test executable
add_executable(heartbeat_tests 
    test_heartbeat.cpp
)

add_executable(simple_heartbeat_test
    simple_heartbeat_test.cpp
)

# Optimized heartbeat test
add_executable(optimized_heartbeat_test
    optimized_heartbeat_test.cpp
)

# Link libraries
target_link_libraries(heartbeat_tests
    PRIVATE
    GTest::GTest
    GTest::Main
    pthread
    protobuf::libprotobuf
    gRPC::grpc++
)

target_link_libraries(simple_heartbeat_test
    PRIVATE
    GTest::GTest
    GTest::Main
    pthread
    gRPC::grpc++
)

# Link optimized heartbeat test
target_link_libraries(optimized_heartbeat_test
    PRIVATE
    GTest::GTest
    GTest::Main
    pthread
)

# Add tests
enable_testing()
add_test(NAME SimpleHeartbeatTest COMMAND simple_heartbeat_test)
add_test(NAME OptimizedHeartbeatTest COMMAND optimized_heartbeat_test)
