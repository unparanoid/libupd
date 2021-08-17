#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <hedley.h>
#include <msgpack.h>
#include <utf8.h>

#include <libupd.h>


typedef struct upd_msgpack_t       upd_msgpack_t;
typedef struct upd_msgpack_recv_t  upd_msgpack_recv_t;

typedef struct upd_msgpack_field_t upd_msgpack_field_t;

struct upd_msgpack_t {
  size_t maxmem;
  size_t mem;

  msgpack_packer   pk;
  msgpack_unpacker upk;

  msgpack_sbuffer out;

  unsigned busy   : 1;
  unsigned broken : 1;

  void* udata;
  void
  (*cb)(
    upd_msgpack_t* mpk);
};

struct upd_msgpack_recv_t {
  upd_file_t*      file;
  upd_file_watch_t watch;
  upd_req_t        req;

  msgpack_unpacker upk;
  msgpack_unpacked upkd;

  msgpack_object* obj;

  unsigned first   : 1;
  unsigned ok      : 1;
  unsigned busy    : 1;
  unsigned reading : 1;
  unsigned pending : 1;

  void* udata;
  void
  (*cb)(
    upd_msgpack_recv_t* recv);
};


struct upd_msgpack_field_t {
  const char* name;
  bool        required;

  const msgpack_object**       obj;
  const msgpack_object**       any;
  const msgpack_object_map**   map;
  const msgpack_object_array** array;
  intmax_t*                    i;
  uintmax_t*                   ui;
  double*                      f;
  bool*                        b;
  const msgpack_object_str**   str;
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
  upd_msgpack_t* mpk,
  const uint8_t* buf,
  size_t         len);

HEDLEY_NON_NULL(1)
static inline
bool
upd_msgpack_pop(
  upd_msgpack_t*    mpk,
  msgpack_unpacked* upkd);

HEDLEY_NON_NULL(1, 2)
static inline
bool
upd_msgpack_handle(
  upd_msgpack_t* mpk,
  upd_req_t*     req);


HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline
bool
upd_msgpack_recv_init(
  upd_msgpack_recv_t* recv);

HEDLEY_NON_NULL(1)
static inline
void
upd_msgpack_recv_deinit(
  upd_msgpack_recv_t* recv);

HEDLEY_NON_NULL(1)
static inline
void
upd_msgpack_recv_next(
  upd_msgpack_recv_t* recv);


HEDLEY_NON_NULL(1, 2)
static inline
const msgpack_object*
upd_msgpack_find_obj(
  const msgpack_object_map* map,
  const msgpack_object*     needle);

HEDLEY_NON_NULL(1, 2)
static inline
const msgpack_object*
upd_msgpack_find_obj_by_str(
  const msgpack_object_map* map,
  const uint8_t*            str,
  size_t                    len);

HEDLEY_NON_NULL(1, 2)
static inline
const msgpack_object*
upd_msgpack_find_obj_by_cstr(
  const msgpack_object_map* map,
  const char*               str);

HEDLEY_NON_NULL(1, 2)
HEDLEY_WARN_UNUSED_RESULT
static inline
const char*
upd_msgpack_find_fields(
  const msgpack_object_map*  map,
  const upd_msgpack_field_t* field);


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
upd_msgpack_recv_watch_cb_(
  upd_file_watch_t* w);

static inline
void
upd_msgpack_recv_read_cb_(
  upd_req_t* req);


static inline bool upd_msgpack_init(upd_msgpack_t* mpk) {
  *mpk = (upd_msgpack_t) {
    .mem    = 1024,
    .maxmem = SIZE_MAX,
  };

  if (HEDLEY_UNLIKELY(!msgpack_unpacker_init(&mpk->upk, 1024))) {
    return false;
  }
  msgpack_packer_init(&mpk->pk, &mpk->out, msgpack_sbuffer_write);
  msgpack_sbuffer_init(&mpk->out);
  return true;
}

static inline void upd_msgpack_deinit(upd_msgpack_t* mpk) {
  msgpack_sbuffer_destroy(&mpk->out);
  msgpack_unpacker_destroy(&mpk->upk);
}

static inline bool upd_msgpack_unpack(
    upd_msgpack_t* mpk, const uint8_t* buf, size_t len) {
  if (HEDLEY_UNLIKELY(mpk->mem+len > mpk->maxmem)) {
    return false;
  }

  if (HEDLEY_UNLIKELY(!msgpack_unpacker_reserve_buffer(&mpk->upk, len))) {
    return false;
  }
  mpk->mem += len;

  memcpy(msgpack_unpacker_buffer(&mpk->upk), buf, len);
  msgpack_unpacker_buffer_consumed(&mpk->upk, len);
  return true;
}

static inline bool upd_msgpack_pop(upd_msgpack_t* mpk, msgpack_unpacked* upkd) {
  size_t consumed = 0;
  const int ret = msgpack_unpacker_next_with_size(&mpk->upk, upkd, &consumed);
  mpk->mem -= consumed;

  switch (ret) {
  case MSGPACK_UNPACK_SUCCESS:
    return true;

  case MSGPACK_UNPACK_CONTINUE:
    return false;

  case MSGPACK_UNPACK_PARSE_ERROR:
    mpk->broken = true;
    return false;
  }
}

