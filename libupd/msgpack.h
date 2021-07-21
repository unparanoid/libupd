#pragma once

#include <stdbool.h>

#include <msgpack.h>

#include <libupd.h>

#include "array.h"
#include "pathfind.h"


typedef struct upd_msgpack_t       upd_msgpack_t;
typedef struct upd_msgpack_field_t upd_msgpack_field_t;
typedef struct upd_msgpack_fetch_t upd_msgpack_fetch_t;

struct upd_msgpack_t {
  upd_iso_t* iso;

  msgpack_packer   pk;
  msgpack_unpacker upk;

  upd_array_of(msgpack_unpacked*) in;
  msgpack_sbuffer                 out;
};

struct upd_msgpack_field_t {
  const char* name;
  bool        any;

  const msgpack_object**       obj;
  const msgpack_object_map**   map;
  const msgpack_object_array** array;
  intmax_t*                    i;
  uintmax_t*                   ui;
  double*                      f;
  bool*                        b;
  const uint8_t**              str;
  size_t*                      len;

  /* don't use the followings in upd_msgpack_find_fields function */
  upd_file_t** file;

  const msgpack_object* obj_;
};

struct upd_msgpack_fetch_t {
  upd_iso_t*            iso;
  const msgpack_object* obj;

  /* must be alive until upd_msgpack_fetch_fields returned */
  upd_msgpack_field_t* fields;

  size_t refcnt;

  void* udata;
  void
  (*cb)(
    upd_msgpack_fetch_t* f);
};


HEDLEY_NON_NULL(1)
static inline
bool
upd_msgpack_init(
  upd_msgpack_t* mpk);

HEDLEY_NON_NULL(1)
static inline
void
upd_msgpack_deinit(
  upd_msgpack_t* mpk);

HEDLEY_NON_NULL(1, 2)
HEDLEY_WARN_UNUSED_RESULT
static inline
bool
upd_msgpack_unpack(
  upd_msgpack_t*             mpk,
  const upd_req_stream_io_t* io);

HEDLEY_NON_NULL(1)
static inline
msgpack_unpacked*
upd_msgpack_pop(
  upd_msgpack_t* mpk);


HEDLEY_NON_NULL(1, 2)
HEDLEY_WARN_UNUSED_RESULT
static inline
bool
upd_msgpack_pathfind(
  upd_pathfind_t*       pf,
  const msgpack_object* obj);

HEDLEY_NON_NULL(1, 2)
static inline void upd_msgpack_find_fields(
  const msgpack_object* obj,
  upd_msgpack_field_t*  f);

/* Don't forget unref fetched files! */
HEDLEY_NON_NULL(1)
static inline
bool
upd_msgpack_fetch_fields(
  upd_msgpack_fetch_t* f);

HEDLEY_NON_NULL(1)
static inline
bool
upd_msgpack_fetch_fields_with_dup(
  const upd_msgpack_fetch_t* src);


HEDLEY_NON_NULL(1, 2)
HEDLEY_WARN_UNUSED_RESULT
static inline
int
upd_msgpack_pack_cstr(
  msgpack_packer* pk,
  const char*     v);

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline
int
upd_msgpack_pack_bool(
  msgpack_packer* pk,
  bool            b);


static inline
void
upd_msgpack_fetch_fields_pathfind_cb_(
  upd_pathfind_t* pf);


static inline bool upd_msgpack_init(upd_msgpack_t* mpk) {
  if (HEDLEY_UNLIKELY(!msgpack_unpacker_init(&mpk->upk, 1024))) {
    return false;
  }
  msgpack_packer_init(&mpk->pk, &mpk->out, msgpack_sbuffer_write);
  msgpack_sbuffer_init(&mpk->out);
  return true;
}

static inline void upd_msgpack_deinit(upd_msgpack_t* mpk) {
  upd_iso_t* iso = mpk->iso;

  for (size_t i = 0; i < mpk->in.n; ++i) {
    msgpack_unpacked* upkd = mpk->in.p[i];
    msgpack_unpacked_destroy(upkd);
    upd_iso_unstack(iso, upkd);
  }
  upd_array_clear(&mpk->in);
  msgpack_sbuffer_destroy(&mpk->out);
  msgpack_unpacker_destroy(&mpk->upk);
}

