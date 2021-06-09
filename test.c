#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <utf8.h>

#define UPD_DECL_FUNC static inline
#include "libupd.h"
#undef UPD_DECL_FUNC

#include "libupd/array.h"
#include "libupd/buf.h"
#include "libupd/memory.h"
#include "libupd/path.h"
#include "libupd/pathfind.h"


static
void
test_array_(
  void);

static
void
test_buf_(
  void);

static
void
test_memory_(
  void);

static
void
test_path_(
  void);


int main(void) {
  test_memory_();

  test_array_();
  test_buf_();
  test_path_();
  return EXIT_SUCCESS;
}


static void test_array_(void) {
  upd_array_t a = {0};

  assert(upd_array_insert(&a, (void*) 0x00, SIZE_MAX));
  assert(upd_array_insert(&a, (void*) 0x02, SIZE_MAX));
  assert(upd_array_insert(&a, (void*) 0x01, 1));

  size_t i;
  assert(upd_array_find(&a, &i, (void*) 0x00) && i == 0);
  assert(upd_array_find(&a, &i, (void*) 0x01) && i == 1);
  assert(upd_array_find(&a, &i, (void*) 0x02) && i == 2);
  assert(!upd_array_find(&a, &i, (void*) 0x03));

  assert(upd_array_remove(&a, 1) == (void*) 0x01);
  assert(!upd_array_find(&a, &i, (void*) 0x01));

  assert(upd_array_resize(&a, 3));
  assert(a.p[2] == NULL);

  upd_array_clear(&a);
  assert(a.n == 0);
  assert(a.p == NULL);
}

static void test_buf_(void) {
  upd_buf_t buf = { .max = 16, };
  assert(upd_buf_append_str(&buf, (uint8_t*) "hello!!!"));
  assert(upd_buf_append_str(&buf, (uint8_t*) "world!!!"));
  assert(!upd_buf_append_str(&buf, (uint8_t*) "goodbye!"));

  assert(buf.size == 16);
  assert(utf8ncmp(buf.ptr, "hello!!!world!!!", 16) == 0);

  upd_buf_drop_tail(&buf, 8);

  assert(buf.size == 8);
  assert(utf8ncmp(buf.ptr, "hello!!!", 8) == 0);

  upd_buf_clear(&buf);

  buf = (upd_buf_t) {0};
  assert(upd_buf_append_str(&buf, (uint8_t*) "hello!!!"));
  assert(upd_buf_append_str(&buf, (uint8_t*) "world!!!"));
  assert(upd_buf_append_str(&buf, (uint8_t*) "goodbye!"));

  assert(buf.size == 24);
  assert(utf8ncmp(buf.ptr, "hello!!!world!!!goodbye!", 24) == 0);

  upd_buf_drop_head(&buf, 8);

  assert(buf.size == 16);
  assert(utf8ncmp(buf.ptr, "world!!!goodbye!", 16) == 0);

  upd_buf_clear(&buf);
}

static void test_memory_(void) {
  void* ptr = NULL;
  assert(upd_malloc(&ptr, 16));
  assert(upd_malloc(&ptr, 32));
  assert(upd_malloc(&ptr, 0));
  upd_free(&ptr);
}

static void test_path_(void) {
# define streq_(p, l, v) (utf8ncmp(p, v, l) == 0 && v[l] == 0)

  uint8_t      p1[] = "///hell//world//////";
  const size_t l1   = upd_path_normalize(p1, sizeof(p1)-1);
  assert(streq_(p1, l1, "/hell/world/"));

  assert( upd_path_validate_name((uint8_t*) "foo",     3));
  assert(!upd_path_validate_name((uint8_t*) "foo/baz", 7));

  uint8_t p2[] = "///hoge//piyo//////////////";
  assert(upd_path_drop_trailing_slash(
    p2, sizeof(p2)-1) == utf8size_lazy("///hoge//piyo"));

  assert(upd_path_dirname(p2, sizeof(p2)-1) == utf8size_lazy("///hoge//"));

  size_t l2 = sizeof(p2)-1;
  assert(utf8cmp(upd_path_basename(p2, &l2), "piyo//////////////") == 0);
  assert(l2 == sizeof("piyo//////////////")-1);

# undef streq_
}


/* mock functions */
static inline void* upd_iso_stack(upd_iso_t* iso, uint64_t len) {
  (void) iso;
  (void) len;
  return NULL;
}
static inline void upd_iso_unstack(upd_iso_t* iso, void* ptr) {
  (void) iso;
  (void) ptr;
}
static inline uint64_t upd_iso_now(upd_iso_t* iso) {
  (void) iso;
  return 0;
}
static inline void upd_iso_msg(upd_iso_t* iso, const uint8_t* msg, uint64_t len) {
  (void) iso;
  (void) msg;
  (void) len;
}
static inline bool upd_iso_start_thread(upd_iso_t* iso, upd_iso_thread_main_t main, void* udata) {
  (void) iso;
  (void) main;
  (void) udata;
  return false;
}
static inline bool upd_iso_start_work(upd_iso_t* iso, upd_iso_thread_main_t main, upd_iso_work_cb_t cb, void* udata) {
  (void) iso;
  (void) main;
  (void) cb;
  (void) udata;
  return false;
}
static inline const upd_driver_t* upd_driver_lookup(upd_iso_t* iso, const uint8_t* name, uint64_t len) {
  (void) iso;
  (void) name;
  (void) len;
  return NULL;
}
static inline upd_file_t* upd_file_new(upd_iso_t* iso, const upd_driver_t* driver) {
  (void) iso;
  (void) driver;
  return NULL;
}
static inline upd_file_t* upd_file_get(upd_iso_t* iso, upd_file_id_t id) {
  (void) iso;
  (void) id;
  return NULL;
}
static inline void upd_file_ref(upd_file_t* f) {
  (void) f;
}
static inline bool upd_file_unref(upd_file_t* f) {
  (void) f;
  return false;
}
static inline bool upd_file_watch(upd_file_watch_t* w) {
  (void) w;
  return false;
}
static inline void upd_file_unwatch(upd_file_watch_t* w) {
  (void) w;
}
static inline void upd_file_trigger(upd_file_t* f, upd_file_event_t e) {
  (void) f;
  (void) e;
}
static inline bool upd_file_trigger_async(upd_file_t* f) {
  (void) f;
  return false;
}
static inline bool upd_file_trigger_timer(upd_file_t* f, uint64_t dur) {
  (void) f;
  (void) dur;
  return false;
}
static inline bool upd_file_lock(upd_file_lock_t* k) {
  (void) k;
  return false;
}
static inline void upd_file_unlock(upd_file_lock_t* k) {
  (void) k;
}
