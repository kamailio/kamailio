/*
 * SIPUTILS mangler module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief SIP-utils ::  URI Operations
 * \ingroup siputils
 * - Module; \ref siputils
 */


#ifndef _SIPOPS_H_
#define _SIPOPS_H_

#include "../../parser/msg_parser.h"

int w_cmp_uri(struct sip_msg *msg, char *uri1, char *uri2);
int w_cmp_aor(struct sip_msg *msg, char *uri1, char *uri2);
int w_is_gruu(sip_msg_t *msg, char *uri1, char *p2);
int w_is_supported(sip_msg_t *msg, char *_option, char *p2);
int w_is_first_hop(sip_msg_t *msg, char *uri1, char *p2);

#endif
