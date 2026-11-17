#include "asterkv/db.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
  #include <fcntl.h>
  #include <io.h>
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <unistd.h>
#endif

namespace asterkv {
namespace {
constexpr char kWalMagic[4] = {'A', 'K', 'W', '1'};
constexpr char kCheckpointMagic[4] = {'A', 'K', 'S', '1'};
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint32_t kMaximumRecordBytes = 256U * 1024U * 1024U;

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU));
}
void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU));
}
std::uint32_t read_u32(const std::vector<std::uint8_t>& in, std::size_t& pos) {
    if (in.size() - pos < 4) throw std::runtime_error("truncated 32-bit field");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(in[pos++]) << (8U * i);
    return value;
}
std::uint64_t read_u64(const std::vector<std::uint8_t>& in, std::size_t& pos) {
    if (in.size() - pos < 8) throw std::runtime_error("truncated 64-bit field");
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(in[pos++]) << (8U * i);
    return value;
}
void append_string(std::vector<std::uint8_t>& out, const std::string& value) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("key/value exceeds format limit");
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}
std::string read_string(const std::vector<std::uint8_t>& in, std::size_t& pos) {
    const auto length = read_u32(in, pos);
    if (length > in.size() - pos) throw std::runtime_error("truncated string field");
    std::string value(reinterpret_cast<const char*>(in.data() + pos), length);
    pos += length;
    return value;
}

void write_all(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, bool append) {
    std::ofstream stream(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    if (!stream) throw std::runtime_error("cannot open file for writing: " + path.string());
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    if (!stream) throw std::runtime_error("failed writing file: " + path.string());
    stream.close();
    if (!stream) throw std::runtime_error("failed closing file: " + path.string());
}

void sync_file(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open file for durability sync: " + path.string());
    const BOOL ok = FlushFileBuffers(handle);
    CloseHandle(handle);
    if (!ok) throw std::runtime_error("FlushFileBuffers failed: " + path.string());
#else
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("cannot open file for fsync: " + path.string() + ": " + std::strerror(errno));
    const int result = ::fsync(fd);
    const int saved_errno = errno;
    ::close(fd);
    if (result != 0) throw std::runtime_error("fsync failed: " + path.string() + ": " + std::strerror(saved_errno));
#endif
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        if (!std::filesystem::exists(path)) return {};
        throw std::runtime_error("cannot open file for reading: " + path.string());
    }
    stream.seekg(0, std::ios::end);
    const auto end = stream.tellg();
    if (end < 0) throw std::runtime_error("cannot determine file size: " + path.string());
    if (static_cast<std::uint64_t>(end) > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        throw std::runtime_error("file is too large for this process");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty()) throw std::runtime_error("failed reading file: " + path.string());
    return bytes;
}

std::vector<Entry> scan_map(const std::map<std::string, std::string>& values,
                            const std::optional<std::string>& start,
                            const std::optional<std::string>& end,
                            std::size_t limit) {
    std::vector<Entry> result;
    auto it = start ? values.lower_bound(*start) : values.begin();
    for (; it != values.end(); ++it) {
        if (end && !(it->first < *end)) break; // end is exclusive
        result.push_back({it->first, it->second});
        if (limit != 0 && result.size() >= limit) break;
    }
    return result;
}

std::vector<std::uint8_t> encode_batch(const std::vector<Operation>& operations, std::uint64_t first_sequence) {
    std::vector<std::uint8_t> payload;
    append_u64(payload, first_sequence);
    append_u32(payload, static_cast<std::uint32_t>(operations.size()));
    for (const auto& operation : operations) {
        payload.push_back(static_cast<std::uint8_t>(operation.type));
        append_string(payload, operation.key);
        if (operation.type == Operation::Type::Put) append_string(payload, operation.value);
    }
    return payload;
}

}  // namespace

Operation Operation::put(std::string key, std::string value) {
    return {Type::Put, std::move(key), std::move(value)};
}
Operation Operation::del(std::string key) {
    return {Type::Delete, std::move(key), {}};
}

Snapshot::Snapshot(std::uint64_t sequence, std::map<std::string, std::string> values)
    : sequence_(sequence), values_(std::move(values)) {}
std::optional<std::string> Snapshot::get(const std::string& key) const {
    const auto it = values_.find(key);
    if (it == values_.end()) return std::nullopt;
    return it->second;
}
std::vector<Entry> Snapshot::scan(const std::optional<std::string>& start,
                                  const std::optional<std::string>& end,
                                  std::size_t limit) const {
    return scan_map(values_, start, end, limit);
}

Database::Database(std::filesystem::path directory)
    : directory_(std::move(directory)), wal_path_(directory_ / "store.wal"),
      checkpoint_path_(directory_ / "store.akv") {}

std::unique_ptr<Database> Database::open(const std::filesystem::path& directory) {
    if (directory.empty()) throw std::invalid_argument("database directory cannot be empty");
    std::filesystem::create_directories(directory);
    auto database = std::unique_ptr<Database>(new Database(directory));
    database->recover();
    return database;
}

Database::~Database() = default;

