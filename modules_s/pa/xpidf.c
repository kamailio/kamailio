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

#include "xpidf.h"
#include "paerrno.h"
#include "../../dprint.h"
#include <string.h>

#define CRLF "\r\n"
#define CRLF_LEN (sizeof(CRLF) - 1)

#define PUBLIC_ID "//IETF//DTD RFCxxxx XPIDF 1.0//EN"
#define PUBLIC_ID_LEN (sizeof(PUBLIC_ID) - 1)

#define MIME_TYPE "application/xpidf+xml"
#define MIME_TYPE_LEN (sizeof(MIME_TYPE) - 1)

#define XML_VERSION "<?xml version=\"1.0\"?>"
#define XML_VERSION_LEN  (sizeof(XML_VERSION) - 1)

#define PRESENCE_STAG "<presence>"
#define PRESENCE_STAG_LEN (sizeof(PRESENCE_STAG) - 1)

#define PRESENCE_ETAG "</presence>"
#define PRESENCE_ETAG_LEN (sizeof(PRESENCE_ETAG) - 1)

#define ADDRESS_ETAG "</address>"
#define ADDRESS_ETAG_LEN (sizeof(ADDRESS_ETAG) - 1)

#define ATOM_ETAG "</atom>"
#define ATOM_ETAG_LEN (sizeof(ATOM_ETAG) - 1)

#define XPIDF_DTD "xpidf.dtd"
#define XPIDF_DTD_LEN (sizeof(XPDIF_DTD) - 1)

#define DOCTYPE "<!DOCTYPE presence PUBLIC \"" PUBLIC_ID "\" \"" XPIDF_DTD "\">"
#define DOCTYPE_LEN (sizeof(DOCTYPE) - 1)

#define PRESENTITY_START "<presentity uri=\""
#define PRESENTITY_START_LEN (sizeof(PRESENTITY_START) - 1)

#define PRESENTITY_END ";method=SUBSCRIBE\"/>"
#define PRESENTITY_END_LEN (sizeof(PRESENTITY_END) - 1)

#define ATOM_STAG "<atom id=\"9r28r49\">"
#define ATOM_STAG_LEN (sizeof(ATOM_STAG) - 1)

#define ADDRESS_START "<address uri=\""
#define ADDRESS_START_LEN (sizeof(ADDRESS_START) - 1)

#define ADDRESS_END "\">"
#define ADDRESS_END_LEN (sizeof(ADDRESS_END) - 1)

#define STATUS_OPEN "<status status=\"open\"/>"
#define STATUS_OPEN_LEN (sizeof(STATUS_OPEN) - 1)

#define STATUS_CLOSED "<status status=\"closed\"/>"
#define STATUS_CLOSED_LEN (sizeof(STATUS_CLOSED) - 1)

#define STATUS_INUSE "<status status=\"inuse\"/>"
#define STATUS_INUSE_LEN (sizeof(STATUS_INUSE) - 1)


/*
 * Create start of pidf document
 */
int start_xpidf_doc(str* _b, int _l)
{
	if ((XML_VERSION_LEN + CRLF_LEN +
	     DOCTYPE_LEN + CRLF_LEN +
	     PRESENCE_STAG_LEN + CRLF_LEN) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "start_xpidf_doc(): Buffer too small\n");
		return -1;
	}

	memcpy(_b->s + _b->len, XML_VERSION CRLF DOCTYPE CRLF PRESENCE_STAG CRLF,
	       XML_VERSION_LEN + CRLF_LEN + DOCTYPE_LEN + CRLF_LEN + PRESENCE_STAG_LEN + CRLF_LEN);
	_b->len += XML_VERSION_LEN + CRLF_LEN + DOCTYPE_LEN + CRLF_LEN + PRESENCE_STAG_LEN + CRLF_LEN;

	return 0;
}


/*
 * Add a presentity information
 */
int xpidf_add_presentity(str* _b, int _l, str* _uri)
{
	if (_l < PRESENTITY_START_LEN + _uri->len + PRESENTITY_END_LEN + CRLF_LEN) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "pidf_add_presentity(): Buffer too small\n");
		return -1;
	}

	memcpy(_b->s + _b->len, PRESENTITY_START, PRESENTITY_START_LEN);
	_b->len += PRESENTITY_START_LEN;

	memcpy(_b->s + _b->len, _uri->s, _uri->len);
	_b->len += _uri->len;

	memcpy(_b->s + _b->len, PRESENTITY_END CRLF, PRESENTITY_END_LEN + CRLF_LEN);
	_b->len += PRESENTITY_END_LEN + CRLF_LEN;

	return 0;
}


/*
 * Add a contact address with given status
 */
int xpidf_add_address(str* _b, int _l, str* _addr, xpidf_status_t _st)
{
	int len = 0;
	char* p;

	switch(_st) {
	case XPIDF_ST_OPEN:   p = STATUS_OPEN;   len = STATUS_OPEN_LEN;   break;
	case XPIDF_ST_CLOSED: p = STATUS_CLOSED; len = STATUS_CLOSED_LEN; break;
	case XPIDF_ST_INUSE:  p = STATUS_INUSE;  len = STATUS_INUSE_LEN;  break;
	default:              p = STATUS_CLOSED; len = STATUS_CLOSED_LEN; break; /* Makes gcc happy */
	}

	if (_l < 
	    (ATOM_STAG_LEN + CRLF_LEN +
	     ADDRESS_START_LEN + _addr->len + ADDRESS_END_LEN + CRLF_LEN +
	     len + CRLF_LEN +
	     ADDRESS_ETAG_LEN + CRLF_LEN +
	     ATOM_ETAG_LEN + CRLF_LEN)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "xpidf_add_address(): Buffer too small\n");
		return -1;
	}

	memcpy(_b->s + _b->len, ATOM_STAG CRLF ADDRESS_START, ATOM_STAG_LEN + CRLF_LEN + ADDRESS_START_LEN);
	_b->len += ATOM_STAG_LEN + CRLF_LEN + ADDRESS_START_LEN;

	memcpy(_b->s + _b->len, _addr->s, _addr->len);
	_b->len += _addr->len;

	memcpy(_b->s + _b->len, ADDRESS_END CRLF, ADDRESS_END_LEN + CRLF_LEN);
	_b->len += ADDRESS_END_LEN + CRLF_LEN;

	memcpy(_b->s + _b->len, p, len);
	_b->len += len;

	memcpy(_b->s + _b->len, CRLF ADDRESS_ETAG CRLF ATOM_ETAG CRLF, CRLF_LEN + ADDRESS_ETAG_LEN + CRLF_LEN + ATOM_ETAG_LEN + CRLF_LEN);
	_b->len += CRLF_LEN + ADDRESS_ETAG_LEN + CRLF_LEN + ATOM_ETAG_LEN + CRLF_LEN;

	return 0;
}


/*
 * End the document
 */
int end_xpidf_doc(str* _b, int _l)
{
	if (_l < (PRESENCE_ETAG_LEN + CRLF_LEN)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "end_xpidf_doc(): Buffer too small\n");
		return -1;
	}

	memcpy(_b->s + _b->len, PRESENCE_ETAG CRLF, PRESENCE_ETAG_LEN + CRLF_LEN);
	_b->len += PRESENCE_ETAG_LEN + CRLF_LEN;

	return 0;
}
