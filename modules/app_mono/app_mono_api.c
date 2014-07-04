/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../lib/kcore/cmpapi.h"

#include "app_mono_api.h"

#include <mono/metadata/mono-config.h>

#define SRVERSION "1.0"

/**
 *
 */
typedef struct _sr_mono_load
{
	char *script;
	MonoDomain *domain;
	MonoAssembly *assembly;
	struct _sr_mono_load *next;
} sr_mono_load_t;

static sr_mono_load_t *_sr_mono_load_list = NULL;

int sr_mono_load_class_core();
int sr_mono_load_class_pv();
int sr_mono_load_class_hdr();

/**
 *
 */
static sr_mono_env_t _sr_M_env;

/**
 * @return the static Mono env
 */
sr_mono_env_t *sr_mono_env_get(void)
{
	return &_sr_M_env;
}

/**
 *
 */
int sr_mono_load_script(char *script)
{
	sr_mono_load_t *mi;

	if(_sr_mono_load_list != NULL)
	{
		LM_ERR("only one assembly can be loaded\n");
		return -1;
	}
	mi = (sr_mono_load_t*)pkg_malloc(sizeof(sr_mono_load_t));
	if(mi==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(mi, 0, sizeof(sr_mono_load_t));
	mi->script = script;
	mi->next = _sr_mono_load_list;
	_sr_mono_load_list = mi;
	return 0;
}

/**
 *
 */
int sr_mono_assembly_loaded(void)
{
	if(_sr_mono_load_list != NULL)
		return 1;
	return 0;
}

/**
 *
 */
int sr_mono_register_module(char *mname)
{
//	if(mono_sr_exp_register_mod(mname)==0)
//		return 0;
	return -1;
}

/**
 *
 */
int mono_sr_init_mod(void)
{
	memset(&_sr_M_env, 0, sizeof(sr_mono_env_t));
//	if(mono_sr_exp_init_mod()<0)
//		return -1;
	return 0;
}

/**
 *
 */
int mono_sr_init_load(void)
{
	sr_mono_load_t *mi;
	if(_sr_mono_load_list == NULL) {
		LM_DBG("no assembly to load\n");
		return 0;
	}
	mono_config_parse (NULL);
	mi = _sr_mono_load_list;
	if(mi->domain != NULL)
	{
		LM_ERR("worker mono environment already initialized\n");
		return 0;
	}
	while(mi!=NULL)
	{
		mi->domain = mono_jit_init (mi->script);
		if (mi->domain==NULL) {
			LM_ERR("failed to init domain for: %s\n", mi->script);
			return -1;
		}
		sr_mono_load_class_core();
		sr_mono_load_class_pv();
		sr_mono_load_class_hdr();

		mi->assembly = mono_domain_assembly_open(mi->domain, mi->script);
		if (mi->assembly==NULL) {
			LM_ERR("failed to open assembly: %s\n", mi->script);
			return -1;
		}
		mi = mi->next;
		/* only one (first) assembly for now */
		break;
	}
	return 0;
}

/**
 *
 */
int mono_sr_init_probe(void)
{
	/* todo: test if assemblies exist */
	return 0;
}

/**
 *
 */
int mono_sr_init_child(void)
{
	memset(&_sr_M_env, 0, sizeof(sr_mono_env_t));

	/*
	 * Load the default Mono configuration file, this is needed
	 * if you are planning on using the dllmaps defined on the
	 * system configuration
	 */
	mono_config_parse (NULL);

	return 0;
}

/**
 *
 */
void mono_sr_destroy(void)
{
	memset(&_sr_M_env, 0, sizeof(sr_mono_env_t));
}

/**
 *
 */
int mono_sr_initialized(void)
{
	if(_sr_M_env.domain==NULL)
		return 1;

	return 1;
}

/**
 *
 */
int app_mono_exec(struct sip_msg *msg, char *script, char *param)
{
	int ret;
	char *argv[2];
	int argc;

	argc = 1;
	argv[0] = script;
	if(param!=NULL) {
		argc++;
		argv[1] = param;
	}
	LM_DBG("executing Mono assembly: [[%s]]\n", argv[0]);
	_sr_M_env.msg = msg;

	mono_config_parse (NULL);
	/*
	 * mono_jit_init() creates a domain: each assembly is
	 * loaded and run in a MonoDomain.
	 */
	_sr_M_env.domain = mono_jit_init (argv[0]);
	/*
	 * We add our special internal functions, so that C# code
	 * can call us back.
	 */
	sr_mono_load_class_core();
	sr_mono_load_class_pv();
	sr_mono_load_class_hdr();

	_sr_M_env.assembly = mono_domain_assembly_open(_sr_M_env.domain, argv[0]);
	if (_sr_M_env.assembly==NULL) {
		ret = -1;
		goto done;
	}
	/*
	 * mono_jit_exec() will run the Main() method in the assembly.
	 * The return value needs to be looked up from
	 * System.Environment.ExitCode.
	 */
	mono_jit_exec(_sr_M_env.domain, _sr_M_env.assembly, argc, argv);
	ret = mono_environment_exitcode_get();
	LM_DBG("returned code from mono environment: %d\n", ret);

done:
	mono_jit_cleanup(_sr_M_env.domain);

	memset(&_sr_M_env, 0, sizeof(sr_mono_env_t));
	return (ret==0)?1:-1;
}

/**
 *
 */
int app_mono_run(struct sip_msg *msg, char *arg)
{
	int ret;
	char *argv[2];
	int argc;
	sr_mono_load_t *mi;

	if(_sr_mono_load_list == NULL)
		return -1;
	mi = _sr_mono_load_list;

	LM_DBG("running Mono assembly: [[%s]]\n", mi->script);
	_sr_M_env.msg = msg;

	_sr_M_env.domain = mi->domain;
	_sr_M_env.assembly = mi->assembly;
	if (_sr_M_env.assembly==NULL) {
		LM_DBG("empty assembly\n");
		memset(&_sr_M_env, 0, sizeof(sr_mono_env_t));
		return -1;
	}
	mono_domain_set(_sr_M_env.domain, 0);
	argc = 1;
	argv[0] = mi->script;
	if(arg!=NULL) {
		argc++;
		argv[1] = arg;
	}
	mono_jit_exec(_sr_M_env.domain, _sr_M_env.assembly, argc, argv);
	ret = mono_environment_exitcode_get();
	LM_DBG("returned code from mono environment: %d\n", ret);

	memset(&_sr_M_env, 0, sizeof(sr_mono_env_t));
	return (ret==0)?1:-1;
}


/**
 *
 */
static MonoString* sr_mono_api_version() {
	return mono_string_new (mono_domain_get(), SRVERSION);
}

static void sr_mono_log(int level, MonoString *text) {
	char *logmsg;
	logmsg = mono_string_to_utf8(text);
	LOG(level, "%s", logmsg);
	mono_free(logmsg);
}

static void sr_mono_err(MonoString *text) {
	char *logmsg;
	logmsg = mono_string_to_utf8(text);
	LOG(L_ERR, "%s", logmsg);
	mono_free(logmsg);
}

static void sr_mono_dbg(MonoString *text) {
	char *logmsg;
	logmsg = mono_string_to_utf8(text);
	LOG(L_DBG, "%s", logmsg);
	mono_free(logmsg);
}

/**
 *
 */
static int sr_mono_modf(MonoString *nfunc)
{
	int ret;
	int mod_type;
	struct run_act_ctx ra_ctx;
	unsigned modver;
	struct action *act = NULL;
	sr31_cmd_export_t* expf;
	sr_mono_env_t *env_M;
	char *func = NULL;

	env_M = sr_mono_env_get();
	if(env_M->msg==NULL)
		goto error;

	func = mono_string_to_utf8(nfunc);

	expf = find_export_record(func, 0, 0, &modver);
	if (expf==NULL) {
		LM_ERR("function '%s' is not available\n", func);
		goto error;
	}
	/* check fixups */
	if (expf->fixup!=NULL && expf->free_fixup==NULL) {
		LM_ERR("function '%s' has fixup - cannot be used\n", func);
		goto error;
	}
	mod_type = MODULE0_T;

	act = mk_action(mod_type,  1        /* number of (type, value) pairs */,
					MODEXP_ST, expf,    /* function */
					NUMBER_ST, 0,       /* parameter number */
					STRING_ST, NULL,    /* param. 1 */
					STRING_ST, NULL,    /* param. 2 */
					STRING_ST, NULL,    /* param. 3 */
					STRING_ST, NULL,    /* param. 4 */
					STRING_ST, NULL,    /* param. 5 */
					STRING_ST, NULL     /* param. 6 */
			);

	if (act==NULL) {
		LM_ERR("action structure could not be created for '%s'\n", func);
		goto error;
	}

	/* handle fixups */
	if (expf->fixup) {
		/* no parameters */
		if(expf->fixup(0, 0)<0)
		{
			LM_ERR("Error in fixup (0) for '%s'\n", func);
			goto error;
		}
	}
	init_run_actions_ctx(&ra_ctx);
	ret = do_action(&ra_ctx, act, env_M->msg);
	pkg_free(act);
	mono_free(func);
	return ret;

error:
	if(func!=NULL)
		mono_free(func);
	if(act!=NULL)
		pkg_free(act);
	return -127;
}


static const sr_M_export_t _sr_M_export_core[] = {
	{"SR.Core::APIVersion", sr_mono_api_version},
	{"SR.Core::Log",        sr_mono_log},
	{"SR.Core::Err",        sr_mono_err},
	{"SR.Core::Dbg",        sr_mono_dbg},
	{"SR.Core::ModF",       sr_mono_modf},
	{NULL, NULL}
};

int sr_mono_load_class_core()
{
	int i;
	for(i=0; _sr_M_export_core[i].name!=NULL; i++)
		mono_add_internal_call(_sr_M_export_core[i].name, _sr_M_export_core[i].method);
	return 0;
}

/**
 *
 */
static MonoString *sr_mono_pv_gets (MonoString *pv)
{
	str pvn = {0};
	pv_spec_t *pvs;
    pv_value_t val;
	sr_mono_env_t *env_M;
	int pl;

	env_M = sr_mono_env_get();

	pvn.s = mono_string_to_utf8(pv);
	if(pvn.s==NULL || env_M->msg==NULL)
		goto error;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len)
	{
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		goto error;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL)
	{
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		goto error;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_M->msg, pvs, &val) != 0)
	{
		LM_ERR("unable to get pv value for [%s]\n", pvn.s);
		goto error;
	}
	if((val.flags&PV_VAL_NULL) || !(val.flags&PV_VAL_STR))
	{
		mono_free(pvn.s);
		return NULL;
	}
	mono_free(pvn.s);
	return mono_string_new_len (mono_domain_get(), val.rs.s, val.rs.len);

