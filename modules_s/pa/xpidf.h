/*
 * Presence Agent, XPIDF document support
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

#ifndef XPIDF_H
#define XPIDF_H

#include "../../str.h"


typedef enum xpidf_status {
	XPIDF_ST_OPEN,
	XPIDF_ST_CLOSED,
	XPIDF_ST_INUSE
} xpidf_status_t;


/*
 * Create start of pidf document
 */
int start_xpidf_doc(str* _b, int* _l);

/*
 * Add a presentity information
 */
int xpidf_add_presentity(str* _b, int* _l, str* _uri);

/*
 * Add a contact address with given status
 */
int xpidf_add_address(str* _b, int* _l, str* _addr, xpidf_status_t _st);


/*
 * End the document
 */
int end_xpidf_doc(str* _b, int* _l);


#endif /* XPIDF_H */
