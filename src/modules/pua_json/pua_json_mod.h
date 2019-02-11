/*
 * PUA_JSON module interface
 *
 * Copyright (C) 2018 VoIPxSWITCH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */

#ifndef __PUA_JSON_MOD_H_
#define __PUA_JSON_MOD_H_

#include "../../core/mod_fix.h"
#include "pua_json_publish.h"

json_api_t json_api;
presence_api_t presence_api;
int pua_include_entity = 1;

static int mod_init(void);

#endif
