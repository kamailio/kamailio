/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of the BinRPC Library (libbinrpc).
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */

#ifndef LOCKS_INIT_NOP
#include <semaphore.h>
#endif

#include "mem.h"
#include "errnr.h"
#include "lock.h"

/* forward declaration */
static brpc_lock_t *_brpc_lock_new(void);
static int _brpc_loc_del(brpc_lock_t *lock);
#ifdef LOCKS_INIT_NOP
static int _brpc_lock_get(brpc_lock_t *lock);
static int _brpc_lock_let(brpc_lock_t *lock);
#endif /* LOCKS_INIT_NOP */

static int locking_model = BRPC_LOCK_PROCESS;

brpc_lock_new_f brpc_lock_new = _brpc_lock_new;
#ifndef LOCKS_INIT_NOP
brpc_lock_get_f brpc_lock_get = (brpc_lock_get_f)sem_wait;
brpc_lock_let_f brpc_lock_let = (brpc_lock_let_f)sem_post;
#else /* LOCKS_INIT_NOP */
brpc_lock_get_f brpc_lock_get = _brpc_lock_get;
brpc_lock_let_f brpc_lock_let = _brpc_lock_let;
#endif /* LOCKS_INIT_NOP */
brpc_lock_del_f brpc_lock_del = _brpc_loc_del;


void brpc_locking_setup(
		brpc_lock_new_f n,
		brpc_lock_get_f a,
		brpc_lock_let_f r,
		brpc_lock_del_f d)
{
	brpc_lock_new = n;
	brpc_lock_get = a;
	brpc_lock_let = r;
	brpc_lock_del = d;
}

void brpc_locking_model(enum BINRPC_LOCKING_MODEL model)
{
	locking_model = model;
}

#ifndef LOCKS_INIT_NOP
static brpc_lock_t *_brpc_lock_new(void)
{
	brpc_lock_t *lock;
	lock = brpc_calloc(1, sizeof(sem_t));
	if (! lock) {
		WERRNO(ENOMEM);
		return NULL;
	}
	if (sem_init((sem_t *)lock, locking_model == BRPC_LOCK_PROCESS, 
			/* semaphore unlocked */1) == -1) {
		WSYSERRNO;
		goto error;
	}
	
	return lock;
error:
	brpc_free(lock);
	return NULL;
}

static int _brpc_loc_del(brpc_lock_t *lock)
{
	if (! lock) {
		WERRNO(EINVAL);
		return -1;
	}
	if (sem_destroy((sem_t *)lock) == -1) {
		WSYSERRNO;
		return -1;
	}
	brpc_free(lock);
	return 0;
}

#else /* LOCKS_INIT_NOP */


static brpc_lock_t *_brpc_lock_new(void) { return (brpc_lock_t *)-1; }
static int _brpc_lock_get(brpc_lock_t *lock) { return 0; }
static int _brpc_lock_let(brpc_lock_t *lock) { return 0; }
static int _brpc_loc_del(brpc_lock_t *lock) { return 0; }

#endif /* LOCKS_INIT_NOP */

