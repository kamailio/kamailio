/**
 *
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _PHONENUM_PV_H_
#define _PHONENUM_PV_H_

#include "../../core/pvar.h"

int pv_parse_phonenum_name(pv_spec_p sp, str *in);
int pv_get_phonenum(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int phonenum_init_pv(int smode);
void phonenum_destroy_pv(void);
void phonenum_pv_reset(str *pvclass);
int phonenum_update_pv(str *tomatch, str *cncode, str *pvclass);
int sr_phonenum_add_resid(str *rname);

#endif

