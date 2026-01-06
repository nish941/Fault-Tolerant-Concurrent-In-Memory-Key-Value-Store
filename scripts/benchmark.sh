
#!/bin/bash

# KV Store Benchmark Script
# Usage: ./benchmark.sh [num_threads] [num_operations]

set -e

# Default values
NUM_THREADS=${1:-8}
NUM_OPS=${2:-100000}
SERVER_PORT=6379
SERVER_HOST="127.0.0.1"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== KV Store Benchmark ===${NC}"
echo "Threads: $NUM_THREADS"
echo "Operations per thread: $NUM_OPS"
echo "Total operations: $((NUM_THREADS * NUM_OPS))"
echo ""

# Check if server is running
check_server() {
    if ! nc -z $SERVER_HOST $SERVER_PORT 2>/dev/null; then
        echo -e "${RED}Error: Server is not running on $SERVER_HOST:$SERVER_PORT${NC}"
        echo "Start the server with: ./kv_server"
        exit 1
    fi
}

# Run benchmark
run_benchmark() {
    local benchmark_type=$1
    local script_name=""
    
    case $benchmark_type in
        "put")
            script_name="benchmark_put.cpp"
            ;;
        "get")
            script_name="benchmark_get.cpp"
            ;;
        "mixed")
            script_name="benchmark_mixed.cpp"
            ;;
        *)
            echo "Unknown benchmark type: $benchmark_type"
            return 1
            ;;
    esac
    
    # Create benchmark program
    cat > /tmp/$script_name << 'EOF'
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include "kv_client.hpp"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <num_ops>" << std::endl;
        return 1;
    }
    
    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    int num_ops = std::stoi(argv[3]);
    
    kvstore::KVClient client(host, port);
    
    // Warm up
    for (int i = 0; i < 100; ++i) {
        client.put("warmup_" + std::to_string(i), "warmup_value");
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_ops; ++i) {
        std::string key = "bench_key_" + std::to_string(i);
        std::string value = "bench_value_" + std::to_string(i);
        
        if (std::string(argv[0]).find("put") != std::string::npos) {
            client.put(key, value);
        } else if (std::string(argv[0]).find("get") != std::string::npos) {
            client.get(key);
        } else {
            // Mixed workload
            if (i % 3 == 0) {
                client.put(key, value);
            } else if (i % 3 == 1) {
                client.get(key);
            } else {
                client.exists(key);
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_sec = (num_ops * 1000.0) / duration.count();
    
    std::cout << duration.count() << "," << ops_per_sec;
    
    return 0;
}
EOF
    
    # Compile and run
    g++ -std=c++17 -O3 -I./include /tmp/$script_name -o /tmp/benchmark_$benchmark_type -lpthread
    /tmp/benchmark_$benchmark_type $SERVER_HOST $SERVER_PORT $NUM_OPS
}

# Concurrent benchmark
run_concurrent_benchmark() {
    echo -e "${YELLOW}Running concurrent benchmark with $NUM_THREADS threads...${NC}"
    
    # Create concurrent benchmark
    cat > /tmp/benchmark_concurrent.cpp << 'EOF'
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include "kv_client.hpp"

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <num_threads> <ops_per_thread>" << std::endl;
        return 1;
    }
    
    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    int num_threads = std::stoi(argv[3]);
    int ops_per_thread = std::stoi(argv[4]);
    
    std::vector<std::thread> threads;
    std::atomic<int> completed_ops{0};
    std::atomic<long long> total_time{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            kvstore::KVClient client(host, port);
            
            auto thread_start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "thread_" + std::to_string(t) + "_key_" + std::to_string(i);
                
                if (t % 4 == 0) {
                    client.put(key, "value");
                } else if (t % 4 == 1) {
                    client.get(key);
                } else if (t % 4 == 2) {
                    client.exists(key);
                } else {
                    client.del(key);
                }
                
                completed_ops++;
            }
            
            auto thread_end = std::chrono::high_resolution_clock::now();
            auto thread_duration = std::chrono::duration_cast<std::chrono::milliseconds>(thread_end - thread_start);
            total_time += thread_duration.count();
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double total_ops = num_threads * ops_per_thread;
    double ops_per_sec = (total_ops * 1000.0) / duration.count();
    double avg_latency = (total_time * 1.0) / total_ops;
    
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << ops_per_sec << " ops/sec" << std::endl;
    std::cout << "Average latency: " << avg_latency << " ms/op" << std::endl;
    std::cout << "Completed operations: " << completed_ops << std::endl;
    
    return 0;
}
EOF
    
    g++ -std=c++17 -O3 -I./include /tmp/benchmark_concurrent.cpp -o /tmp/benchmark_concurrent -lpthread
    /tmp/benchmark_concurrent $SERVER_HOST $SERVER_PORT $NUM_THREADS $NUM_OPS
}

# Main benchmark sequence
main() {
    check_server
    
    echo -e "${GREEN}1. Single-threaded PUT benchmark${NC}"
    result=$(run_benchmark "put")
    time=$(echo $result | cut -d',' -f1)
    throughput=$(echo $result | cut -d',' -f2)
    echo "Time: ${time}ms, Throughput: ${throughput} ops/sec"
    
    echo -e "\n${GREEN}2. Single-threaded GET benchmark${NC}"
    result=$(run_benchmark "get")
    time=$(echo $result | cut -d',' -f1)
    throughput=$(echo $result | cut -d',' -f2)
    echo "Time: ${time}ms, Throughput: ${throughput} ops/sec"
    
    echo -e "\n${GREEN}3. Single-threaded MIXED benchmark${NC}"
    result=$(run_benchmark "mixed")
    time=$(echo $result | cut -d',' -f1)
    throughput=$(echo $result | cut -d',' -f2)
    echo "Time: ${time}ms, Throughput: ${throughput} ops/sec"
    
    echo -e "\n${GREEN}4. Multi-threaded benchmark${NC}"
    run_concurrent_benchmark
    
    echo -e "\n${GREEN}=== Benchmark Complete ===${NC}"
}

# Run main function
main "$@"
