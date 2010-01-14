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

#ifndef __BINRPC_LOCK_H__
#define __BINRPC_LOCK_H__

typedef void* brpc_lock_t;

/**
 * Primitive type to alloc&init a new lock. 
 * @return The lock, in state unlocked, or NULL on error.
 */
typedef brpc_lock_t *(*brpc_lock_new_f)(void);
/**
 * Primitive type to acquire a lock.
 * @return 0 for success, negative otherwise.
 */
typedef int (*brpc_lock_get_f)(brpc_lock_t *);
/**
 * Primitive type to release a lock.
 * @return 0 for success, negative otherwise.
 */
typedef int (*brpc_lock_let_f)(brpc_lock_t *);
/**
 * Primitive type to release any lock resources.
 * @return 0 for success, negative otherwise.
 */
typedef int (*brpc_lock_del_f)(brpc_lock_t *);
/**
 * Primitive type to release 
 */

enum BINRPC_LOCKING_MODEL {
	BRPC_LOCK_PROCESS,
	BRPC_LOCK_THREAD,
};

/**
 * Register locking primitives.
 * Default is RT library locking.
 * @param locksz Number of bytes (including possible allignment padding) used
 * to store a lock object in memory.
 */
void brpc_locking_setup(
		brpc_lock_new_f n,
		brpc_lock_get_f a,
		brpc_lock_let_f r,
		brpc_lock_del_f d);

/**
 * Set the locking model (inter-thread/inter-process).
 * The default is inter-process.
 * The call only makes sense if default (RT library) locking is used.
 */
void brpc_locking_model(enum BINRPC_LOCKING_MODEL model);


#ifdef _LIBBINRPC_BUILD

extern brpc_lock_new_f brpc_lock_new;
extern brpc_lock_get_f brpc_lock_get;
extern brpc_lock_let_f brpc_lock_let;
extern brpc_lock_del_f brpc_lock_del;

#endif /* _LIBBINRPC_BUILD */


#endif /* __BINRPC_LOCK_H__ */
