/*
 * Sdp mangler module
 *
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
/* History:
 * --------
 *  2003-04-07 first version.  
 */


#define DEBUG 

#include "contact_ops.h"
#include "utils.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/hf.h"
#include "../../parser/parse_uri.h"
#include "../../parser/contact/parse_contact.h"
#include "../../ut.h"

#include <stdio.h>
#include <string.h>



int
encode_contact (struct sip_msg *msg, char *encoding_prefix,char *public_ip)
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
#ifdef DEBUG
	fprintf (stdout,"---START--------ENCODE CONTACT-----------------\n");
#endif
	if (msg->contact == NULL)
		return -1;

	separator = DEFAULT_SEPARATOR[0];
	if (contact_flds_separator != NULL)
		if (strlen(contact_flds_separator)>=1)
			separator = contact_flds_separator[0];
	
	
	if (msg->contact->parsed == NULL)
		parse_contact (msg->contact);
	if (msg->contact->parsed != NULL)
	{
		cb = (contact_body_t *) msg->contact->parsed;
		c = cb->contacts;
		/* we visit each contact */

		uri = c->uri;
#ifdef DEBUG
		fprintf (stdout, "olduri.s=[%.*s]\n", uri.len, uri.s);
#endif
		res = encode_uri (uri, encoding_prefix, public_ip,separator, &newUri);
		
		if (res != 0)
			{
#ifdef DEBUG
			fprintf (stdout, "Failed encoding contact.Code %d\n", res);
#endif
			LOG (L_ERR,"ERROR: Failed encoding contact.Code %d\n", res);
			}
		else
			if (patch (msg, uri.s, uri.len, newUri.s, newUri.len) < 0)
			{
				LOG (L_ERR,"ERROR: lumping failed in mangling port \n");
				return -3;
			}
#ifdef DEBUG
		fprintf (stdout, "newuri.s=[%.*s]\nnewlen=%d\n", newUri.len, newUri.s,newUri.len);
#endif
		/* encoding next contacts too?*/
		while (c->next != NULL)
		{
			c = c->next;
			uri = c->uri;
			
			res = encode_uri (uri, encoding_prefix,public_ip,separator,&newUri);
			if (res != 0)
				{
#ifdef DEBUG
				fprintf (stdout,"Failed encoding contact.Code %d\n",res);
#endif
				LOG(L_ERR,"ERROR: encode_contact: Failed encode_uri.Code %d\n",res);
				}
			else
			if (patch (msg, uri.s, uri.len, newUri.s, newUri.len)< 0)
			{
				LOG (L_ERR,"ERROR: lumping failed in mangling port \n");
				return -3;
			}
		}

	}
#ifdef DEBUG
	fprintf (stdout,"---END--------ENCODE CONTACT-----------------\n");
#endif
	return 0;
}


