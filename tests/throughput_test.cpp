#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include "concurrent_hash_map.hpp"
#include "kv_client.hpp"

void benchmarkConcurrentHashMap() {
    const int num_threads = 8;
    const int num_operations = 100000;
    
    kvstore::ConcurrentHashMap<std::string, std::string> map(128);
    std::atomic<int> successful_operations{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < num_operations; ++i) {
                std::string key = "thread_" + std::to_string(t) + "_key_" + std::to_string(i);
                std::string value = "value_" + std::to_string(i);
                
                if (t % 4 == 0) {
                    // Insert
                    if (map.insert(key, value)) {
                        successful_operations++;
                    }
                } else if (t % 4 == 1) {
                    // Get
                    std::string result;
                    if (map.find(key, result)) {
                        successful_operations++;
                    }
                } else if (t % 4 == 2) {
                    // Exists
                    if (map.exists(key)) {
                        successful_operations++;
                    }
                } else {
                    // Erase (if exists)
                    if (map.erase(key)) {
                        successful_operations++;
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double ops_per_sec = (num_threads * num_operations * 1000.0) / duration.count();
    
    std::cout << "=== Concurrent HashMap Benchmark ===" << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Operations per thread: " << num_operations << std::endl;
    std::cout << "Total operations: " << num_threads * num_operations << std::endl;
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << ops_per_sec << " ops/sec" << std::endl;
    std::cout << "Successful operations: " << successful_operations << std::endl;
    std::cout << "===================================" << std::endl;
}

void benchmarkClientServer() {
    // This test assumes server is running on localhost:6379
    
    kvstore::KVClient client;
    
    // Warm up
    for (int i = 0; i < 1000; ++i) {
        client.put("warmup_key_" + std::to_string(i), "warmup_value");
    }
    
    // Benchmark PUT operations
    const int num_puts = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_puts; ++i) {
        client.put("bench_key_" + std::to_string(i), 
                  "bench_value_" + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double puts_per_sec = (num_puts * 1000.0) / duration.count();
    
    std::cout << "=== Client-Server Benchmark ===" << std::endl;
    std::cout << "PUT operations: " << num_puts << std::endl;
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << puts_per_sec << " PUTs/sec" << std::endl;
    
    // Benchmark GET operations
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_puts; ++i) {
        client.get("bench_key_" + std::to_string(i));
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double gets_per_sec = (num_puts * 1000.0) / duration.count();
    
    std::cout << "GET operations: " << num_puts << std::endl;
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << gets_per_sec << " GETs/sec" << std::endl;
    std::cout << "=================================" << std::endl;
}

void concurrentClientTest() {
    const int num_clients = 50;
    const int ops_per_client = 2000;
    
    std::atomic<int> completed_ops{0};
    std::vector<std::thread> clients;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int c = 0; c < num_clients; ++c) {
        clients.emplace_back([&, c]() {
            kvstore::KVClient client;
            
            for (int i = 0; i < ops_per_client; ++i) {
                std::string key = "client_" + std::to_string(c) + "_key_" + std::to_string(i);
                
                if (i % 3 == 0) {
                    client.put(key, "value");
                } else if (i % 3 == 1) {
                    client.get(key);
                } else {
                    client.exists(key);
                }
                
                completed_ops++;
            }
        });
    }
    
    for (auto& client : clients) {
        client.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double total_ops = num_clients * ops_per_client;
    double ops_per_sec = (total_ops * 1000.0) / duration.count();
    
    std::cout << "=== Concurrent Client Test ===" << std::endl;
    std::cout << "Clients: " << num_clients << std::endl;
    std::cout << "Operations per client: " << ops_per_client << std::endl;
    std::cout << "Total operations: " << total_ops << std::endl;
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << ops_per_sec << " ops/sec" << std::endl;
    std::cout << "==============================" << std::endl;
}

int main() {
    std::cout << "Running throughput benchmarks..." << std::endl;
    
    benchmarkConcurrentHashMap();
    std::cout << std::endl;
    
    // Uncomment to run client-server benchmarks
    // (requires server to be running)
    // benchmarkClientServer();
    // std::cout << std::endl;
    
    // concurrentClientTest();
    
    return 0;
}
