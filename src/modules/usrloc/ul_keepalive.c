/*
 * Usrloc module - keepalive
 *
 * Copyright (C) 2020 Asipto.com
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/resolve.h"
#include "../../core/forward.h"
#include "../../core/globals.h"
#include "../../core/pvar.h"
#include "../../core/sr_module.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_rr.h"

#include "ul_keepalive.h"

extern int ul_keepalive_timeout;

static int ul_ka_send(str *kamsg, dest_info_t *kadst);

/**
 *
_KAMETHOD_ _URI_ SIP/2.0\r\n
Via: SIP/2.0/_PROTO_ _IP_:_PORT_;branch=z9hG4bKx._GCNT_._BCNT_.0\r\n
__KAROUTES__
From: <_KAFROM_>;tag=_RUID_-_AORHASH_-_TIMESEC_-_TIMEUSEC_-_GCNT_._BCNT_\r\n
To: <sip:_AOR_>\r\n
Call-ID: _KACALLID_\r\n
CSeq: 1 _KAMETHOD_\r\n
Content-Length: 0\r\n\r\n"

*/

#define ULKA_CALLID_PREFIX "ksrulka-"
#define ULKA_CALLID_PREFIX_LEN (sizeof(ULKA_CALLID_PREFIX) - 1)

#define ULKA_MSG "%.*s %.*s SIP/2.0\r\n" \
  "Via: SIP/2.0/%.*s %s%.*s%s:%.*s;branch=z9hG4bKx.%u.%u.0\r\n" \
  "%s%.*s%.*s" \
  "From: <%.*s>;tag=%.*s-%x-%lx-%lx-%x.%x\r\n" \
  "To: <sip:%.*s%s%.*s>\r\n" \
  "Call-ID: " ULKA_CALLID_PREFIX "%u.%u\r\n" \
  "CSeq: 80 %.*s\r\n" \
  "Content-Length: 0\r\n\r\n"

extern str ul_ka_from;
extern str ul_ka_domain;
extern str ul_ka_method;
extern int ul_ka_mode;
extern int ul_ka_filter;
extern int ul_ka_loglevel;
extern pv_elem_t *ul_ka_logfmt;

extern unsigned int ul_nat_bflag;

static unsigned int _ul_ka_counter = 0;

/**
 *
 */
