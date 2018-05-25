/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/pvar.h"
#include "../../core/sr_module.h"
#include "../../core/mem/shm.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include <squirrel.h>
#include <sqstdblob.h>
#include <sqstdsystem.h>
#include <sqstdio.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdaux.h>

#include "app_sqlang_kemi_export.h"
#include "app_sqlang_api.h"

#define SRSQLANG_FALSE 0
#define SRSQLANG_TRUE 1

void sqlang_sr_kemi_register_libs(HSQUIRRELVM J);

typedef struct _sr_sqlang_env
{
	HSQUIRRELVM J;
	int J_exit;
	HSQUIRRELVM JJ;
	int JJ_exit;
	sip_msg_t *msg;
	unsigned int flags;
	unsigned int nload; /* number of scripts loaded */
} sr_sqlang_env_t;

static sr_sqlang_env_t _sr_J_env = {0};

str _sr_sqlang_load_file = STR_NULL;

static int *_sr_sqlang_reload_version = NULL;
static int _sr_sqlang_local_version = 0;

/**
 *
 */
sr_sqlang_env_t *sqlang_sr_env_get(void)
{
	return &_sr_J_env;
}

/**
 *
 */
int sqlang_sr_initialized(void)
{
	if(_sr_J_env.J==NULL)
		return 0;

	return 1;
}

/**
 *
 */
#define SQLANG_SR_EXIT_THROW_STR "~~ksr~exit~~"
#define SQLANG_SR_EXIT_EXEC_STR "throw '" SQLANG_SR_EXIT_THROW_STR "';"

static str _sr_kemi_sqlang_exit_string = str_init(SQLANG_SR_EXIT_THROW_STR);

/**
 *
 */
str* sr_kemi_sqlang_exit_string_get(void)
{
	return &_sr_kemi_sqlang_exit_string;
}

/**
 *
 */
int app_sqlang_return_int(HSQUIRRELVM J, int v)
{
	sq_pushinteger(J, v);
	return 1;
}

/**
 *
 */
int app_sqlang_return_error(HSQUIRRELVM J)
{
	sq_pushinteger(J, -1);
	return 1;
}

/**
 *
 */
int app_sqlang_return_boolean(HSQUIRRELVM J, int b)
{
	if(b==SRSQLANG_FALSE)
		sq_pushbool(J, SRSQLANG_FALSE);
	else
		sq_pushbool(J, SRSQLANG_TRUE);
	return 1;
}

/**
 *
 */
int app_sqlang_return_false(HSQUIRRELVM J)
{
	sq_pushbool(J, SRSQLANG_FALSE);
	return 1;
}

/**
 *
 */
int app_sqlang_return_true(HSQUIRRELVM J)
{
	sq_pushbool(J, SRSQLANG_TRUE);
	return 1;
}

/**
 *
 */
int sr_kemi_sqlang_return_int(HSQUIRRELVM J, sr_kemi_t *ket, int rc)
{
	if(ket->rtype==SR_KEMIP_INT) {
		sq_pushinteger(J, rc);
		return 1;
	}
	if(ket->rtype==SR_KEMIP_BOOL && rc!=SR_KEMI_FALSE) {
		return app_sqlang_return_true(J);
	}
	return app_sqlang_return_false(J);
}

static char* sqlang_to_string(HSQUIRRELVM J, int idx)
{
    const SQChar *s = NULL;
	if(idx>=0) {
		sq_getstring(J, idx+2, &s);
	} else {
		sq_getstring(J, idx, &s);
	}
	return (char*)s;
}

static int sqlang_to_int(HSQUIRRELVM J, int idx)
{
    SQInteger i = 0;

	if(idx>=0) {
		sq_getinteger(J, idx+2, &i);
	} else {
		sq_getinteger(J, idx, &i);
	}
	return (int)i;
}

static int sqlang_gettop(HSQUIRRELVM J)
{
	return (sq_gettop(J) - 1);
}

static void sqlang_pushstring(HSQUIRRELVM J, char *s)
{
	if(s==NULL) {
		sq_pushnull(J);
		return;
	}
	sq_pushstring(J, (const SQChar*)s, (SQInteger)strlen(s));
}

static void sqlang_pushlstring(HSQUIRRELVM J, char *s, int l)
{
	if(s==NULL) {
		sq_pushnull(J);
		return;
	}
	sq_pushstring(J, (const SQChar*)s, (SQInteger)l);
}

static int sqlang_isnumber(HSQUIRRELVM J, int idx)
{
	if(idx>=0) {
		idx += 2;
	}

	if(sq_gettype(J, idx)==OT_INTEGER)
		return 1;
	return 0;
}

static int sqlang_isstring(HSQUIRRELVM J, int idx)
{
	if(idx>=0) {
		idx += 2;
	}

	if(sq_gettype(J, idx)==OT_STRING)
		return 1;
	return 0;
}

static int sqlang_isfunction(HSQUIRRELVM J, int idx)
{
	if(idx>=0) {
		idx += 2;
	}

	switch(sq_gettype(J, idx)) {
		case OT_CLOSURE:
        case OT_NATIVECLOSURE:
			return 1;
		default:
			return 0;
	}
}

#if 0
static char* sqlang_safe_tostring(HSQUIRRELVM J, int idx)
{
	const SQChar *s = NULL;

	if(idx>=0) {
		idx += 2;
	}
	if(sqlang_isstring(J, idx)) {
		sq_getstring(J, idx, &s);
	}
	return (s)?(char*)s:"Error on sqlang";
}
#endif

