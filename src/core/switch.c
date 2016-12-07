/* 
 * Copyright (C) 2009 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief Kamailio core :: 
 * \ingroup core
 * Module: \ref core
 */

#include "switch.h"
#include "rvalue.h"
#include "route.h"
#include "mem/mem.h"
#include "error.h"

#define MAX_JT_SIZE 100  /* maximum jump table size */


/** create a cond table structure (pkg_malloc'ed).
 * @return 0 on error, pointer on success
 */
static struct switch_cond_table* mk_switch_cond_table(int n)
{
	struct switch_cond_table* sct;
	
	/* allocate everything in a single block, better for cache locality */
	sct=pkg_malloc(ROUND_INT(sizeof(*sct))+
					ROUND_POINTER(n*sizeof(sct->cond[0]))+
					n*sizeof(sct->jump[0]));
	if (sct==0) return 0;
	sct->n=n;
	sct->cond=(int*)((char*)sct+ROUND_INT(sizeof(*sct)));
	sct->jump=(struct action**)
				((char*)sct->cond+ROUND_POINTER(n*sizeof(sct->cond[0])));
	sct->def=0;
	return sct;
}



/** create a jump table structure (pkg_malloc'ed).
 * @param jmp_size - size of the jump table
 * @param rest - size of the fallback condition table
 * @return 0 on error, pointer on success
 */
static struct switch_jmp_table* mk_switch_jmp_table(int jmp_size, int rest)
{
	struct switch_jmp_table* jt;
	int size;
	
	/* alloc everything in a block */
	size = 	ROUND_POINTER(sizeof(*jt))+
			ROUND_INT(jmp_size*sizeof(jt->tbl[0]))+
			ROUND_POINTER(rest*sizeof(jt->rest.cond[0]))+
			rest*sizeof(jt->rest.jump[0]);
	jt=pkg_malloc(size);
	if (jt == 0) return 0;
	memset(jt, 0, size);
	jt->tbl = (struct action**)((char*) jt + ROUND_POINTER(sizeof(*jt)));
	jt->rest.cond = (int*)
					((char*) jt->tbl + ROUND_INT(jmp_size*sizeof(jt->tbl[0])));
	jt->rest.jump = (struct action**) ((char*) jt->rest.cond + 
								ROUND_POINTER(rest*sizeof(jt->rest.cond[0])));
	jt->rest.n=rest;
	return jt;
}



/** create a match cond table structure (pkg_malloc'ed).
 * @return 0 on error, pointer on success
 */
static struct match_cond_table* mk_match_cond_table(int n)
{
	struct match_cond_table* mct;
	
	/* allocate everything in a single block, better for cache locality */
	mct=pkg_malloc(ROUND_POINTER(sizeof(*mct))+
					ROUND_POINTER(n*sizeof(mct->match[0]))+
					n*sizeof(mct->jump[0]));
	if (mct==0) return 0;
	mct->n=n;
	mct->match=(struct match_str*)((char*)mct+ROUND_POINTER(sizeof(*mct)));
	mct->jump=(struct action**)
				((char*)mct->match+ROUND_POINTER(n*sizeof(mct->match[0])));
	mct->def=0;
	return mct;
}



static int fix_match(struct action* t);



void destroy_case_stms(struct case_stms *lst)
{
	struct case_stms* l;
	struct case_stms* n;
	
	for (l=lst; l; l=n){
		n=l->next;
		rve_destroy(l->ct_rve);
		/* TODO: the action list is not freed (missing destroy_action() and
		   there are some case when we need at least part of the action list
		*/
		pkg_free(l);
	}
}



/** fixup function for SWITCH_T actions.
 * can produce 4 different action types:
 *  - BLOCK_T (actions) - actions grouped in a block, break ends the block
 *    execution.
 *  - EVAL_T (cond)  - null switch block, but the condition has to be
 *                       evaluated due to possible side-effects.
 *  - SWITCH_COND_T(cond, jumps) - condition table
 *  - SWITCH_JT_T(cond, jumptable) - jumptable + condition table
 * TODO: external optimizers that would "flatten" BLOCK_T w/no breaks or
 * breaks at the end.
 * 
 */
