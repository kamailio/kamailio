/*
 * Copyright (C)  2007-2008 Voice Sistem SRL
 *
 * Copyright (C)  2008 Juha Heinanen
 *
 * Copyright (C)  2014 Olle E. Johansson, Edvina AB
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
 * \brief Kamailio dialplan :: Module interface
 * \ingroup dialplan
 * Module: \ref dialplan
 */

/*! \defgroup dialplan Kamailio dialplan transformations module
 *
 */




#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "../../core/sr_module.h"
#include "../../lib/srdb1/db.h"
#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/ut.h"
#include "../../core/action.h"
#include "../../core/pvar.h"
#include "../../core/dset.h"
#include "../../core/mod_fix.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_to.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include "dialplan.h"
#include "dp_db.h"

MODULE_VERSION

#define DEFAULT_PARAM    "$rU"

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy();

static int dialplan_init_rpc(void);

static int dp_translate_f(struct sip_msg* msg, char* str1, char* str2);
static int dp_trans_fixup(void ** param, int param_no);
static int dp_reload_f(struct sip_msg* msg);
static int w_dp_replace(sip_msg_t* msg, char* pid, char* psrc, char* pdst);
static int w_dp_match(sip_msg_t* msg, char* pid, char* psrc);

int dp_replace_fixup(void** param, int param_no);
int dp_replace_fixup_free(void** param, int param_no);

str attr_pvar_s = STR_NULL;
pv_spec_t *attr_pvar = NULL;

str default_param_s = str_init(DEFAULT_PARAM);
dp_param_p default_par2 = NULL;

int dp_fetch_rows = 1000;
int dp_match_dynamic = 0;
int dp_append_branch = 1;
int dp_reload_delta = 5;

static time_t *dp_rpc_reload_time = NULL;

static param_export_t mod_params[]={
	{ "db_url",			PARAM_STR,	&dp_db_url },
	{ "table_name",		PARAM_STR,	&dp_table_name },
	{ "dpid_col",		PARAM_STR,	&dpid_column },
	{ "pr_col",			PARAM_STR,	&pr_column },
	{ "match_op_col",	PARAM_STR,	&match_op_column },
	{ "match_exp_col",	PARAM_STR,	&match_exp_column },
	{ "match_len_col",	PARAM_STR,	&match_len_column },
	{ "subst_exp_col",	PARAM_STR,	&subst_exp_column },
	{ "repl_exp_col",	PARAM_STR,	&repl_exp_column },
	{ "attrs_col",		PARAM_STR,	&attrs_column },
	{ "attrs_pvar",	    PARAM_STR,	&attr_pvar_s },
	{ "fetch_rows",		PARAM_INT,	&dp_fetch_rows },
	{ "match_dynamic",	PARAM_INT,	&dp_match_dynamic },
	{ "append_branch",	PARAM_INT,	&dp_append_branch },
	{ "reload_delta",	PARAM_INT,	&dp_reload_delta },
	{0,0,0}
};

static cmd_export_t cmds[]={
	{"dp_translate",(cmd_function)dp_translate_f,	2,	dp_trans_fixup,  0,
		ANY_ROUTE},
	{"dp_translate",(cmd_function)dp_translate_f,	1,	dp_trans_fixup,  0,
		ANY_ROUTE},
	{"dp_reload",(cmd_function)dp_reload_f,	0, 0,  0,
		ANY_ROUTE},
	{"dp_match",(cmd_function)w_dp_match,	2,	fixup_igp_spve,
		fixup_free_igp_spve, ANY_ROUTE},
	{"dp_replace",(cmd_function)w_dp_replace,	3,	dp_replace_fixup,
		dp_replace_fixup_free, ANY_ROUTE},
	{0,0,0,0,0,0}
};