static int sqlang_gettype(HSQUIRRELVM J, int idx)
{
	if(idx>=0) {
		idx += 2;
	}

	return (int)sq_gettype(J, idx);
}

/**
 *
 */
static SQInteger sqlang_sr_get_str_null(HSQUIRRELVM J, int rmode)
{
	if(rmode) {
		sqlang_pushlstring(J, "<<null>>", 8);
		return 1;
	} else {
		return 0;
	}
}

/**
 *
 */
static SQInteger sqlang_sr_pv_get_mode(HSQUIRRELVM J, int rmode)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_sqlang_env_t *env_J;
	int pl;

	env_J = sqlang_sr_env_get();

	pvn.s = (char*)sqlang_to_string(J, 0);
	if(pvn.s==NULL || env_J->msg==NULL)
		return sqlang_sr_get_str_null(J, rmode);

	pvn.len = strlen(pvn.s);
	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return sqlang_sr_get_str_null(J, rmode);
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return sqlang_sr_get_str_null(J, rmode);
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_J->msg, pvs, &val) != 0) {
		LM_ERR("unable to get pv value for [%s]\n", pvn.s);
		return sqlang_sr_get_str_null(J, rmode);
	}
	if(val.flags&PV_VAL_NULL) {
		sqlang_pushstring(J, NULL);
		return 1;
	}
	if(val.flags&PV_TYPE_INT) {
		sq_pushinteger(J, (SQInteger)val.ri);
		return 1;
	}
	sqlang_pushlstring(J, val.rs.s, val.rs.len);
	return 1;
}

/**
 *
 */
static SQInteger sqlang_sr_pv_get(HSQUIRRELVM J)
{
	return sqlang_sr_pv_get_mode(J, 0);
}

/**
 *
 */
static SQInteger sqlang_sr_pv_getw(HSQUIRRELVM J)
{
	return sqlang_sr_pv_get_mode(J, 1);
}

/**
 *
 */
static SQInteger sqlang_sr_pv_seti (HSQUIRRELVM J)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_sqlang_env_t *env_J;
	int pl;

	env_J = sqlang_sr_env_get();

	if(sqlang_gettop(J)<2) {
		LM_ERR("too few parameters [%d]\n", sqlang_gettop(J));
		return 0;
	}
	if(!sqlang_isnumber(J, 1)) {
		LM_ERR("invalid int parameter\n");
		return 0;
	}

	pvn.s = (char*)sqlang_to_string(J, 0);
	if(pvn.s==NULL || env_J->msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return 0;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return 0;
	}

	memset(&val, 0, sizeof(pv_value_t));
	val.ri = sqlang_to_int(J, 1);
	val.flags |= PV_TYPE_INT|PV_VAL_INT;

	if(pv_set_spec_value(env_J->msg, pvs, 0, &val)<0) {
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

/**
 *
 */
static SQInteger sqlang_sr_pv_sets (HSQUIRRELVM J)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_sqlang_env_t *env_J;
	int pl;

	env_J = sqlang_sr_env_get();

	if(sqlang_gettop(J)<2) {
		LM_ERR("too few parameters [%d]\n", sqlang_gettop(J));
		return 0;
	}

	if(!sqlang_isstring(J, 1)) {
		LM_ERR("invalid str parameter\n");
		return 0;
	}

	pvn.s = (char*)sqlang_to_string(J, 0);
	if(pvn.s==NULL || env_J->msg==NULL)
		return 0;

	memset(&val, 0, sizeof(pv_value_t));
	val.rs.s = (char*)sqlang_to_string(J, 1);
	val.rs.len = strlen(val.rs.s);
	val.flags |= PV_VAL_STR;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return 0;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return 0;
	}
	if(pv_set_spec_value(env_J->msg, pvs, 0, &val)<0) {
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

/**
 *
 */
static SQInteger sqlang_sr_pv_unset (HSQUIRRELVM J)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_sqlang_env_t *env_J;
	int pl;

	env_J = sqlang_sr_env_get();

	pvn.s = (char*)sqlang_to_string(J, 0);
	if(pvn.s==NULL || env_J->msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv unset: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return 0;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL)
	{
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_NULL;
	if(pv_set_spec_value(env_J->msg, pvs, 0, &val)<0)
	{
		LM_ERR("unable to unset pv [%s]\n", pvn.s);
		return 0;
	}

	return 0;
}

/**
 *
 */
static SQInteger sqlang_sr_pv_is_null (HSQUIRRELVM J)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	sr_sqlang_env_t *env_J;
	int pl;

	env_J = sqlang_sr_env_get();

	pvn.s = (char*)sqlang_to_string(J, 0);
	if(pvn.s==NULL || env_J->msg==NULL)
		return 0;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv is null test: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return 0;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return 0;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_J->msg, pvs, &val) != 0) {
		LM_NOTICE("unable to get pv value for [%s]\n", pvn.s);
		sq_pushbool(J, 1);
		return 1;
	}
	if(val.flags&PV_VAL_NULL) {
		sq_pushbool(J, 1);
	} else {
		sq_pushbool(J, 0);
	}
	return 1;
}


const SQRegFunction _sr_kemi_pv_J_Map[] = {
	{ "get", sqlang_sr_pv_get, 2 /* 1 args */, NULL },
	{ "getw", sqlang_sr_pv_getw, 2 /* 1 args */, NULL },
	{ "seti", sqlang_sr_pv_seti, 3 /* 2 args */, NULL },
	{ "sets", sqlang_sr_pv_sets, 4 /* 2 args */, NULL },
	{ "unset", sqlang_sr_pv_unset, 2 /* 1 args */, NULL },
	{ "is_null", sqlang_sr_pv_is_null, 2 /* 1 args */, NULL },
	{ NULL, NULL, 0 }
};

