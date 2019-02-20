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
 *
 */

/*!
 * \file
 * \brief SIP-utils :: Mangler module
 * \ingroup siputils
 * - Module; \ref siputils
 */


#define STRICT_CHECK 1

#include "contact_ops.h"
#include "utils.h"
#include "../../core/mem/mem.h"
#include "../../core/data_lump.h"
#include "../../core/parser/hf.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/ut.h"

#include <stdio.h>
#include <string.h>


int encode_contact (struct sip_msg *msg, char *encoding_prefix,char *public_ip)
{
	contact_body_t *cb;
	contact_t *c;
	str uri;
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

	if((msg->contact == NULL) && ((parse_headers(msg, HDR_CONTACT_F, 0) == -1)
			|| (msg->contact == NULL))) {
		LM_ERR("no Contact header present\n");
		return -1;
	}

	separator = DEFAULT_SEPARATOR[0];
	if(contact_flds_separator != NULL)
		if(strlen(contact_flds_separator) >= 1)
			separator = contact_flds_separator[0];

	if(msg->contact->parsed == NULL) {
		if(parse_contact(msg->contact) < 0 || msg->contact->parsed == NULL) {
			LM_ERR("contact parsing failed\n");
			return -4;
		}
	}

	cb = (contact_body_t *)msg->contact->parsed;
	c = cb->contacts;
	/* we visit each contact */
	if(c != NULL) {
		uri = c->uri;
		res = encode_uri(uri, encoding_prefix, public_ip, separator, &newUri);

		if(res != 0) {
			LM_ERR("failed encoding contact.Code %d\n", res);
#ifdef STRICT_CHECK
			return res;
#endif
		} else if(patch(msg, uri.s, uri.len, newUri.s, newUri.len) < 0) {
			LM_ERR("lumping failed in mangling port \n");
			return -2;
		}

/* encoding next contacts too?*/
#ifdef ENCODE_ALL_CONTACTS
		while(c->next != NULL) {
			c = c->next;
			uri = c->uri;

			res = encode_uri(
					uri, encoding_prefix, public_ip, separator, &newUri);
			if(res != 0) {
				LM_ERR("failed encode_uri.Code %d\n", res);
#ifdef STRICT_CHECK
				return res;
#endif
			} else if(patch(msg, uri.s, uri.len, newUri.s, newUri.len) < 0) {
				LM_ERR("lumping failed in mangling port \n");
				return -3;
			}
		} /* while */
#endif	/* ENCODE_ALL_CONTACTS */
	}	 /* if c != NULL */

	return 1;
}

	int
decode_contact (struct sip_msg *msg,char *unused1,char *unused2)
{

	str uri;
	str newUri;
	char separator;
	int res;

	uri.s = 0;
	uri.len = 0;

	LM_DBG("[%.*s]\n", 75, msg->buf);

	separator = DEFAULT_SEPARATOR[0];
	if (contact_flds_separator != NULL)
		if (strlen(contact_flds_separator)>=1)
			separator = contact_flds_separator[0];

	if ((msg->new_uri.s == NULL) || (msg->new_uri.len == 0))
	{
		uri = msg->first_line.u.request.uri;
		if (uri.s == NULL)
			return -1;
	}
	else
	{
		uri = msg->new_uri;
	}

	res = decode_uri (uri, separator, &newUri);

	if (res == 0)
		LM_DBG("newuri.s=[%.*s]\n", newUri.len, newUri.s);

	if (res != 0)
	{
		LM_ERR("failed decoding contact.Code %d\n", res);
#ifdef STRICT_CHECK
		return res;
#endif
	}
	else
	{
		/* we do not modify the original first line */
		if ((msg->new_uri.s == NULL) || (msg->new_uri.len == 0))
			msg->new_uri = newUri;
		else
		{
			pkg_free(msg->new_uri.s);
			msg->new_uri = newUri;
		}
	}
	return 1;
}

	int
