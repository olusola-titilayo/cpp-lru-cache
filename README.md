# cpp-lru-cache

![CI](https://github.com/olusola-titilayo/cpp-lru-cache/actions/workflows/ci.yml/badge.svg)

A header-only **LRU cache for C++17** with optional time-to-live, hit/miss statistics and an
injectable clock. `get`, `put` and `erase` are **O(1)** on average.

```cpp
#include "lru/lru_cache.hpp"
using namespace std::chrono_literals;

lru::Cache<std::string, User> users(1000, 30s);   // 1000 entries, 30-second TTL

users.put("ada", load_user("ada"));
if (auto u = users.get("ada")) use(*u);           // std::optional<User>

users.stats().hit_rate();
```

## How it works

```
unordered_map<Key, list::iterator>        std::list<Entry>  (most recent → least recent)
  "ada" ───────────────────────────────▶  [ada] ⇄ [bob] ⇄ [cy]
  "bob" ───────────────────────────────────────▲              ▲
  "cy"  ───────────────────────────────────────────────────────┘
```

- **Hit:** look up the iterator in the map and `splice` the node to the front. That's O(1) and
  never copies the value.
- **Insert when full:** reclaim expired entries first, and only if none are expired, evict the
  list's tail (the least recently used).
- **TTL:** an entry is treated as absent once `now - stored_at >= ttl`. `put` on an existing key
  refreshes both its value and its timestamp.

## API

| Method | Description |
|---|---|
| `Cache(capacity, ttl = 0, now = Clock::now)` | `ttl == 0` disables expiry. Throws if `capacity == 0` |
| `std::optional<V> get(const K&)` | Marks the entry most recently used |
| `bool put(const K&, V)` | Returns `true` if it replaced an existing entry |
| `bool erase(const K&)` / `void clear()` | |
| `bool contains(const K&) const` | Doesn't change recency or stats |
| `size_t purge_expired()` | Removes stale entries now |
| `const Stats& stats() const` | `hits`, `misses`, `evictions`, `expirations`, `hit_rate()` |
| `for_each(f)` | Visits entries from most to least recently used |

## Build and test

```bash
cmake -S . -B build -DLRU_SANITIZE=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/memoize      # fib(90) with a 64-entry cache
```

CI runs the suite under **GCC and Clang** with `-Wall -Wextra -Wpedantic -Werror` and
AddressSanitizer + UndefinedBehaviorSanitizer.

**Thread safety:** a `Cache` is not synchronised. Even `get` mutates recency order, so guard
shared instances with a mutex.
