#include "trace.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<TraceOp> TraceReader::read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Failed to open trace file: " + path);

    std::vector<TraceOp> ops;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        std::istringstream iss(line);
        char op;
        std::string addr_s;
        if (!(iss >> op >> addr_s)) continue;

        if (op == 'R') op = 'r';
        if (op == 'W') op = 'w';
        if (op != 'r' && op != 'w') continue;

        uint64_t addr = std::stoull(addr_s, nullptr, 0);
        ops.push_back({op, addr});
    }
    return ops;
}