decode_contact_header (struct sip_msg *msg,char *unused1,char *unused2)
{

	contact_body_t *cb;
	contact_t *c;
	str uri;
	str newUri;
	char separator;
	int res;
	str* ruri;

	if ((msg->contact == NULL)&&((parse_headers(msg,HDR_CONTACT_F,0) == -1) ||
				(msg->contact== NULL) ))
	{
		LM_ERR("no Contact header present\n");
		return -1;
	}

	separator = DEFAULT_SEPARATOR[0];
	if (contact_flds_separator != NULL)
		if (strlen(contact_flds_separator)>=1)
			separator = contact_flds_separator[0];

	LM_DBG("Using separator [%c]\n",separator);
	ruri = GET_RURI(msg);
	LM_DBG("New uri [%.*s]\n", ruri->len, ruri->s);
	ruri = &msg->first_line.u.request.uri;
	LM_DBG("Initial uri [%.*s]\n", ruri->len, ruri->s);

	if(msg->contact->parsed == NULL) {
		if(parse_contact(msg->contact) < 0 || msg->contact->parsed == NULL) {
			LM_ERR("contact parsing failed\n");
			return -4;
		}
	}

	cb = (contact_body_t *)msg->contact->parsed;
	c = cb->contacts;
	// we visit each contact
	if(c != NULL) {
		uri = c->uri;

		res = decode_uri(uri, separator, &newUri);
		LM_DBG("newuri.s=[%.*s]\n", newUri.len, newUri.s);
		if(res != 0) {
			LM_ERR("failed decoding contact.Code %d\n", res);
#ifdef STRICT_CHECK
			return res;
#endif
		} else if(patch(msg, uri.s, uri.len, newUri.s, newUri.len) < 0) {
			LM_ERR("lumping failed in mangling port \n");
			return -2;
		}

#ifdef DECODE_ALL_CONTACTS
		while(c->next != NULL) {
			c = c->next;
			uri = c->uri;

			res = decode_uri(uri, separator, &newUri);
			if(res != 0) {
				LM_ERR("failed decoding contact.Code %d\n", res);
#ifdef STRICT_CHECK
				return res;
#endif
			} else if(patch(msg, uri.s, uri.len, newUri.s, newUri.len) < 0) {
				LM_ERR("lumping failed in mangling port \n");
				return -3;
			}
		} // end while
#endif
	} // if c!= NULL

	return 1;
}




	int
encode2format (str uri, struct uri_format *format)
{
	int foo;
	char *string, *pos, *start, *end;
	struct sip_uri sipUri;


	if (uri.s == NULL)
		return -1;
	string = uri.s;

	pos = memchr (string, '<', uri.len);
	if (pos != NULL)	/* we are only interested of chars inside <> */
	{
		/* KD: I think this can be removed as the parsed contact removed <> already */
		start = memchr (string, ':', uri.len);
		if (start == NULL)	return -2;
		if (start - pos < 4) return -3;
		start = start - 3;
		end = strchr (start, '>');
		if (end == NULL)
			return -4;	/* must be a match to < */
	}
	else			/* we do not have  <> */
	{
		start = memchr (string, ':', uri.len);
		if (start == NULL)
			return -5;
		if (start - string < 3)
			return -6;
		/* KD: FIXME: Looks like this code can not handle 'sips' URIs and discards all other URI parameters! */
		start = start - 3;
		end = string + uri.len;
	}
	memset(format,0,sizeof(struct uri_format));
	format->first = start - string + 4;	/*sip: */
	format->second = end - string;
	/* --------------------------testing ------------------------------- */
	/* sip:gva@pass@10.0.0.1;;transport=udp>;expires=2 INCORECT BEHAVIOR OF parse_uri,myfunction works good */
	foo = parse_uri (start, end - start, &sipUri);
	if (foo != 0)
	{
		LM_ERR("parse_uri failed on [%.*s].Code %d \n",uri.len,uri.s,foo);
		LM_DBG("PARSING uri with parse uri not ok [%d]\n", foo);
		return foo-10;
	}


	format->username = sipUri.user;
	format->password = sipUri.passwd;
	format->ip = sipUri.host;
	format->port = sipUri.port;
	format->protocol = sipUri.transport_val;

	LM_DBG("First and second format [%d][%d] transport=[%.*s] transportval=[%.*s]\n",
		format->first, format->second,
		sipUri.transport.len, sipUri.transport.s,
		sipUri.transport_val.len, sipUri.transport_val.s);

	return 0;

}


	int
