/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * --------
 * 2003-01-20  bug_fix: use of return value of snprintf aligned to C99 (jiri)
 * 2003-01-23  added rport patches, contributed by 
 *              Maxim Sobolev <sobomax@FreeBSD.org> and heavily modified by me
 *              (andrei)
 * 2003-01-24  added i param to via of outgoing requests (used by tcp),
 *              modified via_builder params (andrei)
 * 2003-01-27  more rport fixes (make use of new via_param->start)  (andrei)
 * 2003-01-27  next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-01-29  scratchpad removed (jiri)
 * 2003-02-28  scratchpad compatibility abandoned (jiri)
 * 2003-03-01  VOICE_MAIL defs removed (jiri)
 * 2003-03-06  totags in outgoing replies bookmarked to enable
 *             ACK/200 tag matching (andrei)
 * 2003-03-18  killed the build_warning snprintf (andrei)
 * 2003-03-31  added subst lump support (andrei)
 * 2003-04-01  added opt (conditional) lump support (andrei)
 * 2003-04-02  added more subst lumps: SUBST_{SND,RCV}_ALL  
 *              => ip:port;transport=proto (andrei)
 * 2003-04-12  added FL_FORCE_RPORT support (andrei)
 * 2003-04-13  updated warning builder -- fixed (andrei)
 * 2003-07-10  check_via_address knows now how to compare with ipv6 address
 *              references (e.g [::1]) (andrei)
 *             build_req_fomr_sip_req no longer adds 1 for ipv6 via parameter
 *              position calculations ([] are part of host.s now) (andrei)
 * 2003-10-02  via+lump dst address/port can be set to preset values (andrei)
 * 2003-10-08  receive_test function-alized (jiri)
 * 2003-10-20  added body_lump list (sip_msg), adjust_clen (andrei & jan)
 *
 */
/* Via special params:
 * requests:
 * - if the address in via is different from the src_ip or an existing
 *   received=something is found, received=src_ip is added (and any preexisting
 *   received is deleted). received is added as the first via parameter if no
 *   receive is previoulsy present or over the old receive.
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


#define append_str(_dest,_src,_len) \
	do{\
		memcpy( (_dest) , (_src) , (_len) );\
		(_dest) += (_len) ;\
	}while(0);

#define append_str_trans(_dest,_src,_len,_msg) \
	append_str( (_dest), (_src), (_len) );

extern char version[];
extern int version_len;







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

	/* maybe we are lucky and name it's an ip */
	s=ip_addr2a(ip);
	if (s){
		DBG("check_via_address(%s, %.*s, %d)\n", 
			s, name->len, name->s, resolver);
		len=strlen(s);

	#ifdef USE_IPV6
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
	#endif

			if (strncmp(name->s, s, name->len)==0) 
				return 0;
	}else{
		LOG(L_CRIT, "check_via_address: BUG: could not convert ip address\n");
		return -1;
	}

	if (port==0) port=SIP_PORT;
	if (resolver&DO_DNS){
		DBG("check_via_address: doing dns lookup\n");
		/* try all names ips */
		he=sip_resolvehost(name, &port, 0); /* FIXME proto? */
		if (he && ip->af==he->h_addrtype){
			for(i=0;he && he->h_addr_list[i];i++){
				if ( memcmp(&he->h_addr_list[i], ip->u.addr, ip->len)==0)
					return 0;
			}
		}
	}
	if (resolver&DO_REV_DNS){
		DBG("check_via_address: doing rev. dns lookup\n");
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


/* check if IP address in Via != source IP address of signaling */
int received_test( struct sip_msg *msg )
{
	char backup;
	int rcvd;

	/* zero-terminate hostname temporarily in case DNS resolver is used */
	backup = msg->via1->host.s[msg->via1->host.len];
	rcvd=msg->via1->received
			|| check_via_address(&msg->rcv.src_ip, &msg->via1->host,
							msg->via1->port, received_dns);
	msg->via1->host.s[msg->via1->host.len] = backup;
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
		
#define str_ipaddr_print(string, ipaddr_val)\
		do{\
			t=ip_addr2a((ipaddr_val)); \
			print_len=strlen(t); \
			str_pair_print(string, t, print_len);\
		} while(0)
	
	clen=0;
	str_lenpair_print(WARNING, WARNING_LEN,
						msg->rcv.bind_address->name.s,
						msg->rcv.bind_address->name.len);
	str_lenpair_print(":", 1, msg->rcv.bind_address->port_no_str.s,
						msg->rcv.bind_address->port_no_str.len);
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
	str_pair_print(" via_cnt", msg->parsed_flag & HDR_EOH ? "=" : ">", 1);
	str_int_print("=", via_cnt);
	if (clen<MAX_WARNING_LEN){ buf[clen]='"'; clen++; }
	else goto error_overflow;
		
		
	*returned_len=clen;
	return buf;
error_overflow:
	LOG(L_ERR, "ERROR: warning_builder: buffer size exceeded\n");
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
		LOG(L_ERR, "ERROR: received_builder: out of memory\n");
		return 0;
	}
	memcpy(buf, RECEIVED, RECEIVED_LEN);
	if ( (tmp=ip_addr2a(source_ip))==0)
		return 0; /* error*/
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
		LOG(L_ERR, "ERROR: rport_builder: out of memory\n");
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
		LOG(L_CRIT, "BUG: id_builder: not enough space for id\n");
		return 0;
	}
	value_len=p-&revhex[0];
	len=ID_PARAM_LEN+value_len; 
	buf=pkg_malloc(sizeof(char)*(len+1));/* place for ending \0 */
	if (buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: rport_builder: out of memory\n");
		return 0;
	}
	memcpy(buf, ID_PARAM, ID_PARAM_LEN);
	memcpy(buf+ID_PARAM_LEN, revhex, value_len);
	buf[len]=0; /* null terminate it */
	*id_len=len;
	return buf;
}



