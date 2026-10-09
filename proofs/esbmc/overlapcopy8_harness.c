/* ZSTD_overlapCopy8 with op and ip in one buffer, ip before op. */
#include <stdlib.h>

#include "harness.h"

#include "decompress/zstd_decompress_block.c"

#define BUFCAP 48

int main(void)
{
    BYTE* const buf     = malloc(BUFCAP);
    size_t const srcOff = nondet_size_t();
    size_t const dstOff = nondet_size_t();
    __ESBMC_assume(dstOff <= BUFCAP - 8);
    __ESBMC_assume(srcOff < dstOff);

    BYTE* op              = buf + dstOff;
    const BYTE* ip        = buf + srcOff;
    BYTE* const op0       = op;
    const BYTE* const ip0 = ip;

    ZSTD_overlapCopy8(&op, &ip, dstOff - srcOff);

    __ESBMC_assert(op == op0 + 8, "op advances by exactly 8");
    __ESBMC_assert(ip > ip0 && ip <= ip0 + 8, "ip advances by 1 to 8");
    __ESBMC_assert(op - ip >= 8, "offset spread to at least 8");
    ESBMC_REACHED();
    return 0;
}
