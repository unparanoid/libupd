#pragma once

#include <stddef.h>
#include <stdint.h>

#include <hedley.h>

#include <libupd.h>


static inline
void
upd_tensor_conv_f32_to_u16(
  uint16_t*    dst,
  const float* src,
  size_t       n);

static inline
void
upd_tensor_conv_f64_to_u16(
  uint16_t*     dst,
  const double* src,
  size_t        n);


static inline
size_t
upd_tensor_count_scalars(
  const upd_req_tensor_meta_t* meta);

static inline
size_t
upd_tensor_type_sizeof(
  upd_tensor_type_t type);


static inline void upd_tensor_conv_f32_to_u16(
    uint16_t* dst, const float* src, size_t n) {
# define clamp_(v) ((v) < 0? 0: (v) > 1? 1: (v))

  size_t i = (n+7)/8;
  switch (n%8) {
  case 0: do { *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 7:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 6:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 5:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 4:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 3:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 2:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 1:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
          } while (--i);
  }

# undef clamp_
}

static inline void upd_tensor_conv_f64_to_u16(
    uint16_t* dst, const double* src, size_t n) {
# define clamp_(v) ((v) < 0? 0: (v) > 1? 1: (v))

  size_t i = (n+7)/8;
  switch (n%8) {
  case 0: do { *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 7:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 6:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 5:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 4:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 3:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 2:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
  case 1:      *(dst++) = clamp_(*src) * UINT16_MAX; ++src; HEDLEY_FALL_THROUGH;
          } while (--i);
  }

# undef clamp_
}


static inline size_t upd_tensor_count_scalars(
    const upd_req_tensor_meta_t* meta) {
  assert(meta->rank > 0);

  size_t n = 1;
  for (size_t i = 0; i < meta->rank; ++i) {
    n *= meta->reso[i];
  }
  return n;
}

static inline size_t upd_tensor_type_sizeof(upd_tensor_type_t t) {
  switch (t) {
  case UPD_TENSOR_U8:  return sizeof(uint8_t);
  case UPD_TENSOR_U16: return sizeof(uint16_t);
  case UPD_TENSOR_F32: return sizeof(float);
  case UPD_TENSOR_F64: return sizeof(double);
  }
  return 0;
}
