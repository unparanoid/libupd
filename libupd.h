#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <hedley.h>


#define UPD_VER_MAJOR UINT16_C(0)
#define UPD_VER_MINOR UINT16_C(10)

#define UPD_VER  \
  ((UPD_VER_MAJOR) << 16 | UPD_VER_MINOR)


#if defined(UPD_DECL_FUNC)
#  if defined(UPD_EXTERNAL_DRIVER)
#    error "both UPD_EXTERNAL_DRIVER and UPD_DECL_FUNC is defined"
#  endif
#else
#  if defined(UPD_EXTERNAL_DRIVER)
#    define UPD_DECL_FUNC static inline
#  else
#    define UPD_DECL_FUNC
#  endif
#endif


typedef struct upd_iso_t        upd_iso_t;
typedef struct upd_file_t       upd_file_t;
typedef struct upd_file_watch_t upd_file_watch_t;
typedef struct upd_file_lock_t  upd_file_lock_t;
typedef struct upd_driver_t     upd_driver_t;
typedef struct upd_req_t        upd_req_t;
typedef struct upd_mutex_t      upd_mutex_t;

typedef uint32_t upd_ver_t;
typedef int32_t  upd_iso_status_t;
typedef uint64_t upd_file_id_t;
typedef uint8_t  upd_file_event_t;
typedef uint16_t upd_req_cat_t;
typedef uint32_t upd_req_type_t;
typedef uint8_t  upd_req_result_t;
typedef uint8_t  upd_tensor_type_t;

typedef
void
(*upd_iso_thread_main_t)(
  void* udata);

typedef
void
(*upd_iso_work_cb_t)(
  upd_iso_t* iso, void* udata);


/*
 * ---- ISOLATED MACHINE INTERFACE ----
 */
enum {
  /* upd_iso_status_t */
  UPD_ISO_PANIC    = -1,
  UPD_ISO_RUNNING  =  0,
  UPD_ISO_SHUTDOWN =  1,
  UPD_ISO_REBOOT   =  2,
};

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
UPD_DECL_FUNC
void*
upd_iso_stack(
  upd_iso_t* iso,
  uint64_t   len);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
void
upd_iso_unstack(
  upd_iso_t* iso,
  void*      ptr);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
uint64_t
upd_iso_now(
  upd_iso_t* iso);

HEDLEY_NON_NULL(1, 2)
UPD_DECL_FUNC
void
upd_iso_msg(
  upd_iso_t*     iso,
  const uint8_t* msg,
  uint64_t       len);

HEDLEY_NON_NULL(1, 2)
HEDLEY_WARN_UNUSED_RESULT
UPD_DECL_FUNC
bool
upd_iso_start_thread(
  upd_iso_t*            iso,
  upd_iso_thread_main_t main,
  void*                 udata);

HEDLEY_NON_NULL(1, 2, 3)
UPD_DECL_FUNC
bool
upd_iso_start_work(
  upd_iso_t*            iso,
  upd_iso_thread_main_t main,
  upd_iso_work_cb_t     cb,
  void*                 udata);

HEDLEY_NON_NULL(1)
static inline void upd_iso_msgfv(upd_iso_t* iso, const char* fmt, va_list args) {
  uint8_t buf[1024];

  const size_t len = vsnprintf((char*) buf, sizeof(buf), fmt, args);
  upd_iso_msg(iso, buf, len);
}

HEDLEY_NON_NULL(1)
HEDLEY_PRINTF_FORMAT(2, 3)
static inline void upd_iso_msgf(upd_iso_t* iso, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  upd_iso_msgfv(iso, fmt, args);
  va_end(args);
}


/*
 * ---- DRIVER INTERFACE ----
 */
struct upd_driver_t {
  const uint8_t*       name;
  const upd_req_cat_t* cats;

  struct {
    unsigned npoll    : 1;
    unsigned mutex    : 1;
    unsigned preproc  : 1;
    unsigned postproc : 1;
    unsigned timer    : 1;
  } flags;

  bool
  (*init)(
    upd_file_t* f);
  void
  (*deinit)(
    upd_file_t* f);
  bool
  (*handle)(
    upd_req_t* req);
};

HEDLEY_NON_NULL(1, 2)
UPD_DECL_FUNC
const upd_driver_t*
upd_driver_lookup(
  upd_iso_t*     iso,
  const uint8_t* name,
  uint64_t       len);


/*
 * ---- FILE INTERFACE ----
 */
#define UPD_FILE_ID_ROOT 0