char* clen_builder(struct sip_msg* msg, unsigned int *clen_len, int diff)
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
		LOG(L_ERR, "ERROR: clen_builder: no message body found"
					" (missing crlf?)");
		return 0;
	}
	value=msg->len-(int)(body-msg->buf)+diff;
	value_s=int2str(value, &value_len);
	DBG("clen_builder: content-length: %d (%s)\n", value, value_s);
		
	len=CONTENT_LENGTH_LEN+value_len+CRLF_LEN;
	buf=pkg_malloc(sizeof(char)*(len+1));
	if (buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: clen_builder: out of memory\n");
		return 0;
	}
	memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
	memcpy(buf+CONTENT_LENGTH_LEN, value_s, value_len);
	memcpy(buf+CONTENT_LENGTH_LEN+value_len, CRLF, CRLF_LEN);
	buf[len]=0; /* null terminate it */
	*clen_len=len;
	return buf;
}



/* checks if a lump opt condition 
 * returns 1 if cond is true, 0 if false */
static inline int lump_check_opt(	enum lump_conditions cond,
									struct sip_msg* msg,
									struct socket_info* snd_s
									)
{
	struct ip_addr* ip;
	unsigned short port;
	int proto;

#define get_ip_port_proto \
			if (snd_s==0){ \
				LOG(L_CRIT, "ERROR: lump_check_opt: null send socket\n"); \
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
			
	switch(cond){
		case COND_FALSE:
			return 0;
		case COND_TRUE:
			return 1;
		case COND_IF_DIFF_REALMS:
			get_ip_port_proto;
			/* faster tests first */
			if ((port==snd_s->port_no)&&(proto==snd_s->proto)&&
				(ip_addr_cmp(ip, &snd_s->address)))
				return 0;
			else return 1;
		case COND_IF_DIFF_AF:
			get_ip_port_proto;
			if (ip->af!=snd_s->address.af) return 1;
			else return 0;
		case COND_IF_DIFF_PROTO:
			get_ip_port_proto;
			if (proto!=snd_s->proto) return 1;
			else return 0;
		case COND_IF_DIFF_PORT:
			get_ip_port_proto;
			if (port!=snd_s->port_no) return 1;
			else return 0;
		case COND_IF_DIFF_IP:
			get_ip_port_proto;
			if (ip_addr_cmp(ip, &snd_s->address)) return 0;
			else return 1;
		case COND_IF_RAND:
			return (rand()>=RAND_MAX/2);
		default:
			LOG(L_CRIT, "BUG: lump_check_opt: unknown lump condition %d\n",
					cond);
	}
	return 0; /* false */
}



/* computes the "unpacked" len of a lump list,
   code moved from build_req_from_req */
static inline int lumps_len(struct sip_msg* msg, struct lump* lumps, struct socket_info* send_sock)
{
	int s_offset;
	int new_len;
	struct lump* t;
	struct lump* r;
	str* send_address_str;
	str* send_port_str;

#define SUBST_LUMP_LEN(subst_l) \
		switch((subst_l)->u.subst){ \
			case SUBST_RCV_IP: \
				if (msg->rcv.bind_address){ \
					new_len+=msg->rcv.bind_address->address_str.len; \
					if (msg->rcv.bind_address->address.af!=AF_INET) \
						new_len+=2; \
				}else{ \
					/* FIXME */ \
					LOG(L_CRIT, "FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_RCV_PORT: \
				if (msg->rcv.bind_address){ \
					new_len+=msg->rcv.bind_address->port_no_str.len; \
				}else{ \
					/* FIXME */ \
					LOG(L_CRIT, "FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_RCV_PROTO: \
				if (msg->rcv.bind_address){ \
					switch(msg->rcv.bind_address->proto){ \
						case PROTO_NONE: \
						case PROTO_UDP: \
						case PROTO_TCP: \
						case PROTO_TLS: \
								new_len+=3; \
								break; \
						case PROTO_SCTP: \
								new_len+=4; \
								break; \
						default: \
						LOG(L_CRIT, "BUG: lumps_len: unknown proto %d\n", \
								msg->rcv.bind_address->proto); \
					}\
				}else{ \
					/* FIXME */ \
					LOG(L_CRIT, "FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_RCV_ALL: \
				if (msg->rcv.bind_address){ \
					new_len+=msg->rcv.bind_address->address_str.len; \
					if (msg->rcv.bind_address->address.af!=AF_INET) \
						new_len+=2; \
					if (msg->rcv.bind_address->port_no!=SIP_PORT){ \
						/* add :port_no */ \
						new_len+=1+msg->rcv.bind_address->port_no_str.len; \
					}\
						/*add;transport=xxx*/ \
					switch(msg->rcv.bind_address->proto){ \
						case PROTO_NONE: \
						case PROTO_UDP: \
								break; /* udp is the default */ \
						case PROTO_TCP: \
						case PROTO_TLS: \
								new_len+=TRANSPORT_PARAM_LEN+3; \
								break; \
						case PROTO_SCTP: \
								new_len+=TRANSPORT_PARAM_LEN+4; \
								break; \
						default: \
						LOG(L_CRIT, "BUG: lumps_len: unknown proto %d\n", \
								msg->rcv.bind_address->proto); \
					}\
				}else{ \
					/* FIXME */ \
					LOG(L_CRIT, "FIXME: null bind_address\n"); \
				}; \
				break; \
			case SUBST_SND_IP: \
				if (send_sock){ \
					new_len+=send_address_str->len; \
					if (send_sock->address.af!=AF_INET && \
							send_address_str==&(send_sock->address_str)) \
						new_len+=2; \
				}else{ \
					LOG(L_CRIT, "FIXME: lumps_len called with" \
							" null send_sock\n"); \
				}; \
				break; \
			case SUBST_SND_PORT: \
				if (send_sock){ \
					new_len+=send_port_str->len; \
				}else{ \
					LOG(L_CRIT, "FIXME: lumps_len called with" \
							" null send_sock\n"); \
				}; \
				break; \
			case SUBST_SND_PROTO: \
				if (send_sock){ \
					switch(send_sock->proto){ \
						case PROTO_NONE: \
						case PROTO_UDP: \
						case PROTO_TCP: \
						case PROTO_TLS: \
								new_len+=3; \
								break; \
						case PROTO_SCTP: \
								new_len+=4; \
								break; \
						default: \
						LOG(L_CRIT, "BUG: lumps_len: unknown proto %d\n", \
								send_sock->proto); \
					}\
				}else{ \
					LOG(L_CRIT, "FIXME: lumps_len called with" \
							" null send_sock\n"); \
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
								new_len+=TRANSPORT_PARAM_LEN+3; \
								break; \
						case PROTO_SCTP: \
								new_len+=TRANSPORT_PARAM_LEN+4; \
								break; \
						default: \
						LOG(L_CRIT, "BUG: lumps_len: unknown proto %d\n", \
								send_sock->proto); \
					}\
				}else{ \
					/* FIXME */ \
					LOG(L_CRIT, "FIXME: lumps_len called with" \
							" null send_sock\n"); \
				}; \
				break; \
			case SUBST_NOP: /* do nothing */ \
				break; \
			default: \
				LOG(L_CRIT, "BUG: unknown subst type %d\n", \
						(subst_l)->u.subst); \
		}
	
	s_offset=0;
	new_len=0;
	/* init send_address_str & send_port_str */
	if (msg->set_global_address.len)
		send_address_str=&(msg->set_global_address);
	else
		send_address_str=&(send_sock->address_str);
	if (msg->set_global_port.len)
		send_port_str=&(msg->set_global_address);
	else
		send_port_str=&(send_sock->port_no_str);
	
	
	for(t=lumps;t;t=t->next){
		/* skip if this is an OPT lump and the condition is not satisfied */
		if ((t->op==LUMP_ADD_OPT) && !lump_check_opt(t->u.cond, msg, send_sock))
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
					if (!lump_check_opt(r->u.cond, msg, send_sock))
						goto skip_before;
					break;
				default:
					/* only ADD allowed for before/after */
						LOG(L_CRIT, "BUG: lumps_len: invalid op "
							"for data lump (%x)\n", r->op);
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
				LOG(L_CRIT,"BUG:lumps_len: invalid"
							" op for data lump (%x)\n", r->op);
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
					if (!lump_check_opt(r->u.cond, msg, send_sock))
						goto skip_after;
					break;
				default:
					/* only ADD allowed for before/after */
					LOG(L_CRIT, "BUG:lumps_len: invalid"
								" op for data lump (%x)\n", r->op);
			}
		}
skip_after:
		; /* to make gcc 3.* happy */
	}
	return new_len;
}



/* another helper functions, adds/Removes the lump,
	code moved form build_req_from_req  */

static inline void process_lumps(	struct sip_msg* msg,
					                                struct lump* lumps,
									char* new_buf, 
									unsigned int* new_buf_offs, 
									unsigned int* orig_offs,
									struct socket_info* send_sock)
{
	struct lump *t;
	struct lump *r;
	char* orig;
	int size;
	int offset;
	int s_offset;
	str* send_address_str;
	str* send_port_str;

#define SUBST_LUMP(subst_l) \
	switch((subst_l)->u.subst){ \
		case SUBST_RCV_IP: \
			if (msg->rcv.bind_address){  \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]='['; offset++; \
				}\
				memcpy(new_buf+offset, msg->rcv.bind_address->address_str.s, \
						msg->rcv.bind_address->address_str.len); \
				offset+=msg->rcv.bind_address->address_str.len; \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]=']'; offset++; \
				}\
			}else{  \
				/*FIXME*/ \
				LOG(L_CRIT, "FIXME: process_lumps: null bind_address\n"); \
			}; \
			break; \
		case SUBST_RCV_PORT: \
			if (msg->rcv.bind_address){  \
				memcpy(new_buf+offset, msg->rcv.bind_address->port_no_str.s, \
						msg->rcv.bind_address->port_no_str.len); \
				offset+=msg->rcv.bind_address->port_no_str.len; \
			}else{  \
				/*FIXME*/ \
				LOG(L_CRIT, "FIXME: process_lumps: null bind_address\n"); \
			}; \
			break; \
		case SUBST_RCV_ALL: \
			if (msg->rcv.bind_address){  \
				/* address */ \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]='['; offset++; \
				}\
				memcpy(new_buf+offset, msg->rcv.bind_address->address_str.s, \
						msg->rcv.bind_address->address_str.len); \
				offset+=msg->rcv.bind_address->address_str.len; \
				if (msg->rcv.bind_address->address.af!=AF_INET){\
					new_buf[offset]=']'; offset++; \
				}\
				/* :port */ \
				if (msg->rcv.bind_address->port_no!=SIP_PORT){ \
					new_buf[offset]=':'; offset++; \
					memcpy(new_buf+offset, \
							msg->rcv.bind_address->port_no_str.s, \
							msg->rcv.bind_address->port_no_str.len); \
					offset+=msg->rcv.bind_address->port_no_str.len; \
				}\
				switch(msg->rcv.bind_address->proto){ \
					case PROTO_NONE: \
					case PROTO_UDP: \
						break; /* nothing to do, udp is default*/ \
					case PROTO_TCP: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						memcpy(new_buf+offset, "tcp", 3); \
						offset+=3; \
						break; \
					case PROTO_TLS: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						memcpy(new_buf+offset, "tls", 3); \
						offset+=3; \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LOG(L_CRIT, "BUG: process_lumps: unknown proto %d\n", \
								msg->rcv.bind_address->proto); \
				} \
			}else{  \
				/*FIXME*/ \
				LOG(L_CRIT, "FIXME: process_lumps: null bind_address\n"); \
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
				LOG(L_CRIT, "FIXME: process_lumps: called with" \
							" null send_sock\n"); \
			}; \
			break; \
		case SUBST_SND_PORT: \
			if (send_sock){  \
				memcpy(new_buf+offset, send_port_str->s, \
									send_port_str->len); \
				offset+=send_port_str->len; \
			}else{  \
				/*FIXME*/ \
				LOG(L_CRIT, "FIXME: process_lumps: called with" \
						" null send_sock\n"); \
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
						memcpy(new_buf+offset, "tcp", 3); \
						offset+=3; \
						break; \
					case PROTO_TLS: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						memcpy(new_buf+offset, "tls", 3); \
						offset+=3; \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, TRANSPORT_PARAM, \
								TRANSPORT_PARAM_LEN); \
						offset+=TRANSPORT_PARAM_LEN; \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LOG(L_CRIT, "BUG: process_lumps: unknown proto %d\n", \
								send_sock->proto); \
				} \
			}else{  \
				/*FIXME*/ \
				LOG(L_CRIT, "FIXME: process_lumps: null bind_address\n"); \
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
						memcpy(new_buf+offset, "tcp", 3); \
						offset+=3; \
						break; \
					case PROTO_TLS: \
						memcpy(new_buf+offset, "tls", 3); \
						offset+=3; \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LOG(L_CRIT, "BUG: process_lumps: unknown proto %d\n", \
								msg->rcv.bind_address->proto); \
				} \
			}else{  \
				/*FIXME*/ \
				LOG(L_CRIT, "FIXME: process_lumps: called with null" \
							" send_sock \n"); \
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
						memcpy(new_buf+offset, "tcp", 3); \
						offset+=3; \
						break; \
					case PROTO_TLS: \
						memcpy(new_buf+offset, "tls", 3); \
						offset+=3; \
						break; \
					case PROTO_SCTP: \
						memcpy(new_buf+offset, "sctp", 4); \
						offset+=4; \
						break; \
					default: \
						LOG(L_CRIT, "BUG: process_lumps: unknown proto %d\n", \
								send_sock->proto); \
				} \
			}else{  \
				/*FIXME*/ \
				LOG(L_CRIT, "FIXME: process_lumps: called with null" \
							" send_sock \n"); \
			}; \
			break; \
		default: \
					LOG(L_CRIT, "BUG: process_lumps: unknown subst type %d\n", \
							(subst_l)->u.subst); \
	} \
 \
	
	/* init send_address_str & send_port_str */
	if (msg->set_global_address.len)
		send_address_str=&(msg->set_global_address);
	else
		send_address_str=&(send_sock->address_str);
	if (msg->set_global_port.len)
		send_port_str=&(msg->set_global_address);
	else
		send_port_str=&(send_sock->port_no_str);
	
	
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
						(!lump_check_opt(t->u.cond, msg, send_sock))) 
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
							if (!lump_check_opt(r->u.cond, msg, send_sock))
								goto skip_before;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:process_lumps: "
									"invalid op for data lump (%x)\n", r->op);
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
						LOG(L_CRIT, "BUG: process_lumps: unhandled data lump "
								" op %d\n", t->op);
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
							if (!lump_check_opt(r->u.cond, msg, send_sock))
								goto skip_after;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:process_lumps: "
									"invalid op for data lump (%x)\n", r->op);
					}
				}