error:
	if(pvn.s!=NULL)
		mono_free(pvn.s);
	return NULL;
}

/**
 *
 */
static int sr_mono_pv_geti (MonoString *pv)
{
	str pvn = {0};
	pv_spec_t *pvs;
    pv_value_t val;
	int pl;
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	pvn.s = mono_string_to_utf8(pv);

	if(pvn.s==NULL || env_M->msg==NULL)
		goto error;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len)
	{
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		goto error;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL)
	{
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		goto error;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_M->msg, pvs, &val) != 0)
	{
		LM_ERR("unable to get pv value for [%s]\n", pvn.s);
		goto error;
	}
	if((val.flags&PV_VAL_NULL) || !(val.flags&PV_TYPE_INT))
	{
		mono_free(pvn.s);
		return 0;
	}
	mono_free(pvn.s);
	return val.ri;

error:
	if(pvn.s!=NULL)
		mono_free(pvn.s);
	return 0;
}


/**
 *
 */
static int sr_mono_pv_seti (MonoString *pv, int iv)
{
	str pvn = {0};
	pv_spec_t *pvs;
    pv_value_t val;
	int pl;
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	pvn.s = mono_string_to_utf8(pv);

	if(pvn.s==NULL || env_M->msg==NULL)
		goto error;

	memset(&val, 0, sizeof(pv_value_t));
	val.ri = iv;
	val.flags |= PV_TYPE_INT|PV_VAL_INT;
	
	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len)
	{
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		goto error;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL)
	{
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		goto error;
	}
	if(pv_set_spec_value(env_M->msg, pvs, 0, &val)<0)
	{
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		goto error;
	}

	mono_free(pvn.s);
	return 0;
