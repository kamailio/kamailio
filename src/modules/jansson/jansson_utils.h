/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _JANSSON_UTILS_H_
#define _JANSSON_UTILS_H_

#include <jansson.h>

#include "../../core/sr_module.h"
#include "../../core/lvalue.h"
#include "../../core/xavp.h"

typedef int (*jansson_to_val_f)(pv_value_t* val, char** freeme, json_t* v);
int jansson_to_val(pv_value_t* val, char** freeme, json_t* v);
int jansson_to_xval(sr_xval_t *val, char** freeme, json_t* v);

#endif