/**
 *
 */
static SQInteger sqlang_sr_exit (HSQUIRRELVM J)
{
	if(_sr_J_env.JJ==J) {
		_sr_J_env.JJ_exit = 1;
	} else {
		_sr_J_env.J_exit = 1;
	}
	return sq_throwerror(J, _SC("~~ksr~exit~~"));
}

/**
 *
 */
static SQInteger sqlang_sr_drop (HSQUIRRELVM J)
{
	if(_sr_J_env.JJ==J) {
		_sr_J_env.JJ_exit = 1;
	} else {
		_sr_J_env.J_exit = 1;
	}
	sr_kemi_core_drop(NULL);
	return sq_throwerror(J, _SC("~~ksr~exit~~"));
}


/**
 *
 */
static SQInteger sqlang_sr_modf (HSQUIRRELVM J)
{
	int ret;
	char *sqlangv[MAX_ACTIONS];
	char *argv[MAX_ACTIONS];
	int argc;
	int i;
	int mod_type;
	struct run_act_ctx ra_ctx;
	unsigned modver;
	struct action *act;
	sr31_cmd_export_t* expf;
	sr_sqlang_env_t *env_J;

	ret = 1;
	act = NULL;
	argc = 0;
	memset(sqlangv, 0, MAX_ACTIONS*sizeof(char*));
	memset(argv, 0, MAX_ACTIONS*sizeof(char*));
	env_J = sqlang_sr_env_get();
	if(env_J->msg==NULL)
		goto error;

	argc = sqlang_gettop(J);
	if(argc==0) {
		LM_ERR("name of module function not provided\n");
		goto error;
	}
	if(argc>=MAX_ACTIONS) {
		LM_ERR("too many parameters\n");
		goto error;
	}
	/* first is function name, then parameters */
	for(i=0; i<argc; i++) {
		if (!sqlang_isstring(J, i)) {
			LM_ERR("invalid parameter type (%d)\n", i);
			goto error;
		}
		sqlangv[i] = (char*)sqlang_to_string(J, i);
	}
	LM_ERR("request to execute cfg function '%s'\n", sqlangv[0]);
	/* pkg copy only parameters */
	for(i=1; i<MAX_ACTIONS; i++) {
		if(sqlangv[i]!=NULL) {
			argv[i] = (char*)pkg_malloc(strlen(sqlangv[i])+1);
			if(argv[i]==NULL) {
				LM_ERR("no more pkg\n");
				goto error;
			}
			strcpy(argv[i], sqlangv[i]);
		}
	}

	expf = find_export_record(sqlangv[0], argc-1, 0, &modver);
	if (expf==NULL) {
		LM_ERR("function '%s' is not available\n", sqlangv[0]);
		goto error;
	}
	/* check fixups */
	if (expf->fixup!=NULL && expf->free_fixup==NULL) {
		LM_ERR("function '%s' has fixup - cannot be used\n", sqlangv[0]);
		goto error;
	}
	switch(expf->param_no) {
		case 0:
			mod_type = MODULE0_T;
			break;
		case 1:
			mod_type = MODULE1_T;
			break;
		case 2:
			mod_type = MODULE2_T;
			break;
		case 3:
			mod_type = MODULE3_T;
			break;
		case 4:
			mod_type = MODULE4_T;
			break;
		case 5:
			mod_type = MODULE5_T;
			break;
		case 6:
			mod_type = MODULE6_T;
			break;
		case VAR_PARAM_NO:
			mod_type = MODULEX_T;
			break;
		default:
			LM_ERR("unknown/bad definition for function '%s' (%d params)\n",
					sqlangv[0], expf->param_no);
			goto error;
	}

	act = mk_action(mod_type,  argc+1   /* number of (type, value) pairs */,
					MODEXP_ST, expf,    /* function */
					NUMBER_ST, argc-1,  /* parameter number */
					STRING_ST, argv[1], /* param. 1 */
					STRING_ST, argv[2], /* param. 2 */
					STRING_ST, argv[3], /* param. 3 */
					STRING_ST, argv[4], /* param. 4 */
					STRING_ST, argv[5], /* param. 5 */
					STRING_ST, argv[6]  /* param. 6 */
			);

	if (act==NULL) {
		LM_ERR("action structure could not be created for '%s'\n", sqlangv[0]);
		goto error;
	}

	/* handle fixups */
	if (expf->fixup) {
		if(argc==1) {
			/* no parameters */
			if(expf->fixup(0, 0)<0) {
				LM_ERR("Error in fixup (0) for '%s'\n", sqlangv[0]);
				goto error;
			}
		} else {
			for(i=1; i<argc; i++) {
				if(expf->fixup(&(act->val[i+1].u.data), i)<0) {
					LM_ERR("Error in fixup (%d) for '%s'\n", i, sqlangv[0]);
					goto error;
				}
				act->val[i+1].type = MODFIXUP_ST;
			}
		}
	}
	init_run_actions_ctx(&ra_ctx);
	ret = do_action(&ra_ctx, act, env_J->msg);

	/* free fixups */
	if (expf->fixup) {
		for(i=1; i<argc; i++) {
			if ((act->val[i+1].type == MODFIXUP_ST) && (act->val[i+1].u.data)) {
				expf->free_fixup(&(act->val[i+1].u.data), i);
			}
		}
	}
	pkg_free(act);
	for(i=0; i<MAX_ACTIONS; i++) {
		if(argv[i]!=NULL) pkg_free(argv[i]);
		argv[i] = 0;
	}
	sq_pushinteger(J, ret);
	return 1;

error:
	if(act!=NULL)
		pkg_free(act);
	for(i=0; i<MAX_ACTIONS; i++) {
		if(argv[i]!=NULL) pkg_free(argv[i]);
		argv[i] = 0;
	}
	sq_pushinteger(J, -1);
	return 1;
}