int ul_ka_urecord(urecord_t *ur)
{
	ucontact_t *uc;
#define ULKA_BUF_SIZE 2048
	char kabuf[ULKA_BUF_SIZE];
	int kabuf_len;
	str kamsg;
	str vaddr;
	str vport;
	str sdst = STR_NULL;
	str sproto = STR_NULL;
	sip_uri_t duri;
	char dproto;
	struct hostent *he;
	socket_info_t *ssock;
	dest_info_t idst;
	unsigned int bcnt = 0;
	unsigned int via_ipv6 = 0;
	int aortype = 0;
	int i;
	struct timeval tv;
	time_t tnow = 0;

	if (ul_ka_mode == ULKA_NONE) {
		return 0;
	}

	if(likely(destroy_modules_phase()!=0)) {
		return 0;
	}

	LM_DBG("keepalive for aor: %.*s\n", ur->aor.len, ur->aor.s);
	tnow = time(NULL);

	for(i=0; i<ur->aor.len; i++) {
		if(ur->aor.s[i] == '@') {
			aortype = 1;
			break;
		}
	}
	_ul_ka_counter++;
	for (uc = ur->contacts; uc != NULL; uc = uc->next) {
		if (uc->c.len <= 0) {
			continue;
		}
		if((ul_ka_filter&GAU_OPT_SERVER_ID) && (uc->server_id != server_id)) {
			continue;
		}
		if(ul_ka_mode & ULKA_NAT) {
			/* keepalive for natted contacts only */
			if (ul_nat_bflag == 0) {
				continue;
			}
			if ((uc->cflags & ul_nat_bflag) != ul_nat_bflag) {
				continue;
			}
		}

		if(ul_keepalive_timeout>0 && uc->last_keepalive>0) {
			if(uc->last_keepalive+ul_keepalive_timeout < tnow) {
				/* set contact as expired in 10s */
				LM_DBG("set expired contact on keepalive (%u + %u < %u)"
						" - aor: %.*s c: %.*s\n", (unsigned int)uc->last_keepalive,
						(unsigned int)ul_keepalive_timeout, (unsigned int)tnow,
						ur->aor.len, ur->aor.s, uc->c.len, uc->c.s);
				if(uc->expires > tnow + 10) {
					uc->expires = tnow + 10;
					continue;
				}
			}
		}
		if(uc->received.len > 0) {
			sdst = uc->received;
		} else {
			if (uc->path.len > 0) {
				if(get_path_dst_uri(&uc->path, &sdst) < 0) {
					LM_ERR("failed to get first uri for path\n");
					continue;
				}
			} else {
				sdst = uc->c;
			}
		}
		if(parse_uri(sdst.s, sdst.len, &duri) < 0) {
			LM_ERR("cannot parse next hop uri\n");
			continue;
		}

		if(duri.port_no == 0) {
			duri.port_no = SIP_PORT;
		}
		dproto = duri.proto;
		he = sip_resolvehost(&duri.host, &duri.port_no, &dproto);
		if(he == NULL) {
			LM_ERR("cannot resolve destination\n");
			continue;
		}
		if(ul_ka_mode & ULKA_UDP) {
			if(dproto != PROTO_UDP) {
				LM_DBG("skipping non-udp contact - proto %d\n", (int)dproto);
				continue;
			}
		}
		init_dest_info(&idst);
		hostent2su(&idst.to, he, 0, duri.port_no);
		ssock = uc->sock;
		if(ssock == NULL) {
			ssock = get_send_socket(0, &idst.to, dproto);
		}
		if(ssock == NULL) {
			LM_ERR("cannot get sending socket\n");
			continue;
		}
		idst.proto = dproto;
		idst.send_sock = ssock;
		idst.id = uc->tcpconn_id;

		if(ssock->useinfo.name.len > 0) {
			if (ssock->useinfo.address.af == AF_INET6) {
				via_ipv6 = 1;
			}
			vaddr = ssock->useinfo.name;
		} else {
			if (ssock->address.af == AF_INET6) {
				via_ipv6 = 1;
			}
			vaddr = ssock->address_str;
		}
		if(ssock->useinfo.port_no > 0) {
			vport = ssock->useinfo.port_no_str;
		} else {
			vport = ssock->port_no_str;
		}
		get_valid_proto_string(dproto, 1, 1, &sproto);

		bcnt++;
		gettimeofday(&tv, NULL);
		kabuf_len = snprintf(kabuf, ULKA_BUF_SIZE - 1, ULKA_MSG,
				ul_ka_method.len, ul_ka_method.s,
				uc->c.len, uc->c.s,
				sproto.len, sproto.s,
				(via_ipv6==1)?"[":"",
				vaddr.len, vaddr.s,
				(via_ipv6==1)?"]":"",
				vport.len, vport.s,
				_ul_ka_counter, bcnt,
				(uc->path.len>0)?"Route: ":"",
				(uc->path.len>0)?uc->path.len:0,
				(uc->path.len>0)?uc->path.s:"",
				(uc->path.len>0)?2:0,
				(uc->path.len>0)?"\r\n":"",
				ul_ka_from.len, ul_ka_from.s,
				uc->ruid.len, uc->ruid.s, ur->aorhash,
				(unsigned long)tv.tv_sec, (unsigned long)tv.tv_usec,
				_ul_ka_counter, bcnt,
				ur->aor.len, ur->aor.s,
				(aortype==1)?"":"@",
				(aortype==1)?0:ul_ka_domain.len, (aortype==1)?"":ul_ka_domain.s,
				_ul_ka_counter, bcnt,
				ul_ka_method.len, ul_ka_method.s);
		if(kabuf_len<=0 || kabuf_len>=ULKA_BUF_SIZE) {
			LM_ERR("failed to print the keepalive request\n");
		} else {
			LM_DBG("keepalive request (len: %d) [[\n%.*s]]\n",
					kabuf_len, kabuf_len, kabuf);
			kamsg.s = kabuf;
			kamsg.len = kabuf_len;
			ul_ka_send(&kamsg, &idst);
		}

	}
	return 0;
}

