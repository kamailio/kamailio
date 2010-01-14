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


#include <string.h>
#include <stdio.h>
#include "errnr.h"


__brpc_tls int brpc_errno = 0;

#define ERR_BUFF_SIZE	1024
#define ERR_UNKNOWN		"(unknown error)"

#ifndef NDEBUG
__brpc_tls char *brpc_efile = __FILE__;
__brpc_tls int brpc_eline = 0;
#endif /* NDEBUG */

char *brpc_strerror()
{
	static __brpc_tls char buff[ERR_BUFF_SIZE];
#ifdef _BINRPC_REENTRANT
	static __brpc_tls char strerrbuff[ERR_BUFF_SIZE];
#endif
	char *msg;
	switch (brpc_errno) {
		case ELOCK: msg = "Locking subsystem error"; break;
		case ERESLV: msg = "DNS resolution failure"; break;
		case EFMT: msg = "Descriptor - structure missmatch"; break;
#ifdef _BINRPC_REENTRANT
		default: 
			if (strerror_r(brpc_errno, strerrbuff, sizeof(ERR_BUFF_SIZE)))
				return ERR_UNKNOWN;
			else
				msg = strerrbuff;
			break;
#else
		default: 
			if (! (msg = strerror(brpc_errno)))
				return ERR_UNKNOWN; 
			break;
#endif
	}
#ifndef NDEBUG
	if (snprintf(buff, ERR_BUFF_SIZE, "%s [%s:%d]", msg, brpc_efile, 
			brpc_eline) < 0)
		return ERR_UNKNOWN;
#else
	if (snprintf(buff, ERR_BUFF_SIZE, "%s", msg) < 0)
		return ERR_UNKNOWN;
#endif
	return buff;
}

