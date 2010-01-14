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

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h> /* PF_, SOCK_ */
#ifdef ASI_WITH_LOCDGRAM
#	include <stdlib.h> /* dirname */
#	include <stdio.h> /* tempnam */
#	include <sys/stat.h> /* chmod, chown */
#	include <sys/un.h> /* UNIX_PATH_MAX */
#	include <unistd.h> /* unlink */
#endif
#ifdef EXTRA_DEBUG
#	include <assert.h>
#endif /* EXTRA_DEBUG */
#include <binrpc.h>
#include "mem/mem.h"

#include "strutil.h"
#include "tid.h"
#include "binds.h"
#include "appsrv.h"


#define MAX_SIP_METHODS	32

#ifdef ASI_WITH_LOCDGRAM
#include <ut.h>
#define DEFAULT_LOCDGRAM_TMPL	"/tmp/ser.asi.usock.XXXXXX"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX sizeof(((struct sockaddr_un *)0)->sun_path)
#endif /*UNIX_PATH_MAX*/
#endif /*ASI_WITH_LOCDGRAM*/


#define OVER_STREAM(_as_)	(BRPC_ADDR_TYPE(&(_as_)->addr) == SOCK_STREAM)
#define OVER_LOCDGRAM(_as_)	\
		((BRPC_ADDR_DOMAIN(&(_as_)->addr) == PF_LOCAL) \
		&& (BRPC_ADDR_TYPE(&(_as_)->addr) == SOCK_DGRAM))


const BRPC_STR_STATIC_INIT(METH_SYNC, "asi.sync");
const BRPC_STR_STATIC_INIT(METH_METHODS, "methods");
const BRPC_STR_STATIC_INIT(METH_DIGESTS, "digests");

int ct_timeout = -1;
int tx_timeout = -1;
int rx_timeout = -1;

static as_t **app_srvs = NULL; /* list of ASes */
static size_t as_cnt = 0; /* size of app_srvs array */
/* cookie counter for each BINRPC message */
static brpc_id_t brpc_callid;

#define GET_AS_CNT			as_cnt
#define INC_AS_CNT			(++ as_cnt)
#define AS_RS_SERIAL(_as_)	(*(_as_)->resync_serial)

#ifdef ASI_WITH_LOCDGRAM
/* create temporary files using this prefix (dir+file_prefix) */
char *usock_template = DEFAULT_LOCDGRAM_TMPL;
int usock_mode = 0;
int usock_uid = (uid_t)-1;
int usock_gid = (gid_t)-1;
#endif

#ifdef ASI_WITH_RESYNC
/* current process's rank */
static int my_rank;
/* proc Bit Mask Block count: how many bytes needed, 1 bit/process */
static size_t *proc_bmb_cnt;
#endif /* ASI_WITH_RESYNC */

/* do we wait read what AS responds with to a digest? */
int expect_reply = 0;
#ifdef EXTRA_DEBUG
/* make sure t_get() is called after t_get_trans_ident(), to ensure the right
 * T is hooked up. */
static int t_looked_up = 0;
#endif


static inline int dispatch2as(struct sip_msg *sipmsg, as_t *as);


#ifdef ASI_WITH_RESYNC
int setup_bmb(int max_procs)
{
	int i;

	if (! (proc_bmb_cnt = (size_t *)shm_malloc(sizeof(size_t)))) {
		ERR("out of shm mem (proc_bmb_cnt).\n");
		return -1;
	}

	for (i = 1; i * 8 < max_procs; i ++)
		;
	*proc_bmb_cnt = i;
	DEBUG("resync bit mask byte size: %zd.\n", *proc_bmb_cnt);

	if (ASI_BMB_MAX * 8 < *proc_bmb_cnt) {
		BUG("registered processes number (%d) larger than supported by"
				" ASI (%d): increase ASI_MAX_PROCS and recompile.\n", 
				max_procs, ASI_BMB_MAX * 8);
		return -1;
	}
	return 0;
}

static void free_bmb(void)
{
	if (proc_bmb_cnt) {
		shm_free(proc_bmb_cnt);
		proc_bmb_cnt = NULL;
	}
}
#endif

void init_appsrv_proc(int rank, brpc_id_t callid)
{
#ifdef ASI_WITH_RESYNC
	my_rank = rank;
#endif /* ASI_WITH_RESYNC */
	brpc_callid = callid;
}

size_t appsrvs_count(void)
{
	return GET_AS_CNT;
}

static void free_appsrv(as_t *as)
{
	if (! as)
		return;
#ifdef ASI_WITH_RESYNC
	if (as->resync)
		shm_free(as->resync);
	if (as->resync_mutex) {
		lock_destroy(as->resync_mutex);
		shm_free((void *)as->resync_mutex);
	}
	if (as->resync_serial)
		shm_free(as->resync_serial);
#endif
	pkg_free(as);
}

