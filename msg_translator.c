/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 *
 */
/** Via special params:
 * requests:
 * - if the address in via is different from the src_ip or an existing
 *   received=something is found, received=src_ip is added (and any preexisting
 *   received is deleted). received is added as the first via parameter if no
 *   receive is previously present or over the old receive.
 * - if the original via contains rport / rport=something or msg->msg_flags
 *   FL_FORCE_RPORT is set (e.g. script force_rport() cmd) rport=src_port
 *   is added (over previous rport / as first via param or after received
 *   if no received was present and received is added too)
 * local replies:
 *    (see also sl_send_reply)
 *  - rport and received are added in mostly the same way as for requests, but
 *    in the reverse order (first rport and then received). See also
 *    limitations.
 *  - if reply_to_via is set (default off) the local reply will be sent to
 *    the address in via (received is ignored since it was not set by us). The
 *    destination port is either the message source port if via contains rport
 *    or the FL_FORCE_RPORT flag is set or the port from the via. If either
 *    port or rport are present a normal dns lookup (instead of a srv lookup)
 *    is performed on the address. If no port is present and a srv lookup is
 *    performed the port is taken from the srv lookup. If the srv lookup failed
 *    or it was not performed, the port is set to the default sip port (5060).
 *  - if reply_to_via is off (default) the local reply is sent to the message
 *    source ip address. The destination port is set to the source port if
 *    rport is present or FL_FORCE_RPORT flag is set, to the via port or to
 *    the default sip port (5060) if neither rport or via port are present.
 * "normal" replies:
 *  - if received is present the message is sent to the received address else
 *    if no port is present (neither a normal via port or rport) a dns srv
 *    lookup is performed on the host part and the reply is sent to the
 *    resulting ip. If a port is present or the host part is an ip address
 *    the dns lookup will be a "normal" one (A or AAAA).
 *  - if rport is present, it's value will be used as the destination port
 *   (and this will also disable srv lookups)
 *  - if no port is present the destination port will be taken from the srv
 *    lookup. If the srv lookup fails or is not performed (e.g. ip address
 *    in host) the destination port will be set to the default sip port (5060).
 *
 * Known limitations:
 * - when locally replying to a message, rport and received will be appended to
 *   the via header parameters (for forwarded requests they are inserted at the
 *   beginning).
 * - a locally generated reply might get two received via parameters if a
 *   received is already present in the original message (this should not
 *   happen though, but ...)
 *
 *--andrei
*/

/*!
 * \file
 * \brief Kamailio core :: Message translations
 * \ingroup core
 * Module: \ref core
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "comp_defs.h"
#include "msg_translator.h"
#include "globals.h"
#include "error.h"
#include "mem/mem.h"
#include "dprint.h"
#include "config.h"
#include "md5utils.h"
#include "data_lump.h"
#include "data_lump_rpl.h"
#include "ip_addr.h"
#include "resolve.h"
#include "ut.h"
#include "pt.h"
#include "cfg/cfg.h"
#include "parser/parse_to.h"
#include "parser/parse_param.h"
#include "forward.h"
#include "str_list.h"

#define append_str_trans(_dest,_src,_len,_msg) \
	append_str( (_dest), (_src), (_len) );

extern char version[];
extern int version_len;



/** per process fixup function for global_req_flags.
  * It should be called from the configuration framework.
  */
void fix_global_req_flags(str* gname, str* name)
{
	global_req_flags=0;
	switch(cfg_get(core, core_cfg, udp_mtu_try_proto)){
		case PROTO_NONE:
		case PROTO_UDP:
			/* do nothing */
			break;
		case PROTO_TCP:
			global_req_flags|=FL_MTU_TCP_FB;
			break;
		case PROTO_TLS:
			global_req_flags|=FL_MTU_TLS_FB;
			break;
		case PROTO_SCTP:
			global_req_flags|=FL_MTU_SCTP_FB;
			break;
	}
	if (cfg_get(core, core_cfg, force_rport))
		global_req_flags|=FL_FORCE_RPORT;
}



/* checks if ip is in host(name) and ?host(ip)=name?
 * ip must be in network byte order!
 *  resolver = DO_DNS | DO_REV_DNS; if 0 no dns check is made
 * return 0 if equal */
static int check_via_address(struct ip_addr* ip, str *name,
				unsigned short port, int resolver)
{
	struct hostent* he;
	int i;
	char* s;
	int len;
	char lproto;

	/* maybe we are lucky and name it's an ip */
	s=ip_addr2a(ip);
	if (s){
		LM_DBG("(%s, %.*s, %d)\n", s, name->len, name->s, resolver);

		len=strlen(s);

		/* check if name->s is an ipv6 address or an ipv6 address ref. */
		if ((ip->af==AF_INET6) &&
				(	((len==name->len)&&(strncasecmp(name->s, s, name->len)==0))
					||
					((len==(name->len-2))&&(name->s[0]=='[')&&
						(name->s[name->len-1]==']')&&
						(strncasecmp(name->s+1, s, len)==0))
				)
		   )
			return 0;
		else

			if (strncmp(name->s, s, name->len)==0)
				return 0;
	}else{
		LM_CRIT("could not convert ip address\n");
		return -1;
	}

	if (port==0) port=SIP_PORT;
	if (resolver&DO_DNS){
		LM_DBG("doing dns lookup\n");
		/* try all names ips */
		lproto = PROTO_NONE;
		he=sip_resolvehost(name, &port, &lproto); /* don't use naptr */
		if (he && ip->af==he->h_addrtype){
			for(i=0;he && he->h_addr_list[i];i++){
				if ( memcmp(&he->h_addr_list[i], ip->u.addr, ip->len)==0)
					return 0;
			}
		}
	}
	if (resolver&DO_REV_DNS){
		LM_DBG("doing rev. dns lookup\n");
		/* try reverse dns */
		he=rev_resolvehost(ip);
		if (he && (strncmp(he->h_name, name->s, name->len)==0))
			return 0;
		for (i=0; he && he->h_aliases[i];i++){
			if (strncmp(he->h_aliases[i],name->s, name->len)==0)
				return 0;
		}
	}
	return -1;
}


/* check if IP address in Via != source IP address of signaling,
 * or the sender requires adding rport or received values */
int received_test( struct sip_msg *msg )
{
	int rcvd;

	rcvd=msg->via1->received || msg->via1->rport
			|| check_via_address(&msg->rcv.src_ip, &msg->via1->host,
							msg->via1->port, received_dns);
	return rcvd;
}

/* check if IP address in Via != source IP address of signaling */
int received_via_test( struct sip_msg *msg )
{
	int rcvd;

	rcvd = (check_via_address(&msg->rcv.src_ip, &msg->via1->host,
							msg->via1->port, received_dns)!=0);
	return rcvd;
}

static char * warning_builder( struct sip_msg *msg, unsigned int *returned_len)
{
	static char buf[MAX_WARNING_LEN];
	str *foo;
	int print_len, l;
	int clen;
	char* t;

#define str_print(string, string_len) \
		do{ \
			l=(string_len); \
			if ((clen+l)>MAX_WARNING_LEN) \
				goto error_overflow; \
			memcpy(buf+clen, (string), l); \
			clen+=l; \
		}while(0)

#define str_lenpair_print(string, string_len, string2, string2_len) \
		do{ \
			str_print(string, string_len); \
			str_print(string2, string2_len);\
		}while(0)

#define str_pair_print( string, string2, string2_len) \
		str_lenpair_print((string), strlen((string)), (string2), (string2_len))

#define str_int_print(string, intval)\
		do{\
			t=int2str((intval), &print_len); \
			str_pair_print(string, t, print_len);\
		} while(0)

#define str_su_print(sockaddr)\
		do{\
			t=su2a(&sockaddr, sizeof(sockaddr)); \
			print_len=strlen(t); \
			str_print(t, print_len); \
		} while(0)

#define str_ipaddr_print(string, ipaddr_val)\
		do{\
			t=ip_addr2a((ipaddr_val)); \
			print_len=strlen(t); \
			str_pair_print(string, t, print_len);\
		} while(0)

	clen=0;
	str_print(WARNING, WARNING_LEN);
	str_su_print(msg->rcv.bind_address->su);
	str_print(WARNING_PHRASE, WARNING_PHRASE_LEN);

	/*adding out_uri*/
	if (msg->new_uri.s)
		foo=&(msg->new_uri);
	else
		foo=&(msg->first_line.u.request.uri);
	/* pid= */
	str_int_print(" pid=", my_pid());
	/* req_src_ip= */
	str_ipaddr_print(" req_src_ip=", &msg->rcv.src_ip);
	str_int_print(" req_src_port=", msg->rcv.src_port);
	str_pair_print(" in_uri=", msg->first_line.u.request.uri.s,
								msg->first_line.u.request.uri.len);
	str_pair_print(" out_uri=", foo->s, foo->len);
	str_pair_print(" via_cnt",
					(msg->parsed_flag & HDR_EOH_F)==HDR_EOH_F ? "=" : ">", 1);
	str_int_print("=", via_cnt);
	if (clen<MAX_WARNING_LEN){ buf[clen]='"'; clen++; }
	else goto error_overflow;


	*returned_len=clen;
	return buf;
error_overflow:
	LM_NOTICE("buffer size exceeded (probably too long URI)\n");
	*returned_len=0;
	return 0;
}




