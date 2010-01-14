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


#ifndef __BRPC_LOG_H__
#define __BRPC_LOG_H__


#ifdef _LIBBINRPC_BUILD

#include <syslog.h>
#include "config.h"

#define LOG(priority, msg, args...) \
		brpc_syslog(priority, msg, ##args)

#define _STRINGIFY(i) #i
#define STRINGIFY(i) _STRINGIFY(i)

#define REL_REF \
		"[" BINRPC_LIB_VER "]: "
#define CODE_REF \
		"[" __FILE__ ":" STRINGIFY(__LINE__) "]: "

#define LOG_REF	CODE_REF

#define ERR(msg, args...)	LOG(LOG_ERR, "ERROR " LOG_REF msg, ##args)
#define WARN(msg, args...)	LOG(LOG_WARNING, "WARNING " LOG_REF msg, ##args)
#define INFO(msg, args...)	LOG(LOG_INFO, "INFO " LOG_REF msg, ##args)
#define BUG(msg, args...)	ERR("### BUG ### " msg, ##args)

#if defined NDEBUG
#define DBG(msg, args...)	/* no debug */
#else
#define DBG(msg, args...)	\
		LOG(LOG_DEBUG, "--- debug --- %s" CODE_REF msg, __FUNCTION__, ##args)
#endif /* NDEBUG */


void (*brpc_syslog)(int priority, const char *format, ...);

#endif /* _LIBBINRPC_BUILD */


/**
 * API calls.
 */

void brpc_log_setup(void (*s)(int , const char *, ...));

#endif /* __BRPC_LOG_H__ */

