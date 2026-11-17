# Research and Design Notes

## Reference project

Google LevelDB is an embedded ordered key-value library. Its documented interface includes Put/Get/Delete, atomic batches, snapshots and forward/backward iteration. Its implementation uses log files, an in-memory table, sorted table files and compaction.

Primary reference material:

- https://github.com/google/leveldb
- https://github.com/google/leveldb/blob/main/doc/impl.md
- https://github.com/google/leveldb/blob/main/doc/log_format.md
- https://github.com/google/leveldb/blob/main/include/leveldb/db.h
- https://github.com/google/leveldb/blob/main/db/db_impl.cc
- https://github.com/google/leveldb/blob/main/db/write_batch.cc

## What AsterKV adapts conceptually

- Ordered key/value abstraction.
- Write-ahead logging and replay.
- Atomic batches.
- Snapshot semantics.
- Sorted scans.
- Checkpointing to reduce replay work.

## What AsterKV does differently

The implementation uses a simple framed WAL record with one CRC32, a `std::map` memtable and a single full-state checkpoint. It does not copy LevelDB source code or its log/SSTable implementation. It omits multi-level SSTables, block tables, bloom filters, compression, background compaction, custom comparators and multi-process coordination.

## Research questions for future versions

1. When does a skip list or B-tree memtable outperform `std::map` for mixed workloads?
2. How does batch size influence throughput and tail latency when each WAL record is synced?
3. What is the cost of full checkpointing compared with immutable memtables and sorted runs?
4. Can group commit improve write throughput without weakening durability guarantees?
5. Which WAL framing and checksum strategy provides a useful balance of parser simplicity and corruption detection?
6. How should snapshots be implemented with sequence-numbered versions rather than full copies?

## Suggested experiments

- Sequential and random insert workloads.
- Point reads with hot and uniform key distributions.
- Range scans at different range widths.
- Batch sizes of 1, 8, 32, 128 and 512 operations.
- Recovery time as WAL size increases.
- Checkpoint time as live dataset size increases.
- Compare `std::map` against a B-tree or skip-list memtable in a later branch.

Do not compare AsterKV throughput numbers to LevelDB numbers unless hardware, data size, key/value distributions, sync policy, cache state and benchmark code are controlled and reported.