/**
 *
 */
static int ul_ka_send(str *kamsg, dest_info_t *kadst)
{
	if (kadst->proto == PROTO_UDP) {
		return udp_send(kadst, kamsg->s, kamsg->len);
	}

#ifdef USE_TCP
	else if(kadst->proto == PROTO_WS || kadst->proto == PROTO_WSS) {
		/*ws-wss*/
		return wss_send(kadst, kamsg->s, kamsg->len);
	}
	else if(kadst->proto == PROTO_TCP) {
		/*tcp*/
		return tcp_send(kadst, 0, kamsg->s, kamsg->len);
	}
#endif
#ifdef USE_TLS
	else if(kadst->proto == PROTO_TLS) {
		/*tls*/
		return tcp_send(kadst, 0, kamsg->s, kamsg->len);
	}
#endif
#ifdef USE_SCTP
	else if(kadst->proto == PROTO_SCTP) {
		/*sctp*/
		return sctp_core_msg_send(kadst, kamsg->s, kamsg->len);
	}
#endif
	else {
		LM_ERR("unknown proto [%d] for sending keepalive\n",
				kadst->proto);
		return -1;
	}
}

/**
 *
 */
unsigned long ul_ka_fromhex(str *shex, int *err)
{
    unsigned long v = 0;
	int i;

	*err = 0;
    for (i=0; i<shex->len; i++) {
        char b = shex->s[i];
        if (b >= '0' && b <= '9') b = b - '0';
        else if (b >= 'a' && b <='f') b = b - 'a' + 10;
        else if (b >= 'A' && b <='F') b = b - 'A' + 10;
		else { *err = 1; return 0; };
        v = (v << 4) | (b & 0xF);
    }
    return v;
}

/**
 *
 */
