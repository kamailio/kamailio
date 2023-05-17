/*
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
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
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/cfg/cfg_struct.h"
#include "../../core/receive.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"
#include "../../core/events.h"
#include "../../core/onsend.h"

#include "crypto_aes.h"

#define CRYPTO_NIO_OUT (1 << 0)
#define CRYPTO_NIO_ENCRYPT (1 << 1)
#define CRYPTO_NIO_DECRYPT (1 << 2)

/* set/get crypto env */
#define crypto_set_msg_env(_msg, _evenv)  \
	do {                                  \
		_msg->ldv.vdata = (void *)_evenv; \
	} while(0)
#define crypto_get_msg_env(_msg) ((crypto_env_t *)_msg->ldv.vdata)

int crypto_nio_received(sr_event_param_t *evp);
int crypto_nio_sent(sr_event_param_t *evp);

typedef struct _crypto_env
{
	int mflags;
	sr_event_param_t *evp;
} crypto_env_t;

typedef struct _crypto_evroutes
{
	int netio;
	str netio_name;
} crypto_evroutes_t;

static crypto_evroutes_t _crypto_rts;

extern str _crypto_kevcb_netio;
extern str _crypto_netio_key;

static int _crypto_evcb_enabled = 0;

/**
 *
 */
int crypto_evcb_enable(void)
{
	_crypto_evcb_enabled = 1;

	memset(&_crypto_rts, 0, sizeof(crypto_evroutes_t));
	_crypto_rts.netio_name.s = "crypto:netio";
	_crypto_rts.netio_name.len = strlen(_crypto_rts.netio_name.s);
	_crypto_rts.netio = route_lookup(&event_rt, _crypto_rts.netio_name.s);
	if(_crypto_rts.netio < 0 || event_rt.rlist[_crypto_rts.netio] == NULL) {
		_crypto_rts.netio = -1;
	}

	/* register network hooks */
	sr_event_register_cb(SREV_NET_DATA_IN, crypto_nio_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, crypto_nio_sent);

	return 0;
}

/**
 *
 */
int crypto_exec_evroute(crypto_env_t *evenv, int rt, str *kevcb, str *rtname)
{
	int backup_rt;
	struct run_act_ctx ctx;
	sip_msg_t *fmsg;
	sip_msg_t tmsg;
	sr_kemi_eng_t *keng = NULL;
	onsend_info_t onsnd_info = {0};

	if(evenv == 0) {
		LM_ERR("crypto env not set\n");
		return -1;
	}

	if((rt < 0) && (kevcb == NULL || kevcb->s == NULL || kevcb->len <= 0)) {
		return 0;
	}

	if(faked_msg_get_new(&tmsg) < 0) {
		LM_ERR("failed to get a new faked message\n");
		return -1;
	}
	fmsg = &tmsg;

	if(evenv->evp->rcv != NULL) {
		fmsg->rcv = *evenv->evp->rcv;
	}

	if(evenv->mflags & CRYPTO_NIO_OUT) {
		onsnd_info.to = &evenv->evp->dst->to;
		onsnd_info.send_sock = evenv->evp->dst->send_sock;
		onsnd_info.buf = fmsg->buf;
		onsnd_info.len = fmsg->len;
		onsnd_info.msg = fmsg;
		p_onsend = &onsnd_info;
	}

	crypto_set_msg_env(fmsg, evenv);
	backup_rt = get_route_type();
	set_route_type(EVENT_ROUTE);
	init_run_actions_ctx(&ctx);
	if(rt >= 0) {
		run_top_route(event_rt.rlist[rt], fmsg, 0);
	} else {
		keng = sr_kemi_eng_get();
		if(keng != NULL) {
			if(sr_kemi_route(keng, fmsg, EVENT_ROUTE, kevcb, rtname) < 0) {
				LM_ERR("error running event route kemi callback\n");
			}
		}
	}
	set_route_type(backup_rt);
	crypto_set_msg_env(fmsg, NULL);
	/* free the structure -- it is a clone of faked msg */
	free_sip_msg(fmsg);

	if(evenv->mflags & CRYPTO_NIO_OUT) {
		p_onsend = NULL;
	}
	return 0;
}

/**
 *
 */
