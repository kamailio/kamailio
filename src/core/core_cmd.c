/*
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/** Kamailio Core :: core rpcs.
 * @file core_cmd.c
 * @ingroup core
 */


#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include "ver.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "sr_module.h"
#include "rpc_lookup.h"
#include "dprint.h"
#include "core_cmd.h"
#include "globals.h"
#include "forward.h"
#include "socket_info.h"
#include "name_alias.h"
#include "pt.h"
#include "ut.h"
#include "tcp_info.h"
#include "tcp_conn.h"
#include "tcp_options.h"
#include "core_cmd.h"
#include "cfg_core.h"
#include "ppcfg.h"

#ifdef USE_DNS_CACHE
void dns_cache_debug(rpc_t* rpc, void* ctx);
void dns_cache_debug_all(rpc_t* rpc, void* ctx);
void dns_cache_mem_info(rpc_t* rpc, void* ctx);
void dns_cache_view(rpc_t* rpc, void* ctx);
void dns_cache_rpc_lookup(rpc_t* rpc, void* ctx);
void dns_cache_delete_all(rpc_t* rpc, void* ctx);
void dns_cache_delete_all_force(rpc_t* rpc, void* ctx);
void dns_cache_add_a(rpc_t* rpc, void* ctx);
void dns_cache_add_aaaa(rpc_t* rpc, void* ctx);
void dns_cache_add_srv(rpc_t* rpc, void* ctx);
void dns_cache_delete_a(rpc_t* rpc, void* ctx);
void dns_cache_delete_aaaa(rpc_t* rpc, void* ctx);
void dns_cache_delete_srv(rpc_t* rpc, void* ctx);
void dns_cache_delete_naptr(rpc_t* rpc, void* ctx);
void dns_cache_delete_cname(rpc_t* rpc, void* ctx);
void dns_cache_delete_txt(rpc_t* rpc, void* ctx);
void dns_cache_delete_ebl(rpc_t* rpc, void* ctx);
void dns_cache_delete_ptr(rpc_t* rpc, void* ctx);


static const char* dns_cache_mem_info_doc[] = {
	"dns cache memory info.",    /* Documentation string */
	0                      /* Method signature(s) */
};
static const char* dns_cache_debug_doc[] = {
	"dns debug info.",    /* Documentation string */
	0                      /* Method signature(s) */
};

static const char* dns_cache_debug_all_doc[] = {
	"complete dns debug dump",    /* Documentation string */
	0                              /* Method signature(s) */
};

static const char* dns_cache_view_doc[] = {
	"dns cache dump in a human-readable format",
	0
};

static const char* dns_cache_rpc_lookup_doc[] = {
	"perform a dns lookup",
	0
};

static const char* dns_cache_delete_all_doc[] = {
	"deletes all the non-permanent entries from the DNS cache",
	0
};

static const char* dns_cache_delete_all_force_doc[] = {
	"deletes all the entries from the DNS cache including the permanent ones",
	0
};

static const char* dns_cache_add_a_doc[] = {
	"adds an A record to the DNS cache",
	0
};

static const char* dns_cache_add_aaaa_doc[] = {
	"adds an AAAA record to the DNS cache",
	0
};
static const char* dns_cache_add_srv_doc[] = {
	"adds an SRV record to the DNS cache",
	0
};

static const char* dns_cache_delete_a_doc[] = {
	"deletes an A record from the DNS cache",
	0
};

static const char* dns_cache_delete_aaaa_doc[] = {
	"deletes an AAAA record from the DNS cache",
	0
};

static const char* dns_cache_delete_srv_doc[] = {
	"deletes an SRV record from the DNS cache",
	0
};

static const char* dns_cache_delete_naptr_doc[] = {
	"deletes a NAPTR record from the DNS cache",
	0
};

static const char* dns_cache_delete_cname_doc[] = {
	"deletes a CNAME record from the DNS cache",
	0
};

static const char* dns_cache_delete_txt_doc[] = {
	"deletes a TXT record from the DNS cache",
	0
};

static const char* dns_cache_delete_ebl_doc[] = {
	"deletes an EBL record from the DNS cache",
	0
};


static const char* dns_cache_delete_ptr_doc[] = {
	"deletes an PTR record from the DNS cache",
	0
};


#ifdef USE_DNS_CACHE_STATS
void dns_cache_stats_get(rpc_t* rpc, void* ctx);

static const char* dns_cache_stats_get_doc[] = {
	"returns the dns measurement counters.",
	0
};
#endif /* USE_DNS_CACHE_STATS */
#ifdef DNS_WATCHDOG_SUPPORT
void dns_set_server_state_rpc(rpc_t* rpc, void* ctx);

