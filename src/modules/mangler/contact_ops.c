/*
 * mangler module
 *
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
 */

#include "contact_ops.h"
#include "utils.h"
#include "common.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/hf.h"
#include "../../parser/parse_uri.h"
#include "../../parser/contact/parse_contact.h"
#include "../../ut.h"
#include "../../dset.h"

#include <stdio.h>
#include <string.h>

#define SIP_SCH	"sip:"
#define SIP_SCH_LEN	(sizeof(SIP_SCH)-1)


int
encode_contact (struct sip_msg *msg, char *encoding_prefix,char *public_ip)
{

	contact_body_t *cb;
	contact_t *c;
	str* uri;
	str newUri;
	int res;
	char separator;



	/*
	 * I have a list of contacts in contact->parsed which is of type contact_body_t 
	 * inside i have a contact->parsed->contact which is the head of the list of contacts
	 * inside it is a 
	 * str uri;
	 * struct contact *next;
	 * I just have to visit each uri and encode each uri according to a scheme
	 */
	
	if ((msg->contact == NULL)&&((parse_headers(msg,HDR_CONTACT_F,0) == -1) ||
				(msg->contact == NULL) ))
		{
		LOG(L_ERR,"ERROR: encode_contact: no Contact header present\n");
		return -1;
		}


	separator = DEFAULT_SEPARATOR[0];
	if (contact_flds_separator != NULL)
		if (strlen(contact_flds_separator)>=1)
			separator = contact_flds_separator[0];
	
	if (msg->contact->parsed == NULL)	parse_contact (msg->contact);
	if (msg->contact->parsed != NULL)
	{
		cb = (contact_body_t *) msg->contact->parsed;
		c = cb->contacts;
		/* we visit each contact */
		if (c != NULL)
		{
			uri = &c->uri;
			res = encode_uri(msg, uri, encoding_prefix, public_ip,
								separator, &newUri);
			
			if (res != 0)
				{
				LOG (L_ERR,"ERROR: encode_contact: Failed encoding contact.Code %d\n", res);
				return res;
				}
			else
				if (patch (msg, uri->s, uri->len, newUri.s, newUri.len) < 0)
				{
					LOG (L_ERR,"ERROR: encode_contact: lumping failed in mangling port \n");
					return -2;
				}
			
			/* encoding next contacts too?*/
#ifdef ENCODE_ALL_CONTACTS
			while (c->next != NULL)
			{
				c = c->next;
				uri = &c->uri;
				
				res = encode_uri (msg, uri, encoding_prefix, public_ip, 
									separator, &newUri);
				if (res != 0)
					{
					LOG(L_ERR,"ERROR: encode_contact: Failed encode_uri.Code %d\n",res);
#ifdef STRICT_CHECK
				return res;
#endif
					}
				else
				if (patch (msg, uri->s, uri->len, newUri.s, newUri.len)< 0)
				{
					LOG (L_ERR,"ERROR: encode_contact: lumping failed in mangling port \n");
					return -3;
				}
			} /* while */
#endif /* ENCODE_ALL_CONTACTS */
		} /* if c != NULL */

	} /* end if */
	else /* after parsing still NULL */
		{
			LOG(L_ERR,"ERROR: encode_contact: Unable to parse Contact header\n");
#ifdef EXTRA_DEBUG
    			printf("ERROR: encode_contact: Unable to parse Contact header\n");
#endif                        
			return -4;
		}
#ifdef EXTRA_DEBUG
	fprintf (stdout,"---END--------ENCODE CONTACT-----------------\n");
#endif
	return 1;
}

int
decode_contact (struct sip_msg *msg,char *unused1,char *unused2)
{

	str* uri;
	str newUri;
	str dst_uri;
	char separator;
	int res;
	

	separator = DEFAULT_SEPARATOR[0];
	if (contact_flds_separator != NULL)
		if (strlen(contact_flds_separator)>=1)
			separator = contact_flds_separator[0];
		
	if ((msg->new_uri.s == NULL) || (msg->new_uri.len == 0)) {
		uri = &msg->first_line.u.request.uri;
	}else{
		uri = &msg->new_uri;
	}
	
	res = decode_uri (uri, separator, &newUri, &dst_uri);
	
	if (res != 0) {
			LOG (L_ERR,"ERROR: decode_contact:Failed decoding contact."
						"Code %d\n", res);
			return res;
	} else {
		/* we do not modify the original first line */
		if (msg->new_uri.s)
			pkg_free(msg->new_uri.s);
		msg->new_uri = newUri;
		msg->parsed_uri_ok=0;
		msg->dst_uri = dst_uri;
		ruri_mark_new();
	}
	return 1;
}

