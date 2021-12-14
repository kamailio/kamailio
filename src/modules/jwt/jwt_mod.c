/**
 * Copyright (C) 2021 Daniel-Constantin Mierla (asipto.com)
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

#include <jwt.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_param.h"


MODULE_VERSION

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_jwt_generate_4(sip_msg_t* msg, char* pkey, char* palg, char* pclaims, char* pheaders);
static int w_jwt_generate_3(sip_msg_t* msg, char* pkey, char* palg, char* pclaims);
static int w_jwt_verify(sip_msg_t* msg, char* pkey, char* palg, char* pclaims,
		char *pjwtval);

static int _jwt_key_mode = 0;

static str _jwt_result = STR_NULL;
static unsigned int _jwt_verify_status = 0;

typedef struct jwt_fcache {
	str fname;
	str fdata;
	struct jwt_fcache *next;
} jwt_fcache_t;

static jwt_fcache_t *_jwt_fcache_list = NULL;

static cmd_export_t cmds[]={
	{"jwt_generate", (cmd_function)w_jwt_generate_4, 4,
		fixup_spve_all, 0, ANY_ROUTE},
	{"jwt_generate", (cmd_function)w_jwt_generate_3, 3,
		fixup_spve_all, 0, ANY_ROUTE},
	{"jwt_verify", (cmd_function)w_jwt_verify, 4,
		fixup_spve_all, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{ "key_mode", PARAM_INT, &_jwt_key_mode },

	{ 0, 0, 0 }
};

static int jwt_pv_get(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int jwt_pv_parse_name(pv_spec_t *sp, str *in);
static pv_export_t mod_pvs[] = {
	{ {"jwt",  sizeof("jwt")-1}, PVT_OTHER,  jwt_pv_get,    0,
			jwt_pv_parse_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"jwt",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};


/**
 * @brief Initialize crypto module function
 */
static int mod_init(void)
{
	return 0;
}

/**
 * @brief Initialize crypto module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

/**
 *
 */
static int jwt_fcache_get(str *key, str *kdata)
{
	jwt_fcache_t *fc = NULL;

	if(_jwt_key_mode!=1) {
		return -1;
	}
	for(fc=_jwt_fcache_list; fc!=NULL; fc=fc->next) {
		if(fc->fname.len==key->len
				&& strncmp(fc->fname.s, key->s, key->len)==0) {
			LM_DBG("file found in cache: %.*s\n", key->len, key->s);
			*kdata = fc->fdata;
			break;
		}
	}
	return 0;
}

/**
 *
 */
