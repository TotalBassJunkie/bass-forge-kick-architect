import re, sys, os, json

def strings(path, minlen=5):
    data = open(path, 'rb').read()
    out = set()
    # ASCII
    for m in re.finditer(rb'[\x20-\x7e]{%d,}' % minlen, data):
        out.add(m.group().decode('ascii', 'ignore'))
    # UTF-16LE
    for m in re.finditer(rb'(?:[\x20-\x7e]\x00){%d,}' % minlen, data):
        s = m.group().decode('utf-16le', 'ignore')
        out.add(s)
    return out

if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    s = strings(src)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, 'w', encoding='utf-8') as f:
        for line in sorted(s):
            f.write(line.replace('\n', ' ').replace('\r', ' ') + '\n')
    print(len(s), 'strings ->', dst)