int
decode_contact_header (struct sip_msg *msg,char *unused1,char *unused2)
{

	contact_body_t *cb;
	contact_t *c;
	str* uri;
	str newUri;
	char separator;
	int res;
	
	

	if ((msg->contact == NULL)&&((parse_headers(msg,HDR_CONTACT_F,0) == -1) || 
				(msg->contact== NULL) ))
		{
		LOG(L_ERR,"ERROR: decode_contact_header: no Contact header present\n");
		return -1;
		}

	separator = DEFAULT_SEPARATOR[0];
	if (contact_flds_separator != NULL)
		if (strlen(contact_flds_separator)>=1)
			separator = contact_flds_separator[0];
		
	if (msg->contact->parsed == NULL) parse_contact (msg->contact);
	if (msg->contact->parsed != NULL)
	{
		cb = (contact_body_t *) msg->contact->parsed;
		c = cb->contacts;
		// we visit each contact 
	 if (c != NULL)
	  {
		uri = &c->uri;

		res = decode_uri (uri, separator, &newUri, 0);
		if (res != 0)
		{
			LOG (L_ERR,"ERROR: decode_contact_header:Failed decoding contact.Code %d\n", res);
#ifdef STRICT_CHECK
				return res;
#endif
		}
		else
		if (patch (msg, uri->s, uri->len, newUri.s, newUri.len) < 0)
		{
			LOG (L_ERR,"ERROR: decode_contact:lumping failed in mangling port \n");
			return -2;
		}

#ifdef DECODE_ALL_CONTACTS
		while (c->next != NULL)
		{
			c = c->next;
			uri = &c->uri;

			res = decode_uri (uri, separator, &newUri, 0);
			if (res != 0)
				{
				LOG (L_ERR,"ERROR: decode_contact: Failed decoding contact.Code %d\n",res);
#ifdef STRICT_CHECK
				return res;
#endif
				}
			else
			if (patch (msg, uri->s, uri->len, newUri.s, newUri.len) < 0)
			{
				LOG (L_ERR,"ERROR: decode_contact:lumping failed in mangling port \n");
				return -3;
			}
		} // end while 
#endif
	   } // if c!= NULL 
	} // end if 
	else // after parsing still NULL 
		{
			LOG(L_ERR,"ERROR: decode_contact: Unable to parse Contact header\n");
			return -4;
		}
	return 1;
}


static str	s_tcp  = STR_STATIC_INIT("tcp");
static str	s_tls  = STR_STATIC_INIT("tls");
static str	s_sctp = STR_STATIC_INIT("sctp");



