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
#include "../../core/lvalue.h"
#include "../../core/kemi.h"

#include "secsipid_papi.h"

MODULE_VERSION

static int secsipid_expire = 300;
static int secsipid_timeout = 5;

static int secsipid_cache_expire = 3600;
static str secsipid_cache_dir = str_init("");
static str secsipid_modproc = str_init("secsipid_proc.so");

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_secsipid_check_identity(sip_msg_t *msg, char *pkeypath, char *str2);
static int w_secsipid_check_identity_pubkey(sip_msg_t *msg, char *pkeyval, char *str2);
static int w_secsipid_add_identity(sip_msg_t *msg, char *porigtn, char *pdesttn,
			char *pattest, char *porigid, char *px5u, char *pkeypath);
static int w_secsipid_get_url(sip_msg_t *msg, char *purl, char *pout);

secsipid_papi_t _secsipid_papi = {0};

/* clang-format off */
static cmd_export_t cmds[]={
	{"secsipid_check_identity", (cmd_function)w_secsipid_check_identity, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"secsipid_check_identity_pubkey", (cmd_function)w_secsipid_check_identity_pubkey, 1,
		fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
	{"secsipid_add_identity", (cmd_function)w_secsipid_add_identity, 6,
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
	{0, 0, 0}
};

struct module_exports exports = {
	"secsipid",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
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
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	void *handle = NULL;
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
	handle = dlopen(modpath, RTLD_NOW); /* resolve all symbols now */
	if (handle==0) {
		LM_ERR("could not open module <%s>: %s\n", modpath, dlerror());
		goto error;
	}
	/* launch register */
	bind_f = (secsipid_proc_bind_f)dlsym(handle, "secsipid_proc_bind");
	if (((errstr=(char*)dlerror())==NULL) && bind_f!=NULL) {
		/* version control */
		if (!ksr_version_control(handle, modpath)) {
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
	ret = _secsipid_papi.SecSIPIDCheckFull(ibody.s, ibody.len, secsipid_expire,
			keypath->s, secsipid_timeout);

	if(ret==0) {
		LM_DBG("identity check: ok\n");
		return 1;
	}

	LM_DBG("identity check: failed\n");
	return -1;
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

	ibody = hf->body;

	ret = _secsipid_papi.SecSIPIDCheckFullPubKey(ibody.s, ibody.len,
			secsipid_expire, keyval->s, keyval->len);

	if(ret==0) {
		LM_DBG("identity check: ok\n");
		return 1;
	}

	LM_DBG("identity check: failed\n");
	return -1;
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
static int ki_secsipid_add_identity(sip_msg_t *msg, str *origtn, str *desttn,
			str *attest, str *origid, str *x5u, str *keypath)
{
	str ibody = STR_NULL;
	str hdr = STR_NULL;
	sr_lump_t *anchor = NULL;

	ibody.len = _secsipid_papi.SecSIPIDGetIdentity(origtn->s, desttn->s,
			attest->s, origid->s, x5u->s, keypath->s, &ibody.s);

	if(ibody.len<=0) {
		goto error;
	}

	LM_DBG("appending identity: %.*s\n", ibody.len, ibody.s);
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

	return ki_secsipid_add_identity(msg, &origtn, &desttn,
			&attest, &origid, &x5u, &keypath);
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
	int r;
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
	r = _secsipid_papi.SecSIPIDGetURLContent(surl.s, secsipid_timeout,
			&_secsipid_get_url_val.s, &_secsipid_get_url_val.len);
	if(r!=0) {
		return -1;
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
/* clang-format off */
static sr_kemi_t sr_kemi_secsipid_exports[] = {
	{ str_init("secsipid"), str_init("secsipid_check_identity"),
		SR_KEMIP_INT, ki_secsipid_check_identity,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("secsipid"), str_init("secsipid_add_identity"),
		SR_KEMIP_INT, ki_secsipid_add_identity,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR }
	},
	{ str_init("secsipid"), str_init("secsipid_check_identity_pubkey"),
		SR_KEMIP_INT, ki_secsipid_check_identity_pubkey,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
