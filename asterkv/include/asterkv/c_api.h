#ifndef ASTERKV_C_API_H
#define ASTERKV_C_API_H

#include <stddef.h>

#if defined(_WIN32)
  #if defined(ASTERKV_BUILDING_LIBRARY)
    #define ASTERKV_API __declspec(dllexport)
  #else
    #define ASTERKV_API __declspec(dllimport)
  #endif
#else
  #define ASTERKV_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct asterkv_db asterkv_db;

typedef enum asterkv_status {
    ASTERKV_OK = 0,
    ASTERKV_NOT_FOUND = 1,
    ASTERKV_ERROR = 2
} asterkv_status;

ASTERKV_API asterkv_status asterkv_open(const char* path, asterkv_db** out_db);
ASTERKV_API asterkv_status asterkv_put(asterkv_db* db, const void* key, size_t key_len,
                                      const void* value, size_t value_len);
ASTERKV_API asterkv_status asterkv_get(asterkv_db* db, const void* key, size_t key_len,
                                      void** out_value, size_t* out_value_len);
ASTERKV_API asterkv_status asterkv_delete(asterkv_db* db, const void* key, size_t key_len);
ASTERKV_API asterkv_status asterkv_flush(asterkv_db* db);
ASTERKV_API void asterkv_free(void* memory);
ASTERKV_API void asterkv_close(asterkv_db* db);
ASTERKV_API const char* asterkv_last_error(void);

#ifdef __cplusplus
}
#endif
#endif
