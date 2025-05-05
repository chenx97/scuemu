#pragma once
#include <glib.h>
#include <libgccjit.h>
#include <stdint.h>

#define clean_up(fn) __attribute__((cleanup(fn)))

#if defined(DEBUG)
#define cg_dprintf printf
#define DEF 1
#else
#define cg_dprintf(...) (void)0
#define DEF 0
#endif

static gint tb_cmp(gconstpointer a, gconstpointer b) {
  return (intptr_t)a - (intptr_t)b;
}

static gboolean result_cleaner(gpointer key, gpointer value, gpointer data) {
  if ((uintptr_t)value != 1) {
    cg_dprintf("cleaning gcc jit result for pc = %" PRIdPTR "\n",
               (uintptr_t)key);
    gcc_jit_result_release(value);
  } else {
    cg_dprintf("found invalidated tb for pc = %" PRIdPTR "\n", (uintptr_t)key);
  }
  return 0;
}

static void tb_cleanup(GTree **tb) {
  if (tb && *tb) {
    cg_dprintf("cleaning up tb at 0x%" PRIxPTR "\n", (uintptr_t)*tb);
    g_tree_foreach(*tb, result_cleaner, NULL);
    g_tree_destroy(*tb);
  }
}

static void blocks_cleanup(gcc_jit_block ***p) {
  if (p && *p)
    free(*p);
}

static void ctxt_cleanup(gcc_jit_context **p) {
  if (p && *p)
    free(*p);
}

static void ram_cleanup(uint32_t **p) {
  if (p && *p)
    free(*p);
}
