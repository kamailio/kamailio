/*
 * Copyright (C) 2016 kamailio.org
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

/*
 * Support for RFC3455 / RFC7315 P-Charging-Vector
 * - parse charging vector from SIP message
 * - generate new unique charging vector
 * - can remove charging vector
 *
 * pseudo variables are exported and enable read only access to charging vector fields
 * $pcv(all) = whole field
 * $pcv(value) = icid-value field (see RFC3455 section 5.6)
 * $pcv(genaddr) = icid-generated-at field (see RFC3455 section 5.6)
 * $pcv(orig) = orig-ioi field (see RFC3455 section 5.6)
 * $pcv(term) = term-ioi field (see RFC3455 section 5.6)
 *
 * missing:
 * $pcv(transit-ioi) RFC7315 5.6
 * $pcv(related-icid) RFC7315 5.6
 * $pcv(related-icid-gen-addr) RFC7315 5.6
 */

#ifndef _CHARGINGVECTOR_H_
#define _CHARGINGVECTOR_H_

#include "../../core/pvar.h"

int sip_handle_pcv(sip_msg_t *msg, char *flags, char *str2);

int pv_get_charging_vector(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
int pv_parse_charging_vector_name(pv_spec_p sp, str *in);
#endif
