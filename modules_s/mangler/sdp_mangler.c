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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "sdp_mangler.h"
#include "ip_helper.h"
#include "utils.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/hf.h"
#include "../../parser/parse_uri.h"
#include "../../parser/contact/parse_contact.h"
#include "../../ut.h"


#define DEBUG


int
sdp_mangle_port (struct sip_msg *msg, char *offset, char *unused)
{
	int oldContentLength, newContentLength, oldlen, err, oldPort, newPort,
		diff, offsetValue,len,off,ret,needToDealocate;
	
	struct lump *l;
	regmatch_t pmatch;
	regex_t *re;
	char *s, *pos,*begin,*key;
	char buf[6];
	
	
	
	key = PORT_REGEX;
	/*
	 * Checking if msg has a payload
	 */
	if (msg == NULL)
		return -5;
	oldContentLength = strtol (msg->content_length->body.s, NULL, 10);	/* de inlocuit ? */
	if (oldContentLength <= 0)
		return -1;
	/*
	if (offset == NULL)
		return -2;
	if (sscanf (offset, "%d", &offsetValue) != 1)
	{
		return -3;
	}
	*/
	offsetValue = (int)offset;
#ifdef DEBUG
	fprintf(stdout,"===============OFFSET = %d\n",offsetValue);
#endif
	
	if ((offsetValue < -65536) || (offsetValue > 65536))
	{
		return -4;
	}
	begin = msg->buf + msg->first_line.len;	// inlocuiesc cu begin = getbody */
	ret = -1;

	/* try to use precompiled expressions */
	needToDealocate = 0;
	if (portExpression != NULL) 
		{
		re = portExpression;
#ifdef DEBUG
		fprintf(stdout,"Using PRECOMPILED expression for port ...\n");
#endif
		}
		else /* we are not using precompiled expressions */
			{
			re = pkg_malloc(sizeof(regex_t));
			if (re == NULL)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_port: Unable to allocate re\n");
				return -9;
				}
			needToDealocate = 1;
			if ((regcomp (re, key, REG_EXTENDED)) != 0)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_port: Unable to compile %s \n",key);
				return -5;
				}
#ifdef DEBUG
		fprintf(stdout,"Using ALLOCATED expression for port ...\n");
#endif

			}
	
	diff = 0;
	while (begin < msg->buf + msg->len
	       && regexec (re, begin, 1, &pmatch, 0) == 0)
	{
		off = begin - msg->buf;
		if (pmatch.rm_so == -1)
		{
			LOG (L_ERR, "ERROR: replace_all_f: offset unknown\n");
			return -1;
		}

		pos = (char *) memrchr (begin + pmatch.rm_so, ' ',
					pmatch.rm_eo - pmatch.rm_so);
		pos++;		/* jumping over space */
		oldlen = (pmatch.rm_eo - pmatch.rm_so) - (pos - (begin + pmatch.rm_so));	/* port length */

		/* convert port to int */
		oldPort = str2s (pos, oldlen, &err);
		if (err)
			return -4;
		if ((oldPort < 1) || (oldPort > 65536))	/* we silently fail,we ignore this match or return -11 */
		{
			goto continue1;
		}
		newPort = oldPort + offsetValue;
		/* new port is between 1 and 65536, or so should be */
		if ((newPort < 1) || (newPort > 65536))	/* we silently fail,we ignore this match */
		{
			goto continue1;
		}

		len = 1;
		while ((newPort = (newPort / 10)) != 0)
			len++;
		newPort = oldPort + offsetValue;

		/* deleting old port */
		if ((l =
		     del_lump (&msg->add_rm,
			       pmatch.rm_so + off + (pos -
						     (begin + pmatch.rm_so)),
			       oldlen, 0)) == 0)
		{
			LOG (L_ERR,
			     "ERROR: replace_all_f: del_lump failed\n");
			return -1;
		}
		s = pkg_malloc (len);
		if (s == 0)
		{
			LOG (L_ERR,
			     "ERROR: replace_f: mem. allocation failure\n");
			return -1;
		}
		snprintf (buf, len + 1, "%u", newPort);	/* converting to string */
		memcpy (s, buf, len);

		if (insert_new_lump_after (l, s, len, 0) == 0)
		{
			LOG (L_ERR, "ERROR: could not insert new lump\n");
			pkg_free (s);
			return -1;
		}
		diff = diff + len /*new length */  - oldlen;
		/* new cycle */
		ret++;
	      continue1:
		begin = begin + pmatch.rm_eo;

	}			/* while  */
	if (needToDealocate)
		{
		regfree (re);
		pkg_free(re);
#ifdef DEBUG
		fprintf(stdout,"Dealocating expression for port ...\n");
#endif
		}
	
	if (diff != 0)
	{
		newContentLength = oldContentLength + diff;
		patch_content_length (msg, newContentLength);
	}
	
	return ret+1;
}


