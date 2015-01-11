/*
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
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
 */
/*! \file
 * \brief TMX :: var functions
 *
 * \ingroup tm
 * - Module: \ref tm
 */
		       
#ifndef _T_VAR_H_
#define _T_VAR_H_

#include "../../pvar.h"

void pv_tmx_data_init(void);

int pv_get_t_var_inv(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_get_t_var_req(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_get_t_var_rpl(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);

int pv_parse_t_var_name(pv_spec_p sp, str *in);

int pv_get_tm_branch_idx(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_tm_reply_ruid(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_t_branch(struct sip_msg *msg,  pv_param_t *param,
		    pv_value_t *res);
int pv_get_tm_reply_code(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_tm_reply_reason(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
int pv_get_tm_reply_last_received(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

int pv_get_t(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_parse_t_name(pv_spec_p sp, str *in);
#endif
