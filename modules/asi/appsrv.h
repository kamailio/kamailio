/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
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

#ifndef __ASI_AS_H__
#define __ASI_AS_H__

#include <stdlib.h> /*size_t*/

#include "str.h"
#include "locking.h"
#include "parser/msg_parser.h"

#include "digest.h"

#ifdef ASI_WITH_RESYNC
/* max Bit Mask Block cells count (x8 = max proc count) */
#define ASI_BMB_MAX		16
#endif

typedef struct {
	str name; /* points to configuration data */
	int id; /* used to dispatch timeout notifications */
	brpc_addr_t addr; /* ... where the AS is listening. */

	int sockfd; /* comm. with the AS */
#ifdef ASI_WITH_LOCDGRAM
	brpc_addr_t listen; /* if SOCK_DGRAM+PF_LOCAL, listen here from AS rpls */
#endif

	/**
	 * Note: the digests related members could be shm'ized as well (maybe the
	 * whole structure), but AVPs&XLL prepared structures can not be easily
	 * moved into SHM (some nastier deep copies are needed).
	 */
	int serial; /* timestamp to 'sign' digest methods validity */
	meth_dig_t *sipmeth; /* digests for each supported SIP method */
	size_t methcnt; /* size of sipmeth array */

	/* shm'ized */
#ifdef ASI_WITH_RESYNC
	/* bit mask of flags, 1/proc; if set, a new handshake must be performed */
	uint8_t *resync;
	/* mutex to protect rsync access */
	gen_lock_t *resync_mutex;
	int *resync_serial;
#endif
} as_t;


extern int ct_timeout;
extern int tx_timeout;
extern int rx_timeout;

extern int expect_reply;
#ifdef ASI_WITH_LOCDGRAM
extern char *usock_template;
extern int usock_mode;
extern int usock_uid;
extern int usock_gid;
#endif


void init_appsrv_proc(int rank, brpc_id_t callid);
size_t appsrvs_count(void);
int new_appsrv(char *name, char *uri);
void free_appsrvs(void);
as_t *as4id(unsigned int id);
int handshake_appsrv(as_t *as);
void disconnect_appsrv(as_t *as);
int dispatch(struct sip_msg *msg, as_t *as, void *_);
int dispatch_rebuild(struct sip_msg *sipmsg, as_t *as, void *_);
int dispatch_tm_reply(int as_id, struct sip_msg *sipmsg, 
		str *method, enum ASI_DSGT_IDS discr, /*usefull for FAKED_REPLY */
		str *tid, str *opaque);
#ifdef ASI_WITH_RESYNC
int setup_bmb(int max_procs);
int appsrv_set_resync(char *as_uri, int serial, int *as_id);
#endif

#define ASI_VERSION			0x2

enum ASI_STATUS_CODES {
	ASI_EFAILED		= -2,
	ASI_ENOMATCH	= -1,
	/* 0: reserved (halts script) */
	ASI_ESUCCESS	= 1,
};

#define DEFAULT_CT_TIMEOUT		5*1000
#define DEFAULT_TX_TIMEOUT		30*1000 /*30ms*/
#define DEFAULT_RX_TIMEOUT		40*1000 /*40ms*/

#endif /* __ASI_AS_H__ */
