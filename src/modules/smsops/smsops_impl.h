/*
 * Copyright (C) 2015 Carsten Bock, ng-voice GmbH
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

#ifndef _SMSOPS_TRANS_H_
#define _SMSOPS_TRANS_H_

#include "../../core/pvar.h"


int smsdump(struct sip_msg *, char *, char *);
int isRPDATA(struct sip_msg *, char *, char *);

int pv_parse_rpdata_name(pv_spec_p, str *);
int pv_parse_tpdu_name(pv_spec_p, str *);
int pv_get_sms(struct sip_msg *, pv_param_t *, pv_value_t *);
int pv_set_sms(struct sip_msg *, pv_param_t *, int, pv_value_t *);

// Generate SMS-ACK
int pv_sms_ack(struct sip_msg *, pv_param_t *, pv_value_t *);
// Generate a SMS-Body
int pv_sms_body(struct sip_msg *, pv_param_t *, pv_value_t *);

#endif