char* received_builder(struct sip_msg *msg, unsigned int *received_len)
{
	char *buf;
	int  len;
	struct ip_addr *source_ip;
	char *tmp;
	int  tmp_len;

	source_ip=&msg->rcv.src_ip;

	buf=pkg_malloc(sizeof(char)*MAX_RECEIVED_SIZE);
	if (buf==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memcpy(buf, RECEIVED, RECEIVED_LEN);
	if ( (tmp=ip_addr2a(source_ip))==0) {
		pkg_free(buf);
		return 0; /* error*/
	}
	tmp_len=strlen(tmp);
	len=RECEIVED_LEN+tmp_len;

	memcpy(buf+RECEIVED_LEN, tmp, tmp_len);
	buf[len]=0; /*null terminate it */

	*received_len = len;
	return buf;
}



char* rport_builder(struct sip_msg *msg, unsigned int *rport_len)
{
	char* buf;
	char* tmp;
	int tmp_len;
	int len;

	tmp_len=0;
	tmp=int2str(msg->rcv.src_port, &tmp_len);
	len=RPORT_LEN+tmp_len;
	buf=pkg_malloc(sizeof(char)*(len+1));/* space for null term */
	if (buf==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memcpy(buf, RPORT, RPORT_LEN);
	memcpy(buf+RPORT_LEN, tmp, tmp_len);
	buf[len]=0; /*null terminate it*/

	*rport_len=len;
	return buf;
}



char* id_builder(struct sip_msg* msg, unsigned int *id_len)
{
	char* buf;
	int len, value_len;
	char revhex[sizeof(int)*2];
	char* p;
	int size;

	size=sizeof(int)*2;
	p=&revhex[0];
	if (int2reverse_hex(&p, &size, msg->rcv.proto_reserved1)==-1){
		LM_CRIT("not enough space for id\n");
		return 0;
	}
	value_len=p-&revhex[0];
	len=ID_PARAM_LEN+value_len;
	buf=pkg_malloc(sizeof(char)*(len+1));/* place for ending \0 */
	if (buf==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memcpy(buf, ID_PARAM, ID_PARAM_LEN);
	memcpy(buf+ID_PARAM_LEN, revhex, value_len);
	buf[len]=0; /* null terminate it */
	*id_len=len;
	return buf;
}



char* clen_builder(	struct sip_msg* msg, int *clen_len, int diff, 
					int body_only)
{
	char* buf;
	int len;
	int value;
	char* value_s;
	int value_len;
	char* body;


	body=get_body(msg);
	if (body==0){
		ser_error=E_BAD_REQ;
		LM_ERR("no message body found (missing crlf?)");
		return 0;
	}
	value=msg->len-(int)(body-msg->buf)+diff;
	value_s=int2str(value, &value_len);
	LM_DBG("content-length: %d (%s)\n", value, value_s);

	if (body_only) {
		len=value_len;
	}
	else {
		len=CONTENT_LENGTH_LEN+value_len+CRLF_LEN;
	}
	buf=pkg_malloc(sizeof(char)*(len+1));
	if (buf==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	if (body_only) {
		memcpy(buf, value_s, value_len);
	}
	else {
		memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memcpy(buf+CONTENT_LENGTH_LEN, value_s, value_len);
		memcpy(buf+CONTENT_LENGTH_LEN+value_len, CRLF, CRLF_LEN);
	}
	buf[len]=0; /* null terminate it */
	*clen_len=len;
	return buf;
}



/* checks if a lump opt condition
 * returns 1 if cond is true, 0 if false */
static inline int lump_check_opt(	struct lump *l,
									struct sip_msg* msg,
									struct dest_info* snd_i
									)
{
	struct ip_addr* ip;
	unsigned short port;
	int proto;

#define get_ip_port_proto \
			if ((snd_i==0) || (snd_i->send_sock==0)){ \
				LM_CRIT("null send socket\n"); \
				return 1; /* we presume they are different :-) */ \
			} \
			if (msg->rcv.bind_address){ \
				ip=&msg->rcv.bind_address->address; \
				port=msg->rcv.bind_address->port_no; \
				proto=msg->rcv.bind_address->proto; \
			}else{ \
				ip=&msg->rcv.dst_ip; \
				port=msg->rcv.dst_port; \
				proto=msg->rcv.proto; \
			} \

	switch(l->u.cond){
		case COND_FALSE:
			return 0;
		case COND_TRUE:
			LUMP_SET_COND_TRUE(l);
			return 1;
		case COND_IF_DIFF_REALMS:
			get_ip_port_proto;
			/* faster tests first */
			if ((port==snd_i->send_sock->port_no) && 
					(proto==snd_i->send_sock->proto) &&
#ifdef USE_COMP
					(msg->rcv.comp==snd_i->comp) &&
#endif
					(ip_addr_cmp(ip, &snd_i->send_sock->address)))
				return 0;
			else {
				LUMP_SET_COND_TRUE(l);
				return 1;
			}
		case COND_IF_DIFF_AF:
			get_ip_port_proto;
			if (ip->af!=snd_i->send_sock->address.af) {
				LUMP_SET_COND_TRUE(l);
				return 1;
			} else return 0;
		case COND_IF_DIFF_PROTO:
			get_ip_port_proto;
			if (proto!=snd_i->send_sock->proto) {
				LUMP_SET_COND_TRUE(l);
				return 1;
			} else return 0;
		case COND_IF_DIFF_PORT:
			get_ip_port_proto;
			if (port!=snd_i->send_sock->port_no) {
				LUMP_SET_COND_TRUE(l);
				return 1;
			} else return 0;
		case COND_IF_DIFF_IP:
			get_ip_port_proto;
			if (ip_addr_cmp(ip, &snd_i->send_sock->address)) return 0;
			else {
				LUMP_SET_COND_TRUE(l);
				return 1;
			}
		case COND_IF_RAND:
			if(rand()>=RAND_MAX/2) {
				LUMP_SET_COND_TRUE(l);
				return 1;
			} else return 0;
		default:
			LM_CRIT("unknown lump condition %d\n", l->u.cond);
	}
	return 0; /* false */
}



/* computes the "unpacked" len of a lump list,
   code moved from build_req_from_req */
static inline int lumps_len(struct sip_msg* msg, struct lump* lumps, 
								struct dest_info* send_info)
{
	int s_offset;
	int new_len;
	struct lump* t;
	struct lump* r;
	str* send_address_str;
	str* send_port_str;
	str* recv_address_str = NULL;
	str* recv_port_str = NULL;
	int  recv_port_no = 0;
	struct socket_info* send_sock;
	

#ifdef USE_COMP
	#define RCVCOMP_LUMP_LEN \
						/* add;comp=xxx */ \
					switch(msg->rcv.comp){ \
						case COMP_NONE: \
								break; \
						case COMP_SIGCOMP: \
								new_len+=COMP_PARAM_LEN+SIGCOMP_NAME_LEN; \
								break; \
						case COMP_SERGZ: \
								new_len+=COMP_PARAM_LEN+SERGZ_NAME_LEN ; \
								break; \
						default: \
						LM_CRIT("unknown comp %d\n", msg->rcv.comp); \
					}

	#define SENDCOMP_LUMP_LEN \
					/* add;comp=xxx */ \
					switch(send_info->comp){ \
						case COMP_NONE: \
								break; \
						case COMP_SIGCOMP: \
								new_len+=COMP_PARAM_LEN+SIGCOMP_NAME_LEN; \
								break; \
						case COMP_SERGZ: \
								new_len+=COMP_PARAM_LEN+SERGZ_NAME_LEN ; \
								break; \
						default: \
						LM_CRIT("unknown comp %d\n", send_info->comp); \
					}
#else
	#define RCVCOMP_LUMP_LEN
	#define SENDCOMP_LUMP_LEN
#endif /*USE_COMP */
	
#define SUBST_LUMP_LEN(subst_l) \
		switch((subst_l)->u.subst){ \
			case SUBST_RCV_IP: \
				if (msg->rcv.bind_address){ \
					new_len+=recv_address_str->len; \
					if (msg->rcv.bind_address->address.af!=AF_INET) \
						new_len+=2; \
				}else{ \
					/* FIXME */ \
					LM_CRIT("FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_RCV_PORT: \
				if (msg->rcv.bind_address){ \
					new_len+=recv_port_str->len; \
				}else{ \
					/* FIXME */ \
					LM_CRIT("FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_RCV_PROTO: \
				if (msg->rcv.bind_address){ \
					switch(msg->rcv.bind_address->proto){ \
						case PROTO_NONE: \
						case PROTO_UDP: \
							new_len+=3; \
							break; \
						case PROTO_TCP: \
						case PROTO_TLS: \
							switch(msg->rcv.proto){ \
								case PROTO_WS: \
								case PROTO_WSS: \
									new_len+=2; \
									break; \
								default: \
									new_len+=3; \
									break; \
							} \
							break; \
						case PROTO_SCTP: \
							new_len+=4; \
							break; \
						default: \
						LM_CRIT("unknown proto %d\n", msg->rcv.bind_address->proto); \
					}\
				}else{ \
					/* FIXME */ \
					LM_CRIT("FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_RCV_ALL: \
				if (msg->rcv.bind_address){ \
					new_len+=recv_address_str->len; \
					if (msg->rcv.bind_address->address.af!=AF_INET) \
						new_len+=2; \
					if (recv_port_no!=SIP_PORT){ \
						/* add :port_no */ \
						new_len+=1+recv_port_str->len; \
					}\
						/*add;transport=xxx*/ \
					switch(msg->rcv.bind_address->proto){ \
						case PROTO_NONE: \
						case PROTO_UDP: \
							break; /* udp is the default */ \
						case PROTO_TCP: \
						case PROTO_TLS: \
							switch(msg->rcv.proto){ \
								case PROTO_WS: \
								case PROTO_WSS: \
									new_len+=TRANSPORT_PARAM_LEN+2; \
									break; \
								default: \
									new_len+=TRANSPORT_PARAM_LEN+3; \
									break; \
							} \
							break; \
						case PROTO_SCTP: \
							new_len+=TRANSPORT_PARAM_LEN+4; \
							break; \
						default: \
						LM_CRIT("unknown proto %d\n", \
								msg->rcv.bind_address->proto); \
					}\
					RCVCOMP_LUMP_LEN \
				}else{ \
					/* FIXME */ \
					LM_CRIT("FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_SND_IP: \
				if (send_sock){ \
					new_len+=send_address_str->len; \
					if (send_sock->address.af!=AF_INET && \
							send_address_str==&(send_sock->address_str)) \
						new_len+=2; \
				}else{ \
					LM_CRIT("FIXME: null send_sock\n"); \
				}; \
				break; \
			case SUBST_SND_PORT: \
				if (send_sock){ \
					new_len+=send_port_str->len; \
				}else{ \
					LM_CRIT("FIXME: null send_sock\n"); \
				}; \
				break; \
			case SUBST_SND_PROTO: \
				if (send_sock){ \
					switch(send_sock->proto){ \
						case PROTO_NONE: \
						case PROTO_UDP: \
							new_len+=3; \
							break; \
						case PROTO_TCP: \
						case PROTO_TLS: \
							switch(send_info->proto){ \
								case PROTO_WS: \
								case PROTO_WSS: \
									new_len+=2; \
									break; \
								default: \
									new_len+=3; \
									break; \
							} \
							break; \
						case PROTO_SCTP: \
							new_len+=4; \
							break; \
						default: \
						LM_CRIT("unknown proto %d\n", send_sock->proto); \
					}\
				}else{ \
					LM_CRIT("FIXME: null send_sock\n"); \
				}; \
				break; \
			case SUBST_SND_ALL: \
				if (send_sock){ \
					new_len+=send_address_str->len; \
					if ((send_sock->address.af!=AF_INET) && \
							(send_address_str==&(send_sock->address_str))) \
						new_len+=2; \
					if ((send_sock->port_no!=SIP_PORT) || \
							(send_port_str!=&(send_sock->port_no_str))){ \
						/* add :port_no */ \
						new_len+=1+send_port_str->len; \
					}\
					/*add;transport=xxx*/ \
					switch(send_sock->proto){ \
						case PROTO_NONE: \
						case PROTO_UDP: \
							break; /* udp is the default */ \
						case PROTO_TCP: \
						case PROTO_TLS: \
							switch(send_info->proto){ \
								case PROTO_WS: \
								case PROTO_WSS: \
									new_len+=TRANSPORT_PARAM_LEN+2; \
									break; \
								default: \
									new_len+=TRANSPORT_PARAM_LEN+3; \
									break; \
							} \
							break; \
						case PROTO_SCTP: \
							new_len+=TRANSPORT_PARAM_LEN+4; \
							break; \
						default: \
						LM_CRIT("unknown proto %d\n", send_sock->proto); \
					}\
					SENDCOMP_LUMP_LEN \
				}else{ \
					/* FIXME */ \
					LM_CRIT("FIXME: null send_sock\n"); \
				}; \
				break; \
			case SUBST_NOP: /* do nothing */ \
				break; \
			default: \
				LM_CRIT("unknown subst type %d\n", (subst_l)->u.subst); \
		}
	
	if (send_info){
		send_sock=send_info->send_sock;
	}else{
		send_sock=0;
	};
	s_offset=0;
	new_len=0;
	/* init send_address_str & send_port_str */
	if(send_sock && send_sock->useinfo.name.len>0)
		send_address_str=&(send_sock->useinfo.name);
	else if (msg->set_global_address.len)
		send_address_str=&(msg->set_global_address);
	else
		send_address_str=&(send_sock->address_str);
	if(send_sock && send_sock->useinfo.port_no>0)
		send_port_str=&(send_sock->useinfo.port_no_str);
	else if (msg->set_global_port.len)
		send_port_str=&(msg->set_global_port);
	else
		send_port_str=&(send_sock->port_no_str);
	/* init recv_address_str, recv_port_str & recv_port_no */
	if(msg->rcv.bind_address) {
		if(msg->rcv.bind_address->useinfo.name.len>0)
			recv_address_str=&(msg->rcv.bind_address->useinfo.name);
		else
			recv_address_str=&(msg->rcv.bind_address->address_str);
		if(msg->rcv.bind_address->useinfo.port_no>0) {
			recv_port_str=&(msg->rcv.bind_address->useinfo.port_no_str);
			recv_port_no = msg->rcv.bind_address->useinfo.port_no;
		} else {
			recv_port_str=&(msg->rcv.bind_address->port_no_str);
			recv_port_no = msg->rcv.bind_address->port_no;
		}
	}

	for(t=lumps;t;t=t->next){
		/* skip if this is an OPT lump and the condition is not satisfied */
		if ((t->op==LUMP_ADD_OPT)&& !lump_check_opt(t, msg, send_info))
			continue;
		for(r=t->before;r;r=r->before){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				case LUMP_ADD_SUBST:
					SUBST_LUMP_LEN(r);
					break;
				case LUMP_ADD_OPT:
					/* skip if this is an OPT lump and the condition is
					 * not satisfied */
					if (!lump_check_opt(r, msg, send_info))
						goto skip_before;
					break;
				default:
					/* only ADD allowed for before/after */
						LM_CRIT("invalid op for data lump (%x)\n", r->op);
			}
		}
skip_before:
		switch(t->op){
			case LUMP_ADD:
				new_len+=t->len;
				break;
			case LUMP_ADD_SUBST:
				SUBST_LUMP_LEN(t);
				break;
			case LUMP_ADD_OPT:
				/* we don't do anything here, it's only a condition for
				 * before & after */
				break;
			case LUMP_DEL:
				/* fix overlapping deleted zones */
				if (t->u.offset < s_offset){
					/* change len */
					if (t->len>s_offset-t->u.offset)
							t->len-=s_offset-t->u.offset;
					else t->len=0;
					t->u.offset=s_offset;
				}
				s_offset=t->u.offset+t->len;
				new_len-=t->len;
				break;
			case LUMP_NOP:
				/* fix offset if overlapping on a deleted zone */
				if (t->u.offset < s_offset){
					t->u.offset=s_offset;
				}else
					s_offset=t->u.offset;
				/* do nothing */
				break;
			default:
				LM_CRIT("invalid op for data lump (%x)\n", r->op);
		}
		for (r=t->after;r;r=r->after){
			switch(r->op){
				case LUMP_ADD:
					new_len+=r->len;
					break;
				case LUMP_ADD_SUBST:
					SUBST_LUMP_LEN(r);
					break;
				case LUMP_ADD_OPT:
					/* skip if this is an OPT lump and the condition is
					 * not satisfied */
					if (!lump_check_opt(r, msg, send_info))
						goto skip_after;
					break;
				default:
					/* only ADD allowed for before/after */
					LM_CRIT("invalid op for data lump (%x)\n", r->op);
			}
		}
skip_after:
		; /* to make gcc 3.* happy */
	}
	return new_len;
#undef RCVCOMP_LUMP_LEN
#undef SENDCOMP_LUMP_LEN
}



/* another helper functions, adds/Removes the lump,
	code moved form build_req_from_req  */

static inline void process_lumps(	struct sip_msg* msg,
					                                struct lump* lumps,
									char* new_buf,
									unsigned int* new_buf_offs,
									unsigned int* orig_offs,
									struct dest_info* send_info)
{
	struct lump *t;
	struct lump *r;
	char* orig;
	int size;
	int offset;
	int s_offset;
	str* send_address_str;
	str* send_port_str;
	str* recv_address_str = NULL;
	str* recv_port_str = NULL;
	int  recv_port_no = 0;
	struct socket_info* send_sock;

#ifdef USE_COMP
	#define RCVCOMP_PARAM_ADD \
				/* add ;comp=xxxx */ \
				switch(msg->rcv.comp){ \
					case COMP_NONE: \
						break; \
					case COMP_SIGCOMP: \
						memcpy(new_buf+offset, COMP_PARAM, COMP_PARAM_LEN);\
						offset+=COMP_PARAM_LEN; \
						memcpy(new_buf+offset, SIGCOMP_NAME, \
								SIGCOMP_NAME_LEN); \
						offset+=SIGCOMP_NAME_LEN; \
						break; \
					case COMP_SERGZ: \
						memcpy(new_buf+offset, COMP_PARAM, COMP_PARAM_LEN);\
						offset+=COMP_PARAM_LEN; \
						memcpy(new_buf+offset, SERGZ_NAME, SERGZ_NAME_LEN); \
						offset+=SERGZ_NAME_LEN; \
						break;\
					default:\
						LM_CRIT("unknown comp %d\n", msg->rcv.comp); \
				}
	
	#define SENDCOMP_PARAM_ADD \
				/* add ;comp=xxxx */ \
				switch(send_info->comp){ \
					case COMP_NONE: \
						break; \
					case COMP_SIGCOMP: \
						memcpy(new_buf+offset, COMP_PARAM, COMP_PARAM_LEN);\
						offset+=COMP_PARAM_LEN; \
						memcpy(new_buf+offset, SIGCOMP_NAME, \
								SIGCOMP_NAME_LEN); \
						offset+=SIGCOMP_NAME_LEN; \
						break; \
					case COMP_SERGZ: \
						memcpy(new_buf+offset, COMP_PARAM, COMP_PARAM_LEN);\
						offset+=COMP_PARAM_LEN; \
						memcpy(new_buf+offset, SERGZ_NAME, SERGZ_NAME_LEN); \
						offset+=SERGZ_NAME_LEN; \
						break;\
					default:\
						LM_CRIT("unknown comp %d\n", msg->rcv.comp); \
				} 
#else
	#define RCVCOMP_PARAM_ADD
	#define SENDCOMP_PARAM_ADD
#endif /* USE_COMP */

#define SUBST_LUMP(subst_l) \
	switch((subst_l)->u.subst){ \
		case SUBST_RCV_IP: \
			if (msg->rcv.bind_address){  \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]='['; offset++; \
				}\
				memcpy(new_buf+offset, recv_address_str->s, \
						recv_address_str->len); \
				offset+=recv_address_str->len; \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]=']'; offset++; \
				}\
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null bind_address\n"); \
			}; \
			break; \
		case SUBST_RCV_PORT: \
			if (msg->rcv.bind_address){  \
				memcpy(new_buf+offset, recv_port_str->s, \
						recv_port_str->len); \
				offset+=recv_port_str->len; \
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null bind_address\n"); \
			}; \
			break; \
		case SUBST_RCV_ALL: \
			if (msg->rcv.bind_address){  \
				/* address */ \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]='['; offset++; \
				}\
				memcpy(new_buf+offset, recv_address_str->s, \
						recv_address_str->len); \
				offset+=recv_address_str->len; \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]=']'; offset++; \
				}\
				/* :port */ \
				if (recv_port_no!=SIP_PORT){ \
					new_buf[offset]=':'; offset++; \
					memcpy(new_buf+offset, \
							recv_port_str->s, \
							recv_port_str->len); \
					offset+=recv_port_str->len; \
				}\
				switch(msg->rcv.bind_address->proto){ \
					case PROTO_NONE: \
					case PROTO_UDP: \
						break; /* nothing to do, udp is default*/ \
					case PROTO_TCP: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						if (msg->rcv.proto == PROTO_WS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tcp", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_TLS: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						if (msg->rcv.proto == PROTO_WS || msg->rcv.proto == PROTO_WSS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tls", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LM_CRIT("unknown proto %d\n", msg->rcv.bind_address->proto); \
				} \
				RCVCOMP_PARAM_ADD \
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null bind_address\n"); \
			}; \
			break; \
		case SUBST_SND_IP: \
			if (send_sock){  \
				if ((send_sock->address.af!=AF_INET) && \
						(send_address_str==&(send_sock->address_str))){\
					new_buf[offset]='['; offset++; \
				}\
				memcpy(new_buf+offset, send_address_str->s, \
									send_address_str->len); \
				offset+=send_address_str->len; \
				if ((send_sock->address.af!=AF_INET) && \
						(send_address_str==&(send_sock->address_str))){\
					new_buf[offset]=']'; offset++; \
				}\
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null send_sock\n"); \
			}; \
			break; \
		case SUBST_SND_PORT: \
			if (send_sock){  \
				memcpy(new_buf+offset, send_port_str->s, \
									send_port_str->len); \
				offset+=send_port_str->len; \
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null send_sock\n"); \
			}; \
			break; \
		case SUBST_SND_ALL: \
			if (send_sock){  \
				/* address */ \
				if ((send_sock->address.af!=AF_INET) && \
						(send_address_str==&(send_sock->address_str))){\
					new_buf[offset]='['; offset++; \
				}\
				memcpy(new_buf+offset, send_address_str->s, \
						send_address_str->len); \
				offset+=send_address_str->len; \
				if ((send_sock->address.af!=AF_INET) && \
						(send_address_str==&(send_sock->address_str))){\
					new_buf[offset]=']'; offset++; \
				}\
				/* :port */ \
				if ((send_sock->port_no!=SIP_PORT) || \
					(send_port_str!=&(send_sock->port_no_str))){ \
					new_buf[offset]=':'; offset++; \
					memcpy(new_buf+offset, send_port_str->s, \
							send_port_str->len); \
					offset+=send_port_str->len; \
				}\
				switch(send_sock->proto){ \
					case PROTO_NONE: \
					case PROTO_UDP: \
						break; /* nothing to do, udp is default*/ \
					case PROTO_TCP: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						if (send_info->proto == PROTO_WS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tcp", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_TLS: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						if (send_info->proto == PROTO_WS || send_info->proto == PROTO_WSS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tls", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LM_CRIT("unknown proto %d\n", send_sock->proto); \
				} \
				SENDCOMP_PARAM_ADD \
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null bind_address\n"); \
			}; \
			break; \
		case SUBST_RCV_PROTO: \
			if (msg->rcv.bind_address){ \
				switch(msg->rcv.bind_address->proto){ \
					case PROTO_NONE: \
					case PROTO_UDP: \
						memcpy(new_buf+offset, "udp", 3); \
						offset+=3; \
						break; \
					case PROTO_TCP: \
						if (msg->rcv.proto == PROTO_WS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tcp", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_TLS: \
						if (msg->rcv.proto == PROTO_WS || msg->rcv.proto == PROTO_WSS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tls", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LM_CRIT("unknown proto %d\n", msg->rcv.bind_address->proto); \
				} \
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null send_sock \n"); \
			}; \
			break; \
		case  SUBST_SND_PROTO: \
			if (send_sock){ \
				switch(send_sock->proto){ \
					case PROTO_NONE: \
					case PROTO_UDP: \
						memcpy(new_buf+offset, "udp", 3); \
						offset+=3; \
						break; \
					case PROTO_TCP: \
						if (send_info->proto == PROTO_WS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tcp", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_TLS: \
						if (send_info->proto == PROTO_WS || send_info->proto == PROTO_WSS) { \
							memcpy(new_buf+offset, "ws", 2); \
							offset+=2; \
						} else { \
							memcpy(new_buf+offset, "tls", 3); \
							offset+=3; \
						} \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LM_CRIT("unknown proto %d\n", send_sock->proto); \
				} \
			}else{  \
				/*FIXME*/ \
				LM_CRIT("FIXME: null send_sock \n"); \
			}; \
			break; \
		default: \
				LM_CRIT("unknown subst type %d\n", (subst_l)->u.subst); \
	}

	if (send_info){
		send_sock=send_info->send_sock;
	}else{
		send_sock=0;
	}
	/* init send_address_str & send_port_str */
	if (msg->set_global_address.len)
		send_address_str=&(msg->set_global_address);
	else
		send_address_str=&(send_sock->address_str);
	if (msg->set_global_port.len)
		send_port_str=&(msg->set_global_port);
	else
		send_port_str=&(send_sock->port_no_str);
	/* init send_address_str & send_port_str */
	if(send_sock && send_sock->useinfo.name.len>0)
		send_address_str=&(send_sock->useinfo.name);
	else if (msg->set_global_address.len)
		send_address_str=&(msg->set_global_address);
	else
		send_address_str=&(send_sock->address_str);
	if(send_sock && send_sock->useinfo.port_no>0)
		send_port_str=&(send_sock->useinfo.port_no_str);
	else if (msg->set_global_port.len)
		send_port_str=&(msg->set_global_port);
	else
		send_port_str=&(send_sock->port_no_str);
	/* init recv_address_str, recv_port_str & recv_port_no */
	if(msg->rcv.bind_address) {
		if(msg->rcv.bind_address->useinfo.name.len>0)
			recv_address_str=&(msg->rcv.bind_address->useinfo.name);
		else
			recv_address_str=&(msg->rcv.bind_address->address_str);
		if(msg->rcv.bind_address->useinfo.port_no>0) {
			recv_port_str=&(msg->rcv.bind_address->useinfo.port_no_str);
			recv_port_no = msg->rcv.bind_address->useinfo.port_no;
		} else {
			recv_port_str=&(msg->rcv.bind_address->port_no_str);
			recv_port_no = msg->rcv.bind_address->port_no;
		}
	}

	orig=msg->buf;
	offset=*new_buf_offs;
	s_offset=*orig_offs;

	for (t=lumps;t;t=t->next){
		switch(t->op){
			case LUMP_ADD:
			case LUMP_ADD_SUBST:
			case LUMP_ADD_OPT:
				/* skip if this is an OPT lump and the condition is
				 * not satisfied */
				if ((t->op==LUMP_ADD_OPT) &&
						(!lump_check_opt(t, msg, send_info)))
					continue;
				/* just add it here! */
				/* process before  */
				for(r=t->before;r;r=r->before){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						case LUMP_ADD_SUBST:
							SUBST_LUMP(r);
							break;
						case LUMP_ADD_OPT:
							/* skip if this is an OPT lump and the condition is
					 		* not satisfied */
							if (!lump_check_opt(r, msg, send_info))
								goto skip_before;
							break;
						default:
							/* only ADD allowed for before/after */
							LM_CRIT("invalid op for data lump (%x)\n", r->op);
					}
				}
skip_before:
				/* copy "main" part */
				switch(t->op){
					case LUMP_ADD:
						memcpy(new_buf+offset, t->u.value, t->len);
						offset+=t->len;
						break;
					case LUMP_ADD_SUBST:
						SUBST_LUMP(t);
						break;
					case LUMP_ADD_OPT:
						/* do nothing, it's only a condition */
						break;
					default:
						/* should not ever get here */
						LM_CRIT("unhandled data lump op %d\n", t->op);
				}
				/* process after */
				for(r=t->after;r;r=r->after){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						case LUMP_ADD_SUBST:
							SUBST_LUMP(r);
							break;
						case LUMP_ADD_OPT:
							/* skip if this is an OPT lump and the condition is
					 		* not satisfied */
							if (!lump_check_opt(r, msg, send_info))
								goto skip_after;
							break;
						default:
							/* only ADD allowed for before/after */
							LM_CRIT("invalid op for data lump (%x)\n", r->op);
					}
				}
