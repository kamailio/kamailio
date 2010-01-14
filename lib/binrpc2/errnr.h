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


#ifndef __BRPC_ERRNR_H__
#define __BRPC_ERRNR_H__

#ifdef _LIBBINRPC_BUILD

#include <limits.h>
#include <errno.h>

#include "tls.h"

#ifndef EBADMSG
#define	EBADMSG		0	/* Not a data message */
#endif /* EBADMSG */
#ifndef EPROTO
#define EPROTO		0	/* Protocol error */
#endif /* EPROTO */

#define ELOCK	(INT_MAX - 1)
#define ERESLV	(INT_MAX - 2)
#define EFMT	(INT_MAX - 3)

#ifndef NDEBUG

extern __brpc_tls char *brpc_efile;
extern __brpc_tls int brpc_eline;

#define WERRNO(_errno_) \
	do { \
		brpc_errno = _errno_; \
		brpc_efile = __FILE__; \
		brpc_eline = __LINE__; \
	} while (0)

#define WSYSERRNO	WERRNO(errno)

#else /* NDEBUG */

#define WERRNO(_errno_) \
		brpc_errno = _errno_

#define WSYSERRNO	WERRNO(errno)

#endif /* NDEBUG */

#endif /* _LIBBINRPC_BUILD */

extern __brpc_tls int brpc_errno;
char *brpc_strerror();


#endif /* __BRPC_ERRNR_H__ */
