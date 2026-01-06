cmake_minimum_required(VERSION 3.15)
project(KVStore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(USE_ASIO_STANDALONE "Use standalone ASIO" ON)

# Include directories
include_directories(include)

# ASIO setup
if(USE_ASIO_STANDALONE)
    find_package(asio QUIET)
    if(NOT asio_FOUND)
        message(STATUS "ASIO not found, using bundled version")
        include_directories(third_party/asio/include)
    endif()
endif()

# Main executable
add_executable(kv_server
    src/main.cpp
    src/kv_server.cpp
    src/concurrent_hash_map.cpp
    src/write_ahead_log.cpp
)

target_compile_options(kv_server PRIVATE
    -Wall
    -Wextra
    -Werror
    -O3
)

target_link_libraries(kv_server pthread)

# Client demo executable
add_executable(kv_client
    src/client_demo.cpp
    src/kv_client.cpp
)

target_compile_options(kv_client PRIVATE
    -Wall
    -Wextra
    -Werror
    -O3
)

target_link_libraries(kv_client pthread)

# Tests
if(BUILD_TESTS)
    enable_testing()
    
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})
    
    add_executable(test_concurrent
        tests/test_concurrent.cpp
        src/concurrent_hash_map.cpp
    )
    
    target_link_libraries(test_concurrent
        ${GTEST_LIBRARIES}
        pthread
    )
    
    add_executable(test_persistence
        tests/test_persistence.cpp
        src/write_ahead_log.cpp
    )
    
    target_link_libraries(test_persistence
        ${GTEST_LIBRARIES}
        pthread
    )
    
    add_test(NAME ConcurrentHashMapTest COMMAND test_concurrent)
    add_test(NAME WriteAheadLogTest COMMAND test_persistence)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_executable(benchmark
        tests/throughput_test.cpp
        src/concurrent_hash_map.cpp
        src/write_ahead_log.cpp
        src/kv_client.cpp
    )
    
    target_link_libraries(benchmark pthread)
endif()

# Installation
install(TARGETS kv_server kv_client
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/kvstore
    FILES_MATCHING PATTERN "*.hpp"
)

# Package
include(CPack)
