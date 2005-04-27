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
 */

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

/*
 * Ping method. Any word except NULL is treated as method name.
 */
char *natping_method = NULL;

static usrloc_api_t ul;
/* TM bind */
static struct tm_binds tmb;
static int cblen = 0;
static const char sbuf[4] = {0, 0, 0, 0};

static void natping(unsigned int ticks, void *param);

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

		register_timer(natping, NULL, natping_interval);
	}

	return 0;
}

static void
natping(unsigned int ticks, void *param)
{
	int rval;
	void *buf, *cp;
	str c;
	struct sip_uri curi;
	union sockaddr_union to;
	struct hostent* he;
	struct socket_info* send_sock;
	str p_method, p_from;

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
		cblen = rval * 2;
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
	while (1) {
		memcpy(&(c.len), cp, sizeof(c.len));
		if (c.len == 0)
			break;
		c.s = (char *)cp + sizeof(c.len);
		cp =  (char *)cp + sizeof(c.len) + c.len;
		memcpy(&send_sock, cp, sizeof(send_sock));
		cp += sizeof(send_sock);

		if (natping_method != NULL) {
			/* XXX: add send_sock handling */
			p_method.s = natping_method;
			p_method.len = strlen(p_method.s);
			p_from.s = "sip:registrar"; /* XXX */
			p_from.len = strlen(p_from.s);
			if (tmb.t_request(&p_method, &c, &c, &p_from,
			    NULL, NULL, NULL, NULL) == -1) {
				LOG(L_ERR, "ERROR: nathelper::natping(): t_request() failed\n");
			}
		} else {
			if (parse_uri(c.s, c.len, &curi) < 0) {
				LOG(L_ERR, "ERROR: nathelper::natping: can't parse contact uri\n");
				continue;
			}
			if (curi.proto != PROTO_UDP && curi.proto != PROTO_NONE)
				continue;
			if (curi.port_no == 0)
				curi.port_no = SIP_PORT;
			he = sip_resolvehost(&curi.host, &curi.port_no, PROTO_UDP);
			if (he == NULL){
				LOG(L_ERR, "ERROR: nathelper::natping: can't resolve host\n");
				continue;
			}
			hostent2su(&to, he, 0, curi.port_no);
			if (send_sock == NULL) {
				send_sock = force_socket ? force_socket :
					get_send_socket(0, &to, PROTO_UDP);
			}
			if (send_sock == NULL) {
				LOG(L_ERR, "ERROR: nathelper::natping: can't get sending socket\n");
				continue;
			}
			udp_send(send_sock, (char *)sbuf, sizeof(sbuf), &to);
		}
	}
	pkg_free(buf);
}