void Database::recover() {
    load_checkpoint();
    replay_wal();
    if (!std::filesystem::exists(wal_path_)) write_all(wal_path_, {}, false);
    wal_bytes_ = std::filesystem::file_size(wal_path_);
}

void Database::load_checkpoint() {
    const auto bytes = read_file(checkpoint_path_);
    if (bytes.empty()) { checkpoint_bytes_ = 0; return; }
    if (bytes.size() < 4 + 4 + 8 + 8 + 4) throw std::runtime_error("checkpoint is too short");
    if (std::memcmp(bytes.data(), kCheckpointMagic, 4) != 0) throw std::runtime_error("invalid checkpoint magic");
    std::vector<std::uint8_t> content(bytes.begin(), bytes.end() - 4);
    std::size_t crc_pos = bytes.size() - 4;
    const std::uint32_t stored_crc = static_cast<std::uint32_t>(bytes[crc_pos]) |
        (static_cast<std::uint32_t>(bytes[crc_pos + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[crc_pos + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[crc_pos + 3]) << 24U);
    if (crc32(content.data(), content.size()) != stored_crc) throw std::runtime_error("checkpoint checksum mismatch");

    std::size_t pos = 4;
    const auto version = read_u32(content, pos);
    if (version != kFormatVersion) throw std::runtime_error("unsupported checkpoint format version");
    last_sequence_ = read_u64(content, pos);
    const auto count = read_u64(content, pos);
    if (count > 100000000ULL) throw std::runtime_error("checkpoint entry count exceeds safety limit");
    for (std::uint64_t i = 0; i < count; ++i) {
        auto key = read_string(content, pos);
        auto value = read_string(content, pos);
        if (!values_.emplace(std::move(key), std::move(value)).second)
            throw std::runtime_error("duplicate key in checkpoint");
    }
    if (pos != content.size()) throw std::runtime_error("unexpected trailing data in checkpoint");
    checkpoint_bytes_ = bytes.size();
}

void Database::replay_wal() {
    const auto bytes = read_file(wal_path_);
    std::size_t pos = 0;
    std::uint64_t previous_end = 0;
    while (pos < bytes.size()) {
        const std::size_t record_start = pos;
        if (bytes.size() - pos < 8) break; // incomplete tail header
        if (std::memcmp(bytes.data() + pos, kWalMagic, 4) != 0)
            throw std::runtime_error("WAL corruption: invalid record magic at offset " + std::to_string(pos));
        pos += 4;
        std::vector<std::uint8_t> header(bytes.begin() + static_cast<std::ptrdiff_t>(pos), bytes.begin() + static_cast<std::ptrdiff_t>(pos + 4));
        std::size_t hp = 0;
        const auto length = read_u32(header, hp);
        pos += 4;
        if (length > kMaximumRecordBytes) throw std::runtime_error("WAL record exceeds configured safety limit");
        if (bytes.size() - pos < static_cast<std::size_t>(length) + 4) { pos = record_start; break; }
        std::vector<std::uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                                          bytes.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
        std::vector<std::uint8_t> crc_input;
        crc_input.insert(crc_input.end(), header.begin(), header.end());
        crc_input.insert(crc_input.end(), payload.begin(), payload.end());
        std::vector<std::uint8_t> crc_bytes(bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                                            bytes.begin() + static_cast<std::ptrdiff_t>(pos + 4));
        std::size_t cp = 0;
        const auto stored_crc = read_u32(crc_bytes, cp);
        if (crc32(crc_input.data(), crc_input.size()) != stored_crc)
            throw std::runtime_error("WAL checksum mismatch at offset " + std::to_string(record_start));
        pos += 4;

        std::size_t p = 0;
        const auto first_sequence = read_u64(payload, p);
        const auto count = read_u32(payload, p);
        if (count == 0) throw std::runtime_error("WAL contains an empty batch");
        if (first_sequence == 0 || first_sequence > std::numeric_limits<std::uint64_t>::max() - count + 1)
            throw std::runtime_error("invalid WAL sequence number");
        const auto batch_end = first_sequence + count - 1;
        if (previous_end != 0) {
            if (previous_end == std::numeric_limits<std::uint64_t>::max() || first_sequence != previous_end + 1)
                throw std::runtime_error("WAL sequence gap or overlap detected");
        }
        if (previous_end == 0 && first_sequence > last_sequence_ && first_sequence - last_sequence_ > 1)
            throw std::runtime_error("WAL starts after an unexplained sequence gap");
        previous_end = batch_end;

        std::vector<Operation> operations;
        operations.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            if (p >= payload.size()) throw std::runtime_error("truncated WAL operation type");
            const auto type = static_cast<Operation::Type>(payload[p++]);
            auto key = read_string(payload, p);
            if (type == Operation::Type::Put) operations.push_back(Operation::put(std::move(key), read_string(payload, p)));
            else if (type == Operation::Type::Delete) operations.push_back(Operation::del(std::move(key)));
            else throw std::runtime_error("unknown WAL operation type");
        }
        if (p != payload.size()) throw std::runtime_error("unexpected trailing data in WAL batch");

        // A checkpoint may have been installed immediately before a crash that
        // prevented WAL truncation. Such records are already represented in it.
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto sequence = first_sequence + i;
            if (sequence <= last_sequence_) continue;
            if (operations[i].type == Operation::Type::Put) values_[operations[i].key] = operations[i].value;
            else values_.erase(operations[i].key);
            last_sequence_ = sequence;
        }
    }
    // A partial final record is ignored. Earlier complete records remain valid.
}

