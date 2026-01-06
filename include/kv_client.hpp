#ifndef KV_STORE_CLIENT_HPP
#define KV_STORE_CLIENT_HPP

#include <string>
#include <asio.hpp>
#include <iostream>
#include <sstream>

namespace kvstore {

using asio::ip::tcp;

class KVClient {
private:
    asio::io_context io_context;
    tcp::socket socket;
    std::string host;
    uint16_t port;
    
public:
    KVClient(const std::string& host = "127.0.0.1", uint16_t port = 6379)
        : socket(io_context), host(host), port(port) {
        connect();
    }
    
    ~KVClient() {
        disconnect();
    }
    
    void connect() {
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        asio::connect(socket, endpoints);
    }
    
    void disconnect() {
        if (socket.is_open()) {
            std::error_code ec;
            socket.shutdown(tcp::socket::shutdown_both, ec);
            socket.close(ec);
        }
    }
    
    std::string sendCommand(const std::string& command) {
        if (!socket.is_open()) {
            connect();
        }
        
        std::string full_command = command + "\n";
        
        asio::write(socket, asio::buffer(full_command));
        
        asio::streambuf response;
        asio::read_until(socket, response, '\n');
        
        std::string response_str(
            asio::buffers_begin(response.data()),
            asio::buffers_begin(response.data()) + response.size() - 1); // Remove newline
        
        response.consume(response.size());
        return response_str;
    }
    
    bool put(const std::string& key, const std::string& value) {
        std::ostringstream oss;
        oss << "PUT \"" << key << "\" \"" << value << "\"";
        std::string response = sendCommand(oss.str());
        return response == "OK";
    }
    
    std::string get(const std::string& key) {
        std::ostringstream oss;
        oss << "GET \"" << key << "\"";
        return sendCommand(oss.str());
    }
    
    bool del(const std::string& key) {
        std::ostringstream oss;
        oss << "DELETE \"" << key << "\"";
        std::string response = sendCommand(oss.str());
        return response == "OK";
    }
    
    bool exists(const std::string& key) {
        std::ostringstream oss;
        oss << "EXISTS \"" << key << "\"";
        std::string response = sendCommand(oss.str());
        return response == "true";
    }
    
    size_t size() {
        std::string response = sendCommand("SIZE");
        try {
            return std::stoul(response);
        } catch (...) {
            return 0;
        }
    }
    
    bool ping() {
        return sendCommand("PING") == "PONG";
    }
    
    bool flush() {
        return sendCommand("FLUSH") == "OK";
    }
    
    std::string stats() {
        return sendCommand("STATS");
    }
    
    // Batch operations
    bool putBatch(const std::vector<std::pair<std::string, std::string>>& items) {
        bool all_success = true;
        for (const auto& item : items) {
            if (!put(item.first, item.second)) {
                all_success = false;
            }
        }
        return all_success;
    }
};

} // namespace kvstore

#endif // KV_STORE_CLIENT_HPP
