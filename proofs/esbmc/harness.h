/* Shared harness helpers. ESBMC_REACHED() becomes a failing assert under -DESBMC_VACUITY_PROBE. */
#ifndef ZSTD_PROOFS_ESBMC_HARNESS_H
#define ZSTD_PROOFS_ESBMC_HARNESS_H

#include <stddef.h>

size_t nondet_size_t(void);
unsigned char nondet_uchar(void);

#ifdef ESBMC_VACUITY_PROBE
#define ESBMC_REACHED() __ESBMC_assert(0, "vacuity probe: end of harness reached")
#else
#define ESBMC_REACHED() ((void)0)
#endif

#endif
