/*
 * $Id$
 *
 * Route & Record-Route module
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-04-04 Extracted from common.[ch] (janakj)
 * 2005-04-10 add_rr_param() function added (bogdan)
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
 * Appends a new Record-Route parameter
 */
int add_rr_param(struct sip_msg* msg, str* rr_param);


#endif /* RECORD_H */