const SQRegFunction _sr_kemi_x_J_Map[] = {
	{ "exit", sqlang_sr_exit, 1 /* 0 args */, NULL },
	{ "drop", sqlang_sr_drop, 1 /* 0 args */, NULL },
	{ "modf", sqlang_sr_modf, 0 /* var args */, NULL },
	{ NULL, NULL, 0 }
};

/**
 *
 */
void sqlang_errorfunc(HSQUIRRELVM J, const SQChar *fmt, ...)
{
	char ebuf[4096];
	va_list ap;

	if(_sr_J_env.JJ==J) {
		if(_sr_J_env.JJ_exit == 1) {
			LM_DBG("exception on ksr exit (JJ)\n");
			return;
		}
	} else {
		if(_sr_J_env.J_exit == 1) {
			LM_DBG("exception on ksr exit (J)\n");
			return;
		}
	}

	ebuf[0] = '\0';
	va_start(ap, fmt);
	vsnprintf(ebuf, 4094, fmt, ap);
	va_end(ap);
	LM_ERR("SQLang error: %s\n", ebuf);
}

/**
 *
 */
void sqlang_printfunc(HSQUIRRELVM SQ_UNUSED_ARG(J), const SQChar *fmt, ...)
{
	char ebuf[4096];
	va_list ap;

	ebuf[0] = '\0';
	va_start(ap, fmt);
	vsnprintf(ebuf, 4094, fmt, ap);
	va_end(ap);
	LM_INFO("SQLang info: %s\n", ebuf);
}

/**
 *
 */
void sqlang_debughook(HSQUIRRELVM J, SQInteger type, const SQChar *sourcename,
		SQInteger line, const SQChar *funcname)
{
	LM_ERR("SQLang: %s:%d - %s(...) [type %d]\n", (char*)sourcename, (int)line,
			(char*)funcname, (int)type);
}

/**
 * load a JS file into context
 */
static int sqlang_load_file(HSQUIRRELVM J, const char *filename)
{
	if(!SQ_SUCCEEDED(sqstd_dofile(J, _SC(filename), 0, 1))) {
		/* prints syntax errors if any */
		LM_ERR("failed to load file: %s\n", filename);
		return -1;
    }
    LM_DBG("loaded file: %s\n", filename);
	return 0;
#if 0
	FILE *f;
	size_t len;
#define SQLANG_SCRIPT_MAX_SIZE 128*1024
	char buf[SQLANG_SCRIPT_MAX_SIZE];

	f = fopen(filename, "rb");
	if (f) {
		len = fread((void *) buf, 1, sizeof(buf)-1, f);
		fclose(f);
		if(len>0) {
			buf[len] = '\0';
			if(!SQ_SUCCEEDED(sq_compilebuffer(J, buf, len,
							_SC(filename), SQTrue))) {
				LM_ERR("failed to compile: %s\n", filename);
				return -1;
			}
		} else {
			LM_ERR("empty content in %s\n", filename);
			return -1;
		}
	} else {
		LM_ERR("cannot open file: %s\n", filename);
		return -1;
	}
	return 0;
#endif
}

/**
 *
 */
int sqlang_sr_init_mod(void)
{
	if(_sr_sqlang_reload_version == NULL) {
		_sr_sqlang_reload_version = (int*)shm_malloc(sizeof(int));
		if(_sr_sqlang_reload_version == NULL) {
			LM_ERR("failed to allocated reload version\n");
			return -1;
		}
		*_sr_sqlang_reload_version = 0;
	}
	memset(&_sr_J_env, 0, sizeof(sr_sqlang_env_t));

	return 0;
}

/**
 *
 */
int sqlang_kemi_load_script(void)
{
	if(sqlang_load_file(_sr_J_env.JJ, _sr_sqlang_load_file.s)<0) {
		LM_ERR("failed to load sqlang script file: %.*s\n",
				_sr_sqlang_load_file.len, _sr_sqlang_load_file.s);
		return -1;
	}
	return 0;
}
/**
 *
 */
