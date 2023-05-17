/*
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _CRYPTO_EVCB_
#define _CRYPTO_EVCB_

#include "../../core/parser/msg_parser.h"

int crypto_evcb_enable(void);

int crypto_nio_in(sip_msg_t *msg);
int crypto_nio_out(sip_msg_t *msg);
int crypto_nio_encrypt(sip_msg_t *msg);
int crypto_nio_decrypt(sip_msg_t *msg);

#endif