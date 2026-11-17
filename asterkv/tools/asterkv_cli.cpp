#include "asterkv/db.hpp"

#include <iostream>
#include <string>

namespace {
void usage() {
    std::cerr << "AsterKV command line\n"
              << "Usage:\n"
              << "  asterkv <directory> put <key> <value>\n"
              << "  asterkv <directory> get <key>\n"
              << "  asterkv <directory> del <key>\n"
              << "  asterkv <directory> scan [start] [end]\n"
              << "  asterkv <directory> flush\n"
              << "  asterkv <directory> stats\n";
}
}

int main(int argc, char** argv) {
    if (argc < 3) { usage(); return 2; }
    try {
        auto db = asterkv::Database::open(argv[1]);
        const std::string command = argv[2];
        if (command == "put" && argc == 5) {
            db->put(argv[3], argv[4]);
            std::cout << "OK\n";
        } else if (command == "get" && argc == 4) {
            const auto value = db->get(argv[3]);
            if (!value) { std::cout << "NOT_FOUND\n"; return 1; }
            std::cout.write(value->data(), static_cast<std::streamsize>(value->size()));
            std::cout << '\n';
        } else if (command == "del" && argc == 4) {
            db->erase(argv[3]);
            std::cout << "OK\n";
        } else if (command == "scan" && argc >= 3 && argc <= 5) {
            const std::optional<std::string> start = argc >= 4 ? std::optional<std::string>(argv[3]) : std::nullopt;
            const std::optional<std::string> end = argc >= 5 ? std::optional<std::string>(argv[4]) : std::nullopt;
            for (const auto& entry : db->scan(start, end)) {
                std::cout << entry.key << "\t" << entry.value << '\n';
            }
        } else if (command == "flush" && argc == 3) {
            db->flush();
            std::cout << "FLUSHED\n";
        } else if (command == "stats" && argc == 3) {
            const auto stats = db->stats();
            std::cout << "sequence=" << stats.last_sequence << '\n'
                      << "keys=" << stats.key_count << '\n'
                      << "wal_bytes=" << stats.wal_bytes << '\n'
                      << "checkpoint_bytes=" << stats.checkpoint_bytes << '\n';
        } else {
            usage(); return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << "asterkv: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
