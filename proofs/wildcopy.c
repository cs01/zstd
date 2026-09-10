#include <string.h>
#include <stddef.h>
/* Override zstd's builtin wrappers so CBMC gets plain memcpy/memmove. Must
   appear before zstd_deps.h, which is pulled in by zstd_internal.h. */
#define ZSTD_DEPS_NEED_MALLOC
#include "zstd_deps.h"
#undef ZSTD_memcpy
#undef ZSTD_memmove
#undef ZSTD_memset
#define ZSTD_memcpy(d,s,l) memcpy((d),(s),(l))
#define ZSTD_memmove(d,s,l) memmove((d),(s),(l))
#define ZSTD_memset(d,v,l) memset((d),(v),(l))
#define MEM_STATIC
#define FORCE_INLINE_ATTR
#include "zstd_internal.h"

void *__CPROVER_allocate(unsigned long, int);
void __CPROVER_assume(int);

void harness(void)
{
  size_t length;
  __CPROVER_assume(length < 4096);
  BYTE *dst = __CPROVER_allocate(length + WILDCOPY_OVERLENGTH, 0);
  const BYTE *src = __CPROVER_allocate(length + WILDCOPY_OVERLENGTH, 0);
  ZSTD_wildcopy(dst, src, length, ZSTD_no_overlap);
}
