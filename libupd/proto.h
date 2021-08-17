#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <hedley.h>
#include <msgpack.h>

#include <libupd.h>

#include "pathfind.h"
#include "str.h"


#define UPD_PROTO_PARSE_HOLD_MAX 4


typedef struct upd_proto_msg_t   upd_proto_msg_t;
typedef struct upd_proto_parse_t upd_proto_parse_t;


typedef enum upd_proto_iface_t {
  UPD_PROTO_ENCODER = 0x0001,
  UPD_PROTO_OBJECT  = 0x0002,
} upd_proto_iface_t;

typedef enum upd_proto_cmd_t {
  UPD_PROTO_ENCODER_INFO,
  UPD_PROTO_ENCODER_INIT,
  UPD_PROTO_ENCODER_FRAME,
  UPD_PROTO_ENCODER_FINALIZE,

  UPD_PROTO_OBJECT_LOCK,
  UPD_PROTO_OBJECT_LOCKEX,
  UPD_PROTO_OBJECT_UNLOCK,
  UPD_PROTO_OBJECT_GET,
  UPD_PROTO_OBJECT_SET,
} upd_proto_cmd_t;

/* THIS OBJECT DOESN'T HOLD FILE REFCNT */
struct upd_proto_msg_t {
  upd_proto_iface_t         iface;
  upd_proto_cmd_t           cmd;
  const msgpack_object_map* param;

  union {
    struct {
      upd_file_t* file;
    } encoder_frame;
    struct {
      const msgpack_object_array* path;
      const msgpack_object*       value;
    } object;
  };
};


struct upd_proto_parse_t {
  upd_iso_t*            iso;
  const msgpack_object* src;
  upd_proto_iface_t     iface;

  size_t          refcnt;
  upd_proto_msg_t msg;

  const char* err;

  upd_file_t* hold[UPD_PROTO_PARSE_HOLD_MAX];

  void* udata;
  void
  (*cb)(
    upd_proto_parse_t* par);
};


HEDLEY_NON_NULL(1)
static inline
void
upd_proto_parse(
  upd_proto_parse_t* par);

HEDLEY_NON_NULL(1)
static inline
bool
upd_proto_parse_with_dup(
  const upd_proto_parse_t* src);


static inline
void
upd_proto_parse_hold_(
  upd_proto_parse_t* par,
  upd_file_t*        f);

static inline
void
upd_proto_parse_unref_(
  upd_proto_parse_t* par);


static inline
void
upd_proto_parse_encoder_(
  upd_proto_parse_t* par);

static inline
void
upd_proto_parse_object_(
  upd_proto_parse_t* par);


static inline void upd_proto_parse(upd_proto_parse_t* par) {
  ++par->refcnt;

  upd_proto_msg_t* msg = &par->msg;

  if (HEDLEY_UNLIKELY(par->src->type != MSGPACK_OBJECT_MAP)) {
    par->err = "root must be a map";
    goto EXIT;
  }
  const msgpack_object_map* root = &par->src->via.map;

  const msgpack_object_str* iface = NULL;
  const msgpack_object_str* cmd   = NULL;
  const msgpack_object_map* param = NULL;
  const char* invalid = upd_msgpack_find_fields(root, (upd_msgpack_field_t[]) {
      { .name = "interface", .required = true,  .str = &iface, },
      { .name = "command",   .required = true,  .str = &cmd,   },
      { .name = "param",     .required = false, .map = &param, },
      { NULL, },
    });
  if (HEDLEY_UNLIKELY(invalid)) {
    par->err = "invalid msg";
    goto EXIT;
  }
  msg->param = param;

  if (HEDLEY_UNLIKELY(par->iface & UPD_PROTO_ENCODER)) {
    if (HEDLEY_UNLIKELY(upd_strcaseq_c("encoder", iface->ptr, iface->size))) {
      msg->iface = UPD_PROTO_ENCODER;

      const upd_str_switch_case_t* c =
        upd_str_switch((uint8_t*) cmd->ptr, cmd->size, (upd_str_switch_case_t[]) {
            { .str = "info",     .i = UPD_PROTO_ENCODER_INFO,     },
            { .str = "init",     .i = UPD_PROTO_ENCODER_INIT,     },
            { .str = "frame",    .i = UPD_PROTO_ENCODER_FRAME,    },
            { .str = "finalize", .i = UPD_PROTO_ENCODER_FINALIZE, },
            { NULL, },
          });
      if (HEDLEY_UNLIKELY(c == NULL)) {
        par->err = "unknown command";
        goto EXIT;
      }
      msg->cmd = c->i;

      upd_proto_parse_encoder_(par);
      goto EXIT;
    }
  }
  if (HEDLEY_UNLIKELY(par->iface & UPD_PROTO_OBJECT)) {
    if (HEDLEY_UNLIKELY(upd_strcaseq_c("object", iface->ptr, iface->size))) {
      msg->iface = UPD_PROTO_OBJECT;

      const upd_str_switch_case_t* c =
        upd_str_switch((uint8_t*) cmd->ptr, cmd->size, (upd_str_switch_case_t[]) {
            { .str = "lock",   .i = UPD_PROTO_OBJECT_LOCK,   },
            { .str = "lockex", .i = UPD_PROTO_OBJECT_LOCKEX, },
            { .str = "unlock", .i = UPD_PROTO_OBJECT_UNLOCK, },
            { .str = "get",    .i = UPD_PROTO_OBJECT_GET,    },
            { .str = "set",    .i = UPD_PROTO_OBJECT_SET,    },
            { NULL, },
          });
      if (HEDLEY_UNLIKELY(c == NULL)) {
        par->err = "unknown command";
        goto EXIT;
      }
      msg->cmd = c->i;

      upd_proto_parse_object_(par);
      goto EXIT;
    }
  }

  par->err = "unknown interface";

EXIT:
  upd_proto_parse_unref_(par);
}

