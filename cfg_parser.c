/*
 * $Id$
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "cfg_parser.h"
#include "msg_parser.h" /* parse_hostport */
#include "dprint.h"
#include "parser_f.h"
#include "route.h"




/* params: null terminated text line => fills cl
 * returns 0, or on error -1. */
int cfg_parse_line(char* line, struct cfg_line* cl)
{
	/* format:
		line = rule | comment
		comment = SP* '#'.*
		rule = SP* method_re SP* uri_re SP* ip_address comment?
	*/
		
	char* tmp;
	char* end;
	
	end=line+strlen(line);
	tmp=eat_space(line, end-line);
	if ((tmp==end)||(is_empty(tmp, end-tmp))) {
		cl->type=CFG_EMPTY;
		goto skip;
	}
	if (*tmp=='#'){
		cl->type=CFG_COMMENT;
		goto skip;
	}
	cl->method=tmp;
	tmp=eat_token(cl->method,end-cl->method);
	if (tmp==end) goto error;
	*tmp=0;
	tmp++;
	cl->uri=eat_space(tmp,end-tmp);
	if (tmp==end) goto error;
	tmp=eat_token(cl->uri,end-cl->uri);
	if (tmp==end) goto error;
	*tmp=0;
	tmp++;
	cl->address=eat_space(tmp,end-tmp);
	if (tmp==end) goto error;
	tmp=eat_token(cl->address, end-cl->address);
	if (tmp<end) {
		*tmp=0;
		if (tmp+1<end){
			if (!is_empty(tmp+1,end-tmp-1)){
				/* check if comment */
				tmp=eat_space(tmp+1, end-tmp-1);
				if (*tmp!='#'){
					/* extra chars at the end of line */
					goto error;
				}
			}
		}
	}
	/* find port */
	if (parse_hostport(cl->address, &tmp, &cl->port)==0){
			goto error;
	}
	
	cl->type=CFG_RULE;
skip:
	return 0;
error:
	cl->type=CFG_ERROR;
	return -1;
}



/* parses the cfg, returns 0 on success, line no otherwise */
int cfg_parse_stream(FILE* stream)
{
	int line;
	struct cfg_line cl;
	char buf[MAX_LINE_SIZE];
	int ret;

	line=1;
	while(!feof(stream)){
		if (fgets(buf, MAX_LINE_SIZE, stream)){
			cfg_parse_line(buf, &cl);
			switch (cl.type){
				case CFG_RULE:
					if ((ret=add_rule(&cl, &rlist))!=0){
						DPrint("ERROR: could not compile rule at line %d\n",
							line);
						DPrint(" ----: add_rule returned %d\n", ret);
						goto error;
					}
					break;
				case CFG_COMMENT:
				case CFG_SKIP:
					break;
				case CFG_ERROR:
					DPrint("ERROR: bad config line (%d):%s\n", line, buf);
					goto error;
					break;
			}
			line++;
		}else{
			if (ferror(stream)){
				DPrint("ERROR: reading configuration: %s\n", strerror(errno));
				goto error;
			}
			break;
		}
	}
	return 0;

error:
	return line;
}