int sqlang_sr_init_child(void)
{
	memset(&_sr_J_env, 0, sizeof(sr_sqlang_env_t));
	_sr_J_env.J = sq_open(1024);
	if(_sr_J_env.J==NULL) {
		LM_ERR("cannot create SQlang context (exec)\n");
		return -1;
	}
    sq_pushroottable(_sr_J_env.J);
	/*sets the print functions*/
	sq_setprintfunc(_sr_J_env.J, sqlang_printfunc, sqlang_errorfunc);
	//sq_setnativedebughook(_sr_J_env.J, sqlang_debughook);
	sq_enabledebuginfo(_sr_J_env.J, 1);

    sqstd_register_bloblib(_sr_J_env.J);
    sqstd_register_iolib(_sr_J_env.J);
    sqstd_register_systemlib(_sr_J_env.J);
    sqstd_register_mathlib(_sr_J_env.J);
    sqstd_register_stringlib(_sr_J_env.J);
	sqstd_seterrorhandlers(_sr_J_env.J);

	sqlang_sr_kemi_register_libs(_sr_J_env.J);
	if(_sr_sqlang_load_file.s != NULL && _sr_sqlang_load_file.len>0) {
		_sr_J_env.JJ = sq_open(1024);
		if(_sr_J_env.JJ==NULL) {
			LM_ERR("cannot create load SQLang context (load)\n");
			return -1;
		}
		sq_pushroottable(_sr_J_env.JJ);
		LM_DBG("*** sqlang top index now is: %d\n", (int)sqlang_gettop(_sr_J_env.JJ));
		/*sets the print functions*/
		sq_setprintfunc(_sr_J_env.JJ, sqlang_printfunc, sqlang_errorfunc);
		//sq_setnativedebughook(_sr_J_env.JJ, sqlang_debughook);
		sq_enabledebuginfo(_sr_J_env.JJ, 1);

		sqstd_register_bloblib(_sr_J_env.JJ);
		sqstd_register_iolib(_sr_J_env.JJ);
		sqstd_register_systemlib(_sr_J_env.JJ);
		sqstd_register_mathlib(_sr_J_env.JJ);
		sqstd_register_stringlib(_sr_J_env.JJ);
		sqstd_seterrorhandlers(_sr_J_env.JJ);

		sqlang_sr_kemi_register_libs(_sr_J_env.JJ);
		LM_DBG("loading sqlang script file: %.*s\n",
				_sr_sqlang_load_file.len, _sr_sqlang_load_file.s);
		if(sqlang_kemi_load_script()<0) {
			return -1;
		}
	}
	LM_DBG("JS initialized!\n");
	return 0;
}

/**
 *
 */
void sqlang_sr_destroy(void)
{
	if(_sr_J_env.J!=NULL) {
		sq_close(_sr_J_env.J);
		_sr_J_env.J = NULL;
	}
	if(_sr_J_env.JJ!=NULL) {
		sq_close(_sr_J_env.JJ);
		_sr_J_env.JJ = NULL;
	}
	memset(&_sr_J_env, 0, sizeof(sr_sqlang_env_t));
}

/**
 *
 */
int sqlang_kemi_reload_script(void)
{
	int v;
	if(_sr_sqlang_load_file.s == NULL && _sr_sqlang_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		return -1;
	}
	if(_sr_sqlang_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		return -1;
	}
	if(_sr_J_env.JJ==NULL) {
		LM_ERR("load JS context not created\n");
		return -1;
	}

	v = *_sr_sqlang_reload_version;
	if(v == _sr_sqlang_local_version) {
		/* same version */
		return 0;
	}
	LM_DBG("reloading sqlang script file: %.*s (%d => %d)\n",
				_sr_sqlang_load_file.len, _sr_sqlang_load_file.s,
				_sr_sqlang_local_version, v);
	sqlang_kemi_load_script();
	_sr_sqlang_local_version = v;
	return 0;
}


/**
 *
 */
int app_sqlang_run_ex(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3, int emode)
{
	int n;
	int ret;
	int top;
	sip_msg_t *bmsg;
	SQInteger rv;

	if(_sr_J_env.JJ==NULL) {
		LM_ERR("sqlang loading state not initialized (call: %s)\n", func);
		return -1;
	}
	/* check the script version loaded */
	sqlang_kemi_reload_script();

	top = sqlang_gettop(_sr_J_env.JJ);

	LM_DBG("sqlang top index is: %d\n", top);
	sq_pushroottable(_sr_J_env.JJ); /* pushes the global table */
	sq_pushstring(_sr_J_env.JJ, _SC(func), -1);
	if(!SQ_SUCCEEDED(sq_get(_sr_J_env.JJ, -2))) {
		/* failed to gets the func field from the global table */
		sq_settop(_sr_J_env.JJ, (top<=0)?1:top); /* restores the original stack size */
		LM_ERR("sqlang failed to find symbol (call: %s)\n", func);
		return -1;
	}

	if(!sqlang_isfunction(_sr_J_env.JJ, -1))
	{
		LM_ERR("no such function [%s] in sqlang scripts\n", func);
		LM_ERR("top stack type [%d]\n",
			sqlang_gettype(_sr_J_env.JJ, -1));
	}
	/* push the 'this' (in this case is the global table) */
	sq_pushroottable(_sr_J_env.JJ);
	n = 1;
	if(p1!=NULL)
	{
		sqlang_pushstring(_sr_J_env.JJ, p1);
		n++;
		if(p2!=NULL)
		{
			sqlang_pushstring(_sr_J_env.JJ, p2);
			n++;
			if(p3!=NULL)
			{
				sqlang_pushstring(_sr_J_env.JJ, p3);
				n++;
			}
		}
	}
	LM_DBG("executing sqlang function: [[%s]] (n: %d)\n", func, n);
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	_sr_J_env.JJ_exit = 0;
	/* call the function */
	rv = sq_call(_sr_J_env.JJ, n, SQFalse, SQTrue);
	if(SQ_SUCCEEDED(rv)) {
		ret = 1;
	} else {
		if(_sr_J_env.JJ_exit==0) {
			LM_ERR("failed to execute the func: %s (%d)\n", func, (int)rv);
			sqstd_printcallstack(_sr_J_env.JJ);
			ret = -1;
		} else {
			LM_DBG("script execution exit\n");
			ret = 1;
		}
	}
	_sr_J_env.msg = bmsg;
	_sr_J_env.JJ_exit = 0;
	sq_settop(_sr_J_env.JJ, (top<=0)?1:top); /* restores the original stack size */

	return ret;
}

/**
 *
 */
int app_sqlang_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return app_sqlang_run_ex(msg, func, p1, p2, p3, 1);
}

/**
 *
 */