skip_after:
				break;
			case LUMP_NOP:
			case LUMP_DEL:
				/* copy till offset */
				if (s_offset>t->u.offset){
					DBG("Warning: (%d) overlapped lumps offsets,"
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
							if (!lump_check_opt(r->u.cond, msg, send_sock))
								goto skip_nop_before;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:process_lumps: "
									"invalid op for data lump (%x)\n",r->op);
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
							if (!lump_check_opt(r->u.cond, msg, send_sock)) 
								goto skip_nop_after;
							break;
						default:
							/* only ADD allowed for before/after */
							LOG(L_CRIT, "BUG:process_lumps: "
									"invalid op for data lump (%x)\n", r->op);
					}
				}
skip_nop_after:
				break;
			default:
					LOG(L_CRIT, "BUG: process_lumps: "
							"unknown op (%x)\n", t->op);
		}
	}
	*new_buf_offs=offset;
	*orig_offs=s_offset;
}


/*
 * Adjust/insert Content-Length if necesarry
 */
static inline int adjust_clen(struct sip_msg* msg, int body_delta, int proto)
{
	struct lump* anchor;
	char* clen_buf;
	int clen_len;

	/* Calculate message length difference caused by lumps modifying message
	 * body, from this point on the message body must not be modified. Zero
	 * value indicates that the body hasn't been modified
	*/

	clen_buf = 0;
	anchor=0;
	
	/* check to see if we need to add clen */
#ifdef USE_TCP
	if (proto == PROTO_TCP
#ifdef USE_TLS
	    || proto == PROTO_TLS
#endif
	    ) {
		if (parse_headers(msg, HDR_CONTENTLENGTH, 0)==-1){
			LOG(L_ERR, "adjust_clen: error parsing content-length\n");
			goto error;
		}
		if (msg->content_length==0){
			/* not present, we need to add it */
			/* msg->unparsed should point just before the final crlf
			 * - whole message was parsed by the above parse_headers
			 *   which did not find content-length */
			anchor=anchor_lump(msg, msg->unparsed-msg->buf, 0,
												HDR_CONTENTLENGTH);
			if (anchor==0){
				LOG(L_ERR, "adjust_clen: cannot set clen anchor\n");
				goto error;
			}
		}
	}
#endif
	
	
	if ((anchor==0) && body_delta){
		if (parse_headers(msg, HDR_CONTENTLENGTH, 0) == -1) {
			LOG(L_ERR, "adjust_clen: Error parsing Content-Length\n");
			goto error;
		}
		
		/* The body has been changed, try to find
		 * existing Content-Length
		 */
		/* no need for Content-Length if it's and UDP packet and
		 * it hasn't Content-Lentgh already */
		if ((msg->content_length==0)){
		    /* content-length doesn't exist, append it */
			/* msg->unparsed should point just before the final crlf
			 * - whole message was parsed by the above parse_headers
			 *   which did not find content-length */
			if (proto!=PROTO_UDP){
				anchor=anchor_lump(msg, msg->unparsed-msg->buf, 0,
													HDR_CONTENTLENGTH);
				if (anchor==0){
					LOG(L_ERR, "adjust_clen: cannot set clen anchor\n");
					goto error;
				}
			}else{
				DBG("add_clen: UDP packet with no clen => not adding one \n");
			}
		}else{
			/* Content-Length has been found, remove it */
			anchor = del_lump(	msg, msg->content_length->name.s - msg->buf,
								msg->content_length->len, HDR_CONTENTLENGTH);
			if (anchor==0) {
				LOG(L_ERR, "adjust_clen: Can't remove original"
							" Content-Length\n");
				goto error;
			}
		}
	}
	
	if (anchor){
		clen_buf = clen_builder(msg, &clen_len, body_delta);
		if (!clen_buf) goto error;
		if (insert_new_lump_after(anchor, clen_buf, clen_len,
					HDR_CONTENTLENGTH) == 0)
			goto error;
	}

	return 0;
error:
	if (clen_buf) pkg_free(clen_buf);
	return -1;
}



