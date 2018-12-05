/**
 * Copyright (C) 2018 Jose Luis Verdeguer
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

#include <string.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/sr_module.h"
#include "security.h"


int count_chars(char *val, char c);


/* Count chars of a string */
int count_chars(char *val, char c)
{
    int cont = 0;
    int i;
    
    for (i = 0; i < strlen(val); i++)
    {     
        if (val[i] == c) cont++;
    }

    return cont;
}


/* Search for illegal characters in User-agent header */
int check_sqli_ua(struct sip_msg *msg)
{
	char *val = NULL;

	if (msg==NULL) return -1;
	if (parse_headers(msg, HDR_USERAGENT_F, 0)!=0) return 1;
	if (msg->user_agent==NULL || msg->user_agent->body.s==NULL) return 1;

	val = (char*)pkg_malloc(msg->user_agent->body.len+1);
	if (val==NULL)
	{
		LM_ERR("Cannot allocate pkg memory\n");
		return -1;
	}
	strncpy(val, msg->user_agent->body.s, msg->user_agent->body.len);
	val[msg->user_agent->body.len] = '\0';
	
	if (strstr(val, "'") || strstr(val, "\"") || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
	{
		LM_ERR("User-agent header (%s) has illegal characters. Posible SQLi\n", val);
		pkg_free(val);
		return 0;
	}

	if (val) pkg_free(val);
	return 1;
}


/* Search for illegal characters in To header */
int check_sqli_to(struct sip_msg *msg)
{
	struct to_body *to;
	struct sip_uri parsed_uri;
	char *val = NULL;

	if (msg==NULL) return -1;
	if(parse_to_header(msg) < 0) return -1;
	if (msg->to==NULL || msg->to->body.s==NULL) return -1;

	to = get_to(msg);
	if (to != NULL)
	{
		val = (char*)pkg_malloc(to->display.len+1);
		if (val==NULL)
		{
			LM_ERR("Cannot allocate pkg memory\n");
			return -1;
		}
		strncpy(val, to->display.s, to->display.len);
		val[to->display.len] = '\0';

		if (strstr(val, "'") || (strstr(val, "\"") && count_chars(val, '"') != 2) || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
		{
			LM_ERR("Possible SQLi detected in to name (%s)\n", val);
			pkg_free(val);
			return 0;
		}

	        if (parse_uri(to->uri.s, to->uri.len, &parsed_uri)==0)
	        {
			pkg_free(val);
			val = (char*)pkg_malloc(parsed_uri.user.len+1);
			if (val==NULL)
			{
				LM_ERR("Cannot allocate pkg memory\n");
				return -1;
			}
			strncpy(val, parsed_uri.user.s, parsed_uri.user.len);
			val[parsed_uri.user.len] = '\0';

			if (strstr(val, "'") || strstr(val, "\"") || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
			{
				LM_ERR("Possible SQLi detected in to user (%s)\n", val);
				pkg_free(val);
				return 0;
			}

			pkg_free(val);
			val = (char*)pkg_malloc(parsed_uri.host.len+1);
			if (val==NULL)
			{
				LM_ERR("Cannot allocate pkg memory\n");
				return -1;
			}
			strncpy(val, parsed_uri.host.s, parsed_uri.host.len);
			val[parsed_uri.host.len] = '\0';

			if (strstr(val, "'") || strstr(val, "\"") || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "+") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
			{
				LM_ERR("Possible SQLi detected in to domain (%s)\n", val);
				pkg_free(val);
				return 0;
			}
	        }
	}

	if (val) pkg_free(val);
	return 1;
}

/* Search for illegal characters in From header */
int check_sqli_from(struct sip_msg *msg)
{
	struct to_body *from;
	struct sip_uri parsed_uri;
	char *val = NULL;

	if (msg==NULL) return -1;
	if(parse_from_header(msg) < 0) return -1;
	if (msg->from==NULL || msg->from->body.s==NULL) return -1;

	from = get_from(msg);
	if (from != NULL)
	{
		val = (char*)pkg_malloc(from->display.len+1);
		if (val==NULL)
		{
			LM_ERR("Cannot allocate pkg memory\n");
			return -1;
		}
		strncpy(val, from->display.s, from->display.len);
		val[from->display.len] = '\0';

		if (strstr(val, "'") || (strstr(val, "\"") && count_chars(val, '"') != 2) || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
		{
			LM_ERR("Possible SQLi detected in from name (%s)\n", val);
			pkg_free(val);
			return 0;
		}

	        if (parse_uri(from->uri.s, from->uri.len, &parsed_uri)==0)
	        {
			pkg_free(val);
			val = (char*)pkg_malloc(parsed_uri.user.len+1);
			if (val==NULL)
			{
				LM_ERR("Cannot allocate pkg memory\n");
				return -1;
			}
			strncpy(val, parsed_uri.user.s, parsed_uri.user.len);
			val[parsed_uri.user.len] = '\0';

			if (strstr(val, "'") || strstr(val, "\"") || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
			{
				LM_ERR("Possible SQLi detected in from user (%s)\n", val);
				pkg_free(val);
				return 0;
			}

			pkg_free(val);
			val = (char*)pkg_malloc(parsed_uri.host.len+1);
			if (val==NULL)
			{
				LM_ERR("Cannot allocate pkg memory\n");
				return -1;
			}
			strncpy(val, parsed_uri.host.s, parsed_uri.host.len);
			val[parsed_uri.host.len] = '\0';

			if (strstr(val, "'") || strstr(val, "\"") || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "+") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
			{
				LM_ERR("Possible SQLi detected in from domain (%s)\n", val);
				pkg_free(val);
				return 0;
			}
	        }
	}

	if (val) pkg_free(val);
	return 1;
}


/* Search for illegal characters in Contact header */
int check_sqli_contact(struct sip_msg *msg)
{
	str contact = {NULL, 0};
	struct sip_uri parsed_uri;
	char *val = NULL;

	if (msg==NULL) return -1;
	if (parse_headers(msg, HDR_CONTACT_F, 0)!=0) return -1;
	if (msg->contact==NULL || msg->contact->body.s==NULL) return -1;

	if (!msg->contact->parsed && (parse_contact(msg->contact) < 0))
	{
		LM_ERR("Error parsing contact header (%.*s)\n", msg->contact->body.len, msg->contact->body.s);
		return -1;
	}
	
	if (((contact_body_t*)msg->contact->parsed)->contacts && ((contact_body_t*)msg->contact->parsed)->contacts->uri.s != NULL && ((contact_body_t*)msg->contact->parsed)->contacts->uri.len > 0)
	{
		contact.s = ((contact_body_t*)msg->contact->parsed)->contacts->uri.s;
		contact.len = ((contact_body_t*)msg->contact->parsed)->contacts->uri.len;
	}
	if (contact.s != NULL)
	{
		if (parse_uri(contact.s, contact.len, &parsed_uri) < 0)
		{
			LM_ERR("Error parsing contact uri header (%.*s)\n", contact.len, contact.s);
			return -1;
		}

		val = (char*)pkg_malloc(parsed_uri.user.len+1);
		if (val==NULL)
		{
			LM_ERR("Cannot allocate pkg memory\n");
			return -1;
		}
		strncpy(val, parsed_uri.user.s, parsed_uri.user.len);
		val[parsed_uri.user.len] = '\0';

		if (strstr(val, "'") || strstr(val, "\"") || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
		{
			LM_ERR("Possible SQLi detected in contact user (%s)\n", val);
			pkg_free(val);
			return 0;
		}

		pkg_free(val);
		val = (char*)pkg_malloc(parsed_uri.host.len+1);
		if (val==NULL)
		{
			LM_ERR("Cannot allocate pkg memory\n");
			return -1;
		}
		strncpy(val, parsed_uri.host.s, parsed_uri.host.len);
		val[parsed_uri.host.len] = '\0';

		if (strstr(val, "'") || strstr(val, "\"") || strstr(val, "--") || strstr(val, "=") || strstr(val, "#") || strstr(val, "+") || strstr(val, "%27") || strstr(val, "%24") || strstr(val, "%60"))
		{
			LM_ERR("Possible SQLi detected in contact domain (%s)\n", val);
			pkg_free(val);
			return 0;
		}
        }

	if (val) pkg_free(val);
	return 1;
}
