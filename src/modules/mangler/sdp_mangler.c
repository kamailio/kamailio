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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "sdp_mangler.h"
#include "ip_helper.h"
#include "utils.h"
#include "common.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../parser/hf.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_uri.h"
#include "../../parser/contact/parse_contact.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"	/* struct sip_msg */

regex_t *portExpression;
regex_t *ipExpression;



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
		{
		LOG(L_ERR,"ERROR: sdp_mangle_port: Received NULL for msg \n");
		return -1;
		}
                
	if ((msg->content_length==0) &&
			((parse_headers(msg,HDR_CONTENTLENGTH_F,0)==-1) ||
			 (msg->content_length==0) )){
		LOG(L_ERR,"ERROR: sdp_mangle_port: bad or missing "
				"Content-Length \n");
		return -2;
	}

        oldContentLength = get_content_length(msg);
        
	if (oldContentLength <= 0)
		{
		LOG(L_ERR,"ERROR: sdp_mangle_port: Received <= 0 for Content-Length \n");
		return -2;
		}
	
	if (offset == NULL)
		return -14;
	if (sscanf (offset, "%d", &offsetValue) != 1)
	{
		LOG(L_ERR,"ERROR: sdp_mangle_port: Invalid value for offset \n");
		return -13;
	}
	
	//offsetValue = (int)offset;
#ifdef EXTRA_DEBUG
	fprintf (stdout,"---START--------MANGLE PORT-----------------\n");
	fprintf(stdout,"===============OFFSET = %d\n",offsetValue);
#endif
	
	if ((offsetValue < MIN_OFFSET_VALUE) || (offsetValue > MAX_OFFSET_VALUE))
	{
		LOG(L_ERR,"ERROR: sdp_mangle_port: Invalid value %d for offset \n",offsetValue);
		return -3;
	}
	begin = get_body(msg); //msg->buf + msg->first_line.len;	// inlocuiesc cu begin = getbody */
	ret = -1;

	/* try to use pre-compiled expressions */
	needToDealocate = 0;
	if (portExpression != NULL) 
		{
		re = portExpression;
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Using PRECOMPILED expression for port ...\n");
#endif
		}
		else /* we are not using pre-compiled expressions */
			{
			re = pkg_malloc(sizeof(regex_t));
			if (re == NULL)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_port: Unable to allocate re\n");
				return -4;
				}
			needToDealocate = 1;
			if ((regcomp (re, key, REG_EXTENDED)) != 0)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_port: Unable to compile %s \n",key);
				return -5;
				}
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Using ALLOCATED expression for port ...\n");
#endif

			}
	
	diff = 0;
	while ((begin < msg->buf + msg->len) && (regexec (re, begin, 1, &pmatch, 0) == 0))
	{
		off = begin - msg->buf;
		if (pmatch.rm_so == -1)
		{
			LOG (L_ERR, "ERROR: sdp_mangle_port: offset unknown\n");
			return -6;
		}
	
#ifdef STRICT_CHECK
		pmatch.rm_eo --; /* return with one space */
#endif
	
		/* 
                for BSD and Solaris we avoid memrchr
                pos = (char *) memrchr (begin + pmatch.rm_so, ' ',pmatch.rm_eo - pmatch.rm_so); 
                */
                pos = begin+pmatch.rm_eo;
#ifdef EXTRA_DEBUG
                printf("begin=%c pos=%c rm_so=%d rm_eo=%d\n",*begin,*pos,pmatch.rm_so,pmatch.rm_eo);
#endif
                do pos--; while (*pos != ' '); /* we should find ' ' because we matched m=audio port */
                
		pos++;		/* jumping over space */
		oldlen = (pmatch.rm_eo - pmatch.rm_so) - (pos - (begin + pmatch.rm_so));	/* port length */

		/* convert port to int */
		oldPort = str2s (pos, oldlen, &err);
#ifdef EXTRA_DEBUG
                printf("port to convert [%.*s] to int\n",oldlen,pos);
#endif
		if (err)
			{
			LOG(L_ERR,"ERROR: sdp_mangle_port: Error converting [%.*s] to int\n",oldlen,pos);
#ifdef STRICT_CHECK
			return -7;
#else
			goto continue1;
#endif
			}
		if ((oldPort < MIN_ORIGINAL_PORT) || (oldPort > MAX_ORIGINAL_PORT))	/* we silently fail,we ignore this match or return -11 */
		{
#ifdef EXTRA_DEBUG
                printf("WARNING: sdp_mangle_port: Silent fail for not matching old port %d\n",oldPort);
#endif

			LOG(L_WARN,"WARNING: sdp_mangle_port: Silent fail for not matching old port %d\n",oldPort);
#ifdef STRICT_CHECK
			return -8;
#else
			goto continue1;
#endif
		}
                if ((offset[0] != '+')&&(offset[0] != '-')) newPort = offsetValue;//fix value
		else newPort = oldPort + offsetValue;
		/* new port is between 1 and 65536, or so should be */
		if ((newPort < MIN_MANGLED_PORT) || (newPort > MAX_MANGLED_PORT))	/* we silently fail,we ignore this match */
		{
#ifdef EXTRA_DEBUG
                printf("WARNING: sdp_mangle_port: Silent fail for not matching new port %d\n",newPort);
#endif
                
			LOG(L_WARN,"WARNING: sdp_mangle_port: Silent fail for not matching new port %d\n",newPort);
#ifdef STRICT_CHECK
			return -9;
#else
			goto continue1;
#endif
		}

#ifdef EXTRA_DEBUG
		fprintf(stdout,"Extracted port is %d and mangling to %d\n",oldPort,newPort);
#endif

		/*
		len = 1;
		while ((newPort = (newPort / 10)) != 0)	len++;
		newPort = oldPort + offsetValue;
		*/
		if (newPort >= 10000) len = 5;
			else
				if (newPort >= 1000) len = 4;
					else
						if (newPort >= 100) len = 3;
							else
								if (newPort >= 10) len = 2;
									else len = 1;

		/* replaced five div's + 1 add with most probably 1 comparison or 2 */							
		
		/* deleting old port */
		if ((l = del_lump (msg, pmatch.rm_so + off + 
						(pos -(begin + pmatch.rm_so)),oldlen, 0)) == 0)
		{
			LOG (L_ERR,"ERROR: sdp_mangle_port: del_lump failed\n");
			return -10;
		}
		s = pkg_malloc (len);
		if (s == 0)
		{
			LOG (L_ERR,"ERROR: sdp_mangle_port : memory allocation failure\n");
			return -11;
		}
		snprintf (buf, len + 1, "%u", newPort);	/* converting to string */
		memcpy (s, buf, len);

		if (insert_new_lump_after (l, s, len, 0) == 0)
		{
			LOG (L_ERR, "ERROR: sdp_mangle_port: could not insert new lump\n");
			pkg_free (s);
			return -12;
		}
		diff = diff + len /*new length */  - oldlen;
		/* new cycle */
		ret++;
#ifndef STRICT_CHECK
continue1:
#endif
		begin = begin + pmatch.rm_eo;

	}			/* while  */
	if (needToDealocate)
		{
		regfree (re);
		pkg_free(re);
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Deallocating expression for port ...\n");
#endif
		}
	
	if (diff != 0)
	{
		newContentLength = oldContentLength + diff;
		patch_content_length (msg, newContentLength);
	}

