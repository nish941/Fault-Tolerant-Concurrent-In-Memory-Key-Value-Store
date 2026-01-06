# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -pthread
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O3 -DNDEBUG

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin

# Libraries
ASIO_INCLUDE = -I/usr/include/asio
GTEST_LIB = -lgtest -lgtest_main -lpthread

# Source files
SERVER_SRCS = $(SRC_DIR)/main.cpp
CLIENT_SRCS = $(SRC_DIR)/client_demo.cpp
TEST_SRCS = $(TEST_DIR)/test_concurrent.cpp $(TEST_DIR)/test_persistence.cpp

# Targets
TARGETS = kv_server kv_client run_tests benchmark

.PHONY: all clean debug release tests benchmark

all: release

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(TARGETS)

release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(TARGETS)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Server executable
kv_server: $(SERVER_SRCS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(ASIO_INCLUDE) -I$(INC_DIR) $(SERVER_SRCS) -o $(BIN_DIR)/$@

# Client executable
kv_client: $(CLIENT_SRCS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(ASIO_INCLUDE) -I$(INC_DIR) $(CLIENT_SRCS) -o $(BIN_DIR)/$@

# Tests
run_tests: | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) $(TEST_SRCS) $(GTEST_LIB) -o $(BIN_DIR)/$@

# Benchmark
benchmark: $(TEST_DIR)/throughput_test.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) $(TEST_DIR)/throughput_test.cpp -o $(BIN_DIR)/$@

# Clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	rm -f *.log *.wal

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y g++ cmake libasio-dev libgtest-dev

# Run server
run: kv_server
	./$(BIN_DIR)/kv_server

# Run tests
test: run_tests
	./$(BIN_DIR)/run_tests

# Run benchmark
bench: benchmark
	./$(BIN_DIR)/benchmark

# Format code
format:
	find $(SRC_DIR) $(INC_DIR) $(TEST_DIR) -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Static analysis
analyze:
	cppcheck --enable=all --std=c++17 -I$(INC_DIR) $(SRC_DIR) $(TEST_DIR)

# Generate documentation (requires Doxygen)
docs:
	doxygen Doxyfile

# Package for distribution
dist: clean release
	tar -czvf kv-store-$(shell date +%Y%m%d).tar.gz \
		--exclude="*.tar.gz" \
		--exclude=".git" \
		--exclude="*.log" \
		--exclude="*.wal" \
		.
