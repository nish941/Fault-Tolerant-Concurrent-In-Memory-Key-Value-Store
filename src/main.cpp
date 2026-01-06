#include <iostream>
#include <csignal>
#include <memory>
#include "kv_server.hpp"
#include "config.hpp"

std::unique_ptr<kvstore::KVServer> server;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (server) {
        server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Load configuration
    kvstore::Config config;
    if (argc > 1) {
        config = kvstore::ConfigManager::loadFromFile(argv[1]);
    } else {
        config = kvstore::ConfigManager::loadFromFile();
    }
    
    // Create and start server
    server = std::make_unique<kvstore::KVServer>(config);
    
    std::cout << "=== Fault-Tolerant Concurrent KV Store ===" << std::endl;
    std::cout << "Port: " << config.server_port << std::endl;
    std::cout << "Segments: " << config.num_segments << std::endl;
    std::cout << "WAL: " << config.wal_file << std::endl;
    std::cout << "Max connections: " << config.max_connections << std::endl;
    std::cout << "==========================================" << std::endl;
    
    server->start();
    
    // Keep main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print stats periodically
        static int counter = 0;
        if (++counter % 10 == 0) {
            std::cout << "Active connections: " << server->getConnectionCount()
                      << ", Items: " << server->getItemCount() << std::endl;
        }
    }
    
    return 0;
}
