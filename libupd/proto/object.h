#pragma once

#include "../msgpack.h"
#include "../pathfind.h"


static inline void upd_proto_parse_object(upd_proto_parse_t* par) {
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
