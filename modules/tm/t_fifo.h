/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  2003-03-31  200 for INVITE/UAS resent even for UDP (jiri) 
 *  2004-11-15  t_write_xxx can print whatever avp/hdr
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
