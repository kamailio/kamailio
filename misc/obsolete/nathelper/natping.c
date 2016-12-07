/* $Id$
 *
 * Copyright (C) 2005-2008 Sippy Software, Inc., http://www.sippysoft.com
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  LONG-YEARS-AGO, natping.c was born without history tracking in it
 *  2007-08-28 natping_crlf option was introduced (jiri)
 *
 */

#include <unistd.h>
#include <signal.h>
#include "../usrloc/usrloc.h"
#include "../../modules/tm/tm_load.h"
#include "../../dprint.h"
#include "../../parser/parse_hostport.h"
#include "../../resolve.h"
#include "../../cfg/cfg_struct.h"
#include "nathelper.h"

int natping_interval = 0;
/*
 * If this parameter is set then the natpinger will ping only contacts
 * that have the NAT flag set in user location database
 */
int ping_nated_only = 0;


/*
 * If this parameter is set, then pings will not
 * be full requests but only CRLFs
 */
int natping_crlf = 1;

/*
 * Ping method. Any word except NULL is treated as method name.
 */
char *natping_method = NULL;
int natping_stateful = 0;

static pid_t aux_process = -1;
static usrloc_api_t ul;
/* TM bind */
static struct tm_binds tmb;
static int cblen = 0;
static char sbuf[4] = (CRLF CRLF);

static void natping(unsigned int ticks, void *param);
static void natping_cycle(void);

int
natpinger_init(void)
{
	bind_usrloc_t bind_usrloc;
	load_tm_f load_tm;
	char *p;

	if (natping_interval > 0) {
		bind_usrloc = (bind_usrloc_t)find_export("ul_bind_usrloc", 1, 0);
		if (bind_usrloc == NULL) {
			LOG(L_ERR, "ERROR: nathelper::natpinger_init: Can't find usrloc module\n");
			return -1;
		}

		if (bind_usrloc(&ul) < 0) {
			return -1;
		}
		if (natping_method != NULL) {
			for (p = natping_method; *p != '\0'; ++p)
				*p = toupper(*p);
			if (strcmp(natping_method, "NULL") == 0)
				natping_method = NULL;
		}
		if (natping_method != NULL) {
			/* import the TM auto-loading function */
			load_tm = (load_tm_f)find_export("load_tm", NO_SCRIPT, 0);
			if (load_tm == NULL) {
				LOG(L_ERR, "ERROR: nathelper::natpinger_init: can't import load_tm\n");
				return -1;
			}
			/* let the auto-loading function load all TM stuff */
			if (load_tm(&tmb) == -1)
				return -1;
		}

		/*
		 * Use timer only in single process. For forked SER,
		 * use separate process (see natpinger_child_init())
		 */
		if (dont_fork) {
			register_timer(natping, NULL, natping_interval);
		} else {
			register_procs(1); /* register the separate natpinger process */
			/* The process will keep updating its configuration */
			cfg_register_child(1);
		}

		if (natping_method == NULL) {
			if (natping_crlf == 0)
				LOG(L_WARN, "WARNING: nathelper::natpinger_init: "
				    "natping_crlf==0 has no effect, please also set "
				    "natping_method\n");
			if (natping_stateful != 0)
				LOG(L_WARN, "WARNING: nathelper::natpinger_init: "
				    "natping_stateful!=0 has no effect, please also set "
				    "natping_method\n");
		} else if (natping_crlf != 0 && natping_stateful != 0) {
			LOG(L_WARN, "WARNING: nathelper::natpinger_init: "
			    "natping_crlf!=0 has no effect when the"
			    "natping_stateful!=0\n");
		}
	}

	return 0;
}

int
natpinger_child_init(int rank)
{

	/* If forking is prohibited, use only timer. */
	if (dont_fork)
		return 0;

	/*
	 * Fork only from PROC_MAIN (see doc/modules_init.txt) and only
	 * if ping is requested.
	 */
	if ((rank != PROC_MAIN) || (natping_interval == 0))
		return 0;

	/*
	 * Create a new "ser" process, with access to the tcp send, but
	 * don't call child_init() for this process (no need, it's just a pinger).
	 */
	aux_process = fork_process(PROC_NOCHLDINIT, "nathelper pinger", 1);
	if (aux_process == -1) {
		LOG(L_ERR, "natping_child_init(): fork: %s\n",
		    strerror(errno));
		return -1;
	}
	if (aux_process == 0) {
		/* initialize the config framework */
		if (cfg_child_init()) return -1;

		natping_cycle();
		/* UNREACHED */
		_exit(1);
	}
	return 0;
}