static inline bool upd_proto_parse_with_dup(const upd_proto_parse_t* src) {
  upd_iso_t* iso = src->iso;

  upd_proto_parse_t* par = upd_iso_stack(iso, sizeof(*par));
  if (HEDLEY_UNLIKELY(par == NULL)) {
    return false;
  }
  *par = *src;

  upd_proto_parse(par);
  return true;
}


static inline void upd_proto_parse_hold_(
    upd_proto_parse_t* par, upd_file_t* f) {
  for (size_t i = 0; i < UPD_PROTO_PARSE_HOLD_MAX; ++i) {
    if (HEDLEY_LIKELY(par->hold[i] == NULL)) {
      par->hold[i] = f;
      upd_file_ref(f);
      return;
    }
  }
  assert(false);
  HEDLEY_UNREACHABLE();
}

static inline void upd_proto_parse_unref_(upd_proto_parse_t* par) {
  if (HEDLEY_LIKELY(--par->refcnt)) {
    return;
  }

  upd_file_t* hold[UPD_PROTO_PARSE_HOLD_MAX];
  memcpy(hold, par->hold, sizeof(hold));

  par->cb(par);  /* par is freed and not available anymore */

  for (size_t i = 0; i < UPD_PROTO_PARSE_HOLD_MAX; ++i) {
    if (HEDLEY_UNLIKELY(par->hold[i])) {
      upd_file_unref(par->hold[i]);
    }
  }
}


static inline void upd_proto_encoder_frame_file_pathfind_cb_(
    upd_pathfind_t* pf) {
  upd_proto_parse_t* par = pf->udata;
  upd_iso_t*         iso = pf->iso;
  upd_proto_msg_t*   msg = &par->msg;

  upd_file_t* target = pf->len? NULL: pf->base;
  upd_iso_unstack(iso, pf);

  if (HEDLEY_UNLIKELY(target == NULL)) {
    par->err = "file not found";
    goto EXIT;
  }
  msg->encoder_frame.file = target;
  upd_proto_parse_hold_(par, target);

EXIT:
  upd_proto_parse_unref_(par);
}
static inline void upd_proto_parse_encoder_(upd_proto_parse_t* par) {
  upd_iso_t*       iso = par->iso;
  upd_proto_msg_t* msg = &par->msg;

  switch (msg->cmd) {
  case UPD_PROTO_ENCODER_INFO:
    break;

  case UPD_PROTO_ENCODER_INIT:
    break;

  case UPD_PROTO_ENCODER_FRAME: {
    if (HEDLEY_UNLIKELY(msg->param == NULL)) {
      par->err = "invalid param";
      return;
    }

    uintmax_t                 file_i = 0;
    const msgpack_object_str* file_s = NULL;
    const char* invalid =
      upd_msgpack_find_fields(msg->param, (upd_msgpack_field_t[]) {
          { .name = "file", .ui = &file_i, .str = &file_s, .required = true, },
          { NULL, },
        });
    if (HEDLEY_UNLIKELY(invalid)) {
      par->err = "invalid param";
      return;
    }

    if (file_s) {
      ++par->refcnt;
      const bool ok = upd_pathfind_with_dup(&(upd_pathfind_t) {
          .iso   = iso,
          .path  = (uint8_t*) file_s->ptr,
          .len   = file_s->size,
          .udata = par,
          .cb    = upd_proto_encoder_frame_file_pathfind_cb_,
        });
      if (HEDLEY_UNLIKELY(!ok)) {
        --par->refcnt;
        par->err = "subreq failure";
        return;
      }
    } else {
      upd_file_t* target = upd_file_get(iso, file_i);
      if (HEDLEY_UNLIKELY(target == NULL)) {
        par->err = "file not found";
        return;
      }
      msg->encoder_frame.file = target;
      upd_proto_parse_hold_(par, target);
    }
  } break;

  case UPD_PROTO_ENCODER_FINALIZE:
    break;

  default:
    HEDLEY_UNREACHABLE();
    assert(false);
  }
}

static inline void upd_proto_parse_object_(upd_proto_parse_t* par) {
  upd_proto_msg_t* msg = &par->msg;

  switch (msg->cmd) {
  case UPD_PROTO_OBJECT_LOCK:
    return;
  case UPD_PROTO_OBJECT_LOCKEX:
    return;

  case UPD_PROTO_OBJECT_UNLOCK:
    return;

  case UPD_PROTO_OBJECT_GET:
  case UPD_PROTO_OBJECT_SET: {
    if (msg->param == NULL) {
      return;
    }
    const msgpack_object_array* path  = NULL;
    const msgpack_object*       value = NULL;
    const char* invalid =
      upd_msgpack_find_fields(msg->param, (upd_msgpack_field_t[]) {
          { .name = "path",  .array = &path,  },
          { .name = "value", .any   = &value, },
          { NULL, },
        });
    if (HEDLEY_UNLIKELY(invalid)) {
      par->err = "invalid param";
      return;
    }
    msg->object.path  = path;
    msg->object.value = value;
  } return;

  default:
    assert(false);
    HEDLEY_UNREACHABLE();
  }
}
