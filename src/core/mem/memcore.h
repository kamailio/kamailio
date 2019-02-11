/*
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
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


#ifndef _sr_memcore_h_
#define _sr_memcore_h_

/* memory managers implemented in core */

#ifdef F_MALLOC
/* fast malloc - implemented in f_malloc.c */
int fm_malloc_init_pkg_manager(void);
int fm_malloc_init_shm_manager(void);
#endif

#ifdef Q_MALLOC
/* quick malloc - implemented in q_malloc.c */
int qm_malloc_init_pkg_manager(void);
int qm_malloc_init_shm_manager(void);
#endif

#ifdef TLSF_MALLOC
/* two levels segregated fit - implemented in tlsf_malloc.c */
int tlsf_malloc_init_pkg_manager(void);
int tlsf_malloc_init_shm_manager(void);
#endif

#endif