error:
	if(pvn.s!=NULL)
		mono_free(pvn.s);
	return -1;
}

/**
 *
 */
static int sr_mono_pv_sets (MonoString *pv, MonoString *sv)
{
	str pvn = {0};
	pv_spec_t *pvs;
    pv_value_t val;
	int pl;
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	pvn.s = mono_string_to_utf8(pv);

	if(pvn.s==NULL || env_M->msg==NULL)
		goto error;

	memset(&val, 0, sizeof(pv_value_t));
	val.rs.s = mono_string_to_utf8(sv);
	if(val.rs.s == NULL)
		goto error;

	val.rs.len = strlen(val.rs.s);
	val.flags |= PV_VAL_STR;
	
	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len)
	{
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		goto error;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL)
	{
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		goto error;
	}
	if(pv_set_spec_value(env_M->msg, pvs, 0, &val)<0)
	{
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return -1;
	}

	mono_free(pvn.s);
	return 0;
error:
	if(pvn.s!=NULL)
		mono_free(pvn.s);
	return -1;
}

/**
 *
 */
static int sr_mono_pv_unset (MonoString *pv)
{
	str pvn = {0};
	pv_spec_t *pvs;
    pv_value_t val;
	int pl;
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	pvn.s = mono_string_to_utf8(pv);

	if(pvn.s==NULL || env_M->msg==NULL)
		goto error;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv unset: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len)
	{
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		goto error;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL)
	{
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		goto error;
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_NULL;
	if(pv_set_spec_value(env_M->msg, pvs, 0, &val)<0)
	{
		LM_ERR("unable to unset pv [%s]\n", pvn.s);
		goto error;
	}

	mono_free(pvn.s);
	return 0;
error:
	if(pvn.s!=NULL)
		mono_free(pvn.s);
	return -1;
}