int fix_switch(struct action* t)
{
	struct case_stms* c;
	int n, i, j, ret, val;
	struct action* a;
	struct action* block;
	struct action* def_a;
	struct action* action_lst;
	struct action** tail;
	struct switch_jmp_table* jmp;
	struct switch_cond_table* sct;
	struct action** def_jmp_bm;
	int* cond;
	struct action*** jmp_bm;
	int default_found;
	int first, last, start, end, hits, best_hits;
	struct rval_expr* sw_rve;
	
	ret=E_BUG;
	cond=0;
	jmp_bm=0;
	def_jmp_bm=0;
	default_found=0;
	/* check if string switch (first case is string or RE) */
	for (c=(struct case_stms*)t->val[1].u.data; c && c->is_default; c=c->next);
	if (c && (c->type==MATCH_STR || c->type==MATCH_RE))
		return fix_match(t);
	
	sw_rve=(struct rval_expr*)t->val[0].u.data;
	/*  handle null actions: optimize away if no
	   sideffects */
	if (t->val[1].u.data==0){
		if (!rve_has_side_effects(sw_rve)){
			t->type=BLOCK_T;
			rve_destroy(sw_rve);
			t->val[0].type=BLOCK_ST;
			t->val[0].u.data=0;
			LM_DBG("null switch optimized away\n");
		}else{
			t->type=EVAL_T;
			t->val[0].type=RVE_ST;
			LM_DBG("null switch turned to EVAL_T\n");
		}
		return 0;
	}
	def_a=0;
	n=0;
	for (c=(struct case_stms*)t->val[1].u.data; c; c=c->next){
		if (c->ct_rve){
			if (c->type!=MATCH_INT){
				LM_ERR("wrong case type %d (int expected)\n", c->type);
				return E_UNSPEC;
			}
			if (!rve_is_constant(c->ct_rve)){
				LM_ERR("non constant expression in case\n");
				return E_BUG;
			}
			if (rval_expr_eval_int(0, 0,  &c->label.match_int, c->ct_rve)
					<0){
				LM_ERR("case expression (%d,%d) has non-interger type\n",
						c->ct_rve->fpos.s_line,
						c->ct_rve->fpos.s_col);
				return E_BUG;
			}
			c->is_default=0;
			n++; /* count only non-default cases */
		}else{
			if (default_found){
				LM_ERR("more then one \"default\"");
				return E_UNSPEC;
			}
			default_found=1;
			c->label.match_int=-1;
			c->is_default=1;
			def_a=c->actions;
		}
		if ( c->actions && ((ret=fix_actions(c->actions))<0))
			goto error;
	}
	LM_DBG("%d cases, %d default\n", n, default_found);
	/*: handle n==0 (no case only a default:) */
	if (n==0){
		if (default_found){
			if (!rve_has_side_effects(sw_rve)){
				t->type=BLOCK_T;
				rve_destroy(sw_rve);
				destroy_case_stms(t->val[1].u.data);
				t->val[0].type=BLOCK_ST;
				t->val[0].u.data=def_a;
				t->val[1].type=0;
				t->val[1].u.data=0;
				LM_DBG("default only switch optimized away (BLOCK_T)\n");
				return 0;
			}
			LM_CRIT("default only switch with side-effect not expected at this point\n");
			ret=E_BUG;
			goto error;
		}else{
			LM_CRIT("empty switch not expected at this point\n");
			ret=E_BUG;
			goto error;
		}
	}
	cond=pkg_malloc(sizeof(cond[0])*n);
	jmp_bm=pkg_malloc(sizeof(jmp_bm[0])*n);
	if (cond==0 || jmp_bm==0){
		LM_ERR("memory allocation failure\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	
	/* fill condition table and jump point bookmarks and "flatten" the action 
	   lists (transform them into a single list for the entire switch, rather
	    then one block per case ) */
	n=0;
	action_lst=0;
	tail=&action_lst;
	for (c=(struct case_stms*)t->val[1].u.data; c; c=c->next){
		a=c->actions;
		if (a){
			for (; a->next; a=a->next);
			if (action_lst==0)
				action_lst=c->actions;
			else
				*tail=c->actions;
		}
		if (c->is_default){
			def_jmp_bm=tail;
		} else {
			for (j=0; j<n; j++){
				if (cond[j]==c->label.match_int){
					LM_ERR("duplicate case (%d,%d)\n",
							c->ct_rve->fpos.s_line, c->ct_rve->fpos.s_col);
					ret=E_UNSPEC;
					goto error;
				}
			}
			cond[n]=c->label.match_int;
			jmp_bm[n]=tail;
			n++;
		}
		if (c->actions)
			tail=&a->next;
	}
	/* handle constant rve w/ no side-effects: replace the whole case 
	   with the case rve block */
	if ( (scr_opt_lev>=2) &&
			!rve_has_side_effects(sw_rve) && rve_is_constant(sw_rve)){
		if (rval_expr_eval_int(0, 0,  &val, sw_rve) <0){
			LM_ERR("wrong type for switch(...) expression (%d,%d)\n", 
					sw_rve->fpos.s_line, sw_rve->fpos.s_col);
			ret=E_UNSPEC;
			goto error;
		}
		/* start with the "default:" value in case nothing is found */
		block=def_jmp_bm?*def_jmp_bm:0;
		for (i=0; i<n; i++){
			if (cond[i]==val){
				block=*jmp_bm[i];
				break;
			}
		}
		t->type=BLOCK_T;
		rve_destroy(sw_rve);
		t->val[0].type=BLOCK_ST;
		t->val[0].u.data=block;
		destroy_case_stms(t->val[1].u.data);
		t->val[1].type=0;
		t->val[1].u.data=0;
		ret=0;
		LM_DBG("constant switch(%d) with %d cases optimized away to case"
				" %d \n", val, n, i);
		goto end;
	}
	/* try to create a jumptable */
	/* cost: 2 cmp & table lookup
	   => makes sense for more then 3 cases
	   & if size< MAX_JT_SIZE
	*/
	best_hits=3; /* more then 3 hits needed */
	start=end=0;
	for (i=0; i<n; i++){
		last=first=cond[i];
		hits=1;
		for (j=0; j<n; j++){
			if ((i==j) || (cond[j]<=first)) continue;
			if (cond[j]<last)
				hits++;
			else if ((cond[j]-first)<MAX_JT_SIZE){
				last=cond[j];
				hits++;
			}
		}
		if (hits>best_hits){
			best_hits=hits;
			start=first;
			end=last;
			if (hits==n) break;
		}
	}
	if (start!=end){
		/* build jumptable: end-start entries and
		 with a n-best_hits normal switch table */
		jmp=mk_switch_jmp_table(end-start+1, n-best_hits);
		if (jmp==0){
			LM_ERR("memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		jmp->first=start;
		jmp->last=end;
		jmp->rest.n=n-best_hits;
		jmp->rest.def=def_jmp_bm?*def_jmp_bm:0;
		/* fill it with default values */
		for (i=0; i<=(end-start); i++)
			jmp->tbl[i]=jmp->rest.def;
		for (i=0, j=0; i<n; i++){
			if (cond[i]>=start && cond[i]<=end){
				jmp->tbl[cond[i]-start]=*jmp_bm[i];
			}else{
				jmp->rest.cond[j]=cond[i];
				jmp->rest.jump[j]=*jmp_bm[i];
				j++;
			}
		}
		t->type=SWITCH_JT_T;
		t->val[1].type=JUMPTABLE_ST;
		t->val[1].u.data=jmp;
		ret=0;
		LM_DBG("optimized to jumptable [%d, %d] and %d condtable,"
				" default: %s\n ",
				jmp->first, jmp->last, jmp->rest.n, jmp->rest.def?"yes":"no");
	}else{
		sct=mk_switch_cond_table(n);
		if (sct==0){
			LM_ERR("memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		sct->n=n;
		for (i=0; i<n; i++){
			sct->cond[i]=cond[i];
			sct->jump[i]=*jmp_bm[i];
		}
		sct->def=def_jmp_bm?*def_jmp_bm:0;
		t->type=SWITCH_COND_T;
		t->val[1].type=CONDTABLE_ST;
		t->val[1].u.data=sct;
		LM_DBG("optimized to condtable (%d) default: %s\n",
				sct->n, sct->def?"yes":"no");
		ret=0;
	}
end:
error:
	if (cond) pkg_free(cond);
	if (jmp_bm) pkg_free(jmp_bm);
	return ret;
}



/** fixup function for MATCH_T actions.
 * can produce 3 different action types:
 *  - BLOCK_T (actions) - actions grouped in a block, break ends the block
 *    execution.
 *  - EVAL_T (cond)  - null switch block, but the condition has to be
 *                       evaluated due to possible side-effects.
 *  - MATCH_COND_T(cond, jumps) - condition table
 */
static int fix_match(struct action* t)
{
	struct case_stms* c;
	int n, i, j, ret;
	struct action* a;
	struct action* block;
	struct action* def_a;
	struct action* action_lst;
	struct action** tail;
	struct match_cond_table* mct;
	struct action** def_jmp_bm;
	struct match_str* match;
	struct action*** jmp_bm;
	int default_found;
	struct rval_expr* m_rve;
	struct rvalue* rv;
	regex_t* regex;
	str s;
	
	ret=E_BUG;
	match=0;
	jmp_bm=0;
	def_jmp_bm=0;
	default_found=0;
	rv=0;
	s.s=0;
	s.len=0;
	m_rve=(struct rval_expr*)t->val[0].u.data;
	/*  handle null actions: optimize away if no
	   sideffects */
	if (t->val[1].u.data==0){
		if (!rve_has_side_effects(m_rve)){
			t->type=BLOCK_T;
			rve_destroy(m_rve);
			t->val[0].type=BLOCK_ST;
			t->val[0].u.data=0;
			LM_DBG("null switch optimized away\n");
		}else{
			t->type=EVAL_T;
			t->val[0].type=RVE_ST;
			LM_DBG("null switch turned to EVAL_T\n");
		}
		return 0;
	}
	def_a=0;
	n=0;
	for (c=(struct case_stms*)t->val[1].u.data; c; c=c->next){
		if (c->ct_rve){
			if (c->type!=MATCH_STR && c->type!=MATCH_RE){
				LM_ERR("wrong case type %d (string"
							"or RE expected)\n", c->type);
				return E_UNSPEC;
			}
			if (!rve_is_constant(c->ct_rve)){
				LM_ERR("non constant expression in case\n");
				ret=E_BUG;
				goto error;
			}
			if ((rv=rval_expr_eval(0, 0, c->ct_rve)) == 0 ){
				LM_ERR("bad case expression (%d,%d)\n",
						c->ct_rve->fpos.s_line,
						c->ct_rve->fpos.s_col);
				ret=E_BUG;
				goto error;
			}
			if (rval_get_str(0, 0, &s, rv, 0)<0){
				LM_ERR("(%d,%d): out of memory?\n",
						c->ct_rve->fpos.s_line,
						c->ct_rve->fpos.s_col);
				ret=E_BUG;
				goto error;
			}
			if (c->type==MATCH_RE){
				if ((regex=pkg_malloc(sizeof(regex_t))) == 0){
					LM_ERR("out of memory\n");
					ret=E_OUT_OF_MEM;
					goto error;
				}
				if (regcomp(regex, s.s, 
							REG_EXTENDED | REG_NOSUB | c->re_flags) !=0){
					pkg_free(regex);
					regex=0;
					LM_ERR("(%d, %d): bad regular expression %.*s\n",
							c->ct_rve->fpos.s_line,
							c->ct_rve->fpos.s_col,
							s.len, ZSW(s.s));
					ret=E_UNSPEC;
					goto error;
				}
				c->label.match_re=regex;
				regex=0;
			}else if (c->type==MATCH_STR){
				c->label.match_str=s;
				s.s=0;
				s.len=0;
			}else{
				LM_CRIT("(%d,%d): wrong case type %d\n",
						c->ct_rve->fpos.s_line, c->ct_rve->fpos.s_col,
						c->type);
				ret=E_BUG;
				goto error;
			}
			c->is_default=0;
			n++; /* count only non-default cases */
			/* cleanup */
			rval_destroy(rv);
			rv=0;
			if (s.s){
				pkg_free(s.s);
				s.s=0;
				s.len=0;
			}
		}else{
			if (default_found){
				LM_ERR("more then one \"default\" label found (%d, %d)\n",
						(c->ct_rve)?c->ct_rve->fpos.s_line:0,
						(c->ct_rve)?c->ct_rve->fpos.s_col:0);
				ret=E_UNSPEC;
				goto error;
			}
			default_found=1;
			c->is_default=1;
			def_a=c->actions;
		}
		if ( c->actions && ((ret=fix_actions(c->actions))<0))
			goto error;
	}
	LM_DBG("%d cases, %d default\n", n, default_found);
	/*: handle n==0 (no case only a default:) */
	if (n==0){
		if (default_found){
			if (!rve_has_side_effects(m_rve)){
				t->type=BLOCK_T;
				rve_destroy(m_rve);
				destroy_case_stms(t->val[1].u.data);
				t->val[0].type=BLOCK_ST;
				t->val[0].u.data=def_a;
				t->val[1].type=0;
				t->val[1].u.data=0;
				LM_DBG("default only switch optimized away (BLOCK_T)\n");
				return 0;
			}
			LM_CRIT("default only switch with side-effect not expected at this point\n");
			ret=E_BUG;
			goto error;
		}else{
			LM_CRIT("empty switch not expected at this point\n");
			ret=E_BUG;
			goto error;
		}
	}
	/* n is the number of labels here */
	match=pkg_malloc(sizeof(match[0])*n);
	jmp_bm=pkg_malloc(sizeof(jmp_bm[0])*n);
	if (match==0 || jmp_bm==0){
		LM_ERR("memory allocation failure\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	
	/* fill condition table and jump point bookmarks and "flatten" the action 
	   lists (transform them into a single list for the entire switch, rather
	    then one block per case ) */
	n=0;
	action_lst=0;
	tail=&action_lst;
	for (c=(struct case_stms*)t->val[1].u.data; c; c=c->next){
		a=c->actions;
		if (a){
			for (; a->next; a=a->next);
			if (action_lst==0)
				action_lst=c->actions;
			else
				*tail=c->actions;
		}
		if (c->is_default){
			def_jmp_bm=tail;
		} else{
			match[n].type=c->type;
			if (match[n].type == MATCH_STR){
				for (j=0; j<n; j++){
					if ( match[j].type == c->type &&
						 match[j].l.s.len ==  c->label.match_str.len &&
						 memcmp(match[j].l.s.s, c->label.match_str.s,
							 		match[j].l.s.len) == 0 ){
						LM_ERR("duplicate case (%d,%d)\n",
								c->ct_rve->fpos.s_line,
								c->ct_rve->fpos.s_col);
						ret=E_UNSPEC;
						goto error;
					}
				}
				match[n].flags=0;
				match[n].l.s=c->label.match_str;
				c->label.match_str.s=0; /* prevent s being freed */
				c->label.match_str.len=0;
			} else {
				match[n].flags=c->re_flags | REG_EXTENDED | REG_NOSUB;
				match[n].l.regex=c->label.match_re;
				c->label.match_re=0;
			}
			jmp_bm[n]=tail;
			n++;
		}
		if (c->actions)
			tail=&a->next;
	}
	/* handle constant rve w/ no side-effects: replace the whole case 
	   with the case rve block */
	if ( (scr_opt_lev>=2) &&
			!rve_has_side_effects(m_rve) && rve_is_constant(m_rve)){
		if ((rv=rval_expr_eval(0, 0, m_rve)) == 0){
			LM_ERR("bad expression (%d,%d)\n", 
					m_rve->fpos.s_line, m_rve->fpos.s_col);
			ret=E_UNSPEC;
			goto error;
		}
		if (rval_get_str(0, 0, &s, rv, 0) < 0 ){
				LM_ERR("(%d,%d): bad string expression\n",
						m_rve->fpos.s_line,
						m_rve->fpos.s_col);
			ret=E_UNSPEC;
			goto error;
		}
		/* start with the "default:" value in case nothing is found */
		block=def_jmp_bm?*def_jmp_bm:0;
		for (i=0; i<n; i++){
			if (((match[i].type == MATCH_STR) && (match[i].l.s.len == s.len) &&
					(memcmp(match[i].l.s.s, s.s, s.len) == 0)) ||
				((match[i].type == MATCH_RE) && 
					regexec(match[i].l.regex, s.s, 0, 0, 0) == 0) ) {
				block=*jmp_bm[i];
				break;
			}
		}
		LM_DBG("constant switch(\"%.*s\") with %d cases optimized away"
				" to case no. %d\n", s.len, ZSW(s.s), n, i);
		/* cleanup */
		rval_destroy(rv);
		rv=0;
		pkg_free(s.s);
		s.s=0;
		s.len=0;
		ret=0;
		/* replace with BLOCK_ST */
		rve_destroy(m_rve);
		destroy_case_stms(t->val[1].u.data);
		t->type=BLOCK_T;
		t->val[0].type=BLOCK_ST;
		t->val[0].u.data=block;
		t->val[1].type=0;
		t->val[1].u.data=0;
		goto end;
	}
	mct=mk_match_cond_table(n);
	if (mct==0){
		LM_ERR("memory allocation error\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	mct->n=n;
	for (i=0; i<n; i++){
		mct->match[i]=match[i];
		mct->jump[i]=*jmp_bm[i];
	}
	mct->def=def_jmp_bm?*def_jmp_bm:0;
	t->type=MATCH_COND_T;
	t->val[1].type=MATCH_CONDTABLE_ST;
	t->val[1].u.data=mct;
	LM_DBG("optimized to match condtable (%d) default: %s\n",
				mct->n, mct->def?"yes":"no");
		ret=0;
end:
error:
	if (match) pkg_free(match);
	if (jmp_bm) pkg_free(jmp_bm);
	/* cleanup rv & s*/
	if (rv) rval_destroy(rv);
	if (s.s) pkg_free(s.s);
	return ret;
}
/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
