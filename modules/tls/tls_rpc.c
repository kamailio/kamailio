/*
 * $Id$
 *
 * TLS module - management interface
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../rpc.h"
#include "../../tcp_conn.h"
#include "../../timer.h"
#include "tls_init.h"
#include "tls_mod.h"
#include "tls_domain.h"
#include "tls_config.h"
#include "tls_util.h"
#include "tls_server.h"
#include "tls_rpc.h"

static const char* tls_reload_doc[2] = {
	"Reload TLS configuration file",
	0
};

static void tls_reload(rpc_t* rpc, void* ctx)
{
	tls_cfg_t* cfg;

	if (!tls_cfg_file.s) {
		rpc->fault(ctx, 500, "No TLS configuration file configured");
		return;
	}

	     /* Try to delete old configurations first */
	collect_garbage();

	cfg = tls_load_config(&tls_cfg_file);
	if (!cfg) {
		rpc->fault(ctx, 500, "Error while loading TLS configuration file (consult server log)");
		return;
	}

	if (tls_fix_cfg(cfg, &srv_defaults, &cli_defaults) < 0) {
		rpc->fault(ctx, 500, "Error while fixing TLS configuration (consult server log)");
		goto error;
	}
	if (tls_check_sockets(cfg) < 0) {
		rpc->fault(ctx, 500, "No server listening socket found for one of TLS domains (consult server log)");
		goto error;
	}

	DBG("TLS configuration successfuly loaded");
	cfg->next = (*tls_cfg);
	*tls_cfg = cfg;
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
	static char buf[128];
	void* handle;
	char* tls_info;
	SSL* ssl;
	struct tcp_connection* con;
	int i, len, timeout;

	ssl=0;
	TCPCONN_LOCK;
	for(i = 0; i < TCP_ID_HASH_SIZE; i++) {
		if (tcpconn_id_hash[i] == NULL) continue;
		con = tcpconn_id_hash[i];
		while(con) {
			if (con->rcv.proto != PROTO_TLS) goto skip;
			if (con->extra_data) 
				ssl = ((struct tls_extra_data*)con->extra_data)->ssl;
			if (ssl) {
				tls_info = SSL_CIPHER_description(SSL_get_current_cipher(ssl),
													buf, 128);
				len = strlen(buf);
				if (len && buf[len - 1] == '\n') buf[len - 1] = '\0';
			} else {
				tls_info = "Unknown";
			}
			timeout = con->timeout - get_ticks();
			if (timeout < 0) timeout = 0;
			rpc->add(c, "{", &handle);
			rpc->struct_add(handle, "ddsdsds",
					"id", con->id,
					"timeout", timeout,
					"src_ip", ip_addr2a(&con->rcv.src_ip),
					"src_port", con->rcv.src_port,
					"dst_ip", ip_addr2a(&con->rcv.dst_ip),
					"dst_port", con->rcv.dst_port,
					"tls", 	tls_info);
		skip:
			con = con->id_next;
		}
	}

	TCPCONN_UNLOCK;
}



rpc_export_t tls_rpc[] = {
	{"tls.reload", tls_reload, tls_reload_doc, 0},
	{"tls.list",   tls_list,   tls_list_doc,   RET_ARRAY},
	{0, 0, 0, 0}
};
