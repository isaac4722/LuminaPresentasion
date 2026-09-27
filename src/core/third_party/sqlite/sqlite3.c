// Placeholder de sqlite3.c. Reemplazar por la amalgama real antes de compilar.
// Mientras tanto, stub que produce una librería vacía pero compila.
#include "sqlite3.h"
#ifdef __cplusplus
extern "C" {
#endif

int  sqlite3_open(const char* f, struct sqlite3** db) { (void)f; *db = nullptr; return 0; }
int  sqlite3_close(struct sqlite3* db) { (void)db; return 0; }
int  sqlite3_exec(struct sqlite3* db, const char* s, sqlite3_callback cb, void* d, char** e) {
    (void)db; (void)s; (void)cb; (void)d; if (e) *e = nullptr; return 0;
}
void sqlite3_free(void* p) { (void)p; }
int  sqlite3_prepare_v2(struct sqlite3* db, const char* s, int n, struct sqlite3_stmt** st, const char** t) {
    (void)db; (void)s; (void)n; *st = nullptr; if (t) *t = nullptr; return 1;
}
int  sqlite3_step(struct sqlite3_stmt* st) { (void)st; return 101; }
int  sqlite3_finalize(struct sqlite3_stmt* st) { (void)st; return 0; }
int  sqlite3_bind_text(struct sqlite3_stmt* st, int i, const char* s, int n, void(*f)(void*)) {
    (void)st; (void)i; (void)s; (void)n; (void)f; return 0;
}
int  sqlite3_bind_int(struct sqlite3_stmt* st, int i, int v) { (void)st; (void)i; (void)v; return 0; }
int  sqlite3_bind_int64(struct sqlite3_stmt* st, int i, long long v) { (void)st; (void)i; (void)v; return 0; }
long long sqlite3_column_int64(struct sqlite3_stmt* st, int i) { (void)st; (void)i; return 0; }
int  sqlite3_column_int(struct sqlite3_stmt* st, int i) { (void)st; (void)i; return 0; }
const unsigned char* sqlite3_column_text(struct sqlite3_stmt* st, int i) { (void)st; (void)i; return nullptr; }

#ifdef __cplusplus
}
#endif
