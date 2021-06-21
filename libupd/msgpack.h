#pragma once

#include <stdbool.h>

#include <msgpack.h>

#include <libupd.h>

#include "buf.h"
#include "pathfind.h"


typedef struct upd_msgpack_t       upd_msgpack_t;
typedef struct upd_msgpack_field_t upd_msgpack_field_t;
typedef struct upd_msgpack_fetch_t upd_msgpack_fetch_t;

struct upd_msgpack_t {
  msgpack_packer   pk;
  msgpack_unpacker upk;

  upd_buf_t outbuf;

  bool panic;

  unsigned in  : 1;
  unsigned out : 1;
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
  upd_msgpack_t* ctx);

HEDLEY_NON_NULL(1)
static inline
void
upd_msgpack_deinit(
  upd_msgpack_t* ctx);

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline
bool
upd_msgpack_handle(
  upd_msgpack_t* ctx,
  upd_req_t*     req);

HEDLEY_NON_NULL(1, 2)
static inline
const msgpack_object*
upd_msgpack_pop(
  upd_msgpack_t*    ctx,
  msgpack_unpacked* obj);


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
bool
upd_msgpack_pack_cstr(
  msgpack_packer* pk,
  const char*     v);

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline
bool
upd_msgpack_pack_bool(
  msgpack_packer* pk,
  bool            b);


static
int
upd_msgpack_write_cb_(
  void*       data,
  const char* buf,
  size_t      len);

static inline
void
upd_msgpack_fetch_fields_pathfind_cb_(
  upd_pathfind_t* pf);


static inline bool upd_msgpack_init(upd_msgpack_t* ctx) {
  if (HEDLEY_UNLIKELY(!msgpack_unpacker_init(&ctx->upk, 1024))) {
    return false;
  }
  msgpack_packer_init(&ctx->pk, ctx, upd_msgpack_write_cb_);
  return true;
}

static inline void upd_msgpack_deinit(upd_msgpack_t* ctx) {
  upd_buf_clear(&ctx->outbuf);
  msgpack_unpacker_destroy(&ctx->upk);
}

static inline bool upd_msgpack_handle(upd_msgpack_t* ctx, upd_req_t* req) {
  if (HEDLEY_UNLIKELY(ctx->panic)) {
    req->result = UPD_REQ_ABORTED;
    return false;
  }

  switch (req->type) {
  case UPD_REQ_DSTREAM_ACCESS:
    req->stream.access = (upd_req_stream_access_t) {
      .write = ctx->in,
      .read  = ctx->out,
    };
    req->result = UPD_REQ_OK;
    req->cb(req);
    return true;

  case UPD_REQ_DSTREAM_READ: {
    if (HEDLEY_UNLIKELY(req->stream.io.offset)) {
      req->result = UPD_REQ_INVALID;
      return false;
    }
    if (HEDLEY_UNLIKELY(!ctx->out && ctx->outbuf.size == 0)) {
      req->result = UPD_REQ_ABORTED;
      return false;
    }
    req->stream.io = (upd_req_stream_io_t) {
      .buf  = ctx->outbuf.ptr,
      .size = ctx->outbuf.size,
    };
    ctx->outbuf.size = 0;
    req->result = UPD_REQ_OK;
    req->cb(req);
  } return true;

  case UPD_REQ_DSTREAM_WRITE: {
    if (HEDLEY_UNLIKELY(req->stream.io.offset)) {
      req->result = UPD_REQ_INVALID;
      return false;
    }
    if (HEDLEY_UNLIKELY(!ctx->in)) {
      req->result = UPD_REQ_ABORTED;
      return false;
    }
    const upd_req_stream_io_t* io = &req->stream.io;

    const bool reserve = msgpack_unpacker_reserve_buffer(&ctx->upk, io->size);
    if (HEDLEY_UNLIKELY(!reserve)) {
      req->result = UPD_REQ_NOMEM;
      return false;
    }
    memcpy(msgpack_unpacker_buffer(&ctx->upk), io->buf, io->size);
    msgpack_unpacker_buffer_consumed(&ctx->upk, io->size);

    req->result = UPD_REQ_OK;
    req->cb(req);
  } return true;

  default:
    req->result = UPD_REQ_INVALID;
    return false;
  }
}

static inline const msgpack_object* upd_msgpack_pop(
    upd_msgpack_t* ctx, msgpack_unpacked* obj) {
  if (HEDLEY_UNLIKELY(ctx->panic)) {
    return NULL;
  }
  const int ret = msgpack_unpacker_next(&ctx->upk, obj);
  switch (ret) {
  case MSGPACK_UNPACK_SUCCESS:
    return &obj->data;
  case MSGPACK_UNPACK_CONTINUE:
    return NULL;
  case MSGPACK_UNPACK_PARSE_ERROR:
    ctx->panic = true;
    return NULL;
  }
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
  if (HEDLEY_UNLIKELY(obj->type != MSGPACK_OBJECT_MAP)) {
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


static inline bool upd_msgpack_pack_cstr(msgpack_packer* pk, const char* v) {
  const size_t n = utf8size_lazy(v);
  return
    !msgpack_pack_str(pk, n) &&
    !msgpack_pack_str_body(pk, v, n);
}

static inline bool upd_msgpack_pack_bool(msgpack_packer* pk, bool b) {
  return !((b? msgpack_pack_true: msgpack_pack_false)(pk));
}


static int upd_msgpack_write_cb_(void* data, const char* buf, size_t len) {
  upd_msgpack_t* ctx = data;
  if (HEDLEY_UNLIKELY(!upd_buf_append(&ctx->outbuf, (uint8_t*) buf, len))) {
    ctx->panic = true;
    return -1;
  }
  return 0;
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