/**
 *
 */
static int sr_mono_pv_is_null (MonoString *pv)
{
	str pvn = {0};
	pv_spec_t *pvs;
    pv_value_t val;
	int pl;
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	pvn.s = mono_string_to_utf8(pv);

	if(pvn.s==NULL || env_M->msg==NULL)
		goto error;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv is null test: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len)
	{
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		goto error;
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL)
	{
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		goto error;
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(env_M->msg, pvs, &val) != 0)
	{
		LM_INFO("unable to get pv value for [%s]\n", pvn.s);
		goto error;
	}
	mono_free(pvn.s);
	if(val.flags&PV_VAL_NULL)
	{
		return 1;
	}
	return 0;
error:
	if(pvn.s!=NULL)
		mono_free(pvn.s);
	return -1;
}

/**
 *
 */
static const sr_M_export_t _sr_M_export_pv[] = {
	{"SR.PV::GetS",      sr_mono_pv_gets},
	{"SR.PV::GetI",      sr_mono_pv_geti},
	{"SR.PV::SetI",      sr_mono_pv_seti},
	{"SR.PV::SetS",      sr_mono_pv_sets},
	{"SR.PV::Unset",     sr_mono_pv_unset},
	{"SR.PV::IsNull",    sr_mono_pv_is_null},
	{NULL, NULL}
};

int sr_mono_load_class_pv()
{
	int i;
	for(i=0; _sr_M_export_pv[i].name!=NULL; i++)
		mono_add_internal_call(_sr_M_export_pv[i].name, _sr_M_export_pv[i].method);
	return 0;
}


/**
 *
 */
static int sr_mono_hdr_append (MonoString *hv)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *hdr;
	str txt = {0};
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	txt.s = mono_string_to_utf8(hv);

	if(txt.s==NULL || env_M->msg==NULL)
		goto error;

	txt.len = strlen(txt.s);

	LM_DBG("append hf: %s\n", txt.s);
	if (parse_headers(env_M->msg, HDR_EOH_F, 0) == -1)
	{
		LM_ERR("error while parsing message\n");
		goto error;
	}

	hf = env_M->msg->last_header;
	hdr = (char*)pkg_malloc(txt.len);
	if(hdr==NULL)
	{
		LM_ERR("no pkg memory left\n");
		goto error;
	}
	memcpy(hdr, txt.s, txt.len);
	anchor = anchor_lump(env_M->msg,
				hf->name.s + hf->len - env_M->msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, txt.len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		goto error;
	}
	mono_free(txt.s);
	return 0;

