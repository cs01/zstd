/* ZSTD_wildcopy in ZSTD_no_overlap mode with dst and src in separate buffers. */
#include <stdlib.h>

#include "harness.h"

#include "bounds_memcpy_model.h"

#include "zstd_internal.h"

#define LENCAP 24

int main(void)
{
    size_t const length = nondet_size_t();
    __ESBMC_assume(length <= LENCAP);
    BYTE* const dst = malloc(length + WILDCOPY_OVERLENGTH);
    BYTE* const src = malloc(length + WILDCOPY_OVERLENGTH);

    ZSTD_wildcopy(dst, src, length, ZSTD_no_overlap);
    ESBMC_REACHED();
    return 0;
}