static const char* dns_set_server_state_doc[] = {
	"sets the state of the DNS servers " \
	"(0: all the servers are down, 1: at least one server is up)",    /* Documentation string */
	0                              /* Method signature(s) */
};

void dns_get_server_state_rpc(rpc_t* rpc, void* ctx);

static const char* dns_get_server_state_doc[] = {
	"prints the state of the DNS servers " \
	"(0: all the servers are down, 1: at least one server is up)",	/* Documentation string */
	0				/* Method signature(s) */
};

#endif /* DNS_WATCHDOG_SUPPORT */
#endif /* USE_DNS_CACHE */
#ifdef USE_DST_BLACKLIST
void dst_blst_debug(rpc_t* rpc, void* ctx);
void dst_blst_mem_info(rpc_t* rpc, void* ctx);
void dst_blst_view(rpc_t* rpc, void* ctx);
void dst_blst_delete_all(rpc_t* rpc, void* ctx);
void dst_blst_add(rpc_t* rpc, void* ctx);

static const char* dst_blst_mem_info_doc[] = {
	"dst blacklist memory usage info.",  /* Documentation string */
	0                                    /* Method signature(s) */
};
static const char* dst_blst_debug_doc[] = {
	"dst blacklist debug info.",  /* Documentation string */
	0                               /* Method signature(s) */
};
static const char* dst_blst_view_doc[] = {
	"dst blacklist dump in human-readable format.",  /* Documentation string */
	0                               /* Method signature(s) */
};
static const char* dst_blst_delete_all_doc[] = {
	"Deletes all the entries from the dst blacklist except the permanent ones.",  /* Documentation string */
	0                               /* Method signature(s) */
};
static const char* dst_blst_add_doc[] = {
	"Adds a new entry to the dst blacklist.",  /* Documentation string */
	0                               /* Method signature(s) */
};
#ifdef USE_DST_BLACKLIST_STATS
void dst_blst_stats_get(rpc_t* rpc, void* ctx);

static const char* dst_blst_stats_get_doc[] = {
	"returns the dst blacklist measurement counters.",
	0
};
#endif /* USE_DST_BLACKLIST_STATS */

#endif



#define MAX_CTIME_LEN 128

/* up time */
static char up_since_ctime[MAX_CTIME_LEN];


static const char* system_listMethods_doc[] = {
	"Lists all RPC methods supported by the server.",  /* Documentation string */
	0                                                  /* Method signature(s) */
};

static void system_listMethods(rpc_t* rpc, void* c)
{
	int i;

	for(i=0; i<rpc_sarray_crt_size; i++){
		if (rpc->add(c, "s", rpc_sarray[i]->name) < 0) return;
	}
}

static const char* system_methodSignature_doc[] = {
	"Returns signature of given method.",  /* Documentation string */
	0                                      /* Method signature(s) */
};

static void system_methodSignature(rpc_t* rpc, void* c)
{
	rpc->fault(c, 500, "Not Implemented Yet");
}


static const char* system_methodHelp_doc[] = {
	"Print the help string for given method.",  /* Documentation string */
	0                                           /* Method signature(s) */
};

static void system_methodHelp(rpc_t* rpc, void* c)
{
	rpc_export_t* r;
	char* name;

	if (rpc->scan(c, "s", &name) < 1) {
		rpc->fault(c, 400, "Method Name Expected");
		return;
	}

	r=rpc_lookup(name, strlen(name));
	if (r==0){
		rpc->fault(c, 400, "command not found");
	}else{
		if (r->doc_str && r->doc_str[0]) {
			rpc->add(c, "s", r->doc_str[0]);
		} else {
			rpc->add(c, "s", "undocumented");
		}
	}
	return;
}


static const char* core_prints_doc[] = {
	"Returns the strings given as parameters.",   /* Documentation string */
	0                                             /* Method signature(s) */
};


static void core_prints(rpc_t* rpc, void* c)
{
	char* string = 0;
	while((rpc->scan(c, "*s", &string)>0))
		rpc->add(c, "s", string);
}


static const char* core_printi_doc[] = {
	"Returns the integers given as parameters.",  /* Documentation string */
	0                                             /* Method signature(s) */
};


static void core_printi(rpc_t* rpc, void* c)
{
	int i;
	while((rpc->scan(c, "*d", &i)>0))
		rpc->add(c, "d", i);
}


static const char* core_echo_doc[] = {
	"Returns back its parameters.",              /* Documentation string */
	0                                             /* Method signature(s) */
};


