/* $Id: iptrtpproxy.c 30494 2010-07-20 15:05:24Z tma $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// #include <linux/compiler.h>   will be needed to define __user macro
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../atomic_ops.h"
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
#define MAX_MEDIA_NUMBER XT_RTPPROXY_MAX_ALLOC_SESSION
#define MAX_CODEC_NUMBER MAX_MEDIA_NUMBER*5
#define MAX_SWITCHBOARD_NAME_LEN 20
#define MAX_AGGREGATED_NUMBER 30

/* warningless cast at 64-bit */
#define PTR2INT(v) (int)(long) (v)
#define INT2PTR(v) (void*)(long) (v)

struct host_item_stat {
	int last_error_stamp;
	int last_ok_stamp;
};

struct host_item {
	str name;
	struct xt_rtpproxy_connection_rpc_params rpc_params;
	int local;
	struct xt_rtpproxy_handle handle;
	int handle_is_opened;

	struct host_item_stat *stat;
	struct host_item *next;
};

struct switchboard_item_stat {
	atomic_t free;
	atomic_t alloc;
};

struct switchboard_item {
	str name;
	struct xt_rtpproxy_switchboard_id switchboard_addr;
	unsigned int sip_ip;
	str hostname;
	struct host_item *host;
	unsigned int weight;

	struct switchboard_item_stat *stat;

	struct switchboard_item *next;
};

enum sdp_media_type {
	sdpmtUnknown = 0, 
	sdpmtAudio, 
	sdpmtVideo, 
	sdpmtApplication, 
	sdpmtText, 
	sdpmtMessage,
	sdpmtData,  /* not recommended in RFC4566 */
	sdpmtControl   /* dtto */
};

#define NUM_MEDIA_TYPES (sdpmtControl+1)
static str sdp_media_types_str[NUM_MEDIA_TYPES] = {
	STR_STATIC_INIT("unknown"),
	STR_STATIC_INIT("audio"),
	STR_STATIC_INIT("video"),
	STR_STATIC_INIT("application"),
	STR_STATIC_INIT("text"),
	STR_STATIC_INIT("message"),
	STR_STATIC_INIT("data"),
	STR_STATIC_INIT("control")
};

struct aggregation_item {
	str name;
	unsigned int switchboard_count;
	unsigned int sip_ip;
	struct switchboard_item *(*switchboards)[];
	struct aggregation_item *next;
};

struct codec_set_item_throttle {
	int max_streams;
	struct xt_rtpproxy_throttle_stat bandwidth[2];   /* RTP, RTCP */
};

struct codec_set_item {
	str name;
	struct {
		struct codec_set_item_throttle throttle;
		unsigned int (*codec_rights)[];
	} media_types[NUM_MEDIA_TYPES];
	struct codec_set_item *next;
};

struct ipt_session {

	unsigned int session_count;
	struct xt_rtpproxy_sockopt_session sessions[MAX_MEDIA_NUMBER];
	unsigned int sdp_media_count;
	int sdp_media[MAX_MEDIA_NUMBER];

	struct switchboard_item *switchboard;
};

static struct {
	str session_ids;
	str sdp_ip;
	str oline_user;
	str oline_addr;
	struct switchboard_item *switchboard[2];
	struct aggregation_item *aggregation[2];
	int learning_timeout;
	int expiration_timeout;
	int ttl;
	int always_learn;
	struct {
		u_int32_t mark;
		struct xt_rtpproxy_throttle_stat bandwidth[2];
	} throttle;
	struct codec_set_item *codec_set;
	int remove_codec_mask;
	unsigned int auth_rights;
	struct ipt_session protected_sess;
} global_params;

static struct switchboard_item *switchboards = NULL;
static struct host_item *hosts = NULL;
static struct aggregation_item *aggregations = NULL;
static struct codec_set_item *codec_sets = NULL;
static int switchboard_count = 0;
static str iptrtpproxy_cfg_filename = STR_STATIC_INIT("/etc/iptrtpproxy.cfg");
static int iptrtpproxy_cfg_flag = 0;
static char* iptrtpproxy_cfg_hostname = NULL;
static int rpc_heartbeat_timeout = 30;
static int sdp_parsed = 999;   /* we need share parsed SDP between authorize_media & alloc/update * get_param */
static struct sdp_session global_sdp_sess;

#define declare_find_function(x) \
static struct x##_item* find_##x(str *name, struct x##_item*** prev) { \
	struct x##_item* p;\
	struct x##_item** dummy;\
	if (!prev) \
		prev = &dummy;\
	for (p = x##s, *prev = &x##s; p; *prev = &(**prev)->next, p=p->next) {\
		int len, n;\
		len = (name->len < p->name.len) ? name->len : p->name.len;\
		n = strncasecmp(name->s, p->name.s, len);\
		if (n == 0) {\
			if (name->len == p->name.len) \
				return p;\
			else if (name->len < p->name.len)\
				return NULL;\
		}\
		else if (n < 0) { \
			return NULL;\
		}\
	}\
	return NULL;\
}

declare_find_function(host)
declare_find_function(switchboard)
declare_find_function(aggregation)
declare_find_function(codec_set)

static struct switchboard_item* find_switchboard_by_addr(struct xt_rtpproxy_switchboard_id *addr) {
	struct switchboard_item* p;
	for (p = switchboards; p; p=p->next) {
		if (addr->ip == p->switchboard_addr.ip && addr->port == p->switchboard_addr.port) break;
	}
	return p;
}

enum {
	PAR_EXPIRATION_TIMEOUT, 
	PAR_TTL, 
	PAR_LEARNING_TIMEOUT, 
	PAR_ALWAYS_LEARN,
	PAR_AGGREGATION_A, PAR_AGGREGATION_B, 
	PAR_SWITCHBOARD_A, PAR_SWITCHBOARD_B,
	PAR_AGGREGATION_BY_SIP_IP_A, PAR_AGGREGATION_BY_SIP_IP_B,
	PAR_SWITCHBOARD_BY_SIP_IP_A, PAR_SWITCHBOARD_BY_SIP_IP_B,
	PAR_SESSION_IDS, PAR_PROTECTED_SESSION_IDS,
	PAR_SDP_IP, PAR_ACTIVE_MEDIA_NUM,
	PAR_OLINE_USER, PAR_OLINE_ADDR,
	PAR_THROTTLE_MARK, 
	PAR_THROTTLE_RTP_MAX_BYTES, PAR_THROTTLE_RTP_MAX_PACKETS, 
	PAR_THROTTLE_RTCP_MAX_BYTES, PAR_THROTTLE_RTCP_MAX_PACKETS,
	PAR_CODEC_SET, PAR_AUTH_RIGHTS, PAR_REMOVE_CODEC_MASK
};

enum {
	PAR_READ=0x01, PAR_WRITE=0x02, PAR_INT=0x04, PAR_STR=0x08, PAR_DIR=0x10
};

static struct {
	char *name;
	int  id;
	int flags;
} param_list[] = { 
	{"expiration_timeout", PAR_EXPIRATION_TIMEOUT, PAR_READ|PAR_WRITE|PAR_INT},
	{"ttl", PAR_TTL, PAR_READ|PAR_WRITE|PAR_INT},
	{"learning_timeout", PAR_LEARNING_TIMEOUT, PAR_READ|PAR_WRITE|PAR_INT},
	{"always_learn", PAR_ALWAYS_LEARN, PAR_READ|PAR_WRITE|PAR_INT},
	{"aggregation_a", PAR_AGGREGATION_A, PAR_READ|PAR_WRITE|PAR_STR},
	{"aggregation_b", PAR_AGGREGATION_B, PAR_READ|PAR_WRITE|PAR_STR|PAR_DIR},
	{"switchboard_a", PAR_SWITCHBOARD_A, PAR_READ|PAR_WRITE|PAR_STR},
	{"switchboard_b", PAR_SWITCHBOARD_B, PAR_READ|PAR_WRITE|PAR_STR|PAR_DIR},
	{"aggregation_by_sip_ip_a", PAR_AGGREGATION_BY_SIP_IP_A, PAR_WRITE|PAR_STR},
	{"aggregation_by_sip_ip_b", PAR_AGGREGATION_BY_SIP_IP_B, PAR_WRITE|PAR_STR|PAR_DIR},
	{"switchboard_by_sip_ip_a", PAR_SWITCHBOARD_BY_SIP_IP_A, PAR_WRITE|PAR_STR},
	{"switchboard_by_sip_ip_b", PAR_SWITCHBOARD_BY_SIP_IP_B, PAR_WRITE|PAR_STR|PAR_DIR},
	{"session_ids", PAR_SESSION_IDS, PAR_READ|PAR_STR},
	{"protected_session_ids", PAR_PROTECTED_SESSION_IDS, PAR_WRITE|PAR_STR},
	{"sdp_ip", PAR_SDP_IP, PAR_READ|PAR_INT},
	{"active_media_num", PAR_ACTIVE_MEDIA_NUM, PAR_READ|PAR_INT},
	{"o_user", PAR_OLINE_USER, PAR_READ|PAR_WRITE|PAR_STR},
	{"o_addr", PAR_OLINE_ADDR, PAR_READ|PAR_WRITE|PAR_STR},
	{"throttle_mark", PAR_THROTTLE_MARK, PAR_READ|PAR_WRITE|PAR_INT},
	{"throttle_rtp_max_bytes", PAR_THROTTLE_RTP_MAX_BYTES, PAR_READ|PAR_WRITE|PAR_INT},
	{"throttle_rtp_max_packets", PAR_THROTTLE_RTP_MAX_PACKETS, PAR_READ|PAR_WRITE|PAR_INT},
	{"throttle_rtcp_max_bytes", PAR_THROTTLE_RTCP_MAX_BYTES, PAR_READ|PAR_WRITE|PAR_INT},
	{"throttle_rtcp_max_packets", PAR_THROTTLE_RTCP_MAX_PACKETS, PAR_READ|PAR_WRITE|PAR_INT},
	{"codec_set", PAR_CODEC_SET, PAR_READ|PAR_WRITE|PAR_STR},
	{"remove_codec_mask", PAR_REMOVE_CODEC_MASK, PAR_READ|PAR_WRITE|PAR_INT},
	{"auth_rights", PAR_AUTH_RIGHTS, PAR_READ|PAR_INT},
	{ NULL }
};

static int param2idx(str *name, int rw) {
	int i;
	for (i=0; param_list[i].name; i++) {
		if (strlen(param_list[i].name)==name->len && 
		    strncasecmp(param_list[i].name, name->s, name->len) == 0 && 
		    (rw & param_list[i].flags)) {
			return i;
		}
	}
	ERR(MODULE_NAME": param2idx: unknown param '%.*s', rw:0x%0x\n", STR_FMT(name), rw);
	return -1;
}

/** if succesfull allocated sessions available @rtpproxy.session_ids
 */

static int rtpproxy_alloc_update_fixup(void** param, int param_no) {
	switch (param_no) {
		case 1:
			return fixup_var_int_12(param, param_no);
		case 2:
			return fixup_var_str_12(param, param_no);
		default:
			return 0;
	}
}

static int rtpproxy_delete_fixup(void** param, int param_no) {
	switch (param_no) {
		case 1:
			return fixup_var_str_12(param, param_no);
		default:
			return 0;
	}
}

static int rtpproxy_set_param_fixup(void** param, int param_no) {
	int idx;
	action_u_t *a;
	str s;
	switch (param_no) {
		case 1:
			s.s = (char*)*param;
			s.len = strlen(s.s);
			idx = param2idx(&s, PAR_WRITE);
			if (idx < 0) {
				return E_CFG;
			}
			*param = INT2PTR(idx);
			break;
		case 2:
			a = fixup_get_param(param, param_no, 1);
			idx = a->u.number;
			if (param_list[idx].flags & PAR_STR) {
				return fixup_var_str_12(param, param_no);

			} else if (param_list[idx].flags & PAR_INT) {
				return fixup_var_int_12(param, param_no);
			}
			break;
	}
	return 0;
}

static int name2media_type(str *name) {
	int i;
	for (i = 1; i<NUM_MEDIA_TYPES; i++) {
		if (name->len == sdp_media_types_str[i].len &&
			strncasecmp(name->s, sdp_media_types_str[i].s, name->len) == 0) {
				return i;
		}
	}
	return sdpmtUnknown;
}

struct codec_entry {
	str name;
	/* bandwidth */
	int payload_type;  /* -1 .. dynamic */
};

#define MAX_FIXED_PAYLOAD_TYPES 96

static struct codec_entry(*reg_codecs)[] = NULL;
static int reg_codec_count = 0;
static int reg_codec_alloc_count = 0;
static struct {
	int codec_id;
} fixed_payload_types[MAX_FIXED_PAYLOAD_TYPES];


/* unregistered codec .. 0 */
static int name2codec_id(str *name, int *new_codec_id) {
	int i, j;
	i = 0;
	j = reg_codec_count - 1;
	while (i <= j) {	
		int k, r;
		k = (i + j)/2;
		r = strncasecmp((*reg_codecs)[k].name.s, name->s, ((*reg_codecs)[k].name.len < name->len)?(*reg_codecs)[k].name.len:name->len);
		if (r == 0 && (*reg_codecs)[k].name.len == name->len) {
			return k+1;
		} else if (r > 0 || (r == 0 && (*reg_codecs)[k].name.len > name->len)) {
			j = k - 1;
		}
		else {
			i = k + 1;
		}
	}
	if (new_codec_id) {
		*new_codec_id = i + 1;
	}
	return 0;
}

/* return <0 if error, otherwise codec_id */
static int register_codec(str *name) {
	int codec_id, new_codec_id = 0;
	if (!(codec_id = name2codec_id(name, &new_codec_id))) {
		int i;
		if (reg_codec_count + 1 > reg_codec_alloc_count) {
			void *p;
			reg_codec_alloc_count += 10;
			p = pkg_realloc(reg_codecs, sizeof((*reg_codecs)[0])*reg_codec_alloc_count);
			if (!p) {
				return E_OUT_OF_MEM;
			}
			reg_codecs = p;
		}

		/* do not count codec_id == 0 (unknown) */
		for (i=reg_codec_count-1; i >= new_codec_id-1; i--) {
			(*reg_codecs)[i+1] = (*reg_codecs)[i];
		}
		reg_codec_count++;
		(*reg_codecs)[new_codec_id-1].name = *name; 
		codec_id = new_codec_id;

	}
	return codec_id;
}

enum send_rec_modifier {
	sdpaattr_sendonly = 1,
	sdpaattr_recvonly = 2,
	sdpaattr_sendrecv = 3,
	sdpaattr_inactive = 4
};

static str send_rec_modifiers[] = {
	STR_STATIC_INIT(""),
	STR_STATIC_INIT("sendonly"),
	STR_STATIC_INIT("recvonly"),
	STR_STATIC_INIT("sendrecv"),
	STR_STATIC_INIT("inactive"),
};

