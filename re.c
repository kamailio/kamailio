/* 
 * $Id$
 *
 * regexp and regexp substitutions implementations
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
 *
 * History:
 * --------
 *   2003-08-04  created by andrei
 */


#include "dprint.h"
#include "mem/mem.h"
#include "re.h"

#include <string.h>



void subst_expr_free(struct subst_expr* se)
{
	if (se->replacement.s) pkg_free(se->replacement.s);
	if (se->re) { regfree(se->re); pkg_free(se->re); };
	pkg_free(se);
}



/* frees the entire list, head (l) too */
void replace_lst_free(struct replace_lst* l)
{
	struct replace_lst* t;
	
	while (l){
		t=l;
		l=l->next;
		if (t->rpl.s) pkg_free(t->rpl.s);
		pkg_free(t);
	}
}



/* parse a /regular expression/replacement/flags into a subst_expr structure */
struct subst_expr* subst_parser(str* subst)
{
#define MAX_REPLACE_WITH 100
	char c;
	char* end;
	char* p;
	char* re;
	char* re_end;
	char* repl;
	char* repl_end;
	struct replace_with rw[MAX_REPLACE_WITH];
	int rw_no;
	int escape;
	int cflags; /* regcomp flags */
	int replace_all;
	struct subst_expr* se;
	regex_t* regex;
	int max_pmatch;
	int r;
	
	/* init */
	se=0;
	regex=0;
	cflags=REG_EXTENDED  | REG_NEWLINE; /* don't match newline */
	replace_all=0;
	if (subst->len<3){
		LOG(L_ERR, "ERROR: subst_parser: expression is too short: %.*s\n",
				subst->len, subst->s);
		goto error;
	}
	
	p=subst->s;
	c=*p;
	if (c=='\\'){
		LOG(L_ERR, "ERROR: subst_parser: invalid separator char <%c>"
				" in %.*s\n", c, subst->len, subst->s);
		goto error;
	}
	p++;
	end=subst->s+subst->len;
	/* find re */
	re=p;
	for (;p<end;p++){
		/* if unescaped sep. char */
		if ((*p==c) && (*(p-1)!='\\')) goto found_re;
	}
	LOG(L_ERR, "ERROR: subst_parser: no separator found: %.*s\n", subst->len, 
			subst->s);
	goto error;
found_re:
	re_end=p;
	p++;
	/* parse replacement */
	repl=p;
	rw_no=0;
	max_pmatch=0;
	escape=0;
	for(;p<end; p++){
		if (escape){
			escape=0;
			switch (*p){
				/* special char escapes */
				case '\\':
					rw[rw_no].size=2;
					rw[rw_no].offset=(p-1)-repl;
					rw[rw_no].type=REPLACE_CHAR;
					rw[rw_no].u.c='\\';
					break;
				case 'n':
					rw[rw_no].size=2;
					rw[rw_no].offset=(p-1)-repl;
					rw[rw_no].type=REPLACE_CHAR;
					rw[rw_no].u.c='\n';
					break;
				case 'r':
					rw[rw_no].size=2;
					rw[rw_no].offset=(p-1)-repl;
					rw[rw_no].type=REPLACE_CHAR;
					rw[rw_no].u.c='\r';
					break;
				case 't':
					rw[rw_no].size=2;
					rw[rw_no].offset=(p-1)-repl;
					rw[rw_no].type=REPLACE_CHAR;
					rw[rw_no].u.c='\t';
					break;
				/* special sip msg parts escapes */
				case 'u':
					rw[rw_no].size=2;
					rw[rw_no].offset=(p-1)-repl;
					rw[rw_no].type=REPLACE_URI;
					break;
				/* re matches */
				case '0': /* allow 0, too, reference to the whole match */
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					rw[rw_no].size=2;
					rw[rw_no].offset=(p-1)-repl;
					rw[rw_no].type=REPLACE_NMATCH;
					rw[rw_no].u.nmatch=(*p)-'0';/* 0 is the whole matched str*/
					if (max_pmatch<rw[rw_no].u.nmatch) 
						max_pmatch=rw[rw_no].u.nmatch;
					break;
				default: /* just print current char */
					if (*p!=c){
						LOG(L_WARN, "subst_parser: WARNING: \\%c unknown"
								" escape in %.*s\n", *p, subst->len, subst->s);
					}
					rw[rw_no].size=2;
					rw[rw_no].offset=(p-1)-repl;
					rw[rw_no].type=REPLACE_CHAR;
					rw[rw_no].u.c=*p;
					break;
			}
			rw_no++;
			if (rw_no>=MAX_REPLACE_WITH){
				LOG(L_ERR, "ERROR: subst_parser: too many escapes in the"
							" replace part %.*s\n", subst->len, subst->s);
				goto error;
			}
		}else if (*p=='\\') escape=1;
		else  if (*p==c) goto found_repl;
	}
	LOG(L_ERR, "ERROR: subst_parser: missing separator: %.*s\n", subst->len, 
			subst->s);
	goto error;
found_repl:
	repl_end=p;
	p++;
	/* parse flags */
	for(;p<end; p++){
		switch(*p){
			case 'i':
				cflags|=REG_ICASE;
				break;
			case 's':
				cflags&=(~REG_NEWLINE);
				break;
			case 'g':
				replace_all=1;
				break;
			default:
				LOG(L_ERR, "ERROR: subst_parser: unknown flag %c in %.*s\n",
						*p, subst->len, subst->s);
				goto error;
		}
	}

	/* compile the re */
	if ((regex=pkg_malloc(sizeof(regex_t)))==0){
		LOG(L_ERR, "ERROR: subst_parser: out of memory (re)\n");
		goto error;
	}
	c=*re_end; /* regcomp expects null terminated strings -- save */
	*re_end=0;
	if (regcomp(regex, re, cflags)!=0){
		pkg_free(regex);
		*re_end=c; /* restore */
		LOG(L_ERR, "ERROR: subst_parser: bad regular expression %.*s in "
				"%.*s\n", (int)(re_end-re), re, subst->len, subst->s);
		goto error;
	}
	*re_end=c; /* restore */
	/* construct the subst_expr structure */
	se=pkg_malloc(sizeof(struct subst_expr)+
					((rw_no)?(rw_no-1)*sizeof(struct replace_with):0));
		/* 1 replace_with structure is  already included in subst_expr */
	if (se==0){
		LOG(L_ERR, "ERROR: subst_parser: out of memory (subst_expr)\n");
		goto error;
	}
	memset((void*)se, 0, sizeof(struct subst_expr));
	se->replacement.len=repl_end-repl;
	if ((se->replacement.s=pkg_malloc(se->replacement.len))==0){
		LOG(L_ERR, "ERROR: subst_parser: out of memory (replacement)\n");
		goto error;
	}
	/* start copying */
	memcpy(se->replacement.s, repl, se->replacement.len);
	se->re=regex;
	se->replace_all=replace_all;
	se->n_escapes=rw_no;
	se->max_pmatch=max_pmatch;
	for (r=0; r<rw_no; r++) se->replace[r]=rw[r];
	DBG("subst_parser: ok, se is %p\n", se);
	return se;
	
error:
	if (se) { subst_expr_free(se); regex=0; }
	if (regex) { regfree (regex); pkg_free(regex); }
	return 0;
}



static int replace_len(char* match, int nmatch, regmatch_t* pmatch,
					struct subst_expr* se, struct sip_msg* msg)
{
	int r;
	int len;
	str* uri;
	
	len=se->replacement.len;
	for (r=0; r<se->n_escapes; r++){
		switch(se->replace[r].type){
			case REPLACE_NMATCH:
				len-=se->replace[r].size;
				if ((se->replace[r].u.nmatch<nmatch)&&(
						pmatch[se->replace[r].u.nmatch].rm_so!=-1)){
						/* do the replace */
						len+=pmatch[se->replace[r].u.nmatch].rm_eo-
								pmatch[se->replace[r].u.nmatch].rm_so;
				};
				break;
			case REPLACE_CHAR:
				len-=(se->replace[r].size-1);
				break;
			case REPLACE_URI:
				len-=se->replace[r].size;
				if (msg->first_line.type!=SIP_REQUEST){
					LOG(L_CRIT, "BUG: replace_len: uri substitution on"
								" a reply\n");
					break; /* ignore, we can continue */
				}
				uri= (msg->new_uri.s)?(&msg->new_uri):
					(&msg->first_line.u.request.uri);
				len+=uri->len;
				break;
			default:
				LOG(L_CRIT, "BUG: replace_len: unknown type %d\n", 
						se->replace[r].type);
				/* ignore it */
		}
	}
	return len;
}



/* rpl.s will be alloc'ed with the proper size & rpl.len set
 * returns 0 on success, <0 on error*/
static int replace_build(char* match, int nmatch, regmatch_t* pmatch,
					struct subst_expr* se, struct sip_msg* msg, str* rpl)
{
	int r;
	str* uri;
	char* p;
	char* dest;
	char* end;
	int size;
	
