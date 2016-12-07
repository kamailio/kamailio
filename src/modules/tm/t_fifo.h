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
 *
 */



#ifndef _TM_T_FIFO_H_
#define _TM_T_FIFO_H_

#include "../../parser/msg_parser.h"
#include "../../sr_module.h"

int fixup_t_write( void** param, int param_no);

int parse_tw_append( modparam_t type, void* val);

int init_twrite_lines(void);

int init_twrite_sock(void);

int t_write_req(struct sip_msg* msg, char* vm_fifo, char* action);

int t_write_unix(struct sip_msg* msg, char* sock_name, char* action);

#endif
