/*
 * $Id$
 *
 * Route & Record-Route module
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
 * -------
 * 2003-04-04 Extracted from common.[ch] (janakj)
 */

#ifndef RECORD_H
#define RECORD_H

#include "../../parser/msg_parser.h"


/*
 * Insert a new Record-Route header field with lr parameter
 */
int record_route(struct sip_msg* _m, char* _s1, char* _s2);


/*
 * Insert manually created Record-Route header, no checks, no restrictions,
 * always adds lr parameter, only fromtag is added automatically when requested
 */
int record_route_preset(struct sip_msg* _m, char* _ip, char* _s2);


/*
 * Insert a new Record-Route header field without lr parameter
 */
int record_route_strict(struct sip_msg* _m, char* _s1, char* _s2);

/*
 * Remove Record-Route header from message lumps
 */
int remove_record_route(struct sip_msg* _m, char* _s1, char* _s2);

#endif /* RECORD_H */
