/* 
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

/*! \file
 * \brief Parser :: Cseq header field handling
 *
 * \ingroup parser
 */



#include "../comp_defs.h"
#include "parse_cseq.h"
#include "parser_f.h"  /* eat_space_end and so on */
#include "../dprint.h"
#include "parse_def.h"
#include "parse_methods.h"
#include "../mem/mem.h"

/*BUGGY*/
char* parse_cseq(char* const buf, const char* const end, struct cseq_body* const cb)
{
	char *t, *m, *m_end;
	
	cb->error=PARSE_ERROR;
	t=buf;
	
	cb->number.s=t;
	t=eat_token_end(t, end);
	if (t>=end) goto error;
	cb->number.len=t-cb->number.s;

	m=eat_space_end(t, end);
	m_end=eat_token_end(m, end);

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

	/* Cache method id */
	if (parse_method_name(&cb->method, &cb->method_id)!=0){
		LOG(L_ERR, "Cannot parse method string\n");
		goto error;
	}

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

error:
	LOG(L_ERR, "ERROR: parse_cseq: bad cseq\n");
	return t;
}


void free_cseq(struct cseq_body* const cb)
{
	pkg_free(cb);
}