int app_sqlang_runstring(sip_msg_t *msg, char *script)
{
#if 0
	int ret;
	sip_msg_t *bmsg;

	if(_sr_J_env.JJ==NULL) {
		LM_ERR("sqlang loading state not initialized (call: %s)\n", script);
		return -1;
	}

	sqlang_kemi_reload_script();

	LM_DBG("running sqlang string: [[%s]]\n", script);
	LM_DBG("sqlang top index is: %d\n", sqlang_gettop(_sr_J_env.JJ));
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	sqlang_pushstring(_sr_J_env.JJ, script);
	ret = duk_peval(_sr_J_env.JJ);
	if(ret != 0) {
		LM_ERR("JS failed running: %s\n", sqlang_safe_tostring(_sr_J_env.JJ, -1));
	}
	duk_pop(_sr_J_env.JJ);  /* ignore result */

	_sr_J_env.msg = bmsg;
	return (ret==0)?1:-1;
#endif
	LM_ERR("not implemented\n");
	return -1;
}

/**
 *
 */
int app_sqlang_dostring(sip_msg_t *msg, char *script)
{
#if 0
	int ret;
	sip_msg_t *bmsg;

	LM_DBG("executing sqlang string: [[%s]]\n", script);
	LM_DBG("JS top index is: %d\n", sqlang_gettop(_sr_J_env.J));
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	sqlang_pushstring(_sr_J_env.J, script);
	ret = duk_peval(_sr_J_env.J);
	if(ret != 0) {
		LM_ERR("JS failed running: %s\n", sqlang_safe_tostring(_sr_J_env.J, -1));
	}
	duk_pop(_sr_J_env.J);  /* ignore result */
	_sr_J_env.msg = bmsg;
	return (ret==0)?1:-1;
#endif
	LM_ERR("not implemented\n");
	return -1;
}

/**
 *
 */
int app_sqlang_dofile(sip_msg_t *msg, char *script)
{
#if 0
	int ret;
	sip_msg_t *bmsg;

	LM_DBG("executing sqlang file: [[%s]]\n", script);
	LM_DBG("JS top index is: %d\n", sqlang_gettop(_sr_J_env.J));
	bmsg = _sr_J_env.msg;
	_sr_J_env.msg = msg;
	if(sqlang_load_file(_sr_J_env.J, script)<0) {
		LM_ERR("failed to load sqlang script file: %s\n", script);
		return -1;
	}
	ret = duk_peval(_sr_J_env.J);
	if(ret != 0) {
		LM_ERR("JS failed running: %s\n", sqlang_safe_tostring(_sr_J_env.J, -1));
	}
	duk_pop(_sr_J_env.J);  /* ignore result */

	_sr_J_env.msg = bmsg;
	return (ret==0)?1:-1;
#endif
	LM_ERR("not implemented\n");
	return -1;
}

/**
 *
 */
int sr_kemi_sqlang_exec_func_ex(HSQUIRRELVM J, sr_kemi_t *ket)
{
	int i;
	int argc;
	int ret;
	str *fname;
	str *mname;
	sr_kemi_val_t vps[SR_KEMI_PARAMS_MAX];
	sr_sqlang_env_t *env_J;

	env_J = sqlang_sr_env_get();

	if(env_J==NULL || env_J->msg==NULL || ket==NULL) {
		LM_ERR("invalid JS environment attributes or parameters\n");
		return app_sqlang_return_false(J);
	}

	fname = &ket->fname;
	mname = &ket->mname;

	argc = sqlang_gettop(J);
	if(argc==0 && ket->ptypes[0]==SR_KEMIP_NONE) {
		ret = ((sr_kemi_fm_f)(ket->func))(env_J->msg);
		return sr_kemi_sqlang_return_int(J, ket, ret);
	}
	if(argc==0 && ket->ptypes[0]!=SR_KEMIP_NONE) {
		LM_ERR("invalid number of parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return app_sqlang_return_false(J);
	}

	if(argc>SR_KEMI_PARAMS_MAX) {
		LM_ERR("too many parameters for: %.*s.%.*s\n",
				mname->len, mname->s, fname->len, fname->s);
		return app_sqlang_return_false(J);
	}

	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_val_t));
	for(i=0; i<SR_KEMI_PARAMS_MAX; i++) {
		if(ket->ptypes[i]==SR_KEMIP_NONE) {
			break;
		} else if(ket->ptypes[i]==SR_KEMIP_STR) {
			vps[i].s.s = (char*)sqlang_to_string(J, i);
			vps[i].s.len = strlen(vps[i].s.s);
			LM_DBG("param[%d] for: %.*s is str: %.*s\n", i,
				fname->len, fname->s, vps[i].s.len, vps[i].s.s);
		} else if(ket->ptypes[i]==SR_KEMIP_INT) {
			vps[i].n = sqlang_to_int(J, i);
			LM_DBG("param[%d] for: %.*s is int: %d\n", i,
				fname->len, fname->s, vps[i].n);
		} else {
			LM_ERR("unknown parameter type %d (%d)\n", ket->ptypes[i], i);
			return app_sqlang_return_false(J);
		}
	}

	switch(i) {
		case 1:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmn_f)(ket->func))(env_J->msg, vps[0].n);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fms_f)(ket->func))(env_J->msg, &vps[0].s);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_sqlang_return_false(J);
			}
		break;
		case 2:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmnn_f)(ket->func))(env_J->msg, vps[0].n, vps[1].n);
					return sr_kemi_sqlang_return_int(J, ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmns_f)(ket->func))(env_J->msg, vps[0].n, &vps[1].s);
					return sr_kemi_sqlang_return_int(J, ket, ret);
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_sqlang_return_false(J);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmsn_f)(ket->func))(env_J->msg, &vps[0].s, vps[1].n);
					return sr_kemi_sqlang_return_int(J, ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmss_f)(ket->func))(env_J->msg, &vps[0].s, &vps[1].s);
					return sr_kemi_sqlang_return_int(J, ket, ret);
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_sqlang_return_false(J);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_sqlang_return_false(J);
			}
		break;
		case 3:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmnnn_f)(ket->func))(env_J->msg,
								vps[0].n, vps[1].n, vps[2].n);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmnns_f)(ket->func))(env_J->msg,
								vps[0].n, vps[1].n, &vps[2].s);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname->len, fname->s);
						return app_sqlang_return_false(J);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmnsn_f)(ket->func))(env_J->msg,
								vps[0].n, &vps[1].s, vps[2].n);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmnss_f)(ket->func))(env_J->msg,
								vps[0].n, &vps[1].s, &vps[2].s);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname->len, fname->s);
						return app_sqlang_return_false(J);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_sqlang_return_false(J);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmsnn_f)(ket->func))(env_J->msg,
								&vps[0].s, vps[1].n, vps[2].n);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmsns_f)(ket->func))(env_J->msg,
								&vps[0].s, vps[1].n, &vps[2].s);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname->len, fname->s);
						return app_sqlang_return_false(J);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmssn_f)(ket->func))(env_J->msg,
								&vps[0].s, &vps[1].s, vps[2].n);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmsss_f)(ket->func))(env_J->msg,
								&vps[0].s, &vps[1].s, &vps[2].s);
						return sr_kemi_sqlang_return_int(J, ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname->len, fname->s);
						return app_sqlang_return_false(J);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname->len, fname->s);
					return app_sqlang_return_false(J);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_sqlang_return_false(J);
			}
		break;
		case 4:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsssn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnn_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsss_f)(ket->func))(env_J->msg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_sqlang_return_false(J);
			}
		break;
		case 5:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_sqlang_return_false(J);
			}
		break;
		case 6:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR
					&& ket->ptypes[5]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssssss_f)(ket->func))(env_J->msg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s, &vps[5].s);
				return sr_kemi_sqlang_return_int(J, ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname->len, fname->s);
				return app_sqlang_return_false(J);
			}
		break;
		default:
			LM_ERR("invalid parameters for: %.*s\n",
					fname->len, fname->s);
			return app_sqlang_return_false(J);
	}
}