static inline bool upd_msgpack_unpack(
    upd_msgpack_t* mpk, const upd_req_stream_io_t* io) {
  upd_iso_t* iso = mpk->iso;

  if (HEDLEY_UNLIKELY(!msgpack_unpacker_reserve_buffer(&mpk->upk, io->size))) {
    return false;
  }
  memcpy(msgpack_unpacker_buffer(&mpk->upk), io->buf, io->size);
  msgpack_unpacker_buffer_consumed(&mpk->upk, io->size);

  for (;;) {
    msgpack_unpacked* upkd = upd_iso_stack(mpk->iso, sizeof(*upkd));
    if (HEDLEY_UNLIKELY(upkd == NULL)) {
      return false;
    }
    msgpack_unpacked_init(upkd);

    const int ret = msgpack_unpacker_next(&mpk->upk, upkd);
    switch (ret) {
    case MSGPACK_UNPACK_SUCCESS:
      if (HEDLEY_UNLIKELY(!upd_array_insert(&mpk->in, upkd, SIZE_MAX))) {
        return false;
      }
      continue;
    case MSGPACK_UNPACK_CONTINUE:
    case MSGPACK_UNPACK_PARSE_ERROR:
      msgpack_unpacked_destroy(upkd);
      upd_iso_unstack(iso, upkd);
      return ret == MSGPACK_UNPACK_CONTINUE;
    }
  }
}

static inline msgpack_unpacked* upd_msgpack_pop(upd_msgpack_t* ctx) {
  return upd_array_remove(&ctx->in, SIZE_MAX);
}


static inline bool upd_msgpack_pathfind(
    upd_pathfind_t* pf, const msgpack_object* obj) {
  upd_iso_t* iso = pf->base? pf->base->iso: pf->iso;

  if (HEDLEY_UNLIKELY(obj == NULL)) {
    return false;
  }

  switch (obj->type) {
  case MSGPACK_OBJECT_POSITIVE_INTEGER: {
    pf->base = upd_file_get(iso, obj->via.u64);
    pf->len  = 0;
    pf->cb(pf);
  } return true;

  case MSGPACK_OBJECT_STR: {
    pf->path = (uint8_t*) obj->via.str.ptr;
    pf->len  = obj->via.str.size;
    upd_pathfind(pf);
  } return true;

  default:
    return false;
  }
}

static inline void upd_msgpack_find_fields(
    const msgpack_object* obj, upd_msgpack_field_t* f) {
  if (HEDLEY_UNLIKELY(obj == NULL || obj->type != MSGPACK_OBJECT_MAP)) {
    return;
  }
  const msgpack_object_map* map = &obj->via.map;
  while (f->name) {
    const size_t flen = utf8size_lazy(f->name);

    for (size_t i = 0; i < map->size; ++i) {
      const msgpack_object* k = &map->ptr[i].key;
      const msgpack_object* v = &map->ptr[i].val;
      if (HEDLEY_UNLIKELY(k->type != MSGPACK_OBJECT_STR)) {
        continue;
      }
      const char*  ks    = k->via.str.ptr;
      const size_t kslen = k->via.str.size;

      const bool match = flen == kslen && utf8ncmp(f->name, ks, kslen) == 0;
      if (HEDLEY_LIKELY(!match)) {
        continue;
      }
      bool used = false;
      switch (v->type) {
      case MSGPACK_OBJECT_POSITIVE_INTEGER:
        if (f->ui) {
          *f->ui = v->via.u64;
          used   = true;
        }
        if (v->via.u64 > INTMAX_MAX) {
          break;
        }
        HEDLEY_FALL_THROUGH;

      case MSGPACK_OBJECT_NEGATIVE_INTEGER:
        if (f->i) {
          *f->i = v->via.i64;
          used  = true;
        }
        break;

      case MSGPACK_OBJECT_FLOAT32:
      case MSGPACK_OBJECT_FLOAT64:
        if (f->f) {
          *f->f = v->via.f64;
          used  = true;
        }
        break;

      case MSGPACK_OBJECT_BOOLEAN:
        if (f->b) {
          *f->b = v->via.boolean;
          used  = true;
        }
        break;

      case MSGPACK_OBJECT_STR:
        if (f->str) {
          assert(f->len);
          *f->str = (uint8_t*) v->via.str.ptr;
          *f->len = v->via.str.size;
          used    = true;
        }
        break;

      case MSGPACK_OBJECT_MAP:
        if (f->map) {
          *f->map = &v->via.map;
          used    = true;
        }
        break;

      case MSGPACK_OBJECT_ARRAY:
        if (f->array) {
          *f->array = &v->via.array;
          used      = true;
        }
        break;

      default:
        break;
      }
      f->obj_ = v;
      if (used || f->any) {
        if (f->obj) *f->obj = f->obj_;
      }
    }
    ++f;
  }
}

