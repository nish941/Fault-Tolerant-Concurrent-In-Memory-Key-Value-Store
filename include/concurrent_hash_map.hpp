#ifndef KV_STORE_CONCURRENT_HASH_MAP_HPP
#define KV_STORE_CONCURRENT_HASH_MAP_HPP

#include <vector>
#include <list>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <functional>
#include "types.hpp"

namespace kvstore {

template<typename Key, typename Value, typename Hash = StringHasher>
class ConcurrentHashMap {
private:
    struct Bucket {
        std::list<std::pair<Key, Value>> items;
        mutable std::shared_mutex mutex;
        
        typename std::list<std::pair<Key, Value>>::iterator find(const Key& key) {
            return std::find_if(items.begin(), items.end(),
                [&key](const auto& item) { return item.first == key; });
        }
        
        typename std::list<std::pair<Key, Value>>::const_iterator find(const Key& key) const {
            return std::find_if(items.begin(), items.end(),
                [&key](const auto& item) { return item.first == key; });
        }
    };
    
    std::vector<std::unique_ptr<Bucket>> buckets;
    Hash hasher;
    std::atomic<size_t> item_count{0};
    
    Bucket& getBucket(const Key& key) {
        size_t index = hasher(key) % buckets.size();
        return *buckets[index];
    }
    
    const Bucket& getBucket(const Key& key) const {
        size_t index = hasher(key) % buckets.size();
        return *buckets[index];
    }
    
public:
    ConcurrentHashMap(size_t num_buckets = 64) {
        buckets.reserve(num_buckets);
        for (size_t i = 0; i < num_buckets; ++i) {
            buckets.push_back(std::make_unique<Bucket>());
        }
    }
    
    ~ConcurrentHashMap() = default;
    
    // Disable copy constructor and assignment
    ConcurrentHashMap(const ConcurrentHashMap&) = delete;
    ConcurrentHashMap& operator=(const ConcurrentHashMap&) = delete;
    
    // Enable move semantics
    ConcurrentHashMap(ConcurrentHashMap&&) = default;
    ConcurrentHashMap& operator=(ConcurrentHashMap&&) = default;
    
    bool insert(const Key& key, Value value) {
        auto& bucket = getBucket(key);
        std::unique_lock lock(bucket.mutex);
        
        auto it = bucket.find(key);
        if (it != bucket.items.end()) {
            it->second = std::move(value);
            return false; // Key already existed, updated
        }
        
        bucket.items.emplace_back(key, std::move(value));
        item_count.fetch_add(1, std::memory_order_relaxed);
        return true; // New key inserted
    }
    
    bool erase(const Key& key) {
        auto& bucket = getBucket(key);
        std::unique_lock lock(bucket.mutex);
        
        auto it = bucket.find(key);
        if (it == bucket.items.end()) {
            return false;
        }
        
        bucket.items.erase(it);
        item_count.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    
    bool find(const Key& key, Value& value) const {
        const auto& bucket = getBucket(key);
        std::shared_lock lock(bucket.mutex);
        
        auto it = bucket.find(key);
        if (it == bucket.items.end()) {
            return false;
        }
        
        value = it->second;
        return true;
    }
    
    bool exists(const Key& key) const {
        const auto& bucket = getBucket(key);
        std::shared_lock lock(bucket.mutex);
        return bucket.find(key) != bucket.items.end();
    }
    
    size_t size() const {
        return item_count.load(std::memory_order_relaxed);
    }
    
    bool empty() const {
        return size() == 0;
    }
    
    // Thread-safe iteration with a visitor pattern
    template<typename Visitor>
    void for_each(Visitor visitor) const {
        for (const auto& bucket : buckets) {
            std::shared_lock lock(bucket->mutex);
            for (const auto& item : bucket->items) {
                visitor(item.first, item.second);
            }
        }
    }
    
    // Clear all items
    void clear() {
        for (auto& bucket : buckets) {
            std::unique_lock lock(bucket->mutex);
            bucket->items.clear();
        }
        item_count.store(0, std::memory_order_relaxed);
    }
    
    // Get statistics
    struct Statistics {
        size_t item_count;
        size_t bucket_count;
        std::vector<size_t> bucket_sizes;
        double load_factor;
        double utilization;
    };
    
    Statistics getStatistics() const {
        Statistics stats;
        stats.item_count = size();
        stats.bucket_count = buckets.size();
        stats.bucket_sizes.reserve(buckets.size());
        
        size_t used_buckets = 0;
        for (const auto& bucket : buckets) {
            std::shared_lock lock(bucket->mutex);
            size_t size = bucket->items.size();
            stats.bucket_sizes.push_back(size);
            if (size > 0) {
                used_buckets++;
            }
        }
        
        stats.load_factor = static_cast<double>(stats.item_count) / stats.bucket_count;
        stats.utilization = static_cast<double>(used_buckets) / stats.bucket_count;
        
        return stats;
    }
};

} // namespace kvstore

#endif // KV_STORE_CONCURRENT_HASH_MAP_HPP