static void core_echo(rpc_t* rpc, void* c)
{
	char* string = 0;
	while((rpc->scan(c, "*.s", &string)>0))
		rpc->add(c, "s", string);
}


static const char* core_version_doc[] = {
	"Returns the version string of the server.", /* Documentation string */
	0                                           /* Method signature(s) */
};

static void core_version(rpc_t* rpc, void* c)
{
	rpc->add(c, "s", full_version);
}



static const char* core_flags_doc[] = {
	"Returns the compile flags.", /* Documentation string */
	0                             /* Method signature(s) */
};

static void core_flags(rpc_t* rpc, void* c)
{
	rpc->add(c, "s", ver_flags);
}



static const char* core_info_doc[] = {
	"Verbose info, including version number, compile flags, compiler,"
	"repository hash a.s.o.",     /* Documentation string */
	0                             /* Method signature(s) */
};

static void core_info(rpc_t* rpc, void* c)
{
	void* s;

	if (rpc->add(c, "{", &s) < 0) return;
	rpc->struct_printf(s, "version", "%s %s", ver_name, ver_version);
	rpc->struct_add(s, "s", "id", ver_id);
	rpc->struct_add(s, "s", "compiler", ver_compiler);
	rpc->struct_add(s, "s", "compiled", ver_compiled_time);
	rpc->struct_add(s, "s", "flags", ver_flags);
}



static const char* core_uptime_doc[] = {
	"Returns uptime of SIP server.",  /* Documentation string */
	0                                 /* Method signature(s) */
};


static void core_uptime(rpc_t* rpc, void* c)
{
	void* s;
	time_t now;
	str snow;

	time(&now);

	if (rpc->add(c, "{", &s) < 0) return;
	snow.s = ctime(&now);
	if(snow.s) {
		snow.len = strlen(snow.s);
		if(snow.len>2 && snow.s[snow.len-1]=='\n') snow.len--;
		rpc->struct_add(s, "S", "now", &snow);
	}
	rpc->struct_add(s, "s", "up_since", up_since_ctime);
	/* no need for a float here (unless you're concerned that your uptime)
	rpc->struct_add(s, "f", "uptime",  difftime(now, up_since));
	*/
	/* on posix system we can substract time_t directly */
	rpc->struct_add(s, "d", "uptime",  (int)(now-up_since));
}


static const char* core_ps_doc[] = {
	"Returns the description of running processes.",  /* Documentation string */
	0                                                     /* Method signature(s) */
};


static void core_ps(rpc_t* rpc, void* c)
{
	int p;

	for (p=0; p<*process_count;p++) {
		rpc->add(c, "d", pt[p].pid);
		rpc->add(c, "s", pt[p].desc);
	}
}

static const char* core_psx_doc[] = {
	"Returns the detailed description of running processes.",
		/* Documentation string */
	0	/* Method signature(s) */
};


static void core_psx(rpc_t* rpc, void* c)
{
	int p;
	void *handle;

	for (p=0; p<*process_count;p++) {
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "dds",
				"IDX", p,
				"PID", pt[p].pid,
				"DSC", pt[p].desc);
	}
}


static const char* core_pwd_doc[] = {
	"Returns the working directory of server.",    /* Documentation string */
	0                                              /* Method signature(s) */
};


static void core_pwd(rpc_t* rpc, void* c)
{
	char *cwd_buf;
	int max_len;

	max_len = pathmax();
	cwd_buf = pkg_malloc(max_len);
	if (!cwd_buf) {
		ERR("core_pwd: No memory left\n");
		rpc->fault(c, 500, "Server Ran Out of Memory");
		return;
	}

	if (getcwd(cwd_buf, max_len)) {
		rpc->add(c, "s", cwd_buf);
	} else {
		rpc->fault(c, 500, "getcwd Failed");
	}
	pkg_free(cwd_buf);
}


static const char* core_arg_doc[] = {
	"Returns the list of command line arguments used on startup.",  /* Documentation string */
	0                                                               /* Method signature(s) */
};


static void core_arg(rpc_t* rpc, void* c)
{
	int p;

	for (p = 0; p < my_argc; p++) {
		if (rpc->add(c, "s", my_argv[p]) < 0) return;
	}
}


static const char* core_kill_doc[] = {
	"Sends the given signal to server.",  /* Documentation string */
	0                                     /* Method signature(s) */
};


static void core_kill(rpc_t* rpc, void* c)
{
	int sig_no = 15;
	rpc->scan(c, "d", &sig_no);
	rpc->send(c);
	kill(0, sig_no);
}

