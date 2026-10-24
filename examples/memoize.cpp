// Memoising an expensive function with a bounded cache.
#include "lru/lru_cache.hpp"

#include <cstdint>
#include <iostream>

static lru::Cache<int, std::uint64_t> cache(64);

std::uint64_t fib(int n) {
    if (n < 2) return static_cast<std::uint64_t>(n);
    if (auto hit = cache.get(n)) return *hit;
    const auto v = fib(n - 1) + fib(n - 2);
    cache.put(n, v);
    return v;
}

int main() {
    std::cout << "fib(90) = " << fib(90) << "\n";
    const auto& s = cache.stats();
    std::cout << "hits=" << s.hits << " misses=" << s.misses << " hit_rate=" << s.hit_rate() << "\n";
}
