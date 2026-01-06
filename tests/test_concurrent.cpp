#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "concurrent_hash_map.hpp"

class ConcurrentHashMapTest : public ::testing::Test {
protected:
    kvstore::ConcurrentHashMap<std::string, std::string> map;
    
    void SetUp() override {
        map.insert("key1", "value1");
        map.insert("key2", "value2");
        map.insert("key3", "value3");
    }
    
    void TearDown() override {
        map.clear();
    }
};

TEST_F(ConcurrentHashMapTest, BasicOperations) {
    std::string value;
    
    EXPECT_TRUE(map.find("key1", value));
    EXPECT_EQ(value, "value1");
    
    EXPECT_TRUE(map.erase("key1"));
    EXPECT_FALSE(map.exists("key1"));
    
    EXPECT_EQ(map.size(), 2);
}

TEST_F(ConcurrentHashMapTest, ConcurrentInsert) {
    const int num_threads = 10;
    const int num_inserts_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_inserts{0};
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < num_inserts_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                if (map.insert(key, "value")) {
                    successful_inserts++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(map.size(), successful_inserts + 3); // +3 from SetUp
}

TEST_F(ConcurrentHashMapTest, ConcurrentReadWrite) {
    std::atomic<bool> running{true};
    std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    
    // Writer thread
    std::thread writer([&]() {
        for (int i = 0; i < 1000; ++i) {
            map.insert("writer_key_" + std::to_string(i), "value");
            writes++;
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        running = false;
    });
    
    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 5; ++i) {
        readers.emplace_back([&, i]() {
            std::string value;
            while (running) {
                for (int j = 0; j < 100; ++j) {
                    map.find("key" + std::to_string((j % 3) + 1), value);
                    reads++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        });
    }
    
    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }
    
    std::cout << "Reads: " << reads << ", Writes: " << writes << std::endl;
    EXPECT_GT(reads, 0);
    EXPECT_GT(writes, 0);
}

TEST_F(ConcurrentHashMapTest, Statistics) {
    auto stats = map.getStatistics();
    
    EXPECT_EQ(stats.item_count, 3);
    EXPECT_GT(stats.bucket_count, 0);
    EXPECT_GT(stats.load_factor, 0);
    
    // Insert more items
    for (int i = 0; i < 1000; ++i) {
        map.insert("stat_key_" + std::to_string(i), "value");
    }
    
    stats = map.getStatistics();
    EXPECT_EQ(stats.item_count, 1003);
}

TEST_F(ConcurrentHashMapTest, ForEachVisitor) {
    std::atomic<int> count{0};
    
    auto visitor = [&count](const std::string& key, const std::string& value) {
        count++;
    };
    
    map.for_each(visitor);
    
    EXPECT_EQ(count, 3);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
