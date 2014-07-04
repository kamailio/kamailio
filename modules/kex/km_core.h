/**
 * $Id$
 *
 * Copyright (C) 2009
 *
 * This file is part of SIP-Router.org, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
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

#ifndef _KEX_CORE_H_
#define _KEX_CORE_H_

#include "../../sr_module.h"

int w_setdsturi(struct sip_msg *msg, char *uri, str *s2);
int w_resetdsturi(struct sip_msg *msg, char *uri, str *s2);
int w_isdsturiset(struct sip_msg *msg, char *uri, str *s2);
int w_pv_printf(struct sip_msg *msg, char *s1, str *s2);
int pv_printf_fixup(void** param, int param_no);

#endif
