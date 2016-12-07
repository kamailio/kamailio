/*
 * $Id$
 *
 * Lookup contacts in usrloc
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


#ifndef LOOKUP_H
#define LOOKUP_H

#include "../../parser/msg_parser.h"


/*
 * Lookup a contact in usrloc and rewrite R-URI if found
 */
int lookup(struct sip_msg* _m, char* _t, char* _s);


/*
 * Lookup a contact in usrloc and rewrite R-URI if found
 */
int lookup2(struct sip_msg* msg, char* table, char* aor);


/*
 * Return true if the AOR in the Request-URI is registered,
 * it is similar to lookup but registered neither rewrites
 * the Request-URI nor appends branches
 */
int registered(struct sip_msg* _m, char* _t, char* _s);

/*
 * Return true if the AOR in the Request-URI is registered,
 * it is similar to lookup but registered neither rewrites
 * the Request-URI nor appends branches
 */
int registered2(struct sip_msg* msg, char* table, char* p2);


#endif /* LOOKUP_H */
