

#include <stdlib.h>
#include <string.h>
#include "dprint.h"
#include "msg_parser.h"
#include "ut.h"
#include "mem/mem.h"


enum{ START_TO, DISPLAY_QUOTED, E_DISPLAY_QUOTED, DISPLAY_TOKEN
	, S_URI_ENCLOSED, URI_ENCLOSED, E_URI_ENCLOSED
	, URI_OR_TOKEN, MAYBE_URI_END
	, END, F_CR, F_LF, F_CRLF
	};

enum{ S_PARA_NAME=20, PARA_NAME, S_EQUAL, S_PARA_VALUE, TAG1, TAG2, TAG3
	, PARA_VALUE_TOKEN , PARA_VALUE_QUOTED, E_PARA_VALUE, PARA_START
	};



#define add_param( _param , _body ) \
	do{\
		if (!(_body)->param_lst)  (_body)->param_lst=(_param);\
		else (_body)->last_param->next=(_param);\
		(_body)->last_param =(_param);\
		if ((_param)->type==TAG_PARAM)\
			memcpy(&((_body)->tag_value),&((_param)->value),sizeof(str));\
	}while(0);





char* parse_to_param(char *buffer, char *end, struct to_body *to_b,
								int *returned_status)
{
	struct to_param *param;
	int status;
	int saved_status;
	char  *tmp;

	param=0;
	status=PARA_START;
	saved_status=PARA_START;

	for( tmp=buffer; tmp<end; tmp++)
	{
		switch(*tmp)
		{
			case ' ':
			case '\t':
				switch (status)
				{
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
						LOG( L_ERR , "ERROR: parse_to_param : "
						"unexpected char [%c] in status %d .\n",*tmp,status);
				}
				break;
			case '\r':
				switch (status)
				{
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
						LOG( L_ERR , "ERROR: parse_to_param : "
						"unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '\\':
				switch (status)
				{
					case PARA_VALUE_QUOTED:
						switch (*(tmp+1))
						{
							case '\r':
							case '\n':
								break;
							default:
								tmp++;
						}
					default:
						LOG( L_ERR , "ERROR: parse_to_param : "
						"unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '"':
				switch (status)
				{
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
						LOG( L_ERR , "ERROR: parse_to_param :"
						"unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case ';' :
				switch (status)
				{
					case PARA_VALUE_QUOTED:
						break;
					case PARA_START:
						*tmp=0;
					case E_PARA_VALUE:
						param = (struct to_param*)
							pkg_malloc(sizeof(struct to_param));
						if (!param){
							LOG( L_ERR , "ERROR: parse_to_param"
							" - out of memory\n" );
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
						LOG( L_ERR , "ERROR: parse_to_param :"
						"unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case 'T':
			case 't' :
				switch (status)
				{
					case PARA_VALUE_QUOTED:
					case PARA_VALUE_TOKEN:
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
						LOG( L_ERR , "ERROR: parse_to_param :"
						" unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case 'A':
			case 'a' :
				switch (status)
				{
					case PARA_VALUE_QUOTED:
					case PARA_VALUE_TOKEN:
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
						LOG( L_ERR , "ERROR: parse_to_param : "
						"unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case 'G':
			case 'g' :
				switch (status)
				{
					case PARA_VALUE_QUOTED:
					case PARA_VALUE_TOKEN:
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
						LOG( L_ERR , "ERROR: parse_to_param : "
						"unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			case '=':
				switch (status)
				{
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
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to_param : "
						"unexpected char [%c] in status %d .\n",*tmp,status);
						goto error;
				}
				break;
			default:
				switch (status)
				{
					case PARA_VALUE_TOKEN:
					case PARA_NAME:
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
						DBG("DEBUG: parse_to_param: "
						"spitting out [%c] in status %d\n",*tmp,status );
						goto error;
				}
		}/*switch*/
	}/*for*/


endofheader:
	*returned_status=saved_status;
	return tmp;

error:
	LOG(L_ERR, "to_param parse error\n");
	if (param) pkg_free(param);
	to_b->error=PARSE_ERROR;
	return tmp;
}




char* parse_to(char* buffer, char *end, struct to_body *to_b)
{
	int status;
	int saved_status;
	char  *tmp,*foo;

	status=START_TO;
	foo=0;

	for( tmp=buffer; tmp<end; tmp++)
	{
		switch(*tmp)
		{
			case ' ':
			case '\t':
				switch (status)
				{
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now =' '*/
						status=saved_status;
						break;
					case URI_ENCLOSED:
						to_b->uri.len = tmp - to_b->uri.s;
						//*tmp = 0;
						status = E_URI_ENCLOSED;
						break;
					case URI_OR_TOKEN:
						foo = tmp;
						status = MAYBE_URI_END;
						break;
				}
				break;
			case '\n':
				switch (status)
				{
					case URI_OR_TOKEN:
						foo = tmp;
						status = MAYBE_URI_END;
					case MAYBE_URI_END:
					case DISPLAY_TOKEN:
					case E_DISPLAY_QUOTED:
					case END:
						saved_status=status;
						status=F_LF;
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
					case URI_OR_TOKEN:
						foo = tmp;
						status = MAYBE_URI_END;
					case MAYBE_URI_END:
					case DISPLAY_TOKEN:
					case E_DISPLAY_QUOTED:
					case END:
						saved_status=status;
						status=F_CR;
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
					case DISPLAY_QUOTED:
						switch (*(tmp+1))
						{
							case '\n':
							case '\r':
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
						to_b->body.s=tmp;
						status = S_URI_ENCLOSED;
						break;
					case DISPLAY_QUOTED:
						break;
					case E_DISPLAY_QUOTED:
					case URI_OR_TOKEN:
					case DISPLAY_TOKEN: 
					case MAYBE_URI_END:
						status = S_URI_ENCLOSED;
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
					case DISPLAY_QUOTED:
						break;
					case URI_ENCLOSED:
						//*tmp = 0;
					case E_URI_ENCLOSED:
						to_b->uri.len = tmp - to_b->uri.s;
						status = END;
						foo = 0;
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
						to_b->body.s = tmp;
						status = DISPLAY_QUOTED;
						break;
					case DISPLAY_QUOTED:
						status = E_DISPLAY_QUOTED;
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
					case DISPLAY_QUOTED:
					case URI_ENCLOSED:
						break;
					case URI_OR_TOKEN:
						foo = tmp;
					case MAYBE_URI_END:
						to_b->uri.len = foo - to_b->uri.s;
					case END:
						to_b->body.len = tmp-to_b->body.s;
						tmp = parse_to_param(tmp,end,to_b,&saved_status);
						//if (foo) *foo=0;
						goto endofheader;
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
			default:
				switch (status)
				{
					case START_TO:
						to_b->uri.s = to_b->body.s = tmp;
						status = URI_OR_TOKEN;;
						break;
					case S_URI_ENCLOSED:
						to_b->uri.s=tmp;
						status=URI_ENCLOSED;
						break;
					case MAYBE_URI_END:
						status = DISPLAY_TOKEN;
					case DISPLAY_QUOTED:
					case DISPLAY_TOKEN:
					case URI_ENCLOSED:
					case URI_OR_TOKEN:
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						DBG("DEBUG:parse_to: spitting out [%c] in status %d\n",
						*tmp,status );
						goto error;
				}
		}/*char switch*/
	}/*for*/

endofheader:
	DBG("DEBUG: status = %d \n",status);
	status=saved_status;
	DBG("end of header reached, state=%d\n", status);
	/* check if error*/
	switch(status){
		case MAYBE_URI_END:
			//*foo=0;
			to_b->uri.len = foo - to_b->uri.s;
		case END:
			to_b->body.len = tmp - to_b->body.s;
	case E_PARA_VALUE:
			*(tmp-1)=0;
			break;
		default:
			LOG(L_ERR, "ERROR: parse_to: invalid To -  unexpected "
					"end of header in state %d\n", status);
			goto error;
	}
	return tmp;

error:
	LOG(L_ERR, "to parse error\n");
	to_b->error=PARSE_ERROR;
	return tmp;

}

