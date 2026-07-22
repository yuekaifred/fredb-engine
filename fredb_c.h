#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *fredb_handle;

fredb_handle fredb_open(const char *dir, size_t dir_len);
void fredb_close(fredb_handle h);

int fredb_put(fredb_handle h, const char *key, size_t klen, const char *val,
              size_t vlen);

// remember to free!
int fredb_get(fredb_handle h, const char *key, size_t klen, char **out_val,
              size_t *out_vlen);

int fredb_remove(fredb_handle h, const char *key, size_t klen);

int64_t fredb_total_size(fredb_handle h);

// remember to free!
int fredb_get_range(fredb_handle h, const char *start, size_t start_len,
                    const char *end, size_t end_len, char **out_buf,
                    size_t *out_len);

void fredb_free_buf(char *buf);

const char *fredb_last_error(void);

#ifdef __cplusplus
}
#endif
