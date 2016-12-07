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
 * \brief Kamailio core ::  lock operations init
 * \ingroup core
 *
 * Module: \ref core
 *
 * Reference:
 * - \ref LockingDoc
 */

/*!
 * \page LockingDoc Documentation of locking
 * \verbinclude locking.txt
 *
 */



#include "ut.h"
#include "dprint.h"
#include "lock_ops.h"

/* returns 0 on success, -1 on error */
int init_lock_ops(void)
{
#ifdef USE_FUTEX
	int os_ver;
	
	os_ver=get_sys_version(0, 0, 0);
	if (os_ver < 0x020546 ){ /* if ver < 2.5.70 */
		LM_CRIT("old kernel: compiled with FUTEX support which is not"
				" present in the running kernel (try  2.6+)\n");
		return -1;
	}
#endif
	return 0;
}



void destroy_lock_ops(void)
{
}
