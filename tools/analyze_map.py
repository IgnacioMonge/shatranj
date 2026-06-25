import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_map.py <map_file>")
        sys.returncode = 1
        return

    map_file = sys.argv[1]
    symbols = []
    with open(map_file, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 3 and parts[1] == "=" and parts[2].startswith("$"):
                name = parts[0]
                try:
                    addr = int(parts[2].replace("$", "0x"), 16)
                    # only care about things in normal space, e.g. code/data, not constants
                    if addr >= 0x6000:
                        symbols.append((addr, name))
                except:
                    pass

    symbols.sort(key=lambda x: x[0])

    sizes = []
    for i in range(len(symbols)-1):
        addr, name = symbols[i]
        next_addr = symbols[i+1][0]
        size = next_addr - addr
        if size > 0 and size < 10000: # reasonable size filter to ignore gap jumps
            sizes.append((size, name, addr))

    sizes.sort(key=lambda x: x[0], reverse=True)

    print("Largest 50 symbols:")
    for size, name, addr in sizes[:50]:
        print(f"{size:6} bytes - {name} (0x{addr:X})")

if __name__ == "__main__":
    main()