skip_after:
				break;
			case LUMP_NOP:
			case LUMP_DEL:
				/* copy till offset */
				if (s_offset>t->u.offset){
					LM_DBG("WARNING: (%d) overlapped lumps offsets,"
						" ignoring(%x, %x)\n", t->op, s_offset,t->u.offset);
					/* this should've been fixed above (when computing len) */
					/* just ignore it*/
					break;
				}
				size=t->u.offset-s_offset;
				if (size){
					memcpy(new_buf+offset, orig+s_offset,size);
					offset+=size;
					s_offset+=size;
				}
				/* process before  */
				for(r=t->before;r;r=r->before){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						case LUMP_ADD_SUBST:
							SUBST_LUMP(r);
							break;
						case LUMP_ADD_OPT:
							/* skip if this is an OPT lump and the condition is
					 		* not satisfied */
							if (!lump_check_opt(r, msg, send_info))
								goto skip_nop_before;
							break;
						default:
							/* only ADD allowed for before/after */
							LM_CRIT("invalid op for data lump (%x)\n",r->op);
					}
				}
skip_nop_before:
				/* process main (del only) */
				if (t->op==LUMP_DEL){
					/* skip len bytes from orig msg */
					s_offset+=t->len;
				}
				/* process after */
				for(r=t->after;r;r=r->after){
					switch (r->op){
						case LUMP_ADD:
							/*just add it here*/
							memcpy(new_buf+offset, r->u.value, r->len);
							offset+=r->len;
							break;
						case LUMP_ADD_SUBST:
							SUBST_LUMP(r);
							break;
						case LUMP_ADD_OPT:
							/* skip if this is an OPT lump and the condition is
					 		* not satisfied */
							if (!lump_check_opt(r, msg, send_info))
								goto skip_nop_after;
							break;
						default:
							/* only ADD allowed for before/after */
							LM_CRIT("invalid op for data lump (%x)\n", r->op);
					}
				}
