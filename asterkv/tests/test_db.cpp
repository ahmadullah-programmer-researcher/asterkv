#include "asterkv/db.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << " : " #condition "\n"; ++failures; \
} } while (false)

std::filesystem::path fresh_dir(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / ("asterkv_test_" + name);
    std::error_code ec; std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path);
    return path;
}

void test_put_get_delete_and_binary() {
    const auto dir = fresh_dir("basic");
    auto db = asterkv::Database::open(dir);
    db->put("alpha", "one");
    CHECK(db->get("alpha") == std::optional<std::string>("one"));
    db->put("alpha", "two");
    CHECK(db->get("alpha") == std::optional<std::string>("two"));
    db->erase("alpha");
    CHECK(!db->get("alpha"));
    const std::string binary_key("k\0x", 3);
    const std::string binary_value("v\0y\xff", 4);
    db->put(binary_key, binary_value);
    CHECK(db->get(binary_key) == std::optional<std::string>(binary_value));
    db->put("", "");
    CHECK(db->get("") == std::optional<std::string>(""));
}

void test_batch_and_sequence() {
    const auto dir = fresh_dir("batch");
    auto db = asterkv::Database::open(dir);
    db->write({asterkv::Operation::put("a", "1"), asterkv::Operation::put("b", "2"),
               asterkv::Operation::del("a"), asterkv::Operation::put("c", "3")});
    CHECK(!db->get("a"));
    CHECK(db->get("b") == std::optional<std::string>("2"));
    CHECK(db->get("c") == std::optional<std::string>("3"));
    CHECK(db->stats().last_sequence == 4);
}

void test_scan_order_and_bounds() {
    const auto dir = fresh_dir("scan");
    auto db = asterkv::Database::open(dir);
    db->put("c", "3"); db->put("a", "1"); db->put("b", "2"); db->put("d", "4");
    const auto rows = db->scan(std::string("b"), std::string("d"));
    CHECK(rows.size() == 2);
    CHECK(rows[0].key == "b" && rows[1].key == "c");
    CHECK(db->scan(std::nullopt, std::nullopt, 2).size() == 2);
}

void test_snapshot_is_stable() {
    const auto dir = fresh_dir("snapshot");
    auto db = asterkv::Database::open(dir);
    db->put("key", "before");
    const auto snapshot = db->snapshot();
    db->put("key", "after");
    CHECK(snapshot.get("key") == std::optional<std::string>("before"));
    CHECK(db->get("key") == std::optional<std::string>("after"));
}

void test_wal_recovery_and_checkpoint() {
    const auto dir = fresh_dir("recovery");
    {
        auto db = asterkv::Database::open(dir);
        db->put("a", "one");
        db->write({asterkv::Operation::put("b", "two"), asterkv::Operation::put("c", "three")});
    }
    {
        auto db = asterkv::Database::open(dir);
        CHECK(db->get("a") == std::optional<std::string>("one"));
        CHECK(db->get("c") == std::optional<std::string>("three"));
        db->flush();
        db->put("d", "four");
    }
    {
        auto db = asterkv::Database::open(dir);
        CHECK(db->get("a") == std::optional<std::string>("one"));
        CHECK(db->get("d") == std::optional<std::string>("four"));
        CHECK(db->stats().last_sequence == 4);
    }
}

void test_truncated_wal_tail_is_ignored() {
    const auto dir = fresh_dir("truncated");
    {
        auto db = asterkv::Database::open(dir);
        db->put("safe", "value");
    }
    {
        std::ofstream out(dir / "store.wal", std::ios::binary | std::ios::app);
        out.write("AKW", 3); // incomplete magic/header at tail
    }
    auto db = asterkv::Database::open(dir);
    CHECK(db->get("safe") == std::optional<std::string>("value"));
}

void test_stale_wal_after_checkpoint_is_skipped() {
    const auto dir = fresh_dir("stale_wal");
    const auto saved_wal = dir / "saved.wal";
    {
        auto db = asterkv::Database::open(dir);
        db->put("key", "checkpointed-value");
        std::filesystem::copy_file(dir / "store.wal", saved_wal);
        db->flush();
    }
    // Simulate a crash after checkpoint replacement but before WAL truncation.
    std::filesystem::copy_file(saved_wal, dir / "store.wal", std::filesystem::copy_options::overwrite_existing);
    auto db = asterkv::Database::open(dir);
    CHECK(db->get("key") == std::optional<std::string>("checkpointed-value"));
    CHECK(db->stats().last_sequence == 1);
}

void test_corrupt_complete_wal_record_is_rejected() {
    const auto dir = fresh_dir("corrupt_wal");
    {
        auto db = asterkv::Database::open(dir);
        db->put("key", "value");
    }
    auto wal = dir / "store.wal";
    auto bytes = std::filesystem::file_size(wal);
    CHECK(bytes > 12);
    {
        std::fstream file(wal, std::ios::binary | std::ios::in | std::ios::out);
        file.seekg(static_cast<std::streamoff>(bytes - 1));
        char last = 0;
        file.read(&last, 1);
        last ^= 0x5A;
        file.seekp(static_cast<std::streamoff>(bytes - 1));
        file.write(&last, 1);
    }
    bool rejected = false;
    try { (void)asterkv::Database::open(dir); }
    catch (const std::runtime_error&) { rejected = true; }
    CHECK(rejected);
}

void test_concurrent_writers() {
    const auto dir = fresh_dir("concurrent");
    auto db = asterkv::Database::open(dir);
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&, t] {
            for (int i = 0; i < 100; ++i) db->put("t" + std::to_string(t) + ":" + std::to_string(i), "v");
        });
    }
    for (auto& worker : workers) worker.join();
    CHECK(db->stats().key_count == 400);
    CHECK(db->stats().last_sequence == 400);
}
}

int main() {
    test_put_get_delete_and_binary();
    test_batch_and_sequence();
    test_scan_order_and_bounds();
    test_snapshot_is_stable();
    test_wal_recovery_and_checkpoint();
    test_truncated_wal_tail_is_ignored();
    test_stale_wal_after_checkpoint_is_skipped();
    test_corrupt_complete_wal_record_is_rejected();
    test_concurrent_writers();
    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "All AsterKV tests passed\n";
    return 0;
}
