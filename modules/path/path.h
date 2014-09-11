/*
 * $Id$
 *
 * Path handling for intermediate proxies.
 *
 * Copyright (C) 2006 Inode GmbH (Andreas Granig <andreas.granig@inode.info>)
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
/*! \file
 * \brief Path :: Utilities
 *
 * \ingroup path
 * - Module: path
 */



#ifndef MOD_PATH_H
#define MOD_PATH_H

#include "../../parser/msg_parser.h"

/*
 * Prepend own uri to Path header
 */
int add_path(struct sip_msg* _msg, char* _a, char* _b);

/*
 * Prepend own uri to Path header and take care of given
 * user.
 */
int add_path_usr(struct sip_msg* _msg, char* _a, char* _b);

/*
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri.
 */
int add_path_received(struct sip_msg* _msg, char* _a, char* _b);

/*
 * Prepend own uri to Path header and append received address as
 * "received"-param to that uri and take care of given user.
 */
int add_path_received_usr(struct sip_msg* _msg, char* _a, char* _b);

/*
 * rr callback for setting dst-uri
 */
void path_rr_callback(struct sip_msg *_m, str *r_param, void *cb_param);


#endif /* MOD_PATH_H */