static void core_shmmem(rpc_t* rpc, void* c)
{
	struct mem_info mi;
	void *handle;
	char* param;
	long rs;

	rs=0;
	/* look for optional size/divisor parameter */
	if (rpc->scan(c, "*s", &param)>0){
		switch(*param){
			case 'b':
			case 'B':
				rs=0;
				break;
			case 'k':
			case 'K':
				rs=10; /* K -> 1024 */
				break;
			case 'm':
			case 'M':
				rs=20; /* M -> 1048576 */
				break;
			case 'g':
			case 'G':
				rs=30; /* G -> 1024M */
				break;
			default:
				rpc->fault(c, 500, "bad param, (use b|k|m|g)");
				return;
		}
		if (param[1] && ((param[1]!='b' && param[1]!='B') || param[2])){
				rpc->fault(c, 500, "bad param, (use b|k|m|g)");
				return;
		}
	}
	shm_info(&mi);
	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "dddddd",
		"total", (unsigned int)(mi.total_size>>rs),
		"free", (unsigned int)(mi.free>>rs),
		"used", (unsigned int)(mi.used>>rs),
		"real_used",(unsigned int)(mi.real_used>>rs),
		"max_used", (unsigned int)(mi.max_used>>rs),
		"fragments", (unsigned int)mi.total_frags
	);
}

static const char* core_shmmem_doc[] = {
	"Returns shared memory info. It has an optional parameter that specifies"
	" the measuring unit: b - bytes (default), k or kb, m or mb, g or gb. "
	"Note: when using something different from bytes, the value is truncated.",
	0                               /* Method signature(s) */
};


#if defined(SF_MALLOC) || defined(LL_MALLOC)
static void core_sfmalloc(rpc_t* rpc, void* c)
{
	void *handle;
	int i,r;
	unsigned long frags, main_s_frags, main_b_frags, pool_frags;
	unsigned long misses;
	unsigned long max_misses;
	unsigned long max_frags;
	unsigned long max_mem;
	int max_frags_pool, max_frags_hash;
	int max_misses_pool, max_misses_hash;
	int max_mem_pool, max_mem_hash;
	unsigned long mem;

	if (rpc->scan(c, "d", &r) >= 1) {
		if (r>=(int)SF_HASH_POOL_SIZE){
			rpc->fault(c, 500, "invalid hash number %d (max %d)",
								r, (unsigned int)SF_HASH_POOL_SIZE-1);
			return;
		}else if (r<0) goto all;
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "dd",
				"hash  ", r,
				"size  ", r*SF_ROUNDTO);
		for (i=0; i<SFM_POOLS_NO; i++){
			rpc->struct_add(handle, "dddd",
				"pool  ", i,
				"frags ", (unsigned int)shm_block->pool[i].pool_hash[r].no,
				"misses", (unsigned int)shm_block->pool[i].pool_hash[r].misses,
				"mem   ",   (unsigned int)shm_block->pool[i].pool_hash[r].no *
							r*SF_ROUNDTO
			);
		}
	}
	return;
all:
	max_frags=max_misses=max_mem=0;
	max_frags_pool=max_frags_hash=0;
	max_misses_pool=max_misses_hash=0;
	max_mem_pool=max_mem_hash=0;
	pool_frags=0;
	for (i=0; i<SFM_POOLS_NO; i++){
		frags=0;
		misses=0;
		mem=0;
		for (r=0; r<SF_HASH_POOL_SIZE; r++){
			frags+=shm_block->pool[i].pool_hash[r].no;
			misses+=shm_block->pool[i].pool_hash[r].misses;
			mem+=shm_block->pool[i].pool_hash[r].no*r*SF_ROUNDTO;
			if (shm_block->pool[i].pool_hash[r].no>max_frags){
				max_frags=shm_block->pool[i].pool_hash[r].no;
				max_frags_pool=i;
				max_frags_hash=r;
			}
			if (shm_block->pool[i].pool_hash[r].misses>max_misses){
				max_misses=shm_block->pool[i].pool_hash[r].misses;
				max_misses_pool=i;
				max_misses_hash=r;
			}
			if (shm_block->pool[i].pool_hash[r].no*r*SF_ROUNDTO>max_mem){
				max_mem=shm_block->pool[i].pool_hash[r].no*r*SF_ROUNDTO;
				max_mem_pool=i;
				max_mem_hash=r;
			}
		}
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "dddddd",
			"pool  ", i,
			"frags ", (unsigned int)frags,
			"t. misses", (unsigned int)misses,
			"mem   ", (unsigned int)mem,
			"missed", (unsigned int)shm_block->pool[i].missed,
			"hits",   (unsigned int)shm_block->pool[i].hits
		);
		pool_frags+=frags;
	}
	main_s_frags=0;
	for (r=0; r<SF_HASH_POOL_SIZE; r++){
		main_s_frags+=shm_block->free_hash[r].no;
	}
	main_b_frags=0;
	for (; r<SF_HASH_SIZE; r++){
		main_b_frags+=shm_block->free_hash[r].no;
	}
	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "ddddddddddddd",
		"max_frags      ", (unsigned int)max_frags,
		"max_frags_pool ", max_frags_pool,
		"max_frags_hash", max_frags_hash,
		"max_misses     ", (unsigned int)max_misses,
		"max_misses_pool", max_misses_pool,
		"max_misses_hash", max_misses_hash,
		"max_mem        ", (unsigned int)max_mem,
		"max_mem_pool   ", max_mem_pool,
		"max_mem_hash   ", max_mem_hash,
		"in_pools_frags ", (unsigned int)pool_frags,
		"main_s_frags   ", (unsigned int)main_s_frags,
		"main_b_frags   ", (unsigned int)main_b_frags,
		"main_frags     ", (unsigned int)(main_b_frags+main_s_frags)
	);
}



