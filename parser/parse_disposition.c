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
 * \brief Parser :: Content-Disposition header
 *
 * \ingroup parser
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "../mem/mem.h"
#include "../dprint.h"
#include "../ut.h"
#include "parse_disposition.h"



/*! \brief parse a string that supposed to be a disposition and fills up the structure
 * Returns: -1 : error
 *           o : success */
int parse_disposition( str *s, struct disposition *disp)
{
	enum { FIND_TYPE, TYPE, END_TYPE, FIND_PARAM, PARAM, END_PARAM, FIND_VAL,
	       FIND_QUOTED_VAL, QUOTED_VAL, SKIP_QUOTED_VAL, VAL, END_VAL,
	       F_LF, F_CR, F_CRLF};
	struct disposition_param *disp_p;
	struct disposition_param *new_p;
	int  state;
	int  saved_state;
	char *tmp;
	char *end;

	state = saved_state = FIND_TYPE;
	end = s->s + s->len;
	disp_p = 0;

	for( tmp=s->s; tmp<end; tmp++) {
		switch(*tmp) {
			case ' ':
			case '\t':
				switch (state) {
					case FIND_QUOTED_VAL:
						disp_p->body.s = tmp;
						state = QUOTED_VAL;
						break;
					case SKIP_QUOTED_VAL:
						state = QUOTED_VAL;
						break;
					case TYPE:
						disp->type.len = tmp - disp->type.s;
						state = END_TYPE;
						break;
					case PARAM:
						disp_p->name.len = tmp - disp_p->name.s;
						state = END_PARAM;
						break;
					case VAL:
						disp_p->body.len = tmp - disp_p->body.s;
						state = END_VAL;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now =' '*/
						state=saved_state;
						break;
				}
				break;
			case '\n':
				switch (state) {
					case TYPE:
						disp->type.len = tmp - disp->type.s;
						saved_state = END_TYPE;
						state = F_LF;
						break;
					case PARAM:
						disp_p->name.len = tmp - disp_p->name.s;
						saved_state = END_PARAM;
						state = F_LF;
						break;
					case VAL:
						disp_p->body.len = tmp - disp_p->body.s;
						saved_state = END_VAL;
						state = F_CR;
						break;
					case FIND_TYPE:
					case FIND_PARAM:
						saved_state=state;
						state=F_LF;
						break;
					case F_CR:
						state=F_CRLF;
						break;
					default:
						LOG(L_ERR,"ERROR:parse_disposition: unexpected "
							"char [%c] in status %d: <<%.*s>> .\n",
							*tmp,state, (int)(tmp-s->s), s->s);
						goto error;
				}
				break;
			case '\r':
				switch (state) {
					case TYPE:
						disp->type.len = tmp - disp->type.s;
						saved_state = END_TYPE;
						state = F_CR;
						break;
					case PARAM:
						disp_p->name.len = tmp - disp_p->name.s;
						saved_state = END_PARAM;
						state = F_CR;
						break;
					case VAL:
						disp_p->body.len = tmp - disp_p->body.s;
						saved_state = END_VAL;
						state = F_CR;
						break;
					case FIND_TYPE:
					case FIND_PARAM:
						saved_state=state;
						state=F_CR;
						break;
					default:
						LOG(L_ERR,"ERROR:parse_disposition: unexpected "
							"char [%c] in status %d: <<%.*s>> .\n",
							*tmp,state, (int)(tmp-s->s), ZSW(s->s));
						goto error;
				}
				break;
			case 0:
				LOG(L_ERR,"ERROR:parse_disposition: unexpected "
					"char [%c] in status %d: <<%.*s>> .\n",
					*tmp,state, (int)(tmp-s->s), ZSW(s->s));
				goto error;
				break;
			case ';':
				switch (state) {
					case FIND_QUOTED_VAL:
						disp_p->body.s = tmp;
						state = QUOTED_VAL;
						break;
					case SKIP_QUOTED_VAL:
						state = QUOTED_VAL;
					case QUOTED_VAL:
						break;
					case VAL:
						disp_p->body.len = tmp - disp_p->body.s;
						state = FIND_PARAM;
						break;
					case PARAM:
						disp_p->name.len = tmp - disp_p->name.s;
						state = FIND_PARAM;
						break;
					case TYPE:
						disp->type.len = tmp - disp->type.s;
					case END_TYPE:
					case END_VAL:
						state = FIND_PARAM;
						break;
					default:
						LOG(L_ERR,"ERROR:parse_disposition: unexpected "
							"char [%c] in status %d: <<%.*s>> .\n",
							*tmp,state, (int)(tmp-s->s), ZSW(s->s));
						goto error;
				}
				break;
			case '=':
				switch (state) {
					case FIND_QUOTED_VAL:
						disp_p->body.s = tmp;
						state = QUOTED_VAL;
						break;
					case SKIP_QUOTED_VAL:
						state = QUOTED_VAL;
					case QUOTED_VAL:
						break;
					case PARAM:
						disp_p->name.len = tmp - disp_p->name.s;
					case END_PARAM:
						state = FIND_VAL;
						break;
					default:
						LOG(L_ERR,"ERROR:parse_disposition: unexpected "
							"char [%c] in status %d: <<%.*s>> .\n",
							*tmp,state, (int)(tmp-s->s), ZSW(s->s));
						goto error;
				}
				break;
			case '\"':
				switch (state) {
					case SKIP_QUOTED_VAL:
						state = QUOTED_VAL;
						break;
					case FIND_VAL:
						state = FIND_QUOTED_VAL;
						break;
					case QUOTED_VAL:
						disp_p->body.len = tmp - disp_p->body.s;
						disp_p->is_quoted = 1;
						state = END_VAL;
						break;
					default:
						LOG(L_ERR,"ERROR:parse_disposition: unexpected "
							"char [%c] in status %d: <<%.*s>> .\n",
							*tmp,state, (int)(tmp-s->s), ZSW(s->s));
						goto error;
				}
				break;
			case '\\':
				switch (state) {
					case FIND_QUOTED_VAL:
						disp_p->body.s = tmp;
						state = SKIP_QUOTED_VAL;
						break;
					case SKIP_QUOTED_VAL:
						state = QUOTED_VAL;
						break;
					case QUOTED_VAL:
						state = SKIP_QUOTED_VAL;
						break;
					default:
						LOG(L_ERR,"ERROR:parse_disposition: unexpected "
							"char [%c] in status %d: <<%.*s>> .\n",
							*tmp,state, (int)(tmp-s->s), ZSW(s->s));
						goto error;
				}
				break;
			case '(':
			case ')':
			case '<':
			case '>':
			case '@':
			case ',':
			case ':':
			case '/':
			case '[':
			case ']':
			case '?':
			case '{':
			case '}':
				switch (state) {
					case FIND_QUOTED_VAL:
						disp_p->body.s = tmp;
						state = QUOTED_VAL;
						break;
					case SKIP_QUOTED_VAL:
						state = QUOTED_VAL;
					case QUOTED_VAL:
						break;
					default:
						LOG(L_ERR,"ERROR:parse_disposition: unexpected "
							"char [%c] in status %d: <<%.*s>> .\n",
							*tmp,state, (int)(tmp-s->s), ZSW(s->s));
						goto error;
				}
				break;
			default:
				switch (state) {
					case SKIP_QUOTED_VAL:
						state = QUOTED_VAL;
					case QUOTED_VAL:
						break;
					case FIND_TYPE:
						disp->type.s = tmp;
						state = TYPE;
						break;
					case FIND_PARAM:
						new_p=(struct disposition_param*)pkg_malloc
							(sizeof(struct disposition_param));
						if (new_p==0) {
							LOG(L_ERR,"ERROR:parse_disposition: no more "
								"pkg mem\n");
							goto error;
						}
						memset(new_p,0,sizeof(struct disposition_param));
						if (disp_p==0)
							disp->params = new_p;
						else
							disp_p->next = new_p;
						disp_p = new_p;
						disp_p->name.s = tmp;
						state = PARAM;
						break;
					case FIND_VAL:
						disp_p->body.s = tmp;
						state = VAL;
						break;
					case FIND_QUOTED_VAL:
						disp_p->body.s = tmp;
						state = QUOTED_VAL;
						break;
				}
		}/*switch*/
	}/*for*/

	/* check which was the last parser state */
	switch (state) {
		case END_PARAM:
		case END_TYPE:
		case END_VAL:
			break;
		case TYPE:
			disp->type.len = tmp - disp->type.s;
			break;
		case PARAM:
			disp_p->name.len = tmp - disp_p->name.s;
			break;
		case VAL:
			disp_p->body.len = tmp - disp_p->body.s;
			break;
		default:
			LOG(L_ERR,"ERROR:parse_disposition: wrong final state (%d)\n",
				state);
			goto error;
	}
	return 0;
error:
	return -1;
}



