/**
 * $Id$
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

#ifndef _GEOIP_PV_H_
#define _GEOIP_PV_H_

#include <maxminddb.h>

#include "../../pvar.h"

int pv_parse_geoip2_name(pv_spec_p sp, str *in);
int pv_get_geoip2(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int geoip2_init_pv(char *path);
void geoip2_destroy_pv(void);
void geoip2_pv_reset(str *pvclass);
int geoip2_update_pv(str *tomatch, str *pvclass);

#endif

