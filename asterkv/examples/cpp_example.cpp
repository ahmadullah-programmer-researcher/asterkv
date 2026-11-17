#include "asterkv/db.hpp"

#include <iostream>

int main() {
    auto database = asterkv::Database::open("./example-data-cpp");
    database->put("course", "Computer Science");
    database->put("language", "C++20");

    if (const auto value = database->get("course")) {
        std::cout << "course=" << *value << '\n';
    }

    const auto stable_view = database->snapshot();
    database->put("course", "Theoretical Computer Science");
    std::cout << "snapshot-course=" << *stable_view.get("course") << '\n';
    std::cout << "current-course=" << *database->get("course") << '\n';

    database->flush();
    return 0;
}