int new_appsrv(char *name, char *uri)
{
	as_t *as, **as_tmp;
	brpc_addr_t *addr;
	str name_str;
	int i;

	name_str.s = name;
	name_str.len = strlen(name);

	if (! (addr = brpc_parse_uri(uri))) {
		ERR("failed to parse BINRPC URI `%s': %s [%d].\n", uri, 
				brpc_strerror(), brpc_errno);
		return -1;
	}
	/* check against name collision or address reuse */
	for (i = 0; i < GET_AS_CNT; i ++) {
		if (STR_EQ(name_str, app_srvs[i]->name)) {
			ERR("AS name `%s' alread in use.\n", name);
			return -1;
		}
		if (brpc_addr_eq(addr, &app_srvs[i]->addr)) {
			ERR("ASes '%s' and '%s' share same '%s' address.\n", 
					app_srvs[i]->name.s, name, uri);
			return -1;
		}
	}

	if (! (as = (as_t *)pkg_malloc(sizeof(as_t)))) {
		ERR("out of pkg mem (as).\n");
		return -1;
	}
	memset(as, 0, sizeof(sizeof(as_t)));

#ifdef ASI_WITH_RESYNC
	if (! ((as->resync = (uint8_t *)shm_malloc(sizeof(uint8_t) * ASI_BMB_MAX))
			&& (as->resync_mutex = 
					(gen_lock_t *)shm_malloc(sizeof(gen_lock_t)))
			&& (as->resync_serial = (int *)shm_malloc(sizeof(int))) ) ) {
		ERR("out of shm mem (resyncs).\n");
		goto error;
	}
	memset(as->resync, 0, sizeof(uint8_t) * ASI_BMB_MAX);
	if (! lock_init(as->resync_mutex)) {
		ERR("failed to initialize resync_mutex: %s (%d).\n", strerror(errno), 
				errno);
		goto error;
	}
	AS_RS_SERIAL(as) = -1;
#endif

	as->name = name_str;
	as->id = GET_AS_CNT;
	as->addr = *addr;
	as->sockfd = -1;
	as->serial = -1;

	if (! (as_tmp = (as_t **)pkg_realloc(app_srvs, (GET_AS_CNT + 1) * 
			sizeof(as_t *)))) {
		ERR("out of pkg memory (realloc AS array).\n");
		goto error;
	} else {
		app_srvs = as_tmp;
	}
	app_srvs[GET_AS_CNT] = as;
	INC_AS_CNT;

	return GET_AS_CNT;
error:
	free_appsrv(as);
	return -1;
}

void free_appsrvs(void)
{
	int i;
	for (i = 0; i < GET_AS_CNT; i ++)
		free_appsrv(app_srvs[i]);
	if (app_srvs)
		pkg_free(app_srvs);
#ifdef ASI_WITH_RESYNC
	free_bmb();
#endif
}

as_t *as4id(unsigned int id)
{
	if ((id < 0) || (GET_AS_CNT <= id)) {
		ERR("AS index (%u) out of bounds (0:%zd).\n", id, GET_AS_CNT);
#ifdef EXTRA_DEBUG
		abort();
#endif
		return NULL;
	}
	return app_srvs[id];
}


#ifdef ASI_WITH_LOCDGRAM
/**
 * Set modes&permissions for a unix socket file.
 */
static int usock_stat(char *path)
{
	if (usock_mode) {
		if (chmod(path, usock_mode)<0){
			ERR("failed to change the permissions for '%s' to %04o: %s[%d]\n",
					path, usock_mode, strerror(errno), errno);
			return -1;
		}
	}
	/* XXX: still needed? */
	if ((0 <= usock_uid) || (0 <= usock_gid)) {
		if (chown(path, usock_uid, usock_gid)<0){
			ERR("failed to change the owner/group for '%s' to %d.%d: %s[%d]\n",
					path, usock_uid, usock_gid, strerror(errno), errno);
			return -1;
		}
	}
	return 0;
}
/**
 * Initialize a local domain unix socket address to bind to for AS'es response.
 */
static int init_local_dgram(as_t *as)
{
	size_t tmp_len;
	static char tmppath[UNIX_PATH_MAX];
	int tmpfd;

	tmp_len = strlen(usock_template) + /*0-term*/1;
	if (sizeof(tmppath) < tmp_len) {
		ERR("local unix socket template too long: %zd; maximum "
				"allowed: %zd.\n", tmp_len, sizeof(tmppath));
		return -1;
	}
	memcpy(tmppath, usock_template, tmp_len);
	if ((tmpfd = mkstemp(tmppath)) < 0) {
		ERR("failed to create temporary file for local unix "
				"socket: %s [%d].\n", strerror(errno), errno);
		return -1;
	}
	if (close(tmpfd) < 0)
		WARN("failed to close temporary file descriptor: %s [%d].\n",
				strerror(errno), errno);

	as->listen = as->addr; /* copy domain & type */
	memcpy(BRPC_ADDR_UN(&as->listen)->sun_path, tmppath, tmp_len);
	BRPC_ADDR_LEN(&as->listen) = SUN_LEN(BRPC_ADDR_UN(&as->listen));
	DEBUG("init'ed local dgram socket `%.*s'.\n", BRPC_ADDR_LEN(&as->listen),
			BRPC_ADDR_UN(&as->listen)->sun_path);

	return 0;
}