static inline bool upd_msgpack_handle(upd_msgpack_t* mpk, upd_req_t* req) {
  upd_req_stream_io_t* io = &req->stream.io;

  switch (req->type) {
  case UPD_REQ_DSTREAM_WRITE: {
    if (HEDLEY_UNLIKELY(!upd_msgpack_unpack(mpk, io->buf, io->size))) {
      req->result = UPD_REQ_NOMEM;
      return false;
    }
    if (HEDLEY_UNLIKELY(!mpk->busy)) {
      mpk->cb(mpk);
    }
    req->result = UPD_REQ_OK;
    req->cb(req);
  } return true;

  case UPD_REQ_DSTREAM_READ: {
    *io = (upd_req_stream_io_t) {
      .buf  = (uint8_t*) mpk->out.data,
      .size = mpk->out.size,
    };
    uint8_t* ptr = (void*) msgpack_sbuffer_release(&mpk->out);
    req->result = UPD_REQ_OK;
    req->cb(req);
    free(ptr);
  } return true;

  default:
    assert(false);
  }
}


static inline bool upd_msgpack_recv_init(upd_msgpack_recv_t* recv) {
  if (HEDLEY_UNLIKELY(!msgpack_unpacker_init(&recv->upk, 1024))) {
    return false;
  }

  recv->watch = (upd_file_watch_t) {
    .file  = recv->file,
    .udata = recv,
    .cb    = upd_msgpack_recv_watch_cb_,
  };
  if (HEDLEY_UNLIKELY(!upd_file_watch(&recv->watch))) {
    msgpack_unpacker_destroy(&recv->upk);
    return false;
  }

  msgpack_unpacked_init(&recv->upkd);
  upd_file_ref(recv->file);
  return true;
}

static inline void upd_msgpack_recv_deinit(upd_msgpack_recv_t* recv) {
  assert(!recv->reading);

  upd_file_unwatch(&recv->watch);
  upd_file_unref(recv->file);

  msgpack_unpacker_destroy(&recv->upk);
  msgpack_unpacked_destroy(&recv->upkd);
}

static inline void upd_msgpack_recv_next(upd_msgpack_recv_t* recv) {
  recv->req = (upd_req_t) {
    .file = recv->file,
    .type = UPD_REQ_DSTREAM_READ,
    .stream = { .io = {
      .size = SIZE_MAX,
    }, },
    .udata = recv,
    .cb    = upd_msgpack_recv_read_cb_,
  };
  recv->busy    = true;
  recv->reading = true;
  recv->pending = false;
  if (HEDLEY_UNLIKELY(!upd_req(&recv->req))) {
    recv->ok = false;
    recv->cb(recv);
  }
}


static inline const msgpack_object* upd_msgpack_find_obj(
    const msgpack_object_map* map, const msgpack_object* needle) {
  for (size_t i = 0; i < map->size; ++i) {
    const msgpack_object_kv* kv = &map->ptr[i];
    if (HEDLEY_UNLIKELY(kv->key.type != MSGPACK_OBJECT_STR)) {
      continue;
    }
    if (HEDLEY_UNLIKELY(msgpack_object_equal(kv->key, *needle))) {
      return &kv->val;
    }
  }
  return NULL;
}

static inline const msgpack_object* upd_msgpack_find_obj_by_str(
    const msgpack_object_map* map, const uint8_t* name, size_t len) {
  return upd_msgpack_find_obj(map, &(msgpack_object) {
      .type = MSGPACK_OBJECT_STR,
      .via  = { .str = {
        .ptr  = (char*) name,
        .size = len,
      }, },
    });
}

static inline const msgpack_object* upd_msgpack_find_obj_by_cstr(
    const msgpack_object_map* map, const char* str) {
  return upd_msgpack_find_obj_by_str(map, (uint8_t*) str, utf8size_lazy(str));
}

static inline const char* upd_msgpack_find_fields(
    const msgpack_object_map* map, const upd_msgpack_field_t* f) {
  for (; f->name; ++f) {
    const msgpack_object* v = upd_msgpack_find_obj_by_str(
      map, (uint8_t*) f->name, utf8size_lazy(f->name));
    if (HEDLEY_UNLIKELY(v == NULL)) {
      if (HEDLEY_UNLIKELY(f->required)) {
        return f->name;
      }
      continue;
    }

    bool used = false;
    if (f->any) {
      *f->any = v;
      used    = true;
    }
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
        *f->str = &v->via.str;
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
    if (HEDLEY_UNLIKELY(!used)) {
      return f->name;
    }
  }
  return NULL;
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


static inline void upd_msgpack_recv_watch_cb_(upd_file_watch_t* w) {
  upd_msgpack_recv_t* recv = w->udata;

  switch (w->event) {
  case UPD_FILE_UPDATE:
    if (HEDLEY_UNLIKELY(recv->busy)) {
      recv->pending = true;
    } else {
      upd_msgpack_recv_next(recv);
    }
    break;
  }
}

static inline void upd_msgpack_recv_read_cb_(upd_req_t* req) {
  upd_msgpack_recv_t* recv = req->udata;
  recv->reading = false;

  if (HEDLEY_UNLIKELY(req->result != UPD_REQ_OK)) {
    goto ABORT;
  }

  const upd_req_stream_io_t* io = &req->stream.io;
  if (HEDLEY_UNLIKELY(!msgpack_unpacker_reserve_buffer(&recv->upk, io->size))) {
    goto ABORT;
  }
  memcpy(msgpack_unpacker_buffer(&recv->upk), io->buf, io->size);
  msgpack_unpacker_buffer_consumed(&recv->upk, io->size);

  const int ret = msgpack_unpacker_next(&recv->upk, &recv->upkd);
  switch (ret) {
  case MSGPACK_UNPACK_SUCCESS:
    recv->ok  = true;
    recv->obj = &recv->upkd.data;
    recv->cb(recv);
    return;

  case MSGPACK_UNPACK_CONTINUE:
    if (recv->pending) {
      upd_msgpack_recv_next(recv);
    } else {
      recv->busy = false;
    }
    return;

  case MSGPACK_UNPACK_PARSE_ERROR:
    goto ABORT;
  }

ABORT:
  recv->ok = false;
  recv->cb(recv);
}
