#pragma once

#include "../msgpack.h"
#include "../pathfind.h"


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

EXIT:
  upd_proto_parse_unref_(par);
}


static inline void upd_proto_parse_encoder(upd_proto_parse_t* par) {
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
      msg->encoder_frame.file = upd_file_get(iso, file_i);
      if (HEDLEY_UNLIKELY(msg->encoder_frame.file == NULL)) {
        par->err = "file not found";
        return;
      }
    }
  } break;

  case UPD_PROTO_ENCODER_FINALIZE:
    break;

  default:
    HEDLEY_UNREACHABLE();
    assert(false);
  }
}
