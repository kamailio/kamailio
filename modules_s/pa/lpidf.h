/*
 * Presence Agent, LPDIF document support
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


#ifndef LPIDF_H
#define LPIDF_H

#include "../../str.h"


typedef enum lpidf_status {
	LPIDF_ST_OPEN,
	LPIDF_ST_CLOSED,
} lpidf_status_t;

/*
 * Add a presentity information
 */
int lpidf_add_presentity(str* _b, int* _l, str* _uri);

/*
 * Add a contact address with given status
 */
int lpidf_add_address(str* _b, int* _l, str* _addr, lpidf_status_t _st);


#endif /* LPIDF_H */
