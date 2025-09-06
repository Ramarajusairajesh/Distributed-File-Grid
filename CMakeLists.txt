cmake_minimum_required(VERSION 3.16)
project(Distributed-File-Grid VERSION 1.0)

# Set C++ standard and build type
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Enable testing
enable_testing()

# Find required packages
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)

# Find or fetch Prometheus client
include(FetchContent)
FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.1.0
)
FetchContent_MakeAvailable(prometheus-cpp)

# Make Redis++ optional
option(WITH_REDIS "Build with Redis++ support" OFF)
if(WITH_REDIS)
    find_package(redis++ REQUIRED)
    add_definitions(-DWITH_REDIS)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/include)
include_directories(${CMAKE_SOURCE_DIR}/src/protos/v1/generate)

# Generate protobuf files
file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/src/protos/v1/*.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Create executables for each service
add_executable(head_server 
    src/Head_Server/head_server.cpp
    src/Head_Server/asyc_file_recv_to_chunks.cpp
    src/Head_Server/chunck_read_to_file.cpp
    ${PROTO_SRCS}
)

target_link_libraries(head_server PRIVATE
    prometheus-cpp::core
    prometheus-cpp::pull
    prometheus-cpp::metrics
)

# Cluster server with metrics support
add_executable(cluster_server 
    src/Cluster_Server/cluster_server.cpp
    src/Cluster_Server/async_file_recv.cpp
    src/Cluster_Server/metrics_exporter.cpp
    ${PROTO_SRCS}
)

target_link_libraries(cluster_server PRIVATE
    prometheus-cpp::core
    prometheus-cpp::pull
    prometheus-cpp::metrics
)

add_executable(health_checker
    src/Health_Checker/health_checker.cpp
    ${PROTO_SRCS}
)

# Link libraries
target_link_libraries(head_server 
    ${Protobuf_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
    pthread
)

target_link_libraries(cluster_server 
    ${Protobuf_LIBRARIES}
    pthread
)

target_link_libraries(health_checker 
    ${Protobuf_LIBRARIES}
    pthread
)

# Add unit tests
option(BUILD_TESTS "Build test suite" ON)
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    
    # Add test executables
    add_executable(simple_heartbeat_test
        UnitTesting/simple_heartbeat_test.cpp
    )
    
    add_executable(optimized_heartbeat_test
        UnitTesting/optimized_heartbeat_test.cpp
    )
    
    # Link test dependencies
    target_link_libraries(simple_heartbeat_test
        PRIVATE
        GTest::GTest
        GTest::Main
        pthread
    )
    
    target_link_libraries(optimized_heartbeat_test
        PRIVATE
        GTest::GTest
        GTest::Main
        pthread
    )
    
    # Add tests
    add_test(NAME SimpleHeartbeatTest COMMAND simple_heartbeat_test)
    add_test(NAME OptimizedHeartbeatTest COMMAND optimized_heartbeat_test)
    
    # Add a test target that can be run with 'make test'
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS heartbeat_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Include directories for protobuf
target_include_directories(head_server PRIVATE ${Protobuf_INCLUDE_DIRS})
target_include_directories(cluster_server PRIVATE ${Protobuf_INCLUDE_DIRS})
target_include_directories(health_checker PRIVATE ${Protobuf_INCLUDE_DIRS})

# Include directories and libraries for redis++
if(WITH_REDIS)
    target_include_directories(head_server PRIVATE ${REDIS_PLUS_PLUS_INCLUDE_DIRS})
    target_compile_options(head_server PRIVATE ${REDIS_PLUS_PLUS_CFLAGS_OTHER})
    target_link_libraries(head_server PRIVATE redis++::redis++)
endif()
target_compile_options(cluster_server PRIVATE -fcoroutines)
target_compile_options(health_checker PRIVATE -fcoroutines)
