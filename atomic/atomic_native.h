/* 
 * Copyright (C) 2006 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief Native (asm) atomic operations and memory barriers
 * 
 * Native (assembler) atomic operations and memory barriers.
 * \warning atomic ops do not include memory barriers, see atomic_ops.h for
 * more info. Expects atomic_t to be defined (#include "atomic_common.h")
 *
 * Config defines:   
 * - CC_GCC_LIKE_ASM  - the compiler support gcc style inline asm
 * - NOSMP - the code will be a little faster, but not SMP safe
 * - __CPU_i386, __CPU_x86_64, X86_OOSTORE - see atomic_x86.h
 * - __CPU_mips, __CPU_mips2, __CPU_mips64, MIPS_HAS_LLSC - see atomic_mip2.h
 * - __CPU_ppc, __CPU_ppc64 - see atomic_ppc.h
 * - __CPU_sparc - see atomic_sparc.h
 * - __CPU_sparc64, SPARC64_MODE - see atomic_sparc64.h
 * - __CPU_arm, __CPU_arm6 - see atomic_arm.h
 * - __CPU_alpha - see atomic_alpha.h
 * @ingroup atomic
 */

/* 
 * History:
 * --------
 *  2006-03-08  created by andrei
 *  2007-05-13  split from atomic_ops.h (andrei)
 */


#ifndef __atomic_native
#define __atomic_native

#ifdef CC_GCC_LIKE_ASM

#if defined __CPU_i386 || defined __CPU_x86_64

#include "atomic_x86.h"

#elif defined __CPU_mips2 || defined __CPU_mips64 || \
	  ( defined __CPU_mips && defined MIPS_HAS_LLSC )

#include "atomic_mips2.h"

#elif defined __CPU_ppc || defined __CPU_ppc64

#include "atomic_ppc.h"

#elif defined __CPU_sparc64

#include "atomic_sparc64.h"

#elif defined __CPU_sparc

#include "atomic_sparc.h"

#elif defined __CPU_arm || defined __CPU_arm6

#include "atomic_arm.h"

#elif defined __CPU_alpha

#include "atomic_alpha.h"

#endif /* __CPU_xxx  => no known cpu */

#endif /* CC_GCC_LIKE_ASM */


#endif
