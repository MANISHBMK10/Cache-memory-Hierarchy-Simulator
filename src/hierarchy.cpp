#include "hierarchy.hpp"

CacheHierarchy::CacheHierarchy(const CacheConfig& l1, const CacheConfig& l2)
    : l1_(l1), l2_(l2) {}

void CacheHierarchy::reset() {
    l1_.reset();
    l2_.reset();
    hstats_ = {};
}

void CacheHierarchy::maybe_prefetch(Cache& c, uint64_t addr) {
    if (!c.cfg().next_line_prefetch) return;
    if (c.cfg().prefetch_buf_entries == 0) return;

    uint64_t next_blk = c.next_block_addr(addr);
    c.prefetch_push(next_blk);
}

void CacheHierarchy::access(char op, uint64_t addr) {
    if (op != 'r' && op != 'w') return;

    // -----------------------------
    // 0) Prefetch buffer checks
    // -----------------------------

    // L1 prefetch buffer demand hit check
    uint64_t l1_blk = l1_.block_addr(addr);
    if (l1_.cfg().prefetch_buf_entries && l1_.prefetch_hit_consume(l1_blk)) {
        hstats_.l1_prefetch_dem_hits++;

        // Fill L1 with the prefetched line (clean)
        auto ev = l1_.fill(addr, /*make_dirty=*/false);
        if (ev.eviction && ev.eviction_dirty) {
            // writeback evicted dirty line from L1 into L2
            l2_.writeback_block(ev.evicted_block_addr);
        }

        // Now do the real access (will be a hit in L1)
        (void)l1_.access(op, addr);

        // Issue next-line prefetches
        maybe_prefetch(l1_, addr);
        return;
    }

    // -----------------------------
    // 1) L1 access
    // -----------------------------
    auto r1 = l1_.access(op, addr);
    if (r1.hit) {
        maybe_prefetch(l1_, addr);
        return;
    }

    // Decide whether L1 will allocate on this op
    bool l1_will_allocate = !(op == 'w' && l1_.cfg().ap == AllocatePolicy::NoWriteAllocate);

    // -----------------------------
    // 2) L2 prefetch buffer demand hit check
    // -----------------------------
    uint64_t l2_blk = l2_.block_addr(addr);
    bool l2_pfb_hit = (l2_.cfg().prefetch_buf_entries && l2_.prefetch_hit_consume(l2_blk));
    if (l2_pfb_hit) {
        hstats_.l2_prefetch_dem_hits++;

        // Fill L2 with prefetched line (clean)
        (void)l2_.fill(addr, /*make_dirty=*/false);
        // Continue to L2 access below (should now be a hit)
    }

    // -----------------------------
    // 3) L2 access
    // -----------------------------
    auto r2 = l2_.access(op, addr);

    if (!r2.hit) {
        // L2 miss -> memory (modeled as always returns the block)
        // Fill L2: if store + write-allocate => may become dirty in WB
        bool l2_make_dirty = (op == 'w') && (l2_.cfg().ap == AllocatePolicy::WriteAllocate);
        auto ev2 = l2_.fill(addr, l2_make_dirty);
        (void)ev2;
    } else {
        // L2 hit: if WT and op is write, it would write to memory;
        // (not tracked separately unless you add a memory-write stat)
    }

    // Issue L2 next-line prefetch
    maybe_prefetch(l2_, addr);

    // -----------------------------
    // 4) **CRITICAL FIX**: Fill L1 on demand miss
    // -----------------------------
    if (l1_will_allocate) {
        bool l1_make_dirty = (op == 'w') && (l1_.cfg().ap == AllocatePolicy::WriteAllocate);

        auto ev1 = l1_.fill(addr, l1_make_dirty);

        // If L1 evicted a dirty line, write it back to L2
        if (ev1.eviction && ev1.eviction_dirty) {
            l2_.writeback_block(ev1.evicted_block_addr);
        }
    }

    // Issue L1 next-line prefetch after demand install
    maybe_prefetch(l1_, addr);
}
