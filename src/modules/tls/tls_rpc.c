/*
 * TLS module - management interface
 *
 * Copyright (C) 2005 iptelorg GmbH
 * Copyright (C) 2013 Motorola Solutions, Inc.
 *
 * This file is part of Kamailio, a free SIP server.
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
/** tls module management interface (rpc).
 * @file tls_rpc.c
 * @ingroup tls
 * Module: @ref tls
 */


#include "../../core/rpc.h"
#include "../../core/tcp_conn.h"
#include "../../core/tcp_info.h"
#include "../../core/timer.h"
#include "../../core/cfg/cfg.h"
#include "../../core/dprint.h"
#include "../../core/tcp_mtops.h"
#include "tls_openssl.h"
#include "tls_init.h"
#include "tls_mod.h"
#include "tls_domain.h"
#include "tls_config.h"
#include "tls_util.h"
#include "tls_server.h"
#include "tls_ct_wrq.h"
#include "tls_rpc.h"
#include "tls_cfg.h"

extern int ksr_tcp_main_threads;

/*
 * packed into a single shm alloc: [ tcpx_task_t | tls_reload_task_t ]
 * input params and result are both carried in tls_reload_task_t so no
 * second alloc is needed — PROC_TCP_MAIN writes result directly into
 * the block the sender is waiting on.
 */
typedef struct tls_reload_task
{
	/* input — set by sender before dispatch */
	char config_file_buf[256];
	int config_file_len;
	int pidx; /* sender process_no for result channel */
	/* result — set by PROC_TCP_MAIN thread callback */
	int code; /* 0 = success, -1 = error */
	char errmsg[256];
} tls_reload_task_t;

/**
 * tls_reload_do - core reload logic
 *
 * Safe to call only in the process that owns SSL_CTX:
 *   tcp_main_threads == 0: any rank (pre-fork ctx in shm)
 *   tcp_main_threads  > 0: PROC_TCP_MAIN only
 *
 * @return 0 on success, -1 on failure with errmsg populated
 */
static int tls_reload_do(str *config_file, char *errmsg, int errmsg_size)
{
	tls_domains_cfg_t *cfg = NULL;

	collect_garbage();

	cfg = tls_load_config(config_file);
	if(cfg == NULL) {
		snprintf(errmsg, errmsg_size,
				"Error while loading TLS configuration file"
				" (consult server log)");
		return -1;
	}

	if(tls_fix_domains_cfg(cfg, &srv_defaults, &cli_defaults) < 0) {
		snprintf(errmsg, errmsg_size,
				"Error while fixing TLS configuration"
				" (consult server log)");
		tls_free_cfg(cfg);
		return -1;
	}

	if(tls_check_sockets(cfg) < 0) {
		snprintf(errmsg, errmsg_size,
				"No server listening socket found for one of"
				" TLS domains (consult server log)");
		tls_free_cfg(cfg);
		return -1;
	}

	DBG("TLS configuration successfully loaded");

	lock_get(tls_domains_cfg_lock);
	cfg->next = (*tls_domains_cfg);
	*tls_domains_cfg = cfg;
	lock_release(tls_domains_cfg_lock);

#ifdef KSR_SSL_COMMON
	/* reload HSM/engine keys into the new SSL_CTX.
	 * tls_fix_domains_cfg only handles soft keys — without this,
	 * tls.reload silently leaves the new ctx with no private key,
	 * breaking all subsequent TLS handshakes until restart.
	 * Fixes the pre-existing bug for both tcp_main_threads==0 and >0. */
	if(tls_reload_engine_keys() < 0) {
		snprintf(errmsg, errmsg_size,
				"TLS config reloaded but HSM/engine key reload failed"
				" (consult server log)");
		return -1;
	}
#endif /* KSR_SSL_COMMON */

	LM_INFO("TLS configuration reloaded\n");
	return 0;
}

/**
 * tls_reload_mt_thread_cb - gRPC BindableService equivalent
 *
 * Runs in PROC_TCP_MAIN thread context — SSL_CTX owned here.
 * Calls tls_reload_do then signals result back via ksr_tcpx_thread_eresult.
 */