char * build_req_buf_from_sip_req( struct sip_msg* msg,
								unsigned int *returned_len,
								struct socket_info* send_sock, int proto)
{
	unsigned int len, new_len, received_len, rport_len, uri_len, via_len, body_delta;
	char* line_buf;
	char* received_buf;
	char* rport_buf;
	char* new_buf;
	char* buf;
	unsigned int offset, s_offset, size;
	struct lump* anchor;
	struct lump* via_insert_param;
	str branch;
	str extra_params;
	struct hostport hp;

#ifdef USE_TCP
	char* id_buf;
	unsigned int id_len;
	
	
	id_buf=0;
	id_len=0;
#endif
	via_insert_param=0;
	extra_params.len=0;
	extra_params.s=0;
	uri_len=0;
	buf=msg->buf;
	len=msg->len;
	received_len=0;
	rport_len=0;
	new_buf=0;
	received_buf=0;
	rport_buf=0;
	line_buf=0;
	
#ifdef USE_TCP
	/* add id if tcp */
	if (msg->rcv.proto==PROTO_TCP
#ifdef USE_TLS
			|| msg->rcv.proto==PROTO_TLS
#endif
			){
		if  ((id_buf=id_builder(msg, &id_len))==0){
			LOG(L_ERR, "ERROR: build_req_buf_from_sip_req:"
							" id_builder failed\n");
			goto error00; /* we don't need to free anything,
			                 nothing alloc'ed yet*/
		}
		DBG("build_req_from_req: id added: <%.*s>, rcv proto=%d\n",
				(int)id_len, id_buf, msg->rcv.proto);
		extra_params.s=id_buf;
		extra_params.len=id_len;
	}
#endif
	     /* Calculate message body difference and adjust
	      * Content-Length
	      */
	body_delta = lumps_len(msg, msg->body_lumps, send_sock);
	if (adjust_clen(msg, body_delta, proto) < 0) {
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: Error while adjusting"
				" Content-Length\n");
		goto error00;
	}

	branch.s=msg->add_to_branch_s;
	branch.len=msg->add_to_branch_len;
	set_hostport(&hp, msg);
	line_buf = via_builder( &via_len, send_sock, &branch,
							extra_params.len?&extra_params:0, proto, &hp);
	if (!line_buf){
		LOG(L_ERR,"ERROR: build_req_buf_from_sip_req: no via received!\n");
		goto error00;
	}
	/* check if received needs to be added */
	if ( received_test(msg) ) { 
		if ((received_buf=received_builder(msg,&received_len))==0){
			LOG(L_ERR, "ERROR: build_req_buf_from_sip_req:"
							" received_builder failed\n");
			goto error01;  /* free also line_buf */
		}
	}
	
	/* check if rport needs to be updated:
	 *  - if FL_FORCE_RPORT is set add it (and del. any previous version)
	 *  - if via already contains an rport add it and overwrite the previous
	 *  rport value if present (if you don't want to overwrite the previous
	 *  version remove the comments) */
	if ((msg->msg_flags&FL_FORCE_RPORT)||
			(msg->via1->rport /*&& msg->via1->rport->value.s==0*/)){
		if ((rport_buf=rport_builder(msg, &rport_len))==0){
			LOG(L_ERR, "ERROR: build_req_buf_from_sip_req:"
							" rport_builder failed\n");
			goto error01; /* free everything */
		}
	}

	/* add via header to the list */
	/* try to add it before msg. 1st via */
	/* add first via, as an anchor for second via*/
	anchor=anchor_lump(msg, msg->via1->hdr.s-buf, 0, HDR_VIA);
	if (anchor==0) goto error01;
	if (insert_new_lump_before(anchor, line_buf, via_len, HDR_VIA)==0)
		goto error01;
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
		#ifdef USE_IPV6
			if(send_sock->address.af==AF_INET6) size+=1; /* +1 for ']'*/
		#endif
