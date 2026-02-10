#include "cache.hpp"
#include "util.hpp"
#include <stdexcept>

Cache::Cache(const CacheConfig& cfg) : cfg_(cfg), pfb_(cfg.prefetch_buf_entries) {
    validate_cfg();
    reset();
}

void Cache::reset() {
    use_counter_ = 0;
    stats_ = {};
    pfb_.reset();

    std::size_t lines = cfg_.size_bytes / cfg_.block_bytes;
    num_sets_ = lines / cfg_.assoc;
    offset_bits_ = ilog2_pow2(cfg_.block_bytes);
    index_bits_  = ilog2_pow2(num_sets_);

    sets_.assign(num_sets_, std::vector<Line>(cfg_.assoc));
}

void Cache::validate_cfg() {
    if (cfg_.size_bytes == 0 || cfg_.block_bytes == 0 || cfg_.assoc == 0)
        throw std::invalid_argument(cfg_.name + ": size/block/assoc must be > 0");

    if (!is_pow2(cfg_.block_bytes))
        throw std::invalid_argument(cfg_.name + ": block_bytes must be power-of-two");

    if (cfg_.size_bytes % cfg_.block_bytes != 0)
        throw std::invalid_argument(cfg_.name + ": size_bytes must be multiple of block_bytes");

    std::size_t lines = cfg_.size_bytes / cfg_.block_bytes;
    if (lines % cfg_.assoc != 0)
        throw std::invalid_argument(cfg_.name + ": num_lines must be divisible by assoc");

    std::size_t sets = lines / cfg_.assoc;
    if (!is_pow2(sets))
        throw std::invalid_argument(cfg_.name + ": num_sets must be power-of-two");

    if (cfg_.repl != "lru")
        throw std::invalid_argument(cfg_.name + ": only repl=lru supported");
}

uint64_t Cache::block_addr(uint64_t byte_addr) const {
    return byte_addr >> offset_bits_;
}

uint64_t Cache::next_block_addr(uint64_t byte_addr) const {
    // next line prefetch = current block + 1
    return block_addr(byte_addr) + 1ULL;
}

void Cache::decode(uint64_t byte_addr, uint64_t& tag, std::size_t& set_idx) const {
    uint64_t b = block_addr(byte_addr);
    set_idx = static_cast<std::size_t>(b & (static_cast<uint64_t>(num_sets_ - 1)));
    tag = b >> index_bits_;
}

int Cache::find_way(std::size_t set_idx, uint64_t tag) const {
    const auto& set = sets_[set_idx];
    for (std::size_t w = 0; w < set.size(); ++w) {
        if (set[w].valid && set[w].tag == tag) return static_cast<int>(w);
    }
    return -1;
}

std::size_t Cache::choose_victim(std::size_t set_idx) const {
    const auto& set = sets_[set_idx];
    for (std::size_t w = 0; w < set.size(); ++w)
        if (!set[w].valid) return w;

    std::size_t victim = 0;
    uint64_t best = set[0].last_use;
    for (std::size_t w = 1; w < set.size(); ++w) {
        if (set[w].last_use < best) { best = set[w].last_use; victim = w; }
    }
    return victim;
}

AccessResult Cache::install(std::size_t set_idx, std::size_t way, uint64_t tag, bool dirty) {
    AccessResult res;
    auto& line = sets_[set_idx][way];

    if (line.valid) {
        res.eviction = true;
        stats_.evictions++;

        // reconstruct evicted block addr = (tag << index_bits) | set_idx
        res.evicted_block_addr = (line.tag << index_bits_) | static_cast<uint64_t>(set_idx);

        if (cfg_.wp == WritePolicy::WriteBack && line.dirty) {
            res.eviction_dirty = true;
            stats_.writebacks++;
        }
    }

    line.valid = true;
    line.tag = tag;
    line.dirty = dirty;
    line.last_use = use_counter_;
    return res;
}

AccessResult Cache::access(char op, uint64_t byte_addr) {
    if (op != 'r' && op != 'w') return {};

    use_counter_++;

    if (op == 'r') stats_.reads++;
    else stats_.writes++;

    uint64_t tag; std::size_t set_idx;
    decode(byte_addr, tag, set_idx);

    int way = find_way(set_idx, tag);
    if (way >= 0) {
        auto& line = sets_[set_idx][static_cast<std::size_t>(way)];
        line.last_use = use_counter_;

        if (op == 'r') stats_.read_hits++;
        else {
            stats_.write_hits++;
            if (cfg_.wp == WritePolicy::WriteBack) line.dirty = true;
            // WT would "write to memory" at this level; hierarchy models that.
        }

        return {.hit=true};
    }

    // miss
    if (op == 'r') stats_.read_misses++;
    else stats_.write_misses++;

    return {.hit=false};
}

AccessResult Cache::fill(uint64_t byte_addr, bool make_dirty) {
    use_counter_++;

    uint64_t tag; std::size_t set_idx;
    decode(byte_addr, tag, set_idx);

    // If already present, just update dirty/use
    int way = find_way(set_idx, tag);
    if (way >= 0) {
        auto& line = sets_[set_idx][static_cast<std::size_t>(way)];
        line.last_use = use_counter_;
        if (make_dirty && cfg_.wp == WritePolicy::WriteBack) line.dirty = true;
        return {.hit=true};
    }

    std::size_t victim = choose_victim(set_idx);
    return install(set_idx, victim, tag, (make_dirty && cfg_.wp == WritePolicy::WriteBack));
}

void Cache::writeback_block(uint64_t block_addr_in) {
    // treat as a write to that block (no demand stats)
    use_counter_++;

    // convert block->byte to reuse decode
    uint64_t byte_addr = block_addr_in << offset_bits_;

    uint64_t tag; std::size_t set_idx;
    decode(byte_addr, tag, set_idx);

    int way = find_way(set_idx, tag);
    if (way >= 0) {
        auto& line = sets_[set_idx][static_cast<std::size_t>(way)];
        line.last_use = use_counter_;
        if (cfg_.wp == WritePolicy::WriteBack) line.dirty = true;
        return;
    }

    std::size_t victim = choose_victim(set_idx);
    auto res = install(set_idx, victim, tag, (cfg_.wp == WritePolicy::WriteBack));
    (void)res;
}
