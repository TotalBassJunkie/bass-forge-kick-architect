import pypdf, sys, os

src = sys.argv[1]
dst = sys.argv[2]
r = pypdf.PdfReader(src)
print("pages:", len(r.pages))
out = []
for i, pg in enumerate(r.pages):
    try:
        t = pg.extract_text() or ""
    except Exception as e:
        t = f"[ERR {e}]"
    out.append(f"\n===== PAGE {i+1} =====\n" + t)
txt = "".join(out)
os.makedirs(os.path.dirname(dst), exist_ok=True)
open(dst, "w", encoding="utf-8").write(txt)
print("chars:", len(txt))
