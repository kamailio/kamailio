

#include <stdlib.h>
#include "dprint.h"
#include "msg_parser.h"
#include "ut.h"
#include "mem/mem.h"


enum{ START_TO, IN_NAME_ADDR, E_NAME_ADDR , IN_ADDR_SPEC, E_ADDR_SPEC
	, IN_ADDR_SPEC_ONLY, S_PARA_NAME, PARA_NAME, S_EQUAL, S_PARA_VALUE
	, TAG1, TAG2, TAG3, PARA_VALUE_TOKEN , PARA_VALUE_QUOTED, E_PARA_VALUE
	, F_CR, F_LF, F_CRLF
	};


#define add_param( _param , _body ) \
	do{\
		if (!(_body)->param_lst)  (_body)->param_lst=(_param);\
		else (_body)->last_param->next=(_param);\
		(_body)->last_param =(_param);\
		if ((_param)->type==TAG_PARAM)\
			memcpy(&((_body)->tag_value),&((_param)->value),sizeof(str));\
	}while(0);




char* parse_to(char* buffer, char *end, struct to_body *to_b)
{
	struct to_param *param=0;
	int status = START_TO;
	int saved_status;
	char  *tmp;

	for( tmp=buffer; tmp<end; tmp++)
	{
		switch(*tmp)
		{
			case ' ':
			case '\t':
				switch (status)
				{
					case IN_ADDR_SPEC_ONLY:
						to_b->body.len=tmp-to_b->body.s;
						*tmp=0;
						status = E_ADDR_SPEC;
						break;
					case E_ADDR_SPEC:
						*tmp =0;
						break;
					case TAG3:
						param->type=TAG_PARAM;
					case PARA_NAME:
					case TAG1:
					case TAG2:
						param->name.len = tmp-param->name.s;
						*tmp=0;
						status = S_EQUAL;
						break;
					case PARA_VALUE_TOKEN:
						param->value.len = tmp-param->value.s;
						*tmp=0;
						status = E_PARA_VALUE;
						add_param( param , to_b );
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now =' '*/
						status=saved_status;
						break;
				}
				break;
			case '\n':
				switch (status)
				{
					case IN_ADDR_SPEC_ONLY:
						to_b->body.len=tmp-to_b->body.s;
						*tmp=0;
						status = E_ADDR_SPEC;
						break;
					case E_ADDR_SPEC:
						*tmp =0;
					case START_TO:
					case E_NAME_ADDR:
					case S_PARA_NAME:
					case S_EQUAL:
					case S_PARA_VALUE:
					case E_PARA_VALUE:
						saved_status=status;
						status=F_LF;
						break;
					case TAG3:
						param->type=TAG_PARAM;
					case PARA_NAME:
					case TAG1:
					case TAG2:
						param->name.len = tmp-param->name.s;
						*tmp=0;
						status = S_EQUAL;
						break;
					case PARA_VALUE_TOKEN:
						param->value.len = tmp-param->value.s;
						*tmp=0;
						status = E_PARA_VALUE;
						add_param( param , to_b );
						break;
					case F_CR:
						status=F_CRLF;
						break;
					case F_CRLF:
					case F_LF:
						status=saved_status;
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
				}
				break;
			case '\r':
				switch (status)
				{
					case IN_ADDR_SPEC_ONLY:
						to_b->body.len=tmp-to_b->body.s;
						*tmp=0;
						status = E_ADDR_SPEC;
						break;
					case E_ADDR_SPEC:
						*tmp =0;
					case START_TO:
					case E_NAME_ADDR:
					case S_PARA_NAME:
					case S_EQUAL:
					case S_PARA_VALUE:
					case E_PARA_VALUE:
						saved_status=status;
						status=F_CR;
						break;
					case TAG3:
						param->type=TAG_PARAM;
					case PARA_NAME:
					case TAG1:
					case TAG2:
						param->name.len = tmp-param->name.s;
						*tmp=0;
						status = S_EQUAL;
						break;
					case PARA_VALUE_TOKEN:
						param->value.len = tmp-param->value.s;
						*tmp=0;
						status = E_PARA_VALUE;
						add_param( param , to_b );
						break;
					case F_CRLF:
					case F_CR:
					case F_LF:
						status=saved_status;
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '\\':
				switch (status)
				{
					case IN_NAME_ADDR:
					case PARA_VALUE_QUOTED:
						switch (*(tmp+1))
						{
							case F_CR:
							case F_LF:
								break;
							default:
								tmp++;
						}
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '<':
				switch (status)
				{
					case START_TO:
					case E_NAME_ADDR:
						if (!to_b->body.s) to_b->body.s=tmp;
						status = IN_ADDR_SPEC;
						break;
					case IN_NAME_ADDR:
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '>':
				switch (status)
				{
					case IN_ADDR_SPEC:
						to_b->body.len=tmp-to_b->body.s;
						status = E_ADDR_SPEC;
						break;
					case IN_NAME_ADDR:
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '"':
				switch (status)
				{
					case START_TO:
						to_b->body.s=tmp;
						status = IN_NAME_ADDR;
						break;
					case IN_NAME_ADDR:
						status = E_NAME_ADDR;
						break;
					case S_PARA_VALUE:
						param->value.s = tmp+1;
						status = PARA_VALUE_QUOTED;
						break;
					case PARA_VALUE_QUOTED:
						param->value.len=tmp-param->value.s-1 ;
						*tmp = 0;
						add_param( param , to_b );
						status = E_PARA_VALUE;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case ';' :
				switch (status)
				{
					case IN_NAME_ADDR:
					case PARA_VALUE_QUOTED:
						break;
					case IN_ADDR_SPEC_ONLY:
						to_b->body.len=tmp-to_b->body.s;
						*tmp=0;
					case E_ADDR_SPEC:
					case E_PARA_VALUE:
						param = (struct to_param*)pkg_malloc(sizeof(struct to_param));
						if (!param){
							LOG( L_ERR , "ERROR: parse_to - out of memory\n" );
							goto error;
						}
						memset(param,0,sizeof(struct to_param));
						param->type=GENERAL_PARAM;
						status = S_PARA_NAME;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case 'T':
			case 't' :
				switch (status)
				{
					case START_TO:
						to_b->body.s=tmp;
						status = IN_ADDR_SPEC_ONLY;
						break;
					case IN_NAME_ADDR:
					case PARA_VALUE_QUOTED:
					case PARA_VALUE_TOKEN:
					case IN_ADDR_SPEC:
					case IN_ADDR_SPEC_ONLY:
					case PARA_NAME:
						break;
					case S_PARA_NAME:
						param->name.s = tmp;
						status = TAG1;
						break;
					case S_PARA_VALUE:
						param->value.s = tmp;
						status = PARA_VALUE_TOKEN;
						break;
					case TAG1:
					case TAG2:
					case TAG3:
						status = PARA_NAME;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case 'A':
			case 'a' :
				switch (status)
				{
					case START_TO:
						to_b->body.s=tmp;
						status = IN_ADDR_SPEC_ONLY;
						break;
					case IN_NAME_ADDR:
					case PARA_VALUE_QUOTED:
					case PARA_VALUE_TOKEN:
					case IN_ADDR_SPEC:
					case IN_ADDR_SPEC_ONLY:
					case PARA_NAME:
						break;
					case S_PARA_NAME:
						param->name.s = tmp;
						status = PARA_NAME;
						break;
					case S_PARA_VALUE:
						param->value.s = tmp;
						status = PARA_VALUE_TOKEN;
						break;
					case TAG1:
						status = TAG2;
						break;
					case TAG2:
					case TAG3:
						status = PARA_NAME;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case 'G':
			case 'g' :
				switch (status)
				{
					case START_TO:
						to_b->body.s=tmp;
						status = IN_ADDR_SPEC_ONLY;
						break;
					case IN_NAME_ADDR:
					case PARA_VALUE_QUOTED:
					case PARA_VALUE_TOKEN:
					case IN_ADDR_SPEC:
					case IN_ADDR_SPEC_ONLY:
					case PARA_NAME:
						break;
					case S_PARA_NAME:
						param->name.s = tmp;
						status = PARA_NAME;
						break;
					case S_PARA_VALUE:
						param->value.s = tmp;
						status = PARA_VALUE_TOKEN;
						break;
					case TAG1:
					case TAG3:
						status = PARA_NAME;
						break;
					case TAG2:
						status = TAG3;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '=':
				switch (status)
				{
					case IN_NAME_ADDR:
					case PARA_VALUE_QUOTED:
						break;
					case TAG3:
						param->type=TAG_PARAM;
					case PARA_NAME:
					case TAG1:
					case TAG2:
						param->name.len = tmp-param->name.s;
						*tmp=0;
						status = S_PARA_VALUE;
						break;
					case S_EQUAL:
						status = S_PARA_VALUE;
						break;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
						"in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			default:
				switch (status)
				{
					case START_TO:
						to_b->body.s=tmp;
						status = IN_ADDR_SPEC_ONLY;
						break;
					case PARA_VALUE_TOKEN:
					case PARA_NAME:
					case IN_ADDR_SPEC:
					case IN_ADDR_SPEC_ONLY:
					case IN_NAME_ADDR:
					case PARA_VALUE_QUOTED:
						break;
					case S_PARA_NAME:
						param->name.s = tmp;
						status = PARA_NAME;
						break;
					case S_PARA_VALUE:
						param->value.s = tmp;
						status = PARA_VALUE_TOKEN;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						DBG("DEBUG: parse_to: spitting out [%c] in status %d\n",
						*tmp,status );
						goto error;
				}
		}/*char switch*/
	}/*for*/

endofheader:
	status=saved_status;
	DBG("end of header reached, state=%d\n", status);
	/* check if error*/
	switch(status){
		case E_ADDR_SPEC:
		case E_PARA_VALUE:
			break;
		default:
			LOG(L_ERR, "ERROR: parse_to: invalid To - end of header in"
					" state %d\n", status);
			goto error;
	}
	return tmp;

error:
	LOG(L_ERR, "to parse error\n");
	if (param) pkg_free(param);
	to_b->error=PARSE_ERROR;
	return tmp;

}

