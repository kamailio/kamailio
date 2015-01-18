/*
 * Copyright (C) 2001-2003 Fhg Fokus
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

/** Parser :: Parse To: header.
 * @file
 * @ingroup parser
 */

#include "parse_to.h"
#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "msg_parser.h"
#include "parse_uri.h"
#include "../ut.h"
#include "../mem/mem.h"


enum {
	START_TO, DISPLAY_QUOTED, E_DISPLAY_QUOTED, DISPLAY_TOKEN,
	DISPLAY_TOKEN_SP, S_URI_ENCLOSED, URI_ENCLOSED, E_URI_ENCLOSED,
	URI_OR_TOKEN, MAYBE_URI_END, END, F_CR, F_LF, F_CRLF
};


enum {
	S_PARA_NAME=20, PARA_NAME, S_EQUAL, S_PARA_VALUE, TAG1, TAG2,
	TAG3, PARA_VALUE_TOKEN , PARA_VALUE_QUOTED, E_PARA_VALUE
};



#define add_param( _param , _body , _newparam ) \
	do{\
		DBG("DEBUG: add_param: %.*s=%.*s\n",param->name.len,ZSW(param->name.s),\
			param->value.len,ZSW(param->value.s));\
		if (!(_body)->param_lst)  (_body)->param_lst=(_param);\
		else (_body)->last_param->next=(_param);\
		(_body)->last_param =(_param);\
		if ((_param)->type==TAG_PARAM)\
			memcpy(&((_body)->tag_value),&((_param)->value),sizeof(str));\
		_newparam = 0;\
	}while(0);





