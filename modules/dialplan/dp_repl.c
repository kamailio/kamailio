/*
 * $Id$
 *
 * Copyright (C) 2007-2008 Voice Sistem SRL
 *
 * Copyright (C) 2008 Juha Heinanen
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  2007-08-01 initial version (ancuta onofrei)
 */

/*!
 * \file
 * \brief SIP-router dialplan :: database interface - reply parsing
 * \ingroup dialplan
 * Module: \ref dialplan
 */


#include <fnmatch.h>

#include "../../re.h"
#include "../../mem/shm_mem.h"
#include "dialplan.h"


pcre *dpl_dynamic_pcre(sip_msg_t *msg, str *expr, int *cap_cnt)
{
	pv_elem_t *pelem = NULL;
	pcre *re = NULL;
	int ccnt = 0;
	str vexpr;

	if(expr==NULL || expr->s==NULL || expr->len<=0)
		return NULL;

	if(pv_parse_format(expr, &pelem)<0){
		LM_ERR("parsing pcre expression: %.*s\n",
				expr->len, expr->s);
		return NULL;
	}
	if(pv_printf_s(msg, pelem, &vexpr)<0){
		LM_ERR("cannot get pcre dynamic expression value: %.*s\n",
				expr->len, expr->s);
		pv_elem_free_all(pelem);
		return NULL;
	}
	pv_elem_free_all(pelem);

	re = reg_ex_comp(vexpr.s, &ccnt, 1);
	if(!re) {
		LM_ERR("failed to compile pcre expression: %.*s (%.*s)\n",
				expr->len, expr->s, vexpr.len, vexpr.s);
		return NULL;
	}
	if(cap_cnt) {
		*cap_cnt = ccnt;
	}
	LM_DBG("compiled dynamic pcre expression: %.*s (%.*s) %d\n",
				expr->len, expr->s, vexpr.len, vexpr.s, ccnt);
	return re;
}

void repl_expr_free(struct subst_expr *se)
{
	if(!se)
		return;

	if(se->replacement.s){
		shm_free(se->replacement.s);
		se->replacement.s = 0;
	}

	shm_free(se);
	se = 0;
}


struct subst_expr* repl_exp_parse(str subst)
{
	struct replace_with rw[MAX_REPLACE_WITH];
	int rw_no;
	struct subst_expr * se;
	int replace_all;
	char * p, *end, *repl, *repl_end;
	int max_pmatch, r;
	str shms;

	se = 0;
	replace_all = 0;
	shms.s = NULL;

	if (!(shms.s=shm_malloc((subst.len+1) * sizeof(char))) ){
		LM_ERR("out of shm memory\n");
		goto error;
	}
	memcpy(shms.s, subst.s, subst.len);
	shms.len = subst.len;
	shms.s[shms.len] = '\0';

	p = shms.s;
	end = p + shms.len;
	rw_no = 0;

	repl = p;
	if((rw_no = parse_repl(rw, &p, end, &max_pmatch, WITHOUT_SEP))< 0)
		goto error;

	repl_end=p;

	/* construct the subst_expr structure */
	se = shm_malloc(sizeof(struct subst_expr)+
			((rw_no)?(rw_no-1)*sizeof(struct replace_with):0));
	/* 1 replace_with structure is  already included in subst_expr */
	if (se==0){
		LM_ERR("out of shm memory (subst_expr)\n");
		goto error;
	}
	memset((void*)se, 0, sizeof(struct subst_expr));

	se->replacement.s = shms.s;
	shms.s = NULL;
	se->replacement.len=repl_end-repl;
	if(!rw_no){
		replace_all = 1;
	}
	/* start copying */
	LM_DBG("replacement expression is [%.*s]\n", se->replacement.len,
			se->replacement.s);
	se->re=0;
	se->replace_all=replace_all;
	se->n_escapes=rw_no;
	se->max_pmatch=max_pmatch;

	/*replace_with is a simple structure, no shm alloc needed*/
	for (r=0; r<rw_no; r++) se->replace[r]=rw[r];
	return se;

error:
	if(shms.s != NULL)
		shm_free(shms.s);
	if (se) { repl_expr_free(se);}
	return NULL;
}


