/* 
 * $Id$ 
 */

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
	char c;
	
	cb->error=PARSE_ERROR;
	t=eat_space_end(buf, end);
	if (t>=end) goto error;
	
	cb->number.s=t;
	t=eat_token_end(t, end);
	if (t>=end) goto error;
	m=eat_space_end(t, end);
	m_end=eat_token_end(m, end);
	*t=0; /*null terminate it*/
	cb->number.len=t-cb->number.s;

	if (m_end>=end) goto error;
	if (m_end==m){
		/* null method*/
		LOG(L_ERR,  "ERROR:parse_cseq: no method found\n");
		goto error;
	}
	cb->method.s=m;
	t=m_end;
	c=*t;
	*t=0; /*null terminate it*/
	cb->method.len=t-cb->method.s;
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
error:
	LOG(L_ERR, "ERROR: parse_cseq: bad cseq\n");
	return t;
}


void free_cseq(struct cseq_body* cb)
{
	pkg_free(cb);
}