static void tls_reload_mt_thread_cb(void *p, int pidx)
{
	tls_reload_task_t *t = (tls_reload_task_t *)p;
	tcpx_task_result_t *rtask = NULL;
	str config_file;

	config_file.s = t->config_file_buf;
	config_file.len = t->config_file_len;

	rtask = (tcpx_task_result_t *)shm_mallocxz(sizeof(tcpx_task_result_t));
	if(rtask == NULL) {
		SHM_MEM_ERROR;
		t->code = -1;
		snprintf(t->errmsg, sizeof(t->errmsg), "Out of shared memory");
		ksr_tcpx_thread_eresult(NULL, t->pidx);
		return;
	}

	t->code = tls_reload_do(&config_file, t->errmsg, sizeof(t->errmsg));

	rtask->code = t->code;
	rtask->data = NULL; /* result embedded in task block */
	ksr_tcpx_thread_eresult(rtask, t->pidx);
}

/**
 * tls_reload_mt - gRPC client stub equivalent
 *
 * Marshals config_file into shm task, dispatches to PROC_TCP_MAIN,
 * blocks until result received, reports result via rpc.
 */
static void tls_reload_mt(rpc_t *rpc, void *ctx, str *config_file)
{
	int dsize = 0;
	tcpx_task_t *ptask = NULL;
	tcpx_task_result_t *rtask = NULL;
	tls_reload_task_t *t = NULL;

	if(config_file->len >= (int)sizeof(t->config_file_buf)) {
		rpc->fault(ctx, 500, "TLS configuration file path too long");
		return;
	}

	dsize = sizeof(tcpx_task_t) + sizeof(tls_reload_task_t);
	ptask = (tcpx_task_t *)shm_mallocxz(dsize);
	if(ptask == NULL) {
		SHM_MEM_ERROR;
		rpc->fault(ctx, 500, "Out of shared memory");
		return;
	}

	ptask->exec = tls_reload_mt_thread_cb;
	ptask->param = (void *)((char *)ptask + sizeof(tcpx_task_t));
	t = (tls_reload_task_t *)ptask->param;
	memcpy(t->config_file_buf, config_file->s, config_file->len);
	t->config_file_buf[config_file->len] = '\0';
	t->config_file_len = config_file->len;
	t->pidx = process_no;

	LM_DBG("dispatching tls.reload to PROC_TCP_MAIN from rank=%d\n",
			process_no);

	if(ksr_tcpx_task_send(ptask, process_no) < 0) {
		LM_ERR("failed to send tls.reload task to PROC_TCP_MAIN\n");
		rpc->fault(ctx, 500, "Failed to dispatch tls.reload to PROC_TCP_MAIN");
		shm_free(ptask);
		return;
	}

	ksr_tcpx_task_result_recv(&rtask, process_no);
	if(rtask == NULL) {
		LM_ERR("no result received from PROC_TCP_MAIN for tls.reload\n");
		rpc->fault(ctx, 500, "No result received from PROC_TCP_MAIN");
		shm_free(ptask);
		return;
	}

	/* result is embedded in ptask block — read before free */
	if(t->code < 0) {
		LM_ERR("tls.reload failed in PROC_TCP_MAIN: %s\n", t->errmsg);
		rpc->fault(ctx, 500, t->errmsg);
	} else {
		LM_INFO("tls.reload succeeded in PROC_TCP_MAIN\n");
		rpc->rpl_printf(ctx, "Ok. TLS configuration reloaded.");
	}

	shm_free(rtask);
	shm_free(ptask);
}

static const char *tls_reload_doc[2] = {"Reload TLS configuration file", 0};

/**
 * tls_reload - RPC dispatcher
 *
 * Routes to tls_reload_mt (client stub → PROC_TCP_MAIN) when
 * tcp_main_threads > 0, otherwise calls tls_reload_do locally.
 */
static void tls_reload(rpc_t *rpc, void *ctx)
{
	char errmsg[256];
	str config_file;

	config_file = cfg_get(tls, tls_cfg, config_file);
	if(!config_file.s) {
		rpc->fault(ctx, 500, "No TLS configuration file configured");
		return;
	}

	if(ksr_tcp_main_threads > 0) {
		/* SSL_CTX owned by PROC_TCP_MAIN — use client stub */
		LM_INFO("tcp_main_threads=%d: proxying tls.reload to"
				" PROC_TCP_MAIN\n",
				ksr_tcp_main_threads);
		tls_reload_mt(rpc, ctx, &config_file);
		return;
	}

	/* tcp_main_threads == 0: execute locally (in-process server) */
	if(tls_reload_do(&config_file, errmsg, sizeof(errmsg)) < 0) {
		rpc->fault(ctx, 500, errmsg);
		return;
	}
	rpc->rpl_printf(ctx, "Ok. TLS configuration reloaded.");
}


