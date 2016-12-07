/*
 * Copyright (C)  2007-2008 Voice Sistem SRL
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

/*!
 * \file
 * \brief Kamailio dialplan :: database interface
 * \ingroup dialplan
 * Module: \ref dialplan
 */


#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../lib/srdb1/db.h"
#include "../../re.h"
#include "dp_db.h"
#include "dialplan.h"

str dp_db_url       =   str_init(DEFAULT_RODB_URL);
str dp_table_name   =   str_init(DP_TABLE_NAME);
str dpid_column     =   str_init(DPID_COL);
str pr_column       =   str_init(PR_COL);
str match_op_column =   str_init(MATCH_OP_COL);
str match_exp_column=   str_init(MATCH_EXP_COL);
str match_len_column=   str_init(MATCH_LEN_COL);
str subst_exp_column=   str_init(SUBST_EXP_COL);
str repl_exp_column =   str_init(REPL_EXP_COL);
str attrs_column    =   str_init(ATTRS_COL); 

extern int dp_fetch_rows;
extern int dp_match_dynamic;

static db1_con_t* dp_db_handle    = 0; /* database connection handle */
static db_func_t dp_dbf;

#define GET_STR_VALUE(_res, _values, _index)\
	do{\
		if ( VAL_NULL((_values)+ (_index)) ) { \
			LM_ERR(" values %d is NULL - not allowed\n",_index);\
			(_res).s = 0; (_res).len = 0;\
			goto err;\
		} \
		(_res).s = VAL_STR((_values)+ (_index)).s;\
		(_res).len = strlen(VAL_STR((_values)+ (_index)).s);\
	}while(0);

void destroy_rule(dpl_node_t * rule);
void destroy_hash(int);

dpl_node_t * build_rule(db_val_t * values);
int add_rule2hash(dpl_node_t *, int);

void list_rule(dpl_node_t * );
void list_hash(int h_index);


dpl_id_p* rules_hash = NULL;
int * crt_idx, *next_idx;


/**
 * check if string has pvs
 * returns -1 if error, 0 if found, 1 if not found
 */
int dpl_check_pv(str *in)
{
	char *p;
	pv_spec_t *spec = NULL;
	str s;
	int len;

	if(in==NULL || in->s==NULL)
		return -1;

	LM_DBG("parsing [%.*s]\n", in->len, in->s);

	if(in->len == 0)
		return 1;

	p = in->s;

	while(is_in_str(p,in))
	{
		while(is_in_str(p,in) && *p!=PV_MARKER)
			p++;
		if(*p == '\0' || !is_in_str(p,in))
			break;
		/* last char is $ ? */
		if(!is_in_str(p+1, in))
			break;
		s.s = p;
		s.len = in->s+in->len-p;
		len = 0;
		spec = pv_spec_lookup(&s, &len);
		if(spec!=NULL) {
			/* found a variable */
			LM_DBG("string [%.*s] has variables\n", in->len, in->s);
			return 0;
		}
		if(len) p += len;
		else p++;
	}

	/* not found */
	return 1;
}

int init_db_data(void)
{
	if(!dp_table_name.s || dp_table_name.len<=0){
		LM_ERR("invalid database table name\n");
		return -1;
	}

	/* Find a database module */
	if (db_bind_mod(&dp_db_url, &dp_dbf) < 0){
		LM_ERR("unable to bind to a database driver\n");
		return -1;
	}

	if(dp_connect_db() !=0)
		return -1;

	if(db_check_table_version(&dp_dbf, dp_db_handle, &dp_table_name,
				DP_TABLE_VERSION) < 0) {
		LM_ERR("error during table version check.\n");
		goto error;
	}

	if(dp_load_db() != 0){
		LM_ERR("failed to load database data\n");
		goto error;
	}

	dp_disconnect_db();

	return 0;
error:

	dp_disconnect_db();
	return -1;
}


