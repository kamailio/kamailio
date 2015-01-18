/* 
 * Copyright (C) 2007 iptelorg GmbH
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

/*! \file
 * \brief Parser :: Retry-After: header parser
 *
 * \ingroup parser
 */
 

#include "../comp_defs.h"
#include "parse_retry_after.h"
#include "parser_f.h"  /* eat_space_end and so on */
#include "parse_def.h"
#include "../dprint.h"
#include "../mem/mem.h"

/*! \brief Parse the Retry-after header field */
char* parse_retry_after(char* const buf, const char* const end, unsigned* const after, int* const err)
{
	char *t;
	int i;
	unsigned val;
	
	val=0;
	t=buf;
	
	t=eat_lws_end(t, end);
	if (t>=end) goto error;
	for (i=0; t<end; i++,t++){
		if ((*t >= '0') && (*t <= '9')){
			val=val*10+(*t-'0');
		}else{
			switch(*t){
				/* for now we don't care about retry-after params or comment*/
				case ' ':
				case '\t':
				case ';':
				case '\r':
				case '\n':
				case '(':
					goto found;
				default:
					/* invalid char */
					goto error;
			}
		}
	}
	goto error_nocrlf; /* end reached without encountering cr or lf */
found:
	if (i>10 || i==0) /* too many  or too few digits */
		goto error;
	*after=val;
	/* find the end of header */
	for (; t<end; t++){
		if (*t=='\n'){
			if (((t+1)<end) && (*(t+1)=='\r'))
				t++;
			if (((t+1)<end) && (*(t+1)==' ' || *(t+1)=='\t')){
				t++;
				continue; /* line folding ... */
			}
			*err=0;
			return t+1;
		}
	}
error_nocrlf:
	LOG(L_ERR, "ERROR: parse_retry_after: strange EoHF\n");
	goto error;
error:
	LOG(L_ERR, "ERROR: parse_retry_after: bad Retry-After header \n");
	*err=1;
	return t;
}
