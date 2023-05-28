/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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
 *
 */

#ifndef PV_PID_H_
#define PV_PID_H_

#include "../../core/pvar.h"

int pv_pid_parse_name(pv_spec_t *sp, str *in);
int pv_pid_set(struct sip_msg *msg, pv_param_t *param, int op, pv_value_t *val);
int pv_pid_get(struct sip_msg *, pv_param_t *, pv_value_t *);

sr_xavp_t *pv_pid_get_pid(str *name);

void free_pid_fmt_buff();

#endif /* PV_PID_H_ */
