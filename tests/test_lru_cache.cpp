#include "lru/lru_cache.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << "\n";                                                   \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

struct FakeClock {
    Clock::time_point t{};
    Clock::time_point operator()() const { return t; }
};

template <typename C>
std::vector<int> keys_in_order(const C& c) {
    std::vector<int> out;
    c.for_each([&](int k, const auto&) { out.push_back(k); });
    return out;
}

void evicts_least_recently_used() {
    lru::Cache<int, std::string> c(2);
    c.put(1, "one");
    c.put(2, "two");
    CHECK(c.get(1) == std::optional<std::string>("one"));  // 1 is now most recent
    c.put(3, "three");                                    // evicts 2
    CHECK(!c.contains(2));
    CHECK(c.contains(1) && c.contains(3));
    CHECK(c.stats().evictions == 1);
    CHECK((keys_in_order(c) == std::vector<int>{3, 1}));
}

void put_existing_key_replaces_and_refreshes() {
    lru::Cache<int, int> c(2);
    c.put(1, 10);
    c.put(2, 20);
    CHECK(c.put(1, 11));  // replace, 1 becomes most recent
    c.put(3, 30);         // evicts 2, not 1
    CHECK(c.get(1) == 11);
    CHECK(!c.contains(2));
    CHECK(c.size() == 2);
}

void stats_track_hits_and_misses() {
    lru::Cache<int, int> c(4);
    c.put(1, 1);
    c.get(1);
    c.get(1);
    c.get(9);
    CHECK(c.stats().hits == 2);
    CHECK(c.stats().misses == 1);
    CHECK(c.stats().hit_rate() > 0.66 && c.stats().hit_rate() < 0.67);
}

void erase_and_clear() {
    lru::Cache<int, int> c(3);
    c.put(1, 1);
    c.put(2, 2);
    CHECK(c.erase(1));
    CHECK(!c.erase(1));
    CHECK(c.size() == 1);
    c.clear();
    CHECK(c.size() == 0 && !c.contains(2));
}

void ttl_expires_entries() {
    FakeClock clock;
    lru::Cache<int, int> c(4, 10s, std::ref(clock));
    c.put(1, 1);
    clock.t += 9s;
    CHECK(c.get(1) == 1);
    clock.t += 1s;  // exactly ttl old
    CHECK(!c.get(1).has_value());
    CHECK(c.stats().expirations == 1);
    CHECK(c.size() == 0);
}

void put_refreshes_ttl() {
    FakeClock clock;
    lru::Cache<int, int> c(4, 10s, std::ref(clock));
    c.put(1, 1);
    clock.t += 8s;
    c.put(1, 2);
    clock.t += 8s;
    CHECK(c.get(1) == 2);
}

void full_cache_reclaims_expired_before_evicting_live() {
    FakeClock clock;
    lru::Cache<int, int> c(2, 5s, std::ref(clock));
    c.put(1, 1);
    clock.t += 3s;
    c.put(2, 2);
    c.get(1);      // 1 is most recent but will expire first
    clock.t += 3s; // 1 is 6s old (expired), 2 is 3s old
    c.put(3, 3);
    CHECK(c.contains(2) && c.contains(3) && !c.contains(1));
    CHECK(c.stats().evictions == 0);
    CHECK(c.stats().expirations == 1);
}

void purge_expired_removes_only_stale() {
    FakeClock clock;
    lru::Cache<int, int> c(5, 5s, std::ref(clock));
    c.put(1, 1);
    c.put(2, 2);
    clock.t += 4s;
    c.put(3, 3);
    clock.t += 1s;
    CHECK(c.purge_expired() == 2);
    CHECK((keys_in_order(c) == std::vector<int>{3}));
}

void move_only_values() {
    lru::Cache<int, std::shared_ptr<int>> c(1);
    c.put(1, std::make_shared<int>(7));
    auto v = c.get(1);
    CHECK(v && **v == 7);
    c.put(2, std::make_shared<int>(8));
    CHECK(!c.contains(1));
}

void rejects_bad_config() {
    bool threw = false;
    try {
        lru::Cache<int, int> c(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

void capacity_one_stress() {
    lru::Cache<int, int> c(1);
    for (int i = 0; i < 10000; ++i) c.put(i, i);
    CHECK(c.size() == 1 && c.get(9999) == 9999);
    CHECK(c.stats().evictions == 9999);
}

}  // namespace

int main() {
    evicts_least_recently_used();
    put_existing_key_replaces_and_refreshes();
    stats_track_hits_and_misses();
    erase_and_clear();
    ttl_expires_entries();
    put_refreshes_ttl();
    full_cache_reclaims_expired_before_evicting_live();
    purge_expired_removes_only_stale();
    move_only_values();
    rejects_bad_config();
    capacity_one_stress();
    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all tests passed\n";
    return EXIT_SUCCESS;
}