static const char* core_sfmalloc_doc[] = {
	"Returns sfmalloc debugging info.",  /* Documentation string */
	0                                     /* Method signature(s) */
};

#endif



static const char* core_tcpinfo_doc[] = {
	"Returns tcp related info.",    /* Documentation string */
	0                               /* Method signature(s) */
};

static void core_tcpinfo(rpc_t* rpc, void* c)
{
#ifdef USE_TCP
	void *handle;
	struct tcp_gen_info ti;

	if (!tcp_disable){
		tcp_get_info(&ti);
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "dddddd",
			"readers", ti.tcp_readers,
			"max_connections", ti.tcp_max_connections,
			"max_tls_connections", ti.tls_max_connections,
			"opened_connections", ti.tcp_connections_no,
			"opened_tls_connections", ti.tls_connections_no,
			"write_queued_bytes", ti.tcp_write_queued
		);
	}else{
		rpc->fault(c, 500, "tcp support disabled");
	}
#else
	rpc->fault(c, 500, "tcp support not compiled");
#endif
}



static const char* core_tcp_options_doc[] = {
	"Returns active tcp options.",    /* Documentation string */
	0                                 /* Method signature(s) */
};

static void core_tcp_options(rpc_t* rpc, void* c)
{
#ifdef USE_TCP
	void *handle;
	struct cfg_group_tcp t;

	if (!tcp_disable){
		tcp_options_get(&t);
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "ddddddddddddddddddddddd",
			"connect_timeout", t.connect_timeout_s,
			"send_timeout",  TICKS_TO_S(t.send_timeout),
			"connection_lifetime",  TICKS_TO_S(t.con_lifetime),
			"max_connections(soft)", t.max_connections,
			"max_tls_connections(soft)", t.max_tls_connections,
			"no_connect",	t.no_connect,
			"fd_cache",		t.fd_cache,
			"async",		t.async,
			"connect_wait",	t.tcp_connect_wait,
			"conn_wq_max",	t.tcpconn_wq_max,
			"wq_max",		t.tcp_wq_max,
			"defer_accept",	t.defer_accept,
			"delayed_ack",	t.delayed_ack,
			"syncnt",		t.syncnt,
			"linger2",		t.linger2,
			"keepalive",	t.keepalive,
			"keepidle",		t.keepidle,
			"keepintvl",	t.keepintvl,
			"keepcnt",		t.keepcnt,
			"crlf_ping",	t.crlf_ping,
			"accept_aliases", t.accept_aliases,
			"alias_flags",	t.alias_flags,
			"new_conn_alias_flags",	t.new_conn_alias_flags
		);
	}else{
		rpc->fault(c, 500, "tcp support disabled");
	}
#else
	rpc->fault(c, 500, "tcp support not compiled");
#endif
}


static const char* core_tcp_list_doc[] = {
	"Returns tcp connections details.",    /* Documentation string */
	0                               /* Method signature(s) */
};

extern gen_lock_t* tcpconn_lock;
extern struct tcp_connection** tcpconn_id_hash;

