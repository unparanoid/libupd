#pragma once

#include <msgpack.h>

#include <libupd.h>
#include <libupd/pathfind.h>

#include "buf.h"


typedef struct upd_msgpack_t       upd_msgpack_t;
typedef struct upd_msgpack_field_t upd_msgpack_field_t;

struct upd_msgpack_t {
  msgpack_packer   pk;
  msgpack_unpacker upk;
  msgpack_unpacked obj;

  upd_buf_t outbuf;

  bool panic;

  unsigned in  : 1;
  unsigned out : 1;
};

struct upd_msgpack_field_t {
  const char*         name;
  msgpack_object_type type;
  msgpack_object**    obj;
};


static
int
upd_msgpack_write_cb_(
  void*       data,
  const char* buf,
  size_t      len);


HEDLEY_NON_NULL(1)
static inline bool upd_msgpack_init(upd_msgpack_t* ctx) {
  if (HEDLEY_UNLIKELY(!msgpack_unpacker_init(&ctx->upk, 1024))) {
    return false;
  }
  msgpack_packer_init(&ctx->pk, ctx, upd_msgpack_write_cb_);
  msgpack_unpacked_init(&ctx->obj);
  return true;
}

HEDLEY_NON_NULL(1)
static inline void upd_msgpack_deinit(upd_msgpack_t* ctx) {
  upd_buf_clear(&ctx->outbuf);
  msgpack_unpacker_destroy(&ctx->upk);
  msgpack_unpacked_destroy(&ctx->obj);
}

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline bool upd_msgpack_handle(upd_msgpack_t* ctx, upd_req_t* req) {
  if (HEDLEY_UNLIKELY(ctx->panic)) {
    req->result = UPD_REQ_ABORTED;
    return false;
  }

  switch (req->type) {
  case UPD_REQ_STREAM_ACCESS:
    req->stream.access = (upd_req_stream_access_t) {
      .write = ctx->in,
      .read  = ctx->out,
    };
    req->result = UPD_REQ_OK;
    req->cb(req);
    return true;

  case UPD_REQ_STREAM_READ: {
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

  case UPD_REQ_STREAM_WRITE: {
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

static inline const msgpack_object* upd_msgpack_pop(upd_msgpack_t* ctx) {
  if (HEDLEY_UNLIKELY(ctx->panic)) {
    return NULL;
  }

  const int ret = msgpack_unpacker_next(&ctx->upk, &ctx->obj);
  switch (ret) {
  case MSGPACK_UNPACK_SUCCESS:
    return &ctx->obj.data;
  case MSGPACK_UNPACK_CONTINUE:
    return NULL;
  case MSGPACK_UNPACK_PARSE_ERROR:
    ctx->panic = true;
    return NULL;
  }
}


static inline void upd_msgpack_find_fields(
    const msgpack_object* obj, const upd_msgpack_field_t* f) {
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
      if (f->type != MSGPACK_OBJECT_NIL) {
        if (HEDLEY_UNLIKELY(v->type != f->type)) {
          continue;
        }
      }
      const bool match = flen == kslen && utf8ncmp(f->name, ks, kslen) == 0;
      if (HEDLEY_UNLIKELY(match)) {
        *f->obj = v;
      }
    }
    ++f;
  }
}


static inline bool upd_msgpack_pathfind_with_dup(
    const upd_pathfind_t* pf, const msgpack_object* obj) {
  upd_iso_t* iso = pf->base? pf->base->iso: pf->iso;

  if (HEDLEY_UNLIKELY(obj == NULL)) {
    return false;
  }

  switch (obj->type) {
  case MSGPACK_OBJECT_POSITIVE_INTEGER: {
    upd_pathfind_t* p = upd_iso_stack(iso, sizeof(*p));
    if (HEDLEY_UNLIKELY(p == NULL)) {
      return false;
    }
    *p = *pf;
    p->iso  = iso;
    p->base = upd_file_get(iso, obj->via.u64);
    p->len  = 0;
    p->cb(p);
  } return true;

  case MSGPACK_OBJECT_STR: {
    upd_pathfind_t* p = upd_iso_stack(iso, sizeof(*p)+obj->via.str.size);
    if (HEDLEY_UNLIKELY(p == NULL)) {
      return false;
    }
    *p = *pf;
    p->iso  = iso;
    p->len  = obj->via.str.size;
    p->path = utf8ncpy(pf+1, obj->via.str.ptr, p->len);
    upd_pathfind(p);
  } return true;

  default:
    return false;
  }
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
  if (HEDLEY_UNLIKELY(!upd_buf_append(&ctx->outbuf, buf, len))) {
    ctx->panic = true;
    return -1;
  }
  return 0;
}
