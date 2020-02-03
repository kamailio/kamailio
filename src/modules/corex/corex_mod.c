/**
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/socket_info.h"
#include "../../core/resolve.h"
#include "../../core/lvalue.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_uri.h"

#include "corex_lib.h"
#include "corex_rpc.h"
#include "corex_var.h"
#include "corex_nio.h"

MODULE_VERSION

static int nio_intercept = 0;
static int w_append_branch(sip_msg_t *msg, char *su, char *sq);
static int w_send_udp(sip_msg_t *msg, char *su, char *sq);
static int w_send_tcp(sip_msg_t *msg, char *su, char *sq);
static int w_send_data(sip_msg_t *msg, char *suri, char *sdata);
static int w_sendx(sip_msg_t *msg, char *suri, char *ssock, char *sdata);
static int w_msg_iflag_set(sip_msg_t *msg, char *pflag, char *p2);
static int w_msg_iflag_reset(sip_msg_t *msg, char *pflag, char *p2);
static int w_msg_iflag_is_set(sip_msg_t *msg, char *pflag, char *p2);
static int w_file_read(sip_msg_t *msg, char *fn, char *vn);
static int w_file_write(sip_msg_t *msg, char *fn, char *vn);
static int w_isxflagset(struct sip_msg *msg, char *flag, char *s2);
static int w_resetxflag(struct sip_msg *msg, char *flag, char *s2);
static int w_setxflag(struct sip_msg *msg, char *flag, char *s2);
static int w_set_send_socket(sip_msg_t *msg, char *psock, char *p2);
static int w_set_recv_socket(sip_msg_t *msg, char *psock, char *p2);
static int w_set_source_address(sip_msg_t *msg, char *paddr, char *p2);
static int w_via_add_srvid(sip_msg_t *msg, char *pflags, char *p2);
static int w_via_add_xavp_params(sip_msg_t *msg, char *pflags, char *p2);
static int w_via_use_xavp_fields(sip_msg_t *msg, char *pflags, char *p2);

static int fixup_file_op(void** param, int param_no);

int corex_alias_subdomains_param(modparam_t type, void *val);

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static pv_export_t mod_pvs[] = {
	{ {"cfg", (sizeof("cfg")-1)}, PVT_OTHER, pv_get_cfg, 0,
		pv_parse_cfg_name, 0, 0, 0 },

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"append_branch", (cmd_function)w_append_branch, 0, 0,
		0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"append_branch", (cmd_function)w_append_branch, 1, fixup_spve_null,
		0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"append_branch", (cmd_function)w_append_branch, 2, fixup_spve_spve,
		0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_udp", (cmd_function)w_send_udp, 0, 0,
		0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_udp", (cmd_function)w_send_udp, 1, fixup_spve_null,
		0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_tcp", (cmd_function)w_send_tcp, 0, 0,
		0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_tcp", (cmd_function)w_send_tcp, 1, fixup_spve_null,
		0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"send_data", (cmd_function)w_send_data, 2, fixup_spve_spve,
		0, ANY_ROUTE },
	{"sendx", (cmd_function)w_sendx, 3, fixup_spve_all,
		0, ANY_ROUTE },
	{"is_incoming",    (cmd_function)nio_check_incoming, 0, 0,
		0, ANY_ROUTE },
	{"msg_iflag_set", (cmd_function)w_msg_iflag_set,       1, fixup_spve_null,
		0, ANY_ROUTE },
	{"msg_iflag_reset", (cmd_function)w_msg_iflag_reset,   1, fixup_spve_null,
		0, ANY_ROUTE },
	{"msg_iflag_is_set", (cmd_function)w_msg_iflag_is_set, 1, fixup_spve_null,
		0, ANY_ROUTE },
	{"file_read", (cmd_function)w_file_read,   2, fixup_file_op,
		0, ANY_ROUTE },
	{"file_write", (cmd_function)w_file_write, 2, fixup_spve_spve,
		0, ANY_ROUTE },
	{"setxflag", (cmd_function)w_setxflag,          1,fixup_igp_null,
		0, ANY_ROUTE },
	{"resetxflag", (cmd_function)w_resetxflag,      1,fixup_igp_null,
		0, ANY_ROUTE },
	{"isxflagset", (cmd_function)w_isxflagset,      1,fixup_igp_null,
		0, ANY_ROUTE },
	{"set_send_socket", (cmd_function)w_set_send_socket, 1, fixup_spve_null,
		0, ANY_ROUTE },
	{"set_recv_socket", (cmd_function)w_set_recv_socket, 1, fixup_spve_null,
		0, ANY_ROUTE },
	{"set_source_address", (cmd_function)w_set_source_address, 1, fixup_spve_null,
		0, ANY_ROUTE },
	{"via_add_srvid", (cmd_function)w_via_add_srvid, 1, fixup_igp_null,
		0, ANY_ROUTE },
	{"via_add_xavp_params", (cmd_function)w_via_add_xavp_params, 1, fixup_igp_null,
		0, ANY_ROUTE },
	{"via_use_xavp_fields", (cmd_function)w_via_use_xavp_fields, 1, fixup_igp_null,
		0, ANY_ROUTE },

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"alias_subdomains",		STR_PARAM|USE_FUNC_PARAM,
		(void*)corex_alias_subdomains_param},
	{"network_io_intercept",	INT_PARAM, &nio_intercept},
	{"min_msg_len",				INT_PARAM, &nio_min_msg_len},
	{"msg_avp",					PARAM_STR, &nio_msg_avp_param},

	{0, 0, 0}
};

struct module_exports exports = {
	"corex",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	if(corex_init_rpc()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(corex_register_check_self()<0)
	{
		LM_ERR("failed to register check self callback\n");
		return -1;
	}

	if((nio_intercept > 0) && (nio_intercept_init() < 0))
	{
		LM_ERR("failed to register network io intercept callback\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	if (rank!=PROC_MAIN)
		return 0;

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 * config wrapper for append branch
 */