static const char *tls_list_doc[2] = {"List currently open TLS connections", 0};

extern gen_lock_t *tcpconn_lock;
extern struct tcp_connection **tcpconn_id_hash;

static void tls_list(rpc_t *rpc, void *c)
{
	char src_ip[IP_ADDR_MAX_STR_SIZE];
	char dst_ip[IP_ADDR_MAX_STR_SIZE];
	void *handle;
	char *tls_info;
	char *state;
	struct tls_extra_data *tls_d;
	struct tcp_connection *con;
	int i, len, timeout;
	struct tm timestamp;
	char timestamp_s[128];
	const char *sni, *dom;

	TCPCONN_LOCK;
	for(i = 0; i < TCP_ID_HASH_SIZE; i++) {
		for(con = tcpconn_id_hash[i]; con; con = con->id_next) {
			if(con->rcv.proto != PROTO_TLS && con->rcv.proto != PROTO_WSS)
				continue;
			tls_d = con->extra_data;
			rpc->add(c, "{", &handle);
			/* tcp data */
			if((len = ip_addr2sbuf(&con->rcv.src_ip, src_ip, sizeof(src_ip)))
					== 0)
				BUG("failed to convert source ip");
			src_ip[len] = 0;
			if((len = ip_addr2sbuf(&con->rcv.dst_ip, dst_ip, sizeof(dst_ip)))
					== 0)
				BUG("failed to convert destination ip");
			dst_ip[len] = 0;
			timeout = TICKS_TO_S(con->timeout - get_ticks_raw());
			timestamp = *localtime(&con->timestamp);
			if(snprintf(timestamp_s, 128, "%d-%02d-%02d %02d:%02d:%02d",
					   timestamp.tm_year + 1900, timestamp.tm_mon + 1,
					   timestamp.tm_mday, timestamp.tm_hour, timestamp.tm_min,
					   timestamp.tm_sec)
					< 0) {
				timestamp_s[0] = 'N';
				timestamp_s[1] = '/';
				timestamp_s[2] = 'A';
				timestamp_s[3] = '\0';
			}

			if(tls_d) {
				sni = tls_d->ssl_servername;
				dom = tls_d->dom.s;
				if(sni == NULL) {
					sni = "N/A";
				}
			} else {
				sni = "N/A";
				dom = "N/A";
			}

			rpc->struct_add(handle, "dsssdsdsd", "id", con->id, "dom", dom,
					"sni", sni, "timestamp", timestamp_s, "timeout", timeout,
					"src_ip", src_ip, "src_port", con->rcv.src_port, "dst_ip",
					dst_ip, "dst_port", con->rcv.dst_port);
			if(tls_d) {
				tls_info = tls_d->ssl_cipher_desc;
				/* tls data */
				state = "unknown/error";
				lock_get(&con->write_lock);
				switch(tls_d->state) {
					case S_TLS_NONE:
						state = "none/init";
						break;
					case S_TLS_ACCEPTING:
						state = "tls_accept";
						break;
					case S_TLS_CONNECTING:
						state = "tls_connect";
						break;
					case S_TLS_ESTABLISHED:
						state = "established";
						break;
				}
				rpc->struct_add(handle, "sddds", "cipher", tls_info,
						"ct_wq_size", tls_d->ct_wq ? tls_d->ct_wq->queued : 0,
						"enc_rd_buf",
						tls_d->enc_rd_buf ? tls_d->enc_rd_buf->size : 0,
						"flags", tls_d->flags, "state", state);
				lock_release(&con->write_lock);
			} else {
				rpc->struct_add(handle, "sddds", "cipher", "unknown",
						"ct_wq_size", 0, "enc_rd_buf", 0, "flags", 0, "state",
						"pre-init");
			}
		}
	}
	TCPCONN_UNLOCK;
}


static const char *tls_info_doc[2] = {"Returns internal tls related info.", 0};

