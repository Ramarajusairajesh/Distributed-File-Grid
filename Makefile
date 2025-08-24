# Distributed File Grid Makefile

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
INCLUDES = -I./src/include -I./src/Head_server -I./src/Cluster_Server -I./src/Health_Checker -I./generated/protos

# Libraries
LIBS = -lboost_system -lboost_thread -lboost_filesystem -lssl -lcrypto -lprotobuf -lfmt -lpthread -lhiredis

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
PROTO_DIR = protos
GENERATED_DIR = generated

# Source files
HEAD_SERVER_SOURCES = $(SRC_DIR)/Head_server/head_server.cpp \
                      $(SRC_DIR)/Head_server/server_config.cpp \
                      $(SRC_DIR)/include/file_chunks.cpp \
                      $(SRC_DIR)/include/file_search.cpp \
                      $(SRC_DIR)/include/system_resources.cpp \
                      $(SRC_DIR)/include/redis_client.cpp \
                      $(SRC_DIR)/include/metadata_manager.cpp

CLUSTER_SERVER_SOURCES = $(SRC_DIR)/Cluster_Server/cluster_server.cpp \
                         $(SRC_DIR)/Cluster_Server/server_config.cpp

HEALTH_CHECKER_SOURCES = $(SRC_DIR)/Health_Checker/health_checker.cpp

# Proto files
PROTO_FILES = $(PROTO_DIR)/chunk_server.proto \
              $(PROTO_DIR)/file_deatils.proto \
              $(PROTO_DIR)/heat_beat.proto

# Generated protobuf files
PROTO_SOURCES = $(patsubst $(PROTO_DIR)/%.proto,$(GENERATED_DIR)/%.pb.cc,$(PROTO_FILES))
PROTO_HEADERS = $(patsubst $(PROTO_DIR)/%.proto,$(GENERATED_DIR)/%.pb.h,$(PROTO_FILES))

# Object files
HEAD_SERVER_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(HEAD_SERVER_SOURCES) $(PROTO_SOURCES))
CLUSTER_SERVER_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CLUSTER_SERVER_SOURCES) $(PROTO_SOURCES))
HEALTH_CHECKER_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(HEALTH_CHECKER_SOURCES) $(PROTO_SOURCES))

# Executables
HEAD_SERVER = $(BIN_DIR)/head_server
CLUSTER_SERVER = $(BIN_DIR)/cluster_server
HEALTH_CHECKER = $(BIN_DIR)/health_checker

# Default target
all: directories $(HEAD_SERVER) $(CLUSTER_SERVER) $(HEALTH_CHECKER)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)/$(SRC_DIR)/Head_server
	@mkdir -p $(BUILD_DIR)/$(SRC_DIR)/Cluster_Server
	@mkdir -p $(BUILD_DIR)/$(SRC_DIR)/Health_Checker
	@mkdir -p $(BUILD_DIR)/$(SRC_DIR)/include
	@mkdir -p $(BUILD_DIR)/$(GENERATED_DIR)
	@mkdir -p $(BIN_DIR)

# Generate protobuf files
$(GENERATED_DIR)/%.pb.cc $(GENERATED_DIR)/%.pb.h: $(PROTO_DIR)/%.proto
	@echo "Generating protobuf files for $<"
	@mkdir -p $(GENERATED_DIR)
	@protoc --experimental_allow_proto3_optional --cpp_out=$(GENERATED_DIR) --proto_path=$(PROTO_DIR) $<

# Build head server
$(HEAD_SERVER): $(HEAD_SERVER_OBJECTS)
	@echo "Linking head server..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Build cluster server
$(CLUSTER_SERVER): $(CLUSTER_SERVER_OBJECTS)
	@echo "Linking cluster server..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Build health checker
$(HEALTH_CHECKER): $(HEALTH_CHECKER_OBJECTS)
	@echo "Linking health checker..."
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# Compile C++ source files
$(BUILD_DIR)/%.o: %.cpp $(PROTO_HEADERS)
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(GENERATED_DIR) -c $< -o $@

# Compile generated protobuf files
$(BUILD_DIR)/$(GENERATED_DIR)/%.o: $(GENERATED_DIR)/%.cc
	@echo "Compiling protobuf file $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(GENERATED_DIR) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(GENERATED_DIR)

# Install dependencies (Ubuntu/Debian)
install-deps:
	@echo "Installing dependencies..."
	@sudo apt-get update
	@	sudo apt-get install -y build-essential cmake pkg-config \
		libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler \
		libfmt-dev libasio-dev libhiredis-dev redis-server

# Install dependencies (Arch Linux)
install-deps-arch:
	@echo "Installing dependencies for Arch Linux..."
	@	sudo pacman -S --needed base-devel cmake pkg-config \
		boost openssl protobuf fmt asio hiredis redis

# Run tests
test: all
	@echo "Running tests..."
	@cd test && ./run_tests.sh

# Start all services
start: all
	@echo "Starting distributed file grid services..."
	@./scripts/start_services.sh

# Stop all services
stop:
	@echo "Stopping distributed file grid services..."
	@./scripts/stop_services.sh

# Docker targets
docker-build:
	@echo "Building Docker images..."
	@docker-compose build

docker-up:
	@echo "Starting Docker services..."
	@docker-compose up -d

docker-down:
	@echo "Stopping Docker services..."
	@docker-compose down

# Development targets
dev: all
	@echo "Starting development environment..."
	@./scripts/dev_environment.sh

# Documentation
docs:
	@echo "Generating documentation..."
	@doxygen Doxyfile

# Package
package: all
	@echo "Creating package..."
	@mkdir -p package
	@cp -r $(BIN_DIR)/* package/
	@cp -r config package/
	@cp README.md package/
	@tar -czf distributed-file-grid.tar.gz package/
	@rm -rf package

.PHONY: all clean install-deps install-deps-arch test start stop docker-build docker-up docker-down dev docs package directories