int
encode2format (struct sip_msg* msg, str* uri, struct uri_format *format)
{
	int foo;
	char *string, *pos, *start, *end;
	struct sip_uri sipUri;
	int scheme_len;


	if (uri->s == NULL)
		return -1;
	string = uri->s;


	pos = q_memchr (string, '<', uri->len);
	if (pos != NULL)	/* we are only interested of chars inside <> */
	{
		start = q_memchr (string, ':', uri->len);
		if (start == NULL)	return -2;
		if (start - pos < 4) return -3;
		if ((*(start-1)|0x20)=='s' && (start-pos)>4)
			/* if it ends in s: it is a sips or tels uri */
			scheme_len=4;
		else
			scheme_len=3;
		start-=scheme_len;
		end = strchr (start, '>');
		if (end == NULL)
			return -4;	/* must be a match to < */
	}
	else			/* we do not have  <> */
	{
		pos=string;
		start = q_memchr (string, ':', uri->len);
		if (start == NULL)
			return -5;
		if (start - pos < 3)
			return -6;
		if ((*(start-1)|0x20)=='s' && (start-pos)>3)
			/* if it ends in s: it is a sips or tels uri */
			scheme_len=4;
		else
			scheme_len=3;
		start = start - scheme_len;
		end = string + uri->len;
	}
	memset(format,0,sizeof(struct uri_format));
	format->first = start - string + scheme_len+1 /* ':' */;
	format->second = end - string;
	/* --------------------------testing ------------------------------- */
	/* sip:gva@pass@10.0.0.1;;transport=udp>;expires=2 INCORECT BEHAVIOR OF parse_uri,myfunction works good */
	foo = parse_uri (start, end - start, &sipUri);
	if (foo != 0)
	{
		LOG(L_ERR,"ERROR: encode2format: parse_uri failed on [%.*s]."
				"Code %d \n", uri->len, uri->s, foo);
		return foo-10;
	}

	
	format->username = sipUri.user;
	format->password = sipUri.passwd;
	format->ip = sipUri.host;
	format->port = sipUri.port;
	format->protocol = sipUri.transport_val;
	format->transport=sipUri.transport; /* the whole transport header */
	format->rest.s = sipUri.port.s?(sipUri.port.s+sipUri.port.len):
						(sipUri.host.s+sipUri.host.len);
	format->rest.len = (int)(end-format->rest.s);
	format->rcv_ip.s=ip_addr2a(&msg->rcv.src_ip);
	format->rcv_ip.len=strlen(format->rcv_ip.s);
	if (msg->rcv.src_port!=SIP_PORT){
		format->rcv_port.s=
				int2str(msg->rcv.src_port, &format->rcv_port.len);
	}else{
		format->rcv_port.s=0;
		format->rcv_port.len=0;
	}
	if (msg->rcv.proto!=PROTO_UDP){
		switch(msg->rcv.proto){
			case PROTO_TCP:
				format->rcv_proto=s_tcp;
				break;
			case PROTO_TLS:
				format->rcv_proto=s_tls;
				break;
			case PROTO_SCTP:
				format->rcv_proto=s_sctp;
				break;
			default:
				BUG("unknown proto %d\n", msg->rcv.proto);
		}
	}else{
		format->rcv_proto.s=0;
		format->rcv_proto.len=0;
	}
	
#ifdef EXTRA_DEBUG	
	fprintf (stdout, "transport=[%.*s] transportval=[%.*s]\n", sipUri.transport.len,sipUri.transport.s,sipUri.transport_val.len,sipUri.transport_val.s);
	fprintf(stdout,"First %d,second %d\n",format->first,format->second);
	#endif
	
	return 0;

}


int
encode_uri (struct sip_msg* msg, str* uri, char *encoding_prefix,
				char *public_ip,char separator, str * result)
{
	struct uri_format format;
	char *pos;
	int foo,res;

	
	result->s = NULL;
	result->len = 0;
	if (uri->len <= 1)
		return -1;	/* no contact or an invalid one */
	if (public_ip == NULL) 
		{
			LOG(L_ERR,"ERROR: encode_uri: Invalid NULL value for public_ip parameter\n");
			return -2;
		}
	foo = encode2format (msg, uri, &format);
	if (foo < 0)
		{
		LOG(L_ERR,"ERROR: encode_uri: Unable to encode Contact URI"
				" [%.*s].Return code %d\n",uri->len, uri->s, foo);
		return foo - 20;
		}

	/* a complete uri would be sip:username@ip:port;transport=protocol goes to
	 * sip:enc_pref*username*ip*port*protocol@public_ip
	 */

	foo = 1;		/*strlen(separator); */
	result->len = format.first + uri->s+uri->len - format.rest.s +
		strlen (encoding_prefix) + foo +
		format.username.len + foo +
		format.password.len + foo +
		format.ip.len + foo + format.port.len + foo +
		format.protocol.len + foo + format.rcv_ip.len + foo +
		format.rcv_port.len + foo + format.rcv_proto.len +
		1 + strlen (public_ip);
	/* adding one comes from @ */
	result->s = pkg_malloc (result->len);
	pos = result->s;
	if (pos == NULL)
		{
			LOG(L_ERR,"ERROR: encode_uri:Unable to alloc memory\n");
			return -3;
		}
	 	
	res = snprintf(pos,result->len,"%.*s%s%c%.*s%c%.*s%c%.*s%c%.*s%c%.*s%c"
									"%.*s%c%.*s%c%.*s@",
			format.first, uri->s,encoding_prefix,separator,
			format.username.len,format.username.s,separator,
			format.password.len,format.password.s,
			separator,format.ip.len,format.ip.s,separator,format.port.len,
			format.port.s,separator,format.protocol.len,format.protocol.s,
			separator, format.rcv_ip.len, format.rcv_ip.s, separator,
			format.rcv_port.len, format.rcv_port.s, separator,
			format.rcv_proto.len, format.rcv_proto.s
			);

	if ((res < 0 )||(res>result->len)) 
		{
			LOG(L_ERR,"ERROR: encode_uri: Unable to construct new uri.\n");
			if (result->s != NULL) pkg_free(result->s);
			return -4;
		}
	pos = pos + res ;/* overwriting the \0 from snprintf */
	memcpy (pos, public_ip, strlen (public_ip));
	pos = pos + strlen (public_ip);
	/* copy the rest of the parameters and the rest of uri line*/
	memcpy (pos, format.rest.s, uri->s+uri->len - format.rest.s);
	/*memcpy (pos, uri.s + format.second, uri.len - format.second);*/

	/* Because called parse_uri format contains pointers to the inside of msg,
	 *  must not deallocate */

	return 0;
}


