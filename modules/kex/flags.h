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

#ifndef _KEX_FLAGS_H_
#define _KEX_FLAGS_H_

#include "../../sr_module.h"

int w_issflagset(struct sip_msg *msg, char *flag, str *s2);
int w_resetsflag(struct sip_msg *msg, char *flag, str *s2);
int w_setsflag(struct sip_msg *msg, char *flag, char *s2);
int w_isbflagset(struct sip_msg *msg, char *flag, str *idx);
int w_resetbflag(struct sip_msg *msg, char *flag, str *idx);
int w_setbflag(struct sip_msg *msg, char *flag, char *idx);

#endif