/**
 *
 */
int sr_kemi_sqlang_exec_func(HSQUIRRELVM J, int eidx)
{
	sr_kemi_t *ket;

	ket = sr_kemi_sqlang_export_get(eidx);
	return sr_kemi_sqlang_exec_func_ex(J, ket);
}

/**
 *
 */
SQRegFunction *_sr_J_KSRMethods = NULL;
#define SR_SQLANG_KSR_MODULES_SIZE	256
#define SR_SQLANG_KSR_METHODS_SIZE	(SR_KEMI_SQLANG_EXPORT_SIZE + SR_SQLANG_KSR_MODULES_SIZE)


/**
 *
 */
SQInteger sqlang_register_global_func(HSQUIRRELVM J, SQFUNCTION f, char *fname)
{
    sq_pushstring(J, fname, -1);
    sq_newclosure(J, f, 0); /* create a new function */
    sq_newslot(J, -3, SQFalse);
    return 0;
}

/**
 *
 */
SQInteger sqlang_open_KSR(HSQUIRRELVM J)
{
	SQRegFunction *_sr_crt_J_KSRMethods = NULL;
	sr_kemi_module_t *emods = NULL;
	int emods_size = 0;
	int i;
	int k;
	int n;
	char mname[128];
	char malias[256];

	_sr_J_KSRMethods = malloc(SR_SQLANG_KSR_METHODS_SIZE * sizeof(SQRegFunction));
	if(_sr_J_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return 0;
	}
	memset(_sr_J_KSRMethods, 0, SR_SQLANG_KSR_METHODS_SIZE * sizeof(SQRegFunction));

	emods_size = sr_kemi_modules_size_get();
	emods = sr_kemi_modules_get();

	n = 0;
	_sr_crt_J_KSRMethods = _sr_J_KSRMethods;
	if(emods_size==0 || emods[0].kexp==NULL) {
		LM_ERR("no kemi exports registered\n");
		return 0;
	}

	sq_pushroottable(J); /* stack[1] */
	sq_pushstring(J, "KSR", -1);  /* stack[2] */
	sq_newtable(J);  /* stack[3] */

	for(i=0; emods[0].kexp[i].func!=NULL; i++) {
		LM_DBG("exporting KSR.%s(...)\n", emods[0].kexp[i].fname.s);
		_sr_crt_J_KSRMethods[i].name = emods[0].kexp[i].fname.s;
		_sr_crt_J_KSRMethods[i].f =
			sr_kemi_sqlang_export_associate(&emods[0].kexp[i]);
		if(_sr_crt_J_KSRMethods[i].f == NULL) {
			LM_ERR("failed to associate kemi function with sqlang export\n");
			free(_sr_J_KSRMethods);
			_sr_J_KSRMethods = NULL;
			goto error;
		}
		_sr_crt_J_KSRMethods[i].nparamscheck = 0;
		snprintf(malias, 254, "%s", emods[0].kexp[i].fname.s);
		sqlang_register_global_func(J, _sr_crt_J_KSRMethods[i].f, malias);
		n++;
	}

	/* special modules - pv.get(...) can return int or str */
	sq_pushstring(J, "pv", -1);  /* stack[4] */
	sq_newtable(J);  /* stack[5] */
	i=0;
	while(_sr_kemi_pv_J_Map[i].name!=0) {
		snprintf(malias, 254, "%s", _sr_kemi_pv_J_Map[i].name);
		sqlang_register_global_func(J, _sr_kemi_pv_J_Map[i].f, malias);
		i++;
	}
    sq_newslot(J, -3, SQFalse);
	sq_pushstring(J, "x", -1);  /* stack[4] */
	sq_newtable(J);  /* stack[5] */
	i=0;
	while(_sr_kemi_x_J_Map[i].name!=0) {
		snprintf(malias, 254, "%s", _sr_kemi_x_J_Map[i].name);
		sqlang_register_global_func(J, _sr_kemi_x_J_Map[i].f, malias);
		i++;
	}
    sq_newslot(J, -3, SQFalse);

	/* registered kemi modules */
	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
			n++;
			_sr_crt_J_KSRMethods = _sr_J_KSRMethods + n;
			snprintf(mname, 128, "%s", emods[k].kexp[0].mname.s);
			sq_pushstring(J, mname, -1);  /* stack[4] */
			sq_newtable(J);  /* stack[5] */
			for(i=0; emods[k].kexp[i].func!=NULL; i++) {
				LM_DBG("exporting %s.%s(...)\n", mname,
						emods[k].kexp[i].fname.s);
				_sr_crt_J_KSRMethods[i].name = emods[k].kexp[i].fname.s;
				_sr_crt_J_KSRMethods[i].f =
					sr_kemi_sqlang_export_associate(&emods[k].kexp[i]);
				if(_sr_crt_J_KSRMethods[i].f == NULL) {
					LM_ERR("failed to associate kemi function with func export\n");
					free(_sr_J_KSRMethods);
					_sr_J_KSRMethods = NULL;
					goto error;
				}
				_sr_crt_J_KSRMethods[i].nparamscheck = 0;
				snprintf(malias, 256, "%s", _sr_crt_J_KSRMethods[i].name);
				sqlang_register_global_func(J, _sr_crt_J_KSRMethods[i].f, malias);
				n++;
			}
			sq_newslot(J, -3, SQFalse);

			LM_DBG("initializing kemi sub-module: %s (%s)\n", mname,
					emods[k].kexp[0].mname.s);
		}
	}
    sq_newslot(J, -3, SQFalse);
    sq_pop(J ,1); /* pops the root table */
	LM_DBG("module 'KSR' has been initialized\n");
	return 1;
