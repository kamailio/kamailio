/* 
 * $Id$
 * 
 * Copyright (C) 2009 iptelorg GmbH
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

#include "../../rpc_lookup.h"
#include "../../socket_info.h"
#include "../../globals.h"
#include "../../config.h"

#ifdef USE_SCTP
#include "sctp_options.h"
#include "sctp_server.h"
#endif


static const char* core_sctp_options_doc[] = {
	"Returns active sctp options. With one parameter"
	" it returns the sctp options set in the kernel for a specific socket"
	"(debugging), with 0 filled in for non-kernel related options."
	" The parameter can be: \"default\" | \"first\" | address[:port] ."
	" With no parameters it returns ser's idea of the current sctp options"
	 " (intended non-debugging use).",
	/* Documentation string */
	0                                 /* Method signature(s) */
};

static void core_sctp_options(rpc_t* rpc, void* c)
{
#ifdef USE_SCTP
	void *handle;
	struct cfg_group_sctp t;
	char* param;
	struct socket_info* si;
	char* host;
	str hs;
	int hlen;
	int port;
	int proto;

	param=0;
	if (!sctp_disable){
		/* look for optional socket parameter */
		if (rpc->scan(c, "*s", &param)>0){
			si=0;
			if (strcasecmp(param, "default")==0){
				si=sendipv4_sctp?sendipv4_sctp:sendipv6_sctp;
			}else if (strcasecmp(param, "first")==0){
				si=sctp_listen;
			}else{
				if (parse_phostport(param, &host, &hlen, &port, &proto)!=0){
					rpc->fault(c, 500, "bad param (use address, address:port,"
										" default or first)");
					return;
				}
				if (proto && proto!=PROTO_SCTP){
					rpc->fault(c, 500, "bad protocol in param (only SCTP"
										" allowed)");
					return;
				}
				hs.s=host;
				hs.len=hlen;
				si=grep_sock_info(&hs, port, PROTO_SCTP);
				if (si==0){
					rpc->fault(c, 500, "not listening on sctp %s", param);
					return;
				}
			}
			if (si==0 || si->socket==-1){
				rpc->fault(c, 500, "could not find a sctp socket");
				return;
			}
			memset(&t, 0, sizeof(t));
			if (sctp_get_cfg_from_sock(si->socket, &t)!=0){
				rpc->fault(c, 500, "failed to get socket options");
				return;
			}
		}else{
			sctp_options_get(&t);
		}
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "ddddddddddddddddddd",
			"sctp_socket_rcvbuf",	t.so_rcvbuf,
			"sctp_socket_sndbuf",	t.so_sndbuf,
			"sctp_autoclose",		t.autoclose,
			"sctp_send_ttl",	t.send_ttl,
			"sctp_send_retries",	t.send_retries,
			"sctp_assoc_tracking",	t.assoc_tracking,
			"sctp_assoc_reuse",	t.assoc_reuse,
			"sctp_max_assocs", t.max_assocs,
			"sctp_srto_initial",	t.srto_initial,
			"sctp_srto_max",		t.srto_max,
			"sctp_srto_min",		t.srto_min,
			"sctp_asocmaxrxt",	t.asocmaxrxt,
			"sctp_init_max_attempts",	t.init_max_attempts,
			"sctp_init_max_timeo",t.init_max_timeo,
			"sctp_hbinterval",	t.hbinterval,
			"sctp_pathmaxrxt",	t.pathmaxrxt,
			"sctp_sack_delay",	t.sack_delay,
			"sctp_sack_freq",	t.sack_freq,
			"sctp_max_burst",	t.max_burst
		);
	}else{
		rpc->fault(c, 500, "sctp support disabled");
	}
#else
	rpc->fault(c, 500, "sctp support not compiled");
#endif
}



static const char* core_sctpinfo_doc[] = {
	"Returns sctp related info.",    /* Documentation string */
	0                               /* Method signature(s) */
};

static void core_sctpinfo(rpc_t* rpc, void* c)
{
#ifdef USE_SCTP
	void *handle;
	struct sctp_gen_info i;

	if (!sctp_disable){
		sctp_get_info(&i);
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "ddd",
			"opened_connections", i.sctp_connections_no,
			"tracked_connections", i.sctp_tracked_no,
			"total_connections", i.sctp_total_connections
		);
	}else{
		rpc->fault(c, 500, "sctp support disabled");
	}
#else
	rpc->fault(c, 500, "sctp support not compiled");
#endif
}


/*
 * RPC Methods exported by this module
 */
static rpc_export_t sctp_rpc_methods[] = {
	{"sctp.options",      core_sctp_options,      core_sctp_options_doc,
		0},
	{"sctp.info",         core_sctpinfo,          core_sctpinfo_doc,   0},

	{0, 0, 0, 0}
};


int sctp_register_rpc(void)
{
	if (rpc_register_array(sctp_rpc_methods)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	return 0;
}