struct module_exports exports= {
	"dialplan",      /* module's name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	mod_params,      /* param exports */
	0,               /* exported RPC functions */
	0,               /* exported pseudo-variables */
	0,               /* reply processing function */
	mod_init,        /* module initialization function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};


static int mod_init(void)
{
	if(dialplan_init_rpc()!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	LM_DBG("db_url=%s/%d/%p\n", ZSW(dp_db_url.s), dp_db_url.len,dp_db_url.s);

	if(attr_pvar_s.s && attr_pvar_s.len>0) {
		attr_pvar = pv_cache_get(&attr_pvar_s);
		if( (attr_pvar==NULL) ||
				((attr_pvar->type != PVT_AVP) &&
				 (attr_pvar->type != PVT_XAVP) &&
				 (attr_pvar->type!=PVT_SCRIPTVAR))) {
			LM_ERR("invalid pvar name\n");
			return -1;
		}
	}

	default_par2 = (dp_param_p)shm_malloc(sizeof(dp_param_t));
	if(default_par2 == NULL){
		LM_ERR("no shm more memory\n");
		return -1;
	}
	memset(default_par2, 0, sizeof(dp_param_t));

	/* emulate "$rU/$rU" as second parameter for dp_translate() */
	default_param_s.len = strlen(default_param_s.s);
	default_par2->v.sp[0] = pv_cache_get(&default_param_s);
	if (default_par2->v.sp[0]==NULL) {
		LM_ERR("input pv is invalid\n");
		return -1;
	}

	default_param_s.len = strlen(default_param_s.s);
	default_par2->v.sp[1] = pv_cache_get(&default_param_s);
	if (default_par2->v.sp[1]==NULL) {
		LM_ERR("output pv is invalid\n");
		return -1;
	}

	if(dp_fetch_rows<=0)
		dp_fetch_rows = 1000;

	if(dp_reload_delta<0)
		dp_reload_delta = 5;

	if(init_data() != 0) {
		LM_ERR("could not initialize data\n");
		return -1;
	}

