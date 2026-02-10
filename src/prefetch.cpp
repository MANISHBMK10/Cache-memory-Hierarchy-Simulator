#include "prefetch.hpp"

PrefetchBuffer::PrefetchBuffer(std::size_t capacity) : cap_(capacity) {}

void PrefetchBuffer::reset() {
    fifo_.clear();
    set_.clear();
    stats_ = {};
}

void PrefetchBuffer::push(uint64_t block_addr) {
    if (cap_ == 0) return;

    // Avoid duplicates
    if (set_.find(block_addr) != set_.end()) return;

    stats_.issued++;

    if (fifo_.size() >= cap_) {
        // drop oldest
        uint64_t old = fifo_.front();
        fifo_.pop_front();
        set_.erase(old);
        stats_.drops++;
    }
    fifo_.push_back(block_addr);
    set_.insert(block_addr);
}

bool PrefetchBuffer::consume_if_present(uint64_t block_addr) {
    if (cap_ == 0) return false;
    auto it = set_.find(block_addr);
    if (it == set_.end()) return false;

    // Remove from set + fifo (linear in cap; cap is small, e.g., 4/8/16)
    set_.erase(it);
    for (auto qit = fifo_.begin(); qit != fifo_.end(); ++qit) {
        if (*qit == block_addr) { fifo_.erase(qit); break; }
    }
    stats_.hits++;
    return true;
}
