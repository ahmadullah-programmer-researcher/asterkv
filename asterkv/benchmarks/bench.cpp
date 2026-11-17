#include "asterkv/db.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>

int main(int argc, char** argv) {
    const std::size_t count = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 100000;
    if (count == 0) {
        std::cerr << "entry count must be greater than zero\n";
        return 2;
    }
    const auto path = std::filesystem::temp_directory_path() / "asterkv_benchmark_db";
    std::error_code ec; std::filesystem::remove_all(path, ec);
    auto db = asterkv::Database::open(path);
    const auto write_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < count; ++i) db->put("key-" + std::to_string(i), "value-" + std::to_string(i));
    const auto write_end = std::chrono::steady_clock::now();
    std::mt19937_64 rng(42);
    const auto read_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < count; ++i) {
        const auto index = rng() % count;
        (void)db->get("key-" + std::to_string(index));
    }
    const auto read_end = std::chrono::steady_clock::now();
    const auto write_s = std::chrono::duration<double>(write_end - write_start).count();
    const auto read_s = std::chrono::duration<double>(read_end - read_start).count();
    std::cout << "entries=" << count << '\n'
              << "write_ops_per_second=" << count / write_s << '\n'
              << "random_read_ops_per_second=" << count / read_s << '\n';
    db->flush();
}
