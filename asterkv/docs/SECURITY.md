# Security and Reliability Notes

AsterKV is a learning and research prototype. It is not suitable as the sole storage layer for critical or regulated workloads.

## Implemented protections

- Length fields are checked before allocation or copying.
- WAL records and checkpoints are checksummed.
- Unknown operation types and malformed payloads are rejected.
- A partial tail record is ignored during recovery.
- A complete record with a checksum mismatch fails database opening.
- WAL append is synced before the in-memory state is published.
- After an append or WAL sync failure, the live handle disables further writes and checkpointing to avoid appending valid records behind an uncertain partial tail.
- Checkpoint is written and synced before the WAL is truncated.
- C API exceptions are caught and converted to status codes.

## Known limitations

- CRC32 detects accidental corruption; it is not a cryptographic authenticity mechanism.
- No encryption at rest or access-control layer is provided.
- There is no inter-process file lock. Multiple processes must not open the same directory concurrently.
- Directory metadata is not explicitly fsynced after rename.
- Windows checkpoint replacement has platform-specific durability considerations.
- No power-loss testing or systematic crash injection has been performed.
- A process crash between `ofstream::flush` and the underlying sync behaviour depends on the platform and filesystem.
- The engine trusts the local filesystem to honour its documented write and rename semantics.

## Production hardening backlog

1. Inter-process database lock.
2. Directory fsync and platform-specific durable atomic replacement.
3. Crash-injection tests at every write/checkpoint boundary.
4. WAL rotation and bounded recovery time.
5. SSTable blocks, indexes, bloom filters and background compaction.
6. Configurable checksummed framing and stronger corruption diagnostics.
7. Fuzzing for all binary parsers.
8. Sanitizer and ThreadSanitizer CI jobs.
9. Key/value size limits configurable at API boundaries.
10. Encryption and secure deletion policy where required.
