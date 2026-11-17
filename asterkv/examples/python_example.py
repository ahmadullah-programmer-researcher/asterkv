from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from asterkv import Database

with Database("example-data") as db:
    db.put("language", "C++ and Python")
    db.put(b"binary\x00key", b"binary\x00value")
    print(db.get("language").decode())
    print(db.get(b"binary\x00key"))
    db.flush()