#ifdef EXTRA_DEBUG
	fprintf (stdout,"---END--------MANGLE PORT-----------------\n");
#endif

	return ret+2;
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

#ifdef EXTRA_DEBUG
	fprintf (stdout,"---START--------MANGLE IP-----------------\n");
#endif

	
	key = IP_REGEX;

	/*
	 * Checking if msg has a payload
	 */
	if (msg == NULL)
		{
		LOG(L_ERR,"ERROR: sdp_mangle_ip: Received NULL for msg\n");
		return -1;
		}
	if ((msg->content_length==0) &&
				((parse_headers(msg,HDR_CONTENTLENGTH_F,0)==-1) ||
				 (msg->content_length==0) )){
			LOG(L_ERR,"ERROR: sdp_mangle_port: bad or missing "
					"Content-Length \n");
			return -2;
		}
        oldContentLength = get_content_length(msg);
        
	if (oldContentLength <= 0)
		{
		LOG(L_ERR,"ERROR: sdp_mangle_ip: Received <= for Content-Length\n");
		return -2;
		}

	/* checking oldip */
	if (oldip == NULL)
		{
		LOG(L_ERR,"ERROR: sdp_mangle_ip: Received NULL for oldip\n");
		return -3;
		}
	/* checking newip */
	if (newip == NULL)
		{
		LOG(L_ERR,"ERROR: sdp_mangle_ip: Received NULL for newip\n");
		return -4;
		}
	i = parse_ip_netmask (oldip, &pos, &mask);

	if (i == -1)
	{
		/* invalid value for the netmask specified in oldip */
		LOG(L_ERR,"ERROR: sdp_mangle_ip: invalid value for the netmask specified in oldip\n");
		return -5;
	}
	else
	{
		i = parse_ip_address (pos, &address);
		if (pos != NULL) free (pos);
		if (i == 0)
			{
			LOG(L_ERR,"ERROR: sdp_mangle_ip: invalid value for the ip specified in oldip\n");
			return -6;	/* parse error in ip */
			}
	}

	/* now we have in address/netmask binary values */

	begin = get_body(msg);//msg->buf + msg->first_line.len;	// inlocuiesc cu begin = getbody */
	ret = -1;
	len = strlen (newip);

	/* try to use pre-compiled expressions */
	needToDealocate = 0;
	if (ipExpression != NULL) 
		{
		re = ipExpression;
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Using PRECOMPILED expression for ip ...\n");
#endif

		}
		else /* we are not using pre-compiled expressions */
			{
			re = pkg_malloc(sizeof(regex_t));
			if (re == NULL)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_ip: Unable to allocate re\n");
				return -7;
				}
			needToDealocate = 1;
			if ((regcomp (re, key, REG_EXTENDED)) != 0)
				{
				LOG(L_ERR,"ERROR: sdp_mangle_ip: Unable to compile %s \n",key);
				return -8;
				}
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Using ALLOCATED expression for ip ...\n");
#endif
			}

	diff = 0;
	while ((begin < msg->buf + msg->len) && (regexec (re, begin, 1, &pmatch, 0) == 0))
	{
		off = begin - msg->buf;
		if (pmatch.rm_so == -1)
		{
			LOG (L_ERR,"ERROR: sdp_mangler_ip: offset unknown\n");
			return -9;
		}
	
#ifdef STRICT_CHECK
		pmatch.rm_eo --; /* return with one space,\n,\r */
#endif
	
		/* 
                for BSD and Solaris we avoid memrchr
                pos = (char *) memrchr (begin + pmatch.rm_so, ' ',pmatch.rm_eo - pmatch.rm_so); 
                */
                pos = begin+pmatch.rm_eo;
                do pos--; while (*pos != ' '); /* we should find ' ' because we matched c=IN IP4 ip */

		pos++;		/* jumping over space */
		oldlen = (pmatch.rm_eo - pmatch.rm_so) - (pos - (begin + pmatch.rm_so));	/* ip length */
		if (oldlen > 15)
		{
			LOG(L_WARN,"WARNING: sdp_mangle_ip: Silent fail because oldlen > 15\n");
#ifdef STRICT_CHECK
			return -10;
#else 
			goto continue2;	/* silent fail return -10; invalid ip format ,probably like 1000.3.12341.2 */
#endif

			
		}
		buffer[0] = '\0';
		strncat ((char *) buffer, pos, oldlen);	
		buffer[oldlen] = '\0';
		i = parse_ip_address (buffer, &locatedIp);
		if (i == 0)
		{
			LOG(L_WARN,"WARNING: sdp_mangle_ip: Silent fail on parsing matched address \n");
			
#ifdef STRICT_CHECK
			return -11;
#else 
			goto continue2;	
#endif
		}
		if (same_net (locatedIp, address, mask) == 0)
		{
			LOG(L_WARN,"WARNING: sdp_mangle_ip: Silent fail because matched address is not in network\n");
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Extracted ip is %s and not mangling \n",buffer);
#endif
			goto continue2;	/* not in the same net, skipping */
		}
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Extracted ip is %s and mangling to %s\n",buffer,newip);
#endif


		/* replacing ip */

		/* deleting old ip */
		if ((l = del_lump (msg,pmatch.rm_so + off + 
						(pos - (begin + pmatch.rm_so)),oldlen, 0)) == 0)
		{
			LOG (L_ERR,"ERROR: sdp_mangle_ip: del_lump failed\n");
			return -12;
		}
		s = pkg_malloc (len);
		if (s == 0)
		{
			LOG (L_ERR,"ERROR: sdp_mangle_ip: mem. allocation failure\n");
			return -13;
		}
		memcpy (s, newip, len);

		if (insert_new_lump_after (l, s, len, 0) == 0)
		{
			LOG (L_ERR, "ERROR: sdp_mangle_ip: could not insert new lump\n");
			pkg_free (s);
			return -14;
		}
		diff = diff + len /*new length */  - oldlen;
		/* new cycle */
		ret++;
continue2:
		begin = begin + pmatch.rm_eo;

	}			/* while */
	if (needToDealocate)
	{
	regfree (re);		/* if I am going to use pre-compiled expressions to be removed */
	pkg_free(re);
#ifdef EXTRA_DEBUG
		fprintf(stdout,"Deallocating expression for ip ...\n");
#endif
	}
	
	if (diff != 0)
	{
		newContentLength = oldContentLength + diff;
		patch_content_length (msg, newContentLength);
	}

#ifdef EXTRA_DEBUG
	fprintf (stdout,"---END--------MANGLE IP-----------------\n");
#endif

	return ret+2;

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
		{
		regfree(portExpression);
		pkg_free(portExpression);
		portExpression = NULL;
		}
	if (ipExpression != NULL) 
		{
		regfree(ipExpression);
		pkg_free(ipExpression);
		ipExpression = NULL;
		}
	return 0;
}

