# Contributing

1. Open an issue describing the bug or proposed feature.
2. Add a focused test before changing implementation behaviour.
3. Keep binary-format changes versioned and documented in `docs/FORMAT.md`.
4. Run the full CMake build and CTest suite.
5. Prefer small changes with explicit failure handling.
6. Never silently ignore a complete WAL record with a checksum or format failure.

Suggested checks:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```
