# On-Disk Format (Version 1)

All integer fields are little-endian. Keys and values are arbitrary byte strings. The current public C++ interface represents them as `std::string` and does not assume UTF-8.

## WAL record

```text
+----------+----------------+----------------------+----------------+
| magic 4B | payload_len 4B | payload (N bytes)    | CRC32 4B       |
+----------+----------------+----------------------+----------------+
```

Magic is `AKW1`. The CRC covers the 4-byte encoded length followed by the payload.

Payload:

```text
first_sequence : uint64
operation_count: uint32
operations:
  type          : uint8   (1 = PUT, 2 = DELETE)
  key_length    : uint32
  key           : bytes[key_length]
  value_length  : uint32  (PUT only)
  value         : bytes[value_length] (PUT only)
```

A batch is encoded as one record. The record-size safety limit is 256 MiB.

## Checkpoint

```text
magic           : 4 bytes = AKS1
format_version  : uint32 = 1
last_sequence   : uint64
entry_count     : uint64
entries:
  key_length    : uint32
  key           : bytes[key_length]
  value_length  : uint32
  value         : bytes[value_length]
crc32           : uint32
```

The checkpoint CRC covers every byte before the CRC field. Entries are serialized in bytewise key order because the in-memory structure is `std::map<std::string, ...>`.

## Compatibility

Version 1 is an initial research format. There is no migration tool yet. Do not modify database files manually. A checksum failure is reported as corruption.