struct sdp_codec {
	unsigned int payload_type;
	unsigned int codec_id;
	str mline_payload_type_s;
	str a_rtpmap_line_s;
	str a_fmtp_line_s;
};

struct sdp_session {
	str oline_user_s;
	str oline_addr_s;
	unsigned int media_count;
	struct {
		int active;  /* if SDP has been parsed correctly, has a IP (even 0.0.0.0), port!=0 and has supported params */
		unsigned short port;
		unsigned int ip;
		str ip_s;
		str port_s;
		enum sdp_media_type media_type;
		enum send_rec_modifier send_rec_modifier;
		str send_rec_modifier_line_s;
		int codec_count;
		struct sdp_codec (*codecs)[];
	} media[MAX_MEDIA_NUMBER];
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

inline static int next_sdp_line(char** p, char* pend, char *ltype, str* lvalue, str* line) {
	char *cp;
	while (*p < pend) {
		while (*p < pend && (**p == '\n' || **p == '\r')) (*p)++;
		for (cp = *p; cp < pend && *cp != '\n' && *cp != '\r'; cp++);

		if (cp-*p > 2 && (*p)[1] == '=') {
			*ltype = **p;
			lvalue->s = (*p)+2;
			lvalue->len = cp-lvalue->s;
			while (cp < pend && (*cp == '\n' || *cp == '\r')) cp++;
			line->s = (*p);
			line->len = cp-line->s;
			*p = cp;
			return 0;
		}
		*p = cp;
	}
	return -1;
};

static int name2enum(str *name, str (*list)[]) {
	int i;
	for (i = 0; (*list)[i].s != NULL; i++) {
		if (name->len == (*list)[i].len &&
			strncasecmp(name->s, (*list)[i].s, name->len) == 0) {
			return i;
		}
	}
	return -1;
}

static int prefix2enum(str *line, str (*list)[]) {
	int i;
	for (i = 0; (*list)[i].s != NULL; i++) {
		if (line->len > (*list)[i].len &&
			strncmp(line->s, (*list)[i].s, (*list)[i].len) == 0) {
			return i;
		}
	}
	return -1;
}

/* SDP RFC2327 */
static int parse_sdp_content(struct sip_msg* msg, struct sdp_session *sess) {
	char *p, *pend, *cp, *cp2, *lend;
	str line, lvalue, cline_ip_s, body;
	int sess_fl, i, cline_count, codec_count;
	char ltype, savec;
	unsigned int cline_ip;
	enum send_rec_modifier sess_send_rec_modifier;

	static struct sdp_codec codecs[MAX_CODEC_NUMBER];

	static str supported_protocols[] = {
		STR_STATIC_INIT("rtp/avp"),
		STR_STATIC_INIT("rtp/savp"),
		STR_STATIC_INIT("rtp/avpf"),
		STR_STATIC_INIT("rtp/savpf"),
		STR_STATIC_INIT("udp"),
		STR_STATIC_INIT("udptl"),
		STR_NULL
	};

	enum a_attr {sdpaattr_rtpmap, sdpaattr_fmtp, sdpaattr_rtcp};
	static str a_attrs[] = {
		STR_STATIC_INIT("rtpmap:"),
		STR_STATIC_INIT("fmtp:"),
		STR_STATIC_INIT("rtcp:"),
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
			ERR(MODULE_NAME": parse_sdp_content: bad content type '%.*s'\n", STR_FMT(&line));
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
	codec_count = 0;
	memset(&codecs, 0, sizeof(codecs));
	sess_send_rec_modifier = 0;
	while (p < pend) {
		if (next_sdp_line(&p, pend, &ltype, &lvalue, &line) < 0) break;
		lend = lvalue.s + lvalue.len;
		switch (ltype) {
			case 'v':
				/* Protocol Version: v=0 */
				if (sess_fl != 0) {
					ERR(MODULE_NAME": parse_sdp_content: only one session allowed\n");  /* RFC3264 */
					return -1;
				}
				sess_fl = 1;
				break;
			case 'o': 
				/* originator & session description: o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address> */
				if (sess_fl != 1) {
					ERR(MODULE_NAME": parse_sdp_content: o= line is not in session section\n"); 
					return -1;
				}
				for (i=0; i<6; i++) 
				if (sess->oline_addr_s.s) {
					ERR(MODULE_NAME": parse_sdp_content: only one o= line allowed\n"); 
					return -1;
				}
				cp = eat_token_end(lvalue.s, lend);
				sess->oline_user_s.len = cp-lvalue.s;
				if (!sess->oline_user_s.len) goto invalid_o;
				sess->oline_user_s.s = lvalue.s;
				lvalue.s = eat_space_end(cp, lend);
				for (i=0; i<4; i++) {
						cp = eat_token_end(lvalue.s, lend);
						if (cp-lvalue.s == 0) goto invalid_o;
						lvalue.s = eat_space_end(cp, lend);
				}
				cp = eat_token_end(lvalue.s, lend);
				sess->oline_addr_s.len = cp-lvalue.s;
				if (!sess->oline_addr_s.len) goto invalid_o;
				sess->oline_addr_s.s = lvalue.s;
				break;
			invalid_o:
				ERR(MODULE_NAME": parse_sdp_content: invalid o= line '%.*s'\n", (int) (lend-line.s), line.s);
				return -1;
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
						cp = eat_token_end(lvalue.s, lend);
						if (cp-lvalue.s != 2 || memcmp(lvalue.s, "IN", 2) != 0) {
							goto invalidate;
						}
						cp = eat_space_end(cp, lend);
						lvalue.s = cp;
						cp = eat_token_end(cp, lend);
						if (cp-lvalue.s != 3 || memcmp(lvalue.s, "IP4", 3) != 0) {
							goto invalidate;
						}
						cp = eat_space_end(cp, lend);
						lvalue.s = cp;
						cp = eat_token_end(cp, lend);
						lvalue.len = cp-lvalue.s;
						if (lvalue.len == 0 || q_memchr(lvalue.s, '/', lvalue.len)) {
							/* multicast address not supported */
							goto invalidate;
						}
						if (sess_fl == 1) {
							cline_ip_s = lvalue;
							cline_ip = s2ip4(&lvalue);
						}
						else {
							sess->media[sess->media_count-1].ip = s2ip4(&lvalue);
							sess->media[sess->media_count-1].active = sess->media[sess->media_count-1].port != 0;  /* IP may by specified by hostname */
							sess->media[sess->media_count-1].ip_s = lvalue;
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
				/* media: "audio", "video", "application", "data" and "control" */
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
						sess->media[sess->media_count-1].port = 0;
						sess->media[sess->media_count-1].send_rec_modifier = sess_send_rec_modifier;
						cp = eat_token_end(lvalue.s, lend);
						lvalue.len = cp-lvalue.s;
						sess->media[sess->media_count-1].media_type = name2media_type(&lvalue);;
						if (!lvalue.len) {
							break;
						}
						cp = eat_space_end(cp, lend);
						lvalue.s = cp;
						cp = eat_token_end(cp, lend);
						lvalue.len = cp-lvalue.s;
						
						cp2 = q_memchr(lvalue.s, '/', lvalue.len);
						if (cp2) {
							/* strip optional number of ports, if present should be 2 */
							lvalue.len = cp2-lvalue.s;
						}
						sess->media[sess->media_count-1].port_s = lvalue;
						if (lvalue.len == 0) { /* invalid port? */
							break;
						}
						savec = lvalue.s[lvalue.len];
						lvalue.s[lvalue.len] = '\0';
						sess->media[sess->media_count-1].port = atol(lvalue.s);
						lvalue.s[lvalue.len] = savec;
						if (sess->media[sess->media_count-1].port == 0) {
							break;
						}
						cp = eat_space_end(cp, lend);
						
						lvalue.s = cp;
						cp = eat_token_end(cp, lend);
						lvalue.len = cp-lvalue.s;
						if (name2enum(&lvalue, &supported_protocols) >= 0) {
							sess->media[sess->media_count-1].active = cline_ip_s.len != 0;  /* IP may by specified by hostname */
							sess->media[sess->media_count-1].ip_s = cline_ip_s;
							sess->media[sess->media_count-1].ip = cline_ip;
						}
						/* get payload types */
						sess->media[sess->media_count-1].codecs = (struct sdp_codec (*)[]) (codecs + codec_count);
						while (cp < lend) {
							if (codec_count >= MAX_CODEC_NUMBER) {
								ERR(MODULE_NAME": parse_sdp_content: max.number of codecs (%d) exceeded\n", MAX_CODEC_NUMBER);
								return -1;
							}
							codecs[codec_count].mline_payload_type_s.s = cp;
							cp = eat_space_end(cp, lend);
							lvalue.s = cp;
							cp = eat_token_end(cp, lend);
							codecs[codec_count].mline_payload_type_s.len = cp - codecs[codec_count].mline_payload_type_s.s;
							lvalue.len = cp-lvalue.s;
							savec = lvalue.s[lvalue.len];
							lvalue.s[lvalue.len] = '\0';
							codecs[codec_count].payload_type = atol(lvalue.s);
							if (codecs[codec_count].payload_type < MAX_FIXED_PAYLOAD_TYPES) {
								codecs[codec_count].codec_id = fixed_payload_types[codecs[codec_count].payload_type].codec_id;
							}
							lvalue.s[lvalue.len] = savec;
							for (i=0; i < sess->media[sess->media_count-1].codec_count; i++) {
								if (codecs[codec_count].payload_type == (*sess->media[sess->media_count-1].codecs)[i].payload_type) {
									ERR(MODULE_NAME": parse_sdp_content: duplicate payload type in '%.*s'\n", (int) (lend-line.s), line.s);
									return -1;
								}
							}
							codec_count++;
							sess->media[sess->media_count-1].codec_count++;							
						}
						if (!sess->media[sess->media_count-1].codec_count) {
							ERR(MODULE_NAME": parse_sdp_content: no codec declared '%.*s'\n", (int) (lend-line.s), line.s);
							return -1;
						}
						break;
					default:
						;
				}

				break;
			case 'a':
				i = name2enum(&lvalue, &send_rec_modifiers);
				if (i > 0) {
					switch (sess_fl) {
						case 1:
							if (sess_send_rec_modifier) {
								ERR(MODULE_NAME": parse_sdp_content: duplicate send/recv modifier in session '%.*s'\n", (int) (lend-line.s), line.s);
								return -1;
							}
							sess_send_rec_modifier = i;
							break;
						case 2:
							if (sess->media[sess->media_count-1].send_rec_modifier_line_s.s) {
								ERR(MODULE_NAME": parse_sdp_content: duplicate send/recv modifier in stream '%.*s'\n", (int) (lend-line.s), line.s);
								return -1;
							}
							sess->media[sess->media_count-1].send_rec_modifier = i;
							sess->media[sess->media_count-1].send_rec_modifier_line_s = line;
						default:
							;
					}
				}
				else if (sess_fl == 2) {
					int payload_type;
					int a_attr;
					a_attr = prefix2enum(&lvalue, &a_attrs);
					if (a_attr < 0) {
						break;
					}
					lend = lvalue.s + lvalue.len;
					lvalue.s += a_attrs[a_attr].len;
					switch (a_attr) {
						case sdpaattr_rtpmap:
							/* a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters>] , max.one a=rtpmap: per codec */
						case sdpaattr_fmtp:
							/* a=fmtp:<format/payload type> <format specific params>, max.one a=fmtp: per codec */

							/* we validate only things important for us. Other thinkgs not important we leave up to UA. Not tested:
							   - payload order of a:rtpmap corresponds to m= line
							   - all dynamic payloads have cooresponding a:rtpmap line (for us it's unknown codec)
							   - if static payload type corresponds to codec, i.e. e.g. if 0 is PCMU
							 */
							cp = eat_token_end(lvalue.s, lend);
							lvalue.len = cp-lvalue.s;
							savec = lvalue.s[lvalue.len];
							lvalue.s[lvalue.len] = '\0';
							payload_type = atol(lvalue.s);
							lvalue.s[lvalue.len] = savec;
							for (i=0; i < sess->media[sess->media_count-1].codec_count; i++) {
								if ((*sess->media[sess->media_count-1].codecs)[i].payload_type == payload_type) {
									goto found;
								}
							}
							ERR(MODULE_NAME": parse_sdp_content: '%.*s' payload type (%d) has not been mentioned at m= line\n", (int) (lend-line.s), line.s, payload_type);
							return -1;
						found:
							cp = eat_space_end(cp, lend);
							switch (a_attr) {
								case sdpaattr_rtpmap:
									if ((*sess->media[sess->media_count-1].codecs)[i].a_rtpmap_line_s.s) {
										ERR(MODULE_NAME": parse_sdp_content: '%.*s' multiple a=rtpmap lines for payload type (%d)\n", (int) (lend-line.s), line.s, payload_type);
										return -1;

									}
									(*sess->media[sess->media_count-1].codecs)[i].a_rtpmap_line_s = line;
									lvalue.s = cp;
									cp = eat_token2_end(cp, lend, '/');
									lvalue.len = cp-lvalue.s;
									(*sess->media[sess->media_count-1].codecs)[i].codec_id = name2codec_id(&lvalue, NULL);
									break;
								case sdpaattr_fmtp:
									if ((*sess->media[sess->media_count-1].codecs)[i].a_fmtp_line_s.s) {
										ERR(MODULE_NAME": parse_sdp_content: '%.*s' multiple a=fmtp lines for payload type (%d)\n", (int) (lend-line.s), line.s, payload_type);
										return -1;
									}
									(*sess->media[sess->media_count-1].codecs)[i].a_fmtp_line_s = line;
									break;
								default:
									break;
							}
							break;
						case sdpaattr_rtcp:
							/* a=rtcp: port [nettype space addrtype space connection-address] */
							ERR(MODULE_NAME": parse_sdp_content: a=rtcp parameter is ignored '%.*s', RTCP relaying may fail\n", (int) (lend-line.s), line.s);
							break;
						default:
							;
					}
				}
				break;

			default:
				;
		}
	}
	return 0;
}

/* simple wrapper to call parse_sdp_content() only once per request */
static inline int check_parse_sdp_content(struct sip_msg* msg, struct sdp_session *sess) {
	switch (sdp_parsed) {
		case -1:
		case 0:
			return sdp_parsed;
		default:
			sdp_parsed = parse_sdp_content(msg, sess);
			return sdp_parsed;
	}
}

static int prepare_lumps(struct sip_msg* msg, str* position, str* s) {
	struct lump* anchor;
	char *buf;

	if (!position->s)
		return 0;
//ERR("'%.*s' --> '%.*s'\n", STR_FMT(position), STR_FMT(s));	
	anchor = del_lump(msg, position->s - msg->buf, position->len, 0);
	if (anchor == NULL) {
		ERR(MODULE_NAME": prepare_lumps: del_lump failed\n");
		return -1;
	}
	if (!s || !s->len) return 0;

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
	global_params.sdp_ip.len = 0;
	for (i=0; i<sdp_sess->media_count; i++) {
		if (sdp_sess->media[i].active) {
			if (ipt_sess->sdp_media[i] < 0) {
				goto cline_fixed;
			}
			for (j=0; j<i; j++) {
				if (sdp_sess->media[j].active && sdp_sess->media[i].ip_s.s == sdp_sess->media[j].ip_s.s && ipt_sess->sdp_media[j] >= 0) {
					goto cline_fixed;
				}
			}
			if (global_params.sdp_ip.len == 0) {
				/* takes 1st ip to be rewritten, for aux purposes only */
				global_params.sdp_ip = sdp_sess->media[i].ip_s;
			}
			if (sdp_sess->media[i].ip != 0) {  /* we won't update 0.0.0.0 to anything because such a UA cannot receive. The session may be allocated and reused (unless expires) */
				/* apply lump for ip address in c= line */
				ip42s(ipt_sess->sessions[ipt_sess->sdp_media[i]].dir[!gate_a_to_b].switchboard.addr.ip, &s);
				if (prepare_lumps(msg, &sdp_sess->media[i].ip_s, &s) < 0)
					return -1;
			}
	cline_fixed:
			/* apply lump for port in m= line */
			s.s = int2str((ipt_sess->sdp_media[i]<0)? 0/* disable stream */: ipt_sess->sessions[ipt_sess->sdp_media[i]].dir[!gate_a_to_b].stream[0].port, &s.len);
			if (prepare_lumps(msg, &sdp_sess->media[i].port_s, &s) < 0)
				return -1;
		}
	}
	/* do topo hiding if all media are disabled for c= line then set address 0.0.0.0 to hide UA location */
	for (i=0; i<sdp_sess->media_count; i++) {
		if (sdp_sess->media[i].ip && (!sdp_sess->media[i].active || ipt_sess->sdp_media[i] < 0)) { /* not affected but previous loop */
			for (j=0; j<i; j++) {
				if (sdp_sess->media[i].ip_s.s == sdp_sess->media[j].ip_s.s) {
					goto cline_fixed2;  /* must be already updated */
				}
			}
			for (j=i+1; j<sdp_sess->media_count; j++) {
				if (sdp_sess->media[i].ip_s.s == sdp_sess->media[j].ip_s.s && /* same c= line */
				    sdp_sess->media[i].active && ipt_sess->sdp_media[i] >= 0) {  /* has media enabled */
					goto cline_fixed2;
				}
			}
			/* apply lump for ip address in c= line */
			ip42s(0, &s);
			if (prepare_lumps(msg, &sdp_sess->media[i].ip_s, &s) < 0)
				return -1;
		cline_fixed2:
			;
		}
	}


	if (sdp_sess->oline_addr_s.s) {  /* o= line exists */
		if (global_params.oline_user.len) {
			if (prepare_lumps(msg, &sdp_sess->oline_user_s, &global_params.oline_user) < 0)
				return -1;
		}
		if (global_params.oline_addr.len) {
			if (prepare_lumps(msg, &sdp_sess->oline_addr_s, &global_params.oline_addr) < 0)
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
	if (sess->sdp_media_count) {
		if (sess->switchboard) {
			memcpy(p, sess->switchboard->name.s, sess->switchboard->name.len);
			p += sess->switchboard->name.len;
		}
		*p = ':';
		p++;
		for (i=0; i<sess->sdp_media_count; i++) {
			if (sess->sdp_media[i] >= 0) {
				p += sprintf(p, "%u/%u", 
						sess->sessions[sess->sdp_media[i]].dir[0].sess_id, 
						sess->sessions[sess->sdp_media[i]].sh.created
					);
			}
			*p = ',';
			p++;
		}
		p--;
		*p = '\0';
	}
	session_ids->s = buf;
	session_ids->len = p - buf;
}

/* switchboardname [":" [sess_id "/" created] [ * ( "," [sess_id "/" created] )] ] */
/* sessids are placed at dir[0] */
static int unserialize_ipt_session(str* session_ids, struct ipt_session* sess) {
	char *p, *pend, savec;
	str s;
	unsigned int sess_id, created, i;
	
	memset(sess, 0, sizeof(*sess));
	if (session_ids->len == 0) {
		return 0;
	}
	p = session_ids->s;
	pend = session_ids->s+session_ids->len;
	s.s = p;
	while (p < pend && is_alpha(*p)) p++;
	s.len = p-s.s;
	sess->switchboard = find_switchboard(&s, NULL);
	global_params.switchboard[0] = sess->switchboard;
	if (s.len && !sess->switchboard) {  /* empty switchboard is stored if all sessions are forced as disabled (port==0), SDP streams are empty but we need test medias later*/
		ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', switchboard '%.*s' not found\n", STR_FMT(session_ids), STR_FMT(&s));
		return -1;
	}
	if (p == pend) return 0;
	if (*p != ':') {
		ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', colon expected near '%.*s'\n", STR_FMT(session_ids), PTR2INT(pend-p), p);
		return -1;
	}
	do {
		if (sess->sdp_media_count >= MAX_MEDIA_NUMBER) {
		ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', max.media number (%d) exceeded\n", STR_FMT(session_ids), MAX_MEDIA_NUMBER);
			return -1;
		}
		sess->sdp_media[sess->sdp_media_count] = -1;
		p++;		
		if (p < pend && *p != ',') {
			s.s = p;
			while (p < pend && (*p >= '0' && *p <= '9')) p++;
			s.len = p-s.s;
			if (s.len == 0 || p == pend || *p != '/') {
				ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', '/' expected near '%.*s'\n", STR_FMT(session_ids), PTR2INT(pend-p), p);
				return -1;
			}
			savec = s.s[s.len];
			s.s[s.len] = '\0';
			sess_id = atol(s.s);
			s.s[s.len] = savec;
			p++;
			s.s = p;
			while (p < pend && (*p >= '0' && *p <= '9')) p++;
			s.len = p-s.s;
			if (s.len == 0 || (p != pend && *p != ',')) {
				ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', comma expected near '%.*s'\n", STR_FMT(session_ids), PTR2INT(pend-p), p);
				return -1;
			}
			savec = s.s[s.len];
			s.s[s.len] = '\0';
			created = atol(s.s);
			s.s[s.len] = savec;

			for (i=0; i<sess->sdp_media_count; i++) {
				if (sess->sdp_media[i] >= 0 && sess->sessions[sess->sdp_media[i]].dir[0].sess_id == sess_id) {
					if (sess->sessions[sess->sdp_media[i]].sh.created != created) {
						ERR(MODULE_NAME": unserialize_ipt_session: '%.*s', sess-id/created mismatch '%u/(%u!=%u)'\n", 
								STR_FMT(session_ids), sess_id, sess->sessions[sess->sdp_media[i]].sh.created, created);
						return -1;
					}
					sess->sdp_media[sess->sdp_media_count] = sess->sdp_media[i];
					goto cont;
				}
			}
			sess->sessions[sess->session_count].dir[0].switchboard.addr = sess->switchboard->switchboard_addr;
			sess->sessions[sess->session_count].dir[0].sess_id = sess_id;
			sess->sessions[sess->session_count].sh.created = created;
			sess->sdp_media[sess->sdp_media_count] = sess->session_count;
			sess->session_count++;
		}
	cont:
		sess->sdp_media_count++;
	} while (p < pend);
	return 0;
}

static inline int check_host_err(struct host_item *hi, int ret) {
	switch (hi->handle.err_no) {
		case XT_RTPPROXY_ERR_CANNOT_OPEN_SOCKET:
		case XT_RTPPROXY_ERR_RPC:
			atomic_set_int(&hi->stat->last_error_stamp, (int) time(NULL));
			break;
		default:
			atomic_set_int(&hi->stat->last_ok_stamp, (int) time(NULL));
	}
	return ret;
}

static inline int check_open_handle(struct host_item* hi) {
	if (!hi->handle_is_opened) {
		if (hi->local) {
			if (check_host_err(hi, xt_RTPPROXY_open(&hi->handle, xt_rtpproxy_LOCAL, NULL)) < 0) goto err;
		} else {
			if (check_host_err(hi, xt_RTPPROXY_open(&hi->handle, xt_rtpproxy_REMOTE, &hi->rpc_params)) < 0) goto err;
		}
		hi->handle_is_opened = 1;
	}
	return 0;
err:
	ERR(MODULE_NAME": %s (%d)\n", hi->handle.err_str, hi->handle.err_no);
	return -1;
}


static void delete_ipt_sessions(struct host_item* hi, struct ipt_session* ipt_sess, struct ipt_session *ipt_surviving_sess) {
	int i;
	for (i=0; i < ipt_sess->session_count; i++) {
		if (ipt_sess->switchboard == ipt_surviving_sess->switchboard) {				
			int j;
			for (j=0; j < ipt_surviving_sess->session_count; j++) {
				if (ipt_sess->sessions[i].dir[0].sess_id == ipt_surviving_sess->sessions[j].dir[0].sess_id &&
					ipt_sess->sessions[i].sh.created == ipt_surviving_sess->sessions[j].sh.created	) { /* we do non need test also created */
					goto skip_del;
				}
			}
		}
		ipt_sess->sessions[i].sh.flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_DESTROY;
	skip_del: 
		;
	}

//ERR("DEBUG_RTPPROXY: module: delete_ipt_sessions: xt_RTPPROXY_update_sessions(%d)\n", ipt_sess->session_count);
	if (check_host_err(hi, xt_RTPPROXY_update_sessions(&hi->handle, ipt_sess->session_count, &ipt_sess->sessions)) < 0) {
		ERR(MODULE_NAME": delete_ipt_sessions: xt_RTPPROXY_update_session error: %s (%d)\n", hi->handle.err_str, hi->handle.err_no);
		/* what to do ? */
	}
}

#define GATE_FLAG 0x01
#define UPDATE_SDP_ONLY_FLAG 0x02

/* gate_a_to_b has index 0, gate_b_to_a 1 */
#define GATE_A_TO_B(flags) (((flags) & GATE_FLAG) == 0)

/* SDP (sdp_session) -> ipt RTP proxy session [dir == 0] */
inline static void fill_in_session(int flags, int media_idx, struct sdp_session *sdp_sess, struct xt_rtpproxy_sockopt_session *in_session) {
	int j;
	for (j=0; j<2; j++) {
		if (sdp_sess) {
			in_session->dir[GATE_A_TO_B(flags)].stream[j].flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_ADDR;
			in_session->dir[GATE_A_TO_B(flags)].stream[j].source.ip = sdp_sess->media[media_idx].ip;
			in_session->dir[GATE_A_TO_B(flags)].stream[j].source.port = sdp_sess->media[media_idx].port+j;
		}
		if (global_params.learning_timeout > 0) {
			in_session->dir[GATE_A_TO_B(flags)].stream[j].flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_LEARNING_TIMEOUT;
			in_session->dir[GATE_A_TO_B(flags)].stream[j].learning_timeout = global_params.learning_timeout; 
		}
	}
	if (global_params.always_learn >= 0) {
		in_session->dir[GATE_A_TO_B(flags)].always_learn = global_params.always_learn!=0;
		in_session->dir[GATE_A_TO_B(flags)].flags |= XT_RTPPROXY_SOCKOPT_FLAG_ALWAYS_LEARN;
	}
	if (global_params.expiration_timeout > 0) {
		in_session->sh.expires_timeout = global_params.expiration_timeout;
		in_session->sh.flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_EXPIRES;
	}
	if (global_params.ttl >= 0) {
		in_session->sh.ttl = global_params.ttl;
		in_session->sh.flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_TTL;
	}
}

inline static void fill_in_session_throttle(int flags, int media_idx, struct xt_rtpproxy_sockopt_session *in_session) {
	int j;
	if (global_params.throttle.mark > 0) {
		for (j=0; j<2; j++) {			
			in_session->dir[GATE_A_TO_B(flags)].stream[j].throttle.mark = global_params.throttle.mark;
			in_session->dir[GATE_A_TO_B(flags)].stream[j].flags |= XT_RTPPROXY_SOCKOPT_FLAG_THROTTLE_MARK;
		}
	}
	for (j=0; j<2; j++) {
		if (global_params.codec_set && 
		    (global_params.codec_set->media_types[global_sdp_sess.media[media_idx].media_type].throttle.bandwidth[j].packets > 0 ||
		    global_params.codec_set->media_types[global_sdp_sess.media[media_idx].media_type].throttle.bandwidth[j].bytes > 0)
		   ) {
			in_session->dir[GATE_A_TO_B(flags)].stream[j].throttle.max_bandwidth = global_params.codec_set->media_types[global_sdp_sess.media[media_idx].media_type].throttle.bandwidth[j];
			in_session->dir[GATE_A_TO_B(flags)].stream[j].flags |= XT_RTPPROXY_SOCKOPT_FLAG_THROTTLE_BANDWIDTH;
		} else if (global_params.throttle.bandwidth[j].bytes > 0 || global_params.throttle.bandwidth[j].packets > 0 ) {
			in_session->dir[GATE_A_TO_B(flags)].stream[j].throttle.max_bandwidth = global_params.throttle.bandwidth[j];
			in_session->dir[GATE_A_TO_B(flags)].stream[j].flags |= XT_RTPPROXY_SOCKOPT_FLAG_THROTTLE_BANDWIDTH;

		}
	}
}

static int rtpproxy_alloc(struct sip_msg* msg, char* _flags, char* _dummy) {
	int flags;
	struct ipt_session ipt_sess;
	struct host_item* hi = NULL;
	struct xt_rtpproxy_switchboard_id aggregated_switchboards[MAX_AGGREGATED_NUMBER]; 
	time_t stamp;
	
	xt_rtpproxy_sockopt_count cnt[2];
	str s;
	int i, aggr_fl, reuse_existing_count;

	if (get_int_fparam(&flags, msg, (fparam_t*) _flags) < 0) {
		return -1;
	}
	if (check_parse_sdp_content(msg, &global_sdp_sess) < 0) return -1;

ERR("RTPPROXY_DEBUG: sdp.media_count: %d, flags: %d\n", global_sdp_sess.media_count, flags);
	if (global_params.protected_sess.switchboard) {  /* any protected ? */
		/* get session source address from kernel module and compare with SDP content */
		for (i = 0; i < global_params.protected_sess.session_count; i++) {
			global_params.protected_sess.sessions[i].sh.flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_INFO;
		}
		if (check_open_handle(global_params.protected_sess.switchboard->host) < 0) {
			return -1;
		}
ERR("RTPPROXY_DEBUG: xt_RTPPROXY_update_sessions(sess#:%d, sdp#:%d, XT_RTPPROXY_SOCKOPT_FLAG_SESSION_INFO)\n", global_params.protected_sess.session_count, global_params.protected_sess.sdp_media_count);
		if (check_host_err(global_params.protected_sess.switchboard->host, xt_RTPPROXY_update_sessions(&global_params.protected_sess.switchboard->host->handle, global_params.protected_sess.session_count, &global_params.protected_sess.sessions)) < 0) {
			ERR(MODULE_NAME": rtpproxy_alloc: xt_RTPPROXY_update_session error when retrieving sessions: %s (%d)\n", 
				global_params.protected_sess.switchboard->host->handle.err_str, 
				global_params.protected_sess.switchboard->host->handle.err_no
			);
			return -1;
		}
	}

	reuse_existing_count = 0;
	memset(&ipt_sess, 0, sizeof(ipt_sess));
	for (i = 0; i < global_sdp_sess.media_count; i++) {
		ipt_sess.sdp_media[i] = -1;
		if (global_sdp_sess.media[i].active) {
			int j;
			for (j = 0; j < i; j++) {
				/* if two media streams have equal source address than we will allocate only one ipt session */
				if (global_sdp_sess.media[j].active) {
					if (global_sdp_sess.media[i].ip == global_sdp_sess.media[j].ip && global_sdp_sess.media[i].port == global_sdp_sess.media[j].port) {
						if (global_sdp_sess.media[i].ip != 0) {
							ipt_sess.sdp_media[i] = ipt_sess.sdp_media[j];
							goto cont;						
						} else if (i < global_params.protected_sess.sdp_media_count) {
							int k, l;
							k = global_params.protected_sess.sdp_media[i];
							l = global_params.protected_sess.sdp_media[j];
							if ((global_params.protected_sess.sessions[k].sh.flags & XT_RTPPROXY_SOCKOPT_FLAG_NOT_FOUND) == 0 &&
							    global_params.protected_sess.sessions[k].dir[GATE_A_TO_B(flags)].stream[0].source.ip == global_params.protected_sess.sessions[l].dir[GATE_A_TO_B(flags)].stream[0].source.ip &&
							global_params.protected_sess.sessions[k].dir[GATE_A_TO_B(flags)].stream[0].source.port == global_params.protected_sess.sessions[l].dir[GATE_A_TO_B(flags)].stream[0].source.port) {
 
								/* if ip == 0, for example phone goes on-hold we'll take IP from protected sessions if possible */
								ipt_sess.sdp_media[i] = ipt_sess.sdp_media[j];
								goto cont;
							}
						}
					}
				}
			}
			/* if there are existing sessions then we take those instead of allocation new ones */
			/* we can match 1:1 existing media streams against SDP sessions provided by SDP */
			if (i < global_params.protected_sess.sdp_media_count) {
				int k;
				k = global_params.protected_sess.sdp_media[i];
ERR("RTPPROXY_DEBUG: protected.sess media:%d -> sess:%d, flags: %d\n", i, k, global_params.protected_sess.sessions[k].sh.flags);
				if ((global_params.protected_sess.sessions[k].sh.flags & XT_RTPPROXY_SOCKOPT_FLAG_NOT_FOUND) == 0) {
					switch (global_params.protected_sess.sessions[k].sh.state) {
						case xt_rtpproxy_INIT1:
						case xt_rtpproxy_INIT2:	
						case xt_rtpproxy_FORWARD1:
						case xt_rtpproxy_FORWARD2:
							/* is original ip:port of existion session equal to ip:port provided by SDP ? RTP test is sufficient */
							/* Workaround: if a phone (Sipura, X-Lite, ...) goes on-hold then
							c= line address is set to 0.0.0.0. It's not correct because RTCP media
							can't tricle. It would not force new RTP session allocation. 
							So we won't update SDP c= line but get sees_id from protected sess to reuse it when on-hold terminates.
							But when on-hold is too long and session expires then new session will be allocated */
ERR("DEBUG_RTPPROXY: module: CMP %x=%x & %d=%d\n", global_sdp_sess.media[i].ip, global_params.protected_sess.sessions[k].dir[GATE_A_TO_B(flags)].stream[0].source.ip, global_sdp_sess.media[i].port, global_params.protected_sess.sessions[k].dir[GATE_A_TO_B(flags)].stream[0].source.port);
							if ((global_sdp_sess.media[i].ip == 0 || 
							     global_params.protected_sess.sessions[k].dir[GATE_A_TO_B(flags)].stream[0].source.ip == 0 ||
							     global_params.protected_sess.sessions[k].dir[GATE_A_TO_B(flags)].stream[0].source.ip == global_sdp_sess.media[i].ip) &&
							    /* global_sdp_sess.media[i].port always because active && */  
							    global_params.protected_sess.sessions[k].dir[GATE_A_TO_B(flags)].stream[0].source.port == global_sdp_sess.media[i].port) {
								/* keep all reused sess at the beginning of list, i.e. make slot */
ERR("RTPPROXY_DEBUG: REUSE!\n");
								for (j=ipt_sess.session_count; j > 0; j--) {	
									ipt_sess.sessions[j] = ipt_sess.sessions[j-1];									
								}
								for (j=0; j < i; j++) {
									if (ipt_sess.sdp_media[j] >= 0) {
										ipt_sess.sdp_media[j]++;
									}
								}
								/* put it at slot [0], copy data from existing session */
								for (j=0; j<2; j++) {
									ipt_sess.sessions[0].dir[j] = global_params.protected_sess.sessions[k].dir[j];
								}
								ipt_sess.sessions[0].sh = global_params.protected_sess.sessions[k].sh;

								ipt_sess.sdp_media[i] = 0;
								reuse_existing_count++;
								goto skip_fill;
							}
							break;
						default:
							;
					}
				}

			}
			if (global_sdp_sess.media[i].ip == 0) {
				switch (global_sdp_sess.media[i].send_rec_modifier) {
				case sdpaattr_sendonly:
				case sdpaattr_sendrecv: /* it's error because it cannot receive anything but client are weird */
					break;  /* they can send RTP/RTCP, not recommended in RFC3264, maybe allow only when learning possible */
				default:
					/* do not allocate session for on-hold stream unless reused, disable stream (sdp_media[i]) < 0 */
ERR("DEBUG_RTPPROXY: module: do not allocate session for on-hold stream unless reused\n");
					goto cont;
				}
			}
			fill_in_session(flags, i, &global_sdp_sess, ipt_sess.sessions+ipt_sess.session_count);
			fill_in_session_throttle(flags, i, ipt_sess.sessions+ipt_sess.session_count);
			ipt_sess.sdp_media[i] = ipt_sess.session_count;
		skip_fill:
			ipt_sess.session_count++;
		}
	cont:
		;
	}
	ipt_sess.sdp_media_count = global_sdp_sess.media_count;

ERR("RTPPROXY_DEBUG: session_count: %d, reuse_existing_count: %d\n", global_sdp_sess.media_count, reuse_existing_count);

	if (ipt_sess.session_count > reuse_existing_count) {
		stamp = time(NULL);
		if (reuse_existing_count > 0) {
			/* we need allocate sessions at the same switchboard as already being existed */
			aggr_fl = 0;
			hi = global_params.protected_sess.switchboard->host;
			ipt_sess.switchboard = global_params.protected_sess.switchboard;
			for (i=reuse_existing_count; i < ipt_sess.session_count; i++) {
				int j;
				for (j=0; j<2; j++) {
					ipt_sess.sessions[i].dir[j].switchboard.addr = ipt_sess.sessions[0].dir[j].switchboard.addr;
				}
			}
		} else {
			for (i=0; i<2; i++) {
				if (!global_params.switchboard[i] && !global_params.aggregation[i]) {
					ERR(MODULE_NAME": rtpproxy_alloc: aggregation/switchboard not set (dir:%d)\n", i);
					return -1;
				}
			}
			aggr_fl = global_params.aggregation[0] || global_params.aggregation[1];
			if (aggr_fl) {
				struct switchboard_item *si;
				/* calculate switchboard weights. There is minor problem when weight are calculated some time before
				   RPC commands are performed, i.e. if a remote RPC server become unavailable then more processes
				   may spend time waiting for unresponsive machine even it's been discovered by parallel process.
				*/
				for (si=switchboards; si; si=si->next) {
					unsigned int w;
					int a, f;
					time_t ok_stamp, err_stamp;
					a = f = 0;
					ok_stamp = atomic_get_int(&si->host->stat->last_ok_stamp);
					err_stamp = atomic_get_int(&si->host->stat->last_error_stamp);
					if (rpc_heartbeat_timeout > 0 && err_stamp > ok_stamp && (stamp-err_stamp) >= rpc_heartbeat_timeout) {
						/* set max. priority to force remote rtpproxy rpc call, i.e. test if is alive or dead */
						ok_stamp = err_stamp = 0;				
					}
					if (err_stamp > ok_stamp) {
						/* lowest priority */
						/* prefer older error */
						w = time(NULL) - err_stamp + 1;
						if (w > 999) w = 999;

					} else if (ok_stamp == 0) {
						/* not yet acquired, highest */
						w = 100000000 + (rand() & 0xFFFF);  /* randomize not yet asked or being hartbeated */
					} else {
						/* middle */
						w = 1000;
						a = atomic_get(&si->stat->alloc);
						f = atomic_get(&si->stat->free);
						if ((a + f) > 0) {
							/* prefer switchboards having more free slots */
							w += (1000*f)/(a+f);
						}
					}
					si->weight = w;
		//ERR(MODULE_NAME": rtpproxy_alloc: switchboard '%.*s' (ok_stamp: %u, err_stamp: %u, alloc: %u, free: %u, weight: %u)\n", STR_FMT(&si->name), (unsigned int) ok_stamp, (unsigned int) err_stamp, a, f, w);
				}
				hi = NULL;
			}
			else {
				if (global_params.switchboard[0]->host != global_params.switchboard[1]->host) {
					ERR(MODULE_NAME": rtpproxy_alloc: switchboard resides of different hosts '%.*s'!='%.*s'\n", 
						STR_FMT(&global_params.switchboard[0]->host->name),
						STR_FMT(&global_params.switchboard[1]->host->name)
					);
					return -1;
				}
				hi = global_params.switchboard[0]->host;
				for (i=0; i < ipt_sess.session_count; i++) {
					int j;
					for (j=0; j<2; j++) {
						ipt_sess.sessions[i].dir[j].switchboard.addr = global_params.switchboard[j]->switchboard_addr;
					}
				}
			}
		}
	try_next_host:
		cnt[0] = cnt[1] = 0;
		if (aggr_fl) {
			int j;
			hi = NULL;
			if (global_params.aggregation[0] && global_params.aggregation[1]) {
				int w = 0;
				/* find switchboard having max. weight */
				for (i=0; i<global_params.aggregation[0]->switchboard_count; i++) {
					if ((*global_params.aggregation[0]->switchboards)[i]->weight > w) { /* weight==0 is skipped */
						time_t err_stamp;
						err_stamp = atomic_get_int(&(*global_params.aggregation[0]->switchboards)[i]->host->stat->last_error_stamp);
						if (err_stamp >= stamp) {
							if (w > 0) continue;
							/* decrease weight to minimum, parallel process meanwhile got error */
							w = 1;
						} else {
							w = (*global_params.aggregation[0]->switchboards)[i]->weight;
						}
						hi = (*global_params.aggregation[0]->switchboards)[i]->host;
					}
				}
			} else {
				for (j=0; j<2; j++) {
					if (!global_params.aggregation[j] && global_params.switchboard[j]->weight) {
						hi = global_params.switchboard[j]->host;
					}
				}
			}
			if (!hi) {
				ERR(MODULE_NAME": rtpproxy_alloc: cannot allocate aggregated switchboard (#1)\n");
				return -1;
			}
			for (j=0; j<2; j++) {
				if (global_params.aggregation[j]) {				
					struct switchboard_item *aggr_switchboards[MAX_AGGREGATED_NUMBER];
					for (i=0; i<global_params.aggregation[j]->switchboard_count; i++) {
						if ((*global_params.aggregation[j]->switchboards)[i]->weight && 
						    (*global_params.aggregation[j]->switchboards)[i]->host == hi) {
							int k, l;
							if (cnt[0]+cnt[1] >= MAX_AGGREGATED_NUMBER) {
								ERR(MODULE_NAME": rtpproxy_alloc: number of aggregated switchboard exceeded limit %d\n", MAX_AGGREGATED_NUMBER);
								return -1;
							}
							/* put switchboard ordered by weight */
							for (k=0; k<cnt[j] && aggr_switchboards[k]->weight >= (*global_params.aggregation[j]->switchboards)[i]->weight; k++);		
							for (l=cnt[j]; l>k; l--) {
								aggr_switchboards[l] = aggr_switchboards[l-1];
							}
							aggr_switchboards[k] = (*global_params.aggregation[j]->switchboards)[i];
							cnt[j]++;
						}
					}
					if (!cnt[j]) {
						ERR(MODULE_NAME": rtpproxy_alloc: cannot allocate aggregated switchboard (#2)\n");
						return -1;
					}
					for (i = 0; i < cnt[j]; i++) {
						aggregated_switchboards[j*cnt[0]+i] = aggr_switchboards[i]->switchboard_addr;
					}
				}
				else {
					if (cnt[0]+cnt[1] >= MAX_AGGREGATED_NUMBER) {
						ERR(MODULE_NAME": rtpproxy_alloc: number of aggregated switchboard exceeded limit %d\n", MAX_AGGREGATED_NUMBER);
						return -1;
					}
					aggregated_switchboards[cnt[0]+cnt[1]] = global_params.switchboard[j]->switchboard_addr;
					cnt[j]++;
				}
			}
			for (j=0; j<2; j++) {
				if (global_params.aggregation[j]) {				
					for (i=0; i<global_params.aggregation[j]->switchboard_count; i++) {
						if ((*global_params.aggregation[j]->switchboards)[i]->host == hi) {
							/* done, do not process in next round again */
							(*global_params.aggregation[j]->switchboards)[i]->weight = 0;
						}
					}
				}
				else {
					global_params.switchboard[j]->weight = 0;
				}
			}
		}

		if (reuse_existing_count < ipt_sess.session_count) {  /* allocation required ? */
			if (check_open_handle(hi) < 0) {
				if (aggr_fl) {
					goto try_next_host;
				}
				return -1;
			}
		ERR("DEBUG_RTPPROXY: module: rtpproxy_alloc: xt_RTPPROXY_alloc_sessions(%d/%d/%d), host: '%.*s', flags: %d\n", cnt[0], cnt[1], ipt_sess.session_count, STR_FMT(&hi->name), flags);
			if (check_host_err(hi, xt_RTPPROXY_alloc_sessions(&hi->handle,
						cnt[0],
						&aggregated_switchboards, 
						cnt[1],
						(void*) &aggregated_switchboards[cnt[0]],
						ipt_sess.session_count-reuse_existing_count,  /* allocate only non-reused sessions */
						&ipt_sess.sessions+reuse_existing_count
				)) < 0) {
				ERR(MODULE_NAME": rtpproxy_alloc: xt_RTPPROXY_alloc_session error: %s (%d)\n", hi->handle.err_str, hi->handle.err_no);
				if (aggr_fl) {
					goto try_next_host;
				}
				return -1;
			}
		}
	}
	if (update_sdp_content(msg, GATE_A_TO_B(flags), &global_sdp_sess, &ipt_sess) < 0) {
		delete_ipt_sessions(hi, &ipt_sess, &global_params.protected_sess);
		return -1;
	}
	if (ipt_sess.session_count) {
		ipt_sess.switchboard = find_switchboard_by_addr(&ipt_sess.sessions[0].dir[0].switchboard.addr);
		if (!ipt_sess.switchboard) {
			BUG(MODULE_NAME": rtpproxy_alloc: switchboard-a definition not found\n");
			return -1;
		}
	//ERR("DEBUG_RTPPROXY: module: rtpproxy_alloc: switchboard-a '%.*s'\n", STR_FMT(&ipt_sess.switchboard->name));
		global_params.switchboard[0] = ipt_sess.switchboard;
		global_params.switchboard[1] = find_switchboard_by_addr(&ipt_sess.sessions[0].dir[1].switchboard.addr);	
		if (!global_params.switchboard[1]) {
			BUG(MODULE_NAME": rtpproxy_alloc: switchboard-b definition not found\n");
			return -1;
		}
	//ERR("DEBUG_RTPPROXY: module: rtpproxy_alloc: switchboard-b '%.*s'\n", STR_FMT(&global_params.switchboard[1]->name));
		atomic_set(&global_params.switchboard[0]->stat->free, ipt_sess.sessions[0].dir[0].switchboard.free);
		atomic_set(&global_params.switchboard[0]->stat->alloc, ipt_sess.sessions[0].dir[0].switchboard.alloc);
		if (global_params.switchboard[0] != global_params.switchboard[1]) {
			atomic_set(&global_params.switchboard[1]->stat->free, ipt_sess.sessions[0].dir[1].switchboard.free);
			atomic_set(&global_params.switchboard[1]->stat->alloc, ipt_sess.sessions[0].dir[1].switchboard.alloc);
		}
	}
	else {
		ipt_sess.switchboard = global_params.protected_sess.switchboard; /* we need still keep the same switchboard */
		global_params.switchboard[0] = ipt_sess.switchboard;
	}
	serialize_ipt_session(&ipt_sess, &s);
	global_params.session_ids = s; /* it's static and null terminated */
	return 1;
}

static int rtpproxy_update(struct sip_msg* msg, char* _flags, char* _session_ids) {
	str session_ids;
	int flags, i;
	struct ipt_session ipt_sess;

	if (get_int_fparam(&flags, msg, (fparam_t*) _flags) < 0) {
		return -1;
	}
	if (get_str_fparam(&session_ids, msg, (fparam_t*) _session_ids) < 0) {
		return -1;
	}
	if (unserialize_ipt_session(&session_ids, &ipt_sess) < 0) {
		return -1;
	}
	if (check_parse_sdp_content(msg, &global_sdp_sess) < 0) return -1;

	if (ipt_sess.sdp_media_count != global_sdp_sess.media_count) {
		ERR(MODULE_NAME": rtpproxy_update: number of m= item in offer (%d) and answer (%d) do not correspond\n", ipt_sess.sdp_media_count, global_sdp_sess.media_count);
		return -1;
	}
	/* first we check for unexpected duplicate source ports */
	for (i = 0; i < global_sdp_sess.media_count; i++) {
		if (ipt_sess.sdp_media[i] >= 0 && global_sdp_sess.media[i].active) {
			int j;
			for (j = i+1; j < global_sdp_sess.media_count; j++) {
				if (ipt_sess.sdp_media[j] >= 0 && global_sdp_sess.media[j].active) {
					/* if two media streams have equal source address XOR have equal session */
					if ( (global_sdp_sess.media[i].ip == global_sdp_sess.media[j].ip && global_sdp_sess.media[i].port == global_sdp_sess.media[j].port) ^
						 (ipt_sess.sdp_media[i] == ipt_sess.sdp_media[j]) ) {
						ERR(MODULE_NAME": rtpproxy_update: media (%d,%d) violation number\n", i, j);
						return -1;
					}
				}
			}
		}
	}

	if (flags & UPDATE_SDP_ONLY_FLAG) {
		/* get session source address from kernel module, do not update RTP session, only updateSDP content */
		for (i = 0; i < ipt_sess.session_count; i++) {
			ipt_sess.sessions[i].sh.flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_INFO;
		}
	} else {
		/* first we check sessions to delete, the sessions can be "undeleted" if other media still uses session */
		for (i = 0; i < global_sdp_sess.media_count; i++) {
			if (ipt_sess.sdp_media[i] >= 0) {
				if (!global_sdp_sess.media[i].active) {
					ipt_sess.sessions[ipt_sess.sdp_media[i]].sh.flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_DESTROY;
					ipt_sess.sdp_media[i] = -1;
				}
			}
		}

		for (i = 0; i < global_sdp_sess.media_count; i++) {
			if (ipt_sess.sdp_media[i] >= 0) {
				if (global_sdp_sess.media[i].active) {
					fill_in_session(flags, i, &global_sdp_sess, ipt_sess.sessions+ipt_sess.sdp_media[i]);
					fill_in_session_throttle(flags, i, ipt_sess.sessions+ipt_sess.sdp_media[i]);
					ipt_sess.sessions[ipt_sess.sdp_media[i]].sh.flags &= ~XT_RTPPROXY_SOCKOPT_FLAG_SESSION_DESTROY;
				}
			}
		}
		/* we cannot also delete sessions which have been reused from other session set */
		for (i = 0; i < ipt_sess.session_count; i++) {
			if (ipt_sess.sessions[i].sh.flags & XT_RTPPROXY_SOCKOPT_FLAG_SESSION_DESTROY) {
				if (ipt_sess.switchboard == global_params.protected_sess.switchboard) {
					int j;
					for (j=0; j < global_params.protected_sess.session_count; j++) {
						if (ipt_sess.sessions[i].dir[0].sess_id == global_params.protected_sess.sessions[j].dir[0].sess_id &&
							ipt_sess.sessions[i].sh.created == global_params.protected_sess.sessions[j].sh.created	) {						
							ipt_sess.sessions[i].sh.flags &= ~XT_RTPPROXY_SOCKOPT_FLAG_SESSION_DESTROY;
							ipt_sess.sessions[i].sh.flags |= XT_RTPPROXY_SOCKOPT_FLAG_SESSION_INFO;
							/* or we can remove from sessions TODO */
							break;
						}
					}
				}
			}
		}
	}

//ERR("DEBUG_RTPPROXY: module: rtpproxy_update: xt_RTPPROXY_update_sessions(%d), flags:%d, switchboard:%p, sess:%.*s\n", ipt_sess.session_count, flags, ipt_sess.switchboard, STR_FMT(&session_ids));
	global_params.switchboard[0] = ipt_sess.switchboard;
	if (ipt_sess.switchboard) {
		if (check_open_handle(ipt_sess.switchboard->host) < 0) {
			return -1;
		}
		if (check_host_err(ipt_sess.switchboard->host, xt_RTPPROXY_update_sessions(&ipt_sess.switchboard->host->handle, ipt_sess.session_count, &ipt_sess.sessions)) < 0) {
			ERR(MODULE_NAME": rtpproxy_update: xt_RTPPROXY_update_session error: %s (%d)\n", 
					ipt_sess.switchboard->host->handle.err_str, 
					ipt_sess.switchboard->host->handle.err_no
			);
				/* delete all sessions ? */
			return -1;
		}
		global_params.switchboard[1] = find_switchboard_by_addr(&ipt_sess.sessions[0].dir[1].switchboard.addr);
	} else {
		/* disable media from answer too as we did it in request, port = 0 */
		global_params.switchboard[1] = NULL;
	}
	if (update_sdp_content(msg, GATE_A_TO_B(flags), &global_sdp_sess, &ipt_sess) < 0) {
		/* delete all sessions ? */
		return -1;
	}
	serialize_ipt_session(&ipt_sess, &session_ids);
	global_params.session_ids = session_ids; /* it's static and null terminated */
	return 1;
}

static int rtpproxy_adjust_timeout(struct sip_msg* msg, char* _flags, char* _session_ids) {
	str session_ids;
	int flags, i;
	struct ipt_session ipt_sess;

	if (get_int_fparam(&flags, msg, (fparam_t*) _flags) < 0) {
		return -1;
	}
	if (get_str_fparam(&session_ids, msg, (fparam_t*) _session_ids) < 0) {
		return -1;
	}
	if (unserialize_ipt_session(&session_ids, &ipt_sess) < 0) {
		return -1;
	}
	if (!ipt_sess.switchboard) {
		return 1;
	}
	for (i = 0; i < ipt_sess.sdp_media_count; i++) {
		if (ipt_sess.sdp_media[i] >= 0) {
			fill_in_session(flags, i, NULL, ipt_sess.sessions+ipt_sess.sdp_media[i]);
			/* throttle not affected */
		}
	}
//ERR("DEBUG_RTPPROXY: module: rtpproxy_adjust_timeout: xt_RTPPROXY_update_sessions(%d), flags:%d, sess:%.*s\n", ipt_sess.session_count, flags, STR_FMT(&session_ids));
	if (check_open_handle(ipt_sess.switchboard->host) < 0) {
		return -1;
	}
	if (check_host_err(ipt_sess.switchboard->host, xt_RTPPROXY_update_sessions(&ipt_sess.switchboard->host->handle, ipt_sess.session_count, &ipt_sess.sessions)) < 0) {
		ERR(MODULE_NAME": rtpproxy_adjust_timeout: xt_RTPPROXY_adjust_timeout error: %s (%d)\n", 
				ipt_sess.switchboard->host->handle.err_str, 
				ipt_sess.switchboard->host->handle.err_no
		);
		return -1;
	}
	/* do not serialize sessions because it affect static buffer and more valuable values disappears */
	return 1;
}

static int rtpproxy_delete(struct sip_msg* msg, char* _session_ids, char* _dummy) {
	str session_ids;
	struct ipt_session ipt_sess;
	if (get_str_fparam(&session_ids, msg, (fparam_t*) _session_ids) < 0) {
		return -1;
	}
	if (!session_ids.len) return 1;
	if (unserialize_ipt_session(&session_ids, &ipt_sess) < 0) {
		return -1;
	}
	
//ERR("DEBUG_RTPPROXY: module: rtpproxy_delete: sess:%.*s\n", STR_FMT(&session_ids));
	if (!ipt_sess.switchboard) {
		return 1;  /* nothing to delete */
	}
	if (check_open_handle(ipt_sess.switchboard->host) < 0) {
		return -1;
	}
	delete_ipt_sessions(ipt_sess.switchboard->host, &ipt_sess, &global_params.protected_sess);
	/* do not serialize sessions because it affect static buffer and more valuable values disappears */
	return 1;
}

static int rtpproxy_authorize_media(struct sip_msg* msg, char* _dummy1, char* _dummy2) {
	unsigned int media_count[MAX_MEDIA_NUMBER];
	int i;
	if (!global_params.codec_set) return 1;

	if (check_parse_sdp_content(msg, &global_sdp_sess) < 0) return -1;
	global_params.auth_rights = 0;
	memset(&media_count, 0, sizeof(media_count));

	for (i=0; i<global_sdp_sess.media_count; i++) {
		int j, n, fl;
		if (global_sdp_sess.media[i].active != 1) continue;		
		n = 0;
		fl = media_count[global_sdp_sess.media[i].media_type] == global_params.codec_set->media_types[global_sdp_sess.media[i].media_type].throttle.max_streams;
		if (fl) {
			goto remove_stream;
		}
		for (j=0; j<global_sdp_sess.media[i].codec_count; j++) { 
			unsigned int r;
			struct sdp_codec *c;
			c = &(*global_sdp_sess.media[i].codecs)[j];
			/* codec has been already removed */
			if (!c->mline_payload_type_s.s) continue;
			r = (*global_params.codec_set->media_types[global_sdp_sess.media[i].media_type].codec_rights)[c->codec_id];
			if (r) {
				if (r > global_params.auth_rights) {
					global_params.auth_rights = r;
				}
				if (r & global_params.remove_codec_mask) {
					/* remove codec */
					n++;
					if (n < global_sdp_sess.media[i].codec_count) {
						/* if it's last remainng codec then leave it and remove stream */
						if (prepare_lumps(msg, &c->mline_payload_type_s, NULL) < 0)
							return -1;
						c->mline_payload_type_s.s = NULL;  /* mark as removed */
						if (prepare_lumps(msg, &c->a_rtpmap_line_s, NULL) < 0)
							return -1;
						if (prepare_lumps(msg, &c->a_fmtp_line_s, NULL) < 0)
							return -1;
					}
				}				
			}
		}
	remove_stream:
		if (n == global_sdp_sess.media[i].codec_count || fl) {
			/* remove the stream */
			static str zero_s = STR_STATIC_INIT("0");
			if (prepare_lumps(msg, & global_sdp_sess.media[i].port_s, &zero_s) < 0)
				return -1;
			global_sdp_sess.media[i].active = 0;
			continue;
		}
		media_count[global_sdp_sess.media[i].media_type]++;
	}
	return 1;
}

static int rtpproxy_set_param(struct sip_msg* msg, char* _idx, char* _value) {
	int idx, dir;
	unsigned int ip;
	idx = PTR2INT(_idx);
	union {
		str s;
		int i;
	} u;
	dir = (param_list[idx].flags & PAR_DIR) != 0;
	if (param_list[idx].flags & PAR_INT) {
		if (get_int_fparam(&u.i, msg, (fparam_t*) _value) < 0) {
			return -1;
		}
	} else {
		if (get_str_fparam(&u.s, msg, (fparam_t*) _value) < 0) {
			return -1;
		}
	}
	switch (param_list[idx].id) {
		case PAR_EXPIRATION_TIMEOUT:
			global_params.expiration_timeout = u.i;
			break;
		case PAR_TTL:
			global_params.ttl = u.i;
			break;
		case PAR_LEARNING_TIMEOUT:
			global_params.learning_timeout = u.i;
			break;
		case PAR_ALWAYS_LEARN:
			global_params.always_learn = u.i;
			break;
		case PAR_SWITCHBOARD_A:
		case PAR_SWITCHBOARD_B:
			if (!(global_params.switchboard[dir] = find_switchboard(&u.s, NULL)))
				return -1;
			break;
		case PAR_SWITCHBOARD_BY_SIP_IP_A:
		case PAR_SWITCHBOARD_BY_SIP_IP_B:
			ip = s2ip4(&u.s);
			for (global_params.switchboard[dir] = switchboards; 
			     global_params.switchboard[dir]; 
			     global_params.switchboard[dir] = global_params.switchboard[dir]->next) {

				if (ip == global_params.switchboard[dir]->sip_ip) {
					global_params.aggregation[dir] = NULL;	/* invalidate aggregation */
					return 1;
				}
			}
			return -1;
		case PAR_AGGREGATION_A:
		case PAR_AGGREGATION_B:
			if (!(global_params.aggregation[dir] = find_aggregation(&u.s, NULL)))
				return -1;
			break;
		case PAR_AGGREGATION_BY_SIP_IP_A:
		case PAR_AGGREGATION_BY_SIP_IP_B:
			ip = s2ip4(&u.s);
			for (global_params.aggregation[dir] = aggregations; 
			     global_params.aggregation[dir]; 
			     global_params.aggregation[dir] = global_params.aggregation[dir]->next) {

				if (ip == global_params.aggregation[dir]->sip_ip) {
					global_params.switchboard[dir] = NULL;	/* invalidate switchboard */
					return 1;
				}
			}
			return -1;
	    case PAR_THROTTLE_MARK:
			global_params.throttle.mark = u.i;
			break;
		case PAR_THROTTLE_RTP_MAX_BYTES:
		case PAR_THROTTLE_RTCP_MAX_BYTES:
			global_params.throttle.bandwidth[param_list[idx].id==PAR_THROTTLE_RTCP_MAX_BYTES].bytes = u.i;
			break;
		case PAR_THROTTLE_RTP_MAX_PACKETS:
		case PAR_THROTTLE_RTCP_MAX_PACKETS:
			global_params.throttle.bandwidth[param_list[idx].id==PAR_THROTTLE_RTCP_MAX_PACKETS].packets = u.i;
			break;

		case PAR_CODEC_SET:
			if (!(global_params.codec_set = find_codec_set(&u.s, NULL)))
				return -1;
			break;
		case PAR_REMOVE_CODEC_MASK:
			global_params.remove_codec_mask = u.i;
			break;

		case PAR_OLINE_USER: {
				static char buf[30];
				if (u.s.len > sizeof(buf)) {
						return -1;
				}
				global_params.oline_user.len = u.s.len;
				if (u.s.len) {
					memcpy(buf, u.s.s, u.s.len);
					global_params.oline_user.s = buf;
				}
				break;
			}						
		case PAR_OLINE_ADDR: {
				static char buf[30];
				if (u.s.len > sizeof(buf)) {
						return -1;
				}
				global_params.oline_addr.len = u.s.len;
				if (u.s.len) {
					memcpy(buf, u.s.s, u.s.len);
					global_params.oline_addr.s = buf;
				}
				break;
			}
		case PAR_PROTECTED_SESSION_IDS:
			memset(&global_params.protected_sess, 0, sizeof(global_params.protected_sess));
			if (!u.s.len) break;
			if (unserialize_ipt_session(&u.s, &global_params.protected_sess) < 0) {
				return -1;
			}
			if (!global_params.protected_sess.session_count) {
				global_params.protected_sess.switchboard = NULL;
			}
			break;

		default:
			;
	}
	return 1;
}

/* @select implementation */
static int sel_rtpproxy(str* res, select_t* s, struct sip_msg* msg) {  /* dummy */
	union {int i; str s;} u;
	int dir, idx;
	u.s.len = 0;
	if (msg == NULL && res == NULL) {
		/* fixup call */
		int idx;
		idx = param2idx(&s->params[1].v.s, PAR_READ);
		if (idx < 0) {
			return -1;
		}
		s->params[1].v.i = idx;
		s->params[1].type = SEL_PARAM_DIV;
		return 1;
	}
	idx = s->params[1].v.i;
	dir = (param_list[idx].flags & PAR_DIR) != 0;
	switch (param_list[idx].id) {
		case PAR_EXPIRATION_TIMEOUT:
			u.i = global_params.expiration_timeout;
			break;
		case PAR_TTL:
			u.i = global_params.ttl;
			break;
		case PAR_LEARNING_TIMEOUT:
			u.i = global_params.learning_timeout;
			break;
		case PAR_ALWAYS_LEARN:
			u.i = global_params.always_learn;
			break;
		case PAR_SWITCHBOARD_A:
		case PAR_SWITCHBOARD_B:
			if (global_params.switchboard[dir])				
				u.s = global_params.switchboard[dir]->name;
			break;
		case PAR_AGGREGATION_A:
		case PAR_AGGREGATION_B:
			if (global_params.aggregation[dir])
				u.s = global_params.aggregation[dir]->name;
			break;
		case PAR_SDP_IP:
			u.s = global_params.sdp_ip;
			break;
		case PAR_ACTIVE_MEDIA_NUM: {
			int i;
			if (check_parse_sdp_content(msg, &global_sdp_sess) < 0) return -1;
			u.i = 0;
			for (i = 0; i < global_sdp_sess.media_count; i++) {
				if (global_sdp_sess.media[i].active) {
					u.i++;
				}
			}
			break;
		}
		case PAR_OLINE_USER:
			if (check_parse_sdp_content(msg, &global_sdp_sess) < 0) return -1;
			u.s = global_sdp_sess.oline_user_s;
			break;
		case PAR_OLINE_ADDR:
			if (check_parse_sdp_content(msg, &global_sdp_sess) < 0) return -1;
			u.s = global_sdp_sess.oline_addr_s;
			break;
		case PAR_SESSION_IDS:
			u.s = global_params.session_ids;
			break;
		case PAR_THROTTLE_MARK:
			u.i = global_params.throttle.mark;
			break;
		case PAR_THROTTLE_RTP_MAX_BYTES:
		case PAR_THROTTLE_RTCP_MAX_BYTES:
			u.i = global_params.throttle.bandwidth[param_list[idx].id==PAR_THROTTLE_RTCP_MAX_BYTES].bytes;
			break;
		case PAR_THROTTLE_RTP_MAX_PACKETS:
		case PAR_THROTTLE_RTCP_MAX_PACKETS:
			u.i = global_params.throttle.bandwidth[param_list[idx].id==PAR_THROTTLE_RTCP_MAX_PACKETS].packets;
			break;
		case PAR_CODEC_SET:
			if (global_params.codec_set)
				u.s = global_params.codec_set->name;
			break;
		case PAR_AUTH_RIGHTS:
			u.i = global_params.auth_rights;
			break;
		case PAR_REMOVE_CODEC_MASK:
			u.i = global_params.remove_codec_mask;
			break;
		default:
			;
	}
	if (param_list[idx].flags & PAR_STR) {
		if (!u.s.len) return 1;
		*res = u.s;
	}
	else {
		return uint_to_static_buffer(res, u.i);
	}
	return 0;
}

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT(MODULE_NAME), sel_rtpproxy, CONSUME_NEXT_STR | FIXUP_CALL },
	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static int mod_pre_script_cb(struct sip_msg *msg, unsigned int flags, void *param) {
	memset(&global_params, 0, sizeof(global_params));
	global_params.always_learn = -1;
	global_params.ttl = -1;
	sdp_parsed = 999; /* any number not in (-1,0) */
	return 1;
}

static struct {
	enum {iptrtpproxy_default=0x01, iptrtpproxy_switchboard=0x02, iptrtpproxy_host=0x04} flag;
	str name;
	struct {
		struct {
			struct xt_rtpproxy_switchboard_id addr;
			str host;
		} switchboard;
		struct xt_rtpproxy_connection_rpc_params host;
	} dflt, parsed;

} parse_config_vals;

static int cfg_parse_addr(void* param, cfg_parser_t* st, unsigned int flags) {
	str val;
	char buff[50];
	val.s = buff;
	val.len = sizeof(buff)-1;
	if (cfg_parse_str(&val, st, CFG_EXTENDED_ALPHA|CFG_STR_STATIC) < 0) return -1;
	*(uint32_t*)param  = s2ip4(&val);
	if (*(uint32_t*)param == 0) {
		ERR(MODULE_NAME": parse_addr: bad ip address '%.*s'\n", STR_FMT(&val));
		return -1;
	}
	return 0;
}

static int cfg_parse_uint16(void* param, cfg_parser_t* st, unsigned int flags) {
	int val;
	if (cfg_parse_int(&val, st, 0) < 0) 
		return -1;
	*(uint16_t *) param = val;
	return 0;
}

static int cfg_parse_default(void* param, cfg_parser_t* st, unsigned int flags) {
	int ret;
	cfg_token_t t;
	str val, tok;
	cfg_option_t* opt;
	char buf[MAX_TOKEN_LEN];

	tok.s = buf;

	if (st->cur_opt->val.len >= sizeof(buf)-1) goto skip;
	memcpy(tok.s, st->cur_opt->val.s, st->cur_opt->val.len);
	tok.len = st->cur_opt->val.len;

	/* we need process here options containing dash '-' because of too strict parser */
	while (1) {
		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return ret;
		if (ret > 0) return 0;
		if (t.type == '=')
			break;
		if (tok.len+t.val.len >= sizeof(buf)-1) goto skip;
		memcpy(tok.s+tok.len, t.val.s, t.val.len);
		tok.len += t.val.len;
		
	}
	tok.s[tok.len] = '\0';
	if ((opt = cfg_lookup_token(st->options+1/*2nd pass*/, &tok)) && ((opt->flags & CFG_DEFAULT)== 0)) {
		st->cur_opt = &t;
		if (opt->f(opt->param, st, opt->flags) < 0) return -1;
		return 0;
	}
skip:
	if (cfg_parse_str(&val, st, CFG_EXTENDED_ALPHA) < 0) return -1;
	return 0;
}

static int safe_parsed_values() {
#define PROC_DEFAULT(_f_, _def_) \
	if (!parse_config_vals.parsed._f_) {\
		parse_config_vals.parsed._f_ = parse_config_vals.dflt._f_?parse_config_vals.dflt._f_:_def_; \
	}
	
	if (parse_config_vals.flag & iptrtpproxy_default) {
		if (parse_config_vals.flag & iptrtpproxy_switchboard)
			parse_config_vals.dflt.switchboard = parse_config_vals.parsed.switchboard;
		else if (parse_config_vals.flag & iptrtpproxy_host)
			parse_config_vals.dflt.host = parse_config_vals.parsed.host;

	} else if (parse_config_vals.flag) {
		int fl, max_len, i;
		struct switchboard_item **prev_si = NULL;
		struct host_item **prev_hi = NULL;
		char *s;
		if (parse_config_vals.flag & iptrtpproxy_switchboard) {
			fl = find_switchboard(&parse_config_vals.name, &prev_si) != NULL;
			s = "switchboard";
			max_len = MAX_SWITCHBOARD_NAME_LEN;
		}
		else {
			struct host_item *p;
			p = find_host(&parse_config_vals.name, &prev_hi);
			fl = p != NULL;
			s = "host";
			max_len = HOST_NAME_MAX;
			if (p && p->local) {
				pkg_free(parse_config_vals.name.s);
				goto local;
			}	
		}
		if (fl) {
			ERR(MODULE_NAME": safe_parsed_values: %s name '%.*s' already declared\n", s, STR_FMT(&parse_config_vals.name));
			return -1;
		}
		for (i=0; i<parse_config_vals.name.len; i++) {
			if (!is_alpha(parse_config_vals.name.s[i])) {
				ERR(MODULE_NAME": safe_parsed_values: bad %s name '%.*s'\n", s, STR_FMT(&parse_config_vals.name));
				return -1;
			}
		}
		if (parse_config_vals.name.len > max_len) {
			ERR(MODULE_NAME": safe_parsed_values: %s name '%.*s' is too long (%d>%d)\n", s, STR_FMT(&parse_config_vals.name), parse_config_vals.name.len, max_len);
			return -1;
		}
		
		if (parse_config_vals.flag & iptrtpproxy_switchboard) {
			struct switchboard_item *si;
			si = pkg_malloc(sizeof(*si));
			if (!si) goto out_of_mem;
			memset(si, 0, sizeof(*si));
			si->name = parse_config_vals.name;			
			si->next = (*prev_si);
			(*prev_si) = si;
			PROC_DEFAULT(switchboard.addr.ip, 0);
			PROC_DEFAULT(switchboard.addr.port, 0);
			if (parse_config_vals.parsed.switchboard.host.len) {
				si->hostname = parse_config_vals.parsed.switchboard.host;
			}
			else {
				si->hostname = parse_config_vals.dflt.switchboard.host;
			}
			if (!si->hostname.len) {
				si->hostname.s = iptrtpproxy_cfg_hostname;
				si->hostname.len = strlen(si->hostname.s);
			}
			si->switchboard_addr = parse_config_vals.parsed.switchboard.addr;
		}
		else {
			struct host_item *hi;
			hi = pkg_malloc(sizeof(*hi));
			if (!hi) goto out_of_mem;
			memset(hi, 0, sizeof(*hi));
			hi->name = parse_config_vals.name;
			hi->next = (*prev_hi);
			(*prev_hi) = hi;
			PROC_DEFAULT(host.addr, 0);
			PROC_DEFAULT(host.port, 0);
			PROC_DEFAULT(host.request_size, XT_RTPPROXY_RPC_DEFAULT_REQUEST_SIZE);
			PROC_DEFAULT(host.reply_size, XT_RTPPROXY_RPC_DEFAULT_REPLY_SIZE);
			PROC_DEFAULT(host.total_timeout, XT_RTPPROXY_RPC_DEFAULT_TOTAL_TIMEOUT);
			PROC_DEFAULT(host.udp_retry_timeout, XT_RTPPROXY_RPC_DEFAULT_UDP_REPLY_TIMEOUT);
			hi->rpc_params = parse_config_vals.parsed.host;
		}
	}
local:
	memset(&parse_config_vals.parsed, 0, sizeof(parse_config_vals.parsed));
	memset(&parse_config_vals.name, 0, sizeof(parse_config_vals.name));
	return 0;
out_of_mem:
	ERR(MODULE_NAME": safe_parsed_values: not enough pkg memory\n");
	return -1;
}

static cfg_option_t section_switchboard_options[] = {
/* 1st pass */
	{NULL, .flags = CFG_DEFAULT, .f = cfg_parse_default},
/* 2nd pass */
	{"addr", .f = cfg_parse_addr, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.switchboard.addr.ip},
	{"port", .f = cfg_parse_uint16, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.switchboard.addr.port},
	{"host", .f = cfg_parse_str, .flags = CFG_CASE_SENSITIVE|CFG_STR_PKGMEM, .param = &parse_config_vals.parsed.switchboard.host},
	{NULL, .flags = CFG_DEFAULT, .f = cfg_parse_default}
};

static cfg_option_t protos[] = {
	{"udp", .param = &parse_config_vals.parsed.host.proto, .val = xt_rtpproxy_connection_UDP},
	{"tcp", .param = &parse_config_vals.parsed.host.proto, .val = xt_rtpproxy_connection_TCP},
	{NULL, .flags = 0}
};

static cfg_option_t section_host_options[] = {
/* 1st pass */
	{NULL, .flags = CFG_DEFAULT, .f = cfg_parse_default},
/* 2nd pass */
	{"rpc-addr", .f = cfg_parse_addr, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.host.addr},
	{"rpc-port", .f = cfg_parse_uint16, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.host.port},
	{"rpc-proto", .f = cfg_parse_enum, .flags = CFG_CASE_SENSITIVE, .param = protos},
	{"rpc-request-size", .f = cfg_parse_int, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.host.request_size},
	{"rpc-reply-size", .f = cfg_parse_int, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.host.reply_size},
	{"rpc-total-timeout", .f = cfg_parse_int, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.host.total_timeout},
	{"rpc-udp-retry-timeout", .f = cfg_parse_int, .flags = CFG_CASE_SENSITIVE, .param = &parse_config_vals.parsed.host.udp_retry_timeout},
	{NULL, .flags = CFG_DEFAULT, .f = cfg_parse_default}
};

static cfg_option_t section_dummy_options[] = {
/* 1st pass */
	{NULL, .flags = CFG_DEFAULT, .f = cfg_parse_default},
/* 2nd pass */
	{NULL, .flags = CFG_DEFAULT, .f = cfg_parse_default}
};

#define DEFAULT_SECTION "default"
#define SWITCHBOARD_PREFIX "switchboard"
#define HOST_PREFIX "host"

static int parse_section_name(void* param, cfg_parser_t* st, unsigned int flags) {
	cfg_token_t t;
	int ret, fl;
	ret = safe_parsed_values();
	if (ret != 0) return ret;

	cfg_set_options(st, section_dummy_options);

	ret = cfg_get_token(&t, st, 0);
	if (ret != 0) return ret;
	if (t.type != CFG_TOKEN_ALPHA) 
		goto skip;
	if (t.val.len == (sizeof(DEFAULT_SECTION)-1) && strncmp(t.val.s, DEFAULT_SECTION, t.val.len) == 0) 
		fl = iptrtpproxy_default;
	else if (t.val.len == (sizeof(SWITCHBOARD_PREFIX)-1) && strncmp(t.val.s, SWITCHBOARD_PREFIX, t.val.len) == 0) {
		fl = iptrtpproxy_switchboard;
	} else if (t.val.len == (sizeof(HOST_PREFIX)-1) && strncmp(t.val.s, HOST_PREFIX, t.val.len) == 0) { 
		fl = iptrtpproxy_host;
	}
	else
		goto skip;
	ret = cfg_get_token(&t, st, 0);
	if (ret != 0) return ret;
	if (t.type != ':') 
		goto skip;
	ret = cfg_parse_section(&parse_config_vals.name, st, CFG_STR_PKGMEM);
	if (ret != 0) return ret;

	if (fl==iptrtpproxy_default) {
		if (parse_config_vals.name.len == (sizeof(SWITCHBOARD_PREFIX)-1) && strncmp(parse_config_vals.name.s, SWITCHBOARD_PREFIX, parse_config_vals.name.len) == 0) {
			fl |= iptrtpproxy_switchboard;
		}
		else if (parse_config_vals.name.len == (sizeof(HOST_PREFIX)-1) && strncmp(parse_config_vals.name.s, HOST_PREFIX, parse_config_vals.name.len) == 0) {
			fl |= iptrtpproxy_host;
		}
		else {
			goto skip;
		}
		if (parse_config_vals.name.s) {
			pkg_free(parse_config_vals.name.s);
			parse_config_vals.name.s = NULL;
		}
	}
	cfg_set_options(st, section_dummy_options);
	if (fl) {
		if (fl & iptrtpproxy_switchboard) {
			cfg_set_options(st, section_switchboard_options);
		} else if (fl & iptrtpproxy_host) {
			cfg_set_options(st, section_host_options);
		}
		parse_config_vals.flag = fl;
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
	static char buf[HOST_NAME_MAX+1];
	struct switchboard_item *si;
	if (!iptrtpproxy_cfg_hostname || !strlen(iptrtpproxy_cfg_hostname)) {
		if (gethostname(buf, sizeof(buf)-1) < 0) {
			ERR(MODULE_NAME"parse_iptrtpproxy_cfg: gethostname error\n");
			return E_CFG;
		}
		iptrtpproxy_cfg_hostname = buf;
	}
	hosts = pkg_malloc(sizeof(*hosts));
	if (!hosts) return -1;
	memset(hosts, 0, sizeof(*hosts));
	hosts->name.s = iptrtpproxy_cfg_hostname;
	hosts->name.len = strlen(hosts->name.s);
	hosts->local = 1;
	
	if ((parser = cfg_parser_init(0, &iptrtpproxy_cfg_filename)) == NULL) {
		ERR(MODULE_NAME"parse_iptrtpproxy_cfg: Error while initializing configuration file parser.\n");
		return -1;
        }
	cfg_section_parser(parser, parse_section_name, NULL);
	memset(&parse_config_vals, 0, sizeof(parse_config_vals));
	if (sr_cfg_parse(parser)) {
		return -1;
	}
	cfg_parser_close(parser);
	if (safe_parsed_values() < 0) {
		return -1;
	}
	for (si = switchboards; si; si = si->next) {
		si->host = find_host(&si->hostname, NULL);
		if (!si->host) {
			ERR(MODULE_NAME"parse_iptrtpproxy_cfg: host '%.*s' not found.\n", STR_FMT(&si->hostname));
			return -1;
		}
		si->sip_ip = si->switchboard_addr.ip;
	}
	return 0;
}

static struct {
	char *name;
	int payload_type;
	unsigned int media_type;
	int clock_rate;
	int channels;
} def_codecs [] = {
	{"PCMU", 0, 1<<sdpmtAudio, 8000, 1},
	{"GSM", 3, 1<<sdpmtAudio, 8000, 1},
	{"G723", 4, 1<<sdpmtAudio, 8000, 1},
	{"DVI4", 5, 1<<sdpmtAudio, 8000, 1},
	{"DVI4", 6, 1<<sdpmtAudio, 16000, 1},
	{"LPC", 7, 1<<sdpmtAudio, 8000, 1},
	{"PCMA", 8, 1<<sdpmtAudio, 8000, 1},
	{"G722", 9, 1<<sdpmtAudio, 8000, 1},
	{"L16", 10, 1<<sdpmtAudio, 44100, 2},
	{"L16", 11, 1<<sdpmtAudio, 44100, 1},
	{"QCELP", 12, 1<<sdpmtAudio, 8000, 1},
	{"CN", 13, 1<<sdpmtAudio, 8000, 1},
	{"MPA", 14, 1<<sdpmtAudio, 90000},
	{"G728", 15, 1<<sdpmtAudio, 8000, 1},
	{"DVI4", 16, 1<<sdpmtAudio, 11025, 1},
	{"DVI4", 17, 1<<sdpmtAudio, 22050, 1},
	{"G729", 18, 1<<sdpmtAudio, 8000, 1},
	{"CelB", 25, 1<<sdpmtVideo, 90000},
	{"JPEG", 26, 1<<sdpmtVideo, 90000},
	{"nv", 28, 1<<sdpmtVideo, 90000},
	{"H261", 31, 1<<sdpmtVideo, 90000},
	{"MPV", 32, 1<<sdpmtVideo, 90000},
	{"MP2T", 33, 1<<sdpmtAudio | 1<<sdpmtVideo, 90000},
	{"H263", 34, 1<<sdpmtVideo, 90000},
	{"telephone-event", -1, 1<<sdpmtAudio},
	{"tone", -1, 1<<sdpmtAudio},
	{"red", -1, 1<<sdpmtAudio|1<<sdpmtText},
	{"rtx", -1, 1<<sdpmtAudio|1<<sdpmtVideo|1<<sdpmtApplication|1<<sdpmtText},
	{"parityfec", -1, 1<<sdpmtAudio|1<<sdpmtVideo|1<<sdpmtApplication|1<<sdpmtText},
	{"t140c", -1, 1<<sdpmtAudio},
	{"t38", -1, 1<<sdpmtAudio},
	{"AMR", -1, 1<<sdpmtAudio, 8000},
	{"AMR-WB", -1, 1<<sdpmtAudio, 16000},
	{"L8", -1, 1<<sdpmtAudio},
	{"L20", -1, 1<<sdpmtAudio},
	{"L24", -1, 1<<sdpmtAudio},
	{"DAT12", -1, 1<<sdpmtAudio},
	
	{"raw", -1, 1<<sdpmtVideo},
	{"pointer", -1, 1<<sdpmtVideo},
	/* etc.*/	

	{NULL}
};

static int declare_def_codecs() {
	int codec_id, i;
	i = 0;
	while (def_codecs[i].name) {
		str s;
		s.s = def_codecs[i].name;
		s.len = strlen(s.s);
  		codec_id = register_codec(&s);
		if (codec_id < 0) return codec_id;
		i++;
	}
	return 0;
}

/* module initialization */
static int mod_init(void) {
	struct switchboard_item *si;
	struct aggregation_item *ai;
	struct host_item *hi;
	struct codec_set_item *ci;
	int i;
	if (iptrtpproxy_cfg_flag <= 1) {
		if (parse_iptrtpproxy_cfg() < 0)
			return E_CFG;
	}

	for (si = switchboards; si; si=si->next) {
		str ips[2];
		char buf1[17];
		si->stat = shm_malloc(sizeof(*si->stat));
		if (!si->stat) return E_OUT_OF_MEM;
		memset(si->stat, 0, sizeof(*si->stat));

		ip42s(si->switchboard_addr.ip, ips+0);
		strncpy(buf1, ips[0].s, sizeof(buf1)-1);
		ips[0].s = buf1;
		ip42s(si->sip_ip, ips+1);

		INFO(MODULE_NAME": mod_init: switchboard_name=%.*s;addr=%.*s;port=%d;sip-addr=%.*s;hostname=%.*s\n", 
			STR_FMT(&si->name),
			STR_FMT(ips+0),
			si->switchboard_addr.port,
			STR_FMT(ips+1),
			STR_FMT(&si->hostname)
		);

	}
	for (ai = aggregations; ai; ai=ai->next) {
		str ips[1];
		ip42s(ai->sip_ip, ips+0);
		INFO(MODULE_NAME": mod_init: aggregation '%.*s';sip-addr=%.*s\n", 
			STR_FMT(&ai->name),
			STR_FMT(ips+0)
		);
		for (i=0; i<ai->switchboard_count; i++) {
			ERR(MODULE_NAME": mod_init:   '%.*s'\n", STR_FMT(&(*ai->switchboards)[i]->name));
		}
	}
	for (hi = hosts; hi; hi=hi->next) {
		str ips;
		hi->stat = shm_malloc(sizeof(*hi->stat));
		if (!hi->stat) return E_OUT_OF_MEM;
		memset(hi->stat, 0, sizeof(*hi->stat));

		ip42s(hi->rpc_params.addr, &ips);
		INFO(MODULE_NAME": mod_init: host_name=%.*s;rpc-addr=%.*s;rpc-port=%d;rpc-proto=%d,request-size=%d,reply-size=%d,total-timeout=%d,udp-retry-timeout=%d\n", 
			STR_FMT(&hi->name),
			STR_FMT(&ips),
			hi->rpc_params.port,
			hi->rpc_params.proto,
			hi->rpc_params.request_size,
			hi->rpc_params.reply_size,
			hi->rpc_params.total_timeout,
			hi->rpc_params.udp_retry_timeout
		); 
	}

		if (!reg_codecs) {
			int r;
			if ((r = declare_def_codecs()) < 0) {
				return r;
			}
	}
	memset(&fixed_payload_types, 0, sizeof(fixed_payload_types));
	i = 0;
	while (def_codecs[i].name && def_codecs[i].payload_type >= 0) {
		str s;
		int codec_id;
		s.s = def_codecs[i].name;
		s.len = strlen(s.s);		
  		codec_id = name2codec_id(&s, NULL);
		if (!codec_id) {
			BUG(MODULE_NAME":  mod_init: def.codec '%s' not found\n", s.s);
			return -1;
		}
		fixed_payload_types[def_codecs[i].payload_type].codec_id = codec_id;
		i++;
	}
#if 0
	for (i=0; i<MAX_FIXED_PAYLOAD_TYPES; i++) {
		if (fixed_payload_types[i].codec_id) {
			INFO(MODULE_NAME": mod_init: payload_type=%d, codec_name='%.*s'\n", i, STR_FMT(&(*reg_codecs)[fixed_payload_types[i].codec_id-1].name));
		}
	}
	for (i=0; i<reg_codec_count; i++) {
		INFO(MODULE_NAME": mod_init: codec_id=%d, codec_name='%.*s'\n", i+1, STR_FMT(&(*reg_codecs)[i].name));
	}
#endif
	for (ci = codec_sets; ci; ci=ci->next) {
		int media_type;
		INFO(MODULE_NAME": mod_init: codec_set='%.*s'\n", STR_FMT(&ci->name));
		for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
			INFO(MODULE_NAME": mod_init:   media_type='%.*s';max_streams=%d;rtp_bytes=%d;rtcp_bytes=%d;rtp_packets=%d;rtcp_packets=%d\n", 
				STR_FMT(sdp_media_types_str+media_type),
				ci->media_types[media_type].throttle.max_streams,
				ci->media_types[media_type].throttle.bandwidth[0].bytes,
				ci->media_types[media_type].throttle.bandwidth[1].bytes,
				ci->media_types[media_type].throttle.bandwidth[0].packets,
				ci->media_types[media_type].throttle.bandwidth[1].packets
			);
			if (ci->media_types[media_type].throttle.max_streams != 0) {
				for (i=0; i<reg_codec_count; i++) {
					if ((*ci->media_types[media_type].codec_rights)[i] > 0) {
						if (i > 0)
							INFO(MODULE_NAME": mod_init:     codec='%.*s', right=%d\n", STR_FMT(&(*reg_codecs)[i-1].name), (*ci->media_types[media_type].codec_rights)[i]);
						else
							INFO(MODULE_NAME": mod_init:     codec='?', right=%d\n", (*ci->media_types[media_type].codec_rights)[i]);
					}
				}
			}
		}
	}
	/*register_script_cb(mod_pre_script_cb, REQ_TYPE_CB | RPL_TYPE_CB| PRE_SCRIPT_CB, 0);*/
	register_script_cb(mod_pre_script_cb, REQUEST_CB | ONREPLY_CB| PRE_SCRIPT_CB, 0);
	register_select_table(sel_declaration);
	return 0;
}

static void mod_cleanup(void) {
	struct host_item *hi;
	struct switchboard_item *si;
	for (si = switchboards; si; si = si->next) {
		if (si->stat) {
			shm_free(si->stat);
			si->stat = NULL;
		}
	}
	for (hi = hosts; hi; hi = hi->next) {
		if (hi->handle_is_opened) {
			xt_RTPPROXY_close(&hi->handle);
			hi->handle_is_opened = 0;
		}
		if (hi->stat) {
			shm_free(hi->stat);
			hi->stat = NULL;
		}
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
	if (iptrtpproxy_cfg_flag <= 1) {
		iptrtpproxy_cfg_flag = 2;
		iptrtpproxy_cfg_filename = * (str*) val;
		if (parse_iptrtpproxy_cfg() == 0)
			return 0;
	}
	else {		
		switch (iptrtpproxy_cfg_flag) {
			case 2:
				ERR(MODULE_NAME": declare_config: 'config' param may be used only once\n");
				break;
			case 3:
				ERR(MODULE_NAME": declare_config: 'config' param may not be used after 'switchboard'\n");
				break;
			default:
				BUG(MODULE_NAME": declare_config: unexpected 'iptrtpproxy_cfg_flag' value %d\n", iptrtpproxy_cfg_flag);
		}				
	}
	return E_CFG;
}

static int declare_hostname(modparam_t type, void* val) {
	if (!val) return 0;
	if (iptrtpproxy_cfg_flag == 0) {
		iptrtpproxy_cfg_hostname = (char*) val;
		return 0;
	}
	else {
		switch (iptrtpproxy_cfg_flag) {
			case 1:
				ERR(MODULE_NAME": declare_hostname: 'hostname' param may be used only once\n");
				break;
			case 2:
			case 3:
				ERR(MODULE_NAME": declare_hostname: 'hostname' param may not be used after 'switchboard' or 'config'\n");
				break;
			default:
				BUG(MODULE_NAME": declare_hostname: unexpected 'iptrtpproxy_cfg_flag' value %d\n", iptrtpproxy_cfg_flag);
		}				
	}
	return E_CFG;
}

struct parse_param_item {
	char *name;
	unsigned int id;
};

/* returns index to items, -1 if error or param not found */
static int parse_next_param(char **s, struct parse_param_item (*params)[], str *val) {
		str p;
		int i;
		char *c;

		eat_spaces(*s);
		c = *s;
		while ( is_alpha(*c) ) {
			c++;
		}
		if (c == *s) {
			ERR(MODULE_NAME": parse_next_param: param name expected near '%s'\n", *s);
			return -1;
		}
		p.s = *s;
		p.len = c-*s;
		eat_spaces(c);
		*s = c;
		if (*c != '=') {
			ERR(MODULE_NAME": parse_next_param: equal char expected near '%s'\n", *s);
			return -1;
		}
		c++;
		eat_spaces(c);
		*s = c;
		while (*c && *c != ';') c++;
		val->s = *s;
		val->len = c-*s;
		while (val->len > 0 && val->s[val->len-1]<=' ') val->len--;
		if (*c) c++;
		eat_spaces(c);
		*s = c;
		for (i=0; (*params)[i].name; i++) {
			if (strlen((*params)[i].name)==p.len && strncasecmp((*params)[i].name, p.s, p.len) == 0) {
				return i;
			}
		}
		ERR(MODULE_NAME": parse_next_param: unknown param name '%.*s'\n", STR_FMT(&p));
		return -1;
}

static int declare_switchboard_param(modparam_t type, void* val) {

	char *s;
	int all_flag;
	struct switchboard_item *si = NULL;
	enum param_id {
		par_Name =		0x000001,
		par_Aggregation =	0x000002,
		par_SipAddr =		0x000004
	};
	static struct parse_param_item params[] = {
		{.name = "name", .id = par_Name},
		{.name = "sip-addr", .id = par_SipAddr},
		{.name = "aggregation", .id = par_Aggregation},

		{.name = 0, .id = 0}
	};

	if (!val) return 0;
	if (iptrtpproxy_cfg_flag <= 1) {
		iptrtpproxy_cfg_flag = 3;
		if (parse_iptrtpproxy_cfg() < 0)
			return E_CFG;
	}

	s = val;
	all_flag = -1;

	eat_spaces(s);
	if (!*s) return 0;
	/* parse param: name=;aggregation=;sip-addr= */
	while (*s) {
		str val;
		int idx;

		idx = parse_next_param(&s, &params, &val);
		if (idx < 0) goto err_E_CFG;
		
		if (all_flag >= 0 && params[idx].id == par_Name) {
			ERR(MODULE_NAME": declare_switchboard_param: name must be the first param\n");
			goto err_E_CFG;
		}
		if (params[idx].id == par_Name) {
			all_flag = 0;
			si = find_switchboard(&val, NULL);
			if (!si) {
				if (val.len == 1 && val.s[0] == '*')
					all_flag = 1;
				else {
					ERR(MODULE_NAME": declare_switchboard_param: switchboard '%.*s' not found\n", STR_FMT(&val));
					goto err_E_CFG;
				}
			}
		}
		else {
			if (all_flag)
				si = switchboards;
			while (si) {

				switch (params[idx].id) {
					case par_Name:
						break;
					case par_Aggregation: {
						struct aggregation_item *ai;
						struct aggregation_item **prev_ai;
						int i;
						ai = find_aggregation(&val, &prev_ai);
						if (!ai) {
							ai = pkg_malloc(sizeof(*ai));
							if (!ai) return E_OUT_OF_MEM;
							memset(ai, 0, sizeof(*ai));
							ai->name = val;
							ai->next = (*prev_ai);
							(*prev_ai) = ai;
						}
						for (i=0; i<ai->switchboard_count; i++) {
							if ((*ai->switchboards)[i] == si) goto aggr_found;
						}
						if (!ai->sip_ip) {
							ai->sip_ip = si->sip_ip;
						}
						ai->switchboards = pkg_realloc(ai->switchboards, sizeof((*ai->switchboards)[0])*(ai->switchboard_count+1));
						if (!ai->switchboards) return E_OUT_OF_MEM;
						(*ai->switchboards)[ai->switchboard_count] = si;
						ai->switchboard_count++;
					aggr_found:
						break;
					}
					case par_SipAddr:
						si->sip_ip = s2ip4(&val);
						if (si->sip_ip == 0) {
							goto err_E_CFG;
						}
						break;	

					default:
						BUG(MODULE_NAME": declare_switchboard_param: unknown id '%x\n", idx);
						goto err_E_CFG;
				}
				if (!all_flag) break;
				si = si->next;
			}
		}
	}
	if (all_flag) {
		return 0;
	}

	switchboard_count++;

	return 0;

err_E_CFG:
	ERR(MODULE_NAME": declare_switchboard_param(#%d): parse error near \"%s\"\n", switchboard_count, s);

	return E_CFG;
}

static int declare_codec(modparam_t type, void* val) {
	int r;
	str *s;
	s = val;
	if (!s || !s->len) return 0;
	if (codec_sets) {
		ERR(MODULE_NAME": declare_codec: codec declaration cannot follow codec set declaration\n");
		return E_CFG;
	}

	if (!reg_codecs) {
		int r;
		if ((r = declare_def_codecs()) < 0) {
			return r;
		}
	}
	r = register_codec(s);
	if (r < 0) return r;
	return 0;
}

static int declare_codec_set(modparam_t type, void* val) {
	char *s;
	unsigned int cur_rights = 0;
	unsigned int cur_media_type = 0;
	struct codec_set_item *ci = NULL;
	enum param_id {
		par_Name =		          0x000001,
		par_Rights =	          0x000002,
		par_Codecs =	          0x000003,
		par_MediaType =           0x000004,		
		par_MaxStreams =          0x000005,
		par_ThrottleRTPBytes =    0x000100,
		par_ThrottleRTCPBytes =   0x000101,
		par_ThrottleRTPPackets =  0x000200,
		par_ThrottleRTCPPackets = 0x000201
	};
	static struct parse_param_item params[] = {
		{.name = "name", .id = par_Name},
		{.name = "media_type", .id = par_MediaType},
		{.name = "rights", .id = par_Rights},
		{.name = "codecs", .id = par_Codecs},
		{.name = "max_streams", .id = par_MaxStreams},
		{.name = "rtp_bytes", .id = par_ThrottleRTPBytes},
		{.name = "rtcp_bytes", .id = par_ThrottleRTCPBytes},
		{.name = "rtp_packets", .id = par_ThrottleRTPPackets},
		{.name = "rtcp_packets", .id = par_ThrottleRTCPPackets},

		{.name = 0, .id = 0}
	};

	if (!val) return 0;

	if (!reg_codecs) {
		int r;
		if ((r = declare_def_codecs()) < 0) {
			return r;
		}
	}
	s = val;
	eat_spaces(s);
	if (!*s) return 0;
	/* parse param: name=;media_type=audio,video;rights=<int>;codecs=<codec1>,<codec2>,..;max_streams=2 */
	while (*s) {
		str val;
		int idx, media_type;
		char *p, *pend;
		struct codec_set_item **prev_ci;

		idx = parse_next_param(&s, &params, &val);
		if (idx < 0) return E_CFG;

		if (params[idx].id != par_Name && !ci) {
			ERR(MODULE_NAME": declare_codec_set: name must be the first param\n");
			return E_CFG;
		}

		switch (params[idx].id) {
			case par_Name:
				ci = find_codec_set(&val, &prev_ci);
				if (!ci) {
					int media_type;
					ci = pkg_malloc(sizeof(*ci));
					if (!ci) return E_OUT_OF_MEM;
					memset(ci, 0, sizeof(*ci));
					for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
						ci->media_types[media_type].codec_rights = pkg_malloc(sizeof((*ci->media_types[media_type].codec_rights)[0])*(reg_codec_count+1));
						if (!ci->media_types[media_type].codec_rights) return E_OUT_OF_MEM;
						memset(ci->media_types[media_type].codec_rights, 0, sizeof((*ci->media_types[media_type].codec_rights)[0])*(reg_codec_count+1));
						ci->media_types[media_type].throttle.max_streams = -1;
					}					
					ci->name = val;
					ci->next = (*prev_ci);					
					(*prev_ci) = ci;
				}
				break;
			case par_MediaType:
				cur_media_type = 0;
				if (val.len == 1 && val.s[0] == '*') {
					for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
						cur_media_type |= 1 << media_type;
					}
				}
				else {
					p = val.s;
					pend = p + val.len;
					while (p < pend) {
						str s2;
						s2.s = p;
						while (p < pend && is_alpha(*p)) p++;
						s2.len = p - s2.s;
						cur_media_type |= 1 << name2media_type(&s2);
						while (p < pend && !is_alpha(*p)) p++;
					}
				}
				break;
			case par_Rights:
				val.s[val.len] = '\0'; /* we need not save value, it's already parsed */
				cur_rights = atol(val.s);
				break;
			case par_MaxStreams:
				val.s[val.len] = '\0'; /* we need not save value, it's already parsed */
				for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
					if (cur_media_type & (1<<media_type)) {
						ci->media_types[media_type].throttle.max_streams = atol(val.s);
					}
				}
				break;
			case par_ThrottleRTPBytes:
			case par_ThrottleRTCPBytes:
				val.s[val.len] = '\0'; /* we need not save value, it's already parsed */
				for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
					if (cur_media_type & (1<<media_type)) {
						ci->media_types[media_type].throttle.bandwidth[params[idx].id&1].bytes = atol(val.s);
					}
				}
				break;
			case par_ThrottleRTPPackets:
			case par_ThrottleRTCPPackets:
				val.s[val.len] = '\0'; /* we need not save value, it's already parsed */
				for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
					if (cur_media_type & (1<<media_type)) {
						ci->media_types[media_type].throttle.bandwidth[params[idx].id&1].packets = atol(val.s);
					}
				}
				break;
			case par_Codecs:
				if (val.len == 1 && val.s[0] == '*') {
					int codec_id;
					for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
						if (cur_media_type & (1<<media_type)) {
							for (codec_id=0; codec_id<reg_codec_count; codec_id++) {
								(*ci->media_types[media_type].codec_rights)[codec_id] = cur_rights;
							}
						}
					}
				}
				else {
					p = val.s;
					pend = p + val.len;
					while (p < pend) {
						str s2;
						s2.s = p;
						while (p < pend && is_alpha(*p)) p++;
						s2.len = p - s2.s;
						for (media_type=0; media_type<NUM_MEDIA_TYPES; media_type++) {
							if (cur_media_type & (1<<media_type)) {
								(*ci->media_types[media_type].codec_rights)[name2codec_id(&s2, NULL)] = cur_rights;
							}
						}
						while (p < pend && !is_alpha(*p)) p++;
					}
				}
				break;
			default:
				BUG(MODULE_NAME": declare_codec_set: unknown id '%x\n", params[idx].id);
				return E_CFG;
		}

	}

	return 0;
}

