#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct TraceOp {
    char op;        // 'r' or 'w'
    uint64_t addr;  // byte address
};

class TraceReader {
public:
    // Lines like: "r 0x1234" or "w 1234". Ignores blanks and lines starting '#'.
    static std::vector<TraceOp> read_file(const std::string& path);
};
