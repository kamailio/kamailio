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




/* APPEARS TO WORK,PROBLEMS ARE DUE TO parse_uri */

int
encode_contact (struct sip_msg *msg, char *encoding_prefix,char *public_ip)
{

	contact_body_t *cb;
	contact_t *c;
	str uri;
	str newUri;
	int res;
	/*
	 * I have a list of contacts in contact->parsed which is of type contact_body_t 
	 * inside i have a contact->parsed->contact which is the head of the list of contacts
	 * inside it is a 
	 * str uri;
	 * struct contact *next;
	 * I just have to visit each uri and encode each uri according to a scheme
	 */

	fprintf (stdout,"---START--------ENCODE CONTACT-----------------\n");
	if (msg->contact == NULL)
		return -1;


	if (msg->contact->parsed == NULL)
		parse_contact (msg->contact);
	if (msg->contact->parsed != NULL)
	{
		cb = (contact_body_t *) msg->contact->parsed;
		c = cb->contacts;
		/* we visit each contact */

		uri = c->uri;
		
		res = encode_uri (uri, encoding_prefix, public_ip,
				      DEFAULT_SEPARATOR, &newUri);
		fprintf (stdout, "newuri.s=[%.*s]\n", newUri.len, newUri.s);
		if (res != 0)
			{
			LOG (L_ERR,"ERROR: Failed encoding contact.Code %d\n", res);
			fprintf (stdout, "Failed encoding contact.Code %d\n", res);
			}
		else
		if (patch (msg, uri.s, uri.len, newUri.s, newUri.len) < 0)
		{
			LOG (L_ERR,"ERROR: lumping failed in mangling port \n");
			return -3;
		}

		/* encoding next contacts too?*/
		while (c->next != NULL)
		{
			c = c->next;
			uri = c->uri;
			//encode uri
			res = encode_uri (uri, encoding_prefix,
					      public_ip, DEFAULT_SEPARATOR,
					      &newUri);
			if (res != 0)
				fprintf (stdout,"Failed encoding contact.Code %d\n",res);
			else
			if (patch (msg, uri.s, uri.len, newUri.s, newUri.len)< 0)
			{
				LOG (L_ERR,"ERROR: lumping failed in mangling port \n");
				return -3;
			}
		}

	}
	fprintf (stdout,"---END--------ENCODE CONTACT-----------------\n");
	return 0;
}


int
decode_contact (struct sip_msg *msg, char *sep,char *unused)
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
	
	res = (int)sep;
	separator = (char)res; /* for warning */
	fprintf (stdout,"---START--------DECODE CONTACT TEXTOPS-----------------\n");
	if (msg->contact == NULL)
		return -1;


	if (msg->contact->parsed == NULL)
		parse_contact (msg->contact);
	if (msg->contact->parsed != NULL)
	{
		cb = (contact_body_t *) msg->contact->parsed;
		c = cb->contacts;
		/* we visit each contact */

		uri = c->uri;

		res = decode_uri (uri, separator, &newUri);
		fprintf (stdout, "newuri.s=[%.*s]\n", newUri.len, newUri.s);
		if (res != 0)
		{
			LOG (L_ERR,"ERROR: Failed decoding contact.Code %d\n", res);
			fprintf (stdout, "Failed decoding contact.Code %d\n", res);
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
				fprintf (stdout,"Failed decoding contact.Code %d\n",res);
			else
			if (patch (msg, uri.s, uri.len, newUri.s, newUri.len) < 0)
			{
				LOG (L_ERR,"ERROR: lumping failed in mangling port \n");
				return -3;
			}
		}

	}
	fprintf (stdout,"---END--------DECODE CONTACT TEXTOPS-----------------\n");
	return 0;
}





