/**
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/data_lump.h"
#include "../../core/str_list.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"

#include "secsipid_papi.h"

MODULE_VERSION

static void *_secsipid_dlhandle = NULL;

static int secsipid_expire = 300;
static int secsipid_timeout = 5;

static int secsipid_cache_expire = 3600;
static str secsipid_cache_dir = str_init("");
static str secsipid_modproc = str_init("secsipid_proc.so");

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_secsipid_check_identity(sip_msg_t *msg, char *pkeypath, char *str2);
static int w_secsipid_check(sip_msg_t *msg, char *pidentity, char *pkeypath);
static int w_secsipid_check_identity_pubkey(sip_msg_t *msg, char *pkeyval, char *str2);
static int w_secsipid_add_identity(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeypath);
static int w_secsipid_build_identity(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeypath);
static int w_secsipid_build_identity_prvkey(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeydata);
static int w_secsipid_sign(sip_msg_t *msg, char *phdrs, char *ppayload, char *pkeypath);
static int w_secsipid_get_url(sip_msg_t *msg, char *purl, char *pout);

static int secsipid_libopt_param(modparam_t type, void *val);

static int pv_get_secsipid(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int pv_parse_secsipid_name(pv_spec_p sp, str *in);

static str_list_t *secsipid_libopt_list = NULL;
static int secsipid_libopt_list_used = 0;

typedef struct secsipid_data {
	str value;
	int ret;
} secsipid_data_t;

static secsipid_data_t _secsipid_data = {0};

secsipid_papi_t _secsipid_papi = {0};

/* clang-format off */
static cmd_export_t cmds[]={
	{"secsipid_check_identity", (cmd_function)w_secsipid_check_identity, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"secsipid_check_identity_pubkey", (cmd_function)w_secsipid_check_identity_pubkey, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"secsipid_check", (cmd_function)w_secsipid_check, 2,
		fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"secsipid_add_identity", (cmd_function)w_secsipid_add_identity, 6,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"secsipid_build_identity", (cmd_function)w_secsipid_build_identity, 6,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"secsipid_build_identity_prvkey", (cmd_function)w_secsipid_build_identity_prvkey, 6,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"secsipid_sign", (cmd_function)w_secsipid_sign, 3,
		fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{"secsipid_get_url", (cmd_function)w_secsipid_get_url, 2,
		fixup_spve_pvar, fixup_free_spve_pvar, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"expire",        PARAM_INT,   &secsipid_expire},
	{"timeout",       PARAM_INT,   &secsipid_timeout},
	{"cache_expire",  PARAM_INT,   &secsipid_cache_expire},
	{"cache_dir",     PARAM_STR,   &secsipid_cache_dir},
	{"modproc",       PARAM_STR,   &secsipid_modproc},
	{"libopt",        PARAM_STR|USE_FUNC_PARAM,
		(void*)secsipid_libopt_param},

	{0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{{"secsipid", (sizeof("secsipid")-1)}, PVT_OTHER, pv_get_secsipid, 0,
		pv_parse_secsipid_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"secsipid",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	mod_pvs,        /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy    	/* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	if(_secsipid_dlhandle!=0) {
		dlclose(_secsipid_dlhandle);
		_secsipid_dlhandle = NULL;
	}
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	char *errstr = NULL;
	char *modpath = NULL;
	secsipid_proc_bind_f bind_f = NULL;

	if(rank==PROC_MAIN || rank==PROC_TCP_MAIN || rank==PROC_INIT) {
		LM_DBG("skipping child init for rank: %d\n", rank);
		return 0;
	}

	if(ksr_locate_module(secsipid_modproc.s, &modpath)<0) {
		return -1;
	}

	LM_DBG("trying to load <%s>\n", modpath);

#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif
	_secsipid_dlhandle = dlopen(modpath, RTLD_NOW); /* resolve all symbols now */
	if (_secsipid_dlhandle==0) {
		LM_ERR("could not open module <%s>: %s\n", modpath, dlerror());
		goto error;
	}
	/* launch register */
	bind_f = (secsipid_proc_bind_f)dlsym(_secsipid_dlhandle, "secsipid_proc_bind");
	if (((errstr=(char*)dlerror())==NULL) && bind_f!=NULL) {
		/* version control */
		if (!ksr_version_control(_secsipid_dlhandle, modpath)) {
			goto error;
		}
		/* no error - call it */
		if(bind_f(&_secsipid_papi)<0) {
			LM_ERR("filed to bind the api of proc module: %s\n", modpath);
			goto error;
		}
		LM_DBG("bound to proc module: <%s>\n", modpath);
	} else {
		LM_ERR("failure - func: %p - error: %s\n", bind_f, (errstr)?errstr:"none");
		goto error;
	}
	if(secsipid_modproc.s != modpath) {
		pkg_free(modpath);
	}
	return 0;

error:
	if(secsipid_modproc.s != modpath) {
		pkg_free(modpath);
	}
	return -1;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

#define SECSIPID_HDR_IDENTITY "Identity"
#define SECSIPID_HDR_IDENTITY_LEN (sizeof(SECSIPID_HDR_IDENTITY) - 1)

/**
 *
 */
static int ki_secsipid_check_identity(sip_msg_t *msg, str *keypath)
{
	int ret = 1;
	str ibody = STR_NULL;
	hdr_field_t *hf;

	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len==SECSIPID_HDR_IDENTITY_LEN
				&& strncasecmp(hf->name.s, SECSIPID_HDR_IDENTITY,
					SECSIPID_HDR_IDENTITY_LEN)==0)
			break;
	}

	if(hf == NULL) {
		LM_DBG("no identity header\n");
		return -1;
	}

	ibody = hf->body;

	if(secsipid_cache_dir.len > 0) {
		_secsipid_papi.SecSIPIDSetFileCacheOptions(secsipid_cache_dir.s,
				secsipid_cache_expire);
	}
	if(secsipid_libopt_list_used==0) {
		str_list_t *sit;
		for(sit=secsipid_libopt_list; sit!=NULL; sit=sit->next) {
			_secsipid_papi.SecSIPIDOptSetV(sit->s.s);
		}
		secsipid_libopt_list_used = 1;
	}
	ret = _secsipid_papi.SecSIPIDCheckFull(ibody.s, ibody.len, secsipid_expire,
			keypath->s, secsipid_timeout);

	if(ret==0) {
		LM_DBG("identity check: ok\n");
		return 1;
	}

	LM_DBG("identity check: failed\n");
	return ret;
}

/**
 *
 */
static int w_secsipid_check_identity(sip_msg_t *msg, char *pkeypath, char *str2)
{
	str keypath = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_secsipid_check_identity(msg, &keypath);
}

/**
 *
 */
static int ki_secsipid_check_identity_pubkey(sip_msg_t *msg, str *keyval)
{
	int ret = 1;
	str ibody = STR_NULL;
	hdr_field_t *hf;

	for (hf=msg->headers; hf; hf=hf->next) {
		if (hf->name.len==SECSIPID_HDR_IDENTITY_LEN
				&& strncasecmp(hf->name.s, SECSIPID_HDR_IDENTITY,
					SECSIPID_HDR_IDENTITY_LEN)==0)
			break;
	}

	if(hf == NULL) {
		LM_DBG("no identity header\n");
		return -1;
	}

	if(secsipid_libopt_list_used==0) {
		str_list_t *sit;
		for(sit=secsipid_libopt_list; sit!=NULL; sit=sit->next) {
			_secsipid_papi.SecSIPIDOptSetV(sit->s.s);
		}
		secsipid_libopt_list_used = 1;
	}

	ibody = hf->body;

	ret = _secsipid_papi.SecSIPIDCheckFullPubKey(ibody.s, ibody.len,
			secsipid_expire, keyval->s, keyval->len);

	if(ret==0) {
		LM_DBG("identity check: ok\n");
		return 1;
	}

	LM_DBG("identity check: failed\n");
	return ret;
}

/**
 *
 */
static int w_secsipid_check_identity_pubkey(sip_msg_t *msg, char *pkeyval, char *str2)
{
	str keyval = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pkeyval, &keyval)<0) {
		LM_ERR("failed to get keyval parameter\n");
		return -1;
	}

	return ki_secsipid_check_identity_pubkey(msg, &keyval);
}