int dp_connect_db(void)
{
	if (dp_dbf.init==0){
		LM_CRIT("null dp_dbf\n");
		return -1;
	}

	if(dp_db_handle){
		LM_CRIT("BUG: connection to database already open\n");
		return -1;
	}

	if ((dp_db_handle = dp_dbf.init(&dp_db_url)) == 0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	return 0;
}


void dp_disconnect_db(void)
{
	if(dp_db_handle){
		dp_dbf.close(dp_db_handle);
		dp_db_handle = 0;
	}
}


int init_data(void)
{
	int *p;

	rules_hash = (dpl_id_p *)shm_malloc(2*sizeof(dpl_id_p));
	if(!rules_hash) {
		LM_ERR("out of shm memory\n");
		return -1;
	}
	rules_hash[0] = rules_hash[1] = 0;

	p = (int *)shm_malloc(2*sizeof(int));
	if(!p){
		LM_ERR("out of shm memory\n");
		return -1;
	}
	crt_idx = p;
	next_idx = p+1;
	*crt_idx = *next_idx = 0;

	LM_DBG("trying to initialize data from db\n");
	if(init_db_data() != 0)
		return -1;

	return 0;
}


void destroy_data(void)
{
	if(rules_hash){
		destroy_hash(0);
		destroy_hash(1);
		shm_free(rules_hash);
		rules_hash = 0;
	}

	if(crt_idx)
		shm_free(crt_idx);
}


/*load rules from DB*/
int dp_load_db(void)
{
	int i, nr_rows;
	db1_res_t * res = 0;
	db_val_t * values;
	db_row_t * rows;
	db_key_t query_cols[DP_TABLE_COL_NO] = {
		&dpid_column,	&pr_column,
		&match_op_column,	&match_exp_column,	&match_len_column,
		&subst_exp_column,	&repl_exp_column,	&attrs_column };

	db_key_t order = &pr_column;

	dpl_node_t *rule;

	LM_DBG("init\n");
	if( (*crt_idx) != (*next_idx)){
		LM_WARN("a load command already generated, aborting reload...\n");
		return 0;
	}

	if (dp_dbf.use_table(dp_db_handle, &dp_table_name) < 0){
		LM_ERR("error in use_table %.*s\n", dp_table_name.len, dp_table_name.s);
		return -1;
	}

	if (DB_CAPABILITY(dp_dbf, DB_CAP_FETCH)) {
		if(dp_dbf.query(dp_db_handle,0,0,0,query_cols, 0, 
					DP_TABLE_COL_NO, order, 0) < 0){
			LM_ERR("failed to query database!\n");
			return -1;
		}
		if(dp_dbf.fetch_result(dp_db_handle, &res, dp_fetch_rows)<0) {
			LM_ERR("failed to fetch\n");
			if (res)
				dp_dbf.free_result(dp_db_handle, res);
			return -1;
		}
	} else {
		/*select the whole table and all the columns*/
		if(dp_dbf.query(dp_db_handle,0,0,0,query_cols, 0, 
					DP_TABLE_COL_NO, order, &res) < 0){
			LM_ERR("failed to query database\n");
			return -1;
		}
	}

	nr_rows = RES_ROW_N(res);

	*next_idx = ((*crt_idx) == 0)? 1:0;
	destroy_hash(*next_idx);

	if(nr_rows == 0){
		LM_WARN("no data in the db\n");
		goto end;
	}

	do {
		for(i=0; i<RES_ROW_N(res); i++){
			rows 	= RES_ROWS(res);

			values = ROW_VALUES(rows+i);

			if((rule = build_rule(values)) ==0 )
				goto err2;

			if(add_rule2hash(rule , *next_idx) != 0)
				goto err2;

		}
		if (DB_CAPABILITY(dp_dbf, DB_CAP_FETCH)) {
			if(dp_dbf.fetch_result(dp_db_handle, &res, dp_fetch_rows)<0) {
				LM_ERR("failure while fetching!\n");
				if (res)
					dp_dbf.free_result(dp_db_handle, res);
				return -1;
			}
		} else {
			break;
		}
	}  while(RES_ROW_N(res)>0);


end:
	/*update data*/
	*crt_idx = *next_idx;
	list_hash(*crt_idx);
	dp_dbf.free_result(dp_db_handle, res);
	return 0;

err2:
	if(rule)	destroy_rule(rule);
	destroy_hash(*next_idx);
	dp_dbf.free_result(dp_db_handle, res);
	*next_idx = *crt_idx; 
	return -1;
}


int dpl_str_to_shm(str src, str *dest, int mterm)
{
	int mdup = 0;

	if(src.len ==0 || src.s ==0)
		return 0;

	if(mterm!=0 && PV_MARKER=='$') {
		if(src.len>1 && src.s[src.len-1]=='$' && src.s[src.len-2]!='$') {
			mdup = 1;
		}
	}
	dest->s = (char*)shm_malloc((src.len+1+mdup) * sizeof(char));
	if(!dest->s){
		LM_ERR("out of shm memory\n");
		return -1;
	}

	memcpy(dest->s, src.s, src.len);
	dest->s[src.len] = '\0';
	dest->len = src.len;
	if(mdup) {
		dest->s[dest->len] = '$';
		dest->len++;
		dest->s[dest->len] = '\0';
	}

	return 0;
}


/* Compile pcre pattern
 * if mtype==0 - return pointer to shm copy of result
 * if mtype==1 - return pcre pointer that has to be pcre_free() */
pcre *reg_ex_comp(const char *pattern, int *cap_cnt, int mtype)
{
	pcre *re, *result;
	const char *error;
	int rc, err_offset;
	size_t size;

	re = pcre_compile(pattern, 0, &error, &err_offset, NULL);
	if (re == NULL) {
		LM_ERR("PCRE compilation of '%s' failed at offset %d: %s\n",
				pattern, err_offset, error);
		return (pcre *)0;
	}
	rc = pcre_fullinfo(re, NULL, PCRE_INFO_SIZE, &size);
	if (rc != 0) {
		pcre_free(re);
		LM_ERR("pcre_fullinfo on compiled pattern '%s' yielded error: %d\n",
				pattern, rc);
		return (pcre *)0;
	}
	rc = pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, cap_cnt);
	if (rc != 0) {
		pcre_free(re);
		LM_ERR("pcre_fullinfo on compiled pattern '%s' yielded error: %d\n",
				pattern, rc);
		return (pcre *)0;
	}
	if(mtype==0) {
		result = (pcre *)shm_malloc(size);
		if (result == NULL) {
			pcre_free(re);
			LM_ERR("not enough shared memory for compiled PCRE pattern\n");
			return (pcre *)0;
		}
		memcpy(result, re, size);
		pcre_free(re);
		return result;
	} else {
		return re;
	}
}