#define UPD_FILE_LOCK_DEFAULT_TIMEOUT 10000

enum {
  /* upd_file_event_t */
  UPD_FILE_DELETE   = 0x00,
  UPD_FILE_UPDATE   = 0x01,
  UPD_FILE_DELETE_N = 0x10,
  UPD_FILE_UPDATE_N = 0x11,
  UPD_FILE_UNCACHE  = 0x20,
  UPD_FILE_PREPROC  = 0x30,
  UPD_FILE_POSTPROC = 0x38,
  UPD_FILE_ASYNC    = 0x40,
  UPD_FILE_TIMER    = 0x50,
  UPD_FILE_SHUTDOWN = 0xF0,
};

struct upd_file_t {
  /* filled by user */
  upd_iso_t*          iso;
  const upd_driver_t* driver;

  uint8_t* path;
  uint64_t pathlen;

  uint8_t* npath;
  uint64_t npathlen;

  uint8_t* param;
  uint64_t paramlen;

  upd_file_t* backend;

  /* used by iso */
  upd_file_id_t id;
  uint64_t      refcnt;
  uint64_t      last_touch;

  /* used by driver */
  const uint8_t* mimetype;
  uint64_t       cache;  /* estimated cache size in bytes */
  void*          ctx;
};

struct upd_file_watch_t {
  upd_file_t* file;

  void* udata;
  void
  (*cb)(
    upd_file_watch_t* w);

  upd_file_event_t event;
};

struct upd_file_lock_t {
  upd_file_t* file;

  uint64_t basetime;
  uint64_t timeout;

  void* udata;
  void
  (*cb)(
    upd_file_lock_t* k);

  unsigned ex : 1;
  unsigned ok : 1;
};

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
UPD_DECL_FUNC
upd_file_t*
upd_file_new(
  const upd_file_t* src);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
upd_file_t*
upd_file_get(
  upd_iso_t*    iso,
  upd_file_id_t id);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
void
upd_file_ref(
  upd_file_t* file);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
bool
upd_file_unref(
  upd_file_t* file);

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
UPD_DECL_FUNC
bool
upd_file_watch(
  upd_file_watch_t* w);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
void
upd_file_unwatch(
  upd_file_watch_t* w);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
void
upd_file_trigger(
  upd_file_t*      f,
  upd_file_event_t e);

/* thread-safe */
HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
UPD_DECL_FUNC
bool
upd_file_trigger_async(
  upd_iso_t*    iso,
  upd_file_id_t id);

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
UPD_DECL_FUNC
bool
upd_file_trigger_timer(
  upd_file_t* f,
  uint64_t    dur);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
void
upd_file_begin_sync(
  upd_file_t* f);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
void
upd_file_end_sync(
  upd_file_t* f);

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
UPD_DECL_FUNC
bool
upd_file_lock(
  upd_file_lock_t* l);

HEDLEY_NON_NULL(1)
UPD_DECL_FUNC
void
upd_file_unlock(
  upd_file_lock_t* l);

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline upd_file_lock_t* upd_file_lock_with_dup(
    const upd_file_lock_t* l) {
  upd_file_lock_t* k = upd_iso_stack(l->file->iso, sizeof(*k));
  if (k == NULL) {
    return NULL;
  }
  *k = *l;
  if (!upd_file_lock(k)) {
    upd_iso_unstack(l->file->iso, k);
    return NULL;
  }
  return k;
}


/*
 * ---- REQUEST INTERFACE ----
 */
#define UPD_REQ_CAT_EACH(f)  \
  f(0x0001, DIR)  \
  f(0x0002, STREAM)  \
  f(0x0003, PROG)  \
  f(0x0004, DSTREAM)  \
  f(0x0005, TENSOR)

#define UPD_REQ_TYPE_EACH(f)  \
  f(DIR, 0x0010, LIST)  \
  f(DIR, 0x0020, FIND)  \
  f(DIR, 0x0030, ADD)  \
  f(DIR, 0x0038, NEW)  \
  f(DIR, 0x0039, NEWDIR)  \
  f(DIR, 0x0040, RM)  \
\
  f(STREAM, 0x0010, READ)  \
  f(STREAM, 0x0020, WRITE)  \
  f(STREAM, 0x0030, TRUNCATE)  \
\
  f(PROG, 0x0010, EXEC)  \
\
  f(DSTREAM, 0x0010, READ)  \
  f(DSTREAM, 0x0020, WRITE)  \