static int w_append_branch(sip_msg_t *msg, char *su, char *sq)
{
	if(w_corex_append_branch(msg, (gparam_t*)su, (gparam_t*)sq) < 0)
		return -1;
	return 1;
}

/**
 * config wrapper for send_udp()
 */
static int w_send_udp(sip_msg_t *msg, char *su, char *sq)
{
	if(corex_send(msg, (gparam_t*)su, PROTO_UDP) < 0)
		return -1;
	return 1;
}

/**
 * config wrapper for send_tcp()
 */
static int w_send_tcp(sip_msg_t *msg, char *su, char *sq)
{
	if(corex_send(msg, (gparam_t*)su, PROTO_TCP) < 0)
		return -1;
	return 1;
}

static int w_send_data(sip_msg_t *msg, char *suri, char *sdata)
{
	str uri;
	str data;

	if (fixup_get_svalue(msg, (gparam_t*)suri, &uri))
	{
		LM_ERR("cannot get the destination parameter\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)sdata, &data))
	{
		LM_ERR("cannot get the destination parameter\n");
		return -1;
	}
	if(corex_send_data(&uri, NULL, &data) < 0)
		return -1;
	return 1;
}

static int ki_send_data(sip_msg_t *msg, str *uri, str *data)
{
	if(corex_send_data(uri, NULL, data) < 0)
		return -1;
	return 1;
}

static int w_sendx(sip_msg_t *msg, char *suri, char *ssock, char *sdata)
{
	str uri;
	str sock;
	str data;

	if (fixup_get_svalue(msg, (gparam_t*)suri, &uri))
	{
		LM_ERR("cannot get the destination parameter\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)ssock, &sock))
	{
		LM_ERR("cannot get the socket parameter\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)sdata, &data))
	{
		LM_ERR("cannot get the destination parameter\n");
		return -1;
	}
	if(corex_send_data(&uri, &sock, &data) < 0)
		return -1;
	return 1;
}

static int ki_sendx(sip_msg_t *msg, str *uri, str *sock, str *data)
{
	if(corex_send_data(uri, sock, data) < 0)
		return -1;
	return 1;
}

int corex_alias_subdomains_param(modparam_t type, void *val)
{
	if(val==NULL)
		goto error;

	return corex_add_alias_subdomains((char*)val);
error:
	return -1;

}

typedef struct _msg_iflag_name {
	str name;
	int value;
} msg_iflag_name_t;

static msg_iflag_name_t _msg_iflag_list[] = {
	{ str_init("USE_UAC_FROM"), FL_USE_UAC_FROM },
	{ str_init("USE_UAC_TO"),   FL_USE_UAC_TO   },
	{ str_init("UAC_AUTH"),     FL_UAC_AUTH     },
	{ {0, 0}, 0 }
};


/**
 *
 */
static int msg_lookup_flag(str *fname)
{
	int i;
	for(i=0; _msg_iflag_list[i].name.len>0; i++) {
		if(fname->len==_msg_iflag_list[i].name.len
				&& strncasecmp(_msg_iflag_list[i].name.s, fname->s,
					fname->len)==0) {
			return _msg_iflag_list[i].value;
		}
	}
	return -1;
}
/**
 *
 */
static int w_msg_iflag_set(sip_msg_t *msg, char *pflag, char *p2)
{
	int fv;
	str fname;
	if (fixup_get_svalue(msg, (gparam_t*)pflag, &fname))
	{
		LM_ERR("cannot get the msg flag name parameter\n");
		return -1;
	}
	fv =  msg_lookup_flag(&fname);
	if(fv==1) {
		LM_ERR("unsupported flag name [%.*s]\n", fname.len, fname.s);
		return -1;
	}
	msg->msg_flags |= fv;
	return 1;
}

/**
 *
 */
static int w_msg_iflag_reset(sip_msg_t *msg, char *pflag, char *p2)
{
	int fv;
	str fname;
	if (fixup_get_svalue(msg, (gparam_t*)pflag, &fname))
	{
		LM_ERR("cannot get the msg flag name parameter\n");
		return -1;
	}
	fv =  msg_lookup_flag(&fname);
	if(fv<0) {
		LM_ERR("unsupported flag name [%.*s]\n", fname.len, fname.s);
		return -1;
	}
	msg->msg_flags &= ~fv;
	return 1;
}

/**
 *
 */
static int w_msg_iflag_is_set(sip_msg_t *msg, char *pflag, char *p2)
{
	int fv;
	str fname;
	if (fixup_get_svalue(msg, (gparam_t*)pflag, &fname))
	{
		LM_ERR("cannot get the msg flag name parameter\n");
		return -1;
	}
	fv =  msg_lookup_flag(&fname);
	if(fv<0) {
		LM_ERR("unsupported flag name [%.*s]\n", fname.len, fname.s);
		return -1;
	}
	if(msg->msg_flags & fv)
		return 1;
	return -2;
}

/**
 *
 */
static int w_file_read(sip_msg_t *msg, char *fn, char *vn)
{
	str fname;
	pv_spec_t *vp;
	pv_value_t val;

	FILE *f;
	long fsize;
	char *content;

	fname.len = 0;
	if (fixup_get_svalue(msg, (gparam_p)fn, &fname) != 0 || fname.len<=0) {
		LM_ERR("cannot get file path\n");
		return -1;
	}
	LM_DBG("reading from file: %.*s\n", fname.len, fname.s);
	vp = (pv_spec_t*)vn;

	f = fopen(fname.s, "r");
	if(f==NULL) {
		LM_ERR("cannot open file: %.*s\n", fname.len, fname.s);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	if(fsize<0) {
		LM_ERR("ftell failed on file: %.*s\n", fname.len, fname.s);
		fclose(f);
		return -1;
	}
	fseek(f, 0, SEEK_SET);

	content = pkg_malloc(fsize + 1);
	if(content==NULL) {
		LM_ERR("no more pkg memory\n");
		fclose(f);
		return -1;
	}
	if(fread(content, fsize, 1, f) != fsize) {
		if(ferror(f)) {
			LM_ERR("error reading from file: %.*s\n",
				fname.len, fname.s);
		}
	}
	fclose(f);
	content[fsize] = 0;


	val.rs.s = content;
	val.rs.len = fsize;
	LM_DBG("file content: [[%.*s]]\n", val.rs.len, val.rs.s);
	val.flags = PV_VAL_STR;
	vp->setf(msg, &vp->pvp, (int)EQ_T, &val);
	pkg_free(content);

	return 1;
}

/**
 *
 */
static int w_file_write(sip_msg_t *msg, char *fn, char *vn)
{
	str fname;
	str content;
	FILE *f;

	fname.len = 0;
	if (fixup_get_svalue(msg, (gparam_p)fn, &fname) != 0 || fname.len<=0) {
		LM_ERR("cannot get file path\n");
		return -1;
	}
	content.len = 0;
	if (fixup_get_svalue(msg, (gparam_p)vn, &content) != 0 || content.len<=0) {
		LM_ERR("cannot get the content\n");
		return -1;
	}

	LM_DBG("writing to file: %.*s\n", fname.len, fname.s);
	f = fopen(fname.s, "w");
	if(f==NULL) {
		LM_ERR("cannot open file: %.*s\n", fname.len, fname.s);
		return -1;
	}
	fwrite(content.s, 1, content.len, f);
	fclose(f);

	return 1;
}

/**
 *
 */
static int fixup_file_op(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 2) {
		if (fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

/**
 *
 */
static int ki_append_branch(sip_msg_t *msg)
{
	if(corex_append_branch(msg, NULL, NULL) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_append_branch_uri(sip_msg_t *msg, str *uri)
{
	if(corex_append_branch(msg, uri, NULL) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_append_branch_uri_q(sip_msg_t *msg, str *uri, str *q)
{
	if(corex_append_branch(msg, uri, q) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_isxflagset(sip_msg_t *msg, int fval)
{
	if((flag_t)fval>KSR_MAX_XFLAG)
		return -1;
	return isxflagset(msg, (flag_t)fval);
}

/**
 *
 */
static int w_isxflagset(sip_msg_t *msg, char *flag, char *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_t*)flag, &fval)!=0) {
		LM_ERR("no flag value\n");
		return -1;
	}
	return ki_isxflagset(msg, fval);
}

/**
 *
 */
static int ki_resetxflag(sip_msg_t *msg, int fval)
{
	if((flag_t)fval>KSR_MAX_XFLAG)
		return -1;
	return resetxflag(msg, (flag_t)fval);
}

/**
 *
 */
static int w_resetxflag(sip_msg_t *msg, char *flag, char *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_t*)flag, &fval)!=0) {
		LM_ERR("no flag value\n");
		return -1;
	}
	return ki_resetxflag(msg, fval);
}

/**
 *
 */
static int ki_setxflag(sip_msg_t *msg, int fval)
{
	if((flag_t)fval>KSR_MAX_XFLAG)
		return -1;
	return setxflag(msg, (flag_t)fval);
}

/**
 *
 */
static int w_setxflag(sip_msg_t *msg, char *flag, char *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_t*)flag, &fval)!=0) {
		LM_ERR("no flag value\n");
		return -1;
	}
	return ki_setxflag(msg, fval);
}

/**
 *
 */
static int ki_set_socket_helper(sip_msg_t *msg, str *ssock, int smode)
{
	socket_info_t *si;
	int port, proto;
	str host;

	if(msg==NULL) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(ssock==NULL || ssock->len<=0) {
		reset_force_socket(msg);
		return 1;
	}

	if (parse_phostport(ssock->s, &host.s, &host.len, &port, &proto) < 0) {
		LM_ERR("invalid socket specification [%.*s] (%d)\n",
				ssock->len, ssock->s, smode);
		goto error;
	}
	LM_DBG("trying to set %s-socket to [%.*s] (%d)\n",
				(smode==0)?"snd":"rcv", ssock->len, ssock->s, smode);
	si = grep_sock_info(&host, (unsigned short)port, (unsigned short)proto);
	if (si!=NULL) {
		if(smode==0) {
			/* send socket */
			set_force_socket(msg, si);
		} else {
			/* recv socket */
			msg->rcv.bind_address = si;
			msg->rcv.dst_port = si->port_no;
			msg->rcv.dst_ip = si->address;
			msg->rcv.proto = si->proto;
			msg->rcv.proto_reserved1 = 0;
			msg->rcv.proto_reserved2 = 0;
		}
	} else {
		LM_WARN("no socket found to match [%.*s] (%d)\n",
				ssock->len, ssock->s, smode);
	}
	return 1;
error:
	return -1;
}

/**
 *
 */
static int ki_set_send_socket(sip_msg_t *msg, str *ssock)
{
	return ki_set_socket_helper(msg, ssock, 0);
}

/**
 *
 */
static int w_set_send_socket(sip_msg_t *msg, char *psock, char *p2)
{
	str ssock;
	if (fixup_get_svalue(msg, (gparam_t*)psock, &ssock) != 0 || ssock.len<=0) {
		LM_ERR("cannot get socket address value\n");
		return -1;
	}
	return ki_set_send_socket(msg, &ssock);
}

/**
 *
 */
static int ki_set_recv_socket(sip_msg_t *msg, str *ssock)
{
	return ki_set_socket_helper(msg, ssock, 1);
}

/**
 *
 */
static int w_set_recv_socket(sip_msg_t *msg, char *psock, char *p2)
{
	str ssock;
	if (fixup_get_svalue(msg, (gparam_t*)psock, &ssock) != 0 || ssock.len<=0) {
		LM_ERR("cannot get socket address value\n");
		return -1;
	}
	return ki_set_recv_socket(msg, &ssock);
}

/**
 *
 */
static int ki_set_source_address(sip_msg_t *msg, str *saddr)
{
	sr_phostp_t rp;
	union sockaddr_union faddr;
	char cproto;
	int ret;

	if(msg==NULL || saddr==NULL || saddr->len<=0) {
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(parse_protohostport(saddr, &rp)<0) {
		LM_ERR("failed to parse the address [%.*s]\n", saddr->len, saddr->s);
		return -1;
	}

	cproto = (char)rp.proto;
	ret = sip_hostport2su(&faddr, &rp.host, (unsigned short)rp.port, &cproto);
	if(ret!=0) {
		LM_ERR("failed to resolve address [%.*s]\n", saddr->len, saddr->s);
		return -1;
	}

	msg->rcv.src_su=faddr;
	su2ip_addr(&msg->rcv.src_ip, &faddr);
	msg->rcv.src_port=rp.port;

	return 1;
}

/**
 *
 */
static int w_set_source_address(sip_msg_t *msg, char *paddr, char *p2)
{
	str saddr;
	if (fixup_get_svalue(msg, (gparam_t*)paddr, &saddr) != 0 || saddr.len<=0) {
		LM_ERR("cannot get source address value\n");
		return -1;
	}
	return ki_set_source_address(msg, &saddr);
}

/**
 *
 */
static int ki_via_add_srvid(sip_msg_t *msg, int fval)
{
	if(msg==NULL)
		return -1;
	if(fval) {
		msg->msg_flags |= FL_ADD_SRVID;
	} else {
		msg->msg_flags &= ~(FL_ADD_SRVID);
	}
	return 1;
}

/**
 *
 */
static int w_via_add_srvid(sip_msg_t *msg, char *pflags, char *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_t*)pflags, &fval)!=0) {
		LM_ERR("no flag value\n");
		return -1;
	}
	return ki_via_add_srvid(msg, fval);
}

/**
 *
 */
static int ki_via_add_xavp_params(sip_msg_t *msg, int fval)
{
	if(msg==NULL)
		return -1;
	if(fval) {
		msg->msg_flags |= FL_ADD_XAVP_VIA_PARAMS;
	} else {
		msg->msg_flags &= ~(FL_ADD_XAVP_VIA_PARAMS);
	}
	return 1;
}

/**
 *
 */
static int w_via_add_xavp_params(sip_msg_t *msg, char *pflags, char *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_t*)pflags, &fval)!=0) {
		LM_ERR("no flag value\n");
		return -1;
	}
	return ki_via_add_xavp_params(msg, fval);
}

/**
 *
 */
static int ki_via_use_xavp_fields(sip_msg_t *msg, int fval)
{
	if(msg==NULL)
		return -1;
	if(fval) {
		msg->msg_flags |= FL_USE_XAVP_VIA_FIELDS;
	} else {
		msg->msg_flags &= ~(FL_USE_XAVP_VIA_FIELDS);
	}
	return 1;
}

/**
 *
 */
static int w_via_use_xavp_fields(sip_msg_t *msg, char *pflags, char *s2)
{
	int fval=0;
	if(fixup_get_ivalue(msg, (gparam_t*)pflags, &fval)!=0) {
		LM_ERR("no flag value\n");
		return -1;
	}
	return ki_via_use_xavp_fields(msg, fval);
}


/**
 *
 */
static int ki_has_ruri_user(sip_msg_t *msg)
{
	if(msg==NULL)
		return -1;

	if(msg->first_line.type == SIP_REPLY)	/* REPLY doesnt have a ruri */
		return -1;

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse the R-URI\n");
		return -1;
	}

	if(msg->parsed_uri.user.s!=NULL && msg->parsed_uri.user.len>0) {
		return 1;
	}

	return -1;
}

