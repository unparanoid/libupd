#undef NDEBUG

#include <assert.h>
#include <inttypes.h>
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
#include "libupd/tensor.h"
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
test_tensor_(
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
  test_tensor_();
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

  uint8_t      p3[] = "/a///b/.//./c/././d";
  const size_t l3   = upd_path_normalize(p3, sizeof(p3)-1);
  assert(upd_streq_c("/a/b/c/d", p3, l3));

  uint8_t      p4[] = "/a///../b//..////c/d/";
  const size_t l4   = upd_path_normalize(p4, sizeof(p4)-1);
  assert(upd_streq_c("/c/d/", p4, l4));

  uint8_t      p5[] = "emacs//.//..//vim/././is/./not/..//superior/./to///vim/../vscode/";
  const size_t l5   = upd_path_normalize(p5, sizeof(p5)-1);
  assert(upd_streq_c("vim/is/superior/to/vscode/", p5, l5));

  uint8_t      p6[] = "/../this/is/invalid";
  const size_t l6   = upd_path_normalize(p6, sizeof(p6)-1);
  assert(l6 == 0);

  uint8_t      p7[] = "../this/../is/../../valid/../../A";
  const size_t l7   = upd_path_normalize(p7, sizeof(p7)-2);
  assert(upd_streq_c("../../../", p7, l7));
  assert(p7[sizeof(p7)-2] == 'A');  /* canary check */
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

static void test_tensor_(void) {
  const float  in_f32[10] = {.1, .2, .3, .4, .5, .6, .7, .8, .9, 1.};
  const double in_f64[10] = {.1, .2, .3, .4, .5, .6, .7, .8, .9, 1.};

  uint16_t out[10];
  upd_tensor_conv_f32_to_u16(out, in_f32, 10);
  for (size_t i = 0; i < 10; ++i) {
    assert(fabs(out[i] - in_f32[i]*UINT16_MAX) < 1);
  }

  upd_tensor_conv_f64_to_u16(out, in_f64, 10);
  for (size_t i = 0; i < 10; ++i) {
    assert(fabs(out[i] - in_f64[i]*UINT16_MAX) < 1);
  }

  assert(upd_tensor_count_scalars(&(upd_req_tensor_meta_t) {
      .rank = 6,
      .reso = (uint32_t[]) { 1, 100, 200, 300, 400, 500, }
    }) == UINTMAX_C(100)*200*300*400*500);
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

  /* When a value with incompatible type found, returns its name otherwise NULL.  */
  assert(!upd_yaml_find_fields_from_root(&doc, (upd_yaml_field_t[]) {
      { .name = "cat", .required = true,  .str = &cat, },
      { .name = "dog", .required = false, .str = &dog, },
      { .name = "vim",    .str = &vim,   .i = &vim_i,   .ui = &vim_ui,   .f = &vim_f,   },
      { .name = "emacs",  .str = &emacs, .i = &emacs_i, .ui = &emacs_ui, .f = &emacs_f, },
      { .name = "vscode", .str = &vscode, },
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
