# AsterKV Build and Validation Report

## Tested environment

- Compiler: GCC 14.2.0
- Build system: CMake 3.25+ compatible configuration
- Language standard: C++20
- Platform: Linux x86_64

## Validation performed

- Debug configuration and build: passed.
- Release configuration and build: passed.
- CTest functional suite: passed.
- AddressSanitizer + UndefinedBehaviorSanitizer build and tests: passed.
- CLI put/get/stats smoke test: passed.
- Python ctypes binary-safe put/get/flush/reopen smoke test: passed.
- 1,000-entry benchmark smoke run: completed.

The benchmark smoke run was only used to verify that the executable runs. Its timing is not a comparative performance claim and must not be compared with LevelDB or other databases.

## Test coverage

The C++ test executable covers:

1. Put, get, overwrite and delete.
2. Binary keys and values, including embedded NUL bytes.
3. Empty keys and values.
4. Atomic multi-operation batch application.
5. Sequence number progression.
6. Ordered range scans and exclusive end bounds.
7. Stable copy-based snapshots.
8. WAL recovery after reopening.
9. Checkpoint and post-checkpoint WAL recovery.
10. Ignoring an incomplete final WAL record.
11. Skipping stale WAL records already present in a checkpoint.
12. Rejecting a complete WAL record with a checksum mismatch.
13. Concurrent writers within one process.

## Not yet validated

- Native Windows/MSVC build in this environment.
- macOS build.
- Power-loss crash testing on physical storage devices.
- Multi-process access (not supported).
- Large-scale performance comparisons.
- Fuzzing and long-duration stress testing.

A successful local build and test suite does not establish production readiness.
