/**
 * Copyright 2015 (C) Orange
 * <camille.oudot@orange.com>
 *
 * Copyright 2015 (C) Edvina AB
 * <oej@edvina.net>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/globals.h"
#include "../../core/sr_module.h"
#include "../../core/tcp_options.h"
#include "../../core/resolve.h"
#include "../../core/lvalue.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/events.h"
#include "../../core/kemi.h"
#include "../../core/pass_fd.h"

#include "tcpops.h"

MODULE_VERSION

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);
static int w_tcp_keepalive_enable4(sip_msg_t* msg, char* con, char* idle,
		char *cnt, char *intvl);
static int w_tcp_keepalive_enable3(sip_msg_t* msg, char* idle, char *cnt,
		char *intvl);
static int w_tcp_keepalive_disable1(sip_msg_t* msg, char* con, char* p2);
static int w_tcp_keepalive_disable0(sip_msg_t* msg, char* p1, char* p2);
static int w_tcpops_set_connection_lifetime2(sip_msg_t* msg, char* con,
		char* time);
static int w_tcpops_set_connection_lifetime1(sip_msg_t* msg, char* time);
static int w_tcpops_enable_closed_event1(sip_msg_t* msg, char* con, char* p2);
static int w_tcpops_enable_closed_event0(sip_msg_t* msg, char* p1, char* p2);
static int w_tcp_conid_state(sip_msg_t* msg, char* con, char *p2);
static int w_tcp_conid_alive(sip_msg_t* msg, char* con, char *p2);
static int w_tcp_get_conid(sip_msg_t* msg, char *paddr, char *pvn);
static int w_tcp_set_otcpid(sip_msg_t* msg, char* conid, char *p2);
static int w_tcp_set_otcpid_flag(sip_msg_t* msg, char* mode, char *p2);
static int w_tcp_close_connection(sip_msg_t* msg, char* p1, char *p2);
static int w_tcp_close_connection_id(sip_msg_t* msg, char* pconid, char *p2);

str tcpops_event_callback = STR_NULL;

static int pv_get_tcp(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int pv_parse_tcp_name(pv_spec_p sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"tcp", (sizeof("tcp")-1)}, PVT_CONTEXT, pv_get_tcp,
		0, pv_parse_tcp_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static cmd_export_t cmds[]={
	{"tcp_keepalive_enable", (cmd_function)w_tcp_keepalive_enable4, 4,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_keepalive_enable", (cmd_function)w_tcp_keepalive_enable3, 3,
			fixup_igp_all, fixup_free_igp_all, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"tcp_keepalive_disable", (cmd_function)w_tcp_keepalive_disable1, 1,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_keepalive_disable", (cmd_function)w_tcp_keepalive_disable0, 0,
			0, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"tcp_set_connection_lifetime", (cmd_function)w_tcpops_set_connection_lifetime2, 2,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_set_connection_lifetime", (cmd_function)w_tcpops_set_connection_lifetime1, 1,
			fixup_igp_all, fixup_free_igp_all, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"tcp_enable_closed_event", (cmd_function)w_tcpops_enable_closed_event1, 1,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_enable_closed_event", (cmd_function)w_tcpops_enable_closed_event0, 0,
			0, 0, REQUEST_ROUTE|ONREPLY_ROUTE},
	{"tcp_conid_state", (cmd_function)w_tcp_conid_state, 1,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_get_conid", (cmd_function)w_tcp_get_conid, 2,
			fixup_spve_pvar, fixup_free_spve_pvar, ANY_ROUTE},
	{"tcp_conid_alive", (cmd_function)w_tcp_conid_alive, 1,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_set_otcpid", (cmd_function)w_tcp_set_otcpid, 1,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_set_otcpid_flag", (cmd_function)w_tcp_set_otcpid_flag, 1,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{"tcp_close_connection", (cmd_function)w_tcp_close_connection, 0,
			0, 0, ANY_ROUTE},
	{"tcp_close_connection", (cmd_function)w_tcp_close_connection_id, 1,
			fixup_igp_all, fixup_free_igp_all, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"closed_event",    PARAM_INT,    &tcp_closed_event},
	{"event_callback",  PARAM_STR,    &tcpops_event_callback},
	{0, 0, 0}
};



struct module_exports exports = {
	"tcpops",        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions to config */
	params,          /* exported parameters to config */
	0,               /* RPC method exports */
	mod_pvs,         /* exported pseudo-variables */
	0,               /* response function */
	mod_init,        /* module initialization function */
	child_init,      /* per child init function */
	mod_destroy      /* destroy function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	LM_DBG("TCP keepalive module loaded.\n");

	if (tcp_closed_event < 0 || tcp_closed_event > 2) {
		LM_ERR("invalid \"closed_event\" value: %d, must be 0 (disabled),"
				" 1 (enabled) or 2 (manual)\n", tcp_closed_event);
		return -1;
	}

	if (tcp_closed_event) {
		/* register event only if tcp_closed_event != 0 */
		if (sr_event_register_cb(SREV_TCP_CLOSED, tcpops_handle_tcp_closed) != 0) {
			LM_ERR("problem registering tcpops_handle_tcp_closed call-back\n");
			return -1;
		}

		tcpops_init_evroutes();
	}

	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
	LM_DBG("TCP keepalive module unloaded.\n");
}