\
  f(TENSOR, 0x0010, META)  \
  f(TENSOR, 0x0020, FETCH)  \
  f(TENSOR, 0x0028, FLUSH)

enum {
# define each_(i, N) UPD_REQ_##N = i,
  UPD_REQ_CAT_EACH(each_)
# undef each_

# define each_(C, i, N) UPD_REQ_##C##_##N = (UPD_REQ_##C << 16) | i,
  UPD_REQ_TYPE_EACH(each_)
# undef each_

  UPD_REQ_OK      = 0x00,
  UPD_REQ_NOMEM   = 0x01,
  UPD_REQ_ABORTED = 0x02,
  UPD_REQ_INVALID = 0x03,
};


typedef struct upd_req_dir_entry_t {
  uint8_t*    name;
  uint64_t    len;
  upd_file_t* file;
} upd_req_dir_entry_t;

typedef struct upd_req_dir_entries_t {
  upd_req_dir_entry_t** p;
  uint64_t              n;
} upd_req_dir_entries_t;


typedef struct upd_req_stream_io_t {
  uint64_t offset;
  uint64_t size;
  uint8_t* buf;

  unsigned tail : 1;
} upd_req_stream_io_t;


typedef struct upd_req_tensor_meta_t {
  uint8_t           rank;
  upd_tensor_type_t type;
  uint32_t*         reso;
} upd_req_tensor_meta_t;

typedef struct upd_req_tensor_data_t {
  upd_req_tensor_meta_t meta;
  uint8_t* ptr;
  uint64_t size;
} upd_req_tensor_data_t;


struct upd_req_t {
  upd_file_t* file;

  upd_req_type_t   type;
  upd_req_result_t result;

  void* udata;
  void
  (*cb)(
    upd_req_t* req);

  union {
    union {
      upd_req_dir_entry_t   entry;
      upd_req_dir_entries_t entries;
    } dir;
    union {
      upd_file_t* exec;
    } prog;
    union {
      upd_req_stream_io_t io;
    } stream;
    union {
      upd_req_tensor_meta_t meta;
      upd_req_tensor_data_t data;
    } tensor;
  };
};

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline bool upd_req(upd_req_t* req) {
  upd_file_t* f = req->file;
  f->last_touch = upd_iso_now(f->iso);
  return req->file->driver->handle(req);
}

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline upd_req_t* upd_req_with_dup(const upd_req_t* src) {
  upd_req_t* dst = upd_iso_stack(src->file->iso, sizeof(*dst));
  if (dst == NULL) {
    return NULL;
  }
  *dst = *src;

  if (!upd_req(dst)) {
    upd_iso_unstack(src->file->iso, dst);
    return NULL;
  }
  return dst;
}


/* ---- TENSOR TYPE ---- */
enum {
  /* upd_tensor_type_t */
  UPD_TENSOR_U8  = 0x00,
  UPD_TENSOR_U16 = 0x01,
  UPD_TENSOR_F32 = 0x10,
  UPD_TENSOR_F64 = 0x11,
};


/* ---- IMPLEMENTATIONS FOR EXTERNAL DRIVERS ---- */
typedef struct upd_host_t {
  upd_ver_t ver;

  struct {
    void* (*stack)(upd_iso_t* iso, uint64_t len);
    void (*unstack)(upd_iso_t* iso, void* ptr);
    uint64_t (*now)(upd_iso_t* iso);
    void (*msg)(upd_iso_t* iso, const uint8_t* msg, uint64_t len);
    bool (*start_thread)(upd_iso_t* iso, upd_iso_thread_main_t main, void* udata);
    bool (*start_work)(upd_iso_t* iso, upd_iso_thread_main_t main, upd_iso_work_cb_t cb, void* udata);
  } iso;

  struct {
    const upd_driver_t* (*lookup)(upd_iso_t* iso, const uint8_t* name, uint64_t len);
  } driver;

  struct {
    upd_file_t* (*new)(const upd_file_t* src);
    upd_file_t* (*get)(upd_iso_t* iso, upd_file_id_t id);
    void (*ref)(upd_file_t* f);
    bool (*unref)(upd_file_t* f);
    bool (*watch)(upd_file_watch_t* w);
    void (*unwatch)(upd_file_watch_t* w);
    void (*trigger)(upd_file_t* f, upd_file_event_t e);
    bool (*trigger_async)(upd_iso_t* iso, upd_file_id_t id);
    bool (*trigger_timer)(upd_file_t* f, uint64_t dur);
    void (*begin_sync)(upd_file_t* f);
    void (*end_sync)(upd_file_t* f);
    bool (*lock)(upd_file_lock_t* k);
    void (*unlock)(upd_file_lock_t* k);
  } file;

# define UPD_HOST_INSTANCE {  \
    .ver = UPD_VER,  \
    .iso = {  \
      .stack        = upd_iso_stack,  \
      .unstack      = upd_iso_unstack,  \
      .now          = upd_iso_now,  \
      .msg          = upd_iso_msg,  \
      .start_thread = upd_iso_start_thread,  \
      .start_work   = upd_iso_start_work,  \
    },  \
    .driver = {  \
      .lookup = upd_driver_lookup,  \
    },  \
    .file = {  \
      .new           = upd_file_new,  \
      .get           = upd_file_get,  \
      .ref           = upd_file_ref,  \
      .unref         = upd_file_unref,  \
      .watch         = upd_file_watch,  \
      .unwatch       = upd_file_unwatch,  \
      .trigger       = upd_file_trigger,  \
      .trigger_async = upd_file_trigger_async,  \
      .trigger_timer = upd_file_trigger_timer,  \
      .begin_sync    = upd_file_begin_sync,  \
      .end_sync      = upd_file_end_sync,  \
      .lock          = upd_file_lock,  \
      .unlock        = upd_file_unlock,  \
    },  \
  }
} upd_host_t;

