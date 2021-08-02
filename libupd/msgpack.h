#pragma once

#include <stdbool.h>

#include <msgpack.h>

#include <libupd.h>

#include "array.h"
#include "pathfind.h"


typedef struct upd_msgpack_t       upd_msgpack_t;
typedef struct upd_msgpack_recv_t  upd_msgpack_recv_t;

typedef struct upd_msgpack_field_t upd_msgpack_field_t;

struct upd_msgpack_t {
  upd_iso_t* iso;

  size_t backlog;
  size_t maxmem;

  size_t mem;

  msgpack_packer   pk;
  msgpack_unpacker upk;

  upd_array_of(msgpack_unpacked*) in;
  msgpack_sbuffer                 out;
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


/* Don't forget to fill with zero and set required parameters. */
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
  assert(mpk->iso);

  mpk->mem = 1024;
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

  if (HEDLEY_UNLIKELY(mpk->maxmem && mpk->mem+io->size > mpk->maxmem)) {
    return false;
  }

  if (HEDLEY_UNLIKELY(!msgpack_unpacker_reserve_buffer(&mpk->upk, io->size))) {
    return false;
  }
  mpk->mem += io->size;

  memcpy(msgpack_unpacker_buffer(&mpk->upk), io->buf, io->size);
  msgpack_unpacker_buffer_consumed(&mpk->upk, io->size);

  for (;;) {
    msgpack_unpacked* upkd = upd_iso_stack(mpk->iso, sizeof(*upkd));
    if (HEDLEY_UNLIKELY(upkd == NULL)) {
      return false;
    }
    msgpack_unpacked_init(upkd);

    size_t consumed = 0;
    const int ret =
      msgpack_unpacker_next_with_size(&mpk->upk, upkd, &consumed);
    mpk->mem -= consumed;

    switch (ret) {
    case MSGPACK_UNPACK_SUCCESS:
      if (HEDLEY_LIKELY(!mpk->backlog || mpk->in.n < mpk->backlog)) {
        if (HEDLEY_LIKELY(upd_array_insert(&mpk->in, upkd, SIZE_MAX))) {
          continue;
        }
      }
      msgpack_unpacked_destroy(upkd);
      upd_iso_unstack(iso, upkd);
      return false;

    case MSGPACK_UNPACK_CONTINUE:
    case MSGPACK_UNPACK_PARSE_ERROR:
      msgpack_unpacked_destroy(upkd);
      upd_iso_unstack(iso, upkd);
      return ret == MSGPACK_UNPACK_CONTINUE;
    }
  }
}

static inline msgpack_unpacked* upd_msgpack_pop(upd_msgpack_t* ctx) {
  return upd_array_remove(&ctx->in, 0);
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

static inline const char* upd_msgpack_find_fields(
    const msgpack_object_map* map, const upd_msgpack_field_t* f) {
  for (; f; ++f) {
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
