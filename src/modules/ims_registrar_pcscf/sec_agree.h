/**
 * Copyright (C) 2017 kamailio.org
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

#ifndef SEC_AGREE_H
#define SEC_AGREE_H

#include "../ims_usrloc_pcscf/usrloc.h"

/**
 * Looks for the Security-Client header
 * @param msg - the sip message
 * @param params - ptr to struct sec_agree_params, where parsed values will be saved
 * @returns 0 on success, error code on failure
 */
int cscf_get_security(struct sip_msg *msg, security_t *params);

void free_security_t(security_t *params);

#endif // SEC_AGREE_H