int
decode_contact (struct sip_msg *msg,char *unused1,char *unused2)
{

	contact_body_t *cb;
	contact_t *c;
	str uri;
	str newUri;
	char separator;
	int res;
	/*
	 * I have a list of contacts in contact->parsed which is of type contact_body_t 
	 * inside i have a contact->parsed->contact which is the head of the list of contacts
	 * inside it is a 
	 * str uri;
	 * struct contact *next;
	 * I just have to visit each uri and encode each uri according to a scheme
	 */
	
	
#ifdef DEBUG
	fprintf (stdout,"---START--------DECODE CONTACT TEXTOPS-----------------\n");
#endif

	if ((msg->contact == NULL)||((parse_headers(msg,HDR_CONTACT,0) == -1)))
		{
		LOG(L_ERR,"ERROR: decode_contact: no Contact header present\n");
		return -1;
		}

	separator = DEFAULT_SEPARATOR[0];
	if (contact_flds_separator != NULL)
		if (strlen(contact_flds_separator)>=1)
			separator = contact_flds_separator[0];

	if (msg->contact->parsed == NULL)	
		parse_contact (msg->contact);
	if (msg->contact->parsed != NULL)
	{
		cb = (contact_body_t *) msg->contact->parsed;
		c = cb->contacts;
		/* we visit each contact */

		uri = c->uri;

		res = decode_uri (uri, separator, &newUri);
		//fprintf (stdout, "newuri.s=[%.*s]\n", newUri.len, newUri.s);
		if (res != 0)
		{
			LOG (L_ERR,"ERROR: Failed decoding contact.Code %d\n", res);
#ifdef DEBUG
			fprintf (stdout, "Failed decoding contact.Code %d\n", res);
#endif
		}
		else
		if (patch (msg, uri.s, uri.len, newUri.s, newUri.len) < 0)
		{
			LOG (L_ERR,"ERROR: lumping failed in mangling port \n");
			return -3;
		}


		while (c->next != NULL)
		{
			c = c->next;
			uri = c->uri;
			//encode uri
			res = decode_uri (uri, separator, &newUri);
			if (res != 0)
				{
				LOG (L_ERR,"ERROR: decode_contact: Failed decoding contact.Code %d\n",res);
#ifdef DEBUG
				fprintf (stdout,"Failed decoding contact.Code %d\n",res);
#endif
				}
			else
			if (patch (msg, uri.s, uri.len, newUri.s, newUri.len) < 0)
			{
				LOG (L_ERR,"ERROR: lumping failed in mangling port \n");
				return -3;
			}
		}

	}
	else /* after parsing still NULL */
		{
			LOG(L_ERR,"ERROR: decode_contact: Unable to parse Contact header\n");
			return -4;
		}
#ifdef DEBUG
	fprintf (stdout,"---END--------DECODE CONTACT TEXTOPS-----------------\n");
#endif
	return 0;
}





int
free_uri_format (struct uri_format *format)
{
#ifdef DEBUG
	printf("Start of Dealocating format \n");
#endif
	if (format->username.len > 0)
		pkg_free (format->username.s);
	if (format->password.len > 0)
		pkg_free (format->password.s);
	if (format->ip.len > 0)
		pkg_free (format->ip.s);
	if (format->port.len > 0)
		pkg_free (format->port.s);
	if (format->protocol.len > 0)
		pkg_free (format->protocol.s);
	memset(format,0,sizeof(struct uri_format));
#ifdef DEBUG
	printf("End of Dealocating format \n");fflush(stdout);
#endif
	return 0;
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


	pos = q_memchr (string, '<', uri.len);
	if (pos != NULL)	/* we are only interested of chars inside <> */
	{
		start = q_memchr (string, ':', uri.len);
		if (start == NULL)	return -4;
		if (start - pos < 4) return -5;
		start = start - 3;
		end = strchr (start, '>');
		if (end == NULL)
			return -2;	/* must be a match to < */
	}
	else			/* we do not have  <> */
	{
		start = q_memchr (string, ':', uri.len);
		if (start == NULL)
			return -3;
		if (start - pos < 3)
			return -6;
		start = start - 3;
		end = string + uri.len;
	}
	memset(format,0,sizeof(struct uri_format));
	(*format).first = start - string + 4;	/*sip: */
	(*format).second = end - string;
	/* --------------------------testing ------------------------------- */
	/* sip:gva@pass@10.0.0.1;;transport=udp>;expires=2 INCORECT BEHAVIOUR OF parse_uri,myfunction works good */
	foo = parse_uri (start, end - start, &sipUri);
	if (foo != 0)
	{
		LOG(L_ERR,"ERROR: encode2format: parse_uri failed on [%.*s].Code %d \n",uri.len,uri.s,foo);
#ifdef DEBUG
		fprintf (stdout, "PARSING uri with parse uri not ok %d\n", foo);
#endif
		return foo;
	}

	/* fields are directly pointing to parts of the message.They must not be deallocated */
	(*format).username = sipUri.user;
	(*format).password = sipUri.passwd;
	(*format).ip = sipUri.host;
	(*format).port = sipUri.port;
	
	/* locating protocol from transport=udp|tcp */
	string = sipUri.params.s;
	foo = sipUri.params.len;
	start = string;
	while ((pos = q_memchr (start, '=', foo)) != NULL)
	{
		if ((pos - start) >= 9)	/* perhaps an transport= */
		{
			if (strncasecmp (pos - 9, "transport=", 10) == 0)
			{
				(*format).protocol.s = pos + 1;
				(*format).protocol.len = 3;
				break;
			}
#ifdef DEBUG
			fprintf(stdout,"TESTING protocol=%.*s\n",10,pos-9);
#endif
		}

		foo = foo - (pos - start);
		if (foo > 1)
			start = pos + 1;
		else
			break;
	}
	
#ifdef DEBUG	
	fprintf (stdout, "protocol=%.*s\n", (*format).protocol.len,(*format).protocol.s);
	fprintf (stdout, "first=%d second=%d\n", (*format).first,(*format).second);
#endif
	
	return 0;

}