static void core_tcp_list(rpc_t* rpc, void* c)
{
#ifdef USE_TCP
	char src_ip[IP_ADDR_MAX_STR_SIZE];
	char dst_ip[IP_ADDR_MAX_STR_SIZE];
	void* handle;
	char* state;
	char* type;
	struct tcp_connection* con;
	int i, len, timeout, lifetime;

	if (tcp_disable) {
		rpc->fault(c, 500, "tcp support disabled");
		return;
	}

	TCPCONN_LOCK;
	for(i = 0; i < TCP_ID_HASH_SIZE; i++) {
		for (con = tcpconn_id_hash[i]; con; con = con->id_next) {
			rpc->add(c, "{", &handle);
			/* tcp data */
			if (con->rcv.proto == PROTO_TCP)
				type = "TCP";
			else if (con->rcv.proto == PROTO_TLS)
				type = "TLS";
			else if (con->rcv.proto == PROTO_WSS)
				type = "WSS";
			else if (con->rcv.proto == PROTO_WS)
				type = "WS";
			else
				type = "UNKNOWN";

			if ((len = ip_addr2sbuf(&con->rcv.src_ip, src_ip, sizeof(src_ip)))
					== 0)
				BUG("failed to convert source ip");
			src_ip[len] = 0;
			if ((len = ip_addr2sbuf(&con->rcv.dst_ip, dst_ip, sizeof(dst_ip)))
					== 0)
				BUG("failed to convert destination ip");
			dst_ip[len] = 0;
			timeout = TICKS_TO_S(con->timeout - get_ticks_raw());
			lifetime = TICKS_TO_S(con->lifetime);
			switch(con->state) {
				case S_CONN_ERROR:
					state = "CONN_ERROR";
				break;
				case S_CONN_BAD:
					state = "CONN_BAD";
				break;
				case S_CONN_OK:
					state = "CONN_OK";
				break;
				case S_CONN_INIT:
					state = "CONN_INIT";
				break;
				case S_CONN_EOF:
					state = "CONN_EOF";
				break;
				case S_CONN_ACCEPT:
					state = "CONN_ACCEPT";
				break;
				case S_CONN_CONNECT:
					state = "CONN_CONNECT";
				break;
				default:
					state = "UNKNOWN";
			}
			rpc->struct_add(handle, "dssdddsdsd",
					"id", con->id,
					"type", type,
					"state", state,
					"timeout", timeout,
					"lifetime", lifetime,
					"ref_count", con->refcnt,
					"src_ip", src_ip,
					"src_port", con->rcv.src_port,
					"dst_ip", dst_ip,
					"dst_port", con->rcv.dst_port);
		}
	}
	TCPCONN_UNLOCK;
#else
	rpc->fault(c, 500, "tcp support not compiled");
#endif
}


static const char* core_udp4rawinfo_doc[] = {
	"Returns udp4_raw related info.",    /* Documentation string */
	0                                     /* Method signature(s) */
};

static void core_udp4rawinfo(rpc_t* rpc, void* c)
{
#ifdef USE_RAW_SOCKS
	void *handle;

	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "ddd",
		"udp4_raw", cfg_get(core, core_cfg, udp4_raw),
		"udp4_raw_mtu", cfg_get(core, core_cfg, udp4_raw_mtu),
		"udp4_raw_ttl", cfg_get(core, core_cfg, udp4_raw_ttl)
	);
#else /* USE_RAW_SOCKS */
	rpc->fault(c, 500, "udp4_raw mode support not compiled");
#endif /* USE_RAW_SOCKS */
}

/**
 *
 */
static const char* core_aliases_list_doc[] = {
	"List local SIP server host aliases",    /* Documentation string */
	0                                     /* Method signature(s) */
};

/**
 * list the name aliases for SIP server
 */
static void core_aliases_list(rpc_t* rpc, void* c)
{
	void *hr;
	void *hs;
	void *ha;
	struct host_alias* a;

	rpc->add(c, "{", &hr);
	rpc->struct_add(hr, "s",
			"myself_callbacks", is_check_self_func_list_set()?"yes":"no");
	rpc->struct_add(hr, "[", "aliases", &hs);
	for(a=aliases; a; a=a->next) {
		rpc->struct_add(hs, "{", "alias", &ha);
		rpc->struct_add(ha, "sS",
				"proto",  proto2a(a->proto),
				"address", &a->alias
			);
		if (a->port)
			rpc->struct_add(ha, "d",
					"port", a->port);
		else
			rpc->struct_add(ha, "s",
					"port", "*");
	}
}

/**
 *
 */
static const char* core_sockets_list_doc[] = {
	"List local SIP server listen sockets",    /* Documentation string */
	0                                          /* Method signature(s) */
};

/**
 * list listen sockets for SIP server
 */
