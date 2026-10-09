/* Bounds-only ZSTD_memcpy / ZSTD_memmove: checks both ranges are in bounds, copies nothing. */
#ifndef ZSTD_PROOFS_ESBMC_BOUNDS_MEMCPY_MODEL_H
#define ZSTD_PROOFS_ESBMC_BOUNDS_MEMCPY_MODEL_H

#include <stddef.h>

#include "zstd_deps.h"

static void* bounds_copy(void* dst, const void* src, size_t n)
{
    if (n != 0) {
        __ESBMC_assert(
                __ESBMC_POINTER_OFFSET(dst) >= 0
                        && (size_t)__ESBMC_POINTER_OFFSET(dst) + n
                                <= __ESBMC_get_object_size(dst),
                "copy destination in bounds");
        __ESBMC_assert(
                __ESBMC_POINTER_OFFSET(src) >= 0
                        && (size_t)__ESBMC_POINTER_OFFSET(src) + n
                                <= __ESBMC_get_object_size(src),
                "copy source in bounds");
    }
    return dst;
}

#undef ZSTD_memcpy
#undef ZSTD_memmove
#define ZSTD_memcpy(d, s, l) bounds_copy((d), (s), (l))
#define ZSTD_memmove(d, s, l) bounds_copy((d), (s), (l))

#endif
