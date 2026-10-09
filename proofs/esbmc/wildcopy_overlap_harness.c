/* ZSTD_wildcopy in ZSTD_overlap_src_before_dst mode within one buffer. */
#include <stdlib.h>

#include "harness.h"

#include "bounds_memcpy_model.h"

#include "zstd_internal.h"

#define LENCAP 40
#define BUFCAP (LENCAP + 2 * WILDCOPY_OVERLENGTH)

int main(void)
{
    BYTE* const buf     = malloc(BUFCAP);
    size_t const length = nondet_size_t();
    size_t const sep    = nondet_size_t();
    __ESBMC_assume(length <= LENCAP);
    __ESBMC_assume(sep >= 8 && sep <= BUFCAP);
    __ESBMC_assume(sep + length + WILDCOPY_OVERLENGTH <= BUFCAP);

    ZSTD_wildcopy(buf + sep, buf, length, ZSTD_overlap_src_before_dst);
    ESBMC_REACHED();
    return 0;
}