skip_nop_after:
				break;
			default:
					LM_CRIT("unknown op (%x)\n", t->op);
		}
	}
	*new_buf_offs=offset;
	*orig_offs=s_offset;
#undef RCVCOMP_PARAM_ADD 
#undef SENDCOMP_PARAM_ADD
}


/*
 * Adjust/insert Content-Length if necessary
 */
static inline int adjust_clen(struct sip_msg* msg, int body_delta, int proto)
{
	struct lump* anchor;
	char* clen_buf;
	int clen_len, body_only;
#ifdef USE_TCP
	char* body;
	int comp_clen;
#endif /* USE_TCP */

	/* Calculate message length difference caused by lumps modifying message
	 * body, from this point on the message body must not be modified. Zero
	 * value indicates that the body hasn't been modified
	*/

	clen_buf = 0;
	anchor=0;
	body_only=1;

	/* check to see if we need to add clen */
#ifdef USE_TCP
	if (proto == PROTO_TCP
#ifdef USE_TLS
	    || proto == PROTO_TLS
#endif
	    ) {
		if (parse_headers(msg, HDR_CONTENTLENGTH_F, 0)==-1){
			LM_ERR("error parsing content-length\n");
			goto error;
		}
		if (unlikely(msg->content_length==0)){
			/* not present, we need to add it */
			/* msg->unparsed should point just before the final crlf
			 * - whole message was parsed by the above parse_headers
			 *   which did not find content-length */
			anchor=anchor_lump(msg, msg->unparsed-msg->buf, 0,
												HDR_CONTENTLENGTH_T);
			if (anchor==0){
				LM_ERR("cannot set clen anchor\n");
				goto error;
			}
			body_only=0;
		}else{
			/* compute current content length and compare it with the
			   one in the message */
			body=get_body(msg);
			if (unlikely(body==0)){
				ser_error=E_BAD_REQ;
				LM_ERR("no message body found (missing crlf?)");
				goto error;
			}
			comp_clen=msg->len-(int)(body-msg->buf)+body_delta;
			if (comp_clen!=(int)(long)msg->content_length->parsed){
				/* note: we don't distinguish here between received with
				   wrong content-length and content-length changed, we just
				   fix it automatically in both cases (the reason being
				   that an error message telling we have received a msg-
				   with wrong content-length is of very little use) */
				anchor = del_lump(msg, msg->content_length->body.s-msg->buf,
									msg->content_length->body.len,
									HDR_CONTENTLENGTH_T);
				if (anchor==0) {
					LM_ERR("Can't remove original Content-Length\n");
					goto error;
				}
				body_only=1;
			}
		}
	}else
#endif /* USE_TCP */
	if (body_delta){
		if (parse_headers(msg, HDR_CONTENTLENGTH_F, 0) == -1) {
			LM_ERR("Error parsing Content-Length\n");
			goto error;
		}

		/* The body has been changed, try to find
		 * existing Content-Length
		 */
		/* no need for Content-Length if it's and UDP packet and
		 * it hasn't Content-Length already */
		if (msg->content_length==0){
		    /* content-length doesn't exist, append it */
			/* msg->unparsed should point just before the final crlf
			 * - whole message was parsed by the above parse_headers
			 *   which did not find content-length */
			if (proto!=PROTO_UDP){
				anchor=anchor_lump(msg, msg->unparsed-msg->buf, 0,
													HDR_CONTENTLENGTH_T);
				if (anchor==0){
					LM_ERR("cannot set clen anchor\n");
					goto error;
				}
				body_only=0;
			} /* else
				LM_DBG("UDP packet with no clen => not adding one \n"); */
		}else{
			/* Content-Length has been found, remove it */
			anchor = del_lump(	msg, msg->content_length->body.s - msg->buf,
								msg->content_length->body.len,
								HDR_CONTENTLENGTH_T);
			if (anchor==0) {
				LM_ERR("Can't remove original Content-Length\n");
				goto error;
			}
		}
	}

	if (anchor){
		clen_buf = clen_builder(msg, &clen_len, body_delta, body_only);
		if (!clen_buf) goto error;
		if (insert_new_lump_after(anchor, clen_buf, clen_len,
					HDR_CONTENTLENGTH_T) == 0)
			goto error;
	}
	
	return 0;
error:
	if (clen_buf) pkg_free(clen_buf);
	return -1;
}

static inline int find_line_start(char *text, unsigned int text_len,
				  char **buf, unsigned int *buf_len)
{
	char *ch, *start;
	unsigned int len;

	start = *buf;
	len = *buf_len;

	while (text_len <= len) {
		if (strncmp(text, start, text_len) == 0) {
			*buf = start;
			*buf_len = len;
			return 1;
		}
		if ((ch = memchr(start, 13, len - 1))) {
			if (*(ch + 1) != 10) {
				LM_ERR("No LF after CR\n");
				return 0;
			}
			len = len - (ch - start + 2);
			start = ch + 2;
		} else {
			LM_ERR("No CRLF found\n");
			return 0;
		}
	}
	return 0;
}

static inline int get_line(str s)
{
	char *ch;

	if ((ch = memchr(s.s, 13, s.len))) {
		if (*(ch + 1) != 10) {
			LM_ERR("No LF after CR\n");
			return 0;
		}
		return ch - s.s + 2;
	} else {
		LM_ERR("No CRLF found\n");
		return s.len;
	}
	return 0;
}

