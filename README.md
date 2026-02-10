Cache and Memory Hierarchy Simulator

Features:
- Two-level cache hierarchy (L1 + L2)
- Configurable n-way associativity
- LRU replacement
- Write-back / write-allocate
- Prefetch buffers (next-line)
- Trace-driven evaluation

Build:
  make

Run example:
  ./cache_sim --trace traces/trace.txt --l1_size 32768 --l1_block 64 --l1_assoc 8 --l1_wb 1 --l1_wa 1 --l1_pfb 8 --l1_nlp 1 --l2_size 262144 --l2_block 64 --l2_assoc 8 --l2_wb 1 --l2_wa 1 --l2_pfb 16 --l2_nlp 1