int
natpinger_cleanup(void)
{

	if (aux_process != -1)
		kill(aux_process, SIGTERM);
	return 0;
}

static void
natping_cycle(void)
{

	signal(SIGTERM, SIG_DFL); /* no special treat */
	for(;;) {
		sleep(natping_interval);

		/* update the local config */
		cfg_update();

		natping(0, NULL);
	}
}


#define PING_FROM "f:"
#define PING_FROM_LEN (sizeof(PING_FROM)-1)
#define PING_FROMTAG ";tag=1"
#define PING_FROMTAG_LEN (sizeof(PING_FROMTAG)-1)
#define PING_TO "t:"
#define PING_TO_LEN (sizeof(PING_TO)-1)
#define PING_CALLID "i:"
#define PING_CALLID_LEN (sizeof(PING_CALLID)-1)
#define PING_CSEQ "CSeq: 1"
#define PING_CSEQ_LEN (sizeof(PING_CSEQ)-1)
#define PING_CLEN "l: 0"
#define PING_CLEN_LEN (sizeof(PING_CLEN)-1)

/*
 * Ping branch format:  <magic cookie><sep><ping cookie><sep><number>
 *                      ^  prefix                           ^
 */
#define PING_BRANCH_PREFIX MCOOKIE "-GnIp-"
#define PING_BRANCH_PREFIX_LEN (sizeof(PING_BRANCH_PREFIX)-1)

struct nat_ping_params {
	str uri;
	str method;
	str from_uri;
	str to_uri;
	struct dest_info* send_info;
};

static unsigned int ping_no = 0; /* per process ping number */

/*
 * Build a minimal nat ping message (pkg_malloc'ed)
 * returns: pointer to message and sets *len on success, 0 on error
 * Note: the message must be pkg_free()'d
 *
 * Message format:
 * 
 * <METHOD> <sip:uri>
 * Via: ...;branch=<special>
 * f: <from_uri>;tag=1
 * t: <to_uri>
 * c: seq
 * cseq: 1
 * l: 0
 */
