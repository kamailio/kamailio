/*
 * Presence Agent, LPIDF document support
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


#include <string.h>
#include "lpidf.h"
#include "paerrno.h"
#include "../../dprint.h"


#define TO_START "To: <"
#define TO_START_LEN (sizeof(TO_START) - 1)

#define TO_END ">"
#define TO_END_LEN (sizeof(TO_END_LEN) - 1)

#define CONTACT_START "Contact: <"
#define CONTACT_START_LEN (sizeof(CONTACT_START) - 1) 

#define CONTACT_MIDDLE ">;q="
#define CONTACT_MIDDLE_LEN (sizeof(CONTACT_MIDDLE) - 1)

#define Q_OPEN "1"
#define Q_OPEN_LEN (sizeof(Q_OPEN) - 1)

#define Q_CLOSED "0"
#define Q_CLOSED_LEN (sizeof(Q_CLOSED) - 1)

#define CRLF "\r\n"
#define CRLF_LEN (sizeof(CRLF) - 1)


/*
 * Add a presentity information
 */
int lpidf_add_presentity(str* _b, int _l, str* _uri)
{
	if (_l < (TO_START_LEN + _uri->len + TO_END_LEN + 2)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "lpidf_add_presentity(): Buffer too small\n");
		return -1;
	}

	memcpy(_b->s + _b->len, TO_START, TO_START_LEN);
	_b->len += TO_START_LEN;

	memcpy(_b->s + _b->len, _uri->s, _uri->len);
	_b->len += _uri->len;

	memcpy(_b->s + _b->len, TO_END CRLF, TO_END_LEN + CRLF_LEN);
	_b->len += TO_END_LEN + CRLF_LEN;

	return 0;
}


/*
 * Add a contact address with given status
 */
int lpidf_add_address(str* _b, int _l, str* _addr, lpidf_status_t _st)
{
	str s;

	switch(_st) {
	case LPIDF_ST_OPEN:   s.s = Q_OPEN; s.len = Q_OPEN_LEN;     break;
	case LPIDF_ST_CLOSED: s.s = Q_CLOSED; s.len = Q_CLOSED_LEN; break;
	}

	if (_l < (
		  CONTACT_START_LEN + _addr->len + CONTACT_MIDDLE_LEN + s.len + 2)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "lpidf_add_address(): Buffer too small\n");
		return -1;
	}

	memcpy(_b->s + _b->len, CONTACT_START, CONTACT_START_LEN);
	_b->len += CONTACT_START_LEN;

	memcpy(_b->s + _b->len, _addr->s, _addr->len);
	_b->len += _addr->len;

	memcpy(_b->s + _b->len, CONTACT_MIDDLE, CONTACT_MIDDLE_LEN);
	_b->len += CONTACT_MIDDLE_LEN;

	memcpy(_b->s + _b->len, s.s, s.len);
	_b->len += s.len;

	memcpy(_b->s + _b->len, CRLF, CRLF_LEN);
	_b->len += CRLF_LEN;

	return 0;
}