encode_uri (str uri, char *encoding_prefix, char *public_ip,char separator, str * result)
{
	struct uri_format format;
	char *pos;
	int foo,res;

	result->s = NULL;
	result->len = 0;
	if (uri.len <= 1)
		return -1;	/* no contact or an invalid one */
	if (public_ip == NULL)
	{
		LM_ERR("invalid NULL value for public_ip parameter\n");
		return -2;
	}

	LM_DBG("Encoding request for [%.*s] with [%s]-[%s]\n", uri.len,uri.s, encoding_prefix, public_ip);

	foo = encode2format (uri, &format);
	if (foo < 0)
	{
		LM_ERR("unable to encode Contact URI [%.*s].Return code %d\n",uri.len,uri.s,foo);
		return foo - 20;
	}
	LM_DBG("user=%.*s ip=%.*s port=%.*s protocol=%.*s\n",format.username.len,format.username.s,format.ip.len,format.ip.s,
			format.port.len,format.port.s,format.protocol.len,format.protocol.s);

	/* a complete uri would be sip:username@ip:port;transport=protocol goes to
	 * sip:enc_pref*username*ip*port*protocol@public_ip
	 */

	foo = 1;		/*strlen(separator); */
	result->len = format.first + uri.len - format.second +	//ar trebui sa sterg 1
		strlen (encoding_prefix) + foo +
		format.username.len + foo +
		format.password.len + foo +
		format.ip.len + foo + format.port.len + foo +
		format.protocol.len + 1 + strlen (public_ip);
	/* adding one comes from @ */
	result->s = pkg_malloc (result->len);
	pos = result->s;
	if (pos == NULL)
	{
		LM_DBG("Unable to alloc result [%d] end=[%d]\n",
			result->len, format.second);
		LM_ERR("unable to alloc pkg memory\n");
		return -3;
	}
	LM_DBG("pass=[%d]i: allocated [%d], bytes.first=[%d] lengthsec=[%d]; adding [%d]->[%.*s]\n",
		format.password.len, result->len, format.first, uri.len-format.second, format.first, format.first,uri.s);

	res = snprintf(pos,result->len,"%.*s%s%c%.*s%c%.*s%c%.*s%c%.*s%c%.*s@",format.first,uri.s,encoding_prefix,separator,
			format.username.len,format.username.s,separator,format.password.len,format.password.s,
			separator,format.ip.len,format.ip.s,separator,format.port.len,format.port.s,separator,format.protocol.len,format.protocol.s);

	if ((res < 0 )||(res>result->len))
	{
		LM_ERR("unable to construct new uri.\n");
		if (result->s != NULL) pkg_free(result->s);
		return -4;
	}
	LM_DBG("res= %d\npos=%s\n",res,pos);
	pos = pos + res ;/* overwriting the \0 from snprintf */
	memcpy (pos, public_ip, strlen (public_ip));
	pos = pos + strlen (public_ip);
	memcpy (pos, uri.s + format.second, uri.len - format.second);

	LM_DBG("Adding [%.*s] => new uri [%.*s]\n",
		uri.len - format.second, uri.s + format.second,
		result->len, result->s);

	/* Because called parse_uri format contains pointers to the inside of msg,must not deallocate */

	return 0;
}


	int
