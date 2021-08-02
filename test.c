#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <utf8.h>

#define UPD_EXTERNAL_DRIVER
#include "libupd.h"
#undef UPD_EXTERNAL_DRIVER

#include "libupd/array.h"
#include "libupd/buf.h"
#include "libupd/memory.h"
#include "libupd/msgpack.h"
#include "libupd/path.h"
#include "libupd/pathfind.h"
#include "libupd/str.h"
#include "libupd/yaml.h"


upd_external_t upd = {0};  /* just to avoid linker error */

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

static
void
test_str_(
  void);

static
void
test_yaml_(
  void);


int main(void) {
  assert((UPD_VER >> 16 & 0xFFFF) == UPD_VER_MAJOR);
  assert((UPD_VER >>  0 & 0xFFFF) == UPD_VER_MINOR);

  test_memory_();

  test_array_();
  test_buf_();
  test_path_();
  test_str_();
  test_yaml_();
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
  uint8_t      p1[] = "///hell//world//////";
  const size_t l1   = upd_path_normalize(p1, sizeof(p1)-1);
  assert(upd_streq_c("/hell/world/", p1, l1));

  assert( upd_path_validate_name((uint8_t*) "foo",     3));
  assert(!upd_path_validate_name((uint8_t*) "foo/baz", 7));
  assert(!upd_path_validate_name((uint8_t*) "foo,baz", 7));

  uint8_t p2[] = "///hoge//piyo//////////////";
  assert(upd_path_drop_trailing_slash(
    p2, sizeof(p2)-1) == utf8size_lazy("///hoge//piyo"));

  assert(upd_path_dirname(p2, sizeof(p2)-1) == utf8size_lazy("///hoge//"));

  size_t l2 = sizeof(p2)-1;
  assert(utf8cmp(upd_path_basename(p2, &l2), "piyo//////////////") == 0);
  assert(l2 == sizeof("piyo//////////////")-1);
}

static void test_str_(void) {
  assert( upd_streq(NULL, 0, NULL, 0));
  assert(!upd_streq("hi", 2, NULL, 0));

  assert( upd_streq("hello", 5, "hello", 5));
  assert( upd_streq("hello", 4, "hello", 4));
  assert(!upd_streq("hello", 4, "hello", 5));
  assert(!upd_streq("hello", 5, "hello", 4));

  assert(!upd_streq("hello", 5, "world", 5));
  assert(!upd_streq("hello", 4, "world", 4));
  assert(!upd_streq("hello", 4, "world", 5));
  assert(!upd_streq("hello", 5, "world", 4));

  assert( upd_streq_c("hello", "hello", 5));
  assert(!upd_streq_c("hello", "hello", 4));
  assert(!upd_streq_c("hello", "world", 5));
  assert(!upd_streq_c("hello", "world", 4));

  assert( upd_streq_c("hell", "hello", 4));
  assert(!upd_streq_c("hell", "hello", 5));
  assert(!upd_streq_c("hell", "world", 4));
  assert(!upd_streq_c("hell", "world", 5));

  assert( upd_strcaseq(NULL, 0, NULL, 0));
  assert(!upd_strcaseq("hi", 2, NULL, 0));

  assert( upd_strcaseq("HELLO", 5, "heLlo", 5));
  assert( upd_strcaseq("HELLO", 4, "heLlo", 4));
  assert(!upd_strcaseq("HELLO", 4, "heLlo", 5));
  assert(!upd_strcaseq("HELLO", 5, "heLlo", 4));

  assert(!upd_strcaseq("HELLO", 5, "woRld", 5));
  assert(!upd_strcaseq("HELLO", 4, "woRld", 4));
  assert(!upd_strcaseq("HELLO", 4, "woRld", 5));
  assert(!upd_strcaseq("HELLO", 5, "woRld", 4));

  assert( upd_strcaseq_c("HElLO", "hello", 5));
  assert(!upd_strcaseq_c("HElLO", "hello", 4));
  assert(!upd_strcaseq_c("HElLO", "world", 5));
  assert(!upd_strcaseq_c("HElLO", "world", 4));

  assert( upd_strcaseq_c("HELL", "hellO", 4));
  assert(!upd_strcaseq_c("HELL", "hellO", 5));
  assert(!upd_strcaseq_c("HELL", "worlD", 4));
  assert(!upd_strcaseq_c("HELL", "worlD", 5));
}

static void test_yaml_(void) {
  const uint8_t case1[] =
    "cat  : kawaii\n"
    "dog  : noisy\n"
    "vim  : 10000\n"
    "emacs: -100.32\n";
  const uint8_t case2[] =
    "hello: world\n"
    "  this: is invalid\n";

  yaml_document_t doc;
  assert(upd_yaml_parse(&doc, case1, sizeof(case1)-1));

  const yaml_node_t* cat    = NULL;
  const yaml_node_t* dog    = NULL;
  const yaml_node_t* vim    = NULL;
  const yaml_node_t* emacs  = NULL;
  const yaml_node_t* vscode = NULL;

  intmax_t  vim_i  = 0, emacs_i  = 0;
  uintmax_t vim_ui = 0, emacs_ui = 0;
  double    vim_f  = 0, emacs_f  = 0;
  assert(!upd_yaml_find_fields_from_root(&doc, (upd_yaml_field_t[]) {
      { .name = "cat", .str = &cat, },
      { .name = "dog", .str = &dog, },
      { .name = "vim",    .str = &vim,   .i = &vim_i,   .ui = &vim_ui,   .f = &vim_f,   },
      { .name = "emacs",  .str = &emacs, .i = &emacs_i, .ui = &emacs_ui, .f = &emacs_f, },
      { .name = "vscode", .str = &vscode },
      { NULL },
    }));

  assert(cat);
  assert(upd_streq_c("kawaii", cat->data.scalar.value, cat->data.scalar.length));

  assert(dog);
  assert(upd_streq_c("noisy", dog->data.scalar.value, dog->data.scalar.length));

  assert(vim);
  assert(vim_i  == 10000);
  assert(vim_ui == 10000);
  assert(vim_f  == 10000);

  assert(emacs);
  assert(emacs_i  == 0);
  assert(emacs_ui == 0);
  assert(emacs_f  == -100.32);

  assert(vscode == NULL);

  yaml_document_delete(&doc);

  assert(!upd_yaml_parse(&doc, case2, sizeof(case2)-1));
}
