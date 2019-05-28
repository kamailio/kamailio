/**
 * Copyright (C) 2019 Daniel-Constantin Mierla (asipto.com)
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
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"

MODULE_VERSION

struct module_exports exports = {
	"kemix",         /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,               /* cmd (cfg function) exports */
	0,               /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	0,               /* module init function */
	0,               /* per-child init function */
	0                /* module destroy function */
};


/**
 *
 */
static sr_kemi_xval_t _sr_kemi_kx_xval = {0};

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ruri(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->first_line.type == SIP_REPLY) {
		/* REPLY doesnt have a ruri */
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse the R-URI\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	if (msg->new_uri.s!=NULL) {
		_sr_kemi_kx_xval.v.s = msg->new_uri;
	} else {
		_sr_kemi_kx_xval.v.s = msg->first_line.u.request.uri;
	}
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ouri(sip_msg_t *msg)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->first_line.type == SIP_REPLY) {
		/* REPLY doesnt have a ruri */
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse the R-URI\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = msg->first_line.u.request.uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_furi(sip_msg_t *msg)
{
	to_body_t *xto = NULL;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(parse_from_header(msg)<0) {
		LM_ERR("cannot parse From header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(msg->from==NULL || get_from(msg)==NULL) {
		LM_DBG("no From header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	xto = get_from(msg);
	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = xto->uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_turi(sip_msg_t *msg)
{
	to_body_t *xto = NULL;

	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(parse_to_header(msg)<0) {
		LM_ERR("cannot parse To header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}
	if(msg->to==NULL || get_to(msg)==NULL) {
		LM_DBG("no To header\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, 0);
		return &_sr_kemi_kx_xval;
	}

	xto = get_to(msg);
	_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
	_sr_kemi_kx_xval.v.s = xto->uri;
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_xuri_attr(sip_msg_t *msg, sip_uri_t *puri,
		int iattr, int xmode)
{
	if(iattr==1) {
		/* username */
		if(puri->user.s==NULL || puri->user.len<=0) {
			sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
			return &_sr_kemi_kx_xval;
		}
		_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_kx_xval.v.s = puri->user;
		return &_sr_kemi_kx_xval;
	} else if(iattr==2) {
		/* domain */
		if(puri->host.s==NULL || puri->host.len<=0) {
			sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
			return &_sr_kemi_kx_xval;
		}
		_sr_kemi_kx_xval.vtype = SR_KEMIP_STR;
		_sr_kemi_kx_xval.v.s = puri->host;
		return &_sr_kemi_kx_xval;
	}
	LM_ERR("unknown attribute id: %d\n", iattr);
	sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
	return &_sr_kemi_kx_xval;
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ruri_attr(sip_msg_t *msg, int iattr, int xmode)
{
	memset(&_sr_kemi_kx_xval, 0, sizeof(sr_kemi_xval_t));

	if(msg==NULL || msg->first_line.type == SIP_REPLY) {
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}

	if(msg->parsed_uri_ok==0 /* R-URI not parsed*/ && parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse the R-URI\n");
		sr_kemi_xval_null(&_sr_kemi_kx_xval, xmode);
		return &_sr_kemi_kx_xval;
	}
	return ki_kx_get_xuri_attr(msg, &(msg->parsed_uri), iattr, xmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ruser(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 1, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_ruserx(sip_msg_t *msg, int xmode)
{
	return ki_kx_get_ruri_attr(msg, 1, xmode);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rhost(sip_msg_t *msg)
{
	return ki_kx_get_ruri_attr(msg, 2, SR_KEMI_XVAL_NULL_NONE);
}

/**
 *
 */
static sr_kemi_xval_t* ki_kx_get_rhostx(sip_msg_t *msg, int xmode)
{
	return ki_kx_get_ruri_attr(msg, 2, xmode);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_kx_exports[] = {
	{ str_init("kx"), str_init("get_ruri"),
		SR_KEMIP_XVAL, ki_kx_get_ruri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_ouri"),
		SR_KEMIP_XVAL, ki_kx_get_ouri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_furi"),
		SR_KEMIP_XVAL, ki_kx_get_furi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_turi"),
		SR_KEMIP_XVAL, ki_kx_get_turi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_ruser"),
		SR_KEMIP_XVAL, ki_kx_get_ruser,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_ruserx"),
		SR_KEMIP_XVAL, ki_kx_get_ruserx,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rhost"),
		SR_KEMIP_XVAL, ki_kx_get_rhost,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("kx"), str_init("get_rhostx"),
		SR_KEMIP_XVAL, ki_kx_get_rhostx,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},


	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_kx_exports);
	return 0;
}
