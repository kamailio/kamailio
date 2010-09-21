/* $Id$
 *
 * Copyright (C) 2007 Tomas Mandys
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_body.h"
#include "../../resolve.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../msg_translator.h"
#include "../../socket_info.h"
#include "../../select.h"
#include "../../select_buf.h"
#include "../../script_cb.h"
#include "../../cfg_parser.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/netfilter/xt_RTPPROXY.h>
#include <arpa/inet.h>

MODULE_VERSION

#define MODULE_NAME "iptrtpproxy"

/* max.number of RTP streams per session */
#define MAX_MEDIA_NUMBER 20
#define MAX_SWITCHBOARD_NAME_LEN 20

struct switchboard_item {
	str name;
	int ringing_timeout;
	struct xt_rtpproxy_sockopt_in_switchboard in_switchboard;
	struct xt_rtpproxy_sockopt_in_alloc_session in_session;
	unsigned int param_ids;

	struct switchboard_item* next;
};

static char* global_session_ids;
static str sdp_ip;
static struct xt_rtpproxy_handle handle = {.sockfd = 0};
static struct switchboard_item* switchboards = NULL;
static struct switchboard_item* found_switchboard;
static int found_direction;
static int switchboard_count = 0;
static str iptrtpproxy_cfg_filename = STR_STATIC_INIT("/etc/iptrtpproxy.cfg");
static int iptrtpproxy_cfg_flag = 0;


static struct switchboard_item* find_switchboard(str *name) {
	struct switchboard_item* p;
	for (p = switchboards; p; p=p->next) {
		if (name->len == p->name.len && strncasecmp(p->name.s, name->s, name->len)==0) break;
	}
	return p;
}

/** if succesfull allocated sessions available @rtpproxy.session_ids
 */

static int rtpproxy_alloc_fixup(void** param, int param_no) {
	switch (param_no) {
		case 1:
			return fixup_var_int_12(param, param_no);
		case 2:
			return fixup_var_str_12(param, param_no);
		default:
			return 0;
	}
}

static int rtpproxy_update_fixup(void** param, int param_no) {
	switch (param_no) {
		case 1:
			return rtpproxy_alloc_fixup(param, param_no);
		case 2:
			return fixup_var_str_12(param, param_no);
		default:
			return 0;
	}
}

static int rtpproxy_delete_fixup(void** param, int param_no) {
	return rtpproxy_update_fixup(param, 2);
}

static int rtpproxy_find_fixup(void** param, int param_no) {
	return fixup_var_str_12(param, param_no);
}

struct sdp_session {
	unsigned int media_count;
	struct {
		int active;
		unsigned short port;
		unsigned int ip;
		str ip_s;
		str port_s;
	} media[MAX_MEDIA_NUMBER];
};

struct ipt_session {
	struct switchboard_item *switchboard;
	unsigned int stream_count;
	struct {
		int sess_id;
		int created;
		unsigned short proxy_port;
	} streams[MAX_MEDIA_NUMBER];
};

static unsigned int s2ip4(str *s) {
	struct in_addr res;
	char c2;
	c2 = s->s[s->len];
	s->s[s->len] = '\0';
	if (!inet_aton(s->s, &res)) {
		s->s[s->len] = c2;
		return 0;
	}
	s->s[s->len] = c2;
	return res.s_addr;
}

static void ip42s(unsigned int ip, str *s) {
	struct in_addr ip2 = { ip };
	s->s = inet_ntoa(ip2);
	s->len = strlen(s->s);
}

#define is_alpha(_c) (((_c) >= 'a' && (_c) <= 'z') || ((_c) >= 'A' && (_c) <= 'Z') || ((_c) >= '0' && (_c) <= '9') || ((_c) == '_') || ((_c) == '-'))

inline static int next_sdp_line(char** p, char* pend, char *ltype, str* line) {
	char *cp;
	while (*p < pend) {
		while (*p < pend && (**p == '\n' || **p == '\r')) (*p)++;
		for (cp = *p; cp < pend && *cp != '\n' && *cp != '\r'; cp++);

		if (cp-*p > 2 && (*p)[1] == '=') {
			*ltype = **p;
			line->s = (*p)+2;
			line->len = cp-line->s;
			*p = cp;
			return 0;
		}
		*p = cp;
	}
	return -1;
};