static void del_local_dgram(as_t *as)
{
	if (! OVER_LOCDGRAM(as))
		return;
	if (! BRPC_ADDR_LEN(&as->listen))
		return;
	if (unlink(BRPC_ADDR_UN(&as->listen)->sun_path) < 0)
		WARN("failed to remove LOCAL unix socket `%s': %s [%d].\n", 
				BRPC_ADDR_UN(&as->listen)->sun_path, strerror(errno), 
				errno);
	BRPC_ADDR_LEN(&as->listen) = 0;
}
#endif


void disconnect_appsrv(as_t *as)
{
	if (0 <= as->sockfd) {
		if (close(as->sockfd) < 0) {
			WARN("while breaking connection with AS %.*s: %s [%d].\n", 
					STR_FMT(&as->name), strerror(errno), errno);
		}
		as->sockfd = -1;
	}
#ifdef ASI_WITH_LOCDGRAM
	del_local_dgram(as);
#endif
	INFO("connection with AS %.*s closed.\n", STR_FMT(&as->name));
}

static int connect_appsrv(as_t *as)
{
	bool named;
	brpc_addr_t *addr;
	
	DEBUG("connecting to AS '%.*s' @ %s.\n", STR_FMT(&as->name), 
			brpc_print_addr(&as->addr));
	
	if (0 <= as->sockfd) {
		BUG("AS connection socket not closed (%d).\n", as->sockfd);
#ifdef EXTRA_DEBUG
		abort();
#else
		close(as->sockfd);
#endif
	}

#ifdef ASI_WITH_LOCDGRAM
	if (OVER_LOCDGRAM(as)) {
		/* create a (unique) socket (file) name. */
		if (init_local_dgram(as) < 0) {
			ERR("failed to bind local unix socket.\n");
			return -1;
		}
		addr = &as->listen;
		named = true;
	}
	else
#endif
	{
		addr = &as->addr;
		named = false;
	}

	if ((as->sockfd = brpc_socket(addr, /*block.*/false, named)) < 0) {
		ERR("failed to get new socket: %s [%d].\n", brpc_strerror(), 
				brpc_errno);
	}
#ifdef ASI_WITH_LOCDGRAM
	if (OVER_LOCDGRAM(as)) {
		/* change mode&ownership */
		if (usock_stat(BRPC_ADDR_UN(addr)->sun_path) < 0) {
			ERR("failed to updated local unix socket stats: %s [%d].\n",
					strerror(errno), errno);
			goto discon;
		}
	}
#endif
	/* init connection */
	if (! brpc_connect(&as->addr, &as->sockfd, ct_timeout)) {
		ERR("failed to connect to AS: %s [%d].\n", brpc_strerror(),
				brpc_errno);
		goto discon;
	}

	return 0;
discon:
	disconnect_appsrv(as);
	return -1;
}

int is_connected(as_t *as)
{
#ifdef ASI_WITH_RESYNC
	int update;

	if (as->resync[my_rank >> 3] & (1 << (my_rank & 0x07))) {
		DEBUG("resync flag found set for AS '%.*s'.\n", STR_FMT(&as->name));
		/* prevent racing on resync_serial (update resync [appsrv_set_resync],
		 * check resync [is_connected], check resync_serial[is_connected],
		 * clear resync [is_connected], update resync_serial 
		 * [appsrv_set_resync]) */
		lock_get(as->resync_mutex);
		if (as->serial < AS_RS_SERIAL(as)) {
			DEBUG("SIP worker #%d must resync AS '%.*s' connection.\n", 
					my_rank, STR_FMT(&as->name));
			update = 1;
		} else {
			update = 0;
		}
		as->resync[my_rank >> 3] &= ~ (1 << (my_rank & 0x7));
		lock_release(as->resync_mutex);
		if (update) {
			disconnect_appsrv(as);
			return handshake_appsrv(as);
		}
	}
#endif

	/* TODO: for streams:
	 * - implemente a try-reconnect-every-SOMETHING [nr_msgs|time]  
	 * - try select+read to see if RST (?)
	 */

	if (0 <= as->sockfd) {
		DEBUG("AS '%.*s' is connected.\n", STR_FMT(&as->name));
		return 1;
	} else {
		DEBUG("AS '%.*s' not yet connected.\n", STR_FMT(&as->name));
	}
	return handshake_appsrv(as);
}



