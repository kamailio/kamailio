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

#include <string.h>
#include "../../dprint.h"
#include "paerrno.h"
#include "common.h"
#include "pidf.h"

#define CRLF "\r\n"
#define CRLF_L (sizeof(CRLF) - 1)

#define PUBLIC_ID "//IETF//DTD RFCxxxx PIDF 1.0//EN"
#define PUBLIC_ID_L (sizeof(PUBLIC_ID) - 1)

#define MIME_TYPE "application/pidf+xml"
#define MIME_TYPE_L (sizeof(MIME_TYPE) - 1)

#define XML_VERSION "<?xml version=\"1.0\"?>"
#define XML_VERSION_L (sizeof(XML_VERSION) - 1)

#define ADDRESS_ETAG "</contact>"
#define ADDRESS_ETAG_L (sizeof(ADDRESS_ETAG) - 1)

#define TUPLE_ETAG "</tuple>"
#define TUPLE_ETAG_L (sizeof(TUPLE_ETAG) - 1)

#define PIDF_DTD "pidf.dtd"
#define PIDF_DTD_L (sizeof(XPDIF_DTD) - 1)

#define DOCTYPE "<!DOCTYPE presence PUBLIC \"" PUBLIC_ID "\" \"" PIDF_DTD "\">"
#define DOCTYPE_L (sizeof(DOCTYPE) - 1)

#define PRESENCE_START "<presence entity=\"sip:"
#define PRESENCE_START_L (sizeof(PRESENCE_START) - 1)

#define PRESENCE_END "\">"
#define PRESENCE_END_L (sizeof(PRESENCE_END) - 1)

#define PRESENCE_ETAG "</presence>"
#define PRESENCE_ETAG_L (sizeof(PRESENCE_ETAG) - 1)

#define TUPLE_STAG "<tuple id=\"9r28r49\">"
#define TUPLE_STAG_L (sizeof(TUPLE_STAG) - 1)

#define ADDRESS_START "<contact priority=\"0.8\">"
#define ADDRESS_START_L (sizeof(ADDRESS_START) - 1)

#define ADDRESS_END ""
#define ADDRESS_END_L (sizeof(ADDRESS_END) - 1)

#define STATUS_OPEN "<status><basic>open</basic></status>"
#define STATUS_OPEN_L (sizeof(STATUS_OPEN) - 1)

#define STATUS_CLOSED "<status><basic>closed</basic></status>"
#define STATUS_CLOSED_L (sizeof(STATUS_CLOSED) - 1)

#define STATUS_INUSE "<status><basic>inuse</basic></status>"
#define STATUS_INUSE_L (sizeof(STATUS_INUSE) - 1)

#define LOC_STAG "<location><loc>"
#define LOC_STAG_L (sizeof(LOC_STAG) - 1)

#define LOC_ETAG "</loc></location>"
#define LOC_ETAG_L (sizeof(LOC_ETAG) - 1)


/*
 * Create start of pidf document
 */
int start_pidf_doc(str* _b, int _l)
{
	if ((XML_VERSION_L + 
	     CRLF_L +
	     DOCTYPE_L + 
	     CRLF_L
	    ) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "start_pidf_doc(): Buffer too small\n");
		return -1;
	}

	str_append(_b, XML_VERSION CRLF DOCTYPE CRLF,
		   XML_VERSION_L + CRLF_L + DOCTYPE_L + CRLF_L);
	return 0;
}


/*
 * Add a presentity information
 */
int pidf_add_presentity(str* _b, int _l, str* _uri)
{
	if (_l < PRESENCE_START_L + _uri->len + PRESENCE_END_L + CRLF_L) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "pidf_add_presentity(): Buffer too small\n");
		return -1;
	}

	str_append(_b, PRESENCE_START, PRESENCE_START_L);
	str_append(_b, _uri->s, _uri->len);
	str_append(_b, PRESENCE_END CRLF, 
		   PRESENCE_END_L + CRLF_L);
	return 0;
}


/*
 * Create start of pidf tuple
 */
int start_pidf_tuple(str* _b, int _l)
{
	if ((TUPLE_STAG_L + 
	     CRLF_L
	    ) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "start_pidf_tuple(): Buffer too small\n");
		return -1;
	}

	str_append(_b, TUPLE_STAG CRLF,
		   TUPLE_STAG_L + CRLF_L);
	return 0;
}

/*
 * Add a contact address with given status
 */
int pidf_add_address(str* _b, int _l, str* _addr, pidf_status_t _st, str *_loc)
{
	int len = 0;
	char* p;

	switch(_st) {
	case PIDF_ST_OPEN:   p = STATUS_OPEN;   len = STATUS_OPEN_L;   break;
	case PIDF_ST_CLOSED: p = STATUS_CLOSED; len = STATUS_CLOSED_L; break;
	case PIDF_ST_INUSE:  p = STATUS_INUSE;  len = STATUS_INUSE_L;  break;
	default:              p = STATUS_CLOSED; len = STATUS_CLOSED_L; break; /* Makes gcc happy */
	}

	if (_l < (ADDRESS_START_L + 
		  _addr->len + 
		  ADDRESS_END_L + 
		  CRLF_L +
		  len + 
		  CRLF_L +
		  ADDRESS_ETAG_L + 
		  CRLF_L
		 )
	   ) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "pidf_add_address(): Buffer too small\n");
		return -1;
	}

	str_append(_b, ADDRESS_START, ADDRESS_START_L);
	str_append(_b, _addr->s, _addr->len);
	str_append(_b, ADDRESS_END CRLF, ADDRESS_END_L + CRLF_L);
	str_append(_b, CRLF ADDRESS_ETAG CRLF , 
		   CRLF_L + ADDRESS_ETAG_L + CRLF_L);
	str_append(_b, p, len);
	if (_loc->len) {
		str_append(_b, CRLF LOC_STAG, CRLF_L + LOC_STAG_L);
		str_append(_b, _loc->s, _loc->len);
		str_append(_b, CRLF LOC_ETAG, CRLF_L + LOC_ETAG_L);
	}

	return 0;
}

/*
 * Create start of pidf tuple
 */
int end_pidf_tuple(str* _b, int _l)
{
	if ((TUPLE_ETAG_L + 
	     CRLF_L
	    ) > _l) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "end_pidf_tuple(): Buffer too small\n");
		return -1;
	}

	str_append(_b, TUPLE_ETAG CRLF,
		   TUPLE_ETAG_L + CRLF_L);
	return 0;
}

/*
 * End the document
 */
int end_pidf_doc(str* _b, int _l)
{
	if (_l < (PRESENCE_ETAG_L + CRLF_L)) {
		paerrno = PA_SMALL_BUFFER;
		LOG(L_ERR, "end_pidf_doc(): Buffer too small\n");
		return -1;
	}

	str_append(_b, PRESENCE_ETAG CRLF, PRESENCE_ETAG_L + CRLF_L);
	return 0;
}
