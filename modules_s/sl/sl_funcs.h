/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _SL_FUNCS_H
#define _SL_FUNCS_H

#include "../../parser/msg_parser.h"

#define TOTAG_SEPARATOR		'.'

#define SL_RPL_WAIT_TIME  2  // in sec

#define TOTAG_LEN MD5_LEN+CRC16_LEN+1

int sl_startup();
int sl_shutdown();
int sl_send_reply(struct sip_msg*,int,char*);
int sl_filter_ACK(struct sip_msg* );
int sl_reply_error(struct sip_msg *msg );


#endif


