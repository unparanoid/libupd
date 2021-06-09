#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <utf8.h>

#include "libupd/array.h"
#include "libupd/buf.h"
#include "libupd/memory.h"
#include "libupd/path.h"


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


int main(void) {
  test_memory_();

  test_array_();
  test_buf_();
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