/* SDP RFC2327 */
static int parse_sdp_content(struct sip_msg* msg, struct sdp_session *sess) {
	char *p, *pend, *cp, *cp2, *lend;
	str line, cline_ip_s, body;
	int sess_fl, i, cline_count;
	char ltype, savec;
	unsigned int cline_ip;

	static str supported_media_types[] = {
		STR_STATIC_INIT("udp"),
		STR_STATIC_INIT("udptl"),
		STR_STATIC_INIT("rtp/avp"),
		STR_STATIC_INIT("rtp/savpf"),
		STR_NULL
	};
	memset(sess, 0, sizeof(*sess));
	
	
	/* try to get the body part with application/sdp */
	body.s = get_body_part(msg, TYPE_APPLICATION, SUBTYPE_SDP, &body.len);
	if (!body.s) {
		ERR(MODULE_NAME": parse_sdp_content: failed to get the application/sdp body\n");
		return -1;
	}
	
	#if 0
	body.s = get_body(msg);
	if (body.s==0) {
		ERR(MODULE_NAME": parse_sdp_content: failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s - msg->buf);
	if (body.len==0) {
		ERR(MODULE_NAME": parse_sdp_content: message body has length zero\n");
		return -1;
	}

	/* no need for parse_headers(msg, EOH), get_body will parse everything */
	if (!msg->content_type)
	{
		WARN(MODULE_NAME": parse_sdp_content: Content-TYPE header absent!"
			"let's assume the content is text/plain\n");
	}
	else {
		trim_len(line.len, line.s, msg->content_type->body);
		if (line.len != sizeof("application/sdp")-1 || strncasecmp(line.s, "application/sdp", line.len) != 0) {
			ERR(MODULE_NAME": parse_sdp_content: bad content type '%.*s'\n", line.len, line.s);
			return -1;
		}
	}
	#endif
	/*
	 * Parsing of SDP body.
	 * It can contain a few session descriptions (each starts with
	 * v-line), and each session may contain a few media descriptions
	 * (each starts with m-line).
	 * We have to change ports in m-lines, and also change IP addresses in
	 * c-lines which can be placed either in session header (fallback for
	 * all medias) or media description.
	 * Ports should be allocated for any media. IPs all should be changed
	 * to the same value (RTP proxy IP), so we can change all c-lines
	 * unconditionally.
	 * There are sendonly,recvonly modifiers which signalize one-way
	 * streaming, it probably won't work but it's handled the same way,
	 * RTCP commands are still bi-directional. "Inactive" modifier
	 * is not handled anyway. See RFC3264
	 */

	p = body.s;
	pend = body.s + body.len;
	sess_fl = 0;
	sess->media_count = 0;
	cline_ip_s.s = NULL;  /* make gcc happy */
	cline_ip_s.len = 0;
	cline_ip = 0;
	cline_count = 0;
	while (p < pend) {
		if (next_sdp_line(&p, pend, &ltype, &line) < 0) break;
		switch (ltype) {
			case 'v':
				/* Protocol Version: v=0 */
				if (sess_fl != 0) {
					ERR(MODULE_NAME": parse_sdp_content: only one session allowed\n");  /* RFC3264 */
					return -1;
				}
				sess_fl = 1;
				break;
			case 'c':
				/* Connection Data: c=<network type> <address type> <connection address>, ex. c=IN IP4 224.2.17.12/127 */
				switch (sess_fl) {
					case 0:
						ERR(MODULE_NAME": parse_sdp_content: c= line is not in session section\n");
						return -1;
					case 1:
					case 2:
						cline_count++;
						if (cline_count > 1) {
							/* multicast not supported */
							if (sess_fl == 2) {
								goto invalidate;
							}
							else {
								cline_ip_s.len = 0;
							}
							break;
						}
						lend = line.s + line.len;
						cp = eat_token_end(line.s, lend);
						if (cp-line.s != 2 || memcmp(line.s, "IN", 2) != 0) {
							goto invalidate;
						}
						cp = eat_space_end(cp, lend);
						line.s = cp;
						cp = eat_token_end(cp, lend);
						if (cp-line.s != 3 || memcmp(line.s, "IP4", 3) != 0) {
							goto invalidate;
						}
						cp = eat_space_end(cp, lend);
						line.s = cp;
						cp = eat_token_end(cp, lend);
						line.len = cp-line.s;
						if (line.len == 0 || q_memchr(line.s, '/', line.len)) {
							/* multicast address not supported */
							goto invalidate;
						}
						if (sess_fl == 1) {
							cline_ip_s = line;
							cline_ip = s2ip4(&line);
						}
						else {
							sess->media[sess->media_count-1].ip = s2ip4(&line);
							sess->media[sess->media_count-1].active = 1;  /* IP may by specified by hostname */
							sess->media[sess->media_count-1].ip_s = line;
						}
						break;
					default:
						;
				}
				break;
			invalidate:
				if (sess_fl == 2) {
					sess->media[sess->media_count-1].active = 0;
				}
				break;
			case 'm':
				/* Media Announcements: m=<media> <port>[/<number of ports>] <transport> <fmt list>, eg. m=audio 49170 RTP/AVP 0 */
				switch (sess_fl) {
					case 0:
						ERR(MODULE_NAME": parse_sdp_content: m= line is not in session section\n");
						return -1;
					case 1:
					case 2:
						if (sess->media_count >= MAX_MEDIA_NUMBER) {
							ERR(MODULE_NAME": parse_sdp_content: max.number of medias (%d) exceeded\n", MAX_MEDIA_NUMBER);
							return -1;
						}
						cline_count = 0;
						sess_fl = 2;
						sess->media_count++;
						sess->media[sess->media_count-1].active = 0;
						lend = line.s + line.len;
						cp = eat_token_end(line.s, lend);
						if (cp-line.s == 0) {
							break;
						}
						cp = eat_space_end(cp, lend);
						line.s = cp;
						cp = eat_token_end(cp, lend);
						line.len = cp-line.s;
						
						cp2 = q_memchr(line.s, '/', line.len);
						if (cp2) {
							/* strip optional number of ports, if present should be 2 */
							line.len = cp2-line.s;
						}
						sess->media[sess->media_count-1].port_s = line;
						if (line.len == 0) { /* invalid port? */
							break;
						}
						savec = line.s[line.len];
						line.s[line.len] = '\0';
						sess->media[sess->media_count-1].port = atol(line.s);
						line.s[line.len] = savec;
						if (sess->media[sess->media_count-1].port == 0) {
							break;
						}
						cp = eat_space_end(cp, lend);
						
						line.s = cp;
						cp = eat_token_end(cp, lend);
						line.len = cp-line.s;
						for (i = 0; supported_media_types[i].s != NULL; i++) {
							if (line.len == supported_media_types[i].len &&
								strncasecmp(line.s, supported_media_types[i].s, line.len) == 0) {
								sess->media[sess->media_count-1].active = cline_ip_s.len != 0;  /* IP may by specified by hostname */
								sess->media[sess->media_count-1].ip_s = cline_ip_s;
								sess->media[sess->media_count-1].ip = cline_ip;
								break;
							}
						}
						break;
					default:
						;
				}

				break;
			default:
				;
		}
	}
	return 0;
}

static int prepare_lumps(struct sip_msg* msg, str* position, str* s) {
	struct lump* anchor;
	char *buf;

//ERR("'%.*s' --> '%.*s'\n", position->len, position->s, s->len, s->s);	
	anchor = del_lump(msg, position->s - msg->buf, position->len, 0);
	if (anchor == NULL) {
		ERR(MODULE_NAME": prepare_lumps: del_lump failed\n");
		return -1;
	}
	buf = pkg_malloc(s->len);
	if (buf == NULL) {
		ERR(MODULE_NAME": prepare_lumps: out of memory\n");
		return -1;
	}
	memcpy(buf, s->s, s->len);
	if (insert_new_lump_after(anchor, buf, s->len, 0) == 0) {
		ERR(MODULE_NAME": prepare_lumps: insert_new_lump_after failed\n");
		pkg_free(buf);
		return -1;
	}
	return 0;
}

static int update_sdp_content(struct sip_msg* msg, int gate_a_to_b, struct sdp_session *sdp_sess, struct ipt_session *ipt_sess) {
	int i, j;
	str s;
	/* we must apply lumps for relevant c= and m= lines */
	sdp_ip.len = 0;
	for (i=0; i<sdp_sess->media_count; i++) {
		if (sdp_sess->media[i].active) {
			for (j=0; j<i; j++) {
				if (sdp_sess->media[j].active && sdp_sess->media[i].ip_s.s == sdp_sess->media[j].ip_s.s) {
					goto cline_fixed;
				}
			}
			if (sdp_ip.len == 0) {
				/* takes 1st ip to be rewritten, for aux purposes only */
				sdp_ip = sdp_sess->media[i].ip_s;
			}
			/* apply lump for ip address in c= line */
			ip42s(ipt_sess->switchboard->in_switchboard.gate[!gate_a_to_b].ip, &s);
			if (prepare_lumps(msg, &sdp_sess->media[i].ip_s, &s) < 0)
				return -1;
	cline_fixed:
			/* apply lump for port in m= line */
			s.s = int2str(ipt_sess->streams[i].proxy_port, &s.len);
			if (prepare_lumps(msg, &sdp_sess->media[i].port_s, &s) < 0)
				return -1;
		}
	}
	return 0;
}

/* null terminated result is allocated at static buffer */
static void serialize_ipt_session(struct ipt_session* sess, str* session_ids) {
	static char buf[MAX_SWITCHBOARD_NAME_LEN+1+(5+1+1+10)*MAX_MEDIA_NUMBER+1];
	char *p;
	int i;
	buf[0] = '\0';
	p = buf;
	memcpy(p, sess->switchboard->name.s, sess->switchboard->name.len);
	p += sess->switchboard->name.len;
	*p = ':';
	p++;
	for (i=0; i<sess->stream_count; i++) {
		if (sess->streams[i].sess_id >= 0) {
			p += sprintf(p, "%u/%u", sess->streams[i].sess_id, sess->streams[i].created);
		}
		*p = ',';
		p++;
	}
	p--;
	*p = '\0';
	session_ids->s = buf;
	session_ids->len = p - buf;
}

/* switchboardname [":" [sess_id "/" created] [ * ( "," [sess_id "/" created] )] ] */
static int unserialize_ipt_session(str* session_ids, struct ipt_session* sess) {
	char *p, *pend, savec;
	str s;
	memset(sess, 0, sizeof(*sess));
	p = session_ids->s;
	pend = session_ids->s+session_ids->len;
	s.s = p;
	while (p < pend && is_alpha(*p)) p++;
	s.len = p-s.s;
	sess->switchboard = find_switchboard(&s);
	if (!sess->switchboard) {
		ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', switchboard '%.*s' not found\n", session_ids->len, session_ids->s, s.len, s.s);
		return -1;
	}
	if (p == pend) return 0;
	if (*p != ':') {
		ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', colon expected near '%.*s'\n", session_ids->len, session_ids->s, pend-p, p);
		return -1;
	}
	do {
		if (sess->stream_count >= MAX_MEDIA_NUMBER) {
		ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', max.media number (%d) exceeded\n", session_ids->len, session_ids->s, MAX_MEDIA_NUMBER);
			return -1;
		}
		p++;
		sess->stream_count++;
		sess->streams[sess->stream_count-1].sess_id = -1;
		sess->streams[sess->stream_count-1].created = 0;
		s.s = p;
		while (p < pend && (*p >= '0' && *p <= '9')) p++;
		if (p != pend && *p != '/') {
			ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', '/' expected near '%.*s'\n", session_ids->len, session_ids->s, pend-p, p);
			return -1;
		}
		s.len = p-s.s;
		if (s.len > 0) {
			savec = s.s[s.len];
			s.s[s.len] = '\0';
			sess->streams[sess->stream_count-1].sess_id = atol(s.s);
			s.s[s.len] = savec;
		}
		p++;
		s.s = p;
		while (p < pend && (*p >= '0' && *p <= '9')) p++;
		if (p != pend && *p != ',') {
			sess->streams[sess->stream_count-1].sess_id = -1;
			ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', comma expected near '%.*s'\n", session_ids->len, session_ids->s, pend-p, p);
			return -1;
		}
		s.len = p-s.s;
		if (s.len > 0) {
			savec = s.s[s.len];
			s.s[s.len] = '\0';
			sess->streams[sess->stream_count-1].created = atol(s.s);
			s.s[s.len] = savec;
		}
	} while (p < pend);
	return 0;
}

static void delete_ipt_sessions(struct ipt_session* ipt_sess) {
	struct xt_rtpproxy_sockopt_in_sess_id in_sess_id;
	int i, j;
	for (i=0; i < ipt_sess->stream_count; i++) {
		if (ipt_sess->streams[i].sess_id >= 0) {
			j = i;
			in_sess_id.sess_id_min = ipt_sess->streams[i].sess_id;
			in_sess_id.sess_id_max = in_sess_id.sess_id_min;
			in_sess_id.created = ipt_sess->streams[i].created;
			/* group more sessions if possible */
			for (; i < ipt_sess->stream_count-1; i++) {
				if (ipt_sess->streams[i+1].sess_id >= 0) {
					if (ipt_sess->streams[i+1].sess_id == in_sess_id.sess_id_max+1) {
						in_sess_id.sess_id_max = ipt_sess->streams[i+1].sess_id;
						continue;
					}
					break;
				}
			}
			if (xt_RTPPROXY_delete_session(&handle, &ipt_sess->switchboard->in_switchboard, &in_sess_id) < 0) {
				ERR(MODULE_NAME": rtpproxy_delete: xt_RTPPROXY_delete_session error: %s (%d)\n", handle.err_str, handle.err_no);
				/* what to do ? */
			}
			/* invalidate sessions including duplicates */
			for (; j<ipt_sess->stream_count; j++) {
				if (ipt_sess->streams[j].sess_id >= in_sess_id.sess_id_min && ipt_sess->streams[j].sess_id <= in_sess_id.sess_id_max)
					ipt_sess->streams[j].sess_id = -1;
			}
		}
	}
}

#define GATE_FLAG 0x01
#define RINGING_TIMEOUT_FLAG 0x02

/* gate_a_to_b has index 0, gate_b_to_a 1 */
#define GATE_A_TO_B(flags) (((flags) & GATE_FLAG) == 0)

inline static void fill_in_session(int flags, int media_idx, struct sdp_session *sdp_sess, struct ipt_session *ipt_sess, struct xt_rtpproxy_sockopt_in_alloc_session *in_session) {
	int j;
	for (j=0; j<2; j++) {
		in_session->source[GATE_A_TO_B(flags)].stream[j].flags = 
			XT_RTPPROXY_SOCKOPT_FLAG_SESSION_ADDR |
			ipt_sess->switchboard->in_session.source[GATE_A_TO_B(flags)].stream[j].flags |
			((flags & RINGING_TIMEOUT_FLAG) ? XT_RTPPROXY_SOCKOPT_FLAG_SESSION_LEARNING_TIMEOUT : 0);
		in_session->source[GATE_A_TO_B(flags)].stream[j].learning_timeout = (flags & RINGING_TIMEOUT_FLAG) ? 
			ipt_sess->switchboard->ringing_timeout :
			ipt_sess->switchboard->in_session.source[GATE_A_TO_B(flags)].stream[j].learning_timeout;
		in_session->source[GATE_A_TO_B(flags)].stream[j].addr.ip = sdp_sess->media[media_idx].ip;
		in_session->source[GATE_A_TO_B(flags)].stream[j].addr.port = sdp_sess->media[media_idx].port+j;
	}
	in_session->source[GATE_A_TO_B(flags)].always_learn = ipt_sess->switchboard->in_session.source[GATE_A_TO_B(flags)].always_learn;
}

static int rtpproxy_alloc(struct sip_msg* msg, char* _flags, char* _switchboard_id) {
	int flags;
	struct switchboard_item* si = 0;
	struct sdp_session sdp_sess;
	struct ipt_session ipt_sess;
	struct xt_rtpproxy_sockopt_in_alloc_session in_session;
	struct xt_rtpproxy_session out_session;
	str s;
	int i;

	if (get_int_fparam(&flags, msg, (fparam_t*) _flags) < 0) {
		return -1;
	}
	if (get_str_fparam(&s, msg, (fparam_t*) _switchboard_id) < 0) {
		return -1;
	}
	if (s.len) {
		/* switchboard must be fully qualified, it simplifies helper because it's not necessary to store full identification to session_ids - name is sufficient */
		si = find_switchboard(&s);
		if (!si) {
			ERR(MODULE_NAME": rtpproxy_alloc: switchboard '%.*s' not found\n", s.len, s.s);
			return -1;
		}
	}
	else {
		if (!found_switchboard) {
			ERR(MODULE_NAME": rtpproxy_alloc: no implicit switchboard\n");
			return -1;
		}
		si = found_switchboard;
	}
	if (parse_sdp_content(msg, &sdp_sess) < 0)
		return -1;
	memset(&ipt_sess, 0, sizeof(ipt_sess));
	ipt_sess.switchboard = si;
	memset(&in_session, 0, sizeof(in_session));
	for (i = 0; i < sdp_sess.media_count; i++) {
		ipt_sess.streams[i].sess_id = -1;
		ipt_sess.stream_count = i+1;
		if (sdp_sess.media[i].active) {
			int j;
			for (j = 0; j < i; j++) {
				/* if two media streams have equal source address than we will allocate only one ipt session */
				if (sdp_sess.media[j].active && sdp_sess.media[i].ip == sdp_sess.media[j].ip && sdp_sess.media[i].port == sdp_sess.media[j].port) {
					ipt_sess.streams[i].sess_id = ipt_sess.streams[j].sess_id;
					ipt_sess.streams[i].proxy_port = ipt_sess.streams[j].proxy_port;
					ipt_sess.streams[i].created = ipt_sess.streams[j].created;
					goto cont;
				}
			}
			fill_in_session(flags, i, &sdp_sess, &ipt_sess, &in_session);
			if (xt_RTPPROXY_alloc_session(&handle, &ipt_sess.switchboard->in_switchboard, &in_session, NULL, &out_session) < 0) {
				ERR(MODULE_NAME": rtpproxy_alloc: xt_RTPPROXY_alloc_session error: %s (%d)\n", handle.err_str, handle.err_no);
				delete_ipt_sessions(&ipt_sess);
				return -1;
			}
			ipt_sess.streams[i].sess_id = out_session.sess_id;
			ipt_sess.streams[i].created = out_session.created;
			ipt_sess.streams[i].proxy_port = out_session.gate[!GATE_A_TO_B(flags)].stream[0].port;
		cont: ;
		}
	}
	if (update_sdp_content(msg, GATE_A_TO_B(flags), &sdp_sess, &ipt_sess) < 0) {
		delete_ipt_sessions(&ipt_sess);
		return -1;
	}
	serialize_ipt_session(&ipt_sess, &s);
	global_session_ids = s.s; /* it's static and null terminated */
	return 1;
}

static int rtpproxy_update(struct sip_msg* msg, char* _flags, char* _session_ids) {
	str session_ids;
	int flags, i;
	struct sdp_session sdp_sess;
	struct ipt_session ipt_sess;
	struct xt_rtpproxy_sockopt_in_sess_id in_sess_id;
	struct xt_rtpproxy_sockopt_in_alloc_session in_session;

	if (get_int_fparam(&flags, msg, (fparam_t*) _flags) < 0) {
		return -1;
	}
	if (get_str_fparam(&session_ids, msg, (fparam_t*) _session_ids) < 0) {
		return -1;
	}
	if (unserialize_ipt_session(&session_ids, &ipt_sess) < 0) {
		return -1;
	}
	if (parse_sdp_content(msg, &sdp_sess) < 0)
		return -1;

	if (ipt_sess.stream_count != sdp_sess.media_count) {
		ERR(MODULE_NAME": rtpproxy_update: number of m= item in offer (%d) and answer (%d) do not correspond\n", ipt_sess.stream_count, sdp_sess.media_count);
		return -1;
	}
	/* first we check for unexpected duplicate source ports */
	for (i = 0; i < sdp_sess.media_count; i++) {
		if (ipt_sess.streams[i].sess_id >= 0 && sdp_sess.media[i].active) {
			int j;
			for (j = i+1; j < sdp_sess.media_count; j++) {
				if (ipt_sess.streams[j].sess_id >= 0 && sdp_sess.media[j].active) {
					/* if two media streams have equal source address XOR have equal session */
					if ( (sdp_sess.media[i].ip == sdp_sess.media[j].ip && sdp_sess.media[i].port == sdp_sess.media[j].port) ^
						 (ipt_sess.streams[i].sess_id == ipt_sess.streams[j].sess_id) ) {
						ERR(MODULE_NAME": rtpproxy_update: media (%d,%d) violation number\n", i, j);
						return -1;
					}
				}
			}
		}
	}

	memset(&in_session, 0, sizeof(in_session));
	for (i = 0; i < sdp_sess.media_count; i++) {
		if (ipt_sess.streams[i].sess_id >= 0) {
			in_sess_id.sess_id_min = ipt_sess.streams[i].sess_id;
			in_sess_id.created = ipt_sess.streams[i].created;
			in_sess_id.sess_id_max = in_sess_id.sess_id_min;
			if (sdp_sess.media[i].active) {
				fill_in_session(flags, i, &sdp_sess, &ipt_sess, &in_session);
				if (xt_RTPPROXY_update_session(&handle, &ipt_sess.switchboard->in_switchboard, &in_sess_id, &in_session) < 0) {
					ERR(MODULE_NAME": rtpproxy_alloc: xt_RTPPROXY_update_session error: %s (%d)\n", handle.err_str, handle.err_no);
					/* delete all sessions ? */
					return -1;
				}
				/* we don't know proxy port - it was known when being allocated so we got from switchboard - it's not too clear solution because it requires knowledge how ports are allocated */
				ipt_sess.streams[i].proxy_port = ipt_sess.switchboard->in_switchboard.gate[!GATE_A_TO_B(flags)].port + 2*ipt_sess.streams[i].sess_id;
			}
			else {
				/* can we delete any session allocated during offer? */
				if (xt_RTPPROXY_delete_session(&handle, &ipt_sess.switchboard->in_switchboard, &in_sess_id) < 0) {
					ERR(MODULE_NAME": rtpproxy_update: xt_RTPPROXY_delete_session error: %s (%d)\n", handle.err_str, handle.err_no);
				}
				ipt_sess.streams[i].sess_id = -1;
			}
		}
	}
	if (update_sdp_content(msg, GATE_A_TO_B(flags), &sdp_sess, &ipt_sess) < 0) {
		/* delete all sessions ? */
		return -1;
	}
	serialize_ipt_session(&ipt_sess, &session_ids);
	global_session_ids = session_ids.s; /* it's static and null terminated */
	return 1;
}

static int rtpproxy_adjust_timeout(struct sip_msg* msg, char* _flags, char* _session_ids) {
	str session_ids;
	int flags, i;
	struct ipt_session ipt_sess;
	struct xt_rtpproxy_sockopt_in_sess_id in_sess_id;
	struct xt_rtpproxy_sockopt_in_alloc_session in_session;

	if (get_int_fparam(&flags, msg, (fparam_t*) _flags) < 0) {
		return -1;
	}
	if (get_str_fparam(&session_ids, msg, (fparam_t*) _session_ids) < 0) {
		return -1;
	}
	if (unserialize_ipt_session(&session_ids, &ipt_sess) < 0) {
		return -1;
	}

	memset(&in_session, 0, sizeof(in_session));
	for (i = 0; i < ipt_sess.stream_count; i++) {
		if (ipt_sess.streams[i].sess_id >= 0) {
			int j;
			in_sess_id.sess_id_min = ipt_sess.streams[i].sess_id;
			in_sess_id.created = ipt_sess.streams[i].created;
			in_sess_id.sess_id_max = in_sess_id.sess_id_min;


			for (j=0; j<2; j++) {

				in_session.source[GATE_A_TO_B(flags)].stream[j].flags = 
					(flags & RINGING_TIMEOUT_FLAG) ? 
						XT_RTPPROXY_SOCKOPT_FLAG_SESSION_LEARNING_TIMEOUT : 
						(ipt_sess.switchboard->in_session.source[GATE_A_TO_B(flags)].stream[j].flags & XT_RTPPROXY_SOCKOPT_FLAG_SESSION_LEARNING_TIMEOUT)
					;
				in_session.source[GATE_A_TO_B(flags)].stream[j].learning_timeout = (flags & RINGING_TIMEOUT_FLAG) ? 
					ipt_sess.switchboard->ringing_timeout :
					ipt_sess.switchboard->in_session.source[GATE_A_TO_B(flags)].stream[j].learning_timeout;

			}

			if (xt_RTPPROXY_update_session(&handle, &ipt_sess.switchboard->in_switchboard, &in_sess_id, &in_session) < 0) {
				ERR(MODULE_NAME": rtpproxy_alloc: xt_RTPPROXY_adjust_timeout error: %s (%d)\n", handle.err_str, handle.err_no);
					return -1;
			}
		}
	}
	/* do not serialize sessions because it affect static buffer and more valuable values disappears */
	return 1;
}

static int rtpproxy_delete(struct sip_msg* msg, char* _session_ids, char* dummy) {
	str session_ids;
	struct ipt_session ipt_sess;
	if (get_str_fparam(&session_ids, msg, (fparam_t*) _session_ids) < 0) {
		return -1;
	}
	if (unserialize_ipt_session(&session_ids, &ipt_sess) < 0) {
		return -1;
	}
	delete_ipt_sessions(&ipt_sess);
	/* do not serialize sessions because it affect static buffer and more valuable values disappears */
	return 1;
}

static int rtpproxy_find(struct sip_msg* msg, char* _gate_a, char* _gate_b) {
	unsigned int ip_a, ip_b;
	str gate_a, gate_b;

	if (get_str_fparam(&gate_a, msg, (fparam_t*) _gate_a) < 0) {
		return -1;
	}
	ip_a = s2ip4(&gate_a);
	if (get_str_fparam(&gate_b, msg, (fparam_t*) _gate_b) < 0) {
		return -1;
	}
	ip_b = s2ip4(&gate_b);

	found_direction = -1;
	for (found_switchboard = switchboards; found_switchboard; found_switchboard=found_switchboard->next) {
		if (ip_a == found_switchboard->in_switchboard.gate[0].ip) {
			if (ip_b == found_switchboard->in_switchboard.gate[1].ip) {
				found_direction = 1;
				return 1;
				break;
			}
		}
		else if (ip_a == found_switchboard->in_switchboard.gate[1].ip) {
			if (ip_b == found_switchboard->in_switchboard.gate[0].ip) {
				found_direction = 0;
				return 1;
			}
		}
	}
	return -1;
}

/* @select implementation */
static int sel_rtpproxy(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	return 0;
}

static int sel_sdp_ip(str* res, select_t* s, struct sip_msg* msg) {
	*res = sdp_ip;
	return 0;
}

static int sel_session_ids(str* res, select_t* s, struct sip_msg* msg) {
	if (!global_session_ids)
		return 1;
	res->s = global_session_ids;
	res->len = strlen(res->s);
	return 0;
}

static int sel_switchboard(str* res, select_t* s, struct sip_msg* msg) {
	if (!found_switchboard)
		return 1;
	*res = found_switchboard->name;
	return 0;
}

static int sel_direction(str* res, select_t* s, struct sip_msg* msg) {
	static char buf[2] = {'0', '1'};
	if (!found_direction < 0)
		return 1;
	res->s = buf+found_direction;
	res->len = 1;
	return 0;
}

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT(MODULE_NAME), sel_rtpproxy, SEL_PARAM_EXPECTED},
	{ sel_rtpproxy, SEL_PARAM_STR, STR_STATIC_INIT("sdp_ip"), sel_sdp_ip, 0 },
	{ sel_rtpproxy, SEL_PARAM_STR, STR_STATIC_INIT("session_ids"), sel_session_ids, 0 },
	{ sel_rtpproxy, SEL_PARAM_STR, STR_STATIC_INIT("switchboard"), sel_switchboard, 0 },
	{ sel_rtpproxy, SEL_PARAM_STR, STR_STATIC_INIT("direction"), sel_direction, 0 },

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static int mod_pre_script_cb(struct sip_msg *msg, unsigned int flags, void *param) {
	sdp_ip.s = "";
	sdp_ip.len = 0;
	found_switchboard = NULL;
	found_direction = -1;
	global_session_ids = NULL;
	return 1;
}

static struct {
	int flag;
	struct switchboard_item *si;
	struct xt_rtpproxy_sockopt_in_switchboard in_switchboard;
} parse_config_vals;


int cfg_parse_addr_port(void* param, cfg_parser_t* st, unsigned int flags) {
	struct xt_rtpproxy_sockopt_in_switchboard *sw;
	int i, ret;
	cfg_token_t t;
	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return ret;
	if (ret > 0) return 0;
	if (t.type != '-') return 0;
	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return ret;
	if (ret > 0) return 0;
	if (parse_config_vals.flag == 1) {
		sw = &parse_config_vals.in_switchboard;
	}
	else {
		sw = &parse_config_vals.si->in_switchboard;
	}
	if (t.type == CFG_TOKEN_ALPHA && t.val.len == 1) {
		switch (t.val.s[0]) {
			case 'a':
			case 'b':				
				i = t.val.s[0]-'a';
				ret = cfg_get_token(&t, st, 0);
				if (ret < 0) return ret;
				if (ret > 0) return 0;
				if (t.type != '=') break;

				if (param == NULL) {
					str val;
					char buff[50];
					val.s = buff;
					val.len = sizeof(buff)-1;
					if (cfg_parse_str(&val, st, CFG_STR_STATIC|CFG_EXTENDED_ALPHA) < 0) return -1;
					sw->gate[i].ip = s2ip4(&val);
					if (sw->gate[i].ip == 0) {
						ERR(MODULE_NAME": parse_switchboard_section: bad ip address '%.*s'\n", val.len, val.s);
						return -1;
					}
				}
				else {
					int val;
					if (cfg_parse_int(&val, st, 0) < 0) 
						return -1;
					sw->gate[i].port = val;
				}
				break;
			default:;
		}
	}
	return 0;
}

int cfg_parse_dummy(void* param, cfg_parser_t* st, unsigned int flags) {
	int ret;
	cfg_token_t t;
	str val;
	do {
		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return ret;
		if (ret > 0) return 0;
	}
	while (t.type != '=');
	if (cfg_parse_str(&val, st, CFG_EXTENDED_ALPHA) < 0) return -1;
	return 0;
}

static cfg_option_t section_options[] = {
        {"addr", .f = cfg_parse_addr_port, .flags = CFG_PREFIX|CFG_CASE_SENSITIVE, .param = NULL},
        {"port", .f = cfg_parse_addr_port, .flags = CFG_PREFIX|CFG_CASE_SENSITIVE, .param = (void*) 1},
	{NULL, .flags = CFG_DEFAULT, .f = cfg_parse_dummy},
};

#define DEFAULT_SECTION "default"
#define SWITCHBOARD_PREFIX "switchboard"

static int parse_switchboard_section(void* param, cfg_parser_t* st, unsigned int flags) {
	str name;
	cfg_token_t t;
	int ret, fl;
	parse_config_vals.flag = 0;
	ret = cfg_get_token(&t, st, 0);
	if (ret != 0) return ret;
	if (t.type != CFG_TOKEN_ALPHA) 
		goto skip;
	if (t.val.len == (sizeof(DEFAULT_SECTION)-1) && strncmp(t.val.s, DEFAULT_SECTION, t.val.len) == 0) 
		fl = 1;
	else if (t.val.len == (sizeof(SWITCHBOARD_PREFIX)-1) && strncmp(t.val.s, SWITCHBOARD_PREFIX, t.val.len) == 0) 
		fl = 2;
	else
		goto skip;
	ret = cfg_get_token(&t, st, 0);
	if (ret != 0) return ret;
	if (t.type != ':') 
		goto skip;
	name.s = NULL; name.len = 0;	
	ret = cfg_parse_section(&name, st, CFG_STR_PKGMEM);
	if (ret != 0) return ret;

	if (fl==1 && name.len == (sizeof(SWITCHBOARD_PREFIX)-1) && strncmp(name.s, SWITCHBOARD_PREFIX, name.len) == 0) {
		parse_config_vals.flag = 1;		
		if (name.s) 
			pkg_free(name.s);

	}
	else if (fl == 2) {
		int i;
		if (find_switchboard(&name)) {
			ERR(MODULE_NAME": parse_switchboard_section: name '%.*s' already declared\n", name.len, name.s);
			return -1;
		}
		for (i=0; i<name.len; i++) {
			if (!is_alpha(name.s[i])) {
				ERR(MODULE_NAME": parse_switchboard_section: bad section name '%.*s'\n", name.len, name.s);
				return -1;
			}
		}
		if (name.len > MAX_SWITCHBOARD_NAME_LEN) {
			ERR(MODULE_NAME": parse_switchboard_section: name '%.*s' is too long (%d>%d)\n", name.len, name.s, name.len, MAX_SWITCHBOARD_NAME_LEN);
			return -1;
		}
 	
		parse_config_vals.si = pkg_malloc(sizeof(*parse_config_vals.si));
		if (!parse_config_vals.si) {
			ERR(MODULE_NAME": parse_switchboard_section: not enough pkg memory\n");
			return -1;
		}
		memset(parse_config_vals.si, 0, sizeof(*parse_config_vals.si));
		parse_config_vals.si->name = name;
		parse_config_vals.si->ringing_timeout = 60;
		parse_config_vals.si->next = switchboards;
		switchboards = parse_config_vals.si;

		parse_config_vals.flag = 2;		
	}
	return 0;
skip:
	while (t.type != ']') {
		ret = cfg_get_token(&t, st, 0);
		if (ret != 0) return ret;
	}
	return cfg_eat_eol(st, 0);
}

static int parse_iptrtpproxy_cfg() {
	cfg_parser_t* parser = NULL;
	struct switchboard_item *si;
	if ((parser = cfg_parser_init(0, &iptrtpproxy_cfg_filename)) == NULL) {
		ERR(MODULE_NAME"parse_iptrtpproxy_cfg: Error while initializing configuration file parser.\n");
		return -1;
        }
	cfg_section_parser(parser, parse_switchboard_section, NULL);
	cfg_set_options(parser, section_options);
	memset(&parse_config_vals, 0, sizeof(parse_config_vals));
	if (sr_cfg_parse(parser)) {
		return -1;
	}
	cfg_parser_close(parser);
	
	for (si = switchboards; si; si = si->next) {
		int i;
		for (i=0; i<2; i++) {
			if (!si->in_switchboard.gate[i].ip) 
				si->in_switchboard.gate[i].ip = parse_config_vals.in_switchboard.gate[i].ip;
			if (!si->in_switchboard.gate[i].port) 
				si->in_switchboard.gate[i].port = parse_config_vals.in_switchboard.gate[i].port;
		}
		for (i=0; i<2; i++) {
			if (!si->in_switchboard.gate[i^1].ip)
				si->in_switchboard.gate[i^1].ip = si->in_switchboard.gate[i].ip;
			if (!si->in_switchboard.gate[i^1].port)
				si->in_switchboard.gate[i^1].port = si->in_switchboard.gate[i].port;
		}
	}
	return 0;
}

/* module initialization */
static int mod_init(void) {
	struct switchboard_item *si;
	int i;
	if (iptrtpproxy_cfg_flag == 0) {
		if (parse_iptrtpproxy_cfg() < 0)
			return E_CFG;
	}

	for (si = switchboards; si; si=si->next) {
		str ips[2];
		char buf[17];
		ip42s(si->in_switchboard.gate[0].ip, ips+0);
		strncpy(buf, ips[0].s, sizeof(buf)-1);
		ips[0].s = buf;
		ip42s(si->in_switchboard.gate[1].ip, ips+1);

		DBG(MODULE_NAME": mod_init: name=%.*s;addr-a=%.*s;port-a=%d;addr-b=%.*s;port-b=%d;learning-timeout-a=%d;learning-timeout-b=%d;always-learn-a=%d;always-learn-b=%d;ringing-timeout=%d\n", 
			STR_FMT(&si->name),
			STR_FMT(ips+0),
			si->in_switchboard.gate[0].port,
			STR_FMT(ips+1),
			si->in_switchboard.gate[1].port,
			si->in_session.source[0].stream[0].learning_timeout,
			si->in_session.source[1].stream[0].learning_timeout,
			si->in_session.source[0].always_learn,
			si->in_session.source[1].always_learn,
			si->ringing_timeout
		);
	}
	if (xt_RTPPROXY_open(&handle) < 0) goto err;
	for (si = switchboards; si; si=si->next) {
		struct xt_rtpproxy_switchboard *out_switchboard;
		if (xt_RTPPROXY_get_switchboards(&handle, &si->in_switchboard, NULL, XT_RTPPROXY_SOCKOPT_FLAG_OUT_SWITCHBOARD, &out_switchboard) < 0) {
			goto err;
		}
		/* update switchboard info, we need real ports for rtpproxy_update, it may sometimes differ from in_switchboard when addr-a=addr-b. We'll take first switchboard returned, should be always only one */
		if (!out_switchboard) {
			ERR(MODULE_NAME": switchboard '%.*s' not found in iptables\n", si->name.len, si->name.s);
			goto err2;
		}
		if (si->in_switchboard.gate[0].ip == si->in_switchboard.gate[1].ip) {
			for (i=0; i<XT_RTPPROXY_MAX_GATE; i++) {
				si->in_switchboard.gate[i].port = out_switchboard->so.gate[i].addr.port;
			}
		}
		xt_RTPPROXY_release_switchboards(&handle, out_switchboard);
	}

	register_script_cb(mod_pre_script_cb, REQUEST_CB | ONREPLY_CB | PRE_SCRIPT_CB, 0);
	register_select_table(sel_declaration);
	return 0;
err:
	ERR(MODULE_NAME": %s (%d)\n", handle.err_str, handle.err_no);
err2:
	if (handle.sockfd >= 0) {
		xt_RTPPROXY_close(&handle);
	}
	return -1;
}

static void mod_cleanup(void) {
	if (handle.sockfd >= 0) {
		xt_RTPPROXY_close(&handle);
	}
}

static int child_init(int rank) {

	return 0;
}


#define eat_spaces(_p) \
	while( *(_p)==' ' || *(_p)=='\t' ){\
	(_p)++;}


static int declare_config(modparam_t type, void* val) {
	if (!val) return 0;
	if (iptrtpproxy_cfg_flag == 0) {

		iptrtpproxy_cfg_flag = 1;
		iptrtpproxy_cfg_filename = * (str*) val;
		if (parse_iptrtpproxy_cfg() == 0)
			return 0;
	}
	else {
		switch (iptrtpproxy_cfg_flag) {
			case 1:
				ERR(MODULE_NAME": declare_config: config param may be used only once\n");
				break;
			case 2:
				ERR(MODULE_NAME": declare_config: config param may not be used after 'switchboard'\n");
				break;
			default:
				BUG(MODULE_NAME": declare_config: unexpected 'iptrtpproxy_cfg_filename' value %d\n", iptrtpproxy_cfg_flag);
		}				
	}
	return E_CFG;
}

static int declare_switchboard_param(modparam_t type, void* val) {

	char *s, *c;
	int i, all_flag;
	struct switchboard_item *si = NULL;
	enum param_id {
		par_GateB =		8,
		par_Name =		0x000001,
		par_RingingTimeout =	0x000002,
		par_AlwaysLearn =	0x000400,
		par_LearningTimeout =	0x000800
	};
	#define IS_GATE_B(id) ((id &	0xFF0000)!=0)
	static struct {
		char *name;
		unsigned int id;
	} params[] = {
		{.name = "name", .id = par_Name},
		{.name = "always-learn-a", .id = par_AlwaysLearn},
		{.name = "always-learn-b", .id = par_AlwaysLearn << par_GateB},
		{.name = "learning-timeout-a", .id = par_LearningTimeout},
		{.name = "learning-timeout-b", .id = par_LearningTimeout << par_GateB},
		{.name = "ringing-timeout", .id = par_RingingTimeout},

		{.name = 0, .id = 0}
	};

	if (!val) return 0;
	if (iptrtpproxy_cfg_flag == 0) {
		iptrtpproxy_cfg_flag = 2;
		if (parse_iptrtpproxy_cfg() < 0)
			return E_CFG;
	}

	s = val;
	all_flag = -1;

	eat_spaces(s);
	if (!*s) return 0;
	/* parse param: name=;addr-a=;addr-b=;port-a=;port-b=; */
	while (*s) {
		str p, val;
		unsigned int id;

		c = s;
		while ( is_alpha(*c) ) {
			c++;
		}
		if (c == s) {
			ERR(MODULE_NAME": declare_switchboard_param: param name expected near '%s'\n", s);
			goto err_E_CFG;
		}
		p.s = s;
		p.len = c-s;
		eat_spaces(c);
		s = c;
		if (*c != '=') {
			ERR(MODULE_NAME": declare_switchboard_param: equal char expected near '%s'\n", s);
			goto err_E_CFG;
		}
		c++;
		eat_spaces(c);
		s = c;
		while (*c && *c != ';') c++;
		val.s = s;
		val.len = c-s;
		while (val.len > 0 && val.s[val.len-1]<=' ') val.len--;
		if (*c) c++;
		eat_spaces(c);

		id = 0;
		for (i=0; params[i].id; i++) {
			if (strlen(params[i].name)==p.len && strncasecmp(params[i].name, p.s, p.len) == 0) {
				id = params[i].id;
				break;
			}
		}
		if (!id) {
			ERR(MODULE_NAME": declare_switchboard_param: unknown param name '%.*s'\n", p.len, p.s);
			goto err_E_CFG;
		}
		if (all_flag >= 0 && id == par_Name) {
			ERR(MODULE_NAME": declare_switchboard_param: name must be the first param\n");
			goto err_E_CFG;
		}
		if (id == par_Name) {
			all_flag = 0;
			si = find_switchboard(&val);
			if (!si) {
				if (val.len == 1 && val.s[0] == '*')
					all_flag = 1;
				else {
					ERR(MODULE_NAME": declare_switchboard_param: switchboard '%.*s' not found\n", val.len, val.s);
					goto err_E_CFG;
				}
			}
		}
		else {
			if (all_flag)
				si = switchboards;
			while (si) {

				switch (id) {
					case par_Name:
						break;
					case par_AlwaysLearn:
					case par_AlwaysLearn << par_GateB: {
						unsigned int u;
						if (str2int(&val, &u) < 0) {
							goto err_E_CFG;
						}
						si->in_session.source[IS_GATE_B(id)].always_learn = u != 0;
						break;
					}
					case par_LearningTimeout:
					case par_LearningTimeout << par_GateB: {
						unsigned int u;
						if (str2int(&val, &u) < 0) {
							goto err_E_CFG;
						}
						if (u) {
							for (i=0; i<2; i++) {
								si->in_session.source[IS_GATE_B(id)].stream[i].learning_timeout = u;
								si->in_session.source[IS_GATE_B(id)].stream[i].flags = XT_RTPPROXY_SOCKOPT_FLAG_SESSION_LEARNING_TIMEOUT;
							}
						}
						break;
					}
					case par_RingingTimeout: {
						unsigned int u;
						if (str2int(&val, &u) < 0) {
							goto err_E_CFG;
						}
						if (u) {
							si->ringing_timeout = u;
						}
						break;
					}
					default:
						BUG(MODULE_NAME": declare_switchboard_param: unknown id '%x\n", id);
						goto err_E_CFG;
				}
				si->param_ids |= id;
				if (!all_flag) break;
				si = si->next;
			}
		}
		s = c;
	}
	if (all_flag) {
		return 0;
	}

#define DEF_PARAMS(_id,_s,_fld) \
	if ( (si->param_ids & (_id)) && !(si->param_ids & ((_id) << par_GateB)) )  \
		si->_s[1]._fld = si->_s[0]._fld; \
	if ( !(si->param_ids & (_id)) && (si->param_ids & ((_id) << par_GateB)) )  \
		si->_s[0]._fld = si->_s[1]._fld;

	DEF_PARAMS(par_AlwaysLearn,in_session.source,always_learn);
	for (i=0; i<2; i++) {
		DEF_PARAMS(par_LearningTimeout,in_session.source,stream[i].learning_timeout);
		DEF_PARAMS(par_LearningTimeout,in_session.source,stream[i].flags);
	}
	switchboard_count++;

	return 0;

err_E_CFG:
	ERR(MODULE_NAME": declare_switchboard_param(#%d): parse error near \"%s\"\n", switchboard_count, s);

	return E_CFG;
}

static cmd_export_t cmds[] = {
	{MODULE_NAME "_alloc",     rtpproxy_alloc,         2, rtpproxy_alloc_fixup,       REQUEST_ROUTE | ONREPLY_ROUTE },
	{MODULE_NAME "_update",    rtpproxy_update,        2, rtpproxy_update_fixup,      REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{MODULE_NAME "_adjust_timeout", rtpproxy_adjust_timeout, 2, rtpproxy_update_fixup,      REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{MODULE_NAME "_delete",    rtpproxy_delete,        1, rtpproxy_delete_fixup,      REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{MODULE_NAME "_find",      rtpproxy_find,          2, rtpproxy_find_fixup,        REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },

	{0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"config",                PARAM_STR | PARAM_USE_FUNC, &declare_config}, 
	{"switchboard",           PARAM_STRING | PARAM_USE_FUNC, &declare_switchboard_param},
	{0, 0, 0}
};

struct module_exports exports = {
	MODULE_NAME,
	cmds,
	0,       /* RPC methods */
	params,
	mod_init,
	0, /* reply processing */
	mod_cleanup, /* destroy function */
	0, /* on_break */
	child_init
};


#if !defined(NO_SHARED_LIBS) || NO_SHARED_LIBS==0
/* make compiler happy and give it missing symbols */
#include <iptables.h>
#include <stdarg.h>
void xtables_register_target(struct xtables_target *me) {
}

void exit_error(enum exittype status, const char *msg, ...)
{
	va_list args;
	
	va_start(args, msg);
//	ERR(msg/*, args*/);  /* TODO: how to pass ... to macro? */
	ERR(MODULE_NAME": %s", msg);
	va_end(args);
}

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

int check_inverse(const char option[], int *invert, int *optind, int argc)
{
	if (option && strcmp(option, "!") == 0) {
		if (*invert)
			exit_error(PARAMETER_PROBLEM, "Multiple `!' flags not allowed");
		*invert = TRUE;
		if (optind) {
			*optind = *optind+1;
			if (argc && *optind > argc)
				exit_error(PARAMETER_PROBLEM, "no argument following `!'");
		}
		return TRUE;
	}
	return FALSE;
}

#endif


