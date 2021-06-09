#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <hedley.h>
#include <utf8.h>

#include <libupd.h>


typedef struct upd_pathfind_t upd_pathfind_t;

struct upd_pathfind_t {
  upd_iso_t*  iso;
  upd_file_t* base;

  uint8_t* path;
  size_t   len;
  size_t   term;

  bool create;

  upd_req_t       req;
  upd_file_lock_t lock;

  void* udata;
  void
  (*cb)(
    upd_pathfind_t* pf);

  /* I cannot wait to win this match, friend! :) */
};


static
void
upd_pathfind_next_(
  upd_pathfind_t* pf);

static
void
upd_pathfind_lock_cb_(
  upd_file_lock_t* lock);

static
void
upd_pathfind_find_cb_(
  upd_req_t* req);

static
void
upd_pathfind_add_cb_(
  upd_req_t* req);


HEDLEY_NON_NULL(1)
static inline void upd_pathfind(upd_pathfind_t* pf) {
  if (pf->len && pf->path[0] == '/') {
    pf->base = NULL;
  }
  if (!pf->base) {
    pf->base = upd_file_get(pf->iso, UPD_FILE_ID_ROOT);
  }
  if (!pf->iso) {
    pf->iso = pf->base->iso;
  }
  upd_pathfind_next_(pf);
}

HEDLEY_NON_NULL(1)
HEDLEY_WARN_UNUSED_RESULT
static inline upd_pathfind_t* upd_pathfind_with_dup(const upd_pathfind_t* src) {
  upd_iso_t* iso = src->iso? src->iso: src->base->iso;
  assert(iso);

  upd_pathfind_t* pf = upd_iso_stack(iso, sizeof(*pf)+src->len);
  if (HEDLEY_UNLIKELY(pf == NULL)) {
    return NULL;
  }
  *pf = *src;

  pf->path = (uint8_t*) (pf+1);
  utf8ncpy((uint8_t*) pf->path, src->path, pf->len);

  upd_pathfind(pf);
  return pf;
}


static void upd_pathfind_next_(upd_pathfind_t* pf) {
  while (pf->len && pf->path[0] == '/') {
    ++pf->path;
    --pf->len;
  }
  pf->term = 0;
  while (pf->term < pf->len && pf->path[pf->term] != '/') {
    ++pf->term;
  }
  if (!pf->base) {
    pf->base = upd_file_get(pf->iso, UPD_FILE_ID_ROOT);
  }
  if (!pf->len) {
    pf->cb(pf);
    return;
  }

  pf->lock = (upd_file_lock_t) {
    .file  = pf->base,
    .udata = pf,
    .cb    = upd_pathfind_lock_cb_,
  };
  if (HEDLEY_UNLIKELY(!upd_file_lock(&pf->lock))) {
    pf->cb(pf);
    return;
  }
}

static inline void upd_pathfind_lock_cb_(upd_file_lock_t* lock) {
  upd_pathfind_t* pf = lock->udata;

  if (HEDLEY_UNLIKELY(!lock->ok)) {
    goto ABORT;
  }
  pf->req = (upd_req_t) {
    .file = pf->base,
    .type = UPD_REQ_DIR_FIND,
    .dir  = { .entry = {
      .name = (uint8_t*) pf->path,
      .len  = pf->term,
    }, },
    .udata = pf,
    .cb    = upd_pathfind_find_cb_,
  };
  if (HEDLEY_UNLIKELY(!upd_req(&pf->req))) {
    goto ABORT;
  }
  return;

ABORT:
  upd_file_unlock(&pf->lock);
  pf->cb(pf);
}

static void upd_pathfind_find_cb_(upd_req_t* req) {
  upd_pathfind_t* pf = req->udata;

  if (HEDLEY_UNLIKELY(req->dir.entry.file == NULL)) {
    if (pf->create) {
      pf->req = (upd_req_t) {
        .file = pf->base,
        .type = UPD_REQ_DIR_NEWDIR,
        .dir  = { .entry = {
          .name = pf->path,
          .len  = pf->term,
        }, },
        .udata = pf,
        .cb    = upd_pathfind_add_cb_,
      };
      const bool newdir = upd_req(&pf->req);
      if (HEDLEY_LIKELY(newdir)) {
        return;
      }
    }
    upd_file_unlock(&pf->lock);
    pf->cb(pf);
    return;
  }

  upd_file_unlock(&pf->lock);
  pf->base  = req->dir.entry.file;
  pf->path += pf->term;
  pf->len  -= pf->term;
  upd_pathfind_next_(pf);
}

static void upd_pathfind_add_cb_(upd_req_t* req) {
  upd_pathfind_t* pf = req->udata;

  upd_file_unlock(&pf->lock);

  if (HEDLEY_UNLIKELY(req->result != UPD_REQ_OK)) {
    pf->cb(pf);
    return;
  }

  pf->base  = req->dir.entry.file;
  pf->path += pf->term;
  pf->len  -= pf->term;
  upd_pathfind_next_(pf);
}