/**
 *
 */
static int ki_secsipid_check(sip_msg_t *msg, str *sidentity, str *keypath)
{
	int ret = 1;

	if(secsipid_cache_dir.len > 0) {
		_secsipid_papi.SecSIPIDSetFileCacheOptions(secsipid_cache_dir.s,
				secsipid_cache_expire);
	}
	if(secsipid_libopt_list_used==0) {
		str_list_t *sit;
		for(sit=secsipid_libopt_list; sit!=NULL; sit=sit->next) {
			_secsipid_papi.SecSIPIDOptSetV(sit->s.s);
		}
		secsipid_libopt_list_used = 1;
	}
	ret = _secsipid_papi.SecSIPIDCheckFull(sidentity->s, sidentity->len,
			secsipid_expire, keypath->s, secsipid_timeout);

	if(ret==0) {
		LM_DBG("identity check: ok\n");
		return 1;
	}

	LM_DBG("identity check: failed\n");
	return ret;
}

/**
 *
 */
static int w_secsipid_check(sip_msg_t *msg, char *pidentity, char *pkeypath)
{
	str sidentity = STR_NULL;
	str keypath = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)pidentity, &sidentity)<0) {
		LM_ERR("failed to get identity value parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_secsipid_check(msg, &sidentity, &keypath);
}

#define SECSIPID_MODE_VALHDR (1<<0)
#define SECSIPID_MODE_VALVAR (1<<1)
#define SECSIPID_MODE_KEYPATH (1<<2)
#define SECSIPID_MODE_KEYDATA (1<<3)

/**
 *
 */
static int ki_secsipid_add_identity_mode(sip_msg_t *msg, str *origtn, str *desttn,
			str *attest, str *origid, str *x5u, str *keyinfo, int mode)
{
	str ibody = STR_NULL;
	str hdr = STR_NULL;
	sr_lump_t *anchor = NULL;

	if(secsipid_libopt_list_used==0) {
		str_list_t *sit;
		for(sit=secsipid_libopt_list; sit!=NULL; sit=sit->next) {
			_secsipid_papi.SecSIPIDOptSetV(sit->s.s);
		}
		secsipid_libopt_list_used = 1;
	}

	if(mode&SECSIPID_MODE_KEYDATA) {
		ibody.len = _secsipid_papi.SecSIPIDGetIdentityPrvKey(origtn->s, desttn->s,
				attest->s, origid->s, x5u->s, keyinfo->s, &ibody.s);
	} else {
		ibody.len = _secsipid_papi.SecSIPIDGetIdentity(origtn->s, desttn->s,
				attest->s, origid->s, x5u->s, keyinfo->s, &ibody.s);
	}

	if(mode&SECSIPID_MODE_VALVAR) {
		_secsipid_data.ret = ibody.len;
	}

	if(ibody.len<=0) {
		LM_ERR("failed to get identity header body (%d)\n", ibody.len);
		goto error;
	}

	LM_DBG("identity value: %.*s\n", ibody.len, ibody.s);

	if(mode&SECSIPID_MODE_VALVAR) {
		if(_secsipid_data.value.s) {
			free(_secsipid_data.value.s);
		}
		_secsipid_data.value = ibody;
		return 1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
		LM_ERR("error while parsing message\n");
		goto error;
	}

	hdr.len = SECSIPID_HDR_IDENTITY_LEN + 1 + 1 + ibody.len + 2;
	hdr.s = (char*)pkg_malloc(hdr.len + 1);
	if(hdr.s==NULL) {
		PKG_MEM_ERROR;
		goto error;
	}
	memcpy(hdr.s, SECSIPID_HDR_IDENTITY, SECSIPID_HDR_IDENTITY_LEN);
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN) = ':';
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN + 1) = ' ';

	memcpy(hdr.s + SECSIPID_HDR_IDENTITY_LEN + 2, ibody.s, ibody.len);
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN + ibody.len + 2) = '\r';
	*(hdr.s + SECSIPID_HDR_IDENTITY_LEN + ibody.len + 3) = '\n';

	/* anchor after last header */
	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if((anchor==NULL)
			|| (insert_new_lump_before(anchor, hdr.s, hdr.len, 0) == 0)) {
		LM_ERR("cannot insert identity header\n");
		pkg_free(hdr.s);
		goto error;
	}

	if(ibody.s) {
		free(ibody.s);
	}
	return 1;