static void tls_info(rpc_t *rpc, void *c)
{
	struct tcp_gen_info ti;
	void *handle;

	tcp_get_info(&ti);
	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "ddd", "max_connections", ti.tls_max_connections,
			"opened_connections", ti.tls_connections_no,
			"clear_text_write_queued_bytes", tls_ct_wq_total_bytes());
}


static const char *tls_options_doc[2] = {
		"Dumps all the tls config options.", 0};

static void tls_options(rpc_t *rpc, void *c)
{
	void *handle;
	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "dSdddSSSSSdSSdddddddddddddd", "force_run",
			cfg_get(tls, tls_cfg, force_run), "method",
			&cfg_get(tls, tls_cfg, method), "verify_certificate",
			cfg_get(tls, tls_cfg, verify_cert),

			"verify_depth", cfg_get(tls, tls_cfg, verify_depth),
			"require_certificate", cfg_get(tls, tls_cfg, require_cert),
			"verify_client", &cfg_get(tls, tls_cfg, verify_client),
			"private_key", &cfg_get(tls, tls_cfg, private_key), "ca_list",
			&cfg_get(tls, tls_cfg, ca_list), "certificate",
			&cfg_get(tls, tls_cfg, certificate), "cipher_list",
			&cfg_get(tls, tls_cfg, cipher_list), "session_cache",
			cfg_get(tls, tls_cfg, session_cache), "session_id",
			&cfg_get(tls, tls_cfg, session_id), "config",
			&cfg_get(tls, tls_cfg, config_file), "log",
			cfg_get(tls, tls_cfg, log), "debug", cfg_get(tls, tls_cfg, debug),
			"connection_timeout",
			TICKS_TO_S(cfg_get(tls, tls_cfg, con_lifetime)),
			"disable_compression", cfg_get(tls, tls_cfg, disable_compression),
			"ssl_release_buffers", cfg_get(tls, tls_cfg, ssl_release_buffers),
			"ssl_freelist_max", cfg_get(tls, tls_cfg, ssl_freelist_max),
			"ssl_max_send_fragment",
			cfg_get(tls, tls_cfg, ssl_max_send_fragment), "ssl_read_ahead",
			cfg_get(tls, tls_cfg, ssl_read_ahead), "send_close_notify",
			cfg_get(tls, tls_cfg, send_close_notify), "low_mem_threshold1",
			cfg_get(tls, tls_cfg, low_mem_threshold1), "low_mem_threshold2",
			cfg_get(tls, tls_cfg, low_mem_threshold2), "ct_wq_max",
			cfg_get(tls, tls_cfg, ct_wq_max), "con_ct_wq_max",
			cfg_get(tls, tls_cfg, con_ct_wq_max), "ct_wq_blk_size",
			cfg_get(tls, tls_cfg, ct_wq_blk_size));
}

static const char *tls_kill_doc[2] = {
		"Kills a tls session, identified via id.", 0};

static void tls_kill(rpc_t *rpc, void *c)
{
	struct tcp_connection *con;
	int i, kill_id = 0;

	if(rpc->scan(c, "d", &kill_id) < 0) {
		/* Reply is set automatically by scan upon failure,
		* no need to do anything here
		*/
		return;
	}

	TCPCONN_LOCK;
	for(i = 0; i < TCP_ID_HASH_SIZE; i++) {
		for(con = tcpconn_id_hash[i]; con; con = con->id_next) {
			if(con->rcv.proto != PROTO_TLS && con->rcv.proto != PROTO_WSS)
				continue;
			if(con->id == kill_id) {
				con->state = -2;
				con->timeout = get_ticks_raw();

				TCPCONN_UNLOCK;

				rpc->add(c, "s", "OK");
				return;
			}
		}
	}
	TCPCONN_UNLOCK;

	rpc->add(c, "s", "TLS connection id not found");
}


rpc_export_t tls_rpc[] = {
		{"tls.reload", tls_reload, tls_reload_doc, RPC_EXEC_DELTA},
		{"tls.list", tls_list, tls_list_doc, RET_ARRAY},
		{"tls.info", tls_info, tls_info_doc, 0},
		{"tls.options", tls_options, tls_options_doc, 0},
		{"tls.kill", tls_kill, tls_kill_doc, 0}, {0, 0, 0, 0}};
