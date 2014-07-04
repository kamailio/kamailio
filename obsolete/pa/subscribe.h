/*
 * Presence Agent, subscribe handling
 *
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
 */

#ifndef SUBSCRIBE_H
#define SUBSCRIBE_H

#include "../../parser/msg_parser.h"

/*
 * Handle a subscribe Request
 */
int handle_subscription(struct sip_msg* _m, char* _domain, char* _s2);


/*
 * Return 1 if the subscription exists and 0 if not
 */
int existing_subscription(struct sip_msg* _m, char* _domain, char* _s2);


/*
 * Handle a REGISTER Request: ensures AOR is in presentity table.
 */
int pa_handle_registration(struct sip_msg* _m, char* _domain, char* _s2);

/*
 * Get presentity URI, which is stored in R-URI
 */
int get_pres_uri(struct sip_msg* _m, str* _puri);

#endif /* SUBSCRIBE_H */
