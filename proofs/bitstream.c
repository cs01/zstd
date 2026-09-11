/* Translation unit exposing BIT_initDStream and BIT_lookBits to CBMC.
 *
 * There is no harness in this file, and that is the point. Both contracts
 * state their pointers with contract_fresh, so --enforce-contract generates
 * the entry point from the contract itself: the proof then covers every
 * input satisfying the preconditions, not one geometry someone guessed. The
 * same clauses are what stock clang checks at every real call site, so the
 * question "do the proof's inputs match reality?" is answered by the
 * compiler rather than assumed here.
 *
 * The functions that cannot do this -- wildcopy, overlapCopy8, safecopy --
 * take pointers that alias into one object, which contract_fresh cannot
 * express, and so keep hand-written harnesses.
 *
 * Both are declared MEM_STATIC / FORCE_INLINE_TEMPLATE, which goto-cc drops
 * before anything can be proved. compiler.h defines those unconditionally,
 * so blanking them has to happen after it is included and before bitstream.h
 * uses them; pulling compiler.h in first makes bitstream.h's own include of
 * it a no-op and leaves the blanks standing. */
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
#define MEM_STATIC
#define FORCE_INLINE_ATTR
#define FORCE_INLINE_TEMPLATE

#include "bitstream.h"