static inline int rpc_faulted(brpc_t *rpl)
{
	brpc_int_t *code;
	brpc_str_t *reason;

	if (! brpc_is_fault(rpl))
		return 0;
	
	if (! brpc_fault_status(rpl, &code, &reason)) {
		ERR("failed to extract RPC failure reason: %s [%d].\n", 
				brpc_strerror(), brpc_errno);
		goto end;
	}

	ERR("RPC call ID#%d faulted.\n", brpc_id(rpl));
	if (code)
		ERR("RPC ID#%d failure code: %d.\n", brpc_id(rpl), *code);
	if (reason)
		ERR("RPC ID#%d failure reason: %.*s.\n", brpc_id(rpl), 
				BRPC_STR_FMT(reason));
end:
	return 1;
}

static int query_methods(as_t *as)
{
	brpc_t *req = NULL, *rpl = NULL;
	char meth_desc[MAX_SIP_METHODS + 1];
	brpc_str_t *meth[MAX_SIP_METHODS + 1];
	brpc_addr_t from;
	size_t cnt;
	int ret = -1;

	DEBUG("quering for ASI version number.\n");
	
	if (! (	(req = brpc_req(METH_METHODS, brpc_callid ++)) &&
			brpc_send(as->sockfd, req, tx_timeout))) {
		ERR("failed to send request: %s [%d].\n", brpc_strerror(), brpc_errno);
		goto end;
	}
	from = as->addr;
	if (! (	(rpl = brpc_recvfrom(as->sockfd, &from, rx_timeout)) &&
			(! rpc_faulted(rpl)))) {
		ERR("failed to get methods: %s [%d].\n", brpc_strerror(), brpc_errno);
		goto end;
	}

	cnt = brpc_val_cnt(rpl);
	if (MAX_SIP_METHODS + 1 <= cnt) {
		ERR("not enough space to get SIP methods (need %zd, have %d).\n",
				cnt + 2, MAX_SIP_METHODS + 1);
		goto end;
	} else if (cnt == 0) {
		WARN("AS '%.*s' supports no SIP method.\n", STR_FMT(&as->name));
		goto end;
	} else {
		DEBUG("# of supported SIP methods: %zd.\n", cnt);
	}
	memset(meth_desc + /*first: `!'*/1, 's', cnt);
	meth_desc[0] = '!';
	meth_desc[1 + cnt] = 0;

	if (! brpc_dsm(rpl, meth_desc, meth)) {
		ERR("failed to retrieve SIP methods from reply: %s [%d].\n",
				brpc_strerror(), brpc_errno);
		goto end;
	}

	if (! (as->sipmeth = meth_array_new(meth, cnt)))
		goto end;
	as->methcnt = cnt;
	ret = cnt;
end:
	if (req)
		brpc_finish(req);
	if (rpl)
		brpc_finish(rpl);
	return ret;
}


static int digest_for(int sockfd, brpc_addr_t *from, meth_dig_t *meth)
{
	brpc_t *req = NULL, *rpl = NULL;
	brpc_val_t *val;
	int i;
	static const char desc[] = "!{<sL><sL><sL>}";
	void *vals[6];
	brpc_str_t *ident;
	brpc_val_t *array;
	int ret = -1;
	

	DEBUG("querying digests for: %.*s.\n", STR_FMT(&meth->name));
	if (! (	(req = brpc_req(METH_DIGESTS, brpc_callid ++)) &&
			(val = brpc_str(meth->name.s, meth->name.len)) &&
			brpc_add_val(req, val) &&
			brpc_send(sockfd, req, tx_timeout))) {
		ERR("failed to send query digests: %s [%d].\n", brpc_strerror(),
				brpc_errno);
		goto end;
	}
	if (! (	(rpl = brpc_recvfrom(sockfd, from, rx_timeout)) &&
			(! rpc_faulted(rpl)) &&
			brpc_dsm(rpl, desc, vals))) {
		ERR("failed to retrieve message digest': %s [%d].\n", brpc_strerror(),
				brpc_errno);
		goto end;
	}

	for (i = 0; i < /*req, fin, prov*/3; i ++) {
		ident = (brpc_str_t *)vals[2 * i];
		array = (brpc_val_t *)vals[2 * i + 1];
		if (meth_add_digest(meth, ident, array) < 0)
			goto end;
	}

	ret = 0;
end:
	if (req)
		brpc_finish(req);
	/* WARN: brpc_add_val() is safe only for single use of `val' var. */
	if (rpl)
		brpc_finish(rpl);
	return ret;
}

static inline int query_digests(as_t *as)
{
	int i, j;
	brpc_addr_t from;

	for (i = 0; i < as->methcnt; i ++) {
		from = as->addr;
		if (digest_for(as->sockfd, &from, &as->sipmeth[i])<0) {
			ERR("failed to get digests for method `%.*s'.\n", 
					STR_FMT(&as->sipmeth[i].name));
			return -1;
		}
		/* avoid having one message registered twice */
		for (j = 0; j < i; j ++) {
			if (STR_EQ(as->sipmeth[i].name, as->sipmeth[j].name)) {
				ERR("method '%.*s' apears twice in supported methods list.\n",
						STR_FMT(&as->sipmeth[i].name));
				return -1;
			}
		}
	}
	return 0;
}

