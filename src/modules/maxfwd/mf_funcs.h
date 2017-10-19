/*
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
 */


#ifndef _MF_FUNCS_H
#define _MF_FUNCS_H


#include "../../core/parser/msg_parser.h"
#include "../../core/dprint.h"
#include "../../core/config.h"
#include "../../core/str.h"


int decrement_maxfwd(struct sip_msg *msg, int nr_val, str *str_val);
int add_maxfwd_header(struct sip_msg *msg, unsigned int val);
int is_maxfwd_present(struct sip_msg *msg, str *mf_value);

#endif
