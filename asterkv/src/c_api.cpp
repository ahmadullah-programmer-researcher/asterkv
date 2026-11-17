#include "asterkv/c_api.h"
#include "asterkv/db.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>

struct asterkv_db {
    std::unique_ptr<asterkv::Database> database;
};

namespace {
thread_local std::string last_error;
void set_error(const char* message) { last_error = message ? message : "unknown error"; }
void set_error(const std::exception& error) { last_error = error.what(); }

template <typename Function>
asterkv_status guarded(Function&& function) {
    try { function(); last_error.clear(); return ASTERKV_OK; }
    catch (const std::exception& error) { set_error(error); return ASTERKV_ERROR; }
    catch (...) { set_error("unknown C++ exception"); return ASTERKV_ERROR; }
}
}

extern "C" {
asterkv_status asterkv_open(const char* path, asterkv_db** out_db) {
    if (!path || !out_db) { set_error("path and out_db are required"); return ASTERKV_ERROR; }
    *out_db = nullptr;
    return guarded([&] {
        auto handle = std::make_unique<asterkv_db>();
        handle->database = asterkv::Database::open(path);
        *out_db = handle.release();
    });
}

asterkv_status asterkv_put(asterkv_db* db, const void* key, size_t key_len,
                           const void* value, size_t value_len) {
    if (!db || (!key && key_len) || (!value && value_len)) { set_error("invalid put arguments"); return ASTERKV_ERROR; }
    return guarded([&] {
        const auto* key_bytes = static_cast<const char*>(key);
        const auto* value_bytes = static_cast<const char*>(value);
        db->database->put(key_len ? std::string(key_bytes, key_len) : std::string{},
                          value_len ? std::string(value_bytes, value_len) : std::string{});
    });
}

asterkv_status asterkv_get(asterkv_db* db, const void* key, size_t key_len,
                           void** out_value, size_t* out_value_len) {
    if (!db || (!key && key_len) || !out_value || !out_value_len) { set_error("invalid get arguments"); return ASTERKV_ERROR; }
    *out_value = nullptr; *out_value_len = 0;
    try {
        const auto* key_bytes = static_cast<const char*>(key);
        const auto value = db->database->get(key_len ? std::string(key_bytes, key_len) : std::string{});
        if (!value) { last_error.clear(); return ASTERKV_NOT_FOUND; }
        void* memory = std::malloc(value->size() == 0 ? 1 : value->size());
        if (!memory) { set_error("allocation failed"); return ASTERKV_ERROR; }
        if (!value->empty()) std::memcpy(memory, value->data(), value->size());
        *out_value = memory; *out_value_len = value->size();
        last_error.clear();
        return ASTERKV_OK;
    } catch (const std::exception& error) { set_error(error); return ASTERKV_ERROR; }
    catch (...) { set_error("unknown C++ exception"); return ASTERKV_ERROR; }
}

asterkv_status asterkv_delete(asterkv_db* db, const void* key, size_t key_len) {
    if (!db || (!key && key_len)) { set_error("invalid delete arguments"); return ASTERKV_ERROR; }
    return guarded([&] {
        const auto* key_bytes = static_cast<const char*>(key);
        db->database->erase(key_len ? std::string(key_bytes, key_len) : std::string{});
    });
}

asterkv_status asterkv_flush(asterkv_db* db) {
    if (!db) { set_error("database handle is null"); return ASTERKV_ERROR; }
    return guarded([&] { db->database->flush(); });
}

void asterkv_free(void* memory) { std::free(memory); }
void asterkv_close(asterkv_db* db) { delete db; }
const char* asterkv_last_error(void) { return last_error.c_str(); }
}
