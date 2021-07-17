#pragma once

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <hedley.h>
#include <utf8.h>


#define UPD_PATH_MAX 512

#define UPD_PATH_NAME_VALID_CHARS  \
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  \
  "abcdefghijklmnopqrstuvwxyz"  \
  "0123456789"  \
  "-_."


static inline size_t upd_path_normalize(uint8_t* path, size_t len) {
  if (HEDLEY_UNLIKELY(len == 0)) {
    return 0;
  }

  const uint8_t* in  = path;
  uint8_t*       out = path;

  *out = *(in++);
  for (size_t i = 1; i < len; ++i, ++in) {
    if (HEDLEY_LIKELY(*out != '/' || *in != '/')) {
      *(++out) = *in;
    }
  }
  return out - path + 1;
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

static inline const uint8_t* upd_path_basename(const uint8_t* path, size_t* len) {
  const size_t offset = upd_path_dirname(path, *len);
  *len -= offset;
  return path + offset;
}