/*compile the expressions, and if ok, build the rule */
dpl_node_t * build_rule(db_val_t * values)
{
	pcre *match_comp, *subst_comp;
	struct subst_expr *repl_comp;
	dpl_node_t * new_rule;
	str match_exp, subst_exp, repl_exp, attrs;
	int matchop;
	int cap_cnt=0;
	unsigned int tflags=0;

	matchop = VAL_INT(values+2);

	if((matchop != DP_REGEX_OP) && (matchop!=DP_EQUAL_OP)
			&& (matchop!=DP_FNMATCH_OP)){
		LM_ERR("invalid value for match operator\n");
		return NULL;
	}

	match_comp = subst_comp = 0;
	repl_comp = 0;
	new_rule = 0;

	GET_STR_VALUE(match_exp, values, 3);
	if(matchop == DP_REGEX_OP){
		if(unlikely(dp_match_dynamic==1)) {
			if(dpl_check_pv(&match_exp)==0) {
				tflags |= DP_TFLAGS_PV_MATCH;
			}
		}
		if(!(tflags&DP_TFLAGS_PV_MATCH)) {
			match_comp = reg_ex_comp(match_exp.s, &cap_cnt, 0);
			if(!match_comp){
				LM_ERR("failed to compile match expression %.*s\n",
						match_exp.len, match_exp.s);
				goto err;
			}
		}
	}

	GET_STR_VALUE(repl_exp, values, 6);
	if(repl_exp.len && repl_exp.s){
		repl_comp = repl_exp_parse(repl_exp);
		if(!repl_comp){
			LM_ERR("failed to compile replacing expression %.*s\n",
					repl_exp.len, repl_exp.s);
			goto err;
		}
	}

	cap_cnt = 0;
	GET_STR_VALUE(subst_exp, values, 5);
	if(subst_exp.s && subst_exp.len){
		if(unlikely(dp_match_dynamic==1)) {
			if(dpl_check_pv(&subst_exp)==0) {
				tflags |= DP_TFLAGS_PV_SUBST;
			}
		}
		if(!(tflags&DP_TFLAGS_PV_SUBST)) {
			subst_comp = reg_ex_comp(subst_exp.s, &cap_cnt, 0);
			if(!subst_comp){
				LM_ERR("failed to compile subst expression %.*s\n",
						subst_exp.len, subst_exp.s);
				goto err;
			}
			if (cap_cnt > MAX_REPLACE_WITH) {
				LM_ERR("subst expression %.*s has too many sub-expressions\n",
						subst_exp.len, subst_exp.s);
				goto err;
			}
		}
	}

	LM_DBG("building rule for [%d:%.*s/%.*s/%.*s]\n", matchop,
			match_exp.len, ZSW(match_exp.s), subst_exp.len, ZSW(subst_exp.s),
			repl_exp.len, ZSW(repl_exp.s));
	if (!(tflags&(DP_TFLAGS_PV_SUBST|DP_TFLAGS_PV_MATCH)) &&
		repl_comp && (cap_cnt < repl_comp->max_pmatch) &&
		(repl_comp->max_pmatch != 0))
	{
		LM_ERR("repl_exp %.*s refers to %d sub-expressions, but "
				"subst_exp %.*s has only %d\n",
				repl_exp.len, repl_exp.s, repl_comp->max_pmatch,
				subst_exp.len, subst_exp.s, cap_cnt);
		goto err;
	}

	new_rule = (dpl_node_t *)shm_malloc(sizeof(dpl_node_t));
	if(!new_rule){
		LM_ERR("out of shm memory(new_rule)\n");
		goto err;
	}
	memset(new_rule, 0, sizeof(dpl_node_t));

	if(dpl_str_to_shm(match_exp, &new_rule->match_exp,
				tflags&DP_TFLAGS_PV_MATCH)!=0)
		goto err;

	if(dpl_str_to_shm(subst_exp, &new_rule->subst_exp,
				tflags&DP_TFLAGS_PV_SUBST)!=0)
		goto err;

	if(dpl_str_to_shm(repl_exp, &new_rule->repl_exp, 0)!=0)
		goto err;

	/*set the rest of the rule fields*/
	new_rule->dpid		=	VAL_INT(values);
	new_rule->pr		=	VAL_INT(values+1);
	new_rule->matchlen	= 	VAL_INT(values+4);
	new_rule->matchop	=	matchop;
	GET_STR_VALUE(attrs, values, 7);
	if(dpl_str_to_shm(attrs, &new_rule->attrs, 0)!=0)
		goto err;

	LM_DBG("attrs are: '%.*s'\n", new_rule->attrs.len, new_rule->attrs.s);

	new_rule->match_comp = match_comp;
	new_rule->subst_comp = subst_comp;
	new_rule->repl_comp  = repl_comp;
	new_rule->tflags     = tflags;

	return new_rule;

err:
	if(match_comp) shm_free(match_comp);
	if(subst_comp) shm_free(subst_comp);
	if(repl_comp) repl_expr_free(repl_comp);
	if(new_rule) destroy_rule(new_rule);
	return NULL;
}