#define MAX_PHONE_NB_DIGITS		127
static char dp_output_buf[MAX_PHONE_NB_DIGITS+1];
int rule_translate(sip_msg_t *msg, str string, dpl_node_t * rule,
		str * result)
{
	int repl_nb, offset, match_nb, rc, cap_cnt;
	struct replace_with token;
	pcre *subst_comp;
	struct subst_expr * repl_comp;
	str match;
	pv_value_t sv;
	str* uri;
	int ovector[3 * (MAX_REPLACE_WITH + 1)];
	char *p;
	int size;

	dp_output_buf[0] = '\0';
	result->s = dp_output_buf;
	result->len = 0;

	repl_comp 	= rule->repl_comp;
	if(!repl_comp){
		LM_DBG("null replacement\n");
		return 0;
	}

	if(rule->tflags&DP_TFLAGS_PV_SUBST) {
		subst_comp = dpl_dynamic_pcre(msg, &rule->subst_exp, NULL);
	} else {
		subst_comp = rule->subst_comp;
	}

	if(subst_comp){
		/*just in case something went wrong at load time*/
		rc = pcre_fullinfo(subst_comp, NULL, PCRE_INFO_CAPTURECOUNT,
				&cap_cnt);
		if (rc != 0) {
			LM_ERR("pcre_fullinfo on compiled pattern yielded error: %d\n",
					rc);
			if(rule->tflags&DP_TFLAGS_PV_SUBST) pcre_free(subst_comp);
			return -1;
		}
		if(repl_comp->max_pmatch > cap_cnt){
			LM_ERR("illegal access to %i-th subexpr of subst expr (max %d)\n",
					repl_comp->max_pmatch, cap_cnt);
			if(rule->tflags&DP_TFLAGS_PV_SUBST) pcre_free(subst_comp);
			return -1;
		}

		/*search for the pattern from the compiled subst_exp*/
		if (pcre_exec(subst_comp, NULL, string.s, string.len,
					0, 0, ovector, 3 * (MAX_REPLACE_WITH + 1)) <= 0) {
			LM_ERR("the string %.*s matched "
					"the match_exp %.*s but not the subst_exp %.*s!\n", 
					string.len, string.s, 
					rule->match_exp.len, rule->match_exp.s,
					rule->subst_exp.len, rule->subst_exp.s);
			if(rule->tflags&DP_TFLAGS_PV_SUBST) pcre_free(subst_comp);
			return -1;
		}
	}

	/*simply copy from the replacing string*/
	if(!subst_comp || (repl_comp->n_escapes <=0)){
		if(!repl_comp->replacement.s || repl_comp->replacement.len == 0){
			LM_ERR("invalid replacing string\n");
			goto error;
		}
		LM_DBG("simply replace the string, subst_comp %p, n_escapes %i\n",
				subst_comp, repl_comp->n_escapes);
		memcpy(result->s, repl_comp->replacement.s,
				repl_comp->replacement.len);
		result->len = repl_comp->replacement.len;
		result->s[result->len] = '\0';
		if(rule->tflags&DP_TFLAGS_PV_SUBST) pcre_free(subst_comp);
		return 0;
	}

	/* offset- offset in the replacement string */
	result->len = repl_nb = offset = 0;
	p=repl_comp->replacement.s;

	while( repl_nb < repl_comp->n_escapes){

		token = repl_comp->replace[repl_nb];

		if(offset< token.offset){
			if((repl_comp->replacement.len < offset)||
					(result->len + token.offset -offset >= MAX_PHONE_NB_DIGITS)){
				LM_ERR("invalid length\n");
				goto error;
			}
			/*copy from the replacing string*/
			size = token.offset - offset;
			memcpy(result->s + result->len, p + offset, size);
			LM_DBG("copying <%.*s> from replacing string\n",
					size, p + offset);
			result->len += size;
			offset = token.offset;
		}

		switch(token.type) {
			case REPLACE_NMATCH:
				/*copy from the match subexpression*/	
				match_nb = token.u.nmatch * 2;
				match.s =  string.s + ovector[match_nb];
				match.len = ovector[match_nb + 1] - ovector[match_nb];
				if(result->len + match.len >= MAX_PHONE_NB_DIGITS){
					LM_ERR("overflow\n");
					goto error;
				}

				memcpy(result->s + result->len, match.s, match.len);
				LM_DBG("copying match <%.*s> token size %d\n",
						match.len, match.s, token.size);
				result->len += match.len;
				offset += token.size;
				break;
			case REPLACE_CHAR:
				if(result->len + 1>= MAX_PHONE_NB_DIGITS){
					LM_ERR("overflow\n");
					goto error;
				}
				*(result->s + result->len) = token.u.c;
				LM_DBG("copying char <%c> token size %d\n",
						token.u.c, token.size);
				result->len++;
				offset += token.size;
				break;
			case REPLACE_URI:	
				if ( msg== NULL || msg->first_line.type!=SIP_REQUEST){
					LM_CRIT("uri substitution attempt on no request"
							" message\n");
					break; /* ignore, we can continue */
				}
				uri= (msg->new_uri.s)?(&msg->new_uri):
					(&msg->first_line.u.request.uri);
				if(result->len+uri->len>=MAX_PHONE_NB_DIGITS){
					LM_ERR("overflow\n");
					goto error;
				}
				memcpy(result->s + result->len, uri->s, uri->len);
				LM_DBG("copying uri <%.*s> token size %d\n",
						uri->len, uri->s, token.size);
				result->len+=uri->len;
				offset += token.size;
				break;
			case REPLACE_SPEC:
				if (msg== NULL) {
					LM_DBG("replace spec attempted on no message\n");
					break;
				}
				if (pv_get_spec_value(msg, &token.u.spec, &sv) != 0) {
					LM_CRIT("item substitution returned error\n");
					break; /* ignore, we can continue */
				}
				if(result->len+sv.rs.len>=MAX_PHONE_NB_DIGITS){
					LM_ERR("rule_translate: overflow\n");
					goto error;
				}
				memcpy(result->s + result->len, sv.rs.s,
						sv.rs.len);
				LM_DBG("copying pvar value <%.*s> token size %d\n",
						sv.rs.len, sv.rs.s, token.size);
				result->len+=sv.rs.len;
				offset += token.size;
				break;
			default:
				LM_CRIT("unknown type %d\n", repl_comp->replace[repl_nb].type);
				/* ignore it */
		}
		repl_nb++;
	}
	/* anything left? */
	if( repl_nb && offset < repl_comp->replacement.len){
		/*copy from the replacing string*/
		size = repl_comp->replacement.len - offset;
		memcpy(result->s + result->len, p + offset, size);
		LM_DBG("copying leftover <%.*s> from replacing string\n",
				size, p + offset);
		result->len += size;
	}

	result->s[result->len] = '\0';
	if(rule->tflags&DP_TFLAGS_PV_SUBST) pcre_free(subst_comp);
	return 0;

error:
	if((rule->tflags&DP_TFLAGS_PV_SUBST) && subst_comp!=NULL)
		pcre_free(subst_comp);
	result->s = 0;
	result->len = 0;
	return -1;
}

