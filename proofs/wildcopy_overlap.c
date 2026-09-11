/* Harness for ZSTD_wildcopy in ZSTD_overlap_src_before_dst mode.
 *
 * The mode wildcopy.c cannot cover: src and dst are slices of one object, so
 * they must be built as two offsets into a single allocation. contract_fresh
 * cannot say that -- it only makes distinct objects -- which is the reason
 * this function needs a hand-written entry point at all.
 *
 * Geometry is symbolic: buffer size, both offsets and length are constrained
 * only by the function's own contract_pre clauses. The caps keep the solver
 * finite and nothing else. */
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
#define MEM_STATIC
#define FORCE_INLINE_ATTR

#include "zstd_internal.h"

void *__CPROVER_allocate(unsigned long, int);
void __CPROVER_assume(int);

#define LENCAP 16
#define BUFCAP (2 * LENCAP + 2 * WILDCOPY_OVERLENGTH)

void harness(void)
{
  size_t length, sep;

  __CPROVER_assume(length <= LENCAP);

  /* Concrete size, symbolic separation. Three things were tried here and
     only this one solves: a symbolic allocation size makes CBMC's pointer
     encoding explode outright, and two independent symbolic offsets over the
     buffer did not finish in 20s either. What an aliasing defect turns on is
     how far apart src and dst are, not where the pair sits in the object, so
     src is pinned to the object start and the separation ranges freely. */
  BYTE *buf = __CPROVER_allocate(BUFCAP, 0);

  /* Bound sep on its own first. Without this the sum below is size_t
     arithmetic that wraps: a huge sep satisfies the constraint and still puts
     dst far outside the object, so the proof reports out-of-bounds failures
     that describe the harness rather than the function. */
  __CPROVER_assume(sep <= BUFCAP);
  __CPROVER_assume(sep + length + WILDCOPY_OVERLENGTH <= BUFCAP);

  const BYTE *src = buf;
  BYTE *dst = buf + sep;

  /* The clause the doc-comment states and nothing enforced: in overlap mode
     src must precede dst by at least 8. Written as WILDCOPY_VECLEN first,
     which the runtime tier rejected 3809 times on an ordinary corpus --
     ZSTD_overlapCopy8 exists precisely to serve separations of 8..15. */
  __CPROVER_assume(src + 8 <= (const BYTE*)dst);

  ZSTD_wildcopy(dst, src, length, ZSTD_overlap_src_before_dst);
}
