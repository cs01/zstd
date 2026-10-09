/* BIT_initDStream: limitPtr stays within its source object. */
#include <stdlib.h>

#include "harness.h"

#include "bitstream.h"

#define MAX_SRC 16

int main(void)
{
    size_t const srcSize = nondet_size_t();
    __ESBMC_assume(srcSize >= 1 && srcSize <= MAX_SRC);
    unsigned char* const src = malloc(srcSize);
    for (size_t i = 0; i < srcSize; i++) {
        src[i] = nondet_uchar();
    }

    BIT_DStream_t bitD;
    (void)BIT_initDStream(&bitD, src, srcSize);

    __ESBMC_assert(
            __ESBMC_POINTER_OFFSET(bitD.limitPtr) >= 0
                    && (size_t)__ESBMC_POINTER_OFFSET(bitD.limitPtr)
                            <= __ESBMC_get_object_size(bitD.limitPtr),
            "limitPtr points into its object or one past its end");
    ESBMC_REACHED();
    return 0;
}