static cmd_export_t cmds[] = {
	{MODULE_NAME "_alloc",     rtpproxy_alloc,         1, rtpproxy_alloc_update_fixup,       REQUEST_ROUTE | ONREPLY_ROUTE },
	{MODULE_NAME "_update",    rtpproxy_update,        2, rtpproxy_alloc_update_fixup,      REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{MODULE_NAME "_adjust_timeout", rtpproxy_adjust_timeout, 2, rtpproxy_alloc_update_fixup,      REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{MODULE_NAME "_delete",    rtpproxy_delete,        1, rtpproxy_delete_fixup,      REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{MODULE_NAME "_set_param", rtpproxy_set_param,     2, rtpproxy_set_param_fixup,   REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE },
	{MODULE_NAME "_authorize_media", rtpproxy_authorize_media, 0, NULL,       REQUEST_ROUTE | ONREPLY_ROUTE },

	{0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"config",                PARAM_STR | PARAM_USE_FUNC, &declare_config}, 
	{"switchboard",           PARAM_STRING | PARAM_USE_FUNC, &declare_switchboard_param},
	{"hostname",              PARAM_STRING | PARAM_USE_FUNC, &declare_hostname},
	{"codec_set",             PARAM_STRING | PARAM_USE_FUNC, &declare_codec_set},
	{"rpc_heartbeat_timeout", PARAM_INT, &rpc_heartbeat_timeout},
	{"declare_codec",         PARAM_STR | PARAM_USE_FUNC, &declare_codec}, 
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
#ifdef IPT_RTPPROXY_IPTABLES_API
#include <iptables.h>
#undef IPT_RTPPROXY_IPTABLES_API
#else
#include <xtables.h>
#endif

#include <stdarg.h>

#ifdef xtables_error
/* iptables 1.4.8 */

struct xtables_globals *xt_params = NULL;
int xtables_check_inverse(const char option[], int *invert, int *optind, int argc, char **argv) {
        return FALSE;
}
void xtables_register_target(struct xtables_target *me) {
}

#else  /* xtables_error */

#ifdef _IPTABLES_COMMON_H
/* old iptables API, it uses iptables_common.h (instead of xtables.h) included from iptables.h */
/* #ifndef XTABLES_VERSION ... optional test */
#define IPT_RTPPROXY_IPTABLES_API 1
#endif

#ifdef IPT_RTPPROXY_IPTABLES_API
void register_target(struct iptables_target *me) {
}
#else
void xtables_register_target(struct xtables_target *me) {
}
#endif

#if IPT_RTPPROXY_IPTABLES_API
void exit_error(enum exittype status, char *msg, ...)
#else
void exit_error(enum exittype status, const char *msg, ...)
#endif
{
	va_list args;
	
	va_start(args, msg);
//	ERR(msg/*, args*/);  /* TODO: how to pass ... to macro? */
	ERR(MODULE_NAME": %s", msg);
	va_end(args);
}

int check_inverse(const char option[], int *invert, int *optind, int argc) {
	return 0;
}
#endif  /* xtables_error */

#endif