char *
sip_ping_builder(unsigned int* len, struct nat_ping_params* params)
{
	str via;
	char branch_buf[PING_BRANCH_PREFIX_LEN+INT2STR_MAX_LEN];
	str branch_str;
	str callid_str;
	char* msg;
	int size;
	char callid_no_buf[INT2STR_MAX_LEN];
	int callid_no_buf_free;
	char* t;

	via.s = 0;
	msg = 0;

	callid_no_buf_free = sizeof(callid_no_buf);
	t = callid_no_buf;
	int2reverse_hex(&t, &callid_no_buf_free, ping_no + (process_no << 20));
	callid_str.s = callid_no_buf;
	callid_str.len = (int)(t - callid_no_buf);

	/* build branch: MCOOKIE SEP PING_MAGIC SEP callid_str */
	branch_str.len = PING_BRANCH_PREFIX_LEN + callid_str.len;
	if (branch_str.len > sizeof(branch_buf)) {
		LOG(L_WARN, "WARNING: nathelper::sip_ping_builder: branch buffer too small (%d)\n",
		    branch_str.len);
		/* truncate */
		callid_str.len = sizeof(branch_buf) - PING_BRANCH_PREFIX_LEN;
		branch_str.len = sizeof(branch_buf);
	}
	t = branch_buf;
	memcpy(t, PING_BRANCH_PREFIX, PING_BRANCH_PREFIX_LEN);
	t += PING_BRANCH_PREFIX_LEN;
	memcpy(t, callid_str.s, callid_str.len);
	branch_str.s = branch_buf;

	via.s = via_builder((unsigned int *)&via.len, params->send_info, &branch_str,
	    0, 0);
	if (via.s == NULL) {
		LOG(L_ERR, "ERROR: nathelper::sip_ping_builder: via_builder failed\n");
		goto error;
	}
	size = params->method.len + 1 /* space */ + params->uri.len + 1 /* space */ +
	    SIP_VERSION_LEN + CRLF_LEN + via.len /* CRLF included */ +
	    PING_FROM_LEN + 1 /* space */ +
	    params->from_uri.len /* ; included in fromtag */ + PING_FROMTAG_LEN +
	    CRLF_LEN + PING_TO_LEN + 1 /* space */ + params->to_uri.len + CRLF_LEN +
	    PING_CALLID_LEN + 1 /* space */ + callid_str.len + CRLF_LEN +
	    PING_CSEQ_LEN + 1 /* space */ + params->method.len + CRLF_LEN +
	    PING_CLEN_LEN + CRLF_LEN + CRLF_LEN;
	ping_no++;
	msg = pkg_malloc(size);
	if (msg == NULL) {
		LOG(L_ERR, "ERROR: nathelper::sip_ping_builder: out of memory\n");
		goto error;
	}
	/* build the message */
	t = msg;
	/* first line */
	memcpy(t, params->method.s, params->method.len);
	t += params->method.len;
	*t = ' ';
	t++;
	memcpy(t, params->uri.s, params->uri.len);
	t += params->uri.len;
	*t = ' ';
	t++;
	memcpy(t, SIP_VERSION, SIP_VERSION_LEN);
	t += SIP_VERSION_LEN;
	memcpy(t, CRLF, CRLF_LEN);
	t += CRLF_LEN;
	/* via */
	memcpy(t, via.s, via.len);
	t += via.len;
	/* from */
	memcpy(t, PING_FROM, PING_FROM_LEN);
	t += PING_FROM_LEN;
	*t = ' ';
	t++;
	memcpy(t, params->from_uri.s, params->from_uri.len);
	t += params->from_uri.len;
	memcpy(t, PING_FROMTAG, PING_FROMTAG_LEN);
	t += PING_FROMTAG_LEN;
	memcpy(t, CRLF, CRLF_LEN);
	t += CRLF_LEN;
	/* to */
	memcpy(t, PING_TO, PING_TO_LEN);
	t += PING_TO_LEN;
	*t = ' ';
	t++;
	memcpy(t, params->to_uri.s, params->to_uri.len);
	t += params->to_uri.len;
	memcpy(t, CRLF, CRLF_LEN);
	t += CRLF_LEN;
	/* callid */
	memcpy(t, PING_CALLID, PING_CALLID_LEN);
	t += PING_CALLID_LEN;
	*t = ' ';
	t++;
	memcpy(t, callid_str.s, callid_str.len);
	t += callid_str.len;
	memcpy(t, CRLF, CRLF_LEN);
	t += CRLF_LEN;
	/* cseq */
	memcpy(t, PING_CSEQ, PING_CSEQ_LEN);
	t += PING_CSEQ_LEN;
	*t = ' ';
	t++;
	memcpy(t, params->method.s, params->method.len);
	t += params->method.len;
	memcpy(t, CRLF, CRLF_LEN);
	t += CRLF_LEN;
	memcpy(t, PING_CLEN, PING_CLEN_LEN);
	t += PING_CLEN_LEN;
	memcpy(t, CRLF CRLF, 2*CRLF_LEN);
	/* t += 2 * CRLF_LEN; */

	pkg_free(via.s);
	*len = size;
	return msg;
error:
	if (msg != NULL)
		pkg_free(msg);
	if (via.s != NULL)
		pkg_free(via.s);
	*len = 0;
	return NULL;
}

static void
natping(unsigned int ticks, void *param)
{
	int rval, n;
	void *buf, *cp;
	str c;
	struct dest_info dst;

	buf = NULL;
	if (cblen > 0) {
		buf = pkg_malloc(cblen);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: nathelper::natping: out of memory\n");
			return;
		}
	}
	rval = ul.get_all_ucontacts(buf, cblen, (ping_nated_only ? FL_NAT : 0));
	if (rval > 0) {
		if (buf != NULL)
			pkg_free(buf);
		cblen = (cblen + rval) * 2;
		buf = pkg_malloc(cblen);
		if (buf == NULL) {
			LOG(L_ERR, "ERROR: nathelper::natping: out of memory\n");
			return;
		}
		rval = ul.get_all_ucontacts(buf, cblen,
		    (ping_nated_only ? FL_NAT : 0));
		if (rval != 0) {
			pkg_free(buf);
			return;
		}
	}

	if (buf == NULL)
		return;

	cp = buf;
	n = 0;
	for (;;) {
		memcpy(&(c.len), cp, sizeof(c.len));
		if (c.len == 0)
			break;
		c.s = (char *)cp + sizeof(c.len);
		cp =  (char *)cp + sizeof(c.len) + c.len;
		init_dest_info(&dst);
		memcpy(&dst.send_sock, cp, sizeof(dst.send_sock));
		cp += sizeof(dst.send_sock);
		if ((++n % 50) == 0)
			usleep(1);
		natping_contact(c, &dst);
	}
	pkg_free(buf);
}