static int asi_sync(as_t *as)
{
	brpc_t *req = NULL, *rpl = NULL;
	int *serial, *proto, ser = -1;
	brpc_addr_t from;

	if (! ((req = brpc_req(METH_SYNC, brpc_callid ++)) &&
			brpc_asm(req, "dd", ASI_VERSION, as->id))) {
		ERR("failed to build BINRPC context: %s (%d).\n", brpc_strerror(),
				brpc_errno);
		goto end;
	}

	from = as->addr;
	if (! (brpc_send(as->sockfd, req, tx_timeout) &&
			(rpl = brpc_recvfrom(as->sockfd, &from, rx_timeout)))) {
		ERR("BINRPC message xchange failed: %s (%d).\n", brpc_strerror(),
				brpc_errno);
		goto end;
	}

	if (rpc_faulted(rpl))
		goto end;

	if (! (brpc_dsm(rpl, "dd", &proto, &serial) && proto && serial)) {
		ERR("invlaid reply received (%s [%d]).\n", brpc_strerror(), 
				brpc_errno);
		goto end;
	}

	if (ASI_VERSION < *proto) {
		ERR("unsupported ASI version: %d.\n", *proto);
		goto end;
	} else {
		INFO("AS '%.*s' speaks ASI#%x; serial: %d.\n", STR_FMT(&as->name), 
				*proto, *serial);
	}

	ser = *serial;
end:
	if (req)
		brpc_finish(req);
	if (rpl)
		brpc_finish(rpl);
	return ser;
}

int handshake_appsrv(as_t *as)
{
	int serial;

	DEBUG("handshaking with AS '%.*s'.\n", STR_FMT(&as->name));
	if (connect_appsrv(as) < 0)
		return -1;

	if ((serial = asi_sync(as)) < 0) {
		goto discon;
	}

	if (as->serial < serial) {
		DEBUG("AS serial  updated (from %d to current %d); querying "
				"digests.\n", as->serial, serial);
		/* free previously obtained digest formats, if any */
		meth_array_free(as->sipmeth, as->methcnt);
		as->sipmeth = NULL;
		as->methcnt = 0;
		
		/* get supported SIP methods ... */
		if (query_methods(as) < 0) {
			ERR("failed to get supported SIP methods.\n");
			goto discon;
		}

		/* ... and digests for each */
		if (query_digests(as) < 0) {
			ERR("failed to get digests for SIP methods.\n");
			goto discon;
		}

		as->serial = serial;
	} else {
		INFO("AS '%.*s' did not restart since last handshake -> reusing"
				" digest formats.\n", STR_FMT(&as->name));
	}
#ifdef ASI_WITH_LOCDGRAM
	/* if AS is not supposed to sent back confirmations, SER can get rid of the
	 * named local dgram socket it binds to for replies */
	if (! expect_reply)
		del_local_dgram(as);
#endif

	/* all went fine */
	DEBUG("handshake with AS '%.*s' completed.\n", STR_FMT(&as->name));
	return 0;

discon:
	disconnect_appsrv(as);
	return -1;
}


int rpc_msg_xchg(brpc_t *rpcreq, as_t *as)
{
	brpc_int_t *reply;
	brpc_t *rpcrpl;
	brpc_addr_t from;
	int ret = ASI_EFAILED;

#ifdef EXTRA_DEBUG
	if (as->sockfd < 0) {
		BUG("trying to dispatch message to unconnected AS.\n");
		abort();
	}
#endif

	if (! brpc_send(as->sockfd, rpcreq, tx_timeout)) {
		ERR("failed to dispatch digest to AS '%.*s': %s [%d].\n", 
				STR_FMT(&as->name), brpc_strerror(), brpc_errno);
		switch (brpc_errno) {
			case ETIMEDOUT:
			case EMSGSIZE:
				WARN("nothign was sent, so ignoring send error.\n");
				break;
			default:
				ERR("aborting connection to '%.*s'.\n", STR_FMT(&as->name));
				disconnect_appsrv(as);
		}
	} else if (expect_reply) {
		from = as->addr;
		while ((rpcrpl = brpc_recvfrom(as->sockfd, &from, rx_timeout))) {
			if (rpcreq->id == rpcrpl->id)
				break;
			ERR("reply's ID (%u) doesn't match request's (%u) - discarded.\n",
					rpcrpl->id, rpcreq->id);
			brpc_finish(rpcrpl);
		}
		if ((! rpcrpl) || (! rpc_faulted(rpcrpl))) {
			ERR("failed to read response from AS '%.*s': %s [%d].\n",
					STR_FMT(&as->name), brpc_strerror(), brpc_errno);
			disconnect_appsrv(as);
		} else {
			if (! brpc_dsm(rpcrpl, "d", &reply)) {
				ERR("failed to interpret reply: %s [%d].\n", brpc_strerror(),
						brpc_errno);
			} else if (! reply) {
				WARN("NULL integer as reply stat: considering it failure.\n");
			} else {
				DEBUG("AS '%.*s' returned %d.\n", STR_FMT(&as->name), *reply);
				/**
				 * Temptation is to return *reply. But this would "inspire" AS
				 * to return a more meaningful code here, which would 
				 * potentially bring in higher response latency => bad for SER.
				 */
				ret = (*reply < 0) ? ASI_EFAILED : ASI_ESUCCESS;
			}
			
			brpc_finish(rpcrpl);
		}
	} else {
		ret = ASI_ESUCCESS;
	}

	DEBUG("BINRPC message %s AS '%.*s' finished with code %d.\n", 
			expect_reply ? "exchange with" : "sending to", STR_FMT(&as->name),
			ret);
	brpc_finish(rpcreq);
	return ret;
}


