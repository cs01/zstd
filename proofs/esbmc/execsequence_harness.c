/* ZSTD_execSequence with literals stored in dst (ZSTD_in_dst layout), no dictionary. */
#include <stdlib.h>

#include "harness.h"

#include "bounds_memcpy_model.h"

#include "decompress/zstd_decompress_block.c"

#define OUTCAP 48
#define MAXLIT 24
#define MAXMATCH 24
#define MAXOFFSET 64

int main(void)
{
    size_t const outCap = nondet_size_t();
    size_t const opOff  = nondet_size_t();
    size_t const litCap = nondet_size_t();
    __ESBMC_assume(outCap >= 1 && outCap <= OUTCAP);
    __ESBMC_assume(opOff <= outCap);
    __ESBMC_assume(litCap <= MAXLIT);

    BYTE* const dst =
            malloc(outCap + WILDCOPY_OVERLENGTH + litCap + WILDCOPY_OVERLENGTH);
    BYTE* const litBuffer = dst + outCap + WILDCOPY_OVERLENGTH;

    seq_t sequence;
    sequence.litLength   = nondet_size_t();
    sequence.matchLength = nondet_size_t();
    sequence.offset      = nondet_size_t();
    __ESBMC_assume(sequence.litLength <= MAXLIT);
    __ESBMC_assume(sequence.matchLength >= MINMATCH);
    __ESBMC_assume(sequence.matchLength <= MAXMATCH);
    __ESBMC_assume(sequence.offset >= 1 && sequence.offset <= MAXOFFSET);

    const BYTE* litPtr = litBuffer;
    BYTE* const oend   = litBuffer;

    size_t const r = ZSTD_execSequence(
            dst + opOff,
            oend,
            sequence,
            &litPtr,
            litBuffer + litCap,
            dst,
            dst,
            dst);

    if (!ZSTD_isError(r)) {
        __ESBMC_assert(
                r == sequence.litLength + sequence.matchLength,
                "success regenerates the whole sequence");
        __ESBMC_assert(
                r <= (size_t)(oend - (dst + opOff)),
                "success fits before oend");
    }
    ESBMC_REACHED();
    return 0;
}