decode2format (str uri, char separator, struct uri_format *format)
{
	char *start, *end, *pos,*lastpos;
	str tmp;
	enum {EX_PREFIX=0,EX_USER,EX_PASS,EX_IP,EX_PORT,EX_PROT,EX_FINAL} state;

	//memset (format, 0, sizeof ((struct uri_format)));

	if (uri.s == NULL)
	{
		LM_ERR("invalid parameter uri.It is NULL\n");
		return -1;
	}

	/* sip:enc_pref*username*password*ip*port*protocol@public_ip */

	start = memchr (uri.s, ':', uri.len);
	if (start == NULL)
	{
		LM_ERR("invalid SIP uri.Missing :\n");
		return -2;
	}			/* invalid uri */
	start = start + 1;	/* jumping over sip: ATENTIE LA BUFFER OVERFLOW DACA E DOAR sip: */
	format->first = start - uri.s;

	/* start */

	end = memchr(start,'@',uri.len-(start-uri.s));
	if (end == NULL)
	{
		LM_ERR("invalid SIP uri.Missing @\n");
		return -3;/* no host address found */
	}

	LM_DBG("Decoding [%.*s]\n", (int)(long)(end-start), start);

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
				default:
					{
						/* this should not happen, we should find @ not separator */
						return -4;
						break;
					}
			}

			lastpos = pos+1;

		}
	}


	/* we must be in state EX_PROT and protocol is between lastpos and end@ */
	if (state != EX_PROT) return -6;
	format->protocol.len = end - lastpos;
	if (format->protocol.len>0) format->protocol.s = lastpos;
	else format->protocol.s = NULL;
	/* I should check perhaps that 	after @ there is something */

	LM_DBG("username=[%.*s] password=[%.*s] ip=[%.*s] port=[%.*s] protocol=[%.*s]\n",
		format->username.len,format->username.s,
		format->password.len,format->password.s,
		format->ip.len, format->ip.s,
		format->port.len,format->port.s,
		format->protocol.len,format->protocol.s);

	/* looking for the end of public ip */
	start = end;/*we are now at @ */
	for(pos = start;pos<uri.s+uri.len;pos++)
	{
		if ((*pos == ';')||(*pos == '>'))
		{
			/* found end */
			format->second = pos - uri.s;
			return 0;
		}
	}
	/* if we are here we did not find > or ; */
	format->second = uri.len;
	return 0;

}


	int
decode_uri (str uri, char separator, str * result)
{
	char *pos;
	struct uri_format format;
	int foo;

	result->s = NULL;
	result->len = 0;

	if ((uri.len <= 0) || (uri.s == NULL))
	{
		LM_ERR("invalid value for uri\n");
		return -1;
	}

	foo = decode2format (uri, separator, &format);
	if (foo < 0)
	{
		LM_ERR("failed to decode Contact uri .Error code %d\n",foo);
		return foo - 20;
	}
	/* sanity check */
	if (format.ip.len <= 0)
	{
		LM_ERR("unable to decode host address \n");
		return -2;/* should I quit or ignore ? */
	}

	if ((format.password.len > 0) && (format.username.len <= 0))
	{
		LM_ERR("password decoded but no username available\n");
		return -3;
	}

	/* a complete uri would be sip:username:password@ip:port;transport=protocol goes to
	 * sip:enc_pref#username#password#ip#port#protocol@public_ip
	 */
	result->len = format.first + (uri.len - format.second);	/* not NULL terminated */
	if (format.username.len > 0) result->len += format.username.len + 1;	//: or @
	if (format.password.len > 0) result->len += format.password.len + 1;	//@

	/* if (format.ip.len > 0) */	     result->len += format.ip.len;

	if (format.port.len > 0)     result->len += 1 + format.port.len;	//:
	if (format.protocol.len > 0) result->len += 1 + 10 + format.protocol.len;	//;transport=
	LM_DBG("Result size is [%d]. Original Uri size is [%d].\n", result->len, uri.len);

	/* adding one comes from * */
	result->s = pkg_malloc (result->len);
	if (result->s == NULL)
	{
		LM_ERR("unable to allocate pkg memory\n");
		return -4;
	}
	pos = result->s;

	LM_DBG("Adding [%.*s]\n", format.first, uri.s);

	memcpy (pos, uri.s, format.first);	/* till sip: */
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

	LM_DBG("Adding2 [%.*s]\n", uri.len - format.second, uri.s + format.second);

	memcpy (pos, uri.s + format.second, uri.len - format.second);	/* till end: */

	LM_DBG("New decoded uri [%.*s]\n", result->len,result->s);

	return 0;
}