int
free_uri_format (struct uri_format format)
{
	printf("Start of Dealocating format \n");
	if (format.username.len > 0)
		pkg_free (format.username.s);
	if (format.password.len > 0)
		pkg_free (format.password.s);
	if (format.ip.len > 0)
		pkg_free (format.ip.s);
	if (format.port.len > 0)
		pkg_free (format.port.s);
	if (format.protocol.len > 0)
		pkg_free (format.protocol.s);
	printf("End of Dealocating format \n");fflush(stdout);
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
		//fprintf (stdout, "PARSING uri with parse uri not ok %d\n", foo);
		return foo;
	}

	(*format).username = sipUri.user;
	(*format).password = sipUri.passwd;
	(*format).ip = sipUri.host;
	(*format).port = sipUri.port;

	string = sipUri.params.s;
	foo = sipUri.params.len;
	start = string;
	while ((pos = q_memchr (start, '=', foo)) != NULL)
	{

		//fprintf(stdout,"TESTING  %.*s\n",pos-tmp,pos-(pos-tmp));
		if ((pos - start) >= 9)	/* perhaps an transport= */
		{
			if (strncasecmp (pos - 9, "transport=", 10) == 0)
			{
				(*format).protocol.s = pos + 1;
				(*format).protocol.len = 3;
				break;
			}
			//fprintf(stdout,"TESTING protocol=%.*s\n",10,pos-9);
		}

		foo = foo - (pos - start);
		if (foo > 1)
			start = pos + 1;
		else
			break;
	}

	//fprintf (stdout, "protocol=%.*s\n", (*format).protocol.len,(*format).protocol.s);
	//fprintf (stdout, "first=%d second=%d\n", (*format).first,(*format).second);
	
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

	fprintf (stdout, "Primit cerere de encodare a [%.*s] cu %s-%s\n", uri.len,
		 uri.s, encoding_prefix, public_ip);
	fflush (stdout);
	foo = encode2format (uri, &format);
	if (foo < 0)
		return foo - 20;

	fprintf(stdout,"user=%.*s ip=%.*s port=%.*s protocol=%.*s\n",format.username.len,format.username.s,format.ip.len,format.ip.s,
	      format.port.len,format.port.s,format.protocol.len,format.protocol.s);   

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
	/* adding one comes from * */
	(*result).s = pkg_malloc ((*result).len);
	pos = (*result).s;
	if (pos == NULL)
	{
		fprintf (stdout, "Unable to alloc result [%d] end=%d\n",
			 (*result).len, format.second);
		goto error2;
	}
	//memcpy (pos, "sip:", 4);pos = pos + 4;
	fprintf (stdout, "Adding [%d] ->%.*s\n", format.first, format.first,uri.s);
	fflush (stdout);
	/* 	
		res = snprintf(pos,(*result).len,"%.*s%s%c%.*s%c%.*s%c%.*s%c%.*s@%s",format.first,uri.s,encoding_prefix,separator,
		format.username.len,format.username.s,separator,format.password.len,format.password.s,
		separator,format.port.len,format.port.s,separator,format.protocol.len,format.protocol.s,public_ip);
		if ((res < 0 )||(res>(*result).len)) goto error2;
		fprintf(stdout,"res= %d\npos=%s\n",res,pos);
		pos = pos + res;
		memcpy (pos, uri.s + format.second, uri.len - format.second);
		//we have on pos[res] a NULL octet
		//it will be NULL terminated
		//I can do hacks like writing fewer octets and overwriting the last one
	
	*/
	memcpy (pos, uri.s, format.first);	// copy all till sip: inclusive 
	pos = pos + format.first;
	
	memcpy (pos, encoding_prefix, strlen (encoding_prefix));
	pos = pos + strlen (encoding_prefix);	// might be optimezed by gcc.Will see 
	memcpy (pos, &separator, foo);
	pos = pos + foo;
	if (format.username.len > 0)
	{
		memcpy (pos, format.username.s, format.username.len);
		pos = pos + format.username.len;
	}
	memcpy (pos, &separator, foo);
	pos = pos + foo;
	if (format.password.len > 0)
	{
		memcpy (pos, format.password.s, format.password.len);
		pos = pos + format.password.len;
	}
	memcpy (pos, &separator, foo);
	pos = pos + foo;
	if (format.ip.len > 0)
	{
		memcpy (pos, format.ip.s, format.ip.len);
		pos = pos + format.ip.len;
	}
	memcpy (pos, &separator, foo);
	pos = pos + foo;
	if (format.port.len > 0)
	{
		memcpy (pos, format.port.s, format.port.len);
		pos = pos + format.port.len;
	}
	memcpy (pos, &separator, foo);
	pos = pos + foo;
	if (format.protocol.len > 0)
	{
		memcpy (pos, format.protocol.s, format.protocol.len);
		pos = pos + format.protocol.len;
	}
	memcpy (pos, "@", 1);
	pos = pos + 1;
	
	memcpy (pos, public_ip, strlen (public_ip));
	pos = pos + strlen (public_ip);	// might be optimezed by gcc.Will see 
	memcpy (pos, uri.s + format.second, uri.len - format.second);
		
	fprintf (stdout, "Adding2 [%d] ->%.*s\n", uri.len - format.second,uri.len - format.second, uri.s + format.second);
	
	free_uri_format (format);


	fprintf (stdout, "NEW NEW uri is->[%.*s]\n", (*result).len, (*result).s);
	return 0;
error2:
	free_uri_format (format);
	return -10;
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
		fprintf (stdout, "username=%.*s\n", (*format).username.len,
			 (*format).username.s);
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
		fprintf (stdout, "password=%.*s\n", (*format).password.len,
			 (*format).password.s);
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
		fprintf (stdout, "ip=%.*s\n", (*format).ip.len, (*format).ip.s);
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
		fprintf (stdout, "port=%.*s\n", (*format).port.len,
			 (*format).port.s);
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
		fprintf (stdout, "protocol=%.*s\n", (*format).protocol.len,
			 (*format).protocol.s);
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
	
	free_uri_format (*format);
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
		return foo - 20;

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
		goto error3;
	}			/* should I quit or ignore ? */
	if (format.port.len > 0)
		(*result).len += 1 + format.port.len;	//:
	if (format.protocol.len > 0)
		(*result).len += 1 + 10 + format.protocol.len;	//;transport=

	fprintf (stdout, "Result size is %d.Original Uri size is %d\n",
		 (*result).len, uri.len);
	/* adding one comes from * */
	(*result).s = pkg_malloc ((*result).len);
	pos = (*result).s;
	fprintf (stdout, "Adding [%d] ->%.*s\n", format.first, format.first,
		 uri.s);
	fflush (stdout);
	memcpy (pos, uri.s, format.first);	/* till sip: */
	pos = pos + format.first;
	//memcpy(pos,"sip:",4);pos = pos + 4;
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
	fprintf (stdout, "Adding2 [%d] ->%.*s\n", uri.len - format.second,
		 uri.len - format.second, uri.s + format.second);
	fflush (stdout);
	memcpy (pos, uri.s + format.second, uri.len - format.second);	/* till end: */

	free_uri_format (format);

	fprintf (stdout, "New decoded uri is->[%.*s]\n", (*result).len,
		 (*result).s);
	return 0;
error3:
	free_uri_format (format);
	return foo;

}

/*
int main()
{
	str uri;
	str res;
	int i;
	char *s = "<sip:enc*gamma**10.0.0.1*1234*udp@100.100.100.100>;expires=2\n";
	uri.s = pkg_malloc(strlen(s));
	uri.len = strlen(s);
	i = encode_uri(uri,"enc","100.1.1.1",'*',&res);
	printf("i=%d\nres=%.*s\n",i,res.len,res.s);
	return 0;
}
*/
