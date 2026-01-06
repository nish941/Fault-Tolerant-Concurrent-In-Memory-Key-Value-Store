#include <gtest/gtest.h>
#include <filesystem>
#include "write_ahead_log.hpp"

namespace fs = std::filesystem;

class WriteAheadLogTest : public ::testing::Test {
protected:
    std::string test_wal_file = "test_wal.log";
    
    void SetUp() override {
        // Clean up any existing test file
        if (fs::exists(test_wal_file)) {
            fs::remove(test_wal_file);
        }
    }
    
    void TearDown() override {
        if (fs::exists(test_wal_file)) {
            fs::remove(test_wal_file);
        }
    }
};

TEST_F(WriteAheadLogTest, WriteAndReplay) {
    kvstore::WriteAheadLog wal(test_wal_file);
    
    // Write some entries
    EXPECT_TRUE(wal.writeEntry(kvstore::Operation::PUT, "key1", "value1"));
    EXPECT_TRUE(wal.writeEntry(kvstore::Operation::PUT, "key2", "value2"));
    EXPECT_TRUE(wal.writeEntry(kvstore::Operation::DELETE, "key1", ""));
    
    // Simulate in-memory store
    std::map<std::string, std::string> store;
    std::set<std::string> deleted_keys;
    
    auto insert_func = [&](const std::string& key, const std::string& value) {
        store[key] = value;
        deleted_keys.erase(key);
    };
    
    auto delete_func = [&](const std::string& key) {
        store.erase(key);
        deleted_keys.insert(key);
    };
    
    // Create new WAL and replay
    kvstore::WriteAheadLog wal2(test_wal_file);
    wal2.replay(insert_func, delete_func);
    
    // Verify state
    EXPECT_EQ(store.size(), 1);
    EXPECT_TRUE(store.find("key2") != store.end());
    EXPECT_EQ(store["key2"], "value2");
    EXPECT_TRUE(deleted_keys.find("key1") != deleted_keys.end());
}

TEST_F(WriteAheadLogTest, CrashRecovery) {
    {
        // Simulate normal operation with crash
        kvstore::WriteAheadLog wal(test_wal_file);
        
        wal.writeEntry(kvstore::Operation::PUT, "important1", "data1");
        wal.writeEntry(kvstore::Operation::PUT, "important2", "data2");
        wal.writeEntry(kvstore::Operation::PUT, "important3", "data3");
        
        // Simulate crash here (wal goes out of scope)
    }
    
    // Simulate restart and recovery
    std::map<std::string, std::string> recovered_store;
    
    auto insert_func = [&](const std::string& key, const std::string& value) {
        recovered_store[key] = value;
    };
    
    auto delete_func = [&](const std::string& key) {
        recovered_store.erase(key);
    };
    
    kvstore::WriteAheadLog recovery_wal(test_wal_file);
    recovery_wal.replay(insert_func, delete_func);
    
    EXPECT_EQ(recovered_store.size(), 3);
    EXPECT_EQ(recovered_store["important1"], "data1");
    EXPECT_EQ(recovered_store["important2"], "data2");
    EXPECT_EQ(recovered_store["important3"], "data3");
}

TEST_F(WriteAheadLogTest, ClearWAL) {
    kvstore::WriteAheadLog wal(test_wal_file);
    
    // Write some entries
    wal.writeEntry(kvstore::Operation::PUT, "key1", "value1");
    EXPECT_FALSE(wal.empty());
    
    // Clear WAL
    EXPECT_TRUE(wal.clear());
    EXPECT_TRUE(wal.empty());
    
    // Verify new entries work after clear
    EXPECT_TRUE(wal.writeEntry(kvstore::Operation::PUT, "key2", "value2"));
    EXPECT_FALSE(wal.empty());
}

TEST_F(WriteAheadLogTest, SequenceNumberRecovery) {
    uint64_t last_seq;
    
    {
        kvstore::WriteAheadLog wal(test_wal_file);
        
        // Write multiple entries
        for (int i = 0; i < 100; ++i) {
            wal.writeEntry(kvstore::Operation::PUT, 
                          "key" + std::to_string(i), 
                          "value" + std::to_string(i));
        }
        
        // Note: In real implementation, we'd get the sequence number
        // For now, just ensure we can write more after recovery
    }
    
    {
        // Simulate restart
        kvstore::WriteAheadLog wal2(test_wal_file);
        
        // Should be able to write more entries
        EXPECT_TRUE(wal2.writeEntry(kvstore::Operation::PUT, "new_key", "new_value"));
    }
}

TEST_F(WriteAheadLogTest, LargeValueHandling) {
    kvstore::WriteAheadLog wal(test_wal_file);
    
    // Create large value
    std::string large_value(65536, 'X'); // 64KB
    
    EXPECT_TRUE(wal.writeEntry(kvstore::Operation::PUT, "large_key", large_value));
    
    // Replay and verify
    std::map<std::string, std::string> store;
    
    auto insert_func = [&](const std::string& key, const std::string& value) {
        store[key] = value;
    };
    
    kvstore::WriteAheadLog wal2(test_wal_file);
    wal2.replay(insert_func, [](const std::string&) {});
    
    EXPECT_EQ(store["large_key"].size(), 65536);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
