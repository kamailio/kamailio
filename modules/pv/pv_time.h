/*
 * Copyright (C) 2007 Elena-Ramona Modroiu
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

#ifndef _PV_TIME_H_
#define _PV_TIME_H_

#include "../../pvar.h"

int pv_parse_time_name(pv_spec_p sp, str *in);
int pv_get_time(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_parse_strftime_name(pv_spec_p sp, str *in);
int pv_get_strftime(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_timenows(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_timenowf(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_times(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_timef(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_timeb(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_parse_timeval_name(pv_spec_p sp, str *in);
int pv_get_timeval(struct sip_msg *msg, pv_param_t *param,
        pv_value_t *res);
#endif

