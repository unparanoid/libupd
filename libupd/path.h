#pragma once

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <hedley.h>
#include <utf8.h>

#include "str.h"


#define UPD_PATH_MAX 512

#define UPD_PATH_NAME_VALID_CHARS  \
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  \
  "abcdefghijklmnopqrstuvwxyz"  \
  "0123456789"  \
  "-_."


static inline
size_t
upd_path_normalize(
  uint8_t* path,
  size_t   len);

static inline
bool
upd_path_validate_name(
  const uint8_t* name,
  size_t         len);

static inline
size_t
upd_path_drop_trailing_slash(
  const uint8_t* path,
  size_t         len);

static inline
size_t
upd_path_dirname(
  const uint8_t* path,
  size_t         len);

HEDLEY_NON_NULL(2)
static inline
const uint8_t*
upd_path_basename(
  const uint8_t* path,
  size_t*        len);


static inline size_t upd_path_normalize(uint8_t* path, size_t len) {
  if (HEDLEY_UNLIKELY(len == 0)) {
    return 0;
  }

  /* remove duplicated slashes */
  const uint8_t* in  = path;
  uint8_t*       out = path;

  *out = *(in++);
  for (size_t i = 1; i < len; ++i, ++in) {
    if (HEDLEY_LIKELY(*out != '/' || *in != '/')) {
      *(++out) = *in;
    }
  }
  len = out - path + 1;

  /* remove up term */
  uint8_t* tail = path + len;
  uint8_t* r    = path + upd_path_drop_trailing_slash(path, len);
  uint8_t* l    = r-1;

  /* try to remove '..' and '.' */
  size_t up = 0;
  for (; path <= l; --l) {
    if (HEDLEY_UNLIKELY(l == path || *(l-1) == '/')) {
      const size_t n = r - l;

      if (HEDLEY_LIKELY(n > 0)) {
        bool skip = false;
        if (HEDLEY_UNLIKELY(upd_streq_c("..", l, n))) {
          skip = true;
          ++up;
        } else if (upd_streq_c(".", l, n)) {
          skip = true;
        } else {
          if (up) skip = true, --up;
        }
        if (skip) {
          utf8ncpy(l, r+1, tail-r-1);
          tail -= r-l+1;
        }
      }
      r = l-1;
    }
  }
  len = tail-path;
  if (HEDLEY_UNLIKELY(len && path[0] == '/' && up > 0)) {
    return 0;
  }

  /* recover '..' */
  l = path + len - 1;
  r = l + up*3;
  if (up > 0) {
    for (; path <= l; --l, --r) {
      *r = *l;
    }
    for (size_t i = 0; i < up; ++i) {
      utf8ncpy(path+i*3, "../", 3);
    }
  }
  len += up*3;

  return len;
}

static inline bool upd_path_validate_name(const uint8_t* name, size_t len) {
  if (HEDLEY_UNLIKELY(len == 0)) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    if (HEDLEY_UNLIKELY(utf8chr(UPD_PATH_NAME_VALID_CHARS, name[i]) == NULL)) {
      return false;
    }
  }
  if (HEDLEY_UNLIKELY(upd_streq_c(".", name, len))) {
    return false;
  }
  if (HEDLEY_UNLIKELY(upd_streq_c("..", name, len))) {
    return false;
  }
  return true;
}

static inline size_t upd_path_drop_trailing_slash(
    const uint8_t* path, size_t len) {
  while (len && path[len-1] == '/') --len;
  return len;
}

static inline size_t upd_path_dirname(const uint8_t* path, size_t len) {
  len = upd_path_drop_trailing_slash(path, len);
  while (len && path[len-1] != '/') --len;
  return len;
}

static inline const uint8_t* upd_path_basename(
    const uint8_t* path, size_t* len) {
  const size_t offset = upd_path_dirname(path, *len);
  *len -= offset;
  return path + offset;
}
