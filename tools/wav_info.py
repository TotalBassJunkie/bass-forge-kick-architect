import wave, sys, os

path = sys.argv[1]
with wave.open(path, "rb") as w:
    print("Datei      :", os.path.basename(path))
    print("Kanaele    :", w.getnchannels())
    print("Rate       :", w.getframerate(), "Hz")
    print("Bittiefe   :", w.getsampwidth() * 8, "bit")
    print("Samples    :", w.getnframes())
    print("Dauer      : %.3f s" % (w.getnframes() / w.getframerate()))
print("Groesse    :", os.path.getsize(path), "B")