int
natping_contact(str contact, struct dest_info *dst)
{
	struct sip_uri curi;
	struct hostent *he;
	str p_method, p_from;
	char proto;
	uac_req_t uac_r;
	struct nat_ping_params pp;
	char *ping_msg;
	unsigned int ping_msg_len;

	if (natping_method != NULL && natping_stateful != 0) {
		/* XXX: add send_sock handling */
		p_method.s = natping_method;
		p_method.len = strlen(p_method.s);
		p_from.s = "sip:registrar@127.0.0.1:9"; /* XXX */
		p_from.len = strlen(p_from.s);
		set_uac_req(&uac_r, &p_method, 0, 0, 0, 0, 0, 0);
		if (tmb.t_request(&uac_r, &contact, &contact, &p_from, 0) == -1) {
			LOG(L_ERR, "ERROR: nathelper::natping_contact: t_request() failed\n");
			return -1;
		}
	} else {
		if (parse_uri(contact.s, contact.len, &curi) < 0) {
			LOG(L_ERR, "ERROR: nathelper::natping_contact: can't parse contact uri\n");
			return -1;
		}
		if (curi.port_no == 0)
			curi.port_no = SIP_PORT;

		proto = (curi.proto != PROTO_NONE) ? curi.proto : PROTO_UDP;
		he = sip_resolvehost(&curi.host, &curi.port_no, &proto);
		if (he == NULL) {
			LOG(L_ERR, "ERROR: nathelper::natping_contact: can't resolve host\n");
			return -1;
		}
		hostent2su(&dst->to, he, 0, curi.port_no);
		if (dst->send_sock == NULL || (dst->send_sock->flags & SI_IS_MCAST)) {
			dst->send_sock = force_socket ? force_socket :
			    get_send_socket(0, &dst->to, proto);
		}
		if (dst->send_sock == NULL) {
			LOG(L_ERR, "ERROR: nathelper::natping_contact: can't get sending socket\n");
			return -1;
		}
		dst->proto = proto;
		if (natping_method != NULL && natping_crlf == 0) {
			/* Stateless natping using full-blown messages */
			pp.method.s = natping_method;
			pp.method.len = strlen(natping_method);
			pp.uri = contact;
			pp.from_uri.s = "sip:registrar@127.0.0.1:9"; /* XXX */
			pp.from_uri.len = strlen(pp.from_uri.s);
			pp.to_uri = contact;
			pp.send_info = dst;
			ping_msg = sip_ping_builder(&ping_msg_len, &pp);
			if (ping_msg != NULL){
				msg_send(dst, ping_msg, ping_msg_len);
				pkg_free(ping_msg);
			} else {
				LOG(L_ERR, "ERROR: nathelper::natping_contact: failed to build sip ping message\n");
			}
		} else {
			/* Stateless natping using dummy packets */
			if (proto == PROTO_UDP)
				udp_send(dst, (char *)sbuf, sizeof(sbuf));
			else
				msg_send(dst, (char *)sbuf, sizeof(sbuf));
		}
	}
	return 1;
}

int
intercept_ping_reply(struct sip_msg* msg)
{

	if (natping_stateful != 0)
		return 1;
	/* via1 is parsed automatically for replies */
	if (msg->via1 != NULL && msg->via1->branch != NULL && 
	    msg->via1->branch->value.s != NULL &&
	    (msg->via1->branch->value.len > PING_BRANCH_PREFIX_LEN) &&
	    (memcmp(msg->via1->branch->value.s, PING_BRANCH_PREFIX,
	    PING_BRANCH_PREFIX_LEN) == 0)) {
		/* Drop reply */
		return 0;
	}
	return 1;
}