static void core_sockets_list(rpc_t* rpc, void* c)
{
	void *hr;
	void *ha;
	struct socket_info *si;
	struct socket_info** list;
	struct addr_info* ai;
	unsigned short proto;

	proto=PROTO_UDP;
	rpc->add(c, "[", &hr);
	do{
		list=get_sock_info_list(proto);
		for(si=list?*list:0; si; si=si->next){
			rpc->struct_add(hr, "{", "socket", &ha);
			if (si->addr_info_lst){
				rpc->struct_add(ha, "ss",
						"proto", get_proto_name(proto),
						"address", si->address_str.s);
				for (ai=si->addr_info_lst; ai; ai=ai->next)
					rpc->struct_add(ha, "ss",
						"address", ai->address_str.s);
				rpc->struct_add(ha, "sss",
						"port", si->port_no_str.s,
						"mcast", si->flags & SI_IS_MCAST ? "yes" : "no",
						"mhomed", si->flags & SI_IS_MHOMED ? "yes" : "no");
			} else {
				printf("             %s: %s",
						get_proto_name(proto),
						si->name.s);
				rpc->struct_add(ha, "ss",
						"proto", get_proto_name(proto),
						"address", si->name.s);
				if (!(si->flags & SI_IS_IP))
					rpc->struct_add(ha, "s",
						"ipaddress", si->address_str.s);
				rpc->struct_add(ha, "sss",
						"port", si->port_no_str.s,
						"mcast", si->flags & SI_IS_MCAST ? "yes" : "no",
						"mhomed", si->flags & SI_IS_MHOMED ? "yes" : "no");
			}
		}
	} while((proto=next_proto(proto)));
}

/**
 *
 */
static const char* core_modules_doc[] = {
	"List loaded modules",    /* Documentation string */
	0                               /* Method signature(s) */
};

/**
 * list listen sockets for SIP server
 */
static void core_modules(rpc_t* rpc, void* c)
{
	sr_module_t* t;

	for(t = get_loaded_modules(); t; t = t->next) {
		if (rpc->add(c, "s", t->exports.name) < 0) return;
	}
}

/**
 *
 */
static const char* core_ppdefines_doc[] = {
	"List preprocessor defines",    /* Documentation string */
	0                               /* Method signature(s) */
};

/**
 * list listen sockets for SIP server
 */
static void core_ppdefines(rpc_t* rpc, void* c)
{
	str *ppdef;
	int i=0;

	while((ppdef=pp_get_define_name(i))!=NULL) {
		if (rpc->add(c, "s", ppdef->s) < 0) return;
		i++;
	}
}

/*
 * RPC Methods exported by core
 */