static void tm_callback(struct cell *trans, int type, struct tmcb_params *cbp)
{
	as_t *as;
	enum ASI_STATUS_CODES code;

#ifdef EXTRA_DEBUG
	assert(trans != T_NULL_CELL);
	assert(trans != T_UNDEFINED);
	assert(cbp);
	/* TODO: deceiving & inaccurate name for the ACK event */
	assert(((type & TMCB_ACK_NEG_IN) == TMCB_ACK_NEG_IN) || 
			((type & TMCB_E2ECANCEL_IN) == TMCB_E2ECANCEL_IN));
#endif
	DEBUG("ACK/CANCEL callback invoked with type %d.\n", type);

	if (type == TMCB_ACK_NEG_IN)
		if (300 <= trans->uas.status) {
			/* TODO: could this be a useful case?! */
			DEBUG("hbh ACK ignored.\n");
			return;
		}

	if (! (as = (as_t *)*cbp->param)) {
		DEBUG("ACK/CANCEL retransmission in callback.\n");
		return;
	}
	switch ((code = dispatch2as(cbp->req, as))) {
		default:
		case ASI_EFAILED:
			ERR("failed to dispatch TM callback message for request '%.*s' to"
					" AS '%.*s'.\n", STR_FMT(&REQ_LINE(cbp->req).method), 
					STR_FMT(&as->name));
			break;
		case ASI_ENOMATCH:
			DEBUG("failed to dispatch TM callback message to AS '%.*s': no"
					" digests for method '%.*s' registered.\n", 
					STR_FMT(&as->name), STR_FMT(&REQ_LINE(cbp->req).method));
			break;
		case ASI_ESUCCESS:
			DEBUG("successfully dispatched TM callback message '%.*s' to AS "
					"'%.*s'.\n", STR_FMT(&REQ_LINE(cbp->req).method), 
					STR_FMT(&as->name));
			break;
	}

	/* Minimize invokation times.
	 * (In case of simulaneous ACK arrivals,
	 * the callback could actually be invoked multiple times, quasisimult.) */
	*cbp->param = NULL; 
}

inline static str *get_tid(struct sip_msg *sipmsg)
{
	unsigned int h_index, h_label;
#ifdef EXTRA_DEBUG
	t_looked_up = 0;
#endif
#ifdef ORIG_TID_4CANCEL
	if (sipmsg->REQ_METHOD == METHOD_CANCEL) {
		if (tmb.t_get_canceled_ident(sipmsg, &h_index, &h_label) < 0) {
			DEBUG("no transaction for current CANCEL [%u] found.\n",
					sipmsg->id);
			return NULL;
		}
	}
	else
#endif /* ORIG_TID_4CANCEL */
	if (tmb.t_get_trans_ident(sipmsg, &h_index, &h_label) < 0) {
		DEBUG("no transaction pending for message %u.\n", sipmsg->id);
		return NULL;
	}
	DEBUG("TID for msg #%d: %u:%u\n", sipmsg->id, h_index, h_label);
#ifdef EXTRA_DEBUG
	t_looked_up = 1;
#endif
	return tid2str(h_index, h_label);
}

static inline int tm_hookup(struct sip_msg *sipmsg, as_t *as)
{
	if (sipmsg->REQ_METHOD != METHOD_INVITE)
		/* not applicable */
		return 0;

#ifdef EXTRA_DEBUG
	/* FIXME: either abort when no T is found, or allow t_looked_up be 0! */
	if (! t_looked_up) {
		BUG("hooking up before checking current message against current "
				"transaction is unsafe! (tm_hookup() before get_tid()?)\n");
		abort();
	}
#endif
	
	if (tmb.register_tmcb(sipmsg, /* avoid new expensive lookup */tmb.t_gett(),
			TMCB_ACK_NEG_IN | TMCB_E2ECANCEL_IN, tm_callback, as, 
			/*callback*/0) < 0) {
		ERR("failed to register ACK&CANCEL hooks.\n");
		return -1;
	}
	DEBUG("registered e2eACK|CANCEL hooks for INVITE transaction.\n");
	return 1;
}