static inline bool upd_msgpack_fetch_fields(upd_msgpack_fetch_t* f) {
  upd_iso_t* iso = f->iso;

  f->refcnt = 1;
  if (HEDLEY_LIKELY(f->obj)) {
    upd_msgpack_find_fields(f->obj, f->fields);
  }

  const upd_msgpack_field_t* fi = f->fields;
  while (fi->name) {
    if (fi->file) {
      if (HEDLEY_UNLIKELY(!fi->obj_)) {
        goto SKIP;
      }
      if (fi->obj) {
        *fi->obj = fi->obj_;
      }

      upd_pathfind_t* pf = upd_iso_stack(iso, sizeof(*pf)+sizeof(fi->file));
      if (HEDLEY_UNLIKELY(pf == NULL)) {
        return false;
      }
      *pf = (upd_pathfind_t) {
        .iso   = iso,
        .udata = f,
        .cb    = upd_msgpack_fetch_fields_pathfind_cb_,
      };
      *(upd_file_t***) (pf+1) = fi->file;

      ++f->refcnt;
      if (HEDLEY_UNLIKELY(!upd_msgpack_pathfind(pf, fi->obj_))) {
        --f->refcnt;
        goto SKIP;
      }
    }
SKIP:
    ++fi;
  }
  if (HEDLEY_UNLIKELY(--f->refcnt == 0)) {
    f->cb(f);
  }
  return true;
}

static inline bool upd_msgpack_fetch_fields_with_dup(
    const upd_msgpack_fetch_t* src) {
  upd_iso_t* iso = src->iso;

  upd_msgpack_fetch_t* f = upd_iso_stack(iso, sizeof(*f));
  if (HEDLEY_UNLIKELY(f == NULL)) {
    return false;
  }
  *f = *src;
  if (HEDLEY_UNLIKELY(!upd_msgpack_fetch_fields(f))) {
    upd_iso_unstack(iso, f);
    return false;
  }
  return true;
}


static inline int upd_msgpack_pack_cstr(msgpack_packer* pk, const char* v) {
  const size_t n = utf8size_lazy(v);
  return
    msgpack_pack_str(pk, n) ||
    msgpack_pack_str_body(pk, v, n);
}

static inline int upd_msgpack_pack_bool(msgpack_packer* pk, bool b) {
  return (b? msgpack_pack_true: msgpack_pack_false)(pk);
}


static inline void upd_msgpack_fetch_fields_pathfind_cb_(upd_pathfind_t* pf) {
  upd_file_t**         file = *(void**) (pf+1);
  upd_iso_t*           iso  = pf->iso;
  upd_msgpack_fetch_t* f    = pf->udata;

  *file = pf->len? NULL: pf->base;
  upd_iso_unstack(iso, pf);

  if (HEDLEY_LIKELY(*file)) {
    upd_file_ref(*file);
  }
  if (HEDLEY_UNLIKELY(--f->refcnt == 0)) {
    f->cb(f);
  }
}