typedef struct upd_external_t {
  upd_ver_t ver;

  const upd_host_t*    host;
  const upd_driver_t** drivers;
} upd_external_t;


#if defined(UPD_EXTERNAL_DRIVER)

#if defined(_WIN32)
  __declspec(dllexport)
#endif
extern upd_external_t upd;

static inline void* upd_iso_stack(upd_iso_t* iso, uint64_t len) {
  return upd.host->iso.stack(iso, len);
}
static inline void upd_iso_unstack(upd_iso_t* iso, void* ptr) {
  upd.host->iso.unstack(iso, ptr);
}
static inline uint64_t upd_iso_now(upd_iso_t* iso) {
  return upd.host->iso.now(iso);
}
static inline void upd_iso_msg(upd_iso_t* iso, const uint8_t* msg, uint64_t len) {
  upd.host->iso.msg(iso, msg, len);
}
static inline bool upd_iso_start_thread(upd_iso_t* iso, upd_iso_thread_main_t main, void* udata) {
  return upd.host->iso.start_thread(iso, main, udata);
}
static inline bool upd_iso_start_work(upd_iso_t* iso, upd_iso_thread_main_t main, upd_iso_work_cb_t cb, void* udata) {
  return upd.host->iso.start_work(iso, main, cb, udata);
}
static inline const upd_driver_t* upd_driver_lookup(upd_iso_t* iso, const uint8_t* name, uint64_t len) {
  return upd.host->driver.lookup(iso, name, len);
}
static inline upd_file_t* upd_file_new(const upd_file_t* src) {
  return upd.host->file.new(src);
}
static inline upd_file_t* upd_file_get(upd_iso_t* iso, upd_file_id_t id) {
  return upd.host->file.get(iso, id);
}
static inline void upd_file_ref(upd_file_t* f) {
  upd.host->file.ref(f);
}
static inline bool upd_file_unref(upd_file_t* f) {
  return upd.host->file.unref(f);
}
static inline bool upd_file_watch(upd_file_watch_t* w) {
  return upd.host->file.watch(w);
}
static inline void upd_file_unwatch(upd_file_watch_t* w) {
  upd.host->file.unwatch(w);
}
static inline void upd_file_trigger(upd_file_t* f, upd_file_event_t e) {
  upd.host->file.trigger(f, e);
}
static inline bool upd_file_trigger_async(upd_iso_t* iso, upd_file_id_t id) {
  return upd.host->file.trigger_async(iso, id);
}
static inline bool upd_file_trigger_timer(upd_file_t* f, uint64_t dur) {
  return upd.host->file.trigger_timer(f, dur);
}
static inline void upd_file_begin_sync(upd_file_t* f) {
  upd.host->file.begin_sync(f);
}
static inline void upd_file_end_sync(upd_file_t* f) {
  upd.host->file.end_sync(f);
}
static inline bool upd_file_lock(upd_file_lock_t* k) {
  return upd.host->file.lock(k);
}
static inline void upd_file_unlock(upd_file_lock_t* k) {
  upd.host->file.unlock(k);
}

#endif  /* UPD_EXTERNAL_DRIVER */