inline static brpc_t *new_brpc_req(str *method, enum ASI_DSGT_IDS discr, 
		str *tid, str *opaque)
{
	brpc_t *rpcreq = NULL;
	brpc_val_t *val;
	brpc_str_t mname = {method->s, method->len};

	if (! (rpcreq = brpc_req(mname, brpc_callid ++)))
		goto brpc_err;

	/* method type discriminator */
	if (! (val = brpc_int(discr))) goto brpc_err;
	if (! brpc_add_val(rpcreq, val)) goto free_val;
	/* transaction ID */
	val = tid ? brpc_str(tid->s, tid->len) : brpc_null(BRPC_VAL_STR);
	if (! val) goto brpc_err;
	if (! brpc_add_val(rpcreq, val)) goto free_val;
	/* opaque */
	val = opaque ? brpc_str(opaque->s, opaque->len) : brpc_null(BRPC_VAL_STR);
	if (! val) goto brpc_err;
	if (! brpc_add_val(rpcreq, val)) goto free_val;

	return rpcreq;
free_val:
	brpc_val_free(val);
brpc_err:
	ERR("BINRPC operation failed: %s [%d].\n", brpc_strerror(), brpc_errno);
	if (rpcreq)
		brpc_finish(rpcreq);
	return NULL;
}

static inline int unicast(struct sip_msg *sipmsg, as_t *as)
{
	int i;
	meth_dig_t *meth;
	tok_dig_t *toks;
	size_t tokcnt;
	brpc_t *rpcreq;
	enum ASI_DSGT_IDS discr; /* ..iminator */
	int ret;

	DEBUG("dispatching to AS '%.*s'.\n", STR_FMT(&as->name));
	for (i = 0; i < as->methcnt; i ++) {
		/* TODO: use msg_parser.h helpers (precalc. rcvd SIP method value) */
		meth = as->sipmeth + i;
		if (! STR_EQ(meth->name, REQ_LINE(sipmsg).method))
			continue;
		DEBUG("matched registered SIP method `%.*s'.\n", STR_FMT(&meth->name));
		if (sipmsg->first_line.type == SIP_REQUEST) {
			toks = meth->req.toks;
			tokcnt = meth->req.cnt;
			discr = ASI_DGST_ID_REQ;
		} else if (sipmsg->REPLY_STATUS < 200) {
			/* TODO: specify what provisionals are wanted (?) */
			toks = meth->prov.toks;
			tokcnt = meth->prov.cnt;
			discr = ASI_DGST_ID_PRV;
		} else {
			toks = meth->fin.toks;
			tokcnt = meth->fin.cnt;
			discr = ASI_DGST_ID_FIN;
		}

		if (tokcnt <= 0)
			continue;
		
		if (! (rpcreq = new_brpc_req(&REQ_LINE(sipmsg).method, discr, 
				get_tid(sipmsg), NULL))) {
			ERR("failed to build prepared BINRPC request.\n");
			return ASI_EFAILED;
		}
		if (digest(sipmsg, rpcreq, toks, tokcnt) < 0) {
			ERR("failed to digest message into BINRPC request.\n");
			goto free_req_fail;
		}
		if (tm_hookup(sipmsg, as) < 0) {
			ERR("failed to hook up TM.\n");
			goto free_req_fail;
		}
		ret = rpc_msg_xchg(rpcreq, as);
		if ((0 <= ret) && (discr == ASI_DGST_ID_REQ)) {
			/* if AS won't reply, SER should */
			/* TODO: is there any way of handling branches?! */
			if (tmb.t_addblind() < 0) {
				ERR("failed to add UAC blind watcher.\n"); // :))
			}
		}
		return ret;
	}
	
	return ASI_ENOMATCH;

free_req_fail:
	brpc_finish(rpcreq);
	return ASI_EFAILED;
}

static inline int dispatch2as(struct sip_msg *sipmsg, as_t *as)
{
	if (is_connected(as) < 0) {
		INFO("AS '%.*s' skipped because not connected.\n", STR_FMT(&as->name));
		return ASI_EFAILED;
	}
	return unicast(sipmsg, as);
}

int dispatch(struct sip_msg *sipmsg, as_t *as, void *_)
{
	if (! as) {
		/* TODO */
		ERR("broadcasting not yet implemented.\n");
		return -1;
	}

	return dispatch2as(sipmsg, as);
}

int dispatch_rebuild(struct sip_msg *sipmsg, as_t *as, void *_)
{
	int error, ret = -1;
	struct dest_info dst;
	struct sip_msg hot_msg;
	
	memset(&hot_msg, 0, sizeof(struct sip_msg));
	init_dest_info(&dst);
	/* TODO: this would get re-done uselessly, in case no further modifs
	 * between two successive dispatch_rebuid() are made => some static/var
	 * or caching? */
	hot_msg.buf = build_all(sipmsg, /*touch_clen*/1, &hot_msg.len, &error, 
		&dst);
	if (error || (! hot_msg.buf)) {
		ERR("failed to assemble altered SIP msg (%d).\n", error);
		return -1;
	}

	DEBUG("parsing locally assembled message.\n");
	if (parse_msg(hot_msg.buf, hot_msg.len, &hot_msg) != 0) {
		BUG("failed to parse own assembled message.\n");
		goto end;
	} else {
		/* copy all relevant metadata from orig. msg */
		hot_msg.id = sipmsg->id;
		hot_msg.rcv = sipmsg->rcv;
		hot_msg.hash_index = sipmsg->hash_index;
		hot_msg.msg_flags = sipmsg->msg_flags;
		hot_msg.flags = sipmsg->flags;
		hot_msg.force_send_socket = sipmsg->force_send_socket;
	}
	ret = dispatch(&hot_msg, as, _);
end:
	free_sip_msg(&hot_msg);
#ifndef DYN_BUF
	pkg_free(hot_msg.buf);
#endif
	return ret;
}