void Database::append_wal(const std::vector<Operation>& operations, std::uint64_t first_sequence) {
    auto payload = encode_batch(operations, first_sequence);
    if (payload.size() > kMaximumRecordBytes) throw std::runtime_error("batch exceeds maximum WAL record size");
    std::vector<std::uint8_t> record;
    record.insert(record.end(), std::begin(kWalMagic), std::end(kWalMagic));
    std::vector<std::uint8_t> header;
    append_u32(header, static_cast<std::uint32_t>(payload.size()));
    record.insert(record.end(), header.begin(), header.end());
    record.insert(record.end(), payload.begin(), payload.end());
    std::vector<std::uint8_t> crc_input;
    crc_input.insert(crc_input.end(), header.begin(), header.end());
    crc_input.insert(crc_input.end(), payload.begin(), payload.end());
    const auto checksum = crc32(crc_input.data(), crc_input.size());
    append_u32(record, checksum);
    write_all(wal_path_, record, true);
    sync_file(wal_path_);
    wal_bytes_ += record.size();
}

void Database::put(const std::string& key, const std::string& value) {
    write({Operation::put(key, value)});
}
void Database::erase(const std::string& key) {
    write({Operation::del(key)});
}
void Database::write(const std::vector<Operation>& operations) {
    if (operations.empty()) return;
    if (operations.size() > std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("too many operations in batch");
    for (const auto& operation : operations) {
        if (operation.type != Operation::Type::Put && operation.type != Operation::Type::Delete)
            throw std::invalid_argument("unknown operation type");
    }
    std::unique_lock lock(mutex_);
    if (last_sequence_ > std::numeric_limits<std::uint64_t>::max() - operations.size())
        throw std::overflow_error("database sequence number exhausted");
    const auto first_sequence = last_sequence_ + 1;
    if (write_failed_) throw std::runtime_error("database is write-disabled after an earlier WAL I/O failure; close and recover it");
    try {
        append_wal(operations, first_sequence); // durable before publishing to readers
    } catch (...) {
        // A failed append may have written a partial record or may have an
        // uncertain durability outcome. Do not append later records behind it.
        write_failed_ = true;
        throw;
    }
    for (const auto& operation : operations) {
        if (operation.type == Operation::Type::Put) values_[operation.key] = operation.value;
        else values_.erase(operation.key);
    }
    last_sequence_ += operations.size();
}

std::optional<std::string> Database::get(const std::string& key) const {
    std::shared_lock lock(mutex_);
    const auto it = values_.find(key);
    if (it == values_.end()) return std::nullopt;
    return it->second;
}
std::vector<Entry> Database::scan(const std::optional<std::string>& start,
                                  const std::optional<std::string>& end,
                                  std::size_t limit) const {
    std::shared_lock lock(mutex_);
    return scan_map(values_, start, end, limit);
}
Snapshot Database::snapshot() const {
    std::shared_lock lock(mutex_);
    return Snapshot(last_sequence_, values_);
}

void Database::write_checkpoint() {
    std::vector<std::uint8_t> bytes;
    bytes.insert(bytes.end(), std::begin(kCheckpointMagic), std::end(kCheckpointMagic));
    append_u32(bytes, kFormatVersion);
    append_u64(bytes, last_sequence_);
    append_u64(bytes, static_cast<std::uint64_t>(values_.size()));
    for (const auto& [key, value] : values_) {
        append_string(bytes, key);
        append_string(bytes, value);
    }
    append_u32(bytes, crc32(bytes.data(), bytes.size()));
    const auto temporary = checkpoint_path_.string() + ".tmp";
    write_all(temporary, bytes, false);
    sync_file(temporary);
    std::error_code ec;
#ifdef _WIN32
    std::filesystem::remove(checkpoint_path_, ec);
    ec.clear();
#endif
    std::filesystem::rename(temporary, checkpoint_path_, ec);
    if (ec) throw std::runtime_error("cannot atomically install checkpoint: " + ec.message());
    checkpoint_bytes_ = bytes.size();

    // The checkpoint is durable before the WAL is truncated. If a crash occurs
    // between these steps, recovery ignores already-checkpointed sequence numbers.
    write_all(wal_path_, {}, false);
    sync_file(wal_path_);
    wal_bytes_ = 0;
}

void Database::flush() {
    std::unique_lock lock(mutex_);
    if (write_failed_) throw std::runtime_error("cannot checkpoint after an earlier WAL I/O failure; close and recover the database");
    write_checkpoint();
}

Stats Database::stats() const {
    std::shared_lock lock(mutex_);
    return {last_sequence_, static_cast<std::uint64_t>(values_.size()), wal_bytes_, checkpoint_bytes_};
}

}  // namespace asterkv
