/*
 * Presence Agent, PIDF document support
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

#ifndef PIDF_H
#define PIDF_H

#include "../../str.h"


typedef enum pidf_status {
	PIDF_ST_OPEN,
	PIDF_ST_CLOSED,
	PIDF_ST_INUSE
} pidf_status_t;


/*
 * Create start of pidf document
 */
int start_pidf_doc(str* _b, int _l);


/*
 * Add a presentity information
 */
int pidf_add_presentity(str* _b, int _l, str* _uri);

/*
 * Create start of pidf tuple
 */
int start_pidf_tuple(str* _b, int _l);

/*
 * Add a contact address with given status
 */
int pidf_add_address(str* _b, int _l, str* _addr, pidf_status_t _st);

/*
 * Add location information
 */
int pidf_add_location(str* _b, int _l, str *_loc, str *_site, str *_floor, str *_room, double x, double y, double radius);

/*
 * End of pidf tuple
 */
int end_pidf_tuple(str* _b, int _l);

/*
 * End the document
 */
int end_pidf_doc(str* _b, int _l);

void parse_pidf(char *pidf_body, str *basic_str, str *location_str,
		str *site_str, str *floor_str, str *room_str,
		double *xp, double *yp, double *radiusp);

#endif /* PIDF_H */
