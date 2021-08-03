#pragma once

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <hedley.h>
#include <utf8.h>
#include <yaml.h>

#include "libupd/str.h"


typedef struct upd_yaml_field_t {
  const char* name;
  bool        required;

  bool*      b;
  uintmax_t* ui;
  intmax_t*  i;
  double*    f;

  const yaml_node_t** str;
  const yaml_node_t** seq;
  const yaml_node_t** map;
  const yaml_node_t** any;
} upd_yaml_field_t;


HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline
bool
upd_yaml_parse(
  yaml_document_t* doc,
  const uint8_t*   str,
  size_t           len);


HEDLEY_NON_NULL(1, 2)
static inline
const yaml_node_t*
upd_yaml_find_node_by_name(
  yaml_document_t*   doc,
  const yaml_node_t* map,
  const uint8_t*     name,
  size_t             len);


HEDLEY_NON_NULL(1, 2, 3)
static inline
const char*
upd_yaml_find_fields(
  yaml_document_t*        doc,
  const yaml_node_t*      node,
  const upd_yaml_field_t* fields);

HEDLEY_NON_NULL(1, 2)
static inline
const char*
upd_yaml_find_fields_from_root(
  yaml_document_t*        doc,
  const upd_yaml_field_t* fields);


static inline bool upd_yaml_parse(
    yaml_document_t* doc, const uint8_t* str, size_t len) {
  if (HEDLEY_UNLIKELY(len == 0)) {
    return false;
  }

  yaml_parser_t parser = {0};
  if (HEDLEY_UNLIKELY(!yaml_parser_initialize(&parser))) {
    return false;
  }
  yaml_parser_set_input_string(&parser, str, len);

  if (HEDLEY_UNLIKELY(!yaml_parser_load(&parser, doc))) {
    return false;
  }
  yaml_parser_delete(&parser);
  return true;
}


static inline const yaml_node_t* upd_yaml_find_node_by_name(
    yaml_document_t*   doc,
    const yaml_node_t* map,
    const uint8_t*     name,
    size_t             len) {
  if (HEDLEY_UNLIKELY(map->type != YAML_MAPPING_NODE)) {
    return NULL;
  }

  const yaml_node_pair_t* itr = map->data.mapping.pairs.start;
  const yaml_node_pair_t* end = map->data.mapping.pairs.top;
  for (; itr < end; ++itr) {
    const yaml_node_t* k = yaml_document_get_node(doc, itr->key);
    if (HEDLEY_UNLIKELY(k == NULL || k->type != YAML_SCALAR_NODE)) {
      continue;
    }
    const uint8_t* s = k->data.scalar.value;
    const size_t   n = k->data.scalar.length;
    if (HEDLEY_UNLIKELY(upd_streq(name, len, s, n))) {
      return yaml_document_get_node(doc, itr->value);
    }
  }
  return NULL;
}


static inline const char* upd_yaml_find_fields(
    yaml_document_t*        doc,
    const yaml_node_t*      node,
    const upd_yaml_field_t* f) {
  for (; f->name; ++f) {
    const yaml_node_t* item = upd_yaml_find_node_by_name(
      doc, node, (uint8_t*) f->name, utf8size_lazy(f->name));
    if (HEDLEY_UNLIKELY(item == NULL)) {
      if (HEDLEY_UNLIKELY(f->required)) {
        return f->name;
      }
      continue;
    }

    bool used = false;
    if (f->any) {
      *f->any = item;
      used    = true;
    }

    switch (item->type) {
    case YAML_SCALAR_NODE:
      if (f->str) {
        *f->str = item;
        used    = true;
      }
      const uint8_t* s = item->data.scalar.value;
      const size_t   n = item->data.scalar.length;

      if (f->b) {
        const bool y =
          upd_strcaseq_c("true", s, n) ||
          upd_strcaseq_c("yes",  s, n) ||
          upd_strcaseq_c("y",    s, n) ||
          upd_strcaseq_c("on",   s, n);
        const bool n =
          upd_strcaseq_c("false", s, n) ||
          upd_strcaseq_c("no",    s, n) ||
          upd_strcaseq_c("n",     s, n) ||
          upd_strcaseq_c("off",   s, n);
        if (y != n) {
          *f->b = y;
          used  = true;
        }
      }

      char temp[32];
      temp[0] = 0;
      if (f->ui || f->i || f->f) {
        if (n < sizeof(temp)) {
          utf8ncpy(temp, s, n);
          temp[n] = 0;
        }
      }

      char* end;
      if (temp[0] && f->ui) {
        const uintmax_t v = strtoumax(temp, &end, 0);
        if (!*end && v != UINTMAX_MAX) {
          *f->ui = v;
          used   = true;
        }
      }
      if (temp[0] && f->i) {
        const intmax_t v = strtoimax(temp, &end, 0);
        if (!*end && v != INTMAX_MIN && v != INTMAX_MAX) {
          *f->i = v;
          used  = true;
        }
      }
      if (temp[0] && f->f) {
        const double v = strtod(temp, &end);
        if (!*end && isfinite(v)) {
          *f->f = v;
          used  = true;
        }
      }
      break;

    case YAML_SEQUENCE_NODE:
      if (f->seq) {
        *f->seq = item;
        used    = true;
      }
      break;

    case YAML_MAPPING_NODE:
      if (f->map) {
        *f->map = item;
        used    = true;
      }
      break;

    default:
      break;
    }
    if (HEDLEY_UNLIKELY(!used)) {
      return f->name;
    }
  }
  return NULL;
}

static inline const char* upd_yaml_find_fields_from_root(
    yaml_document_t* doc, const upd_yaml_field_t* fields) {
  const yaml_node_t* root = yaml_document_get_root_node(doc);
  if (HEDLEY_UNLIKELY(root == NULL)) {
    return false;
  }
  return upd_yaml_find_fields(doc, root, fields);
}