static char* parse_to_param(char* const buffer, const char* const end,
					struct to_body* const to_b, const int allow_comma_sep,
					int* const returned_status)
{
	struct to_param *param;
	struct to_param *newparam;
	int status;
	int saved_status;
	char  *tmp;

	param=0;
	newparam=0;
	status=E_PARA_VALUE;
	saved_status=E_PARA_VALUE;
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
						status = S_EQUAL;
						break;
					case PARA_VALUE_TOKEN:
						param->value.len = tmp-param->value.s;
						status = E_PARA_VALUE;
						add_param(param, to_b, newparam);
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
						saved_status = S_EQUAL;
						status = F_LF;
						break;
					case PARA_VALUE_TOKEN:
						param->value.len = tmp-param->value.s;
						saved_status = E_PARA_VALUE;
						status = F_LF;
						add_param(param, to_b, newparam);
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
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
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
						saved_status = S_EQUAL;
						status = F_CR;
						break;
					case PARA_VALUE_TOKEN:
						param->value.len = tmp-param->value.s;
						saved_status = E_PARA_VALUE;
						status = F_CR;
						add_param(param, to_b, newparam);
						break;
					case F_CRLF:
					case F_CR:
					case F_LF:
						status=saved_status;
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to_param : "
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case 0:
				switch (status)
				{
					case TAG3:
						param->type = TAG_PARAM;
					case PARA_NAME:
					case TAG1:
					case TAG2:
						param->name.len = tmp-param->name.s;
						status = S_EQUAL;
					case S_EQUAL:
					case S_PARA_VALUE:
						saved_status=status;
						goto endofheader;
					case PARA_VALUE_TOKEN:
						status = E_PARA_VALUE;
						param->value.len = tmp-param->value.s;
						add_param(param , to_b, newparam);
					case E_PARA_VALUE:
						saved_status = status;
						goto endofheader;
						break;
					default:
						LOG( L_ERR , "ERROR: parse_to_param : "
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
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
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
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
						param->value.len=tmp-param->value.s;
						add_param(param, to_b, newparam);
						status = E_PARA_VALUE;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to_param :"
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status,(int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case ';' :
				switch (status)
				{
					case PARA_VALUE_QUOTED:
						break;
					case TAG3:
						param->type = TAG_PARAM;
					case PARA_NAME:
					case TAG1:
					case TAG2:
						param->name.len = tmp-param->name.s;
					case S_EQUAL:
						param->value.s = 0;
						param->value.len = 0;
						goto semicolon_add_param;
					case S_PARA_VALUE:
						param->value.s = tmp;
					case PARA_VALUE_TOKEN:
						param->value.len=tmp-param->value.s;
semicolon_add_param:
						add_param(param, to_b, newparam);
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
						/* link to free mem if not added in to_body list */
						newparam = param;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to_param :"
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
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
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
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
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
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
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
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
							"unexpected char [%c] in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case ',':
				if(status==PARA_VALUE_QUOTED) {
					/* comma is allowed inside quoted values */
					break;
				}
				if (allow_comma_sep)
				{
					switch (status)
					{
						case S_PARA_NAME:
						case S_EQUAL:
						case S_PARA_VALUE:
						case E_PARA_VALUE:
							saved_status=status;
							status=E_PARA_VALUE;
							goto endofheader;
						case TAG3:
							param->type=TAG_PARAM;
						case PARA_NAME:
						case TAG1:
						case TAG2:
							param->name.len = tmp-param->name.s;
							saved_status = S_EQUAL;
							status = E_PARA_VALUE;
							goto endofheader;
						case PARA_VALUE_TOKEN:
							param->value.len = tmp-param->value.s;
							saved_status = E_PARA_VALUE;
							status = E_PARA_VALUE;
							add_param(param, to_b, newparam);
							goto endofheader;
						case F_CRLF:
						case F_CR:
						case F_LF:
							status=saved_status;
							goto endofheader;
						default:
							LOG( L_ERR , "ERROR: parse_to_param : "
								"unexpected char [%c] in status %d: <<%.*s>> .\n",
								*tmp,status, (int)(tmp-buffer), ZSW(buffer));
							goto error;
					}
					break;
				} else {
					if((status==S_PARA_VALUE || status==PARA_VALUE_TOKEN)
							&& param->type==TAG_PARAM) {
						/* if comma is not separator, allow it as part of value
						 * - some user agents use it */
						break;
					}
					LOG( L_ERR, "ERROR parse_to_param : "
							"invalid character ',' in status %d: <<%.*s>>\n",
							status, (int)(tmp-buffer), ZSW(buffer));
				}
			default:
				switch (status)
				{
					case TAG1:
					case TAG2:
					case TAG3:
						status = PARA_NAME;
						break;
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
						LOG(L_ERR, "ERROR: parse_to_param: "
							"spitting out [%c] in status %d\n",*tmp,status );
						goto error;
				}
		}/*switch*/
	}/*for*/
	if (!(status==F_CR || status==F_LF || status==F_CRLF))
		saved_status=status;


endofheader:
	switch(saved_status){
		case TAG3:
			param->type = TAG_PARAM; /* tag at the end */
			/* no break */
		case PARA_NAME:
		case TAG1:
		case TAG2:
			param->name.len = tmp-param->name.s;
			/* no break */
		case S_EQUAL:
			/* parameter without '=', e.g. foo */
			param->value.s=0;
			param->value.len=0;
			add_param(param, to_b, newparam);
			saved_status=E_PARA_VALUE;
			break;
		case S_PARA_VALUE:
			/* parameter with null value, e.g. foo= */
			if ( status==F_CRLF )
				param->value.s=tmp-2;
			else if ( status==F_CR || status==F_LF )
				param->value.s=tmp-1;
			else
				param->value.s=tmp;
			param->value.len=0;
			add_param(param, to_b, newparam);
			saved_status=E_PARA_VALUE;
			break;
		case PARA_VALUE_TOKEN:
			param->value.len=tmp-param->value.s;
			add_param(param, to_b, newparam);
			saved_status=E_PARA_VALUE;
			break;
		case E_PARA_VALUE:
			break;
		default:
			LOG( L_ERR , "ERROR: parse_to_param : unexpected end of header,"
						" status %d: <<%.*s>> .\n",
						saved_status, (int)(tmp-buffer), ZSW(buffer));
			goto error;
	}
	*returned_status=saved_status;
	return tmp;

error:
	if (newparam) pkg_free(newparam);
	to_b->error=PARSE_ERROR;
	*returned_status = status;
	return tmp;
}



char* parse_addr_spec(char* const buffer, const char* const end, struct to_body* const to_b, const int allow_comma_sep)
{
	int status;
	int saved_status;
	char  *tmp,*foo;
	
	saved_status=START_TO; /* fixes gcc 4.x warning */
	status=START_TO;
	memset(to_b, 0, sizeof(struct to_body));
	to_b->error=PARSE_OK;
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
						status = E_URI_ENCLOSED;
						break;
					case URI_OR_TOKEN:
						foo = tmp;
						status = MAYBE_URI_END;
						break;
					case DISPLAY_TOKEN:
						foo = tmp;
						status = DISPLAY_TOKEN_SP;
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
					case DISPLAY_TOKEN_SP:
					case E_DISPLAY_QUOTED:
					case END:
						saved_status=status;
						status=F_LF;
						break;
					case DISPLAY_TOKEN:
						foo=tmp;
						saved_status=DISPLAY_TOKEN_SP;
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
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case '\r':
				switch (status)
				{
					case URI_OR_TOKEN:
						foo = tmp;
						status = MAYBE_URI_END;
					case MAYBE_URI_END:
					case DISPLAY_TOKEN_SP:
					case E_DISPLAY_QUOTED:
					case END:
						saved_status=status;
						status=F_CR;
						break;
					case DISPLAY_TOKEN:
						foo=tmp;
						saved_status=DISPLAY_TOKEN_SP;
						status=F_CR;
						break;
					case F_CRLF:
					case F_CR:
					case F_LF:
						status=saved_status;
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case 0:
				switch (status)
				{
					case URI_OR_TOKEN:
					case MAYBE_URI_END:
						to_b->uri.len = tmp - to_b->uri.s;
					case END:
						saved_status = status = END;
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case '\\':
				switch (status)
				{
					case DISPLAY_QUOTED:
						tmp++; /* jump over next char */
						break;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
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
						status = S_URI_ENCLOSED;
						break;
					case URI_OR_TOKEN:
					case DISPLAY_TOKEN:
						to_b->display.len=tmp-to_b->display.s;
						status = S_URI_ENCLOSED;
						break;
					case DISPLAY_TOKEN_SP:
					case MAYBE_URI_END:
						to_b->display.len=foo-to_b->display.s;
						status = S_URI_ENCLOSED;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case '>':
				switch (status)
				{
					case DISPLAY_QUOTED:
						break;
					case URI_ENCLOSED:
						to_b->uri.len = tmp - to_b->uri.s;
					case E_URI_ENCLOSED:
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
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), ZSW(buffer));
						goto error;
				}
				break;
			case '"':
				switch (status)
				{
					case START_TO:
						to_b->body.s = tmp;
						to_b->display.s = tmp;
						status = DISPLAY_QUOTED;
						break;
					case DISPLAY_QUOTED:
						status = E_DISPLAY_QUOTED;
						to_b->display.len = tmp-to_b->display.s+1;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), buffer);
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
						tmp = parse_to_param(tmp,end,to_b,allow_comma_sep,&saved_status);
						goto endofheader;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
							"in status %d: <<%.*s>> .\n",
							*tmp,status, (int)(tmp-buffer), buffer);
						goto error;
				}
				break;
			case ',' :
				if (allow_comma_sep)
				{
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
							saved_status = END;
							goto endofheader;
						case F_CRLF:
						case F_LF:
						case F_CR:
							/*previous=crlf and now !=' '*/
							goto endofheader;
						default:
							LOG( L_ERR , "ERROR: parse_to : unexpected char [%c] "
								"in status %d: <<%.*s>> .\n",
								*tmp,status, (int)(tmp-buffer), buffer);
							goto error;
					}
					break;
				}
				/* If commas not allowed treat as a default character */
			default:
				switch (status)
				{
					case START_TO:
						to_b->uri.s = to_b->body.s = tmp;
						status = URI_OR_TOKEN;
						to_b->display.s=tmp;
						break;
					case S_URI_ENCLOSED:
						to_b->uri.s=tmp;
						status=URI_ENCLOSED;
						break;
					case MAYBE_URI_END:
					case DISPLAY_TOKEN_SP:
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

	/* Reached end of buffer */
	switch (status)
	{
		case URI_OR_TOKEN:
		case MAYBE_URI_END:
		case END:
			saved_status = status;
			foo = tmp;
	}

endofheader:
	if (to_b->display.len==0) to_b->display.s=0;
	status=saved_status;
	DBG("end of header reached, state=%d\n", status);
	/* check if error*/
	switch(status){
		case URI_OR_TOKEN:
		case MAYBE_URI_END:
			to_b->uri.len = foo - to_b->uri.s;
		case END:
			to_b->body.len = tmp - to_b->body.s;
		case E_PARA_VALUE:
			break;
		default:
			LOG(L_ERR, "ERROR: parse_to: invalid To -  unexpected "
					"end of header in state %d\n", status);
			goto error;
	}
	return tmp;

error:
	free_to_params(to_b);
	to_b->error=PARSE_ERROR;
	return tmp;

}


void free_to_params(struct to_body* const tb)
{
	struct to_param *tp=tb->param_lst;
	struct to_param *foo;
	while (tp){
		foo = tp->next;
		pkg_free(tp);
		tp=foo;
	}
	tb->param_lst = NULL;
}


void free_to(struct to_body* const tb)
{
	free_to_params(tb);
	pkg_free(tb);
}

