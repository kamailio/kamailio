/*
 * Presence Agent, LPIDF document support
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include "lpidf.h"
#include "common.h"
#include "paerrno.h"
#include "../../dprint.h"


#define TO_START "To: <"
#define TO_START_L (sizeof(TO_START) - 1)

#define TO_END ">"
#define TO_END_L (sizeof(TO_END) - 1)

#define CONTACT_START "Contact: <"
#define CONTACT_START_L (sizeof(CONTACT_START) - 1) 

#define CONTACT_MIDDLE ">;q="
#define CONTACT_MIDDLE_L (sizeof(CONTACT_MIDDLE) - 1)

#define Q_OPEN "1"
#define Q_OPEN_L (sizeof(Q_OPEN) - 1)

#define Q_CLOSED "0"
#define Q_CLOSED_L (sizeof(Q_CLOSED) - 1)

#define CRLF "\r\n"
#define CRLF_L (sizeof(CRLF) - 1)


/*
 * Add a presentity information
 */
int lpidf_add_presentity(str* _b, int _l, str* _uri)
{
	if (_l < (TO_START_L + _uri->len + TO_END_L + CRLF_L)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "lpidf_add_presentity(): Buffer too small\n");
		return -1;
	}

	str_append(_b, TO_START, TO_START_L);
	str_append(_b, _uri->s, _uri->len);
	str_append(_b, TO_END CRLF, TO_END_L + CRLF_L);
	return 0;
}


/*
 * Add a contact address with given status
 */
int lpidf_add_address(str* _b, int _l, str* _addr, lpidf_status_t _st)
{
	str s;

	switch(_st) {
	case LPIDF_ST_OPEN:   s.s = Q_OPEN; s.len = Q_OPEN_L;     break;
	case LPIDF_ST_CLOSED: s.s = Q_CLOSED; s.len = Q_CLOSED_L; break;
	}

	if (_l < (CONTACT_START_L + _addr->len + CONTACT_MIDDLE_L + s.len + 2)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "lpidf_add_address(): Buffer too small\n");
		return -1;
	}
	
	str_append(_b, CONTACT_START, CONTACT_START_L);
	str_append(_b, _addr->s, _addr->len);
	str_append(_b, CONTACT_MIDDLE, CONTACT_MIDDLE_L);
	str_append(_b, s.s, s.len);
	str_append(_b, CRLF, CRLF_L);
	return 0;
}
