/*
 * Route & Record-Route module
 *
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


#ifndef __RR_H__
#define __RR_H__

#include "../../parser/msg_parser.h"
#include "../../str.h"

/*
 * Finds Route header field in a SIP message
 */
int findRouteHF(struct sip_msg* _m);


/*
 * Gets the first URI from the first Route
 * header field in a message
 */
int parseRouteHF(struct sip_msg* _m, str* _s, char** _next);


/*
 * Rewrites Request URI from Route HF
 */
int rewriteReqURI(struct sip_msg* _m, str* _s);


/*
 * Removes the first URI from the first Route header
 * field, if there is only one URI in the Route header
 * field, remove the whole header field
 */
int remFirstRoute(struct sip_msg* _m, char* _next);


/*
 * Builds Record-Route line
 */
int buildRRLine(struct sip_msg* _m, str* _l);


/*
 * Add a new Record-Route line in SIP message
 */
int addRRLine(struct sip_msg* _m, str* _l);




#endif
