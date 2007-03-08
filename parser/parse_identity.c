/*
 * $Id$ 
 *
 * Copyright (c) 2007 iptelorg GmbH
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


#include <string.h>
#include "parse_identity.h"
#include "parse_def.h"
#include "parser_f.h"  /* eat_space_end and so on */
#include "../mem/mem.h"
#include "../ut.h"

/*
 * Parse Identity header field
 */

/*
 * If the value of Identity header contains any LWS then we've to create
 * a new buffer and move there the LWSless part
 */
int movetomybuffer (char *buffer, char *end, char *p, struct identity_body *ib)
{
	char *bufend;

	if (!(bufend=q_memchr(p, '"', end-p))) {
		LOG(L_ERR, "parse_identity: quotation mark is missing\n");
		return -1;
	}

	if (!(ib->hash.s=pkg_malloc(bufend-buffer))) {
		LOG(L_ERR, "parse_identity: out of memory\n");
		return -2;
	}
	ib->ballocated=1;

	memcpy(ib->hash.s, buffer, ib->hash.len);

	return 0;
}

char* parse_identity(char *buffer, char* end, struct identity_body* ib)
{
	char *p=NULL;
	char bpadded=0;

	if (!buffer || !end || !ib)
		goto error;

	ib->error=PARSE_ERROR;

	/* there must be '"' sign because there might be '=' sign in the
	 * value of Identity header
	 */
	if (*buffer != '"') {
		LOG(L_ERR, "parse_identity: quotation mark is missing\n");
		goto error;
	}

	/* we step over the '"' mark */
	ib->hash.s=buffer+1;
	ib->hash.len=0;

	for (p=buffer+1; p < end && *p != '"'; p++) {
		/* check the BASE64 alphabet */
		if (!bpadded
		    && ((*p >= 'a' && *p <='z')
			 	|| (*p >= 'A' && *p <='Z')
				|| (*p >= '0' && *p <='9')
				|| (*p == '+' || *p == '/'))) {
			if (ib->ballocated)
				ib->hash.s[ib->hash.len]=*p;
			ib->hash.len++;
			continue;
		}

		/* check the BASE64 padding */
		if ((p+1 < end && *p == '=' && *(p+1) != '=')
		    || (p+2 < end && *p == '=' && *(p+1) == '=' && *(p+2) != '=')) {
			bpadded=1;
			if (ib->ballocated)
				ib->hash.s[ib->hash.len]=*p;
			ib->hash.len++;
			continue;
		}

		/* LSW case */
		if (*p == ' ' || *p == '\t') {
			for (p++; p < end && (*p == ' ' || *p == '\t'); p++);
			if (p == end)
				goto parseerror;
			/* we've to create another whitespaceless buffer */
			if (!ib->ballocated && (movetomybuffer(buffer+1, end, p-1, ib)))
				goto error;
		}
		if (p+2 < end && *p == '\n' && *(p+1) == ' '
		    && !(*(p+2) == ' ' || *(p+2) == '\t')) {
			/* we've to create another whitespaceless buffer */
			if (!ib->ballocated && (movetomybuffer(buffer+1, end, p-1, ib)))
				goto error;
			p+=1;
			continue;
		}
		if (p+3 < end && *p == '\r' && *(p+1) == '\n' && *(p+2) == ' '
		  	&& !(*(p+3) == ' ' || *(p+3) == '\t')) {
			/* we've to create another whitespaceless buffer */
			if (!ib->ballocated && (movetomybuffer(buffer+1, end, p-1, ib)))
				goto error;
			p+=2;
			continue;
		}

		/* parse error */
		break;
	}
	if (p == end || *p != '"')
		goto parseerror;

	/* we step over '"' */
	p++;

	p=eat_lws_end(p, end);
	/*check if the header ends here*/
	if (p>=end) {
		LOG(L_ERR, "ERROR: parse_identity: strange EoHF\n");
		goto error;
	}
	if (*p=='\r' && p+1<end && *(p+1)=='\n') {
		ib->error=PARSE_OK;
		return p+2;
	}
	if (*p=='\n') {
		ib->error=PARSE_OK;
		return p+1;
	}
	LOG(L_ERR, "ERROR: Identity EoL expected\n");
	goto error;

parseerror:
	LOG( L_ERR , "ERROR: parse_identity: "
		"unexpected char [%c]: <<%.*s>> .\n",
		*p,(int)(p-buffer), ZSW(buffer));
error:
	return p;
}


void free_identity(struct identity_body *ib)
{
	if (ib->ballocated)
		pkg_free(ib->hash.s);
	pkg_free(ib);
}