#endif
	}
	/* if received needs to be added, add anchor after host and add it, or 
	 * overwrite the previous one if already present */
	if (received_len){
		if (msg->via1->received){ /* received already present => overwrite it*/
			via_insert_param=del_lump(msg,
								msg->via1->received->start-buf-1, /*;*/
								msg->via1->received->size+1, /*;*/ HDR_VIA);
		}else if (via_insert_param==0){ /* receive not present, ok */
			via_insert_param=anchor_lump(msg,
										msg->via1->hdr.s-buf+size,0, HDR_VIA);
		}
		if (via_insert_param==0) goto error02; /* free received_buf */
		if (insert_new_lump_after(via_insert_param, received_buf, received_len,
					HDR_VIA) ==0 ) goto error02; /* free received_buf */
	}
	/* if rport needs to be updated, delete it if present and add it's value */
	if (rport_len){
		if (msg->via1->rport){ /* rport already present */
			via_insert_param=del_lump(msg,
								msg->via1->rport->start-buf-1, /*';'*/
								msg->via1->rport->size+1 /* ; */, HDR_VIA);
		}else if (via_insert_param==0){ /*force rport, no rport present */
			/* no rport, add it */
			via_insert_param=anchor_lump(msg,
										msg->via1->hdr.s-buf+size,0, HDR_VIA);
		}
		if (via_insert_param==0) goto error03; /* free rport_buf */
		if (insert_new_lump_after(via_insert_param, rport_buf, rport_len,
									HDR_VIA) ==0 )
			goto error03; /* free rport_buf */
			
	}

	/* compute new msg len and fix overlapping zones*/
	new_len=len+body_delta+lumps_len(msg, msg->add_rm, send_sock);