error:
	if(ibody.s) {
		free(ibody.s);
	}
	return -1;
}

/**
 *
 */
static int ki_secsipid_add_identity(sip_msg_t *msg, str *origtn, str *desttn,
			str *attest, str *origid, str *x5u, str *keypath)
{
	return ki_secsipid_add_identity_mode(msg, origtn, desttn,
			attest, origid, x5u, keypath,
			SECSIPID_MODE_VALHDR|SECSIPID_MODE_KEYPATH);
}

/**
 *
 */
static int w_secsipid_add_identity(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeypath)
{
	str origtn = STR_NULL;
	str desttn = STR_NULL;
	str attest = STR_NULL;
	str origid = STR_NULL;
	str x5u = STR_NULL;
	str keypath = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)porigtn, &origtn)<0) {
		LM_ERR("failed to get origtn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pdesttn, &desttn)<0) {
		LM_ERR("failed to get desttn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pattest, &attest)<0) {
		LM_ERR("failed to get attest parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)porigid, &origid)<0) {
		LM_ERR("failed to get origid parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)px5u, &x5u)<0) {
		LM_ERR("failed to get x5u parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_secsipid_add_identity_mode(msg, &origtn, &desttn,
			&attest, &origid, &x5u, &keypath,
			SECSIPID_MODE_VALHDR|SECSIPID_MODE_KEYPATH);
}

/**
 *
 */
static int ki_secsipid_build_identity(sip_msg_t *msg, str *origtn, str *desttn,
			str *attest, str *origid, str *x5u, str *keypath)
{
	if(_secsipid_data.value.s) {
		free(_secsipid_data.value.s);
	}
	memset(&_secsipid_data, 0, sizeof(secsipid_data_t));

	return ki_secsipid_add_identity_mode(msg, origtn, desttn,
			attest, origid, x5u, keypath,
			SECSIPID_MODE_VALVAR|SECSIPID_MODE_KEYPATH);
}

/**
 *
 */
static int w_secsipid_build_identity(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeypath)
{
	str origtn = STR_NULL;
	str desttn = STR_NULL;
	str attest = STR_NULL;
	str origid = STR_NULL;
	str x5u = STR_NULL;
	str keypath = STR_NULL;

	if(_secsipid_data.value.s) {
		free(_secsipid_data.value.s);
	}
	memset(&_secsipid_data, 0, sizeof(secsipid_data_t));

	if(fixup_get_svalue(msg, (gparam_t*)porigtn, &origtn)<0) {
		LM_ERR("failed to get origtn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pdesttn, &desttn)<0) {
		LM_ERR("failed to get desttn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pattest, &attest)<0) {
		LM_ERR("failed to get attest parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)porigid, &origid)<0) {
		LM_ERR("failed to get origid parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)px5u, &x5u)<0) {
		LM_ERR("failed to get x5u parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_secsipid_add_identity_mode(msg, &origtn, &desttn,
			&attest, &origid, &x5u, &keypath,
			SECSIPID_MODE_VALVAR|SECSIPID_MODE_KEYPATH);
}

/**
 *
 */
static int ki_secsipid_build_identity_prvkey(sip_msg_t *msg, str *origtn, str *desttn,
			str *attest, str *origid, str *x5u, str *keydata)
{
	if(_secsipid_data.value.s) {
		free(_secsipid_data.value.s);
	}
	memset(&_secsipid_data, 0, sizeof(secsipid_data_t));

	return ki_secsipid_add_identity_mode(msg, origtn, desttn,
			attest, origid, x5u, keydata,
			SECSIPID_MODE_VALVAR|SECSIPID_MODE_KEYDATA);
}

/**
 *
 */
static int w_secsipid_build_identity_prvkey(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeydata)
{
	str origtn = STR_NULL;
	str desttn = STR_NULL;
	str attest = STR_NULL;
	str origid = STR_NULL;
	str x5u = STR_NULL;
	str keydata = STR_NULL;

	if(_secsipid_data.value.s) {
		free(_secsipid_data.value.s);
	}
	memset(&_secsipid_data, 0, sizeof(secsipid_data_t));

	if(fixup_get_svalue(msg, (gparam_t*)porigtn, &origtn)<0) {
		LM_ERR("failed to get origtn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pdesttn, &desttn)<0) {
		LM_ERR("failed to get desttn parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pattest, &attest)<0) {
		LM_ERR("failed to get attest parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)porigid, &origid)<0) {
		LM_ERR("failed to get origid parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)px5u, &x5u)<0) {
		LM_ERR("failed to get x5u parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)pkeydata, &keydata)<0) {
		LM_ERR("failed to get keydata parameter\n");
		return -1;
	}

	return ki_secsipid_add_identity_mode(msg, &origtn, &desttn,
			&attest, &origid, &x5u, &keydata,
			SECSIPID_MODE_VALVAR|SECSIPID_MODE_KEYDATA);
}

/**
 *
 */
static int ki_secsipid_sign(sip_msg_t *msg, str *sheaders, str *spayload,
		str *keypath)
{
	str ibody = STR_NULL;

	if(secsipid_libopt_list_used==0) {
		str_list_t *sit;
		for(sit=secsipid_libopt_list; sit!=NULL; sit=sit->next) {
			_secsipid_papi.SecSIPIDOptSetV(sit->s.s);
		}
		secsipid_libopt_list_used = 1;
	}

	ibody.len = _secsipid_papi.SecSIPIDSignJSONHP(sheaders->s, spayload->s,
			keypath->s, &ibody.s);

	_secsipid_data.ret = ibody.len;

	if(ibody.len<=0) {
		LM_ERR("failed to get identity value (%d)\n", ibody.len);
		goto error;
	}

	LM_DBG("identity value: %.*s\n", ibody.len, ibody.s);

	if(_secsipid_data.value.s) {
		free(_secsipid_data.value.s);
	}
	_secsipid_data.value = ibody;

	return 1;

error:
	if(ibody.s) {
		free(ibody.s);
	}
	return -1;
}

/**
 *
 */
static int w_secsipid_sign(sip_msg_t *msg, char *phdrs, char *ppayload, char *pkeypath)
{
	str shdrs = STR_NULL;
	str spayload = STR_NULL;
	str keypath = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t*)phdrs, &shdrs)<0) {
		LM_ERR("failed to get JSON headers parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)ppayload, &spayload)<0) {
		LM_ERR("failed to get JSON payload parameter\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t*)pkeypath, &keypath)<0) {
		LM_ERR("failed to get keypath parameter\n");
		return -1;
	}

	return ki_secsipid_sign(msg, &shdrs, &spayload, &keypath);
}

/**
 *
 */
static str _secsipid_get_url_val = STR_NULL;

/**
 *
 */
static sr_kemi_xval_t _sr_kemi_secsipid_xval = {0};

/**
 *
 */
static sr_kemi_xval_t* ki_secsipid_get_url(sip_msg_t *msg, str *surl)
{
	int r;

	memset(&_sr_kemi_secsipid_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_secsipid_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_secsipid_xval;
	}

	if(_secsipid_get_url_val.s != NULL) {
		free(_secsipid_get_url_val.s);
		_secsipid_get_url_val.len = 0;
	}

	if(secsipid_cache_dir.len > 0) {
		_secsipid_papi.SecSIPIDSetFileCacheOptions(secsipid_cache_dir.s,
				secsipid_cache_expire);
	}
	if(secsipid_libopt_list_used==0) {
		str_list_t *sit;
		for(sit=secsipid_libopt_list; sit!=NULL; sit=sit->next) {
			_secsipid_papi.SecSIPIDOptSetV(sit->s.s);
		}
		secsipid_libopt_list_used = 1;
	}
	r = _secsipid_papi.SecSIPIDGetURLContent(surl->s, secsipid_timeout,
			&_secsipid_get_url_val.s,
			&_secsipid_get_url_val.len);
	if(r!=0) {
		sr_kemi_xval_null(&_sr_kemi_secsipid_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_secsipid_xval;
	}

	_sr_kemi_secsipid_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_secsipid_xval.v.s = _secsipid_get_url_val;

	return &_sr_kemi_secsipid_xval;
}

/**
 *
 */
static int w_secsipid_get_url(sip_msg_t *msg, char *purl, char *povar)
{
	int ret;
	pv_spec_t *ovar;
	pv_value_t val;
	str surl = {NULL, 0};

	if(fixup_get_svalue(msg, (gparam_t*)purl, &surl)<0) {
		LM_ERR("failed to get url parameter\n");
		return -1;
	}
	if(_secsipid_get_url_val.s != NULL) {
		free(_secsipid_get_url_val.s);
		_secsipid_get_url_val.len = 0;
	}

	if(secsipid_cache_dir.len > 0) {
		_secsipid_papi.SecSIPIDSetFileCacheOptions(secsipid_cache_dir.s,
				secsipid_cache_expire);
	}
	ret = _secsipid_papi.SecSIPIDGetURLContent(surl.s, secsipid_timeout,
			&_secsipid_get_url_val.s, &_secsipid_get_url_val.len);
	if(ret!=0) {
		return ret;
	}
	ovar = (pv_spec_t *)povar;

	val.rs = _secsipid_get_url_val;
	val.flags = PV_VAL_STR;
	if(ovar->setf) {
		ovar->setf(msg, &ovar->pvp, (int)EQ_T, &val);
	} else {
		LM_WARN("target pv is not writable\n");
		return -1;
	}

	return 1;
}

/**
 *
 */
static int secsipid_libopt_param(modparam_t type, void *val)
{
	str_list_t *sit;

	if(val==NULL || ((str*)val)->s==NULL || ((str*)val)->len==0) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	sit = (str_list_t*)pkg_mallocxz(sizeof(str_list_t));
	if(sit==NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	sit->s = *((str*)val);
	sit->next = secsipid_libopt_list;
	secsipid_libopt_list = sit;

	return 0;
}

/**
 *
 */
static int pv_get_secsipid(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	switch(param->pvn.u.isname.name.n) {
		case 0: /* value */
			if(_secsipid_data.value.s==NULL || _secsipid_data.value.len<=0) {
				return pv_get_null(msg, param, res);
			}
			return pv_get_strval(msg, param, res, &_secsipid_data.value);
		case 1: /* ret code */
			return pv_get_sintval(msg, param, res, _secsipid_data.ret);
	}
	return pv_get_null(msg, param, res);
}

/**
 *
 */
static int pv_parse_secsipid_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	/* attributes not related to dst of reply get an id starting with 20 */
	switch(in->len) {
		case 3:
			if(strncmp(in->s, "val", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "ret", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;

		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV secsipid key: %.*s\n", in->len, in->s);
	return -1;

}


/**
 *
 */
static sr_kemi_xval_t* ki_secsipid_get_val(sip_msg_t *msg)
{
	memset(&_sr_kemi_secsipid_xval, 0, sizeof(sr_kemi_xval_t));
	if(_secsipid_data.value.s==NULL || _secsipid_data.value.len<=0) {
		sr_kemi_xval_null(&_sr_kemi_secsipid_xval, SR_KEMI_XVAL_NULL_EMPTY);
		return &_sr_kemi_secsipid_xval;
	}
	_sr_kemi_secsipid_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_secsipid_xval.v.s = _secsipid_data.value;

	return &_sr_kemi_secsipid_xval;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_secsipid_exports[] = {
	{ str_init("secsipid"), str_init("secsipid_check_identity"),
		SR_KEMIP_INT, ki_secsipid_check_identity,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("secsipid"), str_init("secsipid_check_identity_pubkey"),
		SR_KEMIP_INT, ki_secsipid_check_identity_pubkey,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("secsipid"), str_init("secsipid_check"),
		SR_KEMIP_INT, ki_secsipid_check,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("secsipid"), str_init("secsipid_add_identity"),
		SR_KEMIP_INT, ki_secsipid_add_identity,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR }
	},
	{ str_init("secsipid"), str_init("secsipid_build_identity"),
		SR_KEMIP_INT, ki_secsipid_build_identity,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR }
	},
	{ str_init("secsipid"), str_init("secsipid_build_identity_prvkey"),
		SR_KEMIP_INT, ki_secsipid_build_identity_prvkey,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR }
	},
	{ str_init("secsipid"), str_init("secsipid_get_val"),
		SR_KEMIP_XVAL, ki_secsipid_get_val,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("secsipid"), str_init("secsipid_get_url"),
		SR_KEMIP_XVAL, ki_secsipid_get_url,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_secsipid_exports);
	return 0;
}