error:
    sq_pop(J ,1); /* pops the root table */
	return 0;
}

/**
 *
 */
void sqlang_sr_kemi_register_libs(HSQUIRRELVM J)
{
	int ret;

	ret = sqlang_open_KSR(J);

	LM_INFO("initialized KSR module with return code: %d\n", ret);
}

static const char* app_sqlang_rpc_reload_doc[2] = {
	"Reload sqlang file",
	0
};


static void app_sqlang_rpc_reload(rpc_t* rpc, void* ctx)
{
	int v;
	void *vh;

	if(_sr_sqlang_load_file.s == NULL && _sr_sqlang_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		rpc->fault(ctx, 500, "No script file");
		return;
	}
	if(_sr_sqlang_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		rpc->fault(ctx, 500, "Reload not enabled");
		return;
	}

	v = *_sr_sqlang_reload_version;
	LM_INFO("marking for reload sqlang script file: %.*s (%d => %d)\n",
				_sr_sqlang_load_file.len, _sr_sqlang_load_file.s,
				_sr_sqlang_local_version, v);
	*_sr_sqlang_reload_version += 1;

	if (rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "dd",
			"old", v,
			"new", *_sr_sqlang_reload_version);
}

static const char* app_sqlang_rpc_api_list_doc[2] = {
	"List kemi exports to sqlang",
	0
};

static void app_sqlang_rpc_api_list(rpc_t* rpc, void* ctx)
{
	int i;
	int n;
	sr_kemi_t *ket;
	void* th;
	void* sh;
	void* ih;

	if (rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	n = 0;
	for(i=0; i<SR_KEMI_SQLANG_EXPORT_SIZE; i++) {
		ket = sr_kemi_sqlang_export_get(i);
		if(ket==NULL) continue;
		n++;
	}

	if(rpc->struct_add(th, "d[",
				"msize", n,
				"methods",  &ih)<0)
	{
		rpc->fault(ctx, 500, "Internal error array structure");
		return;
	}
	for(i=0; i<SR_KEMI_SQLANG_EXPORT_SIZE; i++) {
		ket = sr_kemi_sqlang_export_get(i);
		if(ket==NULL) continue;
		if(rpc->struct_add(ih, "{", "func", &sh)<0) {
			rpc->fault(ctx, 500, "Internal error internal structure");
			return;
		}
		if(rpc->struct_add(sh, "SSSS",
				"ret", sr_kemi_param_map_get_name(ket->rtype),
				"module", &ket->mname,
				"name", &ket->fname,
				"params", sr_kemi_param_map_get_params(ket->ptypes))<0) {
			LM_ERR("failed to add the structure with attributes (%d)\n", i);
			rpc->fault(ctx, 500, "Internal error creating dest struct");
			return;
		}
	}
}

rpc_export_t app_sqlang_rpc_cmds[] = {
	{"app_sqlang.reload", app_sqlang_rpc_reload,
		app_sqlang_rpc_reload_doc, 0},
	{"app_sqlang.api_list", app_sqlang_rpc_api_list,
		app_sqlang_rpc_api_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int app_sqlang_init_rpc(void)
{
	if (rpc_register_array(app_sqlang_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