static int jwt_fcache_add(str *key, str *kdata)
{
	jwt_fcache_t *fc = NULL;

	if(_jwt_key_mode!=1) {
		return -1;
	}
	fc = (jwt_fcache_t*)pkg_malloc(sizeof(jwt_fcache_t) + key->len
			+ kdata->len + 2);
	if(fc==NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(fc, 0, sizeof(jwt_fcache_t) + key->len + kdata->len + 2);
	fc->fname.s = (char*)fc + sizeof(jwt_fcache_t);
	fc->fname.len = key->len;
	memcpy(fc->fname.s, key->s, key->len);
	fc->fdata.s = fc->fname.s + fc->fname.len + 1;
	fc->fdata.len = kdata->len;
	memcpy(fc->fdata.s, kdata->s, kdata->len);
	fc->next = _jwt_fcache_list;
	_jwt_fcache_list = fc;

	return 0;
}

/**
 *
 */
static int ki_jwt_generate_hdrs(sip_msg_t* msg, str *key, str *alg, str *claims,
		str *headers)
{
	str dupclaims = STR_NULL;
	str sparams = STR_NULL;
	str dupheaders = STR_NULL;
	str sheaders = STR_NULL;
	str kdata = STR_NULL;
	jwt_alg_t valg = JWT_ALG_NONE;
	time_t iat;
	FILE *fpk = NULL;
	unsigned char keybuf[10240];
	size_t keybuf_len = 0;
	param_t* params_list = NULL;
	param_t* headers_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	param_t *header = NULL;

	int ret = 0;
	jwt_t *jwt = NULL;
	long lval = 0;

	if(key==NULL || key->s==NULL || alg==NULL || alg->s==NULL
			|| claims==NULL || claims->s==NULL || claims->len<=0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	if(_jwt_result.s != NULL) {
		jwt_free_str(_jwt_result.s);
		_jwt_result.s = NULL;
		_jwt_result.len = 0;
	}
	valg = jwt_str_alg(alg->s);
	if (valg == JWT_ALG_INVAL) {
		LM_ERR("not supported algorithm: %s\n", alg->s);
		return -1;
	}
	if(pkg_str_dup(&dupclaims, claims)<0) {
		LM_ERR("failed to duplicate claims\n");
		return -1;
	}
	if (headers!=NULL) {
		if(pkg_str_dup(&dupheaders, headers)<0) {
			LM_ERR("failed to duplicate headers\n");
			return -1;
		}
	}
	jwt_fcache_get(key, &kdata);
	if(kdata.s==NULL) {
		fpk= fopen(key->s, "r");
		if(fpk==NULL) {
			LM_ERR("failed to read key file: %s\n", key->s);
			goto error;
		}
		keybuf_len = fread(keybuf, 1, sizeof(keybuf), fpk);
		fclose(fpk);
		if(keybuf_len==0) {
			LM_ERR("unable to read key file content: %s\n", key->s);
			goto error;
		}
		keybuf[keybuf_len] = '\0';
		kdata.s = (char*)keybuf;
		kdata.len = (int)keybuf_len;
		jwt_fcache_add(key, &kdata);
	}
	sparams = dupclaims;
	if(sparams.s[sparams.len-1]==';') {
		sparams.len--;
	}
	if (headers!=NULL) {
		sheaders = dupheaders;
		if(sheaders.s[sheaders.len-1]==';') {
			sheaders.len--;
		}
	}

	if (parse_params(&sparams, CLASS_ANY, &phooks, &params_list)<0) {
		LM_ERR("failed to parse claims\n");
		goto error;
	}

	if (headers!=NULL && headers->s!=NULL && headers->len>0) {
		if (parse_params(&sheaders, CLASS_ANY, &phooks, &headers_list)<0) {
			LM_ERR("failed to parse headers\n");
			goto error;
		}
	}

	ret = jwt_new(&jwt);
	if (ret != 0 || jwt == NULL) {
		LM_ERR("failed to initialize jwt\n");
		goto error;
	}

	iat = time(NULL);

	ret = jwt_add_grant_int(jwt, "iat", iat);
	if(ret != 0) {
		LM_ERR("failed to add iat grant\n");
		goto error;
	}
	for (pit = params_list; pit; pit=pit->next) {
		if(pit->name.len>0 && pit->body.len>0) {
			pit->name.s[pit->name.len] = '\0';
			pit->body.s[pit->body.len] = '\0';
			if(pit->body.s[-1] == '\"' || pit->body.s[-1] == '\'') {
				ret = jwt_add_grant(jwt, pit->name.s, pit->body.s);
			} else if(str2slong(&pit->body, &lval)==0) {
				ret = jwt_add_grant_int(jwt, pit->name.s, lval);
			} else {
				ret = jwt_add_grant(jwt, pit->name.s, pit->body.s);
			}
			if(ret != 0) {
				LM_ERR("failed to add %s grant\n", pit->name.s);
				goto error;
			}
		}
	}

	for (header = headers_list; header; header=header->next) {
		if(header->name.len>0 && header->body.len>0) {
			header->name.s[header->name.len] = '\0';
			header->body.s[header->body.len] = '\0';
			if(header->body.s[-1] == '\"' || header->body.s[-1] == '\'') {
				ret = jwt_add_header(jwt, header->name.s, header->body.s);
			} else if(str2slong(&header->body, &lval)==0) {
				ret = jwt_add_header_int(jwt, header->name.s, lval);
			} else {
				ret = jwt_add_header(jwt, header->name.s, header->body.s);
			}
			if(ret != 0) {
				LM_ERR("failed to add %s header\n", header->name.s);
				goto error;
			}
		}
	}

	ret = jwt_set_alg(jwt, valg, (unsigned char*)kdata.s, (size_t)kdata.len);
	if (ret != 0) {
		LM_ERR("failed to set algorithm and key\n");
		goto error;
	}

	_jwt_result.s = jwt_encode_str(jwt);
	_jwt_result.len = strlen(_jwt_result.s);

	free_params(params_list);
	pkg_free(dupclaims.s);
	free_params(headers_list);
	pkg_free(dupheaders.s);
	jwt_free(jwt);

	return 1;

error:
	if(params_list!=NULL) {
		free_params(params_list);
	}
	if(dupclaims.s!=NULL) {
		pkg_free(dupclaims.s);
	}
	if(headers_list!=NULL) {
		free_params(headers_list);
	}
	if(dupheaders.s!=NULL) {
		pkg_free(dupheaders.s);
	}
	if(jwt!=NULL) {
		jwt_free(jwt);
	}
	return -1;
}

/**
 *
 */
static int ki_jwt_generate(sip_msg_t* msg, str *key, str *alg, str *claims)
{
	return ki_jwt_generate_hdrs(msg, key, alg, claims, NULL);
}

/**
 *
 */
static int w_jwt_generate_3(sip_msg_t* msg, char* pkey, char* palg, char* pclaims)
{
	str skey = STR_NULL;
	str salg = STR_NULL;
	str sclaims = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)pkey, &skey) != 0) {
		LM_ERR("cannot get path to the key file\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)palg, &salg) != 0) {
		LM_ERR("cannot get algorithm value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pclaims, &sclaims) != 0) {
		LM_ERR("cannot get claims value\n");
		return -1;
	}

	return ki_jwt_generate(msg, &skey, &salg, &sclaims);
}

/**
 *
 */
static int w_jwt_generate_4(sip_msg_t* msg, char* pkey, char* palg, char* pclaims, char* pheaders)
{
	str skey = STR_NULL;
	str salg = STR_NULL;
	str sclaims = STR_NULL;
	str sheaders = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)pkey, &skey) != 0) {
		LM_ERR("cannot get path to the key file\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)palg, &salg) != 0) {
		LM_ERR("cannot get algorithm value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pclaims, &sclaims) != 0) {
		LM_ERR("cannot get claims value\n");
		return -1;
	}

	if (fixup_get_svalue(msg, (gparam_t*)pheaders, &sheaders) != 0) {
		LM_ERR("cannot get headers value\n");
		return -1;
	}

	return ki_jwt_generate_hdrs(msg, &skey, &salg, &sclaims, &sheaders);
}

/**
 *
 */
static int ki_jwt_verify(sip_msg_t* msg, str *key, str *alg, str *claims,
		str *jwtval)
{
	str dupclaims = STR_NULL;
	jwt_alg_t valg = JWT_ALG_NONE;
	str kdata = STR_NULL;
	time_t iat;
	FILE *fpk = NULL;
	unsigned char keybuf[10240];
	size_t keybuf_len = 0;
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit = NULL;
	int ret = 0;
	jwt_t *jwt = NULL;
	jwt_valid_t *jwt_valid = NULL;
	str sparams = STR_NULL;
	long lval = 0;

	if(key==NULL || key->s==NULL || alg==NULL || alg->s==NULL
			|| claims==NULL || claims->s==NULL || claims->len<=0
			|| jwtval==NULL || jwtval->s==NULL || jwtval->len<=0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	_jwt_verify_status = 0;

	valg = jwt_str_alg(alg->s);
	if (valg == JWT_ALG_INVAL) {
		LM_ERR("not supported algorithm: %s\n", alg->s);
		return -1;
	}
	if(pkg_str_dup(&dupclaims, claims)<0) {
		LM_ERR("failed to duplicate claims\n");
		return -1;
	}
	jwt_fcache_get(key, &kdata);
	if(kdata.s==NULL) {
		fpk= fopen(key->s, "r");
		if(fpk==NULL) {
			LM_ERR("failed to read key file: %s\n", key->s);
			goto error;
		}
		keybuf_len = fread(keybuf, 1, sizeof(keybuf), fpk);
		fclose(fpk);
		if(keybuf_len==0) {
			LM_ERR("unable to read key file content: %s\n", key->s);
			goto error;
		}
		keybuf[keybuf_len] = '\0';
		kdata.s = (char*)keybuf;
		kdata.len = (int)keybuf_len;
		jwt_fcache_add(key, &kdata);
	}
	sparams = dupclaims;
	if(sparams.s[sparams.len-1]==';') {
		sparams.len--;
	}
	if (parse_params(&sparams, CLASS_ANY, &phooks, &params_list)<0) {
		LM_ERR("failed to parse claims\n");
		goto error;
	}

	ret = jwt_valid_new(&jwt_valid, valg);
	if (ret != 0 || jwt_valid == NULL) {
		LM_ERR("failed to initialize jwt valid\n");
		goto error;
	}

	iat = time(NULL);
	jwt_valid_set_headers(jwt_valid, 1);
	jwt_valid_set_now(jwt_valid, iat);

	for (pit = params_list; pit; pit=pit->next) {
		if(pit->name.len>0 && pit->body.len>0) {
			pit->name.s[pit->name.len] = '\0';
			pit->body.s[pit->body.len] = '\0';
			if(pit->body.s[-1] == '\"' || pit->body.s[-1] == '\'') {
				ret = jwt_valid_add_grant(jwt_valid, pit->name.s, pit->body.s);
			} else if(str2slong(&pit->body, &lval)==0) {
				ret = jwt_valid_add_grant_int(jwt_valid, pit->name.s, lval);
			} else {
				ret = jwt_valid_add_grant(jwt_valid, pit->name.s, pit->body.s);
			}
			if(ret != 0) {
				LM_ERR("failed to add %s valid grant\n", pit->name.s);
				goto error;
			}
		}
	}

	ret = jwt_decode(&jwt, jwtval->s, (unsigned char*)kdata.s, (size_t)kdata.len);
	if (ret!=0 || jwt==NULL) {
		LM_ERR("failed to decode jwt value\n");
		goto error;
	}
	if (jwt_validate(jwt, jwt_valid) != 0) {
		_jwt_verify_status = jwt_valid_get_status(jwt_valid);
		LM_ERR("failed to validate jwt: %08x\n", _jwt_verify_status);
		goto error;
	}

	free_params(params_list);
	pkg_free(dupclaims.s);
	jwt_free(jwt);
	jwt_valid_free(jwt_valid);

	return 1;

error:
	if(params_list!=NULL) {
		free_params(params_list);
	}
	if(dupclaims.s!=NULL) {
		pkg_free(dupclaims.s);
	}
	if(jwt!=NULL) {
		jwt_free(jwt);
	}
	if(jwt_valid!=NULL) {
		jwt_valid_free(jwt_valid);
	}
	return -1;
}

/**
 *
 */
static int w_jwt_verify(sip_msg_t* msg, char* pkey, char* palg, char* pclaims,
		char *pjwtval)
{
	str skey = STR_NULL;
	str salg = STR_NULL;
	str sclaims = STR_NULL;
	str sjwtval = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)pkey, &skey) != 0) {
		LM_ERR("cannot get path to the key file\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)palg, &salg) != 0) {
		LM_ERR("cannot get algorithm value\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pclaims, &sclaims) != 0) {
		LM_ERR("cannot get claims value\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pjwtval, &sjwtval) != 0) {
		LM_ERR("cannot get jwt value\n");
		return -1;
	}

	return ki_jwt_verify(msg, &skey, &salg, &sclaims, &sjwtval);
}

/**
 *
 */
static int jwt_pv_get(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			if(_jwt_result.s==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_jwt_result);
		case 1:
			return pv_get_uintval(msg, param, res, _jwt_verify_status);
		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int jwt_pv_parse_name(pv_spec_t *sp, str *in)
{
	if(in->len==3 && strncmp(in->s, "val", 3)==0) {
		sp->pvp.pvn.u.isname.name.n = 0;
	} else if(in->len==6 && strncmp(in->s, "status", 6)==0) {
		sp->pvp.pvn.u.isname.name.n = 1;
	} else {
		LM_ERR("unknown inner name [%.*s]\n", in->len, in->s);
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_jwt_exports[] = {
	{ str_init("jwt"), str_init("jwt_generate"),
		SR_KEMIP_INT, ki_jwt_generate,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jwt"), str_init("jwt_generate_hdrs"),
		SR_KEMIP_INT, ki_jwt_generate_hdrs,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("jwt"), str_init("jwt_verify"),
		SR_KEMIP_INT, ki_jwt_verify,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_jwt_exports);
	return 0;
}
