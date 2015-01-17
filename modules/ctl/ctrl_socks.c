/*
 * Copyright (C) 2006 iptelorg GmbH
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
 */

#include "ctrl_socks.h"
#include "init_socks.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"

#ifdef USE_FIFO
#include "fifo_server.h"
#endif

#include "ctl.h"

/* parse proto:address:port   or proto:address */
/* returns struct id_list on success (ctl_malloc'ed), 0 on error
 * WARNING: it will add \0 in the string*/
/* parses:
 *     tcp|udp|unix:host_name:port
 *     tcp|udp|unix:host_name
 *     host_name:port
 *     host_name
 * 
 *
 *     where host_name=string, ipv4 address, [ipv6 address],
 *         unix socket path (starts with '/')
 */
struct id_list* parse_listen_id(char* l, int len, enum socket_protos def)
{
	char* p;
	enum socket_protos proto;
	char* name;
	char* port_str;
	int port;
	int err;
	struct servent* se;
	char* s;
	struct id_list* id;
	
	s=ctl_malloc((len+1)*sizeof(char));
	if (s==0){
		LOG(L_ERR, "ERROR:parse_listen_id: out of memory\n");
		goto error;
	}
	memcpy(s, l, len);
	s[len]=0; /* null terminate */
	
	/* duplicate */
	proto=UNKNOWN_SOCK;
	port=0;
	name=0;
	port_str=0;
	p=s;
	
	if ((*p)=='[') goto ipv6;
	/* find proto or name */
	for (; *p; p++){
		if (*p==':'){
			*p=0;
			if (strcasecmp("tcp", s)==0){
				proto=TCP_SOCK;
				goto find_host;
			}else if (strcasecmp("udp", s)==0){
				proto=UDP_SOCK;
				goto find_host;
			}else if (strcasecmp("unixd", s)==0){
				proto=UNIXD_SOCK;
				goto find_host;
			}else if ((strcasecmp("unix", s)==0)||(strcasecmp("unixs", s)==0)){
				proto=UNIXS_SOCK;
				goto find_host;
#ifdef USE_FIFO
			}else if (strcasecmp("fifo", s)==0){
				proto=FIFO_SOCK;
				goto find_host;
#endif
			}else{
				proto=UNKNOWN_SOCK;
				/* this might be the host */
				name=s;
				goto find_port;
			}
		}
	}
	name=s;
	goto end; /* only name found */
find_host:
	p++;
	if (*p=='[') goto ipv6;
	name=p;
	for (; *p; p++){
		if ((*p)==':'){
			*p=0;
			goto find_port;
		}
	}
	goto end; /* nothing after name */
ipv6:
	name=p;
	p++;
	for(;*p;p++){
		if(*p==']'){
			if(*(p+1)==':'){
				p++; *p=0;
				goto find_port;
			}else if (*(p+1)==0) goto end;
		}else{
			goto error;
		}
	}
	
find_port:
	p++;
	port_str=(*p)?p:0;
	
end:
	/* fix all the stuff */
	if (name==0) goto error;
	if (proto==UNKNOWN_SOCK){
		/* try to guess */
		if (port_str){
			switch(def){
				case TCP_SOCK:
				case UDP_SOCK:
					proto=def;
					break;
				default:
					proto=UDP_SOCK;
					DBG("guess:%s is a tcp socket\n", name);
			}
		}else if (name && strchr(name, '/')){
			switch(def){
				case TCP_SOCK:
				case UDP_SOCK:
					DBG("guess:%s is a unix socket\n", name);
					proto=UNIXS_SOCK;
					break;
				default:
					/* def is filename based => use default */
					proto=def;
			}
		}else{
			/* using default */
			proto=def;
		}
	}
	if (port_str){
		port=str2s(port_str, strlen(port_str), &err);
		if (err){
			/* try getservbyname */
			se=getservbyname(port_str, 
					(proto==TCP_SOCK)?"tcp":(proto==UDP_SOCK)?"udp":0);
			if (se) port=ntohs(se->s_port);
			else goto error;
		}
	}else{
		/* no port, check if the hostname is a port 
		 * (e.g. tcp:3012 == tcp:*:3012 */
		if (proto==TCP_SOCK|| proto==UDP_SOCK){
			port=str2s(name, strlen(name), &err);
			if (err){
				port=0;
			}else{
				name="*"; /* inaddr any  */
			}
		}
	}
	id=ctl_malloc(sizeof(struct id_list));
	if (id==0){
		LOG(L_ERR, "ERROR:parse_listen_id: out of memory\n");
		goto error;
	}
	id->name=name;
	id->proto=proto;
	id->data_proto=P_BINRPC;
	id->port=port;
	id->buf=s;
	id->next=0;
	return id;
error:
	if (s) ctl_free(s);
	return 0;
}


void free_id_list_elem(struct id_list* id)
{
	if (id->buf){
		ctl_free(id->buf);
		id->buf=0;
	}
}

void free_id_list(struct id_list* l)
{
	struct id_list* nxt;
	
	for (;l; l=nxt){
		nxt=l->next;
		free_id_list_elem(l);
		ctl_free(l);
	}
}



int init_ctrl_sockets(struct ctrl_socket** c_lst, struct id_list* lst,
						int def_port, int perm, int uid, int gid)
{
	struct id_list* l;
	int s;
	struct ctrl_socket* cs;
	int extra_fd;
	union sockaddr_u su;
	
	for (l=lst; l; l=l->next){
		extra_fd=-1;
		switch(l->proto){
			case UNIXS_SOCK:
				s=init_unix_sock(&su.sa_un, l->name, SOCK_STREAM,
						perm, uid, gid);
				break;
			case UNIXD_SOCK:
				s=init_unix_sock(&su.sa_un, l->name, SOCK_DGRAM,
						perm, uid, gid);
				break;
			case TCP_SOCK:
				if (l->port==0) l->port=def_port;
				s=init_tcpudp_sock(&su.sa_in, l->name, l->port, TCP_SOCK);
				break;
			case UDP_SOCK:
				if (l->port==0) l->port=def_port;
				s=init_tcpudp_sock(&su.sa_in, l->name, l->port, UDP_SOCK);
				break;
#ifdef USE_FIFO
			case FIFO_SOCK:
				s=init_fifo_fd(l->name, perm, uid, gid, &extra_fd);
				break;
#endif
			default:
				LOG(L_ERR, "ERROR: init_ctrl_listeners: unsupported"
						" proto %d\n", l->proto);
				continue;
		}
		if (s==-1) goto error;
		/* add listener */
		cs=ctl_malloc(sizeof(struct ctrl_socket));
		if (cs==0){
			LOG(L_ERR, "ERROR: init_ctrl_listeners: out of memory\n");
			goto error;
		}
		memset(cs,0, sizeof(struct ctrl_socket));
		cs->transport=l->proto;
		cs->p_proto=l->data_proto;
		cs->fd=s;
		cs->write_fd=extra_fd; /* needed for fifo write */
		cs->name=l->name;
		cs->port=l->port;
		cs->u=su;
		/* add it to the list */
		cs->next=*c_lst;
		*c_lst=cs;
	}
	return 0;
error:
	return -1;
}



void free_ctrl_socket_list(struct ctrl_socket* l)
{
	struct ctrl_socket* nxt;
	
	for (;l; l=nxt){
		nxt=l->next;
		if (l->data)
			ctl_free(l->data);
		ctl_free(l);
	}
}
