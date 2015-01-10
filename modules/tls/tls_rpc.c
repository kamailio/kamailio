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


#include "../../rpc.h"
#include "../../tcp_conn.h"
#include "../../tcp_info.h"
#include "../../timer.h"
#include "../../cfg/cfg.h"
#include "../../dprint.h"
#include "tls_init.h"
#include "tls_mod.h"
#include "tls_domain.h"
#include "tls_config.h"
#include "tls_util.h"
#include "tls_server.h"
#include "tls_ct_wrq.h"
#include "tls_rpc.h"
#include "tls_cfg.h"

static const char* tls_reload_doc[2] = {
	"Reload TLS configuration file",
	0
};

static void tls_reload(rpc_t* rpc, void* ctx)
{
	tls_domains_cfg_t* cfg;
	str tls_domains_cfg_file;

	tls_domains_cfg_file = cfg_get(tls, tls_cfg, config_file);
	if (!tls_domains_cfg_file.s) {
		rpc->fault(ctx, 500, "No TLS configuration file configured");
		return;
	}

	/* Try to delete old configurations first */
	collect_garbage();

	cfg = tls_load_config(&tls_domains_cfg_file);

	if (!cfg) {
		rpc->fault(ctx, 500, "Error while loading TLS configuration file"
							" (consult server log)");
		return;
	}

	if (tls_fix_domains_cfg(cfg, &srv_defaults, &cli_defaults) < 0) {
		rpc->fault(ctx, 500, "Error while fixing TLS configuration"
								" (consult server log)");
		goto error;
	}
	if (tls_check_sockets(cfg) < 0) {
		rpc->fault(ctx, 500, "No server listening socket found for one of"
							" TLS domains (consult server log)");
		goto error;
	}

	DBG("TLS configuration successfuly loaded");

	lock_get(tls_domains_cfg_lock);

	cfg->next = (*tls_domains_cfg);
	*tls_domains_cfg = cfg;

	lock_release(tls_domains_cfg_lock);

	return;

 error:
	tls_free_cfg(cfg);
	
}


static const char* tls_list_doc[2] = {
	"List currently open TLS connections",
	0
};

extern gen_lock_t* tcpconn_lock;
extern struct tcp_connection** tcpconn_id_hash;

static void tls_list(rpc_t* rpc, void* c)
{
	char buf[128];
	char src_ip[IP_ADDR_MAX_STR_SIZE];
	char dst_ip[IP_ADDR_MAX_STR_SIZE];
	void* handle;
	char* tls_info;
	char* state;
	struct tls_extra_data* tls_d;
	struct tcp_connection* con;
	int i, len, timeout;

	TCPCONN_LOCK;
	for(i = 0; i < TCP_ID_HASH_SIZE; i++) {
		for (con = tcpconn_id_hash[i]; con; con = con->id_next) {
			if (con->rcv.proto != PROTO_TLS) continue;
			tls_d = con->extra_data;
			rpc->add(c, "{", &handle);
			/* tcp data */
			if ((len = ip_addr2sbuf(&con->rcv.src_ip, src_ip, sizeof(src_ip)))
					== 0)
				BUG("failed to convert source ip");
			src_ip[len] = 0;
			if ((len = ip_addr2sbuf(&con->rcv.dst_ip, dst_ip, sizeof(dst_ip)))
					== 0)
				BUG("failed to convert destination ip");
			dst_ip[len] = 0;
			timeout = TICKS_TO_S(con->timeout - get_ticks_raw());
			rpc->struct_add(handle, "ddsdsd",
					"id", con->id,
					"timeout", timeout,
					"src_ip", src_ip,
					"src_port", con->rcv.src_port,
					"dst_ip", dst_ip,
					"dst_port", con->rcv.dst_port);
			if (tls_d) {
				if(SSL_get_current_cipher(tls_d->ssl)) {
					tls_info = SSL_CIPHER_description(
									SSL_get_current_cipher(tls_d->ssl),
									buf, sizeof(buf));
					len = strlen(buf);
					if (len && buf[len - 1] == '\n') buf[len - 1] = '\0';
				} else {
					tls_info = "unknown";
				}
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
					rpc->struct_add(handle, "sddds",
							"cipher", tls_info,
							"ct_wq_size", tls_d->ct_wq?
											tls_d->ct_wq->queued:0,
							"enc_rd_buf", tls_d->enc_rd_buf?
											tls_d->enc_rd_buf->size:0,
							"flags", tls_d->flags,
							"state", state
							);
				lock_release(&con->write_lock);
			} else {
				rpc->struct_add(handle, "sddds",
						"cipher", "unknown",
						"ct_wq_size", 0,
						"enc_rd_buf", 0,
						"flags", 0,
						"state", "pre-init"
						);
			}
		}
	}
	TCPCONN_UNLOCK;
}