/**
 *
 */
static int ki_has_user_agent(sip_msg_t *msg)
{
	if(msg==NULL)
		return -1;

	if(msg->user_agent==NULL && ((parse_headers(msg, HDR_USERAGENT_F, 0)==-1)
			|| (msg->user_agent==NULL)))
	{
		LM_DBG("no User-Agent header\n");
		return -1;
	}

	if(msg->user_agent->body.s!=NULL && msg->user_agent->body.len>0) {
		return 1;
	}

	return -1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_corex_exports[] = {
	{ str_init("corex"), str_init("append_branch"),
		SR_KEMIP_INT, ki_append_branch,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("append_branch_uri"),
		SR_KEMIP_INT, ki_append_branch_uri,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("append_branch_uri_q"),
		SR_KEMIP_INT, ki_append_branch_uri_q,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("setxflag"),
		SR_KEMIP_INT, ki_setxflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("resetxflag"),
		SR_KEMIP_INT, ki_resetxflag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("isxflagset"),
		SR_KEMIP_INT, ki_isxflagset,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("set_send_socket"),
		SR_KEMIP_INT, ki_set_send_socket,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("set_recv_socket"),
		SR_KEMIP_INT, ki_set_recv_socket,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("set_source_address"),
		SR_KEMIP_INT, ki_set_source_address,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("via_add_srvid"),
		SR_KEMIP_INT, ki_via_add_srvid,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("via_add_xavp_params"),
		SR_KEMIP_INT, ki_via_add_xavp_params,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("via_use_xavp_fields"),
		SR_KEMIP_INT, ki_via_use_xavp_fields,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("has_ruri_user"),
		SR_KEMIP_INT, ki_has_ruri_user,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("has_user_agent"),
		SR_KEMIP_INT, ki_has_user_agent,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("send_data"),
		SR_KEMIP_INT, ki_send_data,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("corex"), str_init("sendx"),
		SR_KEMIP_INT, ki_sendx,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_corex_exports);
	return 0;
}