int
encode_uri (str uri, char *encoding_prefix, char *public_ip,char separator, str * result)
{
	struct uri_format format;
	char *pos;
	int foo,res;

	
	(*result).s = NULL;
	(*result).len = 0;
	if (uri.len <= 1)
		return -1;	/* no contact or an invalid one */
	if (public_ip == NULL) 
		{
			LOG(L_ERR,"ERROR: encode_uri: Invalid NULL value for public_ip parameter\n");
			return -2;
		}
#ifdef DEBUG
	fprintf (stdout, "Primit cerere de encodare a [%.*s] cu %s-%s\n", uri.len,uri.s, encoding_prefix, public_ip);
#endif
	fflush (stdout);
	foo = encode2format (uri, &format);
	if (foo < 0)
		{
		LOG(L_ERR,"ERROR: encode_uri: Unable to encode Contact URI [%.*s].Return code %d\n",uri.len,uri.s,foo);
		return foo - 20;
		}
#ifdef DEBUG
	fprintf(stdout,"user=%.*s ip=%.*s port=%.*s protocol=%.*s\n",format.username.len,format.username.s,format.ip.len,format.ip.s,
	     format.port.len,format.port.s,format.protocol.len,format.protocol.s);   
#endif

	/* a complete uri would be sip:username@ip:port;transport=protocol goes to
	 * sip:enc_pref*username*ip*port*protocol@public_ip
	 */

	foo = 1;		/*strlen(separator); */
	(*result).len = format.first + uri.len - format.second +	//ar trebui sa sterg 1
		strlen (encoding_prefix) + foo +
		format.username.len + foo +
		format.password.len + foo +
		format.ip.len + foo + format.port.len + foo +
		format.protocol.len + 1 + strlen (public_ip);
	/* adding one comes from @ */
	(*result).s = pkg_malloc ((*result).len);
	pos = (*result).s;
	if (pos == NULL)
		{
#ifdef DEBUG
			fprintf (stdout, "Unable to alloc result [%d] end=%d\n",(*result).len, format.second);
#endif
			LOG(L_ERR,"ERROR: encode_uri:Unable to alloc memory\n");
			return -3;
		}
#ifdef DEBUG
	fprintf (stdout, "[pass=%d][Alocated %d bytes][first=%d][lengthsec=%d]\nAdding [%d] ->%.*s\n",format.password.len,result->len,format.first,uri.len-format.second,format.first, format.first,uri.s);fflush (stdout);
#endif
	 	
	res = snprintf(pos,(*result).len,"%.*s%s%c%.*s%c%.*s%c%.*s%c%.*s%c%.*s@",format.first,uri.s,encoding_prefix,separator,
	format.username.len,format.username.s,separator,format.password.len,format.password.s,
	separator,format.ip.len,format.ip.s,separator,format.port.len,format.port.s,separator,format.protocol.len,format.protocol.s);

	if ((res < 0 )||(res>(*result).len)) 
		{
			LOG(L_ERR,"ERROR: encode_uri: Unable to construct new uri.\n");
			if ((*result).s != NULL) pkg_free((*result).s);
			return -3;
		}
#ifdef DEBUG
	fprintf(stdout,"res= %d\npos=%s\n",res,pos);
#endif
	pos = pos + res ;/* overwriting the \0 from snprintf */
	memcpy (pos, public_ip, strlen (public_ip));
	pos = pos + strlen (public_ip);
	memcpy (pos, uri.s + format.second, uri.len - format.second);

#ifdef DEBUG	
	fprintf (stdout, "Adding2 [%d] ->%.*s\n", uri.len - format.second,uri.len - format.second, uri.s + format.second);
	fprintf (stdout, "NEW NEW uri is->[%.*s]\n", (*result).len, (*result).s);
#endif

	/* Because called parse_uri format contains pointers to the inside of msg,must not deallocate */
	/* free_uri_format (&format);*/

	return 0;
}


