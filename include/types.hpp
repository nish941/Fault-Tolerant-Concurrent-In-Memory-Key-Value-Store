#ifndef KV_STORE_TYPES_HPP
#define KV_STORE_TYPES_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace kvstore {

// Operation types
enum class Operation {
    PUT,
    GET,
    DELETE,
    EXISTS,
    SIZE
};

// Operation result structure
struct OperationResult {
    bool success;
    std::string value;
    std::string error_message;
    uint64_t timestamp;
};

// Key-Value pair structure
struct KeyValue {
    std::string key;
    std::string value;
    uint64_t timestamp;
    bool deleted;
};

// WAL Entry structure
struct WALEntry {
    Operation op;
    std::string key;
    std::string value;
    uint64_t timestamp;
    uint64_t sequence_number;
};

// Hash function for string keys
struct StringHasher {
    size_t operator()(const std::string& key) const {
        // Using FNV-1a hash for good distribution
        const uint64_t prime = 1099511628211ULL;
        uint64_t hash = 14695981039346656037ULL;
        
        for (char c : key) {
            hash ^= static_cast<uint8_t>(c);
            hash *= prime;
        }
        
        return static_cast<size_t>(hash);
    }
};

// Configuration structure
struct Config {
    size_t num_segments = 64;           // Number of hash map segments
    size_t initial_bucket_size = 16;    // Initial buckets per segment
    std::string wal_file = "kv_store.wal";
    size_t wal_buffer_size = 8192;      // 8KB buffer for WAL writes
    bool sync_wal = true;               // Synchronous WAL writes
    uint16_t server_port = 6379;        // Default port
    size_t max_key_size = 1024;         // 1KB max key size
    size_t max_value_size = 65536;      // 64KB max value size
    size_t max_connections = 1000;      // Max concurrent connections
};

} // namespace kvstore

#endif // KV_STORE_TYPES_HPP
