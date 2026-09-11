/* Harness for ZSTD_safecopy: exposes cross-object pointer subtraction.
 *
 * safecopy computes `diff = op - ip` unconditionally, before anything has
 * established that op and ip are in the same object. Under ZSTD_no_overlap
 * they are in different allocations by construction, and subtracting
 * pointers into different objects is undefined in C.
 *
 * Proves the real static function in lib/decompress/zstd_decompress_block.c
 * by including that translation unit, not a pasted copy of the body.
 *
 * Length and both buffer geometries are symbolic; the bound only keeps the
 * solver finite. */
#include <string.h>
#include <stddef.h>

#define ZSTD_DEPS_NEED_MALLOC
#include "zstd_deps.h"
#undef ZSTD_memcpy
#undef ZSTD_memmove
#undef ZSTD_memset
#define ZSTD_memcpy(d,s,l) memcpy((d),(s),(l))
#define ZSTD_memmove(d,s,l) memmove((d),(s),(l))
#define ZSTD_memset(d,v,l) memset((d),(v),(l))

#include "compiler.h"
#undef MEM_STATIC
#undef FORCE_INLINE_ATTR
#undef FORCE_INLINE_TEMPLATE
#undef HINT_INLINE
#define MEM_STATIC
#define FORCE_INLINE_ATTR
#define FORCE_INLINE_TEMPLATE
#define HINT_INLINE

#include "zstd_internal.h"
#include "decompress/zstd_decompress_block.c"

void *__CPROVER_allocate(unsigned long, int);
void __CPROVER_assume(int);

void harness(void)
{
    size_t length;

    /* Short enough to stay on the byte-copy tail and keep the proof finite;
       the defect is in the unconditional `diff` on entry, which every length
       reaches. */
    __CPROVER_assume(length <= 8);

    /* The contract's own two clauses: WILDCOPY_OVERLENGTH of readable and
       writable slack past length. Distinct objects, which is exactly what
       ZSTD_no_overlap asserts and what makes op - ip undefined. Sized
       concretely; a symbolic allocation size makes CBMC's pointer encoding
       explode and the defect does not turn on the object size. */
    BYTE *op = __CPROVER_allocate(8 + WILDCOPY_OVERLENGTH, 0);
    const BYTE *ip = __CPROVER_allocate(8 + WILDCOPY_OVERLENGTH, 0);

    /* oend_w is the caller's "wildcopy is still safe below here" mark. It is
       a bound on the destination object, so it is derived from op. */
    size_t oendOff;
    __CPROVER_assume(oendOff <= length);
    const BYTE *oend_w = op + oendOff;

    ZSTD_safecopy(op, oend_w, ip, length, ZSTD_no_overlap);
}
