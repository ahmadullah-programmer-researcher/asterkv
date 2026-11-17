# AsterKV

AsterKV is an original, compact embedded ordered key-value storage engine written in C++20, with a small binary-safe Python interface through a C ABI and `ctypes`.

It is **inspired by public storage-engine design ideas** used by projects such as Google's LevelDB. It is not a fork, port, or source-code reproduction of LevelDB, and it does not claim LevelDB's feature set or production maturity.

## Features

- Binary-safe string keys and values.
- Ordered in-memory index (`std::map`).
- `put`, `get`, `delete`, atomic multi-operation batches.
- Append-only write-ahead log (WAL) with checksums.
- Recovery of complete WAL records after restart; incomplete final records are ignored.
- Atomic checkpoint installation and WAL truncation.
- Point-in-time snapshots (copy-based in this version).
- Ordered range scans with inclusive start and exclusive end.
- Thread-safe operations within one process.
- C API and Python `ctypes` wrapper.
- CLI, tests and benchmark.

## Current design limits

This is an educational/research prototype, not a production database. It currently uses one checkpoint file plus one WAL, not LevelDB's multi-level SSTables, bloom filters, block cache or background compaction. Snapshot creation copies the full in-memory map. There is no multi-process lock, client/server protocol, replication, compression, SQL layer or crash-fault-injection suite. Only a single process should open a given database directory at a time.

## Requirements

- CMake 3.20+
- C++20 compiler (GCC 11+, Clang 13+, or recent MSVC)
- Python 3.10+ for the wrapper (optional)

## Build

### Windows PowerShell

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The shared library will normally be `build/Release/asterkv.dll` with Visual Studio generators.

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## CLI examples

```bash
./build/asterkv ./data put user:1 "Ahmad"
./build/asterkv ./data get user:1
./build/asterkv ./data put user:2 "Researcher"
./build/asterkv ./data scan
./build/asterkv ./data stats
./build/asterkv ./data flush
```

On Windows, run the executable under `build/Release/asterkv.exe` if using a Visual Studio generator.

## C++ example

```bash
cmake --build build --target asterkv_cpp_example
./build/asterkv_cpp_example
```

The example is also available at `examples/cpp_example.cpp`.

## Python example

Set `ASTERKV_LIBRARY` to the built shared-library path if it is not in the wrapper's default search locations.

PowerShell:

```powershell
$env:ASTERKV_LIBRARY = (Resolve-Path .\build\Release\asterkv.dll).Path
python .\examples\python_example.py
```

Linux/macOS:

```bash
export ASTERKV_LIBRARY="$PWD/build/libasterkv.so" # macOS: libasterkv.dylib
python3 examples/python_example.py
```

## Minimal C++ usage

```cpp
#include "asterkv/db.hpp"
#include <iostream>

int main() {
    auto db = asterkv::Database::open("./data");
    db->put("name", "AsterKV");
    if (auto value = db->get("name")) std::cout << *value << '\n';
    db->flush();
}
```

## Repository layout

```text
include/asterkv/   Public C++ and C interfaces
src/               Storage engine and C API implementation
tools/             Command-line utility
tests/             Functional and recovery tests
python/             Python ctypes wrapper
examples/           Small examples
benchmarks/         Microbenchmark
docs/               Architecture, format, security and research notes
```

## Data files

- `store.wal`: append-only durable write batches.
- `store.akv`: latest compacted checkpoint.
- `store.akv.tmp`: temporary checkpoint during installation.

## License

MIT License. See `LICENSE`.
