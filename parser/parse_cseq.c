/* 
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
 * 
 * History:
 * --------
 * 2003-01-22 zero-termination in CSeq eliminated (jiri)
 */


#include "../comp_defs.h"
#include "parse_cseq.h"
#include "parser_f.h"  /* eat_space_end and so on */
#include "../dprint.h"
#include "parse_def.h"
#include "../mem/mem.h"

/*
 * Parse CSeq header field
 */

/*BUGGY*/
char* parse_cseq(char *buf, char* end, struct cseq_body* cb)
{
	char *t, *m, *m_end;
#ifdef PRESERVE_ZT
	char c;
#endif
	
	cb->error=PARSE_ERROR;
#ifdef PRESERVE_ZT /* already called in calling function */
	t=eat_space_end(buf, end);
	if (t>=end) goto error;
#else
	t=buf;
#endif
	
	cb->number.s=t;
	t=eat_token_end(t, end);
	if (t>=end) goto error;
	cb->number.len=t-cb->number.s;

	m=eat_space_end(t, end);
	m_end=eat_token_end(m, end);
	SET_ZT(*t);

	if (m_end>=end) {
			LOG(L_ERR, "ERROR: parse_cseq: "
						"method terminated unexpectedly\n");
			goto error;
	}
	if (m_end==m){
		/* null method*/
		LOG(L_ERR,  "ERROR:parse_cseq: no method found\n");
		goto error;
	}
	cb->method.s=m;
	t=m_end;
	cb->method.len=t-cb->method.s;

#ifdef PRESERVE_ZT
	c=*t;
	*t=0; /*null terminate it*/
	t++;
	/*check if the header ends here*/
	if (c=='\n') goto check_continue;
	do{
		for (;(t<end)&&((*t==' ')||(*t=='\t')||(*t=='\r'));t++);
		if (t>=end) goto error;
		if (*t!='\n'){
			LOG(L_ERR, "ERROR:parse_cseq: unexpected char <%c> at end of"
					" cseq\n", *t);
			goto error;
		}
		t++;
check_continue:
		;
	}while( (t<end) && ((*t==' ')||(*t=='\t')) );
	cb->error=PARSE_OK;
	return t;
#else
	/* there may be trailing LWS 
	 * (it was not my idea to put it in SIP; -jiri )
	 */
	t=eat_lws_end(t, end);
	/*check if the header ends here*/
	if (t>=end) {
		LOG(L_ERR, "ERROR: parse_cseq: strange EoHF\n");
		goto error;
	}
	if (*t=='\r' && t+1<end && *(t+1)=='\n') {
			cb->error=PARSE_OK;
			return t+2;
	}
	if (*t=='\n') {
			cb->error=PARSE_OK;
			return t+1;
	}
	LOG(L_ERR, "ERROR: CSeq EoL expected\n");
#endif
error:
	LOG(L_ERR, "ERROR: parse_cseq: bad cseq\n");
	return t;
}


void free_cseq(struct cseq_body* cb)
{
	pkg_free(cb);
}
