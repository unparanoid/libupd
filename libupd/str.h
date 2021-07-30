#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <utf8.h>


static inline
bool
upd_streq(
  const void* v1,
  size_t      v1len,
  const void* v2,
  size_t      v2len);

static inline
bool
upd_streq_c(
  const void* v1,
  const void* v2,
  size_t      v2len);

static inline
bool
upd_strcaseq(
  const void* v1,
  size_t      v1len,
  const void* v2,
  size_t      v2len);

static inline
bool
upd_strcaseq_c(
  const void* v1,
  const void* v2,
  size_t      v2len);


static inline bool upd_streq(
    const void* v1, size_t v1len, const void* v2, size_t v2len) {
  return v1len == v2len && utf8ncmp(v1, v2, v1len) == 0;
}

static inline bool upd_streq_c(
    const void* v1, const void* v2, size_t v2len) {
  return upd_streq(v1, utf8size_lazy(v1), v2, v2len);
}

static inline bool upd_strcaseq(
    const void* v1, size_t v1len, const void* v2, size_t v2len) {
  return v1len == v2len && utf8ncasecmp(v1, v2, v1len) == 0;
}

static inline bool upd_strcaseq_c(
    const void* v1, const void* v2, size_t v2len) {
  return upd_strcaseq(v1, utf8size_lazy(v1), v2, v2len);
}