int dispatch_tm_reply(int as_id, struct sip_msg *sipmsg, 
		str *method, enum ASI_DSGT_IDS discr, /*usefull for FAKED_REPLY */
		str *tid, str *opaque)
{
	brpc_t *rpcreq;
	as_t *as;
	tok_dig_t *toks;
	size_t tokcnt;
	int i;
	meth_dig_t *meth;

#ifdef EXTRA_DEBUG
	assert(tid);

	if ((GET_AS_CNT <= as_id) || (as_id < 0)) {
		BUG("invalid AS ID value: %d; maximum: %zd.\n", as_id, GET_AS_CNT - 1);
		abort();
	}
	else
#endif
	{
		as = app_srvs[as_id];
	}

	if (is_connected(as) < 0) {
		INFO("AS '%.*s' skipped because not connected.\n", STR_FMT(&as->name));
		return ASI_EFAILED;
	}

	if (! (rpcreq = new_brpc_req(method, discr, tid, opaque))) {
		ERR("failed to build prepared BINRPC request.\n");
		return ASI_EFAILED;
	}
	if (sipmsg != FAKED_REPLY) {
		meth = NULL;
		for (i = 0; i < as->methcnt; i ++) {
			if (STR_EQ(as->sipmeth[i].name, *method)) {
				meth = &as->sipmeth[i];
				break;
			}
		}
		if (! meth) {
			ERR("AS '%.*s' flaged reply for initiated request `%.*s', but"
					" no digest format provided.\n", 
					STR_FMT(&as->name), STR_FMT(method));
			goto error;
		}
		switch (discr) {
			case ASI_DGST_ID_PRV:
				toks = meth->prov.toks;
				tokcnt = meth->prov.cnt;
				break;
			case ASI_DGST_ID_FIN:
				toks = meth->fin.toks;
				tokcnt = meth->fin.cnt;
				break;
			default:
				BUG("illegal message type discriminator (%d) in TM reply "
						"dispatcher.\n", discr);
				abort();
		}
		if (digest(sipmsg, rpcreq, toks, tokcnt) < 0) {
			ERR("failed to digest message into BINRPC request.\n");
			goto error;
		}
	} else {
		/* TODO: add cfg option to send NULLs (maintain 1 signature) */
		;
	}

	return rpc_msg_xchg(rpcreq, as);
error:
	brpc_finish(rpcreq);
	return ASI_EFAILED;
}

#ifdef ASI_WITH_RESYNC
/**
 * Set the resync flag for one AS.
 * @return:
 * 	EBADMSG failed to parse URI
 * 	EADDRNOTAVAIL AS not found
 * 	EINPROGRESS updating
 * 	0 updated
 */
int appsrv_set_resync(char *as_uri, int serial, int *as_id)
{
	brpc_addr_t *as_addr;
	int i, updated;
	uint8_t *resync;
	as_t *as;
	
	/* the string can be anbiguous (ex.: brpc4d:// == brpcnd:// or different
	 * names for same IP) => compare structures */
	if (! (as_addr = brpc_parse_uri(as_uri))) {
		ERR("failed to parse BINRPC URI `%s': %s (%d).\n", as_uri, 
				brpc_strerror(), brpc_errno);
		return EBADMSG;
	}
	
	for (i = 0; i < GET_AS_CNT; i ++)
		if (brpc_addr_eq(&app_srvs[i]->addr, as_addr)) {
			as = app_srvs[i];
			lock_get(as->resync_mutex);
			if (AS_RS_SERIAL(as) < serial) {
				DEBUG("AS '%.*s' asking for resync with updated serial %d.\n",
						STR_FMT(&as->name), serial);
				resync = as->resync;
				for (i = 0; i < *proc_bmb_cnt; i++)
					resync[i] = 0xFF;
				AS_RS_SERIAL(as) = serial;
				updated = 1;
			} else {
				DEBUG("AS '%.*s' asking for resync with already learned "
						"serial %d.\n", STR_FMT(&as->name), serial);
				updated = 0;
			}
			lock_release(as->resync_mutex);
			*as_id = as->id;
			return updated ? 0 : EINPROGRESS;
		}
	DEBUG("AS@%s not found.\n", as_uri);
	return EADDRNOTAVAIL;
}
#endif /* ASI_WITH_RESYNC */