int add_rule2hash(dpl_node_t * rule, int h_index)
{
	dpl_id_p crt_idp, last_idp;
	dpl_index_p indexp, last_indexp, new_indexp;
	int new_id;

	if(!rules_hash){
		LM_ERR("data not allocated\n");
		return -1;
	}

	new_id = 0;

	/*search for the corresponding dpl_id*/
	for(crt_idp = last_idp =rules_hash[h_index]; crt_idp!= NULL; 
			last_idp = crt_idp, crt_idp = crt_idp->next)
		if(crt_idp->dp_id == rule->dpid)
			break;

	/*didn't find a dpl_id*/
	if(!crt_idp){
		crt_idp = (dpl_id_t*)shm_malloc(sizeof(dpl_id_t));
		if(!crt_idp){
			LM_ERR("out of shm memory (crt_idp)\n");
			return -1;
		}
		memset(crt_idp, 0, sizeof(dpl_id_t));
		crt_idp->dp_id = rule->dpid;
		new_id = 1;
		LM_DBG("new dpl_id %i\n", rule->dpid);
	}

	/*search for the corresponding dpl_index*/
	for(indexp = last_indexp =crt_idp->first_index; indexp!=NULL; 
			last_indexp = indexp, indexp = indexp->next){
		if(indexp->len == rule->matchlen)
			goto add_rule;
		if((rule->matchlen!=0)&&((indexp->len)?(indexp->len>rule->matchlen):1))
			goto add_index;
	}

add_index:
	LM_DBG("new index , len %i\n", rule->matchlen);

	new_indexp = (dpl_index_t *)shm_malloc(sizeof(dpl_index_t));
	if(!new_indexp){
		LM_ERR("out of shm memory\n");
		goto err;
	}
	memset(new_indexp , 0, sizeof(dpl_index_t));
	new_indexp->next = indexp;
	new_indexp->len = rule->matchlen;

	/*add as first index*/
	if(last_indexp == indexp){
		crt_idp->first_index = new_indexp;
	}else{
		last_indexp->next = new_indexp;
	}

	indexp = new_indexp;

add_rule:
	rule->next = 0;
	if(!indexp->first_rule)
		indexp->first_rule = rule;

	if(indexp->last_rule)
		indexp->last_rule->next = rule;

	indexp->last_rule = rule;

	if(new_id){
		crt_idp->next = rules_hash[h_index];
		rules_hash[h_index] = crt_idp;
	}
	LM_DBG("added the rule id %i index %i pr %i next %p to the "
			"index with %i len\n", rule->dpid, rule->matchlen,
			rule->pr, rule->next, indexp->len);

	return 0;

err:
	if(new_id)
		shm_free(crt_idp);
	return -1;
}