int replace_body(struct sip_msg *msg, str txt)
{
	struct lump *anchor;
	char *buf;
	str body = {0,0};

	body.s = get_body(msg);
	if(body.s==0)
	{
		LM_ERR("malformed sip message\n");
		return 0;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	LM_DBG("old size body[%d] actual[%d]\n", body.len, txt.len);
	if(body.s+body.len>msg->buf+msg->len)
	{
		LM_ERR("invalid content length: %d\n", body.len);
		return 0;
	}
	del_nonshm_lump( &(msg->body_lumps) );
	msg->body_lumps = NULL;

	if(del_lump(msg, body.s-msg->buf, body.len, 0) == 0)
	{
		LM_ERR("cannot delete existing body");
		return 0;
	}

	anchor = anchor_lump(msg, body.s - msg->buf, 0, 0);
	if(anchor==0)
	{
		LM_ERR("failed to get anchor\n");
		return 0;
	}

	buf=pkg_malloc(sizeof(char)*txt.len);
	if(buf==0)
	{
		PKG_MEM_ERROR;
		return 0;
	}
	memcpy(buf, txt.s, txt.len);
	if(insert_new_lump_after(anchor, buf, txt.len, 0)==0)
	{
		LM_ERR("failed to insert body lump\n");
		pkg_free(buf);
		return 0;
	}
	return 1;
}

/**
 * returns the boundary defined by the Content-Type
 * header
 */
int get_boundary(struct sip_msg* msg, str* boundary)
{
	str params;
	param_t *p, *list;
	param_hooks_t hooks;

	params.s = memchr(msg->content_type->body.s, ';',
		msg->content_type->body.len);
	if (params.s == NULL)
	{
		LM_ERR("Content-Type hdr has no params\n");
		return -1;
	}
	params.len = msg->content_type->body.len -
		(params.s - msg->content_type->body.s);
	if (parse_params(&params, CLASS_ANY, &hooks, &list) < 0)
	{
		LM_ERR("while parsing Content-Type params\n");
		return -1;
	}
	boundary->s = NULL;
	boundary->len = 0;
	for (p = list; p; p = p->next) {
		if ((p->name.len == 8)
			&& (strncasecmp(p->name.s, "boundary", 8) == 0))
		{
			boundary->s = pkg_malloc(p->body.len + 2);
			if (boundary->s == NULL)
			{
				free_params(list);
				LM_ERR("no memory for boundary string\n");
				return -1;
			}
			*(boundary->s) = '-';
			*(boundary->s + 1) = '-';
			memcpy(boundary->s + 2, p->body.s, p->body.len);
			boundary->len = 2 + p->body.len;
			LM_DBG("boundary is <%.*s>\n", boundary->len, boundary->s);
			break;
		}
	}
	free_params(list);
	return 0;
}

int check_boundaries(struct sip_msg *msg, struct dest_info *send_info)
{
	str b = {0,0};
	str fb = {0,0};
	str ob = {0,0};
	str bsuffix = {"\r\n", 2};
	str fsuffix = {"--\r\n", 4};
	str body = {0,0};
	str buf = {0,0};
	str tmp = {0,0};
	struct str_list* lb = NULL;
	struct str_list* lb_t = NULL;
	int lb_found = 0;
	int t, ret, lb_size;
	char *pb;

	if(!(msg->msg_flags&FL_BODY_MULTIPART)) return 0;
	else
	{
		buf.s = build_body(msg, (unsigned int *)&buf.len, &ret, send_info);
		if(ret) {
			LM_ERR("Can't get body\n");
			return -1;
		}
		tmp.s = buf.s;
		t = tmp.len = buf.len;
		if(get_boundary(msg, &ob)!=0) return -1;
		if(str_append(&ob, &bsuffix, &b)!=0) {
			LM_ERR("Can't append suffix to boundary\n");
			goto error;
		}
		if(str_append(&ob, &fsuffix,&fb)!=0) {
			LM_ERR("Can't append suffix to final boundary\n");
			goto error;
		}
		ret = b.len-2;
		while(t>0)
		{
			if(find_line_start(b.s, ret, &tmp.s,
				(unsigned int *)&tmp.len))
			{
				/*LM_DBG("found t[%d] tmp.len[%d]:[%.*s]\n",
					t, tmp.len, tmp.len, tmp.s);*/
				if(!lb)
				{
					lb = pkg_malloc(sizeof(struct str_list));
					if (!lb) {
						PKG_MEM_ERROR;
						goto error;
					}
					lb->s.s = tmp.s;
					lb->s.len = tmp.len;
					lb->next = 0;
					lb_t = lb;
				}
				else
				{
					lb_t = append_str_list(tmp.s, tmp.len, &lb_t, &lb_size);
				}
				lb_found = lb_found + 1;
				tmp.s = tmp.s + ret;
				t =  t - ret;
				tmp.len = tmp.len - ret;
			}
			else { t=0; }
		}
		if(lb_found<2)
		{
			LM_ERR("found[%d] wrong number of boundaries\n", lb_found);
			goto error;
		}
		/* adding 2 chars in advance */
		body.len = buf.len + 2;
		body.s = pkg_malloc(sizeof(char)*body.len);
		if (!body.s) {
			PKG_MEM_ERROR;
			goto error;
		}
		pb = body.s; body.len = 0;
		lb_t = lb;
		while(lb_t)
		{
			tmp.s = lb_t->s.s; tmp.len = lb_t->s.len;
			tmp.len = get_line(lb_t->s);
			if(tmp.len!=b.len || strncmp(b.s, tmp.s, b.len)!=0)
			{
				LM_DBG("malformed bondary in the middle\n");
				memcpy(pb, b.s, b.len); body.len = body.len + b.len;
				pb = pb + b.len;
				t = lb_t->s.s - (lb_t->s.s + tmp.len);
				memcpy(pb, lb_t->s.s+tmp.len, t); pb = pb + t;
				/*LM_DBG("new chunk[%d][%.*s]\n", t, t, pb-t);*/
			}
			else {
				t = lb_t->next->s.s - lb_t->s.s;
				memcpy(pb, lb_t->s.s, t);
				/*LM_DBG("copy[%d][%.*s]\n", t, t, pb);*/
				pb = pb + t;
			}
			body.len = body.len + t;
			/*LM_DBG("body[%d][%.*s]\n", body.len, body.len, body.s);*/
			lb_t = lb_t->next;
			if(!lb_t->next) lb_t = NULL;
		}
		/* last boundary */
		tmp.s = lb->s.s; tmp.len = lb->s.len;
		tmp.len = get_line(lb->s);
		if(tmp.len!=fb.len || strncmp(fb.s, tmp.s, fb.len)!=0)
		{
			LM_DBG("last bondary without -- at the end\n");
			memcpy(pb, fb.s, fb.len);
			/*LM_DBG("new chunk[%d][%.*s]\n", fb.len, fb.len, pb);*/
			pb = pb + fb.len;
			body.len = body.len + fb.len;
		}
		else {
			memcpy(pb, lb->s.s, lb->s.len); pb = pb + lb->s.len;
			body.len = body.len + lb->s.len;
			/*LM_DBG("copy[%d][%.*s]\n", lb->s.len, lb->s.len, pb - lb->s.len);*/
		}
		/*LM_DBG("body[%d][%.*s] expected[%ld]\n",
			body.len, body.len, body.s, pb-body.s); */
		if(!replace_body(msg, body))
		{
			LM_ERR("Can't replace body\n");
			goto error;
		}
		msg->msg_flags &= ~FL_BODY_MULTIPART;
		ret = 1;
		goto clean;
	}

error:
	ret = -1;
clean:
	if(ob.s) pkg_free(ob.s);
	if(b.s) pkg_free(b.s);
	if(fb.s) pkg_free(fb.s);
	if(body.s) pkg_free(body.s);
	if(buf.s) pkg_free(buf.s);
	while(lb)
	{
		lb_t = lb->next;
		pkg_free(lb);
		lb = lb_t;
	}
	return ret;
}

/** builds a request in memory from another sip request.
  *
  * Side-effects: - it adds lumps to the msg which are _not_ cleaned.
  * The added lumps are HDR_VIA_T (almost always added), HDR_CONTENLENGTH_T
  * and HDR_ROUTE_T (when a Route: header is added as a result of a non-null
  * msg->path_vec).
  *               - it might change send_info->proto and send_info->send_socket
  *                 if proto fallback is enabled (see below).
  *
  * Uses also global_req_flags ( OR'ed with msg->msg_flags, see send_info
  * below).
  *
  * @param msg  - sip message structure, complete with lumps
  * @param returned_len - result length (filled in)
  * @param send_info  - dest_info structure (value/result), contains where the
  *                     packet will be sent to (it's needed for building a 
  *                     correct via, fill RR lumps a.s.o.). If MTU based
  *                     protocol fall-back is enabled (see flags below),
  *                     send_info->proto might be updated with the new
  *                     protocol.
  *                     msg->msg_flags used:
  *                     - FL_TCP_MTU_FB, FL_TLS_MTU_FB and FL_SCTP_MTU_FB -
  *                       fallback to the corresp. proto if the built 
  *                       message > mtu and send_info->proto==PROTO_UDP. 
  *                       It will also update send_info->proto.
  *                     - FL_FORCE_RPORT: add rport to via
  * @param mode - flags for building the message, can be a combination of:
  *                 * BUILD_NO_LOCAL_VIA - don't add a local via
  *                 * BUILD_NO_VIA1_UPDATE - don't update first via (rport,
  *                    received a.s.o)
  *                 * BUILD_NO_PATH - don't add a Route: header with the 
  *                   msg->path_vec content.
  *                 * BUILD_IN_SHM - build the result in shm memory
  *
  * @return pointer to the new request (pkg_malloc'ed or shm_malloc'ed,
  * depending on the presence of the BUILD_IN_SHM flag, needs freeing when
  *   done) and sets returned_len or 0 on error.
  */
char * build_req_buf_from_sip_req( struct sip_msg* msg,
								unsigned int *returned_len,
								struct dest_info* send_info,
								unsigned int mode)
{
	unsigned int len, new_len, received_len, rport_len, uri_len, via_len,
				 body_delta;
	char* line_buf;
	char* received_buf;
	char* rport_buf;
	char* new_buf;
	char* buf;
	str path_buf;
	unsigned int offset, s_offset, size;
	struct lump* via_anchor;
	struct lump* via_lump;
	struct lump* via_insert_param;
	struct lump* path_anchor;
	struct lump* path_lump;
	str branch;
	unsigned int flags;
	unsigned int udp_mtu;
	struct dest_info di;

	via_insert_param=0;
	uri_len=0;
	buf=msg->buf;
	len=msg->len;
	received_len=0;
	rport_len=0;
	new_buf=0;
	received_buf=0;
	rport_buf=0;
	via_anchor=0;
	line_buf=0;
	via_len=0;
	path_buf.s=0;
	path_buf.len=0;

	flags=msg->msg_flags|global_req_flags;
	if(check_boundaries(msg, send_info)<0){
		LM_WARN("check_boundaries error\n");
	}
	/* Calculate message body difference and adjust Content-Length */
	body_delta = lumps_len(msg, msg->body_lumps, send_info);
	if (adjust_clen(msg, body_delta, send_info->proto) < 0) {
		LM_ERR("Error while adjusting Content-Length\n");
		goto error00;
	}

	if(unlikely(mode&BUILD_NO_LOCAL_VIA))
		goto after_local_via;

	/* create the via header */
	branch.s=msg->add_to_branch_s;
	branch.len=msg->add_to_branch_len;

	via_anchor=anchor_lump(msg, msg->via1->hdr.s-buf, 0, HDR_VIA_T);
	if (unlikely(via_anchor==0)) goto error00;
	line_buf = create_via_hf( &via_len, msg, send_info, &branch);
	if (unlikely(!line_buf)){
		LM_ERR("could not create Via header\n");
		goto error00;
	}
after_local_via:
	if(unlikely(mode&BUILD_NO_VIA1_UPDATE))
		goto after_update_via1;
	/* check if received needs to be added */
	if ( received_test(msg) ) {
		if ((received_buf=received_builder(msg,&received_len))==0){
			LM_ERR("received_builder failed\n");
			goto error01;  /* free also line_buf */
		}
	}

	/* check if rport needs to be updated:
	 *  - if FL_FORCE_RPORT is set add it (and del. any previous version)
	 *  - if via already contains an rport add it and overwrite the previous
	 *  rport value if present (if you don't want to overwrite the previous
	 *  version remove the comments) */
	if ((flags&FL_FORCE_RPORT)||
			(msg->via1->rport /*&& msg->via1->rport->value.s==0*/)){
		if ((rport_buf=rport_builder(msg, &rport_len))==0){
			LM_ERR("rport_builder failed\n");
			goto error01; /* free everything */
		}
	}

	/* find out where the offset of the first parameter that should be added
	 * (after host:port), needed by add receive & maybe rport */
	if (msg->via1->params.s){
			size= msg->via1->params.s-msg->via1->hdr.s-1; /*compensate
														  for ';' */
	}else{
			size= msg->via1->host.s-msg->via1->hdr.s+msg->via1->host.len;
			if (msg->via1->port!=0){
				/*size+=strlen(msg->via1->hdr.s+size+1)+1;*/
				size += msg->via1->port_str.len + 1; /* +1 for ':'*/
			}
#if 0
			/* no longer necessary, now hots.s contains [] */
			if(send_sock->address.af==AF_INET6) size+=1; /* +1 for ']'*/
#endif
	}
	/* if received needs to be added, add anchor after host and add it, or
	 * overwrite the previous one if already present */
	if (received_len){
		if (msg->via1->received){ /* received already present => overwrite it*/
			via_insert_param=del_lump(msg,
								msg->via1->received->start-buf-1, /*;*/
								msg->via1->received->size+1, /*;*/ HDR_VIA_T);
		}else if (via_insert_param==0){ /* receive not present, ok */
			via_insert_param=anchor_lump(msg, msg->via1->hdr.s-buf+size, 0,
											HDR_VIA_T);
		}
		if (via_insert_param==0) goto error02; /* free received_buf */
		if (insert_new_lump_after(via_insert_param, received_buf, received_len,
					HDR_VIA_T) ==0 ) goto error02; /* free received_buf */
	}
	/* if rport needs to be updated, delete it if present and add it's value */
	if (rport_len){
		if (msg->via1->rport){ /* rport already present */
			via_insert_param=del_lump(msg,
								msg->via1->rport->start-buf-1, /*';'*/
								msg->via1->rport->size+1 /* ; */, HDR_VIA_T);
		}else if (via_insert_param==0){ /*force rport, no rport present */
			/* no rport, add it */
			via_insert_param=anchor_lump(msg, msg->via1->hdr.s-buf+size, 0,
											HDR_VIA_T);
		}
		if (via_insert_param==0) goto error03; /* free rport_buf */
		if (insert_new_lump_after(via_insert_param, rport_buf, rport_len,
									HDR_VIA_T) ==0 )
			goto error03; /* free rport_buf */

	}

after_update_via1:
	/* add route with path content */
	if(unlikely(!(mode&BUILD_NO_PATH) && msg->path_vec.s &&
					msg->path_vec.len)){
		path_buf.len=ROUTE_PREFIX_LEN+msg->path_vec.len+CRLF_LEN;
		path_buf.s=pkg_malloc(path_buf.len+1);
		if (unlikely(path_buf.s==0)){
			LM_ERR("out of memory\n");
			ser_error=E_OUT_OF_MEM;
			goto error00;
		}
		memcpy(path_buf.s, ROUTE_PREFIX, ROUTE_PREFIX_LEN);
		memcpy(path_buf.s+ROUTE_PREFIX_LEN, msg->path_vec.s,
					msg->path_vec.len);
		memcpy(path_buf.s+ROUTE_PREFIX_LEN+msg->path_vec.len, CRLF, CRLF_LEN);
		path_buf.s[path_buf.len]=0;
		/* insert Route header either before the other routes
		   (if present & parsed), after the local via or after in front of
		    the first via if we don't add a local via*/
		if (msg->route){
			path_anchor=anchor_lump(msg, msg->route->name.s-buf, 0, 
									HDR_ROUTE_T);
		}else if (likely(via_anchor)){
			path_anchor=via_anchor;
		}else if (likely(msg->via1)){
			path_anchor=anchor_lump(msg, msg->via1->hdr.s-buf, 0, 
									HDR_ROUTE_T);
		}else{
			/* if no via1 (theoretically possible for non-sip messages,
			   e.g. http xmlrpc) */
			path_anchor=anchor_lump(msg, msg->headers->name.s-buf, 0, 
									HDR_ROUTE_T);
		}
		if (unlikely(path_anchor==0))
			goto error05;
		if (unlikely((path_lump=insert_new_lump_after(path_anchor, path_buf.s,
														path_buf.len,
														HDR_ROUTE_T))==0))
			goto error05;
	}
	/* compute new msg len and fix overlapping zones*/
	new_len=len+body_delta+lumps_len(msg, msg->add_rm, send_info)+via_len;
#ifdef XL_DEBUG
	LM_ERR("new_len(%d)=len(%d)+lumps_len\n", new_len, len);
#endif
	udp_mtu=cfg_get(core, core_cfg, udp_mtu);
	di.proto=PROTO_NONE;
	if (unlikely((send_info->proto==PROTO_UDP) && udp_mtu && 
					(flags & FL_MTU_FB_MASK) && (new_len>udp_mtu)
					&& (!(mode&BUILD_NO_LOCAL_VIA)))){

		di=*send_info; /* copy whole struct - will be used in the Via builder */
		di.proto=PROTO_NONE; /* except the proto */
#ifdef USE_TCP
		if (!tcp_disable && (flags & FL_MTU_TCP_FB) &&
				(di.send_sock=get_send_socket(msg, &send_info->to, PROTO_TCP))){
			di.proto=PROTO_TCP;
		}
	#ifdef USE_TLS
		else if (!tls_disable && (flags & FL_MTU_TLS_FB) &&
				(di.send_sock=get_send_socket(msg, &send_info->to, PROTO_TLS))){
			di.proto=PROTO_TLS;
		}
	#endif /* USE_TLS */
#endif /* USE_TCP */
#ifdef USE_SCTP
	#ifdef USE_TCP
		else
	#endif /* USE_TCP */
		 if (!sctp_disable && (flags & FL_MTU_SCTP_FB) &&
				(di.send_sock=get_send_socket(msg, &send_info->to, PROTO_SCTP))){
			di.proto=PROTO_SCTP;
		 }
#endif /* USE_SCTP */
		
		if (di.proto!=PROTO_NONE){
			new_len-=via_len;
			if(likely(line_buf)) pkg_free(line_buf);
			line_buf = create_via_hf( &via_len, msg, &di, &branch);
			if (!line_buf){
				LM_ERR("memory allocation failure!\n");
				goto error00;
			}
			new_len+=via_len;
		}
	}
	/* add via header to the list */
	/* try to add it before msg. 1st via */
	/* add first via, as an anchor for second via*/
	if(likely(line_buf)) {
		if ((via_lump=insert_new_lump_before(via_anchor, line_buf, via_len,
											HDR_VIA_T))==0)
			goto error04;
	}
	if (msg->new_uri.s){
		uri_len=msg->new_uri.len;
		new_len=new_len-msg->first_line.u.request.uri.len+uri_len;
	}
	if(unlikely(mode&BUILD_IN_SHM))
		new_buf=(char*)shm_malloc(new_len+1);
	else
		new_buf=(char*)pkg_malloc(new_len+1);
	if (new_buf==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		goto error00;
	}

	offset=s_offset=0;
	if (msg->new_uri.s){
		/* copy message up to uri */
		size=msg->first_line.u.request.uri.s-buf;
		memcpy(new_buf, buf, size);
		offset+=size;
		s_offset+=size;
		/* add our uri */
		memcpy(new_buf+offset, msg->new_uri.s, uri_len);
		offset+=uri_len;
		s_offset+=msg->first_line.u.request.uri.len; /* skip original uri */
	}
	new_buf[new_len]=0;
	/* copy msg adding/removing lumps */
	process_lumps(msg, msg->add_rm, new_buf, &offset, &s_offset, send_info);
	process_lumps(msg, msg->body_lumps, new_buf, &offset, &s_offset,send_info);
	/* copy the rest of the message */
	memcpy(new_buf+offset, buf+s_offset, len-s_offset);
	new_buf[new_len]=0;

	/* update the send_info if udp_mtu affected */
	if (di.proto!=PROTO_NONE) { 
		send_info->proto=di.proto;
		send_info->send_sock=di.send_sock;
	}

#ifdef DBG_MSG_QA
	if (new_buf[new_len-1]==0) {
		LM_ERR("0 in the end\n");
		abort();
	}
#endif

	*returned_len=new_len;
	return new_buf;

error01:
error02:
	if (received_buf) pkg_free(received_buf);
error03:
	if (rport_buf) pkg_free(rport_buf);
error04:
	if (line_buf) pkg_free(line_buf);
error05:
	if (path_buf.s) pkg_free(path_buf.s);
error00:
	*returned_len=0;
	return 0;
}

char * generate_res_buf_from_sip_res( struct sip_msg* msg,
				unsigned int *returned_len, unsigned int mode)
{
	unsigned int new_len, via_len, body_delta;
	char* new_buf;
	unsigned offset, s_offset, via_offset;
	char* buf;
	unsigned int len;

	buf=msg->buf;
	len=msg->len;
	new_buf=0;

	if(unlikely(mode&BUILD_NO_VIA1_UPDATE)) {
		via_len = 0;
		via_offset = 0;
	} else {
		/* we must remove the first via */
		if (msg->via1->next) {
			via_len=msg->via1->bsize;
			via_offset=msg->h_via1->body.s-buf;
		} else {
			via_len=msg->h_via1->len;
			via_offset=msg->h_via1->name.s-buf;
		}
	}

	     /* Calculate message body difference and adjust
	      * Content-Length
	      */
	body_delta = lumps_len(msg, msg->body_lumps, 0);
	if (adjust_clen(msg, body_delta, (msg->via2? msg->via2->proto:PROTO_UDP))
			< 0) {
		LM_ERR("error while adjusting Content-Length\n");
		goto error;
	}

	if(likely(!(mode&BUILD_NO_VIA1_UPDATE))) {
		/* remove the first via*/
		if (del_lump( msg, via_offset, via_len, HDR_VIA_T)==0){
			LM_ERR("error trying to remove first via\n");
			goto error;
		}
	}

	new_len=len+body_delta+lumps_len(msg, msg->add_rm, 0); /*FIXME: we don't
														know the send sock */

	LM_DBG("old size: %d, new size: %d\n", len, new_len);
	new_buf=(char*)pkg_malloc(new_len+1); /* +1 is for debugging
											 (\0 to print it )*/
	if (new_buf==0){
		LM_ERR("out of mem\n");
		goto error;
	}
	new_buf[new_len]=0; /* debug: print the message */
	offset=s_offset=0;
	/*FIXME: no send sock*/
	process_lumps(msg, msg->add_rm, new_buf, &offset, &s_offset, 0);/*FIXME:*/
	process_lumps(msg, msg->body_lumps, new_buf, &offset, &s_offset, 0);
	/* copy the rest of the message */
	memcpy(new_buf+offset,
		buf+s_offset,
		len-s_offset);
	 /* send it! */
	LM_DBG("copied size: orig:%d, new: %d, rest: %d msg=\n%s\n",
			s_offset, offset, len-s_offset, new_buf);

	*returned_len=new_len;
	return new_buf;
error:
	*returned_len=0;
	return 0;
}

char * build_res_buf_from_sip_res( struct sip_msg* msg,
				unsigned int *returned_len)
{
	return generate_res_buf_from_sip_res(msg, returned_len, 0);
}

char * build_res_buf_from_sip_req( unsigned int code, str *text ,str *new_tag,
		struct sip_msg* msg, unsigned int *returned_len, struct bookmark *bmark)
{
	char              *buf, *p;
	unsigned int      len,foo;
	struct hdr_field  *hdr;
	struct lump_rpl   *lump;
	struct lump_rpl   *body;
	int               i;
	char*             received_buf;
	unsigned int      received_len;
	char*             rport_buf;
	unsigned int      rport_len;
	char*             warning_buf;
	unsigned int      warning_len;
	char*             content_len_buf;
	unsigned int      content_len_len;
	char *after_body;
	str  to_tag;
	char *totags;
	int httpreq;
	char *pvia;

	body = 0;
	buf=0;
	received_buf=rport_buf=warning_buf=content_len_buf=0;
	received_len=rport_len=warning_len=content_len_len=0;

	to_tag.s=0;  /* fixes gcc 4.0 warning */
	to_tag.len=0;

	/* force parsing all headers -- we want to return all
	Via's in the reply and they may be scattered down to the
	end of header (non-block Vias are a really poor property
	of SIP :( ) */
	if (parse_headers( msg, HDR_EOH_F, 0 )==-1) {
		LM_ERR("alas, parse_headers failed\n");
		goto error00;
	}

	/*computes the length of the new response buffer*/
	len = 0;

	httpreq = IS_HTTP(msg);

	/* check if received needs to be added */
	if (received_test(msg)) {
		if ((received_buf=received_builder(msg,&received_len))==0) {
			LM_ERR("alas, received_builder failed\n");
			goto error00;
		}
	}
	/* check if rport needs to be updated */
	if ( ((msg->msg_flags|global_req_flags)&FL_FORCE_RPORT)||
		(msg->via1->rport /*&& msg->via1->rport->value.s==0*/)){
		if ((rport_buf=rport_builder(msg, &rport_len))==0){
			LM_ERR("rport_builder failed\n");
			goto error01; /* free everything */
		}
		if (msg->via1->rport)
			len -= msg->via1->rport->size+1; /* include ';' */
	}

	/* first line */
	len += msg->first_line.u.request.version.len + 1/*space*/ + 3/*code*/ + 1/*space*/ +
		text->len + CRLF_LEN/*new line*/;
	/*headers that will be copied (TO, FROM, CSEQ,CALLID,VIA)*/
	for ( hdr=msg->headers ; hdr ; hdr=hdr->next ) {
		switch (hdr->type) {
			case HDR_TO_T:
				if (new_tag && new_tag->len) {
					to_tag=get_to(msg)->tag_value;
					if ( to_tag.len || to_tag.s )
						len+=new_tag->len-to_tag.len;
					else
						len+=new_tag->len+TOTAG_TOKEN_LEN/*";tag="*/;
				}
				len += hdr->len;
				break;
			case HDR_VIA_T:
				/* we always add CRLF to via*/
				len+=(hdr->body.s+hdr->body.len)-hdr->name.s+CRLF_LEN;
				if (hdr==msg->h_via1) len += received_len+rport_len;
				break;
			case HDR_RECORDROUTE_T:
				/* RR only for 1xx and 2xx replies */
				if (code<180 || code>=300)
					break;
			case HDR_FROM_T:
			case HDR_CALLID_T:
			case HDR_CSEQ_T:
				/* we keep the original termination for these headers*/
				len += hdr->len;
				break;
			default:
				/* do nothing, we are interested only in the above headers */
				;
		}
	}
	/* lumps length */
	for(lump=msg->reply_lump;lump;lump=lump->next) {
		len += lump->text.len;
		if (lump->flags&LUMP_RPL_BODY)
			body = lump;
	}
	/* server header */
	if (server_signature && server_hdr.len)
		len += server_hdr.len + CRLF_LEN;
	/* warning hdr */
	if (sip_warning) {
		warning_buf = warning_builder(msg,&warning_len);
		if (warning_buf) len += warning_len + CRLF_LEN;
		else LM_WARN("warning skipped -- too big\n");
	}
	/* content length hdr */
	if (body) {
		content_len_buf = int2str(body->text.len, (int*)&content_len_len);
		len += CONTENT_LENGTH_LEN + content_len_len + CRLF_LEN;
	} else {
		len += CONTENT_LENGTH_LEN + 1/*0*/ + CRLF_LEN;
	}
	/* end of message */
	len += CRLF_LEN; /*new line*/

	/*allocating mem*/
	buf = (char*) pkg_malloc( len+1 );
	if (!buf)
	{
		LM_ERR("out of memory; needs %d\n",len);
		goto error01;
	}

	/* filling the buffer*/
	p=buf;
	/* first line */
	memcpy( p , msg->first_line.u.request.version.s , 
		msg->first_line.u.request.version.len);
	p += msg->first_line.u.request.version.len;
	*(p++) = ' ' ;
	/*code*/
	for ( i=2 , foo = code  ;  i>=0  ;  i-- , foo=foo/10 )
		*(p+i) = '0' + foo - ( foo/10 )*10;
	p += 3;
	*(p++) = ' ' ;
	memcpy( p , text->s , text->len );
	p += text->len;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	/* headers*/
	for ( hdr=msg->headers ; hdr ; hdr=hdr->next ) {
		switch (hdr->type)
		{
			case HDR_VIA_T:
				/* if is HTTP, backup start of Via header in response */
				if(unlikely(httpreq)) pvia = p;
				if (hdr==msg->h_via1){
					if (rport_buf){
						if (msg->via1->rport){ /* delete the old one */
							/* copy until rport */
							append_str_trans( p, hdr->name.s ,
								msg->via1->rport->start-hdr->name.s-1,msg);
							/* copy new rport */
							append_str(p, rport_buf, rport_len);
							/* copy the rest of the via */
							append_str_trans(p, msg->via1->rport->start+
												msg->via1->rport->size,
												hdr->body.s+hdr->body.len-
												msg->via1->rport->start-
												msg->via1->rport->size, msg);
						}else{ /* just append the new one */
							/* normal whole via copy */
							append_str_trans( p, hdr->name.s ,
								(hdr->body.s+hdr->body.len)-hdr->name.s, msg);
							append_str(p, rport_buf, rport_len);
						}
					}else{
						/* normal whole via copy */
						append_str_trans( p, hdr->name.s ,
								(hdr->body.s+hdr->body.len)-hdr->name.s, msg);
					}
					if (received_buf)
						append_str( p, received_buf, received_len);
				}else{
					/* normal whole via copy */
					append_str_trans( p, hdr->name.s,
							(hdr->body.s+hdr->body.len)-hdr->name.s, msg);
				}
				append_str( p, CRLF,CRLF_LEN);
				/* if is HTTP, replace Via with Sia
				 * - HTTP Via format is different than SIP Via
				 */
				if(unlikely(httpreq)) *pvia = 'S';
				break;
			case HDR_RECORDROUTE_T:
				/* RR only for 1xx and 2xx replies */
				if (code<180 || code>=300) break;
				append_str(p, hdr->name.s, hdr->len);
				break;
			case HDR_TO_T:
				if (new_tag && new_tag->len){
					if (to_tag.s ) { /* replacement */
						/* before to-tag */
						append_str( p, hdr->name.s, to_tag.s-hdr->name.s);
						/* to tag replacement */
						bmark->to_tag_val.s=p;
						bmark->to_tag_val.len=new_tag->len;
						append_str( p, new_tag->s,new_tag->len);
						/* the rest after to-tag */
						append_str( p, to_tag.s+to_tag.len,
							hdr->name.s+hdr->len-(to_tag.s+to_tag.len));
					}else{ /* adding a new to-tag */
						after_body=hdr->body.s+hdr->body.len;
						append_str( p, hdr->name.s, after_body-hdr->name.s);
						append_str(p, TOTAG_TOKEN, TOTAG_TOKEN_LEN);
						bmark->to_tag_val.s=p;
						bmark->to_tag_val.len=new_tag->len;
						append_str( p, new_tag->s,new_tag->len);
						append_str( p, after_body,
										hdr->name.s+hdr->len-after_body);
					}
					break;
				} /* no new to-tag -- proceed to 1:1 copying  */
				totags=((struct to_body*)(hdr->parsed))->tag_value.s;
				if (totags) {
					bmark->to_tag_val.s=p+(totags-hdr->name.s);
					bmark->to_tag_val.len=
							((struct to_body*)(hdr->parsed))->tag_value.len;
				}
				else {
					bmark->to_tag_val.len = 0;
					bmark->to_tag_val.s = p+(hdr->body.s+hdr->body.len-hdr->name.s);
				}
				/* no break */
			case HDR_FROM_T:
			case HDR_CALLID_T:
			case HDR_CSEQ_T:
					append_str(p, hdr->name.s, hdr->len);
					break;
			default:
				/* do nothing, we are interested only in the above headers */
				;
		} /* end switch */
	} /* end for */
	/* lumps */
	for(lump=msg->reply_lump;lump;lump=lump->next)
		if (lump->flags&LUMP_RPL_HDR){
			memcpy(p,lump->text.s,lump->text.len);
			p += lump->text.len;
		}
	/* server header */
	if (server_signature && server_hdr.len>0) {
		memcpy( p, server_hdr.s, server_hdr.len );
		p+=server_hdr.len;
		memcpy( p, CRLF, CRLF_LEN );
		p+=CRLF_LEN;
	}
	/* content_length hdr */
	if (content_len_len) {
		append_str( p, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		append_str( p, content_len_buf, content_len_len );
		append_str( p, CRLF, CRLF_LEN );
	} else {
		append_str( p, CONTENT_LENGTH"0"CRLF,CONTENT_LENGTH_LEN+1+CRLF_LEN);
	}
	/* warning header */
	if (warning_buf) {
		memcpy( p, warning_buf, warning_len);
		p+=warning_len;
		memcpy( p, CRLF, CRLF_LEN);
		p+=CRLF_LEN;
	}
	/*end of message*/
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	/* body */
	if (body) {
		memcpy ( p, body->text.s, body->text.len );
		p+=body->text.len;
	}

	if (len!=p-buf)
		LM_CRIT("diff len=%d p-buf=%d\n", len, (int)(p-buf));

	*(p) = 0;
	*returned_len = len;
	/* in req2reply, received_buf is not introduced to lumps and
	   needs to be deleted here
	*/
	if (received_buf) pkg_free(received_buf);
	if (rport_buf) pkg_free(rport_buf);
	return buf;

error01:
	if (received_buf) pkg_free(received_buf);
	if (rport_buf) pkg_free(rport_buf);
error00:
	*returned_len=0;
	return 0;
}



/* return number of chars printed or 0 if space exceeded;
   assumes buffer size of at least MAX_BRANCH_PARAM_LEN
 */
int branch_builder( unsigned int hash_index,
	/* only either parameter useful */
	unsigned int label, char * char_v,
	int branch,
	char *branch_str, int *len )
{

	char *begin;
	int size;

	/* hash id provided ... start with it */
	size=MAX_BRANCH_PARAM_LEN;
	begin=branch_str;
	*len=0;

	memcpy(begin, MCOOKIE, MCOOKIE_LEN );
	size-=MCOOKIE_LEN;begin+=MCOOKIE_LEN;

	if (int2reverse_hex( &begin, &size, hash_index)==-1)
		return 0;

	if (size) {
		*begin=BRANCH_SEPARATOR;
		begin++; size--;
	} else return 0;

	/* string with request's characteristic value ... use it ... */
	if (char_v) {
		if (memcpy(begin,char_v,MD5_LEN)) {
			begin+=MD5_LEN; size-=MD5_LEN;
		} else return 0;
	} else { /* ... use the "label" value otherwise */
		if (int2reverse_hex( &begin, &size, label )==-1)
			return 0;
	}

	if (size) {
		*begin=BRANCH_SEPARATOR;
		begin++; size--;
	} else return 0;

	if (int2reverse_hex( &begin, &size, branch)==-1)
		return 0;

	*len=MAX_BRANCH_PARAM_LEN-size;
	return size;

}



/* uses only the send_info->send_socket, send_info->proto and 
 * send_info->comp (so that a send_info used for sending can be passed
 * to this function w/o changes and the correct via will be built) */
char* via_builder( unsigned int *len,
	struct dest_info* send_info /* where to send the reply */,
	str* branch, str* extra_params, struct hostport* hp)
{
	unsigned int  via_len, extra_len;
	char               *line_buf;
	int max_len;
	int via_prefix_len;
	str* address_str; /* address displayed in via */
	str* port_str; /* port no displayed in via */
	struct socket_info* send_sock;
	int comp_len, comp_name_len;
#ifdef USE_COMP
	char* comp_name;
#endif /* USE_COMP */
	int port;
	struct ip_addr ip;
	union sockaddr_union *from = NULL;
	union sockaddr_union local_addr;
	struct tcp_connection *con = NULL;

	send_sock=send_info->send_sock;
	/* use pre-set address in via, the outbound socket alias or address one */
	if (hp && hp->host->len)
		address_str=hp->host;
	else if(send_sock->useinfo.name.len>0)
		address_str=&(send_sock->useinfo.name);
	else
		address_str=&(send_sock->address_str);
	if (hp && hp->port->len)
		port_str=hp->port;
	else if(send_sock->useinfo.port_no>0)
		port_str=&(send_sock->useinfo.port_no_str);
	else
		port_str=&(send_sock->port_no_str);
	
	comp_len=comp_name_len=0;
#ifdef USE_COMP
	comp_name=0;
	switch(send_info->comp){
		case COMP_NONE:
			break;
		case COMP_SIGCOMP:
			comp_len=COMP_PARAM_LEN;
			comp_name_len=SIGCOMP_NAME_LEN;
			comp_name=SIGCOMP_NAME;
			break;
		case COMP_SERGZ:
			comp_len=COMP_PARAM_LEN;
			comp_name_len=SERGZ_NAME_LEN;
			comp_name=SERGZ_NAME;
			break;
		default:
			LM_CRIT("unknown comp %d\n", send_info->comp);
			/* continue, we'll just ignore comp */
	}
#endif /* USE_COMP */
			
	via_prefix_len=MY_VIA_LEN+(send_info->proto==PROTO_SCTP);
	max_len=via_prefix_len +address_str->len /* space in MY_VIA */
		+2 /* just in case it is a v6 address ... [ ] */
		+1 /*':'*/+port_str->len
		+(branch?(MY_BRANCH_LEN+branch->len):0)
		+(extra_params?extra_params->len:0)
		+comp_len+comp_name_len
		+CRLF_LEN+1;
	line_buf=pkg_malloc( max_len );
	if (line_buf==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}

	extra_len=0;

	via_len=via_prefix_len+address_str->len; /*space included in MY_VIA*/

	memcpy(line_buf, MY_VIA, MY_VIA_LEN);
	if (send_info->proto==PROTO_UDP){
		/* do nothing */
	}else if (send_info->proto==PROTO_TCP){
		memcpy(line_buf+MY_VIA_LEN-4, "TCP ", 4);
	}else if (send_info->proto==PROTO_TLS){
		memcpy(line_buf+MY_VIA_LEN-4, "TLS ", 4);
	}else if (send_info->proto==PROTO_SCTP){
		memcpy(line_buf+MY_VIA_LEN-4, "SCTP ", 5);
	}else if (send_info->proto==PROTO_WS){
                if (unlikely(send_info->send_flags.f & SND_F_FORCE_SOCKET
                                && send_info->send_sock)) {
                        local_addr = send_info->send_sock->su;
                        su_setport(&local_addr, 0); /* any local port will do */
                        from = &local_addr;
                }

                port = su_getport(&send_info->to);
                if (likely(port)) {
                        su2ip_addr(&ip, &send_info->to);
                        con = tcpconn_get(send_info->id, &ip, port, from, 0);
                }
                else if (likely(send_info->id))
                        con = tcpconn_get(send_info->id, 0, 0, 0, 0);
                else {
                        LM_CRIT("null_id & to\n"); pkg_free(line_buf);
                        return 0;
                }

                if (con == NULL) {
                        LM_WARN("TCP/TLS connection (id: %d) for WebSocket could not be found\n",
								send_info->id);
						pkg_free(line_buf);
                        return 0;
                }

		if (con->rcv.proto==PROTO_WS) {
			memcpy(line_buf+MY_VIA_LEN-4, "WS ", 3);
		} else if (con->rcv.proto==PROTO_WSS) {
			memcpy(line_buf+MY_VIA_LEN-4, "WSS ", 4);
		} else {
			tcpconn_put(con);
			LM_CRIT("unknown proto %d\n", con->rcv.proto);
			pkg_free(line_buf);
			return 0;
		}
		tcpconn_put(con);
	}else if (send_info->proto==PROTO_WSS){
		memcpy(line_buf+MY_VIA_LEN-4, "WSS ", 4);
	}else{
		LM_CRIT("unknown proto %d\n", send_info->proto);
		pkg_free(line_buf);
		return 0;
	}
	/* add [] only if ipv6 and outbound socket address is used;
	 * if using pre-set no check is made */
	if ((send_sock->address.af==AF_INET6) &&
		(address_str==&(send_sock->address_str))) {
		line_buf[via_prefix_len]='[';
		line_buf[via_prefix_len+1+address_str->len]=']';
		extra_len=1;
		via_len+=2; /* [ ]*/
	}
	memcpy(line_buf+via_prefix_len+extra_len, address_str->s,
				address_str->len);
	if ((send_sock->port_no!=SIP_PORT) ||
				(port_str!=&send_sock->port_no_str)){
		line_buf[via_len]=':'; via_len++;
		memcpy(line_buf+via_len, port_str->s, port_str->len);
		via_len+=port_str->len;
	}

	/* branch parameter */
	if (branch){
		memcpy(line_buf+via_len, MY_BRANCH, MY_BRANCH_LEN );
		via_len+=MY_BRANCH_LEN;
		memcpy(line_buf+via_len, branch->s, branch->len );
		via_len+=branch->len;
	}
	/* extra params  */
	if (extra_params){
		memcpy(line_buf+via_len, extra_params->s, extra_params->len);
		via_len+=extra_params->len;
	}
#ifdef USE_COMP
	/* comp */
	if (comp_len){
		memcpy(line_buf+via_len, COMP_PARAM, COMP_PARAM_LEN);
		via_len+=COMP_PARAM_LEN;
		memcpy(line_buf+via_len, comp_name, comp_name_len);
		via_len+=comp_name_len;
	}
#endif

	memcpy(line_buf+via_len, CRLF, CRLF_LEN);
	via_len+=CRLF_LEN;
	line_buf[via_len]=0; /* null terminate the string*/

	*len = via_len;
	return line_buf;
}

/* creates a via header honoring the protocol of the incomming socket
 * msg is an optional parameter */
char* create_via_hf( unsigned int *len,
	struct sip_msg *msg,
	struct dest_info* send_info /* where to send the reply */,
	str* branch)
{
	char* via;
	str extra_params;
	struct hostport hp;
#if defined USE_TCP || defined USE_SCTP
	char* id_buf;
	unsigned int id_len;


	id_buf=0;
	id_len=0;
#endif
	extra_params.len=0;
	extra_params.s=0;


#if defined USE_TCP || defined USE_SCTP
	/* add id if tcp */
	if (msg && (
#ifdef USE_TCP
		(msg->rcv.proto==PROTO_TCP)
#ifdef USE_TLS
			|| (msg->rcv.proto==PROTO_TLS)
#endif
#ifdef USE_SCTP
			||
#endif /* USE_SCTP */
#endif /* USE_TCP */
#ifdef USE_SCTP
			(msg->rcv.proto==PROTO_SCTP)
#endif /* USE_SCTP */
			)){
		if  ((id_buf=id_builder(msg, &id_len))==0){
			LM_ERR("id_builder failed\n");
			return 0; /* we don't need to free anything,
			                 nothing alloc'ed yet*/
		}
		LM_DBG("id added: <%.*s>, rcv proto=%d\n",
				(int)id_len, id_buf, msg->rcv.proto);
		extra_params.s=id_buf;
		extra_params.len=id_len;
	}
#endif /* USE_TCP || USE_SCTP */

	/* test and add rport parameter to local via - rfc3581 */
	if(msg && msg->msg_flags&FL_ADD_LOCAL_RPORT) {
		/* params so far + ';rport' + '\0' */
		via = (char*)pkg_malloc(extra_params.len+RPORT_LEN);
		if(via==0) {
			LM_ERR("building local rport via param failed\n");
			if (extra_params.s) pkg_free(extra_params.s);
			return 0;
		}
		if(extra_params.len!=0) {
			memcpy(via, extra_params.s, extra_params.len);
			pkg_free(extra_params.s);
		}
		memcpy(via + extra_params.len, RPORT, RPORT_LEN-1);
		extra_params.s = via;
		extra_params.len += RPORT_LEN-1;
		extra_params.s[extra_params.len]='\0';
	}

	set_hostport(&hp, msg);
	via = via_builder( len, send_info, branch,
							extra_params.len?&extra_params:0, &hp);

	/* we do not need extra_params any more, already in the new via header */
	if (extra_params.s) pkg_free(extra_params.s);
	return via;
}

/* builds a char* buffer from message headers without body
 * first line is excluded in case of skip_first_line=1
 * error is set -1 if the memory allocation failes
 */
char * build_only_headers( struct sip_msg* msg, int skip_first_line,
				unsigned int *returned_len,
				int *error,
				struct dest_info* send_info)
{
	char		*buf, *new_buf;
	unsigned int	offset, s_offset, len, new_len;

	*error = 0;
	buf = msg->buf;
	if (skip_first_line)
		s_offset = msg->headers->name.s - buf;
	else
		s_offset = 0;

	/* original length without body, and without final \r\n */
	len = msg->unparsed - buf;
	/* new msg length */
	new_len =	len - /* original length  */
			s_offset + /* skipped first line */
			lumps_len(msg, msg->add_rm, send_info); /* lumps */

	if (new_len == 0) {
		*returned_len = 0;
		return 0;
	}

	new_buf = (char *)pkg_malloc(new_len+1);
	if (!new_buf) {
		LM_ERR("Not enough memory\n");
		*error = -1;
		return 0;
	}
	new_buf[0] = 0;
	offset = 0;

	/* copy message lumps */
	process_lumps(msg, msg->add_rm, new_buf, &offset, &s_offset, send_info);
	/* copy the rest of the message without body */
	if (len > s_offset) {
		memcpy(new_buf+offset, buf+s_offset, len-s_offset);
		offset += (len-s_offset);
	}
	new_buf[offset] = 0;

	*returned_len = offset;
	return new_buf;
}

/* builds a char* buffer from message body
 * error is set -1 if the memory allocation failes
 */
char * build_body( struct sip_msg* msg,
			unsigned int *returned_len,
			int *error,
			struct dest_info* send_info)
{
	char		*buf, *new_buf, *body;
	unsigned int	offset, s_offset, len, new_len;

	*error = 0;
	body = get_body(msg);

	if (!body || (body[0] == 0)) {
		*returned_len = 0;
		return 0;
	}
	buf = msg->buf;
	s_offset = body - buf;

	/* original length of msg with body */
	len = msg->len;
	/* new body length */
	new_len =	len - /* original length  */
			s_offset + /* msg without body */
			lumps_len(msg, msg->body_lumps, send_info); /* lumps */

	new_buf = (char *)pkg_malloc(new_len+1);
	if (!new_buf) {
		LM_ERR("Not enough memory\n");
		*error = -1;
		return 0;
	}
	new_buf[0] = 0;
	offset = 0;

	/* copy body lumps */
	process_lumps(msg, msg->body_lumps, new_buf, &offset, &s_offset, send_info);
	/* copy the rest of the message without body */
	if (len > s_offset) {
		memcpy(new_buf+offset, buf+s_offset, len-s_offset);
		offset += (len-s_offset);
	}
	new_buf[offset] = 0;

	*returned_len = offset;
	return new_buf;
}

/* builds a char* buffer from SIP message including body
 * The function adjusts the Content-Length HF according
 * to body lumps in case of touch_clen=1.
 */
char * build_all( struct sip_msg* msg, int touch_clen,
			unsigned int *returned_len,
			int *error,
			struct dest_info* send_info)
{
	char		*buf, *new_buf;
	unsigned int	offset, s_offset, len, new_len;
	unsigned int	body_delta;

	*error = 0;
	/* Calculate message body difference */
	body_delta = lumps_len(msg, msg->body_lumps, send_info);
	if (touch_clen) {
		/* adjust Content-Length */
		if (adjust_clen(msg, body_delta, send_info->proto) < 0) {
			LM_ERR("Error while adjusting Content-Length\n");
			*error = -1;
			return 0;
		}
	}

	buf = msg->buf;
	/* original msg length */
	len = msg->len;
	/* new msg length */
	new_len =	len + /* original length  */
			lumps_len(msg, msg->add_rm, send_info) + /* hdr lumps */
			body_delta; /* body lumps */

	if (new_len == 0) {
		returned_len = 0;
		return 0;
	}

	new_buf = (char *)pkg_malloc(new_len+1);
	if (!new_buf) {
		LM_ERR("Not enough memory\n");
		*error = -1;
		return 0;
	}
	new_buf[0] = 0;
	offset = s_offset = 0;

	/* copy message lumps */
	process_lumps(msg, msg->add_rm, new_buf, &offset, &s_offset, send_info);
	/* copy body lumps */
	process_lumps(msg, msg->body_lumps, new_buf, &offset, &s_offset, send_info);
	/* copy the rest of the message */
	memcpy(new_buf+offset, buf+s_offset, len-s_offset);
	offset += (len-s_offset);
	new_buf[offset] = 0;

	*returned_len = offset;
	return new_buf;	
}



/**
 * parse buf in msg and fill several fields
 */
int build_sip_msg_from_buf(struct sip_msg *msg, char *buf, int len,
		unsigned int id)
{
	if(msg==0 || buf==0)
		return -1;

	memset(msg, 0, sizeof(sip_msg_t));
	msg->id = id;
	msg->buf = buf;
	msg->len = len;
	if (parse_msg(buf, len, msg)!=0) {
		LM_ERR("parsing failed");
		return -1;
	}
	msg->set_global_address=default_global_address;
	msg->set_global_port=default_global_port;
	return 0;
}

