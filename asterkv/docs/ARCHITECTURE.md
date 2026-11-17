# AsterKV Architecture

## 1. Goals

AsterKV is a small embedded ordered key-value engine designed to make storage-engine concepts concrete: durable logging, recovery, batching, ordered indexing and checkpointing.

## 2. Components

```text
                C++ API             C ABI             Python ctypes
                   |                  |                    |
                   +------------------+--------------------+
                                      |
                                Database facade
                                      |
                       shared_mutex + ordered std::map
                                      |
                    +-----------------+-----------------+
                    |                                   |
              WAL writer                          Checkpoint writer
                    |                                   |
              store.wal                            store.akv
                    |                                   |
               Recovery <-------------------------- startup
```

## 3. Write path

1. Validate a batch.
2. Acquire the exclusive database mutex.
3. Assign a contiguous sequence-number range.
4. Encode the entire batch as one WAL record.
5. Append the record and request a durability sync.
6. Apply the operations to the in-memory ordered map.
7. Advance the visible sequence number.

The WAL record contains the whole batch, so recovery can apply a complete record as one unit. A failed append is not published in memory.

## 4. Read path

A reader acquires a shared lock and looks up the key in the ordered map. Scans use `lower_bound(start)` and stop before the optional exclusive end key. A snapshot copies the map and sequence number while holding a shared lock, then remains stable even after later writes.

## 5. Recovery

Startup loads and validates the checkpoint first, then walks the WAL from offset zero. Each record is checked for magic, bounded length and CRC32. A complete, valid batch is decoded and replayed. A partial final header or payload is treated as an interrupted tail and ignored. A complete record with an invalid checksum or malformed contents is treated as corruption and causes open to fail rather than silently discarding data. Sequence numbers must be contiguous between WAL records; unexplained gaps are reported as corruption.

If a crash occurs after checkpoint installation but before WAL truncation, the old WAL can still be present. Sequence numbers let recovery skip operations already represented by the checkpoint.

## 6. Checkpoint protocol

While holding the exclusive mutex:

1. Serialize the current live key/value map and last sequence number.
2. Write `store.akv.tmp`.
3. Flush and sync the temporary file.
4. Rename the temporary file to `store.akv`.
5. Truncate and sync `store.wal`.

Installing the checkpoint before truncating the WAL avoids a window in which both the old checkpoint and the log are absent. Directory fsync and platform-specific rename durability semantics are not yet implemented; this remains a production-hardening task.

## 7. Complexity

Let N be the number of live keys and B the batch size.

- `get`: O(log N) map lookup.
- `put` / `delete`: O(log N) map update plus O(record size) WAL append and sync.
- `scan`: O(log N + K), where K is the number of returned entries.
- `snapshot`: O(N) copy time and memory.
- `flush`: O(total live key/value bytes) serialization and write.

This is intentionally not a high-throughput LSM implementation: the in-memory ordered map is the index, and checkpointing rewrites all live data.
