/*
 * $Id$
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
 */


#include "parse_uri.h"
#include <string.h>
#include "../dprint.h"
#include "../ut.h"    /* q_memchr */
#include "../error.h"

/* buf= pointer to begining of uri (sip:x@foo.bar:5060;a=b?h=i)
 * len= len of uri
 * returns: fills uri & returns <0 on error or 0 if ok 
 */
int parse_uri(char *buf, int len, struct sip_uri* uri)
{
	char* next, *end;
	char *user, *passwd, *host, *port, *params, *headers, *ipv6;
	int host_len, port_len, params_len, headers_len;
	int err;
	int ret;
	
	
	ret=0;
	host_len=0;
	end=buf+len;
	memset(uri, 0, sizeof(struct sip_uri)); /* zero it all, just to be sure */
	/* look for "sip:"*/;
	next=q_memchr(buf, ':',  len);
	if ((next==0)||(strncasecmp(buf,"sip",next-buf)!=0)){
		LOG(L_DBG, "ERROR: parse_uri: bad sip uri\n");
		ser_error=ret=E_BAD_URI;
		return ret;
	}
	buf=next+1; /* next char after ':' */
	if (buf>end){
		LOG(L_DBG, "ERROR: parse_uri: uri too short\n");
		ser_error=ret=E_BAD_URI;
		return ret;
	}
	/*look for '@' */
	next=q_memchr(buf,'@', end-buf);
	if (next==0){
		/* no '@' found, => no userinfo */
		uri->user.s=0;
		uri->passwd.s=0;
		host=buf;
	}else{
		/* found it */
		user=buf;
		/* try to find passwd */
		passwd=q_memchr(user,':', next-user);
		if (passwd==0){
			/* no ':' found => no password */
			uri->passwd.s=0;
			uri->user.s=user;
			uri->user.len=next-user;
		}else{
			uri->user.s=user;
			uri->user.len=passwd-user;
			passwd++; /*skip ':' */
			uri->passwd.s=passwd;
			uri->passwd.len=next-passwd;
		}
		host=next+1; /* skip '@' */
	}
	/* try to find the rest */
	if(host>=end){
		LOG(L_DBG, "ERROR: parse_uri: missing hostport\n");
		ser_error=ret=E_UNSPEC;
		return ret;
	}
	next=host;
	ipv6=q_memchr(host, '[', end-host);
	if (ipv6){
		host=ipv6+1; /* skip '[' in "[3ffe::abbcd]" */
		if (host>=end){
			LOG(L_DBG, "ERROR: parse_uri: bad ipv6 uri\n");
			ret=E_UNSPEC;
			return ret;
		}
		ipv6=q_memchr(host, ']', end-host);
		if ((ipv6==0)||(ipv6==host)){
			LOG(L_DBG, "ERROR: parse_uri: bad ipv6 uri - null address"
					" or missing ']'\n");
			ret=E_UNSPEC;
			return ret;
		}
		host_len=ipv6-host;
		next=ipv6;
	}

		
	headers=q_memchr(next,'?',end-next);
	params=q_memchr(next,';',end-next);
	port=q_memchr(next,':',end-next);
	if (host_len==0){ /* host not ipv6 addr */
		host_len=(port)?port-host:(params)?params-host:(headers)?headers-host:
				end-host;
	}
	/* get host */
	uri->host.s=host;
	uri->host.len=host_len;

	/* get port*/
	if ((port)&&(port+1<end)){
		port++;
		if ( ((params) &&(params<port))||((headers) &&(headers<port)) ){
			/* error -> invalid uri we found ';' or '?' before ':' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ser_error=ret=E_BAD_URI;
			return ret;
		}
		port_len=(params)?params-port:(headers)?headers-port:end-port;
		uri->port.s=port;
		uri->port.len=port_len;
	}else uri->port.s=0;
	/* get params */
	if ((params)&&(params+1<end)){
		params++;
		if ((headers) && (headers<params)){
			/* error -> invalid uri we found '?' or '?' before ';' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ser_error=ret=E_BAD_URI;
			return ret;
		}
		params_len=(headers)?headers-params:end-params;
		uri->params.s=params;
		uri->params.len=params_len;
	}else uri->params.s=0;
	/*get headers */
	if ((headers)&&(headers+1<end)){
		headers++;
		headers_len=end-headers;
		uri->headers.s=headers;
		uri->headers.len=headers_len;
	}else uri->headers.s=0;

	err=0;
	if (uri->port.s) uri->port_no=str2s(uri->port.s, uri->port.len, &err);
	if (err){
		LOG(L_DBG, "ERROR: parse_uri: bad port number in sip uri: %s\n",
				uri->port.s);
		ser_error=ret=E_BAD_URI;
		return ret;
	}

	return ret;
}



int parse_sip_msg_uri(struct sip_msg* msg)
{
	char* tmp;
	int tmp_len;
	if (msg->parsed_uri_ok) return 1;
	else{
		if (msg->new_uri.s){
			tmp=msg->new_uri.s;
			tmp_len=msg->new_uri.len;
		}else{
			tmp=msg->first_line.u.request.uri.s;
			tmp_len=msg->first_line.u.request.uri.len;
		}
		if (parse_uri(tmp, tmp_len, &msg->parsed_uri)<0){
			LOG(L_ERR, "ERROR: parse_sip_msg_uri: bad uri <%.*s>\n",
						tmp_len, tmp);
			msg->parsed_uri_ok=0;
			return -1;
		}
		msg->parsed_uri_ok=1;
		return 1;
	}
}

