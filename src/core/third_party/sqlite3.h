// Placeholder de sqlite3.h. Reemplazar por la amalgama real antes de compilar.
// Origen: https://sqlite.org/download.html (sqlite-amalgamation-XXXXXXX.zip)
#pragma once
#include <cstdint>
#include <cstddef>
#ifdef __cplusplus
extern "C" {
#endif

struct sqlite3;
struct sqlite3_stmt;
#define SQLITE_OK         0
#define SQLITE_ROW        100
#define SQLITE_DONE       101
#define SQLITE_TRANSIENT  ((void(*)(void*))-1)
#define SQLITE_STATIC     ((void(*)(void*))0)
typedef int (*sqlite3_callback)(void*,int,char**,char**);

__declspec(dllimport) int  sqlite3_open(const char*, struct sqlite3**);
__declspec(dllimport) int  sqlite3_close(struct sqlite3*);
__declspec(dllimport) int  sqlite3_exec(struct sqlite3*, const char*, sqlite3_callback, void*, char**);
__declspec(dllimport) void sqlite3_free(void*);
__declspec(dllimport) int  sqlite3_prepare_v2(struct sqlite3*, const char*, int, struct sqlite3_stmt**, const char**);
__declspec(dllimport) int  sqlite3_step(struct sqlite3_stmt*);
__declspec(dllimport) int  sqlite3_finalize(struct sqlite3_stmt*);
__declspec(dllimport) int  sqlite3_bind_text(struct sqlite3_stmt*, int, const char*, int, void(*)(void*));
__declspec(dllimport) int  sqlite3_bind_int(struct sqlite3_stmt*, int, int);
__declspec(dllimport) int  sqlite3_bind_int64(struct sqlite3_stmt*, int, long long);
__declspec(dllimport) long long sqlite3_column_int64(struct sqlite3_stmt*, int);
__declspec(dllimport) int  sqlite3_column_int(struct sqlite3_stmt*, int);
__declspec(dllimport) const unsigned char* sqlite3_column_text(struct sqlite3_stmt*, int);

#ifdef __cplusplus
}
#endif
