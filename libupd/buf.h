#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <hedley.h>
#include <utf8.h>

#include "memory.h"


typedef struct upd_buf_t {
  size_t max;

  size_t   size;
  uint8_t* ptr;
} upd_buf_t;


HEDLEY_NON_NULL(1)
static inline void upd_buf_clear(upd_buf_t* buf) {
  upd_free(&buf->ptr);
  buf->size = 0;
}

HEDLEY_NON_NULL(1)
static inline uint8_t* upd_buf_append(
    upd_buf_t* buf, const uint8_t* ptr, size_t size) {
  if (HEDLEY_UNLIKELY(buf->max && buf->max < buf->size+size)) {
    return NULL;
  }
  if (HEDLEY_UNLIKELY(!upd_malloc(&buf->ptr, buf->size+size))) {
    return NULL;
  }

  uint8_t* head = buf->ptr+buf->size;
  buf->size += size;
  if (ptr) {
    memcpy(head, ptr, size);
  }
  return head;
}

HEDLEY_NON_NULL(1, 2)
static inline const uint8_t* upd_buf_append_str(
    upd_buf_t* buf, const uint8_t* str) {
  return upd_buf_append(buf, str, utf8size_lazy(str));
}

HEDLEY_NON_NULL(1)
static inline void upd_buf_drop_tail(upd_buf_t* buf, size_t n) {
  if (n > buf->size) {
    n = buf->size;
  }
  buf->size -= n;

  const bool ok = upd_malloc(&buf->ptr, buf->size);
  (void) ok;
}

HEDLEY_NON_NULL(1)
static inline void upd_buf_drop_head(upd_buf_t* buf, size_t n) {
  if (n > buf->size) {
    n = buf->size;
  }
  memmove(buf->ptr, buf->ptr+n, buf->size-n);
  upd_buf_drop_tail(buf, n);
}