void destroy_hash(int index)
{
	dpl_id_p crt_idp;
	dpl_index_p indexp;
	dpl_node_p rulep;

	if(!rules_hash[index])
		return;

	for(crt_idp = rules_hash[index]; crt_idp != NULL;){

		for(indexp = crt_idp->first_index; indexp != NULL;){

			for(rulep = indexp->first_rule; rulep!= NULL;){

				destroy_rule(rulep);

				indexp->first_rule = rulep->next;
				shm_free(rulep);
				rulep=0;
				rulep= indexp->first_rule;
			}
			crt_idp->first_index= indexp->next;
			shm_free(indexp);
			indexp=0;
			indexp = crt_idp->first_index;

		}

		rules_hash[index] = crt_idp->next;
		shm_free(crt_idp);
		crt_idp = 0;
		crt_idp = rules_hash[index];
	}

	rules_hash[index] = 0;
}


void destroy_rule(dpl_node_t * rule){

	if(!rule)
		return;

	LM_DBG("destroying rule with priority %i\n", 
			rule->pr);

	if(rule->match_comp)
		shm_free(rule->match_comp);

	if(rule->subst_comp)
		shm_free(rule->subst_comp);

	/*destroy repl_exp*/
	if(rule->repl_comp)
		repl_expr_free(rule->repl_comp);

	if(rule->match_exp.s)
		shm_free(rule->match_exp.s);

	if(rule->subst_exp.s)
		shm_free(rule->subst_exp.s);

	if(rule->repl_exp.s)
		shm_free(rule->repl_exp.s);

	if(rule->attrs.s)
		shm_free(rule->attrs.s);
}


dpl_id_p select_dpid(int id)
{
	dpl_id_p idp;

	if(!rules_hash || !crt_idx)
		return NULL;

	for(idp = rules_hash[*crt_idx]; idp!=NULL; idp = idp->next)
		if(idp->dp_id == id)
			return idp;

	return NULL;
}


/*FOR DEBUG PURPOSE*/
void list_hash(int h_index)
{
	dpl_id_p crt_idp;
	dpl_index_p indexp;
	dpl_node_p rulep;


	if(!rules_hash[h_index])
		return;

	for(crt_idp=rules_hash[h_index]; crt_idp!=NULL; crt_idp = crt_idp->next){
		LM_DBG("DPID: %i, pointer %p\n", crt_idp->dp_id, crt_idp);
		for(indexp=crt_idp->first_index; indexp!=NULL;indexp= indexp->next){
			LM_DBG("INDEX LEN: %i\n", indexp->len);
			for(rulep = indexp->first_rule; rulep!= NULL;rulep = rulep->next){
				list_rule(rulep);
			}
		}
	}
}


void list_rule(dpl_node_t *rule)
{
	LM_DBG("RULE %p: pr %i next %p op %d tflags %u match_exp %.*s, "
			"subst_exp %.*s, repl_exp %.*s and attrs %.*s\n", rule,
			rule->pr, rule->next,
			rule->matchop, rule->tflags,
			rule->match_exp.len, ZSW(rule->match_exp.s),
			rule->subst_exp.len, ZSW(rule->subst_exp.s),
			rule->repl_exp.len, ZSW(rule->repl_exp.s),
			rule->attrs.len,	ZSW(rule->attrs.s));

}
