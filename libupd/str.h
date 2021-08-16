#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <utf8.h>


typedef struct upd_str_switch_case_t {
  const char* str;
  union {
    intmax_t i;
    void*    ptr;
    void   (*func)();
  };
} upd_str_switch_case_t;


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


static inline
const upd_str_switch_case_t*
upd_str_switch(
  const uint8_t*              str,
  size_t                      len,
  const upd_str_switch_case_t cases[]);

static inline
const upd_str_switch_case_t*
upd_strcase_switch(
  const uint8_t*              str,
  size_t                      len,
  const upd_str_switch_case_t cases[]);


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


static inline const upd_str_switch_case_t* upd_str_switch(
    const uint8_t* str, size_t len, const upd_str_switch_case_t cases[]) {
  while (cases->str) {
    if (HEDLEY_UNLIKELY(upd_streq_c(cases->str, str, len))) {
      return cases;
    }
    ++cases;
  }
  return NULL;
}

static inline const upd_str_switch_case_t* upd_strcase_switch(
    const uint8_t* str, size_t len, const upd_str_switch_case_t cases[]) {
  while (cases->str) {
    if (HEDLEY_UNLIKELY(upd_strcaseq_c(cases->str, str, len))) {
      return cases;
    }
    ++cases;
  }
  return NULL;
}