int
sdp_mangle_ip (struct sip_msg *msg, char *oldip, char *newip)
{
	int i, oldContentLength, newContentLength, diff, oldlen,len,off,ret,needToDealocate;
	unsigned int mask, address, locatedIp;
	struct lump *l;
	regmatch_t pmatch;
	regex_t *re;
	char *s, *pos,*begin,*key;
	char buffer[16];	/* 123.456.789.123\0 */


	key = IP_REGEX;

	/*
	 * Checking if msg has a payload
	 */
	if (msg == NULL)
		return -5;
	oldContentLength = strtol (msg->content_length->body.s, NULL, 10);	/* de inlocuit ? */
	if (oldContentLength <= 0)
		return -1;

	/* checking oldip */
	if (oldip == NULL)
		return -3;
	/* checking newip */
	if (newip == NULL)
		return -4;
	i = parse_ip_netmask (oldip, &pos, &mask);

	if (i == -1)
	{
		/* invalid value for the netmask specified in oldip */
		return -8;
	}
	else
	{
		i = parse_ip_address (pos, &address);
		if (pos != NULL)
			free (pos);
		if (i == 0)
			return -9;	/* parse error in ip */
	}

	/* now we have in address/netmask binary values */

	begin = msg->buf + msg->first_line.len;	// inlocuiesc cu begin = getbody */
	ret = -1;
	len = strlen (newip);

	/* try to use precompiled expressions */
	needToDealocate = 0;
	if (ipExpression != NULL) 
		{
		re = ipExpression;
#ifdef DEBUG
		fprintf(stdout,"Using PRECOMPILED expression for ip ...\n");
#endif

		}
		else /* we are not using precompiled expressions */
			{
			re = pkg_malloc(sizeof(regex_t));
			if (re == NULL)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_ip: Unable to allocate re\n");
				return -9;
				}
			needToDealocate = 1;
			if ((regcomp (re, key, REG_EXTENDED)) != 0)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_ip: Unable to compile %s \n",key);
				return -5;
				}
#ifdef DEBUG
		fprintf(stdout,"Using ALLOCATED expression for ip ...\n");
#endif
			}

	diff = 0;
	while (begin < msg->buf + msg->len
	       && regexec (re, begin, 1, &pmatch, 0) == 0)
	{
		off = begin - msg->buf;
		if (pmatch.rm_so == -1)
		{
			LOG (L_ERR,
			     "ERROR: sdp_mangler_port: offset unknown\n");
			return -1;
		}

		pos = (char *) memrchr (begin + pmatch.rm_so, ' ',
					pmatch.rm_eo - pmatch.rm_so);
		pos++;		/* jumping over space */
		oldlen = (pmatch.rm_eo - pmatch.rm_so) - (pos - (begin + pmatch.rm_so));	/* ip length */
		if (oldlen > 15)
		{
			goto continue2;	/* silent fail return -10; invalid ip format ,probably like 1000.3.12341.2 */
		}
		buffer[0] = '\0';
		strncat ((char *) buffer, pos, oldlen);	
		buffer[oldlen] = '\0';
		i = parse_ip_address (buffer, &locatedIp);
		if (i == 0)
		{
			DBG ("DEBUG:mangle_ip silent fail on parse_address %s\n",buffer);
			goto continue2;	/* silent fail */
		}
		if (same_net (locatedIp, address, mask) == 0)
		{
			DBG ("DEBUG:mangle_ip silent fail on matching filter for address %s\n",buffer);
			goto continue2;	/* not in the same net, skiping */
		}


		/* replacing ip */

		/* deleting old ip */
		if ((l =
		     del_lump (&msg->add_rm,
			       pmatch.rm_so + off + (pos -
						     (begin + pmatch.rm_so)),
			       oldlen, 0)) == 0)
		{
			LOG (L_ERR,
			     "ERROR: replace_all_f: del_lump failed\n");
			return -1;
		}
		s = pkg_malloc (len);
		if (s == 0)
		{
			LOG (L_ERR,
			     "ERROR: replace_f: mem. allocation failure\n");
			return -1;
		}
		memcpy (s, newip, len);

		if (insert_new_lump_after (l, s, len, 0) == 0)
		{
			LOG (L_ERR, "ERROR: could not insert new lump\n");
			pkg_free (s);
			return -1;
		}
		diff = diff + len /*new length */  - oldlen;
		/* new cycle */
		ret++;
	      continue2:
		begin = begin + pmatch.rm_eo;

	}			/* while */
	if (needToDealocate)
	{
	regfree (re);		/* if I am going to use precompiled expressions to be removed */
	pkg_free(re);
#ifdef DEBUG
		fprintf(stdout,"Dealocating expression for ip ...\n");
#endif
	}
	
	if (diff != 0)
	{
		newContentLength = oldContentLength + diff;
		patch_content_length (msg, newContentLength);
	}
	return ret+1;

}

int compile_expresions(char *port,char *ip)
{
	portExpression = NULL;
	portExpression = pkg_malloc(sizeof(regex_t));
	if (portExpression != NULL)
		{
		if ((regcomp (portExpression,port, REG_EXTENDED)) != 0)
			{
			LOG(L_ERR,"ERROR: compile_expresions: Unable to compile portExpression [%s]\n",port);
			pkg_free(portExpression);
			portExpression = NULL;
			}
		}
	else
		{
			LOG(L_ERR,"ERROR: compile_expresions: Unable to alloc portExpression \n");
		}
	
	ipExpression = NULL;
	ipExpression = pkg_malloc(sizeof(regex_t));
	if (ipExpression != NULL)
		{
		if ((regcomp (ipExpression,ip, REG_EXTENDED)) != 0)
			{
			LOG(L_ERR,"ERROR: compile_expresions: Unable to compile ipExpression [%s]\n",ip);
			pkg_free(ipExpression);
			ipExpression = NULL;
			}
		}
	else
		{
			LOG(L_ERR,"ERROR: compile_expresions: Unable to alloc ipExpression \n");
		}
	
	return 0;
}

int free_compiled_expresions()
{
	if (portExpression != NULL) 
		regfree(portExpression);
	if (ipExpression != NULL) 
		regfree(ipExpression);
	portExpression = NULL;
	ipExpression = NULL;
	return 0;
}

