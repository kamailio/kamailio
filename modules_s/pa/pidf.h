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
	PIDF_ST_CLOSED
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
int start_pidf_tuple(str* _b, str *_id, int _l);

/*
 * Add a contact address with given status and priority
 */
int pidf_add_contact(str* _b, int _l, str* _addr, pidf_status_t _st, double priority);

/*
 * Add location information
 */
int pidf_add_location(str* _b, int _l, str *_loc, str *_site, str *_floor, str *_room, double x, double y, double radius, 
		      enum prescaps prescaps);

/*
 * End of pidf tuple
 */
int end_pidf_tuple(str* _b, int _l);

/*
 * End the document
 */
int end_pidf_doc(str* _b, int _l);

/* returns flags indicating which fields were parsed */
int parse_pidf(char *pidf_body, str *contact_str, str *basic_str, str *status_str, 
	       str *location_str, str *site_str, str *floor_str, str *room_str,
	       double *xp, double *yp, double *radiusp,
	       str *packet_loss_str, double *priorityp, time_t *expiresp,
	       int *prescapsp);
#define PARSE_PIDF_CONTACT (1 << 0)
#define PARSE_PIDF_BASIC (1 << 1)
#define PARSE_PIDF_STATUS (1 << 2)
#define PARSE_PIDF_LOC (1 << 3)
#define PARSE_PIDF_SITE (1 << 4)
#define PARSE_PIDF_FLOOR (1 << 5)
#define PARSE_PIDF_ROOM (1 << 6)
#define PARSE_PIDF_X (1 << 7)
#define PARSE_PIDF_Y (1 << 8)
#define PARSE_PIDF_RADIUS (1 << 9)
#define PARSE_PIDF_PACKET_LOSS (1 << 10)
#define PARSE_PIDF_PRIORITY (1 << 11)
#define PARSE_PIDF_EXPIRES  (1 << 12)
#define PARSE_PIDF_PRESCAPS  (1 << 13)

#define PARSE_PIDF_LOCATION_MASK 0x3F8

#endif /* PIDF_H */
