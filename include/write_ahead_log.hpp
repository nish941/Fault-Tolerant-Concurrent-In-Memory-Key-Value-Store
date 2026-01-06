#ifndef KV_STORE_WRITE_AHEAD_LOG_HPP
#define KV_STORE_WRITE_AHEAD_LOG_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <atomic>
#include "types.hpp"

namespace kvstore {

class WriteAheadLog {
private:
    std::string filename;
    std::fstream log_file;
    std::mutex file_mutex;
    std::atomic<uint64_t> sequence_number{0};
    bool sync_mode;
    size_t buffer_size;
    std::vector<char> write_buffer;
    
    void ensureOpen() {
        if (!log_file.is_open()) {
            log_file.open(filename, std::ios::in | std::ios::out | std::ios::app);
            if (!log_file) {
                log_file.open(filename, std::ios::out);
                log_file.close();
                log_file.open(filename, std::ios::in | std::ios::out | std::ios::app);
            }
        }
    }
    
    void syncToDisk() {
        if (sync_mode && log_file.is_open()) {
            log_file.flush();
            // On systems that support it, we could use:
            // std::ofstream::sync() or platform-specific sync
        }
    }
    
public:
    WriteAheadLog(const std::string& filename, bool sync = true, size_t buffer_size = 8192)
        : filename(filename), sync_mode(sync), buffer_size(buffer_size) {
        write_buffer.reserve(buffer_size);
        ensureOpen();
        recoverSequenceNumber();
    }
    
    ~WriteAheadLog() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }
    
    // Write an entry to the WAL
    bool writeEntry(Operation op, const std::string& key, 
                   const std::string& value = "") {
        std::lock_guard lock(file_mutex);
        ensureOpen();
        
        uint64_t seq = sequence_number.fetch_add(1, std::memory_order_relaxed);
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Format: seq|timestamp|op|key_size|key|value_size|value
        size_t key_size = key.size();
        size_t value_size = value.size();
        
        // Write to buffer first
        write_buffer.clear();
        
        // Write sequence number
        auto* ptr = reinterpret_cast<const char*>(&seq);
        write_buffer.insert(write_buffer.end(), ptr, ptr + sizeof(seq));
        
        // Write timestamp
        ptr = reinterpret_cast<const char*>(&timestamp);
        write_buffer.insert(write_buffer.end(), ptr, ptr + sizeof(timestamp));
        
        // Write operation
        auto op_val = static_cast<uint8_t>(op);
        write_buffer.push_back(static_cast<char>(op_val));
        
        // Write key size and key
        ptr = reinterpret_cast<const char*>(&key_size);
        write_buffer.insert(write_buffer.end(), ptr, ptr + sizeof(key_size));
        write_buffer.insert(write_buffer.end(), key.begin(), key.end());
        
        // Write value size and value
        ptr = reinterpret_cast<const char*>(&value_size);
        write_buffer.insert(write_buffer.end(), ptr, ptr + sizeof(value_size));
        write_buffer.insert(write_buffer.end(), value.begin(), value.end());
        
        // Write to file
        log_file.write(write_buffer.data(), write_buffer.size());
        
        if (!log_file) {
            return false;
        }
        
        syncToDisk();
        return true;
    }
    
    // Replay the WAL to rebuild state
    template<typename InsertFunc, typename DeleteFunc>
    void replay(InsertFunc insert_func, DeleteFunc delete_func) {
        std::lock_guard lock(file_mutex);
        ensureOpen();
        
        log_file.seekg(0, std::ios::beg);
        
        while (log_file.peek() != EOF) {
            uint64_t seq;
            uint64_t timestamp;
            uint8_t op_val;
            size_t key_size, value_size;
            
            // Read sequence number
            log_file.read(reinterpret_cast<char*>(&seq), sizeof(seq));
            if (log_file.eof()) break;
            
            // Read timestamp
            log_file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            
            // Read operation
            log_file.read(reinterpret_cast<char*>(&op_val), sizeof(op_val));
            Operation op = static_cast<Operation>(op_val);
            
            // Read key
            log_file.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
            std::string key(key_size, '\0');
            log_file.read(&key[0], key_size);
            
            // Read value
            log_file.read(reinterpret_cast<char*>(&value_size), sizeof(value_size));
            std::string value(value_size, '\0');
            if (value_size > 0) {
                log_file.read(&value[0], value_size);
            }
            
            // Apply operation
            switch (op) {
                case Operation::PUT:
                    insert_func(key, value);
                    break;
                case Operation::DELETE:
                    delete_func(key);
                    break;
                default:
                    // Skip other operations during replay
                    break;
            }
            
            // Update sequence number
            if (seq >= sequence_number.load()) {
                sequence_number.store(seq + 1);
            }
        }
    }
    
    // Clear the WAL (start fresh)
    bool clear() {
        std::lock_guard lock(file_mutex);
        
        if (log_file.is_open()) {
            log_file.close();
        }
        
        // Delete the file
        if (std::remove(filename.c_str()) != 0) {
            return false;
        }
        
        // Reopen empty file
        sequence_number.store(0);
        ensureOpen();
        return true;
    }
    
    // Get current size of WAL
    size_t size() {
        std::lock_guard lock(file_mutex);
        ensureOpen();
        
        log_file.seekg(0, std::ios::end);
        return static_cast<size_t>(log_file.tellg());
    }
    
    // Check if WAL is empty
    bool empty() {
        return size() == 0;
    }
    
private:
    void recoverSequenceNumber() {
        std::lock_guard lock(file_mutex);
        ensureOpen();
        
        log_file.seekg(0, std::ios::end);
        auto file_size = log_file.tellg();
        
        if (file_size == 0) {
            sequence_number.store(0);
            return;
        }
        
        // Read the last entry to get sequence number
        const size_t max_entry_size = sizeof(uint64_t) * 2 + 1 + 
                                     sizeof(size_t) * 2 + 1024 + 65536; // Max sizes
        
        size_t read_size = std::min(static_cast<size_t>(file_size), max_entry_size);
        log_file.seekg(-static_cast<int64_t>(read_size), std::ios::end);
        
        std::vector<char> buffer(read_size);
        log_file.read(buffer.data(), read_size);
        
        // Try to find the last complete entry
        // This is simplified - in production, you'd want more robust recovery
        uint64_t last_seq = 0;
        try {
            // Scan backwards for a valid sequence number
            // In a real implementation, you'd parse the WAL properly
            for (int64_t i = buffer.size() - 8; i >= 0; --i) {
                uint64_t potential_seq;
                std::memcpy(&potential_seq, &buffer[i], sizeof(potential_seq));
                
                // Basic validation (sequence should increase over time)
                if (potential_seq >= last_seq) {
                    last_seq = potential_seq;
                }
            }
        } catch (...) {
            // If recovery fails, start from 0
            last_seq = 0;
        }
        
        sequence_number.store(last_seq + 1);
    }
};

} // namespace kvstore

#endif // KV_STORE_WRITE_AHEAD_LOG_HPP