	dp_rpc_reload_time = shm_malloc(sizeof(time_t));
	if(dp_rpc_reload_time == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	*dp_rpc_reload_time = 0;

	return 0;
}


static int child_init(int rank)
{
	return 0;
}


static void mod_destroy(void)
{
	/*destroy shared memory*/
	if(default_par2){
		shm_free(default_par2);
		default_par2 = NULL;
	}
	if(dp_rpc_reload_time!=NULL) {
		shm_free(dp_rpc_reload_time);
		dp_rpc_reload_time = 0;
	}
	destroy_data();
}


static int dp_get_ivalue(struct sip_msg* msg, dp_param_p dp, int *val)
{
	pv_value_t value;

	if(dp->type==DP_VAL_INT) {
		*val = dp->v.id;
		LM_DBG("dpid is %d from constant argument\n", *val);
		return 0;
	}

	LM_DBG("searching %d\n",dp->v.sp[0]->type);

	if( pv_get_spec_value( msg, dp->v.sp[0], &value)!=0
			|| value.flags&(PV_VAL_NULL|PV_VAL_EMPTY) || !(value.flags&PV_VAL_INT)) {
		LM_ERR("no AVP, XAVP or SCRIPTVAR found (error in scripts)\n");
		return -1;
	}
	*val = value.ri;
	LM_DBG("dpid is %d from pv argument\n", *val);
	return 0;
}


static int dp_get_svalue(struct sip_msg * msg, pv_spec_t *spec, str* val)
{
	pv_value_t value;

	LM_DBG("searching %d \n", spec->type);

	if ( pv_get_spec_value(msg,spec,&value)!=0 || value.flags&PV_VAL_NULL
			|| value.flags&PV_VAL_EMPTY || !(value.flags&PV_VAL_STR)){
		LM_ERR("no AVP, XAVP or SCRIPTVAR found (error in scripts)\n");
		return -1;
	}

	*val = value.rs;
	return 0;
}


static int dp_update(struct sip_msg * msg, pv_spec_t * dest,
		str * repl, str * attrs)
{
	int no_change;
	pv_value_t val;

	memset(&val, 0, sizeof(pv_value_t));
	val.flags = PV_VAL_STR;

	no_change = (dest==NULL) || (dest->type == PVT_NONE)
						|| (!repl->s) || (!repl->len);

	if (no_change)
		goto set_attr_pvar;

	val.rs = *repl;

	if(dest->setf) {
		if(dest->setf(msg, &dest->pvp, (int)EQ_T, &val)<0) {
			LM_ERR("setting dst pseudo-variable failed\n");
			return -1;
		}
	} else {
		LM_WARN("target variable is read only - skipping setting its value\n");
	}

	if(dp_append_branch!=0) {
		if(is_route_type(FAILURE_ROUTE)
				&& (dest->type == PVT_RURI
						|| dest->type == PVT_RURI_USERNAME)) {
			if(append_branch(msg, 0, 0, 0, Q_UNSPECIFIED, 0, 0, 0, 0, 0, 0)
					!= 1) {
				LM_ERR("append branch action failed\n");
				return -1;
			}
		}
	}

set_attr_pvar:

	if(attr_pvar==NULL || attrs==NULL)
		return 0;

	val.rs = *attrs;
	if(attr_pvar->setf(msg, &attr_pvar->pvp, (int)EQ_T, &val)<0)
	{
		LM_ERR("setting attr pseudo-variable failed\n");
		return -1;
	}

	return 0;
}


static int dp_translate_f(struct sip_msg* msg, char* str1, char* str2)
{
	int dpid;
	str input, output;
	dpl_id_p idp;
	dp_param_p id_par, repl_par;
	str attrs, *outattrs;

	if(!msg)
		return -1;

	/*verify first param's value*/
	id_par = (dp_param_p) str1;
	if (dp_get_ivalue(msg, id_par, &dpid) != 0){
		LM_ERR("no dpid value\n");
		return -1;
	}

	if ((idp = select_dpid(dpid)) ==0 ){
		LM_DBG("no information available for dpid %i\n", dpid);
		return -2;
	}

	repl_par = (str2!=NULL)? ((dp_param_p)str2):default_par2;
	if (dp_get_svalue(msg, repl_par->v.sp[0], &input)!=0){
		LM_ERR("invalid param 2\n");
		return -1;
	}

	LM_DBG("input is %.*s\n", input.len, input.s);

	outattrs = (!attr_pvar)?NULL:&attrs;
	if (dp_translate_helper(msg, &input, &output, idp, outattrs)!=0) {
		LM_DBG("could not translate %.*s "
				"with dpid %i\n", input.len, input.s, idp->dp_id);
		return -1;
	}
	LM_DBG("input %.*s with dpid %i => output %.*s\n",
			input.len, input.s, idp->dp_id, output.len, output.s);

	/* set the output */
	if (dp_update(msg, repl_par->v.sp[1], &output, outattrs) !=0){
		LM_ERR("cannot set the output\n");
		return -1;
	}

	return 1;

}

#define verify_par_type(_par_no, _spec, _ret) \
	do{\
		if( ((_par_no == 1) \
					&& (_spec->type != PVT_AVP) && (_spec->type != PVT_XAVP) && \
					(_spec->type!=PVT_SCRIPTVAR) )\
				||((_par_no == 2) \
					&& (_spec->type != PVT_AVP) && (_spec->type != PVT_XAVP) && \
					(_spec->type!=PVT_SCRIPTVAR) \
					&& (_spec->type!=PVT_RURI) && (_spec->type!=PVT_RURI_USERNAME))){\
			\
			LM_ERR("Unsupported Parameter TYPE[%d]\n", _spec->type);\
			_ret = E_UNSPEC; \
			goto error; \
		}\
	}while(0);


/* first param: DPID: type: INT, AVP, XAVP, SVAR
 * second param: SRC type: any psedo variable type
 * second param: DST type: RURI, RURI_USERNAME, AVP, XAVP, SVAR, N/A
 * default value for the second param: $ru.user/$ru.user
 */
static int dp_trans_fixup(void ** param, int param_no){

	int dpid;
	dp_param_p dp_par= NULL;
	char *p, *s=NULL;
	str lstr;
	int ret = E_INVALID_PARAMS;

	if(param_no!=1 && param_no!=2)
		return 0;

	p = (char*)*param;
	if(!p || (*p == '\0')){
		LM_DBG("null param %i\n", param_no);
		return E_CFG;
	}

	LM_DBG("param_no is %i\n", param_no);

	dp_par = (dp_param_p)pkg_malloc(sizeof(dp_param_t));
	if(dp_par == NULL){
		LM_ERR("no more pkg memory\n");
		return E_OUT_OF_MEM;
	}
	memset(dp_par, 0, sizeof(dp_param_t));

	if(param_no == 1) {
		if(*p != '$') {
			dp_par->type = DP_VAL_INT;
			lstr.s = *param; lstr.len = strlen(*param);
			if(str2sint(&lstr, &dpid) != 0) {
				LM_ERR("bad number <%s>\n",(char *)(*param));
				ret = E_CFG;
				goto error;
			}

			dp_par->type = DP_VAL_INT;
			dp_par->v.id = dpid;
		}else{
			lstr.s = p; lstr.len = strlen(p);
			dp_par->v.sp[0] = pv_cache_get(&lstr);
			if (dp_par->v.sp[0]==NULL) {
				goto error;
			}

			verify_par_type(param_no, dp_par->v.sp[0], ret);
			dp_par->type = DP_VAL_SPEC;
		}
	} else {

		if (((s = strchr(p, '/')) != 0) && (*(s+1)=='\0'))
			goto error;

		if (s != 0) {
			*s = '\0'; s++;
		}

		lstr.s = p; lstr.len = strlen(p);
		dp_par->v.sp[0] = pv_cache_get(&lstr);
		if(dp_par->v.sp[0]==NULL) {
			goto error;
		}

		if (s != 0) {
			lstr.s = s; lstr.len = strlen(s);
			dp_par->v.sp[1] = pv_cache_get(&lstr);
			if (dp_par->v.sp[1]==NULL) {
				goto error;
			}
			verify_par_type(param_no, dp_par->v.sp[1], ret);
		}

		dp_par->type = DP_VAL_SPEC;

	}

	*param = (void *)dp_par;

	return 0;

error:
	LM_ERR("failed to parse param %i\n", param_no);
	if(dp_par) pkg_free(dp_par);

	return ret;
}

static int dp_replace_helper(sip_msg_t *msg, int dpid, str *input,
		pv_spec_t *pvd)
{
	dpl_id_p idp;
	str tmp = STR_NULL;
	str attrs = STR_NULL;
	str *output = NULL;
	str *outattrs = NULL;

	if ((idp = select_dpid(dpid)) ==0) {
		LM_DBG("no information available for dpid %i\n", dpid);
		return -2;
	}

	outattrs = (!attr_pvar)?NULL:&attrs;
	output = (!pvd)?NULL:&tmp;
	if (dp_translate_helper(msg, input, output, idp, outattrs)!=0) {
		LM_DBG("could not translate %.*s "
				"with dpid %i\n", input->len, input->s, idp->dp_id);
		return -1;
	}
	if (output) {
		LM_DBG("input %.*s with dpid %i => output %.*s\n",
				input->len, input->s, idp->dp_id, output->len, output->s);
	}

	/* set the output */
	if (dp_update(msg, pvd, output, outattrs) !=0){
		LM_ERR("cannot set the output\n");
		return -1;
	}

	return 1;
}

static int w_dp_replace(sip_msg_t* msg, char* pid, char* psrc, char* pdst)
{
	int dpid = 1;
	str src = STR_NULL;
	pv_spec_t *pvd = NULL;

	if(fixup_get_ivalue(msg, (gparam_t*)pid, &dpid)<0) {
		LM_ERR("failed to get dialplan id value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)psrc, &src)<0) {
		LM_ERR("failed to get src value\n");
		return -1;
	}
	pvd = (pv_spec_t*)pdst;

	return dp_replace_helper(msg, dpid, &src, pvd);
}

static int ki_dp_replace(sip_msg_t* msg, int dpid, str* src, str* dst)
{
	pv_spec_t *pvd = NULL;

	pvd = pv_cache_get(dst);
	if(pvd==NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", dst->len, dst->s);
		return -1;
	}

	return dp_replace_helper(msg, dpid, src, pvd);
}

static int w_dp_match(sip_msg_t* msg, char* pid, char* psrc)
{
	int dpid = 1;
	str src = STR_NULL;

	if(fixup_get_ivalue(msg, (gparam_t*)pid, &dpid)<0) {
		LM_ERR("failed to get dialplan id value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)psrc, &src)<0) {
		LM_ERR("failed to get src value\n");
		return -1;
	}

	return dp_replace_helper(msg, dpid, &src, NULL);
}

static int ki_dp_match(sip_msg_t* msg, int dpid, str* src)
{
	return dp_replace_helper(msg, dpid, src, NULL);
}

int dp_replace_fixup(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_igp_null(param, param_no);
	else if (param_no == 2)
		return fixup_spve_all(param, param_no);
	else if (param_no == 3)
		return fixup_pvar_all(param, param_no);
	return E_UNSPEC;
}


int dp_replace_fixup_free(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_free_igp_null(param, param_no);
	else if (param_no == 2)
		return fixup_free_spve_all(param, param_no);
	else if (param_no == 3)
		return fixup_free_pvar_all(param, param_no);
	return E_UNSPEC;
}

/**
 * trigger reload of dialplan db records from config file
 */
static int dp_reload_f(struct sip_msg* msg)
{
	if (dp_connect_db() < 0) {
		LM_ERR("failed to reload rules fron database (db connect)\n");
		return -1;
	}

	if(dp_load_db() != 0){
		LM_ERR("failed to reload rules fron database (db load)\n");
		dp_disconnect_db();
		return -1;
	}

	dp_disconnect_db();

	LM_DBG("reloaded dialplan\n");
	return 1;
}


static const char* dialplan_rpc_reload_doc[2] = {
	"Reload dialplan table from database",
	0
};


/*
 * RPC command to reload dialplan table
 */
static void dialplan_rpc_reload(rpc_t* rpc, void* ctx)
{
	if(dp_rpc_reload_time==NULL) {
		LM_ERR("not ready for reload\n");
		rpc->fault(ctx, 500, "Not ready for reload");
		return;
	}
	if(*dp_rpc_reload_time!=0 && *dp_rpc_reload_time > time(NULL) - dp_reload_delta) {
		LM_ERR("ongoing reload\n");
		rpc->fault(ctx, 500, "ongoing reload");
		return;
	}
	*dp_rpc_reload_time = time(NULL);
	if (dp_connect_db() < 0) {
		LM_ERR("failed to reload rules fron database (db connect)\n");
		rpc->fault(ctx, 500, "DB Connection Error");
		return;
	}

	if(dp_load_db() != 0){
		LM_ERR("failed to reload rules fron database (db load)\n");
		dp_disconnect_db();
		rpc->fault(ctx, 500, "Dialplan Reload Failed");
		return;
	}

	dp_disconnect_db();
	return;
}



static const char* dialplan_rpc_translate_doc[2] = {
	"Perform dialplan translation",
	0
};


/*
 * RPC command to perform dialplan translation
 */
static void dialplan_rpc_translate(rpc_t* rpc, void* ctx)
{
	dpl_id_p idp;
	str input;
	int dpid;
	str attrs  = {"", 0};
	str output = {0, 0};
	void* th;

	if (rpc->scan(ctx, "dS", &dpid, &input) < 2)
	{
		rpc->fault(ctx, 500, "Invalid parameters");
		return;
	}

	if ((idp = select_dpid(dpid)) == 0 ){
		LM_ERR("no information available for dpid %i\n", dpid);
		rpc->fault(ctx, 500, "Dialplan ID not matched");
		return;
	}

	if(input.s == NULL || input.len== 0)	{
		LM_ERR("empty input parameter\n");
		rpc->fault(ctx, 500, "Empty input parameter");
		return;
	}

	LM_DBG("trying to translate %.*s with dpid %i\n",
			input.len, input.s, idp->dp_id);
	if (dp_translate_helper(NULL, &input, &output, idp, &attrs)!=0){
		LM_DBG("could not translate %.*s with dpid %i\n",
				input.len, input.s, idp->dp_id);
		rpc->fault(ctx, 500, "No translation");
		return;
	}
	LM_DBG("input %.*s with dpid %i => output %.*s\n",
			input.len, input.s, idp->dp_id, output.len, output.s);

	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error creating rpc");
		return;
	}
	if(rpc->struct_add(th, "SS",
				"Output", &output,
				"Attributes", &attrs)<0)
	{
		rpc->fault(ctx, 500, "Internal error creating rpc");
		return;
	}

	return;
}

/*
 * RPC command to dump dialplan
 */
static void dialplan_rpc_dump(rpc_t* rpc, void* ctx)
{
	dpl_id_p idp;
	dpl_index_p indexp;
	dpl_node_p rulep;
	int dpid;
	void* th;
	void* ih;
	void* sh;

	if (rpc->scan(ctx, "d", &dpid) < 1)
	{
		rpc->fault(ctx, 500, "Missing parameter");
		return;
	}

	if ((idp = select_dpid(dpid)) == 0 ) {
		LM_ERR("no information available for dpid %i\n", dpid);
		rpc->fault(ctx, 500, "Dialplan ID not matched");
		return;
	}

	LM_DBG("trying to dump dpid %i\n", idp->dp_id);

	/* add entry node */
	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}

	if(rpc->struct_add(th, "d[",
				"DPID",  dpid,
				"ENTRIES", &ih)<0)
	{
		rpc->fault(ctx, 500, "Internal error sets structure");
		return;
	}

	for(indexp=idp->first_index; indexp!=NULL;indexp=indexp->next) {
		LM_DBG("INDEX LEN: %i\n", indexp->len);
                for(rulep = indexp->first_rule; rulep!= NULL;rulep = rulep->next) {
			LM_DBG("DPID: %i PRIO : %i\n", rulep->dpid, rulep->pr);
			if (rpc->struct_add(ih, "{","ENTRY", &sh) < 0)
			{
				rpc->fault(ctx, 500, "Internal error root reply");
				return;
			}

			if (rpc->struct_add(sh, "dd", "PRIO", rulep->pr,
				"MATCHOP", rulep->matchop)<0)
			{
				rpc->fault(ctx, 500, "Internal error adding prio");
				return;
			}
			if (rpc->struct_add(sh, "s", "MATCHEXP", rulep->match_exp) < 0 )
			{
				rpc->fault(ctx, 500, "Internal error adding match exp");
				return;
			}
			if (rpc->struct_add(sh, "d", "MATCHLEN", rulep->matchlen) < 0 )
			{
				rpc->fault(ctx, 500, "Internal error adding expression data and attribute");
				return;
			}
			if (rpc->struct_add(sh, "s", "SUBSTEXP", rulep->subst_exp) < 0 )
			{
				rpc->fault(ctx, 500, "Internal error adding subst exp");
				return;
			}
			if (rpc->struct_add(sh, "s", "REPLEXP", rulep->repl_exp) < 0 )
			{
				rpc->fault(ctx, 500, "Internal error adding replace exp ");
				return;
			}
			if (rpc->struct_add(sh, "s", "ATTRS", rulep->attrs) < 0 )
			{
				rpc->fault(ctx, 500, "Internal error adding attribute");
				return;
			}
		}
	}

	return;
}

static const char* dialplan_rpc_dump_doc[2] = {
	"Dump dialplan content",
	0
};


rpc_export_t dialplan_rpc_list[] = {
	{"dialplan.reload", dialplan_rpc_reload,
		dialplan_rpc_reload_doc, 0},
	{"dialplan.translate",   dialplan_rpc_translate,
		dialplan_rpc_translate_doc, 0},
	{"dialplan.dump",   dialplan_rpc_dump,
		dialplan_rpc_dump_doc, 0},
	{0, 0, 0, 0}
};

static int dialplan_init_rpc(void)
{
	if (rpc_register_array(dialplan_rpc_list)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_dialplan_exports[] = {
	{ str_init("dialplan"), str_init("dp_match"),
		SR_KEMIP_INT, ki_dp_match,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dialplan"), str_init("dp_replace"),
		SR_KEMIP_INT, ki_dp_replace,
		{ SR_KEMIP_INT, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_dialplan_exports);
	return 0;
}