int
decode2format (str* uri, char separator, struct uri_format *format)
{
	char *start, *end, *pos,*lastpos;
	str tmp;
	enum {EX_PREFIX=0,EX_USER,EX_PASS,EX_IP,EX_PORT,EX_PROT, EX_RCVIP,
			EX_RCVPORT, EX_RCVPROTO, EX_FINAL} state;

	memset (format, 0, sizeof(struct uri_format));

	if (uri->s == NULL)
		{
		LOG(L_ERR,"ERROR: decode2format: Invalid parameter uri.It is NULL\n");
		return -1;
		}
	/* sip:enc_pref*username*password*ip*port*protocol@public_ip */
	
	start = q_memchr (uri->s, ':', uri->len);
	if (start == NULL)
	{
		LOG(L_ERR,"ERROR: decode2format: Invalid SIP uri.Missing :\n");
		return -2;
	}			/* invalid uri */
	start = start + 1;
	if (start >= (uri->s+uri->len)){
		LOG(L_ERR, "ERROR: decode2format> Invalid sip uri: too short: %.*s\n",
				uri->len, uri->s);
		return -2;
	}
	format->first = start - uri->s;
	
	/* start */

	end = q_memchr(start,'@',uri->len-(start-uri->s));
	if (end == NULL) 
		{
		LOG(L_ERR,"ERROR: decode2format: Invalid SIP uri.Missing @\n");
		return -3;/* no host address found */
		}

#ifdef EXTRA_DEBUG
		fprintf (stdout, "Decoding %.*s\n",(int)(long)(end-start), start);
#endif
	
	state = EX_PREFIX;
	lastpos = start;
	
	for (pos = start;pos<end;pos++)
		{
			if (*pos == separator)
				{
					/* we copy between lastpos and pos */
					tmp.len = pos - lastpos;
					if (tmp.len>0) tmp.s = lastpos;
						else tmp.s = NULL;
					switch (state)
						{
							case EX_PREFIX: state = EX_USER;break;
							case EX_USER:format->username = tmp;state = EX_PASS;break;
							case EX_PASS:format->password = tmp;state = EX_IP;break;
							case EX_IP:format->ip = tmp;state = EX_PORT;break;
							case EX_PORT:format->port = tmp;state = EX_PROT;break;
							case EX_PROT:
										 format->protocol=tmp;
										 state=EX_RCVIP;
										 break;
							case EX_RCVIP:
										 format->rcv_ip=tmp;
										 state=EX_RCVPORT;
										 break;
							case EX_RCVPORT:
										 format->rcv_port=tmp;
										 state=EX_RCVPROTO;
										 break;
							default:
								{
								/* this should not happen, we should find @ not separator */
								return -4;
								break;
								}
						}
						
					lastpos = pos+1;
				
				}
			else
			if (((*pos) == '>')||(*pos == ';'))
				{
				/* invalid chars inside username part */
				return -5;
				}
		}
		
		
	/* we must be in state EX_RCVPROTO and protocol is between lastpos and 
	 * end@ */
	if (state != EX_RCVPROTO) return -6;
	format->rcv_proto.len = end - lastpos;
	if (format->rcv_proto.len>0) format->rcv_proto.s = lastpos;
	/* I should check perhaps that 	after @ there is something */
		
	/* looking for the end of public ip */
	start = end;/*we are now at @ */
	for(pos = start;pos<(uri->s+uri->len);pos++)
		{
			if ((*pos == ';')||(*pos == '>'))
				{
				/* found end */
				format->second = pos - uri->s;
				return 0;
				}
		}
	/* if we are here we did not find > or ; */
	format->second = uri->len;
	return 0;	
	
}