static rpc_export_t core_rpc_methods[] = {
	{"system.listMethods",     system_listMethods,     system_listMethods_doc,     RET_ARRAY},
	{"system.methodSignature", system_methodSignature, system_methodSignature_doc, 0        },
	{"system.methodHelp",      system_methodHelp,      system_methodHelp_doc,      0        },
	{"core.prints",            core_prints,            core_prints_doc,
	RET_ARRAY},
	{"core.printi",            core_printi,            core_printi_doc,
	RET_ARRAY},
	{"core.echo",              core_echo,              core_echo_doc,
	RET_ARRAY},
	{"core.version",           core_version,           core_version_doc,
		0        },
	{"core.flags",             core_flags,             core_flags_doc,
		0        },
	{"core.info",              core_info,              core_info_doc,
		0        },
	{"core.uptime",            core_uptime,            core_uptime_doc,            0        },
	{"core.ps",                core_ps,                core_ps_doc,                RET_ARRAY},
	{"core.psx",               core_psx,               core_psx_doc,               RET_ARRAY},
	{"core.pwd",               core_pwd,               core_pwd_doc,               RET_ARRAY},
	{"core.arg",               core_arg,               core_arg_doc,               RET_ARRAY},
	{"core.kill",              core_kill,              core_kill_doc,              0        },
	{"core.shmmem",            core_shmmem,            core_shmmem_doc,            0	},
#if defined(SF_MALLOC) || defined(LL_MALLOC)
	{"core.sfmalloc",          core_sfmalloc,          core_sfmalloc_doc,   0},
#endif
	{"core.tcp_info",          core_tcpinfo,           core_tcpinfo_doc,    0},
	{"core.tcp_options",       core_tcp_options,       core_tcp_options_doc,0},
	{"core.tcp_list",          core_tcp_list,          core_tcp_list_doc,0},
	{"core.udp4_raw_info",     core_udp4rawinfo,       core_udp4rawinfo_doc,
		0},
	{"core.aliases_list",      core_aliases_list,      core_aliases_list_doc, 0},
	{"core.sockets_list",      core_sockets_list,      core_sockets_list_doc, 0},
	{"core.modules",           core_modules,           core_modules_doc,    RET_ARRAY},
	{"core.ppdefines",         core_ppdefines,         core_ppdefines_doc,  RET_ARRAY},
#ifdef USE_DNS_CACHE
	{"dns.mem_info",          dns_cache_mem_info,     dns_cache_mem_info_doc,
		0	},
	{"dns.debug",          dns_cache_debug,           dns_cache_debug_doc,
		0	},
	{"dns.debug_all",      dns_cache_debug_all,       dns_cache_debug_all_doc,
		0	},
	{"dns.view",               dns_cache_view,        dns_cache_view_doc,
		RET_ARRAY	},
	{"dns.lookup",             dns_cache_rpc_lookup,  dns_cache_rpc_lookup_doc,
		0	},
	{"dns.delete_all",         dns_cache_delete_all,  dns_cache_delete_all_doc,
		0	},
	{"dns.delete_all_force",   dns_cache_delete_all_force, dns_cache_delete_all_force_doc,
		0	},
	{"dns.add_a",              dns_cache_add_a,       dns_cache_add_a_doc,
		0	},
	{"dns.add_aaaa",           dns_cache_add_aaaa,    dns_cache_add_aaaa_doc,
		0	},
	{"dns.add_srv",            dns_cache_add_srv,     dns_cache_add_srv_doc,
		0	},
	{"dns.delete_a",           dns_cache_delete_a,    dns_cache_delete_a_doc,
		0	},
	{"dns.delete_aaaa",        dns_cache_delete_aaaa,
		dns_cache_delete_aaaa_doc, 0	},
	{"dns.delete_srv",         dns_cache_delete_srv,
		dns_cache_delete_srv_doc,  0	},
	{"dns.delete_naptr",         dns_cache_delete_naptr,
		dns_cache_delete_naptr_doc,  0	},
	{"dns.delete_cname",         dns_cache_delete_cname,
		dns_cache_delete_cname_doc,  0	},
	{"dns.delete_txt",         dns_cache_delete_txt,
		dns_cache_delete_txt_doc,  0	},
	{"dns.delete_ebl",         dns_cache_delete_ebl,
		dns_cache_delete_ebl_doc,  0	},
	{"dns.delete_ptr",         dns_cache_delete_ptr,
		dns_cache_delete_ptr_doc,  0	},
#ifdef USE_DNS_CACHE_STATS
	{"dns.stats_get",    dns_cache_stats_get,   dns_cache_stats_get_doc,
		0	},
#endif /* USE_DNS_CACHE_STATS */
#ifdef DNS_WATCHDOG_SUPPORT
	{"dns.set_server_state",   dns_set_server_state_rpc,
		dns_set_server_state_doc, 0 },
	{"dns.get_server_state",   dns_get_server_state_rpc,
		dns_get_server_state_doc, 0 },
#endif
#endif
#ifdef USE_DST_BLACKLIST
	{"dst_blacklist.mem_info",  dst_blst_mem_info,     dst_blst_mem_info_doc,
		0	},
	{"dst_blacklist.debug",    dst_blst_debug,         dst_blst_debug_doc,
		0	},
	{"dst_blacklist.view",     dst_blst_view,         dst_blst_view_doc,
		0	},
	{"dst_blacklist.delete_all", dst_blst_delete_all, dst_blst_delete_all_doc,
		0	},
	{"dst_blacklist.add",      dst_blst_add,          dst_blst_add_doc,
		0	},
#ifdef USE_DST_BLACKLIST_STATS
	{"dst_blacklist.stats_get", dst_blst_stats_get, dst_blst_stats_get_doc, 0},
#endif /* USE_DST_BLACKLIST_STATS */
#endif
	{0, 0, 0, 0}
};



int register_core_rpcs(void)
{
	int i;

	i=rpc_register_array(core_rpc_methods);
	if (i<0){
		BUG("failed to register core RPCs\n");
		goto error;
	}else if (i>0){
		ERR("%d duplicate RPCs name detected while registering core RPCs\n", i);
		goto error;
	}
	return 0;
error:
	return -1;
}



int rpc_init_time(void)
{
	char *t;
	t=ctime(&up_since);
	if (strlen(t)+1>=MAX_CTIME_LEN) {
		ERR("Too long data %d\n", (int)strlen(t));
		return -1;
	}
	strcpy(up_since_ctime, t);
	t = up_since_ctime + strlen(up_since_ctime);
	while(t>up_since_ctime) {
		if(*t=='\0' || *t=='\r' || *t=='\n') {
			*t = '\0';
		} else {
			break;
		}
		t--;
	}
	return 0;
}
