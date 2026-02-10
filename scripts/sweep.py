import itertools, subprocess, re, sys

BIN = "./cache_sim"

def run(cfg):
    cmd = [BIN] + cfg
    out = subprocess.check_output(cmd, text=True)
    # parse L1 miss_rate
    m = re.search(r"\[L1\].*miss_rate=([0-9.]+)", out)
    if not m:
        raise RuntimeError("failed parse")
    return float(m.group(1)), out

def main():
    trace = sys.argv[1] if len(sys.argv) > 1 else "traces/trace.txt"

    l1_sizes = ["16384","32768","65536"]
    l1_assoc = ["2","4","8"]
    l2_sizes = ["131072","262144","524288"]
    l2_assoc = ["4","8"]
    pfb = ["0","8","16"]

    best = (1e9, None, None)

    for l1s, l1a, l2s, l2a, p in itertools.product(l1_sizes, l1_assoc, l2_sizes, l2_assoc, pfb):
        cfg = [
            "--trace", trace,
            "--l1_size", l1s, "--l1_block", "64", "--l1_assoc", l1a, "--l1_wb", "1", "--l1_wa", "1",
            "--l2_size", l2s, "--l2_block", "64", "--l2_assoc", l2a, "--l2_wb", "1", "--l2_wa", "1",
            "--l1_pfb", p, "--l1_nlp", "1",
            "--l2_pfb", p, "--l2_nlp", "1",
        ]
        mr, out = run(cfg)
        if mr < best[0]:
            best = (mr, cfg, out)
            print("NEW BEST miss_rate=", mr, "cfg=", " ".join(cfg))

    print("\n=== BEST CONFIG ===")
    print("miss_rate=", best[0])
    print("cfg=", " ".join(best[1]))
    print(best[2])

if __name__ == "__main__":
    main()
