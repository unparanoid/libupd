#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <hedley.h>
#include <msgpack.h>

#include <libupd.h>

#include "str.h"


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


#define upd_proto_parse_unref_(par) do {  \
    if (HEDLEY_UNLIKELY(--par->refcnt == 0)) {  \
      par->cb(par);  \
    }  \
  } while (0)

#include "proto/encoder.h"
#include "proto/object.h"


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

      upd_proto_parse_encoder(par);
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

      upd_proto_parse_object(par);
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


#undef upd_proto_parse_unref_
