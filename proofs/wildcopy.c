/* Harness for ZSTD_wildcopy in ZSTD_no_overlap mode.
 *
 * ZSTD_wildcopy is MEM_STATIC FORCE_INLINE_ATTR, so goto-cc drops it before
 * anything can be proved about it; blanking both gives it external linkage.
 *
 * Its buffers are stated with contract_readable / contract_writable and no
 * contract_fresh -- deliberately, because wildcopy is called on slices of
 * buffers its callers own. That says the memory is accessible but not which
 * object it belongs to, so there is nothing for a generated entry point to
 * allocate and enforce mode has no frame to check. Hence -H.
 *
 * ZSTD_no_overlap means the two buffers are distinct objects, which is what
 * this file sets up. The overlap mode has different pointer geometry -- one
 * object, two offsets -- and lives in wildcopy_overlap.c.
 *
 * length is symbolically sized rather than fixed: the loop inside wildcopy is
 * discharged by its own loop contract, not by an unwind bound, so the cap is
 * only what keeps the solver finite. */
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

void harness(void)
{
  size_t length;
  __CPROVER_assume(length <= LENCAP);

  /* The two contract_pre clauses that apply in this mode: WILDCOPY_OVERLENGTH
     of slack past length on both sides. Distinct objects, per ZSTD_no_overlap.
     Sized concretely -- a symbolic allocation size makes CBMC's pointer
     encoding explode, and the loop contract already discharges the loop, so
     the proof does not turn on how large the objects are. */
  BYTE *dst = __CPROVER_allocate(LENCAP + WILDCOPY_OVERLENGTH, 0);
  const BYTE *src = __CPROVER_allocate(LENCAP + WILDCOPY_OVERLENGTH, 0);

  ZSTD_wildcopy(dst, src, length, ZSTD_no_overlap);
}