#define _IVALUE_ERROR(NAME) LM_ERR("invalid parameter '" #NAME "' (must be a number)\n")
#define _IVALUE(NAME)\
int i_##NAME ;\
if(fixup_get_ivalue(msg, (gparam_t*)NAME, &( i_##NAME))!=0)\
{ \
	_IVALUE_ERROR(NAME);\
	return -1;\
}


/**
 *
 */
static int w_tcp_keepalive_enable4(sip_msg_t* msg, char* con, char* idle,
		char *cnt, char *intvl)
{
	int fd;
	int closefd = 0;

	_IVALUE (con)

	if (msg != NULL && msg->rcv.proto_reserved1 == i_con) {
		if (!tcpops_get_current_fd(msg->rcv.proto_reserved1, &fd)) {
			return -1;
		}
	} else {
		if (!tcpops_acquire_fd_from_tcpmain(i_con, &fd)) {
			return -1;
		}
		closefd = 1;
	}

	_IVALUE (idle)
	_IVALUE (cnt)
	_IVALUE (intvl)

	return tcpops_keepalive_enable(fd, i_idle, i_cnt, i_intvl, closefd);

}

/**
 *
 */
static int ki_tcp_keepalive_enable_cid(sip_msg_t* msg, int i_con, int i_idle,
		int i_cnt, int i_intvl)
{
	int fd;
	int closefd = 0;

	if (msg != NULL && msg->rcv.proto_reserved1 == i_con) {
		if (!tcpops_get_current_fd(msg->rcv.proto_reserved1, &fd)) {
			return -1;
		}
	} else {
		if (!tcpops_acquire_fd_from_tcpmain(i_con, &fd)) {
			return -1;
		}
		closefd = 1;
	}

	return tcpops_keepalive_enable(fd, i_idle, i_cnt, i_intvl, closefd);

}

static int w_tcp_keepalive_enable3(sip_msg_t* msg, char* idle, char *cnt,
		char *intvl)
{
	int fd;

	if (unlikely(msg == NULL)) {
		return -1;
	}

	if(unlikely(msg->rcv.proto != PROTO_TCP && msg->rcv.proto != PROTO_TLS
			&& msg->rcv.proto != PROTO_WS && msg->rcv.proto != PROTO_WSS))
	{
		LM_ERR("the current message does not come from a TCP connection\n");
		return -1;
	}

	if (!tcpops_get_current_fd(msg->rcv.proto_reserved1, &fd)) {
		return -1;
	}

	_IVALUE (idle)
	_IVALUE (cnt)
	_IVALUE (intvl)

	return tcpops_keepalive_enable(fd, i_idle, i_cnt, i_intvl, 0);
}

static int ki_tcp_keepalive_enable(sip_msg_t* msg, int i_idle, int i_cnt,
		int i_intvl)
{
	int fd;

	if (unlikely(msg == NULL)) {
		return -1;
	}

	if(unlikely(msg->rcv.proto != PROTO_TCP && msg->rcv.proto != PROTO_TLS
			&& msg->rcv.proto != PROTO_WS && msg->rcv.proto != PROTO_WSS))
	{
		LM_ERR("the current message does not come from a TCP connection\n");
		return -1;
	}

	if (!tcpops_get_current_fd(msg->rcv.proto_reserved1, &fd)) {
		return -1;
	}

	return tcpops_keepalive_enable(fd, i_idle, i_cnt, i_intvl, 0);
}

static int ki_tcp_keepalive_disable_cid(sip_msg_t* msg, int i_con)
{
	int fd;
	int closefd = 0;

	if (msg != NULL && msg->rcv.proto_reserved1 == i_con) {
		if (!tcpops_get_current_fd(msg->rcv.proto_reserved1, &fd)) {
			return -1;
		}
	} else {
		if (!tcpops_acquire_fd_from_tcpmain(i_con, &fd)) {
			return -1;
		}
		closefd = 1;
	}

	return tcpops_keepalive_disable(fd, closefd);
}

static int w_tcp_keepalive_disable1(sip_msg_t* msg, char* con, char *p2)
{
	_IVALUE (con)
	return ki_tcp_keepalive_disable_cid(msg, i_con);
}

static int ki_tcp_keepalive_disable(sip_msg_t* msg)
{
	int fd;

	if (unlikely(msg == NULL))
		return -1;

	if(unlikely(msg->rcv.proto != PROTO_TCP && msg->rcv.proto != PROTO_TLS
			&& msg->rcv.proto != PROTO_WS && msg->rcv.proto != PROTO_WSS))
	{
		LM_ERR("the current message does not come from a TCP connection\n");
		return -1;
	}

	if (!tcpops_get_current_fd(msg->rcv.proto_reserved1, &fd)) {
		return -1;
	}

	return tcpops_keepalive_disable(fd, 0);
}

static int w_tcp_keepalive_disable0(sip_msg_t* msg, char *p1, char *p2)
{
	return ki_tcp_keepalive_disable(msg);
}

/*! \brief Check the state of the TCP connection */
static int ki_tcp_conid_state(sip_msg_t* msg, int i_conid)
{
	struct tcp_connection *s_con;
	int ret = -1;

	if (unlikely((s_con = tcpconn_get(i_conid, 0, 0, 0, 0)) == NULL)) {
		LM_DBG("Connection id %d does not exist.\n", i_conid);
		ret = -1;
		goto done;
	}
	/* Connection structure exists, now check what Kamailio thinks of it */
	if (s_con->state == S_CONN_OK) {
		/* All is fine, return happily */
		ret = 1;
		goto done;
	}
	if (s_con->state == S_CONN_EOF) {	/* Socket closed or about to close under our feet */
		ret = -2;
		goto done;
	}
	if (s_con->state == S_CONN_ERROR) {	/* Error on read/write - will close soon */
		ret = -3;
		goto done;
	}
	if (s_con->state == S_CONN_BAD) {	/* Unknown state */
		ret = -4;
		goto done;
	}
	if (s_con->state == S_CONN_ACCEPT) {	/* Incoming connection to be set up */
		ret = 2;
		goto done;
	}
	if (s_con->state == S_CONN_CONNECT) {	/* Outbound connection moving to S_CONN_OK */
		ret = 3;
		goto done;
	}
	/* Wonder what state we're in here */
	LM_DBG("Connection id %d is in unexpected state %d - assuming ok.\n",
			i_conid, s_con->flags);

	/* Good connection */
	ret = 1;
done:
	if(s_con) tcpconn_put(s_con);
	return ret;
}

static int w_tcp_conid_state(sip_msg_t* msg, char* conid, char *p2)
{
	_IVALUE (conid)

	return ki_tcp_conid_state(msg, i_conid);
}

/*! \brief A simple check to see if a connection is alive or not,
	avoiding all the various connection states
 */
static int w_tcp_conid_alive(sip_msg_t* msg, char* conid, char *p2)
{
	int ret = w_tcp_conid_state(msg, conid, p2);
	if (ret >= 1) {
		return 1;	/* TRUE */
	}
	/* We have some kind of problem */
	return -1;
}

static int ki_tcp_conid_alive(sip_msg_t* msg, int i_conid)
{
	int ret = ki_tcp_conid_state(msg, i_conid);
	if (ret >= 1) {
		return 1;	/* TRUE */
	}
	/* We have some kind of problem */
	return -1;
}

static int ki_tcpops_set_connection_lifetime_cid(sip_msg_t* msg, int i_conid,
		int i_time)
{
	struct tcp_connection *s_con;
	int ret = -1;

	if (unlikely((s_con = tcpconn_get(i_conid, 0, 0, 0, 0)) == NULL)) {
		LM_ERR("invalid connection id %d, (must be a TCP conid)\n", i_conid);
		return 0;
	} else {
		ret = tcpops_set_connection_lifetime(s_con, i_time);
		tcpconn_put(s_con);
	}
	return ret;
}

static int w_tcpops_set_connection_lifetime2(sip_msg_t* msg, char* conid,
		char* time)
{
	_IVALUE (conid)
	_IVALUE (time)

	return ki_tcpops_set_connection_lifetime_cid(msg, i_conid, i_time);
}

static int ki_tcpops_set_connection_lifetime(sip_msg_t* msg, int i_time)
{
	struct tcp_connection *s_con;
	int ret = -1;

	if(unlikely(msg->rcv.proto != PROTO_TCP && msg->rcv.proto != PROTO_TLS
				&& msg->rcv.proto != PROTO_WS && msg->rcv.proto != PROTO_WSS))
	{
		LM_ERR("the current message does not come from a TCP connection\n");
		return -1;
	}

	if (unlikely((s_con = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, 0))
				== NULL)) {
		return -1;
	} else {
		ret = tcpops_set_connection_lifetime(s_con, i_time);
		tcpconn_put(s_con);
	}
	return ret;
}

static int w_tcpops_set_connection_lifetime1(sip_msg_t* msg, char* time)
{
	_IVALUE (time)

	return ki_tcpops_set_connection_lifetime(msg, i_time);
}

static int ki_tcpops_enable_closed_event_cid(sip_msg_t* msg, int i_conid)
{
	struct tcp_connection *s_con;

	if (unlikely((s_con = tcpconn_get(i_conid, 0, 0, 0, 0)) == NULL)) {
		LM_ERR("invalid connection id %d, (must be a TCP conid)\n", i_conid);
		return 0;
	} else {
		s_con->flags |= F_CONN_CLOSE_EV;
		tcpconn_put(s_con);
	}
	return 1;
}

static int w_tcpops_enable_closed_event1(sip_msg_t* msg, char* conid, char* p2)
{
	_IVALUE (conid)

	return ki_tcpops_enable_closed_event_cid(msg, i_conid);
}

static int ki_tcpops_enable_closed_event(sip_msg_t* msg)
{
	struct tcp_connection *s_con;

	if (unlikely(tcp_closed_event != 2)) {
		LM_WARN("tcp_enable_closed_event() can only be used if"
				" the \"closed_event\" modparam is set to 2\n");
		return -1;
	}

	if(unlikely(msg->rcv.proto != PROTO_TCP && msg->rcv.proto != PROTO_TLS
			&& msg->rcv.proto != PROTO_WS && msg->rcv.proto != PROTO_WSS))
	{
		LM_ERR("the current message does not come from a TCP connection\n");
		return -1;
	}

	if (unlikely((s_con = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, 0))
			== NULL)) {
		return -1;
	} else {
		s_con->flags |= F_CONN_CLOSE_EV;
		tcpconn_put(s_con);
	}
	return 1;
}

static int w_tcpops_enable_closed_event0(sip_msg_t* msg, char* p1, char* p2)
{
	return ki_tcpops_enable_closed_event(msg);
}

/**
 *
 */
static int ki_tcp_get_conid_helper(sip_msg_t* msg, str *saddr, pv_spec_t *pvs)
{
	int conid = 0;
	sip_uri_t uaddr, *u;
	dest_info_t dst;
	char *p;
	int ret;
	ticks_t clifetime;
	tcp_connection_t *c;
	ip_addr_t ip;
	int port;
	pv_value_t val;

	if(pvs->setf==NULL) {
		LM_ERR("output variable is read only\n");
		return -1;
	}

	init_dest_info(&dst);

	u = &uaddr;
	u->port_no = 5060;
	u->host = *saddr;
	/* detect ipv6 */
	p = memchr(saddr->s, ']', saddr->len);
	if (p) {
		p++;
		p = memchr(p, ':', saddr->s + saddr->len - p);
	} else {
		p = memchr(saddr->s, ':', saddr->len);
	}
	if (p) {
		u->host.len = p - saddr->s;
		p++;
		u->port_no = str2s(p, saddr->len - (p - saddr->s), NULL);
	}

	ret = sip_hostport2su(&dst.to, &u->host, u->port_no, &dst.proto);
	if(ret!=0) {
		LM_ERR("failed to resolve [%.*s]\n", u->host.len, ZSW(u->host.s));
		return E_BUG;
	}

	dst.proto = PROTO_TCP;
	dst.id = 0;
	clifetime = cfg_get(tcp, tcp_cfg, con_lifetime);
	su2ip_addr(&ip, &dst.to);
	port = su_getport(&dst.to);
	c = tcpconn_get(dst.id, &ip, port, NULL, clifetime);

	if (unlikely(c==0)) {
		goto setvalue;
	}
	conid = c->id;
	tcpconn_put(c);

setvalue:
	memset(&val, 0, sizeof(pv_value_t));
	val.ri = conid;
	val.flags = PV_VAL_INT|PV_TYPE_INT;
	if(pvs->setf(msg, &pvs->pvp, (int)EQ_T, &val)<0) {
		LM_ERR("failed to set the output var\n");
		return -1;
	}
	if(conid==0) {
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_tcp_get_conid(sip_msg_t* msg, char *paddr, char *pvn)
{
	str saddr;

	if(fixup_get_svalue(msg, (gparam_t*)paddr, &saddr)<0) {
		LM_ERR("failed to get address parameter\n");
		return -1;
	}

	return ki_tcp_get_conid_helper(msg, &saddr, (pv_spec_t*)pvn);
}

/**
 *
 */
static int ki_tcp_set_otcpid(sip_msg_t* msg, int vconid)
{
	if(msg == NULL) {
		return -1;
	}
	msg->otcpid = vconid;
	return 1;
}

/**
 *
 */
static int w_tcp_set_otcpid(sip_msg_t* msg, char* conid, char *p2)
{
	int vconid = 0;

	if(fixup_get_ivalue(msg, (gparam_t*)conid, &vconid)<0) {
		LM_ERR("failed to get connection id\n");
		return -1;
	}
	return ki_tcp_set_otcpid(msg, vconid);
}

/**
 *
 */
static int ki_tcp_set_otcpid_flag(sip_msg_t* msg, int vmode)
{
	if(msg == NULL) {
		return -1;
	}
	if(vmode) {
		msg->msg_flags |= FL_USE_OTCPID;
	} else {
		msg->msg_flags &= ~(FL_USE_OTCPID);
	}
	return 1;
}

/**
 *
 */
static int w_tcp_set_otcpid_flag(sip_msg_t* msg, char* mode, char *p2)
{
	int vmode = 0;

	if(fixup_get_ivalue(msg, (gparam_t*)mode, &vmode)<0) {
		LM_ERR("failed to get mode parameter\n");
		return -1;
	}
	return ki_tcp_set_otcpid_flag(msg, vmode);
}

/*!
 * \brief Close a TCP connection
 *
 * Requests the TCP main process to close the specified TCP connection
 * \param conid the internal connection ID
 */
static int ki_tcp_close_connection_id(sip_msg_t *msg, int conid)
{
	struct tcp_connection *con;
	long mcmd[2];
	int n;

	if ((con = tcpconn_get(conid, 0, 0, 0, 0))) {
		mcmd[0] = (long)con;
		mcmd[1] = CONN_EOF;

		con->send_flags.f |= SND_F_CON_CLOSE;
		con->flags |= F_CONN_FORCE_EOF;

		n = send_all(unix_tcp_sock, mcmd, sizeof(mcmd));
		if (unlikely(n <= 0)){
			LM_ERR("failed to send close request: %s (%d)\n", strerror(errno), errno);
			return -2;
		}
		return 1;
	}
	return -1;
}

/**
 *
 */
static int ki_tcp_close_connection(sip_msg_t *msg)
{
	return ki_tcp_close_connection_id(msg, msg->rcv.proto_reserved1);
}

/**
 *
 */
static int w_tcp_close_connection_id(sip_msg_t* msg, char* pconid, char *p2)
{
	int conid = 0;

	if(fixup_get_ivalue(msg, (gparam_t*)pconid, &conid)<0) {
		LM_ERR("failed to get conid parameter\n");
		return -1;
	}
	return ki_tcp_close_connection_id(msg, conid);
}

/**
 *
 */
static int w_tcp_close_connection(sip_msg_t* msg, char* p1, char *p2)
{
	return ki_tcp_close_connection_id(msg, msg->rcv.proto_reserved1);
}

/**
 *
 */
static int pv_get_tcp(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	tcp_connection_t *con;
	int ival;
	str sval;

	if (msg == NULL) {
		return -1;
	}

	if ((con = tcpconn_get(msg->rcv.proto_reserved1, 0, 0, 0, 0)) == NULL) {
		return pv_get_null(msg, param, res);
	}

	switch(param->pvn.u.isname.name.n) {
		case 1:
			sval.s = ip_addr2a(&con->cinfo.src_ip);
			tcpconn_put(con);
			sval.len = strlen(sval.s);
			return pv_get_strval(msg, param, res, &sval);
		case 2:
			ival = con->cinfo.src_port;
			tcpconn_put(con);
			return pv_get_sintval(msg, param, res, ival);
		default:
			ival = con->id;
			tcpconn_put(con);
			return pv_get_sintval(msg, param, res, ival);
	}
}


/**
 *
 */
static int pv_parse_tcp_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len) {
		case 4:
			if(strncmp(in->s, "c_si", 4)==0) {
				sp->pvp.pvn.u.isname.name.n = 1;
			} else if(strncmp(in->s, "c_sp", 4)==0) {
				sp->pvp.pvn.u.isname.name.n = 2;
			} else { goto error; }
		break;
		case 5:
			if(strncmp(in->s, "conid", 5)==0) {
				sp->pvp.pvn.u.isname.name.n = 0;
			} else { goto error; }
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown pv key: %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_tcpops_exports[] = {
	{ str_init("tcpops"), str_init("tcp_keepalive_enable"),
		SR_KEMIP_INT, ki_tcp_keepalive_enable,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_keepalive_enable_cid"),
		SR_KEMIP_INT, ki_tcp_keepalive_enable_cid,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_INT,
			SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_keepalive_disable"),
		SR_KEMIP_INT, ki_tcp_keepalive_disable,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_keepalive_disable_cid"),
		SR_KEMIP_INT, ki_tcp_keepalive_disable_cid,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_set_connection_lifetime"),
		SR_KEMIP_INT, ki_tcpops_set_connection_lifetime,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_set_connection_lifetime_cid"),
		SR_KEMIP_INT, ki_tcpops_set_connection_lifetime_cid,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_enable_closed_event"),
		SR_KEMIP_INT, ki_tcpops_enable_closed_event,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_enable_closed_event_cid"),
		SR_KEMIP_INT, ki_tcpops_enable_closed_event_cid,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_conid_alive"),
		SR_KEMIP_INT, ki_tcp_conid_alive,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_conid_state"),
		SR_KEMIP_INT, ki_tcp_conid_state,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_set_otcpid"),
		SR_KEMIP_INT, ki_tcp_set_otcpid,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_set_otcpid_flag"),
		SR_KEMIP_INT, ki_tcp_set_otcpid_flag,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_close_connection"),
		SR_KEMIP_INT, ki_tcp_close_connection,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("tcpops"), str_init("tcp_close_connection_id"),
		SR_KEMIP_INT, ki_tcp_close_connection_id,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_tcpops_exports);
	return 0;
}