int
decode2format (str uri, char separator, struct uri_format *format)
{
	char *start, *end, *string, *pos;
	int foo;
	int len;

	memset (format, 0, sizeof ((*format)));

	if (uri.s == NULL)
		return -1;
	/* sip:enc_pref*username*password*ip*port*protocol@public_ip */
	len = 0;

	string = uri.s;

	start = q_memchr (string, ':', uri.len - len);
	if (start == NULL)
	{
		foo = -3;
		goto error1;
	}			/* invalid uri */
	start = start + 1;	/* jumping over sip: */
	(*format).first = start - string;
	len = start - string;

	/* jumping over encoding prefix */

	start = q_memchr (start, separator, uri.len - len);
	if (start == NULL)	/* should not be , wrong encoding or */
	{
		foo = -9;
		goto error1;
	}
	if (uri.len - len > 0) start = start + 1;
	len = (start - string);
	/* username */
	end = q_memchr (start, separator, uri.len - len);
	if (end == NULL)	/* should not be , wrong encoding or */
	{
		foo = -4;
		goto error1;
	}
	if ((end - start) != 0)
	{
		(*format).username.s = pkg_malloc (end - start);
		(*format).username.len = end - start;
		memcpy ((*format).username.s, start, end - start);
#ifdef DEBUG
		fprintf (stdout, "username=%.*s\n", (*format).username.len,(*format).username.s);
#endif
	}
	if (uri.len - len > 0) start = end + 1;	/* now start is after * */
	len = (start - string);

	/* password */
	end = q_memchr (start, separator, uri.len - len);
	if (end == NULL)	/* should not be , wrong encoding or */
	{
		foo = -5;
		goto error1;
	}
	if ((end - start) != 0)
	{
		(*format).password.s = pkg_malloc (end - start);
		(*format).password.len = end - start;
		memcpy ((*format).password.s, start, end - start);
#ifdef DEBUG
		fprintf (stdout, "password=%.*s\n", (*format).password.len,(*format).password.s);
#endif
	}
	if (uri.len - len > 0) start = end + 1;	/* now start is after * */
	len = (start - string);

	/* ip */
	end = q_memchr (start, separator, uri.len - len);
	if (end == NULL)	/* should not be , wrong encoding or */
	{
		foo = -6;
		goto error1;
	}
	if ((end - start) != 0)
	{
		(*format).ip.s = pkg_malloc (end - start);
		(*format).ip.len = end - start;
		memcpy ((*format).ip.s, start, end - start);
#ifdef DEBUG
		fprintf (stdout, "ip=%.*s\n", (*format).ip.len, (*format).ip.s);
#endif
	}
	if (uri.len - len > 0) start = end + 1;	/* now start is after * */
	len = (start - string);

	/* port */
	end = q_memchr (start, separator, uri.len - len);
	if (end == NULL)	/* should not be , wrong encoding or */
	{
		foo = -7;
		goto error1;
	}
	if ((end - start) != 0)
	{
		(*format).port.s = pkg_malloc (end - start);
		(*format).port.len = end - start;
		memcpy ((*format).port.s, start, end - start);
#ifdef DEBUG
		fprintf (stdout, "port=%.*s\n", (*format).port.len,(*format).port.s);
#endif
	}
	if (uri.len - len > 0) start = end + 1;	/* now start is after * */
	len = (start - string);

	/* protocol */
	end = q_memchr (start, '@', uri.len - len);	/* ...*udp@PUBLIC */
	if (end == NULL)	/* should not be , wrong encoding or */
	{
		foo = -8;
		goto error1;
	}

	if ((end - start) != 0)
	{
		(*format).protocol.s = pkg_malloc (end - start);
		(*format).protocol.len = end - start;
		memcpy ((*format).protocol.s, start, end - start);
		start = end + 1;	/* now start is after @ */
#ifdef DEBUG
		fprintf (stdout, "protocol=%.*s\n", (*format).protocol.len,(*format).protocol.s);
#endif
	}
	if (uri.len - len > 0) start = end + 1;	/* now start is after * looking for end */
	len = (start - string);
	end = q_memchr (start, '>', uri.len - len);
	pos = q_memchr (start, ';', uri.len - len);
	if ((end == NULL) && (pos == NULL))
		end = string + uri.len;
	else if (pos == NULL) ;
	else if (end == NULL)
		end = pos;

	(*format).second = end - string;
	return 0;
error1:
	
	free_uri_format (format);
	return foo;

}


