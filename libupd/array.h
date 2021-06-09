#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <hedley.h>

#include "memory.h"


typedef struct upd_array_t {
  size_t n;
  void** p;
} upd_array_t;

#define upd_array_of(T) upd_array_t


HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline bool upd_array_resize(upd_array_t* a, size_t n) {
  size_t i = a->n;
  if (HEDLEY_UNLIKELY(!upd_malloc(&a->p, n*sizeof(*a->p)))) {
    return false;
  }
  for (; i < n; ++i) {
    a->p[i] = NULL;
  }
  a->n = n;
  return true;
}

HEDLEY_NON_NULL(1)
static inline void upd_array_clear(upd_array_t* a) {
  const bool ret = upd_array_resize(a, 0);
  (void) ret;
}

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline bool upd_array_insert(upd_array_t* a, void* p, size_t i) {
  if (i > a->n) {
    i = a->n;
  }
  if (HEDLEY_UNLIKELY(!upd_array_resize(a, a->n+1))) {
    return false;
  }
  memmove(a->p+i+1, a->p+i, (a->n-i-1)*sizeof(*a->p));
  a->p[i] = p;
  return true;
}

HEDLEY_NON_NULL(1)
static inline void* upd_array_remove(upd_array_t* a, size_t i) {
  if (a->n == 0) {
    return NULL;
  }
  if (i >= a->n) {
    i = a->n-1;
  }

  void* ptr = a->p[i];
  memmove(a->p+i, a->p+i+1, (a->n-i-1)*sizeof(*a->p));

  const bool ret = upd_array_resize(a, a->n-1);
  (void) ret;

  return ptr;
}

HEDLEY_NON_NULL(1, 2)
HEDLEY_WARN_UNUSED_RESULT
static inline bool upd_array_find(const upd_array_t* a, size_t* i, void* p) {
  for (size_t j = 0; j < a->n; ++j) {
    if (HEDLEY_UNLIKELY(a->p[j] == p)) {
      *i = j;
      return true;
    }
  }
  return false;
}

HEDLEY_NON_NULL(1)
static inline bool upd_array_find_and_remove(upd_array_t* a, void* p) {
  size_t i;
  if (HEDLEY_UNLIKELY(!upd_array_find(a, &i, p))) {
    return NULL;
  }
  return upd_array_remove(a, i) == p;
}
