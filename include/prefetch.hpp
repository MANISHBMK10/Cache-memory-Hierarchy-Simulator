#pragma once
#include <cstdint>
#include <deque>
#include <unordered_set>
#include <cstddef>

struct PrefetchStats {
    uint64_t issued = 0;
    uint64_t hits = 0;     // demand access found in prefetch buffer
    uint64_t drops = 0;    // buffer full -> dropped
};

class PrefetchBuffer {
public:
    explicit PrefetchBuffer(std::size_t capacity = 0);

    void reset();
    bool enabled() const { return cap_ > 0; }

    // Store a prefetched *block address* (already shifted by block offset bits)
    void push(uint64_t block_addr);

    // Check and consume if present
    bool consume_if_present(uint64_t block_addr);

    const PrefetchStats& stats() const { return stats_; }

private:
    std::size_t cap_;
    std::deque<uint64_t> fifo_;
    std::unordered_set<uint64_t> set_;
    PrefetchStats stats_;
};
