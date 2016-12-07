/*
 * Presence Agent, error reporting
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

#ifndef PAERRNO_H
#define PAERRNO_H

typedef enum paerr {
	PA_OK,            /* Everything went OK */
	PA_PARSE_ERR,     /* Error while parsing headers */
	PA_FROM_MISS,     /* From header field missing */
	PA_EVENT_MISS,    /* Event header field missing */
	PA_EVENT_PARSE,   /* Error while parsing Event header field */
	PA_EXPIRES_PARSE, /* Error while parsing Expires header field */
	PA_EVENT_UNSUPP,  /* Unsupported event package */
	PA_WRONG_ACCEPTS, /* Accepts does not match event package */
	PA_NO_MEMORY,     /* No memory left */
	PA_TIMER_ERROR,   /* Error in timer */
	PA_EXTRACT_USER,  /* Error while extracting username from R-URI */
	PA_FROM_ERR,      /* From malformed or missing */
	PA_TO_ERR,        /* To malformed or missing */
	PA_SMALL_BUFFER,  /* Buffer too small */
	PA_UNSUPP_DOC,    /* Unsupported presence document format */
	PA_ACCEPT_PARSE,  /* Error while parsing Accept header field */
	PA_URI_PARSE,     /* Error while parsing URI */
	PA_DIALOG_ERR,    /* Error while creating dialog */
	PA_INTERNAL_ERROR, /* Internal server error */
	PA_SUBSCRIPTION_REJECTED,
	PA_NO_MATCHING_TUPLE,	/* there is no tuple with published SIP-ETag */
	PA_OK_WAITING_FOR_AUTH,	/* OK but waiting for auth -> should return 202 */
	PA_SUBSCRIPTION_NOT_EXISTS /* -> 481 */
} paerr_t;


extern paerr_t paerrno;


#endif /* PAERRNO_H */
