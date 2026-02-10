#pragma once
#include "cache.hpp"

struct HierarchyStats {
    uint64_t l1_prefetch_dem_hits = 0;
    uint64_t l2_prefetch_dem_hits = 0;
};

class CacheHierarchy {
public:
    CacheHierarchy(const CacheConfig& l1, const CacheConfig& l2);

    void reset();

    // Demand access from CPU: returns final hit status (L1/L2/mem)
    void access(char op, uint64_t addr);

    const Cache& L1() const { return l1_; }
    const Cache& L2() const { return l2_; }
    const HierarchyStats& hstats() const { return hstats_; }

private:
    Cache l1_;
    Cache l2_;
    HierarchyStats hstats_;

private:
    void maybe_prefetch(Cache& c, uint64_t addr);
};
