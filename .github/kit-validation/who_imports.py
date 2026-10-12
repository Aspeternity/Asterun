"""Investigation helper (kit validation only): for the DLLs a process loaded
(Load Image rows of a Process Monitor CSV), list which of them import the
given DLL names statically or as delay-load imports."""
import csv, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from verify_pe_contract import PeImage, PeError  # noqa: E402

csv_path, process, names = sys.argv[1], sys.argv[2].lower(), [n.lower() for n in sys.argv[3].split(",")]
loaded = []
with open(csv_path, encoding="utf-8-sig", newline="") as handle:
    for row in csv.DictReader(handle):
        if row["Process Name"].lower() == process and row["Operation"] == "Load Image":
            if row["Path"] not in loaded:
                loaded.append(row["Path"])
print(f"{process}: {len(loaded)} images loaded")
for order, path in enumerate(loaded):
    try:
        image = PeImage(Path(path))
        imports = [i.lower() for i in image.imports()]
        delay = [i.lower() for i in image.delay_imports()]
    except (PeError, OSError, Exception) as error:  # noqa: BLE001
        continue
    hits = [f"{n} (static)" for n in names if n in imports] + [f"{n} (delay)" for n in names if n in delay]
    if hits:
        print(f"#{order:03d} {path} imports {', '.join(hits)}")
for name in names:
    positions = [i for i, p in enumerate(loaded) if p.lower().endswith("\\" + name)]
    print(f"{name} load order position: {positions}")