#ifdef XL_DEBUG
	LOG(L_ERR, "DEBUG: new_len(%d)=len(%d)+lumps_len\n", new_len, len);
#endif

	if (msg->new_uri.s){
		uri_len=msg->new_uri.len;
		new_len=new_len-msg->first_line.u.request.uri.len+uri_len;
	}
	new_buf=(char*)pkg_malloc(new_len+1);
	if (new_buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: out of memory\n");
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
	process_lumps(msg, msg->add_rm, new_buf, &offset, &s_offset, send_sock);
	process_lumps(msg, msg->body_lumps, new_buf, &offset, &s_offset,send_sock);
	/* copy the rest of the message */
	memcpy(new_buf+offset, buf+s_offset, len-s_offset);
	new_buf[new_len]=0;

#ifdef DBG_MSG_QA
	if (new_buf[new_len-1]==0) {
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: 0 in the end\n");
		abort();
	}
#endif

	*returned_len=new_len;
	/* cleanup */
#ifdef USE_TCP
	if (id_buf) pkg_free(id_buf); /* it's not in a lump => we don't need it
									 anymore */
#endif
	return new_buf;

error01:
	if (line_buf) pkg_free(line_buf);
error02:
	if (received_buf) pkg_free(received_buf);
error03:
	if (rport_buf) pkg_free(rport_buf);
error00:
#ifdef USE_TCP
	if (id_buf) pkg_free(id_buf);
#endif
	*returned_len=0;
	return 0;
}



