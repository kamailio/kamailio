/*
 * Copyright (c) 2007 iptelorg GmbH
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

/*! \file
 * \brief Parser :: Parse Identity header field
 *
 * \ingroup parser
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

#define SP(_c) ((_c)=='\t' || (_c)==' ')
inline static int isendofhash (char* p, char* end)
{
	/* new header line */
	if ((p<end && *p=='"')
		/* end of message */
		|| ((*p=='\n' || *p=='\r') && p+1==end))
		return 1;
	else
		return 0;
}


/*! \brief
 * If the value of Identity header contains any LWS then we've to create
 * a new buffer and move there the LWSless part
 */
int movetomybuffer (char *pstart,
					char *pend,
					char *pcur,
					struct identity_body *ib)
{
	char *phashend;

	for (phashend = pcur; !isendofhash(phashend, pend); phashend++);

	if (!(ib->hash.s=pkg_malloc(phashend-pstart))) {
		LOG(L_ERR, "parse_identity: out of memory\n");
		return -2;
	}
	ib->ballocated=1;

	memcpy(ib->hash.s, pstart, ib->hash.len);

	return 0;
}


void parse_identity(char *buffer, char* end, struct identity_body* ib)
{
	char *p=NULL, *pstart=NULL;

	if (!buffer || !end || !ib)
		goto error;

	ib->error=PARSE_ERROR;

	/* if there is a '"' sign then we'll step over it */
	*buffer == '"' ? (pstart = buffer + 1) : (pstart = buffer);

	ib->hash.s=pstart;
	ib->hash.len=0;

	for (p = pstart; p < end; p++) {
		/* check the BASE64 alphabet */
		if (((*p >= 'a' && *p <='z')
			|| (*p >= 'A' && *p <='Z')
			|| (*p >= '0' && *p <='9')
			|| (*p == '+' || *p == '/' || *p == '='))) {
			if (ib->ballocated)
				ib->hash.s[ib->hash.len]=*p;
			ib->hash.len++;
			continue;
		}

		/* LWS */
		if (*p=='\n' && p+1<end && SP(*(p+1))) {
			/* p - 1 because we don't want to pass '\n' */
			if (!ib->ballocated && (movetomybuffer(pstart, end, p-1, ib)))
				goto error;
			/* p + 1 < end because 'continue' increases p so we'd skip \n
			   we need after this for loop */
			for (p+=1; p + 1 < end && SP(*(p + 1)); p++);
			continue;
		}
		if (*p=='\r' && p+2<end && *(p+1)=='\n' && SP(*(p+2))) {
			if (!ib->ballocated && (movetomybuffer(pstart, end, p-1, ib)))
				goto error;
			for (p+=2; p + 1 < end && SP(*(p + 1)); p++);
			continue;
		}

		if (isendofhash(p, end))
			break;

		/* parse error */
		goto parseerror;
	}

	/* this is the final quotation mark so we step over */
	ib->error=PARSE_OK;
	return ;

parseerror:
	LOG( L_ERR , "ERROR: parse_identity: "
		"unexpected char [0x%X]: <<%.*s>> .\n",
		*p,(int)(p-buffer), ZSW(buffer));
error:
	return ;
}

int parse_identity_header(struct sip_msg *msg)
{
	struct identity_body* identity_b;


	if ( !msg->identity
		 && (parse_headers(msg,HDR_IDENTITY_F,0)==-1
		 || !msg->identity) ) {
		LOG(L_ERR,"ERROR:parse_identity_header: bad msg or missing IDENTITY header\n");
		goto error;
	}

	/* maybe the header is already parsed! */
	if (msg->identity->parsed)
		return 0;

	identity_b=pkg_malloc(sizeof(*identity_b));
	if (identity_b==0){
		LOG(L_ERR, "ERROR:parse_identity_header: out of memory\n");
		goto error;
	}
	memset(identity_b, 0, sizeof(*identity_b));

	parse_identity(msg->identity->body.s,
				   msg->identity->body.s + msg->identity->body.len+1,
				   identity_b);
	if (identity_b->error==PARSE_ERROR){
		free_identity(identity_b);
		goto error;
	}
	msg->identity->parsed=(void*)identity_b;

	return 0;
error:
	return -1;
}

void free_identity(struct identity_body *ib)
{
	if (ib->ballocated)
		pkg_free(ib->hash.s);
	pkg_free(ib);
}
