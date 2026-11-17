#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

namespace asterkv {

struct Operation {
    enum class Type : std::uint8_t { Put = 1, Delete = 2 };
    Type type{Type::Put};
    std::string key;
    std::string value;

    static Operation put(std::string key, std::string value);
    static Operation del(std::string key);
};

struct Entry {
    std::string key;
    std::string value;
};

struct Stats {
    std::uint64_t last_sequence{0};
    std::uint64_t key_count{0};
    std::uint64_t wal_bytes{0};
    std::uint64_t checkpoint_bytes{0};
};

class Snapshot {
public:
    Snapshot() = default;
    Snapshot(std::uint64_t sequence, std::map<std::string, std::string> values);

    std::uint64_t sequence() const noexcept { return sequence_; }
    std::optional<std::string> get(const std::string& key) const;
    std::vector<Entry> scan(
        const std::optional<std::string>& start = std::nullopt,
        const std::optional<std::string>& end = std::nullopt,
        std::size_t limit = 0) const;

private:
    std::uint64_t sequence_{0};
    std::map<std::string, std::string> values_;
};

class Database {
public:
    static std::unique_ptr<Database> open(const std::filesystem::path& directory);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void put(const std::string& key, const std::string& value);
    void erase(const std::string& key);
    void write(const std::vector<Operation>& operations);

    std::optional<std::string> get(const std::string& key) const;
    std::vector<Entry> scan(
        const std::optional<std::string>& start = std::nullopt,
        const std::optional<std::string>& end = std::nullopt,
        std::size_t limit = 0) const;

    Snapshot snapshot() const;
    void flush();
    Stats stats() const;
    const std::filesystem::path& directory() const noexcept { return directory_; }

private:
    explicit Database(std::filesystem::path directory);
    void recover();
    void load_checkpoint();
    void replay_wal();
    void append_wal(const std::vector<Operation>& operations, std::uint64_t first_sequence);
    void write_checkpoint();

    std::filesystem::path directory_;
    std::filesystem::path wal_path_;
    std::filesystem::path checkpoint_path_;
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::string> values_;
    std::uint64_t last_sequence_{0};
    std::uint64_t wal_bytes_{0};
    std::uint64_t checkpoint_bytes_{0};
    bool write_failed_{false};
};

}  // namespace asterkv
