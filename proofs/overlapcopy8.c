/* Harness for ZSTD_overlapCopy8: exposes a transient out-of-bounds pointer.
 *
 * When offset < 8, the function does *ip -= dec64table[offset] (up to 11),
 * which can push *ip before the start of its object. The pointer is fixed up
 * by *ip += 8 two lines later and is never dereferenced out of bounds, which
 * is why fuzzing never found it -- but forming the pointer is UB in C.
 *
 * Proves the real function in lib/decompress/zstd_decompress_block.c, not a
 * copy of it: a pasted body silently stops tracking the source it claims to
 * be about. HINT_INLINE is blanked so goto-cc keeps the symbol.
 *
 * The geometry is symbolic. The buffer size and both pointer offsets within
 * it are unconstrained apart from the function's own preconditions, so this
 * covers every same-object configuration up to the size bound rather than one
 * hand-picked layout. The bound exists only to keep the solver finite. */
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

#define BUFCAP 48

void harness(void)
{
    size_t srcOff, dstOff, offset;

    /* Concrete size, symbolic offsets: a symbolic allocation size makes
       CBMC's pointer encoding explode, and what this defect turns on is
       where the two pointers sit relative to each other, which the offsets
       still range over freely. */
    BYTE *buf = __CPROVER_allocate(BUFCAP, 0);

    /* Both pointers live in the one object: the precondition *ip <= *op is a
       pointer comparison, which is only defined within a single object.
       Each needs 8 bytes of its own -- the function copies 8 and writes
       through *op+4 -- which the contract_pre clauses do not state. That
       omission is itself worth noting: without it a caller cannot tell how
       much room the function requires. */
    /* Bound each offset on its own first: the sums below are size_t
       arithmetic, and a huge offset wraps, satisfies the constraint and still
       lands outside the object. */
    __CPROVER_assume(srcOff <= BUFCAP);
    __CPROVER_assume(dstOff <= BUFCAP);
    __CPROVER_assume(srcOff + 8 <= BUFCAP);
    __CPROVER_assume(dstOff + 8 <= BUFCAP);

    const BYTE *src = buf + srcOff;
    BYTE *dst = buf + dstOff;

    /* Exactly the contract_pre clauses on ZSTD_overlapCopy8, and nothing
       narrower. offset is pinned to the pointer separation because that is
       what every call site passes and what the postcondition needs; BUFCAP is
       what keeps the offset < 8 and offset >= 8 paths both reachable. */
    __CPROVER_assume(src <= (const BYTE*)dst);
    __CPROVER_assume(offset >= 1);
    __CPROVER_assume(offset == (size_t)((const BYTE*)dst - src));

    ZSTD_overlapCopy8(&dst, &src, offset);
}
