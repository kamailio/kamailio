/* 
 * Copyright (C) 2010 iptelorg GmbH
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
 * \brief Kamailio core :: Shared memory initialization
 * \ingroup core
 * Module: \ref core
 */

#include "shm_init.h"
#include "mem/mem.h"
#include "globals.h"

static int shm_init = 0;


/** check if shm is initialized.
 * @return 1 if initialized, 0 if not
 */
int shm_initialized()
{
	return shm_init;
}



#ifdef SHM_MEM
/** init shm mem.
 * @return 0 on success, < 0 on error
 * it _must_ be called:
 *  - after the shm_mem_size is known
 *  - after shm_force_alloc is known (mlock_pages should happen at a later
 *     point so it's not yet needed here)
 *  - after the user is known (so that in the SYSV sems case the sems will
 *     have the correct euid)
 *  - before init_timer and init_tcp
 * --andrei
 *
 * Global vars used: shm_mem_size, shm_force_alloc, user & uid.
 * Side effects: it might set uid, gid and shm_mem_size.
 */
int init_shm()
{
	/* set uid if user is set */
	if (user && uid == 0){
		if (user2uid(&uid, &gid, user)<0){
			fprintf(stderr, "bad user name/uid number: -u %s\n", user);
			goto error;
		}
	}
	if (shm_mem_size == 0)
		shm_mem_size=SHM_MEM_SIZE * 1024 * 1024;
	if (init_shm_mallocs(shm_force_alloc)==-1)
		goto error;
	shm_init=1;
	return 0;
error:
	return -1;
}
#endif /* SHM_MEM */

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