error:
	if(txt.s!=NULL)
		mono_free(txt.s);
	return -1;
}

/**
 *
 */
static int sr_mono_hdr_remove (MonoString *hv)
{
	struct lump* anchor;
	struct hdr_field *hf;
	str hname;
	str txt = {0};
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	txt.s = mono_string_to_utf8(hv);

	if(txt.s==NULL || env_M->msg==NULL)
		goto error;

	txt.len = strlen(txt.s);

	LM_DBG("remove hf: %s\n", txt.s);
	if (parse_headers(env_M->msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		goto error;
	}

	hname.s = txt.s;
	hname.len = txt.len;
	for (hf=env_M->msg->headers; hf; hf=hf->next)
	{
		if (cmp_hdrname_str(&hf->name, &hname)==0)
		{
			anchor=del_lump(env_M->msg,
					hf->name.s - env_M->msg->buf, hf->len, 0);
			if (anchor==0)
			{
				LM_ERR("cannot remove hdr %s\n", txt.s);
				goto error;
			}
		}
	}
	mono_free(txt.s);
	return 0;

error:
	if(txt.s!=NULL)
		mono_free(txt.s);
	return -1;
}

/**
 *
 */
static int sr_mono_hdr_insert (MonoString *hv)
{
	struct lump* anchor;
	struct hdr_field *hf;
	char *hdr;
	str txt = {0};
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	txt.s = mono_string_to_utf8(hv);

	if(txt.s==NULL || env_M->msg==NULL)
		goto error;

	txt.len = strlen(txt.s);

	LM_DBG("insert hf: %s\n", txt.s);
	hf = env_M->msg->headers;
	hdr = (char*)pkg_malloc(txt.len);
	if(hdr==NULL)
	{
		LM_ERR("no pkg memory left\n");
		goto error;
	}
	memcpy(hdr, txt.s, txt.len);
	anchor = anchor_lump(env_M->msg,
				hf->name.s + hf->len - env_M->msg->buf, 0, 0);
	if(insert_new_lump_before(anchor, hdr, txt.len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(hdr);
		goto error;
	}
	mono_free(txt.s);
	return 0;

error:
	if(txt.s!=NULL)
		mono_free(txt.s);
	return -1;
}

/**
 *
 */
static int sr_mono_hdr_append_to_reply (MonoString *hv)
{
	str txt = {0};
	sr_mono_env_t *env_M;

	env_M = sr_mono_env_get();
	txt.s = mono_string_to_utf8(hv);

	if(txt.s==NULL || env_M->msg==NULL)
		goto error;

	txt.len = strlen(txt.s);

	LM_DBG("append to reply: %s\n", txt.s);

	if(add_lump_rpl(env_M->msg, txt.s, txt.len, LUMP_RPL_HDR)==0)
	{
		LM_ERR("unable to add reply lump\n");
		goto error;
	}

	mono_free(txt.s);
	return 0;

error:
	if(txt.s!=NULL)
		mono_free(txt.s);
	return -1;
}


/**
 *
 */
static const sr_M_export_t _sr_M_export_hdr[] = {
	{"SR.HDR::Append", sr_mono_hdr_append},
	{"SR.HDR::Remove", sr_mono_hdr_remove},
	{"SR.HDR::Insert", sr_mono_hdr_insert},
	{"SR.HDR::AppendToReply", sr_mono_hdr_append_to_reply},
	{NULL, NULL}
};

int sr_mono_load_class_hdr()
{
	int i;
	for(i=0; _sr_M_export_hdr[i].name!=NULL; i++)
		mono_add_internal_call(_sr_M_export_hdr[i].name, _sr_M_export_hdr[i].method);
	return 0;
}