#define DP_MAX_ATTRS_LEN	128
static char dp_attrs_buf[DP_MAX_ATTRS_LEN+1];
int translate(sip_msg_t *msg, str input, str *output, dpl_id_p idp,
		str *attrs)
{
	dpl_node_p rulep;
	dpl_index_p indexp;
	int user_len, rez;
	char b;
	pcre *match_re;

	if(!input.s || !input.len) {
		LM_ERR("invalid input string\n");
		return -1;
	}

	user_len = input.len;
	for(indexp = idp->first_index; indexp!=NULL; indexp = indexp->next)
		if(!indexp->len || (indexp->len!=0 && indexp->len == user_len) )
			break;

	if(!indexp || (indexp!= NULL && !indexp->first_rule)){
		LM_DBG("no rule for len %i\n", input.len);
		return -1;
	}

search_rule:
	for(rulep=indexp->first_rule; rulep!=NULL; rulep= rulep->next) {
		switch(rulep->matchop) {

			case DP_REGEX_OP:
				LM_DBG("regex operator testing over [%.*s]\n",
						input.len, input.s);
				if(rulep->tflags&DP_TFLAGS_PV_MATCH) {
					match_re = dpl_dynamic_pcre(msg, &rulep->match_exp, NULL);
					if(match_re==NULL) {
						/* failed to compile dynamic pcre -- ignore */
						continue;
					}
				} else {
					match_re = rulep->match_comp;
				}
				rez = pcre_exec(match_re, NULL, input.s, input.len,
						0, 0, NULL, 0);
				if(rulep->tflags&DP_TFLAGS_PV_MATCH) {
					pcre_free(match_re);
				}
				break;

			case DP_EQUAL_OP:
				LM_DBG("equal operator testing\n");
				if(rulep->match_exp.len != input.len) {
					rez = -1;
				} else {
					rez = strncmp(rulep->match_exp.s,input.s,input.len);
					rez = (rez==0)?0:-1;
				}
				break;

			case DP_FNMATCH_OP:
				LM_DBG("fnmatch operator testing\n");
				b = input.s[input.len];
				input.s[input.len] = '\0';
				rez = fnmatch(rulep->match_exp.s, input.s, 0);
				input.s[input.len] = b;
				rez = (rez==0)?0:-1;
				break;

			default:
				LM_ERR("bogus match operator code %i\n", rulep->matchop);
				return -1;
		}
		if(rez >= 0)
			goto repl;
	}
	/*test the rules with len 0*/
	if(indexp->len){
		for(indexp = indexp->next; indexp!=NULL; indexp = indexp->next)
			if(!indexp->len)
				break;
		if(indexp)
			goto search_rule;
	}

	LM_DBG("no matching rule\n");
	return -1;

repl:
	LM_DBG("found a matching rule %p: pr %i, match_exp %.*s\n",
			rulep, rulep->pr, rulep->match_exp.len, rulep->match_exp.s);

	if(attrs) {
		attrs->len = 0;
		attrs->s = 0;
		if(rulep->attrs.len>0) {
			LM_DBG("the rule's attrs are %.*s\n",
					rulep->attrs.len, rulep->attrs.s);
			if(rulep->attrs.len >= DP_MAX_ATTRS_LEN) {
				LM_ERR("out of memory for attributes\n");
				return -1;
			}
			attrs->s = dp_attrs_buf;
			memcpy(attrs->s, rulep->attrs.s, rulep->attrs.len*sizeof(char));
			attrs->len = rulep->attrs.len;
			attrs->s[attrs->len] = '\0';

			LM_DBG("the copied attributes are: %.*s\n",
					attrs->len, attrs->s);
		}
	}

	if(rule_translate(msg, input, rulep, output)!=0){
		LM_ERR("could not build the output\n");
		return -1;
	}

	return 0;
}