int
decode_uri (str* uri, char separator, str * result, str* dst_uri)
{
	char *pos;
	struct uri_format format;
	int foo;

	result->s = NULL;
	result->len = 0;
	if (dst_uri){
		dst_uri->s=0;
		dst_uri->len=0;
	}

	if ((uri->len <= 0) || (uri->s == NULL))
		{
		LOG(L_ERR,"ERROR: decode_uri: Invalid value for uri\n");
		return -1;
		}

	foo = decode2format (uri, separator, &format);
	if (foo < 0)
		{
		LOG(L_ERR,"ERROR: decode_uri: Error decoding Contact uri .Error code %d\n",foo);
		return foo - 20;
		}
	/* sanity check */
	if (format.ip.len <= 0)
		{
			LOG(L_ERR,"ERROR: decode_uri: Unable to decode host address \n");
			return -2;/* should I quit or ignore ? */
		}			

	if ((format.password.len > 0) && (format.username.len <= 0))
		{
			LOG(L_ERR,"ERROR: decode_uri: Password decoded but no username available\n");
			return -3;
		}
		
	/* a complete uri would be sip:username:password@ip:port;transport=protocol goes to
	 * sip:enc_pref#username#password#ip#port#protocol@public_ip
	 */
	result->len = format.first + (uri->len - format.second);	/* not NULL terminated */
	if (format.username.len > 0) result->len += format.username.len + 1;	//: or @
	if (format.password.len > 0) result->len += format.password.len + 1;	//@
		
	/* if (format.ip.len > 0) */	     result->len += format.ip.len;
		
	if (format.port.len > 0)     result->len += 1 + format.port.len;	//:
	if (format.protocol.len > 0) result->len += 1 + 10 + format.protocol.len;	//;transport=
	
	/* adding one comes from * */
	result->s = pkg_malloc (result->len);
	if (result->s == NULL)
		{
			LOG(L_ERR,"ERROR: decode_contact: Unable to allocate memory\n");
			return -4;
		}
	pos = result->s;
	memcpy (pos, uri->s, format.first);	/* till sip: */
	pos = pos + format.first;
	
	if (format.username.len > 0)
	{
		memcpy (pos, format.username.s, format.username.len);
		pos = pos + format.username.len;
		if (format.password.len > 0)
			memcpy (pos, ":", 1);
		else
			memcpy (pos, "@", 1);
		pos = pos + 1;
	}
	if (format.password.len > 0)
	{
		memcpy (pos, format.password.s, format.password.len);
		pos = pos + format.password.len;
		memcpy (pos, "@", 1);
		pos = pos + 1;
	}
	/* if (format.ip.len > 0) */

		memcpy (pos, format.ip.s, format.ip.len);
		pos = pos + format.ip.len;
	
	if (format.port.len > 0)
	{
		memcpy (pos, ":", 1);
		pos = pos + 1;
		memcpy (pos, format.port.s, format.port.len);
		pos = pos + format.port.len;
	}
	if (format.protocol.len > 0)
	{
		memcpy (pos, ";transport=", 11);
		pos = pos + 11;
		memcpy (pos, format.protocol.s, format.protocol.len);
		pos = pos + format.protocol.len;
	}
	
	memcpy (pos, uri->s + format.second, uri->len - format.second);	/* till end: */
	
	/* dst_uri */
	if (dst_uri && format.rcv_ip.s){
		dst_uri->len=4 /* sip: */ + format.rcv_ip.len;
		if (format.rcv_port.len){
			dst_uri->len+=1 /* : */+format.rcv_port.len;
		}
		if (format.rcv_proto.len){
			dst_uri->len+=TRANSPORT_PARAM_LEN+format.rcv_proto.len;
		}
		dst_uri->s=pkg_malloc(dst_uri->len);
		if (dst_uri->s==0){
			LOG(L_ERR,"ERROR: decode_contact: dst_uri: memory allocation"
					" failed\n");
			dst_uri->len=0;
			pkg_free(result->s);
			result->s=0;
			result->len=0;
			return -4;
		}
		pos=dst_uri->s;
		memcpy(pos, SIP_SCH, SIP_SCH_LEN);
		pos+=SIP_SCH_LEN;
		memcpy(pos, format.rcv_ip.s, format.rcv_ip.len);
		pos+=format.rcv_ip.len;
		if (format.rcv_port.len){
			*pos=':';
			pos++;
			memcpy(pos, format.rcv_port.s, format.rcv_port.len);
			pos+=format.rcv_port.len;
		}
		if (format.rcv_proto.len){
			memcpy(pos, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
			pos+=TRANSPORT_PARAM_LEN;
			memcpy(pos, format.rcv_proto.s, format.rcv_proto.len);
		}
	}
	return 0;
}

