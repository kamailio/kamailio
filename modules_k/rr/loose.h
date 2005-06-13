/*
 * $Id$
 *
 * Route & Record-Route module, loose routing support
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
 */


#ifndef LOOSE_H
#define LOOSE_H

#include "../../parser/msg_parser.h"


/*
 * Do loose routing as per RFC3621
 */
int loose_route(struct sip_msg* _m, char* _s1, char* _s2);


/*
 * Check if the our route hdr has required params
 */
int check_route_param(struct sip_msg * msg, char *re, char *foo);

#endif /* LOOSE_H */
