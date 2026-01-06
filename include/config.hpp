#ifndef KV_STORE_CONFIG_HPP
#define KV_STORE_CONFIG_HPP

#include <string>
#include <fstream>
#include <sstream>
#include "types.hpp"

namespace kvstore {

class ConfigManager {
public:
    static Config loadFromFile(const std::string& filename = "kv_config.conf") {
        Config config;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            // Return default config if file doesn't exist
            return config;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            std::istringstream iss(line);
            std::string key, value;
            if (std::getline(iss, key, '=') && std::getline(iss, value)) {
                trim(key);
                trim(value);
                
                if (key == "num_segments") {
                    config.num_segments = std::stoul(value);
                } else if (key == "initial_bucket_size") {
                    config.initial_bucket_size = std::stoul(value);
                } else if (key == "wal_file") {
                    config.wal_file = value;
                } else if (key == "wal_buffer_size") {
                    config.wal_buffer_size = std::stoul(value);
                } else if (key == "sync_wal") {
                    config.sync_wal = (value == "true" || value == "1");
                } else if (key == "server_port") {
                    config.server_port = static_cast<uint16_t>(std::stoi(value));
                } else if (key == "max_key_size") {
                    config.max_key_size = std::stoul(value);
                } else if (key == "max_value_size") {
                    config.max_value_size = std::stoul(value);
                } else if (key == "max_connections") {
                    config.max_connections = std::stoul(value);
                }
            }
        }
        
        return config;
    }
    
    static void saveToFile(const Config& config, 
                          const std::string& filename = "kv_config.conf") {
        std::ofstream file(filename);
        
        file << "# KV Store Configuration\n";
        file << "# Generated automatically\n\n";
        
        file << "num_segments=" << config.num_segments << "\n";
        file << "initial_bucket_size=" << config.initial_bucket_size << "\n";
        file << "wal_file=" << config.wal_file << "\n";
        file << "wal_buffer_size=" << config.wal_buffer_size << "\n";
        file << "sync_wal=" << (config.sync_wal ? "true" : "false") << "\n";
        file << "server_port=" << config.server_port << "\n";
        file << "max_key_size=" << config.max_key_size << "\n";
        file << "max_value_size=" << config.max_value_size << "\n";
        file << "max_connections=" << config.max_connections << "\n";
        
        file.close();
    }
    
private:
    static void trim(std::string& str) {
        str.erase(0, str.find_first_not_of(" \t\n\r\f\v"));
        str.erase(str.find_last_not_of(" \t\n\r\f\v") + 1);
    }
};

} // namespace kvstore

#endif // KV_STORE_CONFIG_HPP
