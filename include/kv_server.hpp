#ifndef KV_STORE_SERVER_HPP
#define KV_STORE_SERVER_HPP

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <asio.hpp>
#include "concurrent_hash_map.hpp"
#include "write_ahead_log.hpp"
#include "types.hpp"

namespace kvstore {

using asio::ip::tcp;

class KVServer {
private:
    asio::io_context io_context;
    tcp::acceptor acceptor;
    std::unique_ptr<std::thread> io_thread;
    std::atomic<bool> running{false};
    
    ConcurrentHashMap<std::string, std::string> store;
    WriteAheadLog wal;
    Config config;
    
    std::vector<std::thread> worker_threads;
    std::atomic<size_t> current_connections{0};
    
    void startAccept() {
        auto socket = std::make_shared<tcp::socket>(io_context);
        
        acceptor.async_accept(*socket,
            [this, socket](const asio::error_code& error) {
                if (!error) {
                    if (current_connections < config.max_connections) {
                        current_connections++;
                        std::thread(&KVServer::handleConnection, this, socket).detach();
                    } else {
                        // Connection limit reached
                        std::error_code ec;
                        socket->close(ec);
                    }
                }
                
                if (running) {
                    startAccept();
                }
            });
    }
    
    void handleConnection(std::shared_ptr<tcp::socket> socket) {
        try {
            asio::streambuf buffer;
            asio::error_code error;
            
            while (running) {
                // Read command
                size_t n = asio::read_until(*socket, buffer, '\n', error);
                
                if (error == asio::error::eof || error == asio::error::connection_reset) {
                    break; // Connection closed
                } else if (error) {
                    throw asio::system_error(error);
                }
                
                std::string command(
                    asio::buffers_begin(buffer.data()),
                    asio::buffers_begin(buffer.data()) + n - 1); // Remove newline
                
                buffer.consume(n);
                
                // Process command
                std::string response = processCommand(command);
                response += "\n";
                
                // Send response
                asio::write(*socket, asio::buffer(response), error);
                
                if (error) {
                    break;
                }
            }
        } catch (const std::exception& e) {
            // Log error
        }
        
        current_connections--;
        std::error_code ec;
        socket->close(ec);
    }
    
    std::string processCommand(const std::string& command) {
        std::istringstream iss(command);
        std::string op_str, key;
        std::string value;
        
        if (!(iss >> op_str >> std::ws)) {
            return "ERROR Invalid command format";
        }
        
        // Read key (may contain spaces if quoted)
        char first_char = iss.peek();
        if (first_char == '"' || first_char == '\'') {
            char quote = first_char;
            iss.get(); // Skip quote
            std::getline(iss, key, quote);
        } else {
            iss >> key;
        }
        
        // Read value (rest of the line, may be empty)
        std::getline(iss >> std::ws, value);
        
        // Trim quotes from value if present
        if (!value.empty() && (value[0] == '"' || value[0] == '\'')) {
            char quote = value[0];
            if (value.back() == quote) {
                value = value.substr(1, value.size() - 2);
            }
        }
        
        // Validate sizes
        if (key.size() > config.max_key_size) {
            return "ERROR Key too large";
        }
        
        if (value.size() > config.max_value_size) {
            return "ERROR Value too large";
        }
        
        // Process operation
        if (op_str == "PUT") {
            if (wal.writeEntry(Operation::PUT, key, value)) {
                store.insert(key, value);
                return "OK";
            }
            return "ERROR WAL write failed";
        }
        else if (op_str == "GET") {
            std::string result;
            if (store.find(key, result)) {
                return result;
            }
            return "NOT_FOUND";
        }
        else if (op_str == "DELETE") {
            if (wal.writeEntry(Operation::DELETE, key)) {
                if (store.erase(key)) {
                    return "OK";
                }
                return "NOT_FOUND";
            }
            return "ERROR WAL write failed";
        }
        else if (op_str == "EXISTS") {
            return store.exists(key) ? "true" : "false";
        }
        else if (op_str == "SIZE") {
            return std::to_string(store.size());
        }
        else if (op_str == "PING") {
            return "PONG";
        }
        else if (op_str == "FLUSH") {
            store.clear();
            wal.clear();
            return "OK";
        }
        else if (op_str == "STATS") {
            auto stats = store.getStatistics();
            std::ostringstream oss;
            oss << "items: " << stats.item_count << "\n"
                << "buckets: " << stats.bucket_count << "\n"
                << "load_factor: " << stats.load_factor << "\n"
                << "utilization: " << stats.utilization;
            return oss.str();
        }
        else {
            return "ERROR Unknown command";
        }
    }
    
public:
    KVServer(const Config& config)
        : acceptor(io_context, tcp::endpoint(tcp::v4(), config.server_port)),
          store(config.num_segments),
          wal(config.wal_file, config.sync_wal, config.wal_buffer_size),
          config(config) {
        
        // Recover from WAL
        recoverFromWAL();
    }
    
    ~KVServer() {
        stop();
    }
    
    void start(size_t num_workers = 4) {
        if (running) return;
        
        running = true;
        
        // Start IO context in separate thread
        io_thread = std::make_unique<std::thread>([this]() {
            io_context.run();
        });
        
        // Start worker threads
        for (size_t i = 0; i < num_workers; ++i) {
            worker_threads.emplace_back([this]() {
                io_context.run();
            });
        }
        
        startAccept();
        
        std::cout << "KV Server started on port " << config.server_port << std::endl;
        std::cout << "Segments: " << config.num_segments << std::endl;
        std::cout << "WAL: " << config.wal_file << std::endl;
    }
    
    void stop() {
        if (!running) return;
        
        running = false;
        io_context.stop();
        
        if (io_thread && io_thread->joinable()) {
            io_thread->join();
        }
        
        for (auto& thread : worker_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        worker_threads.clear();
        
        std::cout << "KV Server stopped" << std::endl;
    }
    
    void recoverFromWAL() {
        std::cout << "Recovering from WAL..." << std::endl;
        
        auto insert_func = [this](const std::string& key, const std::string& value) {
            store.insert(key, value);
        };
        
        auto delete_func = [this](const std::string& key) {
            store.erase(key);
        };
        
        wal.replay(insert_func, delete_func);
        
        std::cout << "Recovery complete. " << store.size() << " items loaded." << std::endl;
    }
    
    size_t getConnectionCount() const {
        return current_connections.load();
    }
    
    size_t getItemCount() const {
        return store.size();
    }
    
    Config getConfig() const {
        return config;
    }
};

} // namespace kvstore

#endif // KV_STORE_SERVER_HPP
