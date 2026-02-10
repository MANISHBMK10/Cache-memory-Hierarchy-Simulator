#include "hierarchy.hpp"
#include "trace.hpp"
#include <iostream>
#include <string>
#include <stdexcept>

static void usage(const char* p) {
    std::cerr
      << "Two-Level Cache & Memory Hierarchy Simulator\n\n"
      << "Required:\n"
      << "  --trace <file>\n\n"
      << "L1 options:\n"
      << "  --l1_size <bytes> --l1_block <bytes> --l1_assoc <ways>\n"
      << "L2 options:\n"
      << "  --l2_size <bytes> --l2_block <bytes> --l2_assoc <ways>\n\n"
      << "Policies (both levels):\n"
      << "  --l1_wb 1|0 --l1_wa 1|0\n"
      << "  --l2_wb 1|0 --l2_wa 1|0\n\n"
      << "Prefetch:\n"
      << "  --l1_pfb <entries> --l1_nlp 1|0   (nlp = next-line prefetch)\n"
      << "  --l2_pfb <entries> --l2_nlp 1|0\n\n"
      << "Example:\n"
      << "  " << p << " --trace traces/t.txt "
      << "--l1_size 32768 --l1_block 64 --l1_assoc 8 --l1_wb 1 --l1_wa 1 --l1_pfb 8 --l1_nlp 1 "
      << "--l2_size 262144 --l2_block 64 --l2_assoc 8 --l2_wb 1 --l2_wa 1 --l2_pfb 16 --l2_nlp 1\n";
}

static bool isflag(const std::string& a, const std::string& f) { return a == f; }

int main(int argc, char** argv) {
    try {
        if (argc == 1) { usage(argv[0]); return 1; }

        CacheConfig l1, l2;
        l1.name = "L1"; l2.name = "L2";

        // sensible defaults
        l1.size_bytes = 32768; l1.block_bytes = 64; l1.assoc = 8;
        l2.size_bytes = 262144; l2.block_bytes = 64; l2.assoc = 8;

        std::string trace_path;

        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            auto need = [&](const std::string& f)->std::string{
                if (i+1 >= argc) throw std::invalid_argument("Missing value for " + f);
                return std::string(argv[++i]);
            };

            if (isflag(a,"--trace")) trace_path = need(a);

            else if (isflag(a,"--l1_size")) l1.size_bytes = std::stoull(need(a));
            else if (isflag(a,"--l1_block")) l1.block_bytes = std::stoull(need(a));
            else if (isflag(a,"--l1_assoc")) l1.assoc = std::stoull(need(a));
            else if (isflag(a,"--l1_wb")) l1.wp = (std::stoull(need(a))? WritePolicy::WriteBack:WritePolicy::WriteThrough);
            else if (isflag(a,"--l1_wa")) l1.ap = (std::stoull(need(a))? AllocatePolicy::WriteAllocate:AllocatePolicy::NoWriteAllocate);
            else if (isflag(a,"--l1_pfb")) l1.prefetch_buf_entries = std::stoull(need(a));
            else if (isflag(a,"--l1_nlp")) l1.next_line_prefetch = (std::stoull(need(a))!=0);

            else if (isflag(a,"--l2_size")) l2.size_bytes = std::stoull(need(a));
            else if (isflag(a,"--l2_block")) l2.block_bytes = std::stoull(need(a));
            else if (isflag(a,"--l2_assoc")) l2.assoc = std::stoull(need(a));
            else if (isflag(a,"--l2_wb")) l2.wp = (std::stoull(need(a))? WritePolicy::WriteBack:WritePolicy::WriteThrough);
            else if (isflag(a,"--l2_wa")) l2.ap = (std::stoull(need(a))? AllocatePolicy::WriteAllocate:AllocatePolicy::NoWriteAllocate);
            else if (isflag(a,"--l2_pfb")) l2.prefetch_buf_entries = std::stoull(need(a));
            else if (isflag(a,"--l2_nlp")) l2.next_line_prefetch = (std::stoull(need(a))!=0);

            else if (isflag(a,"--help") || isflag(a,"-h")) { usage(argv[0]); return 0; }
            else throw std::invalid_argument("Unknown arg: " + a);
        }

        if (trace_path.empty()) throw std::invalid_argument("Missing --trace <file>");

        auto ops = TraceReader::read_file(trace_path);

        CacheHierarchy h(l1, l2);
        for (const auto& t : ops) h.access(t.op, t.addr);

        const auto& s1 = h.L1().stats();
        const auto& s2 = h.L2().stats();
        const auto& p1 = h.L1().pstats();
        const auto& p2 = h.L2().pstats();
        const auto& hs = h.hstats();

        auto rate = [](uint64_t hits, uint64_t misses)->double{
            uint64_t tot = hits + misses;
            return tot ? (double)misses / (double)tot : 0.0;
        };

        uint64_t l1_hits = s1.read_hits + s1.write_hits;
        uint64_t l1_miss = s1.read_misses + s1.write_misses;

        uint64_t l2_hits = s2.read_hits + s2.write_hits;
        uint64_t l2_miss = s2.read_misses + s2.write_misses;

        std::cout << "=== Results ===\n";
        std::cout << "Trace accesses: " << ops.size() << "\n\n";

        std::cout << "[L1] hits=" << l1_hits << " misses=" << l1_miss
                  << " miss_rate=" << rate(l1_hits, l1_miss)
                  << " evictions=" << s1.evictions << " writebacks=" << s1.writebacks << "\n";
        std::cout << "     prefetch_issued=" << p1.issued << " pfb_hits=" << hs.l1_prefetch_dem_hits
                  << " pfb_drops=" << p1.drops << "\n\n";

        std::cout << "[L2] hits=" << l2_hits << " misses=" << l2_miss
                  << " miss_rate=" << rate(l2_hits, l2_miss)
                  << " evictions=" << s2.evictions << " writebacks=" << s2.writebacks << "\n";
        std::cout << "     prefetch_issued=" << p2.issued << " pfb_hits=" << hs.l2_prefetch_dem_hits
                  << " pfb_drops=" << p2.drops << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        usage(argv[0]);
        return 1;
    }
}