static const char* tls_info_doc[2] = {
	"Returns internal tls related info.",
	0 };

static void tls_info(rpc_t* rpc, void* c)
{
	struct tcp_gen_info ti;
	void* handle;

	tcp_get_info(&ti);
	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "ddd",
			"max_connections", ti.tls_max_connections,
			"opened_connections", ti.tls_connections_no,
			"clear_text_write_queued_bytes", tls_ct_wq_total_bytes());
}



static const char* tls_options_doc[2] = {
	"Dumps all the tls config options.",
	0 };

static void tls_options(rpc_t* rpc, void* c)
{
	void* handle;
	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "dSdddSSSSdSSdddddddddddddd",
		"force_run",	cfg_get(tls, tls_cfg, force_run),
		"method",		&cfg_get(tls, tls_cfg, method),
		"verify_certificate", cfg_get(tls, tls_cfg, verify_cert),

		"verify_depth",		cfg_get(tls, tls_cfg, verify_depth),
		"require_certificate",	cfg_get(tls, tls_cfg, require_cert),
		"private_key",		&cfg_get(tls, tls_cfg, private_key),
		"ca_list",			&cfg_get(tls, tls_cfg, ca_list),
		"certificate",		&cfg_get(tls, tls_cfg, certificate),
		"cipher_list",		&cfg_get(tls, tls_cfg, cipher_list),
		"session_cache",	cfg_get(tls, tls_cfg, session_cache),
		"session_id",		&cfg_get(tls, tls_cfg, session_id),
		"config",			&cfg_get(tls, tls_cfg, config_file),
		"log",				cfg_get(tls, tls_cfg, log),
		"debug",			cfg_get(tls, tls_cfg, debug),
		"connection_timeout", TICKS_TO_S(cfg_get(tls, tls_cfg, con_lifetime)),
		"disable_compression",	cfg_get(tls, tls_cfg, disable_compression),
		"ssl_release_buffers",	cfg_get(tls, tls_cfg, ssl_release_buffers),
		"ssl_freelist_max",		cfg_get(tls, tls_cfg, ssl_freelist_max),
		"ssl_max_send_fragment", cfg_get(tls, tls_cfg, ssl_max_send_fragment),
		"ssl_read_ahead",		cfg_get(tls, tls_cfg, ssl_read_ahead),
		"send_close_notify",	cfg_get(tls, tls_cfg, send_close_notify),
		"low_mem_threshold1",	cfg_get(tls, tls_cfg, low_mem_threshold1),
		"low_mem_threshold2",	cfg_get(tls, tls_cfg, low_mem_threshold2),
		"ct_wq_max",			cfg_get(tls, tls_cfg, ct_wq_max),
		"con_ct_wq_max",		cfg_get(tls, tls_cfg, con_ct_wq_max),
		"ct_wq_blk_size",		cfg_get(tls, tls_cfg, ct_wq_blk_size)
		);
}




rpc_export_t tls_rpc[] = {
	{"tls.reload", tls_reload, tls_reload_doc, 0},
	{"tls.list",   tls_list,   tls_list_doc,   RET_ARRAY},
	{"tls.info",   tls_info,   tls_info_doc, 0},
	{"tls.options",tls_options, tls_options_doc, 0},
	{0, 0, 0, 0}
};