char * build_res_buf_from_sip_res( struct sip_msg* msg,
				unsigned int *returned_len)
{
	unsigned int new_len, via_len, body_delta;
	char* new_buf;
	unsigned offset, s_offset, via_offset;
	char* buf;
	unsigned int len;

	buf=msg->buf;
	len=msg->len;
	new_buf=0;
	/* we must remove the first via */
	if (msg->via1->next) {
		via_len=msg->via1->bsize;
		via_offset=msg->h_via1->body.s-buf;
	} else {
		via_len=msg->h_via1->len;
		via_offset=msg->h_via1->name.s-buf;
	}

	     /* Calculate message body difference and adjust
	      * Content-Length
	      */
	body_delta = lumps_len(msg, msg->body_lumps, 0);
	if (adjust_clen(msg, body_delta, (msg->via2? msg->via2->proto:PROTO_UDP))
			< 0) {
		LOG(L_ERR, "ERROR: build_req_buf_from_sip_req: Error while adjusting"
				" Content-Length\n");
		goto error;
	}

	/* remove the first via*/
	if (del_lump( msg, via_offset, via_len, HDR_VIA)==0){
		LOG(L_ERR, "build_res_buf_from_sip_res: error trying to remove first"
					"via\n");
		goto error;
	}

	new_len=len+body_delta+lumps_len(msg, msg->add_rm, 0); /*FIXME: we don't
														know the send sock */
	
	DBG(" old size: %d, new size: %d\n", len, new_len);
	new_buf=(char*)pkg_malloc(new_len+1); /* +1 is for debugging 
											 (\0 to print it )*/
	if (new_buf==0){
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_res: out of mem\n");
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
	DBG("build_res_from_sip_res: copied size: orig:%d, new: %d, rest: %d"
			" msg=\n%s\n", s_offset, offset, len-s_offset, new_buf);

	*returned_len=new_len;
	return new_buf;
error:
	*returned_len=0;
	return 0;
}


char * build_res_buf_from_sip_req( unsigned int code, char *text ,str *new_tag,
		struct sip_msg* msg, unsigned int *returned_len, struct bookmark *bmark)
{
	char              *buf, *p;
	unsigned int      len,foo;
	struct hdr_field  *hdr;
	struct lump_rpl   *lump;
	struct lump_rpl   *body;
	int               i;
	str               received = {0,0};
	str               rport = {0,0};
	str               warning = {0,0};
	str               content_len = {0,0};
	unsigned int      text_len;
	char *after_body;
	str  to_tag;
	char *totags;

	body = 0;
	buf=0;

	text_len=strlen(text);

	/* force parsing all headers -- we want to return all
	Via's in the reply and they may be scattered down to the
	end of header (non-block Vias are a really poor property
	of SIP :( ) */
	if (parse_headers( msg, HDR_EOH, 0 )==-1) {
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_req: "
			"alas, parse_headers failed\n");
		goto error00;
	}

	/*computes the lenght of the new response buffer*/
	len = 0;

	/* check if received needs to be added */
	if (received_test(msg)) {
		if ((received.s=received_builder(msg,&received.len))==0) {
			LOG(L_ERR, "ERROR: build_res_buf_from_sip_req: "
				"alas, received_builder failed\n");
			goto error00;
		}
	}
	/* check if rport needs to be updated */
	if ( (msg->msg_flags&FL_FORCE_RPORT)||
		(msg->via1->rport /*&& msg->via1->rport->value.s==0*/)){
		if ((rport.s=rport_builder(msg, &rport.len))==0){
			LOG(L_ERR, "ERROR: build_res_buf_from_sip_req:"
							" rport_builder failed\n");
			goto error01; /* free everything */
		}
		if (msg->via1->rport) 
			len -= msg->via1->rport->size+1; /* include ';' */
	}

	/* first line */
	len += SIP_VERSION_LEN + 1/*space*/ + 3/*code*/ + 1/*space*/ +
		text_len + CRLF_LEN/*new line*/;
	/*headers that will be copied (TO, FROM, CSEQ,CALLID,VIA)*/
	for ( hdr=msg->headers ; hdr ; hdr=hdr->next ) {
		switch (hdr->type) {
			case HDR_TO:
				if (new_tag && new_tag->len) {
					to_tag=get_to(msg)->tag_value;
					if (to_tag.len )
						len+=new_tag->len-to_tag.len;
					else
						len+=new_tag->len+TOTAG_TOKEN_LEN/*";tag="*/;
				}
				len += hdr->len;
				break;
			case HDR_VIA:
				/* we always add CRLF to via*/
				len+=(hdr->body.s+hdr->body.len)-hdr->name.s+CRLF_LEN;
				if (hdr==msg->h_via1) len += received.len+rport.len;
				break;
			case HDR_RECORDROUTE:
				/* RR only for 1xx and 2xx replies */
				if (code<180 || code>=300)
					break;
			case HDR_FROM:
			case HDR_CALLID:
			case HDR_CSEQ:
				/* we keep the original termination for these headers*/
				len += hdr->len;
		}
	}
	/* lumps length */
	for(lump=msg->reply_lump;lump;lump=lump->next) {
		len += lump->text.len;
		if (lump->type==LUMP_RPL_BODY)
			body = lump;
	}
	/* server header */
	if (server_signature)
		len += SERVER_HDR_LEN + CRLF_LEN;
	/* warning hdr */
	if (sip_warning) {
		warning.s = warning_builder(msg,&warning.len);
		if (warning.s) len += warning.len + CRLF_LEN;
		else LOG(L_WARN, "WARNING: warning skipped -- too big\n");
	}
	/* content length hdr */
	if (body) {
		content_len.s = int2str(body->text.len, &content_len.len);
		len += CONTENT_LENGTH_LEN + content_len.len + CRLF_LEN;
	} else {
		len += CONTENT_LENGTH_LEN + 1/*0*/ + CRLF_LEN;
	}
	/* end of message */
	len += CRLF_LEN; /*new line*/

	/*allocating mem*/
	buf = (char*) pkg_malloc( len+1 );
	if (!buf)
	{
		LOG(L_ERR, "ERROR: build_res_buf_from_sip_req: out of memory "
			" ; needs %d\n",len);
		goto error01;
	}

	/* filling the buffer*/
	p=buf;
	/* first line */
	memcpy( p , SIP_VERSION , SIP_VERSION_LEN );
	p += SIP_VERSION_LEN;
	*(p++) = ' ' ;
	/*code*/
	for ( i=2 , foo = code  ;  i>=0  ;  i-- , foo=foo/10 )
		*(p+i) = '0' + foo - ( foo/10 )*10;
	p += 3;
	*(p++) = ' ' ;
	memcpy( p , text , text_len );
	p += text_len;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;
	/* headers*/
	for ( hdr=msg->headers ; hdr ; hdr=hdr->next ) {
		switch (hdr->type)
		{
			case HDR_VIA:
				if (hdr==msg->h_via1){
					if (rport.s){
						if (msg->via1->rport){ /* delete the old one */
							/* copy until rport */
							append_str_trans( p, hdr->name.s ,
								msg->via1->rport->start-hdr->name.s-1,msg);
							/* copy new rport */
							append_str(p, rport.s, rport.len);
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
							append_str(p, rport.s, rport.len);
						}
					}else{
						/* normal whole via copy */
						append_str_trans( p, hdr->name.s , 
								(hdr->body.s+hdr->body.len)-hdr->name.s, msg);
					}
					if (received.s)
						append_str( p, received.s, received.len);
				}else{
					/* normal whole via copy */
					append_str_trans( p, hdr->name.s,
							(hdr->body.s+hdr->body.len)-hdr->name.s, msg);
				}
				append_str( p, CRLF,CRLF_LEN);
				break;
			case HDR_RECORDROUTE:
				/* RR only for 1xx and 2xx replies */
				if (code<180 || code>=300) break;
				append_str(p, hdr->name.s, hdr->len);
				break;
			case HDR_TO:
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
				};
			case HDR_FROM:
			case HDR_CALLID:
			case HDR_CSEQ:
					append_str(p, hdr->name.s, hdr->len);
		} /* end switch */
	} /* end for */
	/* lumps */
	for(lump=msg->reply_lump;lump;lump=lump->next)
		if (lump->type==LUMP_RPL_HDR){
			memcpy(p,lump->text.s,lump->text.len);
			p += lump->text.len;
		}
	/* server header */
	if (server_signature) {
		memcpy( p, SERVER_HDR , SERVER_HDR_LEN );
		p+=SERVER_HDR_LEN;
		memcpy( p, CRLF, CRLF_LEN );
		p+=CRLF_LEN;
	}
	/* content_length hdr */
	if (content_len.len) {
		append_str( p, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		append_str( p, content_len.s, content_len.len );
		append_str( p, CRLF, CRLF_LEN );
	} else {
		append_str( p, CONTENT_LENGTH"0"CRLF,CONTENT_LENGTH_LEN+1+CRLF_LEN);
	}
	/* warning header */
	if (warning.s) {
		memcpy( p, warning.s, warning.len);
		p+=warning.len;
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
		LOG(L_CRIT,"BUGGGG!!!! diff len=%d p-buf=%d\n",len,p-buf);

	*(p) = 0;
	*returned_len = len;
	/* in req2reply, received_buf is not introduced to lumps and
	   needs to be deleted here
	*/
	if (received.s) pkg_free(received.s);
	if (rport.s) pkg_free(rport.s);
	return buf;

error01:
	if (received.s) pkg_free(received.s);
	if (rport.s) pkg_free(rport.s);
error00:
	*returned_len=0;
	return 0;
}



/* return number of chars printed or 0 if space exceeded;
   assumes buffer sace of at least MAX_BRANCH_PARAM_LEN
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


char* via_builder( unsigned int *len, 
	struct socket_info* send_sock,
	str* branch, str* extra_params, int proto, struct hostport* hp)
{
	unsigned int  via_len, extra_len;
	char               *line_buf;
	int max_len;
	str* address_str; /* address displayed in via */
	str* port_str; /* port no displayed in via */
	
	/* use pre-set address in via or the outbound socket one */
	if ( hp && hp->host->len)
		address_str=hp->host;
	else
		address_str=&(send_sock->address_str);
	if (hp && hp->port->len)
		port_str=hp->port;
	else
		port_str=&(send_sock->port_no_str);

	max_len=MY_VIA_LEN+address_str->len /* space in MY_VIA */
		+2 /* just in case it is a v6 address ... [ ] */
		+1 /*':'*/+port_str->len
		+(branch?(MY_BRANCH_LEN+branch->len):0)
		+(extra_params?extra_params->len:0)
		+CRLF_LEN+1;
	line_buf=pkg_malloc( max_len );
	if (line_buf==0){
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: via_builder: out of memory\n");
		return 0;
	}

	extra_len=0;

	via_len=MY_VIA_LEN+address_str->len; /*space included in MY_VIA*/

	memcpy(line_buf, MY_VIA, MY_VIA_LEN); 
	if (proto==PROTO_UDP){
		/* dop nothing */
	}else if (proto==PROTO_TCP){
		memcpy(line_buf+MY_VIA_LEN-4, "TCP ", 4);
	}else if (proto==PROTO_TLS){
		memcpy(line_buf+MY_VIA_LEN-4, "TLS ", 4);
	}else{
		LOG(L_CRIT, "BUG: via_builder: unknown proto %d\n", proto);
		return 0;
	}
#	ifdef USE_IPV6
	/* add [] only if ipv6 and outbound socket address is used;
	 * if using pre-set no check is made */
	if ((send_sock->address.af==AF_INET6) &&
		(address_str==&(send_sock->address_str))) {
		line_buf[MY_VIA_LEN]='[';
		line_buf[MY_VIA_LEN+1+address_str->len]=']';
		extra_len=1;
		via_len+=2; /* [ ]*/
	}
#	endif
	memcpy(line_buf+MY_VIA_LEN+extra_len, address_str->s, address_str->len);
	if ((send_sock->port_no!=SIP_PORT) || (port_str!=&send_sock->port_no_str)){
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
	
	memcpy(line_buf+via_len, CRLF, CRLF_LEN);
	via_len+=CRLF_LEN;
	line_buf[via_len]=0; /* null terminate the string*/

	*len = via_len;
	return line_buf;
}
