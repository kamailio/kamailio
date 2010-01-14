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


#ifndef __BRPC_CONFIG_H__
#define __BRPC_CONFIG_H__

/* maximum supported size of a packet */
#define BINRPC_MAX_PKT_LEN	((1<<16) - 1)
/* 'standard' BINRPC port */
#define BINRPC_PORT			2046


#ifdef _LIBBINRPC_BUILD

/* (initial) lenght for buffer representation */
#define BINRPC_MAX_REPR_LEN	256

/* dissector's initial stack size */
#define BINRPC_DISSECTOR_SSZ	8

#ifndef BINRPC_LIB_VER
#define BINRPC_LIB_VER "?.?.?"
#warning "unknown version numbers"
#endif /* BINRPC_LIB_VER */

#endif /* _LIBBINRPC_BUILD */


#endif /* __BRPC_CONFIG_H__ */