	rpl->len=replace_len(match, nmatch, pmatch, se, msg);
	if (rpl->len==0){
		rpl->s=0; /* emtpy string */
		return 0;
	}
	rpl->s=pkg_malloc(rpl->len);
	if (rpl->s==0){
		LOG(L_ERR, "ERROR: replace_build: out of mem (rpl)\n");
		goto error;
	}
	p=se->replacement.s;
	end=p+se->replacement.len;
	dest=rpl->s;
	for (r=0; r<se->n_escapes; r++){
		/* copy the unescaped parts */
		size=se->replacement.s+se->replace[r].offset-p;
		memcpy(dest, p, size);
		p+=size+se->replace[r].size;
		dest+=size;
		switch(se->replace[r].type){
			case REPLACE_NMATCH:
				if ((se->replace[r].u.nmatch<nmatch)&&(
						pmatch[se->replace[r].u.nmatch].rm_so!=-1)){
						/* do the replace */
						size=pmatch[se->replace[r].u.nmatch].rm_eo-
								pmatch[se->replace[r].u.nmatch].rm_so;
						memcpy(dest, 
								match+pmatch[se->replace[r].u.nmatch].rm_so,
								size);
						dest+=size;
				};
				break;
			case REPLACE_CHAR:
				*dest=se->replace[r].u.c;
				dest++;
				break;
			case REPLACE_URI:
				if (msg->first_line.type!=SIP_REQUEST){
					LOG(L_CRIT, "BUG: replace_build: uri substitution on"
								" a reply\n");
					break; /* ignore, we can continue */
				}
				uri= (msg->new_uri.s)?(&msg->new_uri):
					(&msg->first_line.u.request.uri);
				memcpy(dest, uri->s, uri->len);
				dest+=uri->len;
				break;
			default:
				LOG(L_CRIT, "BUG: replace_build: unknown type %d\n", 
						se->replace[r].type);
				/* ignore it */
		}
	}
	memcpy(dest, p, end-p);
	return 0;
error:
	return -1;
}



/* WARNING: input must be 0 terminated! */
struct replace_lst* subst_run(struct subst_expr* se, char* input,
								struct sip_msg* msg)
{
	struct replace_lst *head;
	struct replace_lst **crt;
	char *p;
	int r;
	regmatch_t* pmatch;
	int nmatch;
	int eflags;
	
	
	/* init */
	head=0;
	crt=&head;
	p=input;
	nmatch=se->max_pmatch+1;
	/* no of () referenced + 1 for the whole string: pmatch[0] */
	pmatch=pkg_malloc(nmatch*sizeof(regmatch_t));
	if (pmatch==0){
		LOG(L_ERR, "ERROR: subst_run_ out of mem. (pmatch)\n");
		goto error;
	}
	eflags=0;
	do{
		r=regexec(se->re, p, nmatch, pmatch, eflags);
		DBG("subst_run: running. r=%d\n", r);
		/* subst */
		if (r==0){ /* != REG_NOMATCH */
			/* change eflags, not to match any more at string start */
			eflags|=REG_NOTBOL;
			*crt=pkg_malloc(sizeof(struct replace_lst));
			if (*crt==0){
				LOG(L_ERR, "ERROR: subst_run: out of mem (crt)\n");
				goto error;
			}
			memset(*crt, sizeof(struct replace_lst), 0);
			if (pmatch[0].rm_so==-1){
				LOG(L_ERR, "ERROR: subst_run: unknown offset?\n");
				goto error;
			}
			(*crt)->offset=pmatch[0].rm_so+(int)(p-input);
			(*crt)->size=pmatch[0].rm_eo-pmatch[0].rm_so;
			DBG("subst_run: matched (%d, %d): [%.*s]\n",
					(*crt)->offset, (*crt)->size, 
					(*crt)->size, input+(*crt)->offset);
			/* create subst. string */
			/* construct the string from replace[] */
			if (replace_build(p, nmatch, pmatch, se, msg, &((*crt)->rpl))<0){
				goto error;
			}
			crt=&((*crt)->next);
			p+=pmatch[0].rm_eo;
		}
	}while((r==0) && se->replace_all);
	pkg_free(pmatch);
	return head;
error:
	if (head) replace_lst_free(head);
	if (pmatch) pkg_free(pmatch);
	return 0;
}



/* return the substitution result in a str, input must be 0 term */ 
str* subst_str(char *input, struct sip_msg* msg, struct subst_expr* se)
{
	str* res;
	struct replace_lst *lst;
	struct replace_lst* l;
	int len;
	int size;
	char* p;
	char* dest;
	char* end;
	
	
	/* compute the len */
	len=strlen(input);
	end=input+len;
	lst=subst_run(se, input, msg);
	for (l=lst; l; l=l->next)
		len+=(int)(l->rpl.len)-l->size;
	res=pkg_malloc(sizeof(str));
	if (res==0){
		LOG(L_ERR, "ERROR: subst_str: mem. allocation error\n");
		goto error;
	}
	res->s=pkg_malloc(len);
	if (res->s==0){
		LOG(L_ERR, "ERROR: subst_str: mem. allocation error (res->s)\n");
		goto error;
	}
	res->len=len;
	
	/* replace */
	dest=res->s;
	p=input;
	for(l=lst; l; l=l->next){
		size=l->offset+input-p;
		memcpy(dest, p, size);
		p+=size;
		dest+=size;
		if (l->rpl.len){
			memcpy(dest, l->rpl.s, l->rpl.len);
			dest+=l->rpl.len;
		}
	}
	memcpy(dest, p, end-p);
	if(lst) replace_lst_free(lst);
	return res;
error:
	if (lst) replace_lst_free(lst);
	if (res){
		if (res->s) pkg_free(res->s);
		pkg_free(res);
	}
	return 0;
}
