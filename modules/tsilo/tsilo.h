/**
 * Copyright (C) 2014 Federico Cabiddu (federico.cabiddu@gmail.com)
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

#ifndef _TSILO_MOD_H_
#define _TSILO_MOD_H_

#include "../../modules/tm/tm_load.h"
#include "../../modules/registrar/api.h"
#include "../../modules/usrloc/usrloc.h"

/** TM bind */
extern struct tm_binds _tmb;
/** REGISTRAR bind */
extern registrar_api_t _regapi;
/** USRLOC BIND **/
extern usrloc_api_t _ul;

extern int use_domain;

#endif
