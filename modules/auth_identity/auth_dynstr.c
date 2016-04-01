/*
 * $Id$ 
 *
 * Copyright (c) 2007 iptelorg GmbH
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief SIP-router auth-identity :: Dynamic strings
 * \ingroup auth-identity
 * Module: \ref auth-identity
 */

#include <errno.h>

#include "../../parser/parse_from.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_uri.h"
#include "../../parser/contact/parse_contact.h"

#include "../../data_lump.h"
#include "../../msg_translator.h"
#include "auth_identity.h"

/*
 * Dynamic string functions
 */

int initdynstr(dynstr *sout, int isize)
{
	memset(sout,0,sizeof(*sout));
	getstr_dynstr(sout).s=pkg_malloc(isize);
	if (!getstr_dynstr(sout).s) {
		LOG(L_WARN,
			"AUTH_IDENTITY:initdynstr: Not enough memory error\n");
		return -1;
	}
	sout->size=isize;

	return 0;
}

int cpy2dynstr(dynstr *sout, str *s2app)
{
	char *stmp;
	int isize = s2app->len;

	if (isize > sout->size) {
		stmp=pkg_realloc(sout->sd.s, isize);
		if (!stmp) {
			LOG(L_ERR, "AUTH_IDENTITY:cpy2dynstr: Not enough memory error\n");
			return -1;
		}
		sout->sd.s=stmp;
		sout->size=isize;
	}

	memcpy(sout->sd.s,s2app->s,s2app->len);
	sout->sd.len = isize;

	return 0;
}

int app2dynchr(dynstr *sout, char capp)
{
	char *stmp;
	int isize = sout->sd.len + 1;

	if (isize > sout->size) {
		stmp=pkg_realloc(sout->sd.s, isize);
		if (!stmp) {
			LOG(L_ERR, "AUTH_IDENTITY:app2dynchr: Not enough memory error\n");
			return -1;
		}
		sout->sd.s=stmp;
		sout->size++;
	}

	sout->sd.s[sout->sd.len]=capp;
	sout->sd.len++;

	return 0;
}

int app2dynstr(dynstr *sout, str *s2app)
{
	char *stmp;
	int isize = sout->sd.len + s2app->len;

	if (isize > sout->size) {
		stmp=pkg_realloc(sout->sd.s, isize);
		if (!stmp) {
			LOG(L_ERR, "AUTH_IDENTITY:app2dynstr: Not enough memory error\n");
			return -1;
		}
		sout->sd.s=stmp;
		sout->size=isize;
	}

	memcpy(&sout->sd.s[sout->sd.len],s2app->s,s2app->len);
	sout->sd.len = isize;

	return 0;
}
