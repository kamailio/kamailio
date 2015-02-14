/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 */
/* History:
 * --------
 *  2006-02-20  created by andrei
 */


#include <stdio.h>
#include <stdlib.h> /* malloc */
#include <string.h>
#include <netdb.h> /* getservbyname*/
#include <arpa/inet.h> /* ntohs */

#include "parse_listen_id.h"

/* ser source compat. defines*/
#define pkg_malloc malloc
#define pkg_free   free

#ifdef EXTRA_DEBUG
static int _debug = 1;
#else
static int _debug = 0;
#endif

#ifdef __SUNPRO_C
#define DBG(...) \
	do { \
		if(_debug==1) \
			fprintf(stderr,  __VA_ARGS__); \
	} while(0)
#define LOG(lev, ...) fprintf(stderr,  __VA_ARGS__)
#else
#define DBG(fmt, args...) \
	do { \
		if(_debug==1) \
			fprintf(stderr, fmt, ## args); \
	} while(0)
#define LOG(lev, fmt, args...) fprintf(stderr, fmt, ## args)
#endif





/* converts a str to an u. short, returns the u. short and sets *err on
 * error and if err!=null
  */
static inline unsigned short str2s(const char* s, unsigned int len,
									int *err)
{
	unsigned short ret;
	int i;
	unsigned char *limit;
	unsigned char *init;
	unsigned char* str;

	/*init*/
	str=(unsigned char*)s;
	ret=i=0;
	limit=str+len;
	init=str;

	for(;str<limit ;str++){
		if ( (*str <= '9' ) && (*str >= '0') ){
				ret=ret*10+*str-'0';
				i++;
				if (i>5) goto error_digits;
		}else{
				/* error unknown char */
				goto error_char;
		}
	}
	if (err) *err=0;
	return ret;

error_digits:
	DBG("str2s: ERROR: too many letters in [%.*s]\n", (int)len, init);
	if (err) *err=1;
	return 0;
error_char:
	DBG("str2s: ERROR: unexpected char %c in %.*s\n", *str, (int)len, init);
	if (err) *err=1;
	return 0;
}



/* parse proto:address:port   or proto:address */
/* returns struct id_list on success (pkg_malloc'ed), 0 on error
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
	
	s=pkg_malloc((len+1)*sizeof(char));
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
	id=pkg_malloc(sizeof(struct id_list));
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
	if (s) pkg_free(s);
	return 0;
}
