"""Schneller WAV-Header-Scan der Content-Library.

Liest nur die Kopfdaten (kein Dekodieren) und zaehlt Format, Rate, Kanaele,
Bittiefe je Ordnergruppe. Ergebnis: Uebersicht + ggf. Auffaelligkeiten.
"""
import os, struct, sys
from collections import Counter, defaultdict

FMT_NAMES = {1: "PCM-int", 3: "IEEE-float", 6: "A-law", 7: "mu-law", 0xFFFE: "extensible"}


def read_header(path):
    with open(path, "rb") as f:
        head = f.read(4096)
    if len(head) < 44 or head[0:4] != b"RIFF" or head[8:12] != b"WAVE":
        return None
    pos = 12
    fmt = None
    while pos + 8 <= len(head):
        cid = head[pos:pos + 4]
        csz = struct.unpack_from("<I", head, pos + 4)[0]
        body = pos + 8
        if cid == b"fmt " and body + 16 <= len(head):
            tag, ch, rate, _brate, _align, bits = struct.unpack_from("<HHIIHH", head, body)
            fmt = (tag, ch, rate, bits)
            if tag == 0xFFFE and body + 40 <= len(head):
                fmt = (struct.unpack_from("<H", head, body + 24)[0], ch, rate, bits)
        if cid == b"data":
            break
        pos = body + csz + (csz & 1)
    return fmt


def main(root):
    per_group = defaultdict(Counter)
    per_group_files = Counter()
    formats = Counter()
    total = 0

    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            if not name.lower().endswith(".wav"):
                continue
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, root)
            group = rel.split(os.sep)[0]
            info = read_header(full)
            total += 1
            per_group_files[group] += 1
            if info is None:
                formats["unlesbar"] += 1
                per_group[group]["unlesbar"] += 1
                continue
            tag, ch, rate, bits = info
            key = "%s %dbit %dch %dHz" % (FMT_NAMES.get(tag, "tag%d" % tag), bits, ch, rate)
            formats[key] += 1
            per_group[group][key] += 1

    print("Dateien gesamt:", total)
    print("\n=== Formate insgesamt ===")
    for k, v in formats.most_common():
        print("  %-32s %5d" % (k, v))

    print("\n=== Formate je Gruppe ===")
    for g in sorted(per_group):
        print("\n%s  (%d Dateien)" % (g, per_group_files[g]))
        for k, v in per_group[g].most_common(4):
            print("    %-32s %5d" % (k, v))


if __name__ == "__main__":
    main(sys.argv[1])