int ul_ka_reply_received(sip_msg_t *msg)
{
	to_body_t *fb;
	str ruid;
	str tok;
	int err;
	unsigned int aorhash;
	char *p;
	struct timeval tvm;
	struct timeval tvn;
	unsigned int tvdiff;

	if(msg->cseq == NULL) {
		if((parse_headers(msg, HDR_CSEQ_F, 0) == -1) || (msg->cseq == NULL)) {
			LM_ERR("invalid CSeq header\n");
			return -1;
		}
	}

	if(get_cseq(msg)->method.len != ul_ka_method.len) {
		return 1;
	}
	if(strncmp(get_cseq(msg)->method.s, ul_ka_method.s, ul_ka_method.len) != 0) {
		return 1;
	}

	/* there must be no second via */
	if(!(parse_headers(msg, HDR_VIA2_F, 0) == -1 || (msg->via2 == 0)
			   || (msg->via2->error != PARSE_OK))) {
		return 1;
	}

	/* from uri check */
	if((parse_from_header(msg)) < 0) {
		LM_ERR("cannot parse From header\n");
		return -1;
	}

	fb = get_from(msg);
	if(fb->uri.len != ul_ka_from.len
			|| strncmp(fb->uri.s, ul_ka_from.s, ul_ka_from.len) != 0) {
		return 1;
	}

	/* from-tag is: ruid-aorhash-tmsec-tmusec-counter */
	if(fb->tag_value.len <= 8) {
		return 1;
	}

	LM_DBG("checking keepalive reply [%.*s]\n", fb->tag_value.len,
			fb->tag_value.s);

	/* todo: macro/function to tokenize */
	/* skip counter */
	p = q_memrchr(fb->tag_value.s, '-', fb->tag_value.len);
	if(p == NULL) {
		LM_DBG("from tag format mismatch [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		return 1;
	}

	/* tv_usec hash */
	tok.len = p - fb->tag_value.s;
	p = q_memrchr(fb->tag_value.s, '-', tok.len);
	if(p == NULL) {
		LM_DBG("from tag format mismatch [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		return 1;
	}
	tok.s = p + 1;
	tok.len = fb->tag_value.s + tok.len - tok.s;
	if(tok.len <= 0) {
		LM_DBG("empty token\n");
		return 1;
	}
	LM_DBG("tv usec string is [%.*s] (%d)\n", tok.len, tok.s, tok.len);
	tvm.tv_usec = ul_ka_fromhex(&tok, &err);
	if(err==1) {
		LM_DBG("invalid tv usec value\n");
		return 1;
	}
	LM_DBG("tv usec is [%lu]\n", (unsigned long)tvm.tv_usec);

	/* tv_sec hash */
	tok.len = p - fb->tag_value.s;
	p = q_memrchr(fb->tag_value.s, '-', tok.len);
	if(p == NULL) {
		LM_DBG("from tag format mismatch [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		return 1;
	}
	tok.s = p + 1;
	tok.len = fb->tag_value.s + tok.len - tok.s;
	if(tok.len <= 0) {
		LM_DBG("empty token\n");
		return 1;
	}
	LM_DBG("tv sec string is [%.*s] (%d)\n", tok.len, tok.s, tok.len);
	tvm.tv_sec = ul_ka_fromhex(&tok, &err);
	if(err==1) {
		LM_DBG("invalid tv sec value\n");
		return 1;
	}
	LM_DBG("tv sec is [%lu]\n", (unsigned long)tvm.tv_sec);

	/* aor hash */
	tok.len = p - fb->tag_value.s;
	p = q_memrchr(fb->tag_value.s, '-', tok.len);
	if(p == NULL) {
		LM_DBG("from tag format mismatch [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		return 1;
	}
	tok.s = p + 1;
	tok.len = fb->tag_value.s + tok.len - tok.s;
	if(tok.len <= 0) {
		LM_DBG("empty token\n");
		return 1;
	}
	LM_DBG("aor hash string is [%.*s] (%d)\n", tok.len, tok.s, tok.len);
	aorhash = ul_ka_fromhex(&tok, &err);
	if(err==1) {
		LM_DBG("invalid aor hash value\n");
		return 1;
	}
	LM_DBG("aor hash is [%u]\n", aorhash);

	ruid.s = fb->tag_value.s;
	ruid.len = tok.s - ruid.s - 1;

	if(ruid.len <= 0) {
		LM_DBG("cannot get ruid in [%.*s]\n", fb->tag_value.len,
				fb->tag_value.s);
		return 1;
	}

	gettimeofday(&tvn, NULL);
	tvdiff = (tvn.tv_sec - tvm.tv_sec) * 1000000
					+ (tvn.tv_usec - tvm.tv_usec);
	ul_update_keepalive(aorhash, &ruid, tvn.tv_sec, tvdiff);

	if(ul_ka_loglevel != 255 && ul_ka_logfmt != NULL) {
		if (pv_printf_s(msg, ul_ka_logfmt, &tok) == 0) {
			LOG(ul_ka_loglevel, "keepalive roundtrip: %u.%06u sec - ruid [%.*s]%.*s\n",
					tvdiff/1000000, tvdiff%1000000, ruid.len, ruid.s,
					tok.len, tok.s);
			return 0;
		}
	}

	LM_DBG("response of keepalive for ruid [%.*s] aorhash [%u] roundtrip: %u.%06u secs\n",
			ruid.len, ruid.s, aorhash, tvdiff/1000000, tvdiff%1000000);

	return 0;
}