/*! \brief Frees the entire disposition structure (params + itself) */
void free_disposition( struct disposition **disp)
{
	struct disposition_param *param;

	/* free the params */
	while((*disp)->params) {
		param = (*disp)->params->next;
		pkg_free( (*disp)->params);
		(*disp)->params = param;
	}
	pkg_free( *disp );
	*disp = 0;
}



/*! \brief looks inside the message, gets the Content-Disposition hdr, parse it, builds
 * and fills a disposition structure for it what will be attached to hdr as
 * parsed link.
 * Returns:  -1 : error
 *            0 : success
 *            1 : hdr not found
 */
int parse_content_disposition( struct sip_msg *msg )
{
	struct disposition *disp;

	/* look for Content-Disposition header */
	if (msg->content_disposition==0) {
		if (parse_headers(msg, HDR_CONTENTDISPOSITION_F, 0)==-1)
			goto error;
		if (msg->content_disposition==0) {
			DBG("DEBUG:parse_content_disposition: hdr not found\n");
			return 1;
		}
	}

	/* now, we have the header -> look if it isn't already parsed */
	if (msg->content_disposition->parsed!=0) {
		/* already parsed, nothing more to be done */
		return 0;
	}

	/* parse the body */
	disp = (struct disposition*)pkg_malloc(sizeof(struct disposition));
	if (disp==0) {
		LOG(L_ERR,"ERROR:parse_content_disposition: no more pkg memory\n");
		goto error;
	}
	memset(disp,0,sizeof(struct disposition));

	if (parse_disposition( &(msg->content_disposition->body), disp)==-1) {
		/* error when parsing the body */
		free_disposition( &disp );
		goto error;
	}

	/* attach the parsed form to the header */
	msg->content_disposition->parsed = (void*)disp;

	return 0;
error:
	return -1;
}


/*! \brief Prints recursive a disposition structure */
void print_disposition( struct disposition *disp)
{
	struct disposition_param *param;

	DBG("*** Disposition type=<%.*s>[%d]\n",
		disp->type.len,disp->type.s,disp->type.len);
	for( param=disp->params; param; param=param->next) {
		DBG("*** Disposition param: <%.*s>[%d]=<%.*s>[%d] is_quoted=%d\n",
			param->name.len,param->name.s, param->name.len,
			param->body.len,param->body.s, param->body.len,
			param->is_quoted);
	}
}


