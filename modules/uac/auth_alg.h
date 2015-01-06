/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#ifndef _UAC_AUTH_ALG_H_
#define _UAC_AUTH_ALG_H_

#include "../../str.h"

#include "auth.h"


void uac_calc_HA1( struct uac_credential *crd,
		struct authenticate_body *auth,
		str* cnonce,
		HASHHEX sess_key);

void uac_calc_HA2( str *method, str *uri,
		struct authenticate_body *auth,
		HASHHEX hentity,
		HASHHEX HA2Hex );

void uac_calc_response( HASHHEX ha1, HASHHEX ha2,
		struct authenticate_body *auth,
		str* nc, str* cnonce,
		HASHHEX response);

#endif
