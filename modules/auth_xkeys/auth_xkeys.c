/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../trim.h"
#include "../../pvapi.h"
#include "../../data_lump.h"
#include "../../mem/shm_mem.h"
#include "../../parser/hf.h"
#include "../../parser/parse_param.h"
#include "../../parser/msg_parser.h"
#include "../../lib/srutils/shautils.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"

#include "auth_xkeys.h"

typedef struct _auth_xkey {
	str kid;
	str kname;
	str kvalue;
	time_t kexpires;
	struct _auth_xkey *next;
	struct _auth_xkey *next_id;
} auth_xkey_t;

static auth_xkey_t **_auth_xkeys_list = NULL;

/**
 *
 */
int auth_xkeys_list_init(void)
{
	if(_auth_xkeys_list!=NULL)
		return 0;
	_auth_xkeys_list = shm_malloc(sizeof(auth_xkey_t));
	if(_auth_xkeys_list==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(_auth_xkeys_list, 0, sizeof(auth_xkey_t*));
	return 0;
}

/**
 *
 */
int authx_xkey_insert(auth_xkey_t *nkey)
{
	auth_xkey_t *ukey;
	auth_xkey_t *itp;
	auth_xkey_t *itc;
	int ksize;
	char *p;

	if(auth_xkeys_list_init())
		return -1;
	if(nkey==NULL)
		return -1;

	ksize = sizeof(auth_xkey_t) + nkey->kid.len + nkey->kname.len
		+ nkey->kvalue.len + 3;
	ukey = (auth_xkey_t*)shm_malloc(ksize);
	if(ukey==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(ukey, 0, ksize);
	p = (char*)ukey + sizeof(auth_xkey_t);

	ukey->kid.len = nkey->kid.len;
	ukey->kid.s = p;
	strncpy(ukey->kid.s, nkey->kid.s, ukey->kid.len);
	ukey->kid.s[ukey->kid.len] = '\0';
	p += ukey->kid.len + 1;

	ukey->kname.len = nkey->kname.len;
	ukey->kname.s = p;
	strncpy(ukey->kname.s, nkey->kname.s, ukey->kname.len);
	ukey->kname.s[ukey->kname.len] = '\0';
	p += ukey->kname.len + 1;

	ukey->kvalue.len = nkey->kvalue.len;
	ukey->kvalue.s = p;
	strncpy(ukey->kvalue.s, nkey->kvalue.s, ukey->kvalue.len);
	ukey->kvalue.s[ukey->kvalue.len] = '\0';
	p += ukey->kvalue.len + 1;

	ukey->kexpires = nkey->kexpires;

	if(*_auth_xkeys_list==NULL) {
		*_auth_xkeys_list = ukey;
		return 0;
	}

	itp = NULL;
	for(itc = *_auth_xkeys_list; itc; itc = itc->next_id) {
		if(itc->kid.len==ukey->kid.len
				&& strncasecmp(itc->kid.s, ukey->kid.s, ukey->kid.len)==0)
			break;
		itp = itc;
	}
	if(itc==NULL) {
		/* new id */
		ukey->next_id = *_auth_xkeys_list;
		*_auth_xkeys_list = ukey;
		return 0;
	}

	if(itp!=NULL) {
		itp->next_id = ukey;
	} else {
		*_auth_xkeys_list = ukey;
	}
	ukey->next_id = itc->next_id;
	ukey->next = itc;
	itc->next_id = NULL;
	return 0;
}

/**
 *
 */
int authx_xkey_add_params(str *sparam)
{
	param_t* params_list = NULL;
	param_hooks_t phooks;
	param_t *pit=NULL;
	auth_xkey_t tmp;
	unsigned int uv;

	if (parse_params(sparam, CLASS_ANY, &phooks, &params_list)<0)
		return -1;

	memset(&tmp, 0, sizeof(auth_xkey_t));

	for (pit = params_list; pit; pit=pit->next)
	{
		if (pit->name.len==2
				&& strncasecmp(pit->name.s, "id", 2)==0) {
			tmp.kid = pit->body;
		} else if(pit->name.len==4
				&& strncasecmp(pit->name.s, "name", 4)==0) {
			tmp.kname = pit->body;
		} else if(pit->name.len==5
				&& strncasecmp(pit->name.s, "value", 5)==0) {
			tmp.kvalue = pit->body;
		}  else if(pit->name.len==7
				&& strncasecmp(pit->name.s, "expires", 7)==0) {
			str2int(&pit->body, &uv);
			tmp.kexpires = time(NULL) + uv;
		}
	}
	if(tmp.kid.len<=0 || tmp.kname.len<=0 || tmp.kvalue.len<=0) {
		LM_ERR("invalid parameters (%d/%d/%d)\n", tmp.kid.len,
				tmp.kname.len, tmp.kvalue.len);
		return -1;
	}

	if(authx_xkey_insert(&tmp)<0) {
		LM_ERR("unable to insert the key [%.*s:%.*s]\n",
				tmp.kid.len, tmp.kid.s, tmp.kname.len, tmp.kname.s);
		return -1;
	}

	return 0;
}

/**
 *
 */
int auth_xkeys_add(sip_msg_t* msg, str *hdr, str *key,
		str *alg, str *data)
{
	str xdata;
	auth_xkey_t *itc;
	char xout[SHA512_DIGEST_STRING_LENGTH];
	struct lump* anchor;

	if(_auth_xkeys_list==NULL || *_auth_xkeys_list==NULL) {
		LM_ERR("no stored keys\n");
		return -1;
	}
	if(parse_headers(msg, HDR_EOH_F, 0)<0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	for(itc = *_auth_xkeys_list; itc; itc = itc->next_id) {
		if(itc->kid.len==key->len
				&& strncasecmp(itc->kid.s, key->s, key->len)==0)
			break;
	}
	if(itc==NULL) {
		LM_DBG("no key chain id [%.*s]\n", key->len, key->s);
		return -1;
	}

	xdata.s = pv_get_buffer();
	xdata.len = data->len + itc->kvalue.len + 1;
	if(xdata.len + 1 >= pv_get_buffer_size()) {
		LM_ERR("size of data and key is too big\n");
		return -1;
	}

	strncpy(xdata.s, itc->kvalue.s, itc->kvalue.len);
	xdata.s[itc->kvalue.len] = ':';
	strncpy(xdata.s + itc->kvalue.len + 1, data->s, data->len);
	if(alg->len==6 && strncasecmp(alg->s, "sha256", 6)==0) {
		compute_sha256(xout, (u_int8_t*)xdata.s, xdata.len);
		xdata.len = SHA256_DIGEST_STRING_LENGTH - 1;
	} else if(alg->len==6 && strncasecmp(alg->s, "sha384", 6)==0) {
		compute_sha384(xout, (u_int8_t*)xdata.s, xdata.len);
		xdata.len = SHA384_DIGEST_STRING_LENGTH - 1;
	} else if(alg->len==6 && strncasecmp(alg->s, "sha512", 6)==0) {
		compute_sha512(xout, (u_int8_t*)xdata.s, xdata.len);
		xdata.len = SHA512_DIGEST_STRING_LENGTH - 1;
	} else {
		LM_ERR("unknown algorithm [%.*s]\n", alg->len, alg->s);
		return -1;
	}

	if(xdata.len + hdr->len + 6 >= pv_get_buffer_size()) {
		LM_ERR("size of new header is too big for pv buffer\n");
		return -1;
	}

	strncpy(xdata.s, hdr->s, hdr->len);
	xdata.s[hdr->len] = ':';
	xdata.s[hdr->len+1] = ' ';
	strncpy(xdata.s + hdr->len + 2, xout, xdata.len);
	xdata.len += hdr->len + 2;
	xdata.s[xdata.len] = '\r';
	xdata.s[xdata.len+1] = '\n';
	xdata.s[xdata.len+2] = '\0';
	xdata.len += 2;

	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if(anchor == 0) {
		LM_ERR("can't get anchor\n");
		return -1;
	}
	if (insert_new_lump_before(anchor, xdata.s, xdata.len, 0) == 0) {
		LM_ERR("cannot insert the new header [%.*s]\n", hdr->len, hdr->s);
		return -1;
	}
	return 0;
}

/**
 *
 */
int auth_xkeys_check(sip_msg_t* msg, str *hdr, str *key,
		str *alg, str *data)
{
	hdr_field_t *hf;
	str xdata;
	auth_xkey_t *itc;
	char xout[SHA512_DIGEST_STRING_LENGTH];
	str hbody;

	if(_auth_xkeys_list==NULL || *_auth_xkeys_list==NULL) {
		LM_ERR("no stored keys\n");
		return -1;
	}
	if(parse_headers(msg, HDR_EOH_F, 0)<0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	for (hf=msg->headers; hf; hf=hf->next) {
		if (cmp_hdrname_str(&hf->name, hdr)==0)
			break;
	}
	if(hf==NULL) {
		LM_DBG("no header with name [%.*s]\n", hdr->len, hdr->s);
		return -1;
	}
	if(hf->body.len<=0) {
		LM_DBG("empty header with name [%.*s]\n", hdr->len, hdr->s);
		return -1;
	}
	hbody = hf->body;
	trim(&hbody);
	if(hbody.len!=SHA256_DIGEST_STRING_LENGTH-1
			&& hbody.len!=SHA384_DIGEST_STRING_LENGTH-1
			&& hbody.len!=SHA512_DIGEST_STRING_LENGTH-1) {
		LM_DBG("not maching digest size for [%.*s]\n",
				hf->body.len, hf->body.s);
		return -1;
	}

	for(itc = *_auth_xkeys_list; itc; itc = itc->next_id) {
		if(itc->kid.len==key->len
				&& strncasecmp(itc->kid.s, key->s, key->len)==0)
			break;
	}
	if(itc==NULL) {
		LM_DBG("no key chain id [%.*s]\n", key->len, key->s);
		return -1;
	}
	xdata.s = pv_get_buffer();
	for(; itc; itc = itc->next) {
		xdata.len = data->len + itc->kvalue.len + 1;
		if(xdata.len + 1 >= pv_get_buffer_size()) {
			LM_WARN("size of data and key is too big - ignoring\n");
			continue;
		}
		strncpy(xdata.s, itc->kvalue.s, itc->kvalue.len);
		xdata.s[itc->kvalue.len] = ':';
		strncpy(xdata.s + itc->kvalue.len + 1, data->s, data->len);
		if(alg->len==6 && strncasecmp(alg->s, "sha256", 6)==0) {
			if(hbody.len!=SHA256_DIGEST_STRING_LENGTH-1) {
				LM_DBG("not maching sha256 digest size for [%.*s]\n",
						hf->body.len, hf->body.s);
				return -1;
			}
			compute_sha256(xout, (u_int8_t*)xdata.s, xdata.len);
			if(strncasecmp(xout, hbody.s, hbody.len)==0) {
				LM_DBG("no digest sha256 matched for key [%.*s:%.*s]\n",
						key->len, key->s, itc->kname.len, itc->kname.s);
				return 0;
			}
		} else if(alg->len==6 && strncasecmp(alg->s, "sha384", 6)==0) {
			if(hbody.len!=SHA384_DIGEST_STRING_LENGTH-1) {
				LM_DBG("not maching sha384 digest size for [%.*s]\n",
						hf->body.len, hf->body.s);
				return -1;
			}
			compute_sha384(xout, (u_int8_t*)xdata.s, xdata.len);
			if(strncasecmp(xout, hbody.s, hbody.len)==0) {
				LM_DBG("no digest sha384 matched for key [%.*s:%.*s]\n",
						key->len, key->s, itc->kname.len, itc->kname.s);
				return 0;
			}
		} else if(alg->len==6 && strncasecmp(alg->s, "sha512", 6)==0) {
			if(hbody.len!=SHA512_DIGEST_STRING_LENGTH-1) {
				LM_DBG("not maching sha512 digest size for [%.*s]\n",
						hf->body.len, hf->body.s);
				return -1;
			}
			compute_sha512(xout, (u_int8_t*)xdata.s, xdata.len);
			if(strncasecmp(xout, hbody.s, hbody.len)==0) {
				LM_DBG("no digest sha512 matched for key [%.*s:%.*s]\n",
						key->len, key->s, itc->kname.len, itc->kname.s);
				return 0;
			}
		} else {
			LM_ERR("unknown algorithm [%.*s]\n", alg->len, alg->s);
			return -1;
		}
	}

	LM_DBG("no digest matched for key [%.*s]\n", key->len, key->s);
	return -1;
}


static const char* auth_xkeys_rpc_list_doc[2] = {
	"List existing keys",
	0
};

/*
 * RPC command to list the keys
 */
static void auth_xkeys_rpc_list(rpc_t* rpc, void* ctx)
{
	void* th;
	void* ih;
	void* vh;
	auth_xkey_t *itc;
	auth_xkey_t *itd;

	if(_auth_xkeys_list==NULL || *_auth_xkeys_list==NULL) {
		rpc->fault(ctx, 500, "No keys");
		return;
	}
	/* add entry node */
	if (rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	for(itc = *_auth_xkeys_list; itc; itc = itc->next_id) {
		if(rpc->struct_add(th, "S[",
					"KID", &itc->kid,
					"KEYS",  &ih)<0) {
			rpc->fault(ctx, 500, "Internal error keys array");
			return;
		}

		for(itd=itc; itd; itd = itd->next) {
			if(rpc->struct_add(ih, "{",
						"KEY", &vh)<0) {
				rpc->fault(ctx, 500, "Internal error creating keys data");
				return;
			}
			if(rpc->struct_add(vh, "SDd",
						"NAME",  &itd->kname,
						"VALUE", &itd->kvalue,
						"EXPIRES", itd->kexpires)<0)
			{
				rpc->fault(ctx, 500, "Internal error creating dest struct");
				return;
			}
		}
	}
	return;
}

static const char* auth_xkeys_rpc_set_doc[2] = {
	"Set expires of existing key or add a new key",
	0
};

/*
 * RPC command to set the expires of a key or add a new key
 */
static void auth_xkeys_rpc_set(rpc_t* rpc, void* ctx)
{
	auth_xkey_t tmp;
	auth_xkey_t *itc;

	memset(&tmp, 0, sizeof(auth_xkey_t));

	if(rpc->scan(ctx, ".SSSd", &tmp.kid, &tmp.kname,
				&tmp.kvalue, &tmp.kexpires)<4)
	{
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	for(itc = *_auth_xkeys_list; itc; itc = itc->next_id) {
		if(itc->kid.len==tmp.kid.len
				&& strncasecmp(itc->kid.s, tmp.kid.s, tmp.kid.len)==0)
			break;
	}
	if(itc==NULL) {
		LM_DBG("no key chain id [%.*s]\n", tmp.kid.len, tmp.kid.s);
		/* add one */
		if(authx_xkey_insert(&tmp)<0) {
			LM_ERR("unable to insert the key [%.*s:%.*s]\n",
				tmp.kid.len, tmp.kid.s, tmp.kname.len, tmp.kname.s);
			rpc->fault(ctx, 500, "Insert failure");
			return;
		}
		return;
	}
	itc->kexpires = time(NULL) + tmp.kexpires;
	return;
}

rpc_export_t auth_xkeys_rpc_cmds[] = {
	{"auth_xkeys_.list",   auth_xkeys_rpc_list,
		auth_xkeys_rpc_list_doc,   0},
	{"auth_xkeys_.set",   auth_xkeys_rpc_set,
		auth_xkeys_rpc_set_doc,   0},
	{0, 0, 0, 0}
};

/**
 *
 */
int auth_xkeys_init_rpc(void)
{
	if (rpc_register_array(auth_xkeys_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
