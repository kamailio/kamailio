/*
 * Copyright (C) 2007 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio core :: Compiler specific optimizations
 * \see \ref CompilerOptions
 * \auth Andrei
 *
 * \ingroup core
 * Module: \ref core
 *
 * \page CompilerOptions compiler specific optimizations:
 *
\verbatim
 *   - likely(expr)         - branch predicition optimization - is more likely
 *                          that expr value will be 1 so optimize for this 
 *                          case.
 *                          Example: if (likely(p!=NULL)) {... }
 *   - unlikely(expr)       - branch prediction optimization - is unlikely that 
 *                          expr will be true, so optimize for this case
 *   - prefetch(addr)        - will prefetch addr. for reading
 *   - prefetch_w(addr)      - will prefetch addr. for writing
 *   - prefetch_loc_r(addr, loc) - prefetch for reading, data at addr has
 *                                no temporal locality (loc==0), a short
 *                                degree of temporal locality (loc==1), 
 *                                moderate (loc==2) or high (loc==3).
 *                                prefetch(addr) is equiv. to 
 *                                prefetch_loc_r(addr, 3).
 *  prefetch_loc_w(addr, loc) - like above but for writing.
\endverbatim
 */

#ifndef __compiler_opt_h
#define __compiler_opt_h

/* likely/unlikely */
#if __GNUC__ >= 3

#define likely(expr)              __builtin_expect(!!(expr), 1)
#define unlikely(expr)            __builtin_expect(!!(expr), 0)

#else /* __GNUC__ */

/* #warning "No compiler optimizations supported try gcc 4.x" */
#define likely(expr) (expr)
#define unlikely(expr) (expr)

#endif /* __GNUC__ */



/* prefetch* */
#if ( __GNUC__ > 3 ) || ( __GNUC__ == 3 && __GNUC_MINOR__ >= 1 )

#define prefetch(addr)            __builtin_prefetch((addr))
#define prefetch_w(addr)          __builtin_prefetch((addr), 1)
#define prefetch_loc_r(addr, loc) __builtin_prefetch((addr), 0, (loc))
#define prefetch_loc_w(addr, loc) __builtin_prefetch((addr), 1, (loc))

#else

#define prefetch(addr)
#define prefetch_w(addr)
#define prefetch_loc_r(addr, loc)
#define prefetch_loc_w(addr, loc)

#endif /* __GNUC__ > 3  || ( __GNUC__ == 3 && __GNUC_MINOR__ >= 1 ) */

#endif
