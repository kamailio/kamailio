/* $Id$
 *
 * Copyright (C) 2005 Porta Software Ltd
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
 *
 * History:
 * --------
 *  LONG-YEARS-AGO, natping.c was born without history tracking in it
 *  2007-08-28 tcpping_crlf option was introduced (jiri)
 *
 */

#include <unistd.h>
#include <signal.h>
#include "../usrloc/usrloc.h"
#include "../tm/tm_load.h"
#include "../../dprint.h"
#include "../../parser/parse_hostport.h"
#include "../../resolve.h"
#include "nathelper.h"

int natping_interval = 0;
/*
 * If this parameter is set then the natpinger will ping only contacts
 * that have the NAT flag set in user location database
 */
int ping_nated_only = 0;


/* if this parameter is set, then pings to TCP destinations will not
 * be full requests but only CRLFs
 */
int tcpping_crlf = 0;

/*
 * Ping method. Any word except NULL is treated as method name.
 */
char *natping_method = NULL;

static pid_t aux_process = -1;
static usrloc_api_t ul;
/* TM bind */
static struct tm_binds tmb;
static int cblen = 0;
static const char sbuf[4] = {0, 0, 0, 0};

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
		if (!bind_usrloc) {
			LOG(L_ERR, "ERROR: nathelper: natpinger_init: Can't find usrloc module\n");
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
			if (!(load_tm = (load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
				LOG(L_ERR, "ERROR: nathelper: natpinger_init: can't import load_tm\n");
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
		if (dont_fork)
			register_timer(natping, NULL, natping_interval);
	}

	return 0;
}

int
natpinger_child_init(int rank)
{

	/* If forking is prohibited, use only timer. */
	if (dont_fork)
		return 0;

	/* don't do anything for main process and TCP manager process */
	if (rank==PROC_INIT || rank == PROC_MAIN || rank == PROC_TCP_MAIN)
		return 0;

	/* only child 1 will fork the aux process, if ping requested */
	if ((rank != 1) || (natping_interval == 0))
		return 0;

	aux_process = fork();
	if (aux_process == -1) {
		LOG(L_ERR, "natping_child_init(): fork: %s\n", strerror(errno));
		return -1;
	}
	if (aux_process == 0) {
		natping_cycle();
		/*UNREACHED*/
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
		natping(0, NULL);
	}
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
		rval = ul.get_all_ucontacts(buf, cblen, (ping_nated_only ? FL_NAT : 0));
		if (rval != 0) {
			pkg_free(buf);
			return;
		}
	}

	if (buf == NULL)
		return;

	cp = buf;
	n = 0;
	while (1) {
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
natping_contact(str contact, struct dest_info *dst) {
	struct sip_uri curi;
	struct hostent *he;
	str p_method, p_from;
	char proto;
	uac_req_t	uac_r;

	if (natping_method != NULL) {

		/* if natping-ing is set to send full SIP request and crlf
		 * is set, send only CRLF only for TCP; this is much more
		 * leightweighted, free of SIP-interop struggles, and solicits
		 * TCP packets in reverse direction, thus keeping NAT alive
		 * Note: the code bellow deserves a review, as it was rather
		 * copied'and'pasted from the natping_method==NULL section
		 * bellow
		 */
		if (tcpping_crlf && 
				parse_uri(contact.s, contact.len, &curi) == 0 &&
				curi.proto == PROTO_TCP) {
			if (curi.port_no==0) curi.port_no=SIP_PORT;
			he=sip_resolvehost(&curi.host,&curi.port_no,&proto);
			if (he==NULL) {
				LOG(L_ERR, "ERROR: nathelper: tcpping)_crlf can't resolve\n");
				return -1;
			}
			hostent2su(&dst->to, he, 0, curi.port_no);
			if (dst->send_sock == NULL) {
				dst->send_sock = force_socket ? force_socket :
			    	get_send_socket(0, &dst->to, PROTO_TCP);
			}
			if (dst->send_sock == NULL) {
				LOG(L_ERR, "ERROR: nathelper::crlf: can't get socket\n");
				return -1;
			}
			dst->proto=PROTO_TCP;
			if (msg_send(dst, CRLF, CRLF_LEN)==-1) {
				LOG(L_ERR, "ERROR: nathelper: crlf: can't send\n");
				return -1;
			}
			return 1;
		}



		/* XXX: add send_sock handling */
		p_method.s = natping_method;
		p_method.len = strlen(p_method.s);
		p_from.s = "sip:registrar"; /* XXX */
		p_from.len = strlen(p_from.s);
		set_uac_req(&uac_r,
				&p_method,
				0,
				0,
				0,
				0,
				0,
				0
			);				
		if (tmb.t_request(&uac_r, &contact, &contact, &p_from, 0) == -1) {
			LOG(L_ERR, "ERROR: nathelper::natping(): t_request() failed\n");
			return -1;
		}
	} else {
		if (parse_uri(contact.s, contact.len, &curi) < 0) {
			LOG(L_ERR, "ERROR: nathelper::natping: can't parse contact uri\n");
			return -1;
		}
		if (curi.proto != PROTO_UDP && curi.proto != PROTO_NONE)
			return -1;
		if (curi.port_no == 0)
			curi.port_no = SIP_PORT;
		proto=PROTO_UDP;
		he = sip_resolvehost(&curi.host, &curi.port_no, &proto);
		if (he == NULL){
			LOG(L_ERR, "ERROR: nathelper::natping: can't resolve host\n");
			return -1;
		}
		hostent2su(&dst->to, he, 0, curi.port_no);
		if (dst->send_sock == NULL) {
			dst->send_sock = force_socket ? force_socket :
			    get_send_socket(0, &dst->to, PROTO_UDP);
		}
		if (dst->send_sock == NULL) {
			LOG(L_ERR, "ERROR: nathelper::natping: can't get sending socket\n");
			return -1;
		}
		dst->proto=PROTO_UDP;
		udp_send(dst, (char *)sbuf, sizeof(sbuf));
	}
	return 1;
}
