#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstddef>
#include "prefetch.hpp"

enum class WritePolicy { WriteBack, WriteThrough };
enum class AllocatePolicy { WriteAllocate, NoWriteAllocate };

struct CacheConfig {
    std::string name = "L1";
    std::size_t size_bytes = 32768;
    std::size_t block_bytes = 64;
    std::size_t assoc = 8;
    WritePolicy wp = WritePolicy::WriteBack;
    AllocatePolicy ap = AllocatePolicy::WriteAllocate;
    std::string repl = "lru"; // lru
    std::size_t prefetch_buf_entries = 0; // 0 disables
    bool next_line_prefetch = false;      // simple prefetcher trigger
};

struct CacheStats {
    uint64_t reads = 0, writes = 0;
    uint64_t read_hits = 0, read_misses = 0;
    uint64_t write_hits = 0, write_misses = 0;
    uint64_t evictions = 0;
    uint64_t writebacks = 0; // dirty evictions (WB)
};

struct AccessResult {
    bool hit = false;
    bool eviction = false;
    bool eviction_dirty = false;
    uint64_t evicted_block_addr = 0; // block address (addr >> offset_bits)
};

class Cache {
public:
    explicit Cache(const CacheConfig& cfg);

    void reset();

    // Access in terms of byte address + op.
    // Returns hit/miss and eviction info.
    AccessResult access(char op, uint64_t byte_addr);

    // Insert a block (used when lower level returns data)
    // make_dirty indicates write allocate on store for WB
    AccessResult fill(uint64_t byte_addr, bool make_dirty);

    // For hierarchical writeback: write back an evicted block into this cache
    // (treat as a write to that block, but without counting as a demand access)
    void writeback_block(uint64_t block_addr);

    // Prefetch buffer helper: check if demand access hits buffer
    bool prefetch_hit_consume(uint64_t block_addr) { return pfb_.consume_if_present(block_addr); }
    void prefetch_push(uint64_t block_addr) { pfb_.push(block_addr); }

    // Address helpers
    uint64_t block_addr(uint64_t byte_addr) const;
    uint64_t next_block_addr(uint64_t byte_addr) const;

    const CacheConfig& cfg() const { return cfg_; }
    const CacheStats& stats() const { return stats_; }
    const PrefetchStats& pstats() const { return pfb_.stats(); }

private:
    struct Line {
        bool valid = false;
        bool dirty = false;
        uint64_t tag = 0;
        uint64_t last_use = 0; // LRU timestamp
    };

    CacheConfig cfg_;
    CacheStats stats_;
    PrefetchBuffer pfb_;

    std::size_t num_sets_ = 0;
    std::size_t offset_bits_ = 0;
    std::size_t index_bits_ = 0;
    uint64_t use_counter_ = 0;

    std::vector<std::vector<Line>> sets_;

private:
    void validate_cfg();
    void decode(uint64_t byte_addr, uint64_t& tag, std::size_t& set_idx) const;
    int find_way(std::size_t set_idx, uint64_t tag) const;
    std::size_t choose_victim(std::size_t set_idx) const;

    AccessResult install(std::size_t set_idx, std::size_t way, uint64_t tag, bool dirty);
};
