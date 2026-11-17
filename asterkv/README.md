# AsterKV

### An Embedded Key-Value Storage Engine Built with C++20

AsterKV is an independent, research-oriented embedded key-value storage engine developed in C++20, with a Python interface.

Inspired by the architectural principles of Google's LevelDB, AsterKV explores how database systems store information, maintain consistency, recover from failures and manage persistent data.

The project is designed to provide practical experience with database internals, systems programming, data structures, storage architecture and low-level software engineering.

> **Project status:** Early development (v0.1.0). Educational and research-oriented implementation, not a production-ready database.

---

## Table of Contents

- [Overview](#overview)
- [Objectives](#objectives)
- [Features](#features)
- [Architecture](#architecture)
- [How It Works](#how-it-works)
- [Technology Stack](#technology-stack)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Installation](#installation)
- [Building from Source](#building-from-source)
- [Running Tests](#running-tests)
- [Using AsterKV](#using-asterkv)
- [Python Integration](#python-integration)
- [Performance and Benchmarking](#performance-and-benchmarking)
- [Current Limitations](#current-limitations)
- [Roadmap](#roadmap)
- [Research Directions](#research-directions)
- [Inspiration and References](#inspiration-and-references)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

Databases are fundamental components of modern computing infrastructure. Applications depend on them to store, retrieve and manage information reliably.

AsterKV explores the internal engineering behind embedded database systems.

Instead of treating a database as a black box, the project implements fundamental storage-engine concepts, including:

- Persistent key-value storage
- Write-ahead logging (WAL)
- Ordered in-memory indexing
- Atomic write batches
- Snapshot-based reads
- Checkpointing
- Recovery through log replay
- Data-integrity verification

The goal is to build a clear, extensible foundation for experimenting with database architecture and storage algorithms.

## Objectives

The primary objectives are:

1. Understand how embedded databases manage persistent information.
2. Implement a storage engine using modern C++.
3. Explore write-ahead logging and crash recovery.
4. Understand transaction and snapshot semantics.
5. Study indexing and data structures used in database systems.
6. Build a Python interface for interacting with the native engine.
7. Establish a foundation for future LSM-tree and SSTable development.
8. Evaluate database correctness and performance through testing and benchmarking.

## Features

| Feature | Description |
|---|---|
| Key-value storage | Store and retrieve key-value records |
| Persistent storage | Preserve database state across restarts |
| Write-ahead logging | Record updates in an append-only log |
| Recovery | Reconstruct database state through log replay |
| Ordered indexing | Maintain keys in sorted order |
| Atomic batches | Group multiple updates into one logical operation |
| Snapshots | Provide point-in-time read views |
| Checkpointing | Persist the current database state |
| Checksums | Detect certain forms of record corruption |
| C++ API | Native database interface |
| Python integration | Access the native library through ctypes |
| CLI | Perform database operations from a terminal |
| Automated tests | Verify core database behaviour |

## Architecture

AsterKV currently uses a log-based persistence architecture with an ordered in-memory index and a checkpoint file.

### High-level architecture

```text
              Application
                   |
          +--------+--------+
          |                 |
       C++ API          Python API
          |                 |
          +--------+--------+
                   |
              AsterKV Core
                   |
          +--------+--------+
          |                 |
      Write Engine      Read Engine
          |                 |
          +--------+--------+
                   |
            Ordered Index
                   |
          +--------+--------+
          |                 |
       WAL Log          Checkpoint
          |                 |
          +--------+--------+
                   |
              Persistent Disk
```

### Main Components

**1. Write-Ahead Log**

The write-ahead log records updates before they are applied to the in-memory database state.

It supports recovery by allowing the engine to replay valid operations after restarting.

**2. Ordered In-Memory Index**

The database maintains an ordered representation of keys, enabling efficient lookup and ordered iteration.

**3. Checkpointing**

Checkpointing saves the current database state to persistent storage, reducing the amount of log data that needs to be replayed during recovery.

**4. Recovery Engine**

During startup, the recovery engine loads the checkpoint and replays subsequent valid log records.

**5. Snapshot Management**

Snapshots provide a consistent point-in-time view of the database for read operations.

**6. Python Interface**

The Python interface communicates with the native C++ library through `ctypes`.

## How It Works

A simplified write operation follows this sequence:

1. An application submits a key-value update.
2. The engine records the operation in the write-ahead log.
3. The in-memory index is updated.
4. The database can persist its current state through checkpointing.
5. During recovery, the engine reconstructs the state from the checkpoint and valid log records.

This design introduces important database concepts without claiming to implement the complete architecture of LevelDB.

## Technology Stack

| Technology | Purpose |
|---|---|
| C++20 | Core storage engine |
| Python | High-level integration |
| CMake | Build configuration |
| ctypes | Python-to-C++ interface |
| Zlib | Checksum and integrity support, where configured |
| C++ testing tools | Native unit and integration tests |
| PyTest | Python-side testing |

## Project Structure

```text
asterkv/
│
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
│
├── include/
│   └── asterkv/
│       ├── database.h
│       ├── options.h
│       ├── status.h
│       └── write_batch.h
│
├── src/
│   ├── database.cpp
│   ├── storage.cpp
│   ├── wal.cpp
│   ├── recovery.cpp
│   ├── snapshot.cpp
│   └── checksum.cpp
│
├── python/
│   └── asterkv/
│
├── cli/
│   └── main.cpp
│
├── tests/
│   ├── database_test.cpp
│   ├── recovery_test.cpp
│   ├── integrity_test.cpp
│   └── test_python.py
│
├── benchmarks/
│   └── benchmark.cpp
│
├── examples/
│   ├── cpp_example.cpp
│   └── python_example.py
│
└── docs/
    ├── architecture.md
    ├── file_format.md
    └── development.md
```

*The structure above describes the intended organization. Refer to the actual repository files for the components implemented in the current release.*

## Requirements

Before building AsterKV, install:

- A C++20-compatible compiler
- CMake 3.20 or newer
- Git
- Python 3.10 or newer for Python integration
- Zlib development libraries if required by the selected build configuration

Supported development environments:

- Windows
- Linux
- macOS

Platform compatibility depends on the availability of the required compiler and build dependencies.

## Installation

Clone the repository:

```bash
git clone https://github.com/YOUR_USERNAME/asterkv.git
```

Enter the project directory:

```bash
cd asterkv
```

## Building from Source

### Linux and macOS

Configure the project:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Compile:

```bash
cmake --build build --parallel
```

### Windows

Open PowerShell in the project directory:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Build:

```powershell
cmake --build build --config Release
```

## Running Tests

Run the CMake-configured test suite:

```bash
ctest --test-dir build --output-on-failure
```

On Windows, use:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Tests should cover:

- Database initialization
- Key insertion
- Value retrieval
- Key deletion
- Write-batch behaviour
- Snapshot consistency
- Database reopening
- Log recovery
- Corruption detection
- Python integration

Only tests actually present and enabled in the repository should be considered implemented.

## Using AsterKV

The following examples demonstrate the intended CLI interface.

### Insert a record

```bash
asterkv ./mydata put name Ahmad
```

### Retrieve a record

```bash
asterkv ./mydata get name
```

Expected value:

```text
Ahmad
```

### Display database statistics

```bash
asterkv ./mydata stats
```

### Delete a record

```bash
asterkv ./mydata delete name
```

On Windows, the executable may be located under:

```powershell
.\build\Release\asterkv.exe
```

Adjust the executable path if your build configuration places it elsewhere.

## Python Integration

AsterKV includes a Python interface that communicates with the native C++ library through `ctypes`.

Example of the intended Python usage:

```python
from asterkv import Database

db = Database("./mydata")

db.put("name", "Ahmad")

value = db.get("name")

print(value)

db.close()
```

The exact imports and methods must match the Python API implemented in the repository.

## Performance and Benchmarking

Database performance depends on multiple factors:

- Read throughput
- Write throughput
- Random versus sequential access
- Key and value sizes
- Disk latency
- Memory consumption
- Recovery time
- Batch size
- Checkpoint frequency

The benchmark directory provides a foundation for evaluating storage operations.

Future benchmarks should compare AsterKV with established engines under identical workloads and hardware conditions.

No performance advantage over LevelDB or other established databases is claimed without reproducible measurements.

## Current Limitations

AsterKV is an early-stage implementation.

The current architecture does not yet provide all the capabilities of a mature LSM-tree database.

The following features remain outside the current implementation:

- Full multi-level LSM-tree architecture
- Sorted String Table (SSTable) block format
- Background compaction
- Bloom filters
- Advanced block caching
- Multi-process database coordination
- Production-grade concurrent write scheduling
- Extensive fault-injection testing
- Production-scale performance evaluation

These limitations are important when evaluating the project's maturity and suitability for real applications.

## Roadmap

### Version 0.1.0: Foundation

- [x] C++20-based project
- [x] Persistent key-value storage
- [x] Write-ahead logging
- [x] Ordered in-memory index
- [x] Basic recovery
- [x] Python integration
- [x] Command-line interface
- [x] Initial tests

### Version 0.2.0: Storage Architecture

- [ ] SSTable file format
- [ ] Sorted block storage
- [ ] Index blocks
- [ ] Multi-level storage
- [ ] Bloom filters

### Version 0.3.0: Performance

- [ ] Background compaction
- [ ] Block cache
- [ ] Improved write batching
- [ ] Memory optimization
- [ ] Read and write benchmarks

### Version 0.4.0: Concurrency

- [ ] Concurrent readers
- [ ] Improved write scheduling
- [ ] Thread-safety improvements
- [ ] Concurrency stress tests

### Version 1.0.0: Research and Stability

- [ ] Comprehensive crash testing
- [ ] Fuzz testing
- [ ] Benchmark comparisons
- [ ] Performance documentation
- [ ] API stability
- [ ] Security review

## Research Directions

AsterKV can be extended into a broader research project in database systems.

Potential research questions include:

1. How can an embedded storage engine minimize write amplification?
2. Which data structures provide the best balance between memory consumption and lookup performance?
3. How can adaptive compaction strategies improve storage efficiency?
4. How can crash recovery be made faster without sacrificing correctness?
5. How do different caching policies affect read performance?
6. How can database systems use workload-aware optimization?
7. What trade-offs exist between write throughput, consistency and recovery overhead?

These questions provide opportunities for controlled experiments, performance evaluation and future academic research.

## Inspiration and References

AsterKV was inspired by studying established database systems and their engineering principles.

**Primary reference:**

- [Google LevelDB](https://github.com/google/leveldb)

Additional concepts worth studying:

- Log-Structured Merge Trees
- Write-Ahead Logging
- Sorted String Tables
- Database Recovery
- Transaction Processing
- Storage Indexing
- Database Benchmarking

AsterKV is an independent implementation. It does not contain a claim of affiliation with, endorsement by or equivalence to Google.

## Contributing

Contributions, bug reports and technical discussions are welcome.

Suggested contribution areas:

- Database architecture
- Storage algorithms
- C++ implementation
- Python integration
- Testing and debugging
- Performance benchmarking
- Technical documentation

For significant changes, please open an issue to discuss the proposed design before submitting a pull request.

## License

See the `LICENSE` file in this repository for the applicable license terms.

---

## Author

**AsterKV Project**

Independent Computer Science and Systems Programming Research Project.

**Areas of interest:**

- Computer Science
- Database Engineering
- Systems Programming
- C++ Development
- Storage Architecture
- Algorithms and Data Structures
- Performance Engineering

---

**AsterKV: Understanding how databases work by building one.**