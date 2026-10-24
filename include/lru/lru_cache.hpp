// Header-only LRU cache with optional time-to-live.
//
// get / put / erase are O(1) on average: a doubly linked list keeps recency
// order and an unordered_map points from each key into the list.
#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace lru {

struct Stats {
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t evictions = 0;   // removed to make room
    std::size_t expirations = 0; // removed because the TTL elapsed

    double hit_rate() const {
        const auto total = hits + misses;
        return total == 0 ? 0.0 : static_cast<double>(hits) / static_cast<double>(total);
    }
};

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename Clock = std::chrono::steady_clock>
class Cache {
public:
    using Duration = typename Clock::duration;
    using TimePoint = typename Clock::time_point;
    using NowFn = std::function<TimePoint()>;

    // capacity: maximum number of entries (must be >= 1).
    // ttl: entries older than this are treated as absent; zero disables expiry.
    // now: time source, injectable for tests.
    explicit Cache(std::size_t capacity, Duration ttl = Duration::zero(),
                   NowFn now = [] { return Clock::now(); })
        : capacity_(capacity), ttl_(ttl), now_(std::move(now)) {
        if (capacity_ == 0) throw std::invalid_argument("lru::Cache capacity must be >= 1");
        if (ttl_ < Duration::zero()) throw std::invalid_argument("lru::Cache ttl must be >= 0");
        index_.reserve(capacity_);
    }

    // Returns the value and marks it most recently used, or nullopt if absent/expired.
    std::optional<Value> get(const Key& key) {
        auto it = index_.find(key);
        if (it == index_.end()) {
            ++stats_.misses;
            return std::nullopt;
        }
        if (expired(*it->second)) {
            remove(it);
            ++stats_.expirations;
            ++stats_.misses;
            return std::nullopt;
        }
        items_.splice(items_.begin(), items_, it->second);
        ++stats_.hits;
        return it->second->value;
    }

    // Inserts or replaces a value. Returns true if an existing entry was replaced.
    bool put(const Key& key, Value value) {
        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->value = std::move(value);
            it->second->stored_at = now_();
            items_.splice(items_.begin(), items_, it->second);
            return true;
        }
        if (items_.size() == capacity_) evict_one();
        items_.push_front(Entry{key, std::move(value), now_()});
        index_.emplace(key, items_.begin());
        return false;
    }

    bool erase(const Key& key) {
        auto it = index_.find(key);
        if (it == index_.end()) return false;
        remove(it);
        return true;
    }

    // True if present and not expired. Does not change recency or stats.
    bool contains(const Key& key) const {
        auto it = index_.find(key);
        return it != index_.end() && !expired(*it->second);
    }

    // Drops every expired entry; returns how many were removed.
    std::size_t purge_expired() {
        std::size_t removed = 0;
        for (auto it = items_.begin(); it != items_.end();) {
            if (expired(*it)) {
                index_.erase(it->key);
                it = items_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        stats_.expirations += removed;
        return removed;
    }

    void clear() {
        items_.clear();
        index_.clear();
    }

    std::size_t size() const { return items_.size(); }
    std::size_t capacity() const { return capacity_; }
    const Stats& stats() const { return stats_; }

    // Visits entries from most to least recently used.
    template <typename F>
    void for_each(F&& f) const {
        for (const auto& e : items_) f(e.key, e.value);
    }

private:
    struct Entry {
        Key key;
        Value value;
        TimePoint stored_at;
    };
    using List = std::list<Entry>;

    bool expired(const Entry& e) const {
        return ttl_ != Duration::zero() && now_() - e.stored_at >= ttl_;
    }

    void remove(typename std::unordered_map<Key, typename List::iterator, Hash>::iterator it) {
        items_.erase(it->second);
        index_.erase(it);
    }

    void evict_one() {
        // Prefer reclaiming an expired entry over evicting a live one.
        if (ttl_ != Duration::zero() && purge_expired() > 0) return;
        index_.erase(items_.back().key);
        items_.pop_back();
        ++stats_.evictions;
    }

    std::size_t capacity_;
    Duration ttl_;
    NowFn now_;
    List items_;
    std::unordered_map<Key, typename List::iterator, Hash> index_;
    Stats stats_;
};

}  // namespace lru
