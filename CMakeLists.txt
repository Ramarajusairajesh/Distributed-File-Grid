cmake_minimum_required(VERSION 3.16)
project(DistributedFileGrid VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Find fmt library
find_package(fmt REQUIRED)

# Find asio (header-only)
find_package(asio REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/Head_server)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/Cluster_Server)
include_directories(${Protobuf_INCLUDE_DIRS})

# Generate protobuf files
set(PROTO_FILES
    protos/chunk_server.proto
    protos/file_deatils.proto
    protos/heat_beat.proto
)

# Generate C++ files from protobuf
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Head Server sources
set(HEAD_SERVER_SOURCES
    src/Head_server/head_server.cpp
    src/Head_server/server_config.cpp
    src/include/file_chunks.cpp
    src/include/file_search.cpp
    src/include/system_resources.cpp
    ${PROTO_SRCS}
)

# Cluster Server sources
set(CLUSTER_SERVER_SOURCES
    src/Cluster_Server/cluster_server.cpp
    src/Cluster_Server/server_config.cpp
    ${PROTO_SRCS}
)

# Health Checker sources
set(HEALTH_CHECKER_SOURCES
    src/Health_Checker/health_checker.cpp
    ${PROTO_SRCS}
)

# Create executables
add_executable(head_server ${HEAD_SERVER_SOURCES})
add_executable(cluster_server ${CLUSTER_SERVER_SOURCES})
add_executable(health_checker ${HEALTH_CHECKER_SOURCES})

# Link libraries for head_server
target_link_libraries(head_server
    ${Protobuf_LIBRARIES}
    Boost::system
    Boost::thread
    Boost::filesystem
    OpenSSL::SSL
    OpenSSL::Crypto
    fmt::fmt
    asio::asio
    Threads::Threads
)

# Link libraries for cluster_server
target_link_libraries(cluster_server
    ${Protobuf_LIBRARIES}
    Boost::system
    Boost::thread
    Boost::filesystem
    OpenSSL::SSL
    OpenSSL::Crypto
    fmt::fmt
    asio::asio
    Threads::Threads
)

# Link libraries for health_checker
target_link_libraries(health_checker
    ${Protobuf_LIBRARIES}
    Boost::system
    Boost::thread
    fmt::fmt
    asio::asio
    Threads::Threads
)

# Set compiler flags
target_compile_options(head_server PRIVATE -Wall -Wextra -O2)
target_compile_options(cluster_server PRIVATE -Wall -Wextra -O2)
target_compile_options(health_checker PRIVATE -Wall -Wextra -O2)

# Install targets
install(TARGETS head_server cluster_server health_checker
    RUNTIME DESTINATION bin
)

# Install configuration files
install(DIRECTORY config/ DESTINATION etc/distributed-file-grid)

# Create package
set(CPACK_PACKAGE_NAME "DistributedFileGrid")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Distributed File Storage System")
set(CPACK_GENERATOR "DEB;RPM;TGZ")

include(CPack) 