int crypto_nio_received(sr_event_param_t *evp)
{
	int ret;
	crypto_env_t evenv;
	EVP_CIPHER_CTX *de = NULL;
	str dtext;
	str *obuf;

	memset(&evenv, 0, sizeof(crypto_env_t));

	evenv.evp = evp;

	ret = crypto_exec_evroute(&evenv, _crypto_rts.netio, &_crypto_kevcb_netio,
			&_crypto_rts.netio_name);

	LM_DBG("sent event callback - ret:%d - flags:%d\n", ret, evenv.mflags);

	if(!(evenv.mflags & CRYPTO_NIO_DECRYPT)) {
		goto done;
	}
	LM_DBG("decrypting\n");

	de = EVP_CIPHER_CTX_new();
	if(de == NULL) {
		LM_ERR("cannot get new cipher context\n");
		return -1;
	}

	/* gen key and iv. init the cipher ctx object */
	if(crypto_aes_init((unsigned char *)_crypto_netio_key.s,
			   _crypto_netio_key.len, (unsigned char *)crypto_get_salt(), NULL,
			   NULL, de)) {
		EVP_CIPHER_CTX_free(de);
		LM_ERR("couldn't initialize AES cipher\n");
		return -1;
	}

	obuf = (str *)evp->data;
	dtext.len = obuf->len;
	dtext.s = (char *)crypto_aes_decrypt(
			de, (unsigned char *)obuf->s, &dtext.len);
	if(dtext.s == NULL) {
		EVP_CIPHER_CTX_free(de);
		LM_ERR("AES decryption failed\n");
		return -1;
	}

	if(dtext.len >= BUF_SIZE) {
		LM_ERR("new buffer overflow (%d)\n", dtext.len);
		free(dtext.s);
		EVP_CIPHER_CTX_free(de);
		return -1;
	}

	obuf->len = dtext.len;
	memcpy(obuf->s, dtext.s, obuf->len);
	obuf->s[obuf->len] = '\0';

	EVP_CIPHER_CTX_cleanup(de);
	EVP_CIPHER_CTX_free(de);
	free(dtext.s);

done:
	return 0;
}

/**
 *
 */
int crypto_nio_sent(sr_event_param_t *evp)
{
	int ret;
	crypto_env_t evenv;
	EVP_CIPHER_CTX *en = NULL;
	str etext;
	str nbuf;
	str *obuf;

	memset(&evenv, 0, sizeof(crypto_env_t));

	evenv.mflags |= CRYPTO_NIO_OUT;
	evenv.evp = evp;

	ret = crypto_exec_evroute(&evenv, _crypto_rts.netio, &_crypto_kevcb_netio,
			&_crypto_rts.netio_name);

	LM_DBG("sent event callback - ret:%d - flags:%d\n", ret, evenv.mflags);

	if(!(evenv.mflags & CRYPTO_NIO_ENCRYPT)) {
		goto done;
	}

	LM_DBG("encrypting\n");
	en = EVP_CIPHER_CTX_new();
	if(en == NULL) {
		LM_ERR("cannot get new cipher context\n");
		return -1;
	}

	/* gen key and iv. init the cipher ctx object */
	if(crypto_aes_init((unsigned char *)_crypto_netio_key.s,
			   _crypto_netio_key.len, (unsigned char *)crypto_get_salt(), NULL,
			   en, NULL)) {
		EVP_CIPHER_CTX_free(en);
		LM_ERR("couldn't initialize AES cipher\n");
		return -1;
	}

	obuf = (str *)evp->data;

	etext.len = obuf->len;
	etext.s = (char *)crypto_aes_encrypt(
			en, (unsigned char *)obuf->s, &etext.len);
	if(etext.s == NULL) {
		EVP_CIPHER_CTX_free(en);
		LM_ERR("AES encryption failed\n");
		return -1;
	}

	nbuf.s = (char *)pkg_malloc(etext.len + 1);
	if(nbuf.s == NULL) {
		EVP_CIPHER_CTX_free(en);
		PKG_MEM_ERROR;
		free(etext.s);
		return -1;
	}
	pkg_free(obuf->s);
	nbuf.len = etext.len;
	memcpy(nbuf.s, etext.s, nbuf.len);
	nbuf.s[nbuf.len] = '\0';

	EVP_CIPHER_CTX_cleanup(en);
	EVP_CIPHER_CTX_free(en);
	free(etext.s);

	obuf->s = nbuf.s;
	obuf->len = nbuf.len;

done:
	return 0;
}

/**
 *
 */
int crypto_nio_in(sip_msg_t *msg)
{
	crypto_env_t *evenv;

	evenv = crypto_get_msg_env(msg);
	if(evenv->mflags & CRYPTO_NIO_OUT) {
		return -1;
	}

	return 1;
}

/**
 *
 */
int crypto_nio_out(sip_msg_t *msg)
{
	crypto_env_t *evenv;

	evenv = crypto_get_msg_env(msg);
	if(evenv->mflags & CRYPTO_NIO_OUT) {
		return 1;
	}

	return -1;
}

/**
 *
 */
int crypto_nio_encrypt(sip_msg_t *msg)
{
	crypto_env_t *evenv;

	evenv = crypto_get_msg_env(msg);

	evenv->mflags |= CRYPTO_NIO_ENCRYPT;

	return 1;
}

/**
 *
 */
int crypto_nio_decrypt(sip_msg_t *msg)
{
	crypto_env_t *evenv;

	evenv = crypto_get_msg_env(msg);

	evenv->mflags |= CRYPTO_NIO_DECRYPT;

	return 1;
}