int
decode_uri (str uri, char separator, str * result)
{
	char *pos;
	struct uri_format format;
	int foo;

	(*result).s = NULL;
	(*result).len = 0;

	if ((uri.len <= 0) || (uri.s == NULL))
		return -1;

	foo = decode2format (uri, separator, &format);
	if (foo < 0)
		{
		LOG(L_ERR,"ERROR: decode_uri: Error decoding Contact uri [%.*s].Error code %d\n",uri.len,uri.s,foo);
		return foo - 20;
		}

	/* a complete uri would be sip:username:password@ip:port;transport=protocol goes to
	 * sip:enc_pref#username#password#ip#port#protocol@public_ip
	 */
	(*result).len = format.first + (uri.len - format.second);	/* not NULL terminated */
	if (format.username.len > 0)
		(*result).len += format.username.len + 1;	//: or @
	if (format.password.len > 0)
		(*result).len += format.password.len + 1;	//@
	if (format.ip.len > 0)
		(*result).len += format.ip.len;
	else
	{
		foo = -3;
		LOG(L_ERR,"ERROR: decode_uri: Unable to decode host address \n");
		goto error3;
	}			/* should I quit or ignore ? */
	if (format.port.len > 0)
		(*result).len += 1 + format.port.len;	//:
	if (format.protocol.len > 0)
		(*result).len += 1 + 10 + format.protocol.len;	//;transport=
#ifdef DEBUG
	fprintf (stdout, "Result size is %d.Original Uri size is %d\n",(*result).len, uri.len);
#endif
	/* adding one comes from * */
	(*result).s = pkg_malloc ((*result).len);
	pos = (*result).s;
#ifdef DEBUG
	fprintf (stdout, "Adding [%d] ->%.*s\n", format.first, format.first,uri.s);fflush (stdout);
#endif
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
		if (format.username.len <= 0)
		{
			foo = -4;
			LOG(L_ERR,"ERROR: decode_uri: Password decoded but now username available\n");
			goto error3;
		}		/* error: this is going to look like a username */
		memcpy (pos, format.password.s, format.password.len);
		pos = pos + format.password.len;
		memcpy (pos, "@", 1);
		pos = pos + 1;
	}
	if (format.ip.len > 0)
	{
		memcpy (pos, format.ip.s, format.ip.len);
		pos = pos + format.ip.len;
	}
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

#ifdef DEBUG
	fprintf (stdout, "Adding2 [%d] ->%.*s\n", uri.len - format.second,uri.len - format.second, uri.s + format.second);fflush (stdout);
#endif

	memcpy (pos, uri.s + format.second, uri.len - format.second);	/* till end: */
	free_uri_format (&format);

#ifdef DEBUG
	fprintf (stdout, "New decoded uri is->[%.*s]\n", (*result).len,(*result).s);
#endif

	return 0;

error3:
	free_uri_format (&format);
	return foo;
}

