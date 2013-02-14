/*
 * $Id$
 *
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for sip-router, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */
#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_event.h"
#include "sca_reply.h"

    int
sca_reply( sca_mod *scam, int status_code, char *status_msg,
	int event_type, int expires, sip_msg_t *msg )
{
    str		status_str = STR_NULL;
    str		extra_headers = STR_NULL;
    char	hdr_buf[ 1024 ];
    int		len;

    assert( scam != NULL && scam->sl_api != NULL );
    assert( msg != NULL );

    if ( event_type != SCA_EVENT_TYPE_CALL_INFO &&
		event_type != SCA_EVENT_TYPE_LINE_SEIZE ) {
	LM_ERR( "Unrecognized event type %d", event_type );
	return( -1 );
    }

    status_str.s = status_msg;
    status_str.len = strlen( status_msg );

    if ( status_code < 300 ) {
	/* Add Event, Contact, Allow-Events and Expires headers */
	extra_headers.s = hdr_buf;
	len = snprintf( extra_headers.s, sizeof( hdr_buf ),
		"Event: %s%s", sca_event_name_from_type( event_type ), CRLF );
	extra_headers.len = len;

	SCA_STR_APPEND_CSTR( &extra_headers, "Contact: " );
	SCA_STR_APPEND( &extra_headers, &REQ_LINE( msg ).uri );
	SCA_STR_APPEND_CSTR( &extra_headers, CRLF );

	SCA_STR_COPY_CSTR( &extra_headers,
		"Allow-Events: call-info, line-seize" CRLF );

	SCA_STR_COPY_CSTR( &extra_headers, "Expires: " );

	len = snprintf( extra_headers.s + extra_headers.len,
		  sizeof( hdr_buf ) - extra_headers.len,
		  "%d%s", expires, CRLF );
	extra_headers.len += len;

	if ( add_lump_rpl( msg, extra_headers.s, extra_headers.len,
			   LUMP_RPL_HDR ) == NULL ) {
	    LM_ERR( "Failed to add Allow-Events and Expires headers" );
	    return( -1 );
	}
    } else if ( status_code == 480 ) {
	/* tell loser of line-seize SUBSCRIBE race to try again shortly */ 
	extra_headers.s = hdr_buf;
	len = snprintf( extra_headers.s, sizeof( hdr_buf ),
			"Retry-After: %d%s", 1, CRLF );
	extra_headers.len = len;

	if ( add_lump_rpl( msg, extra_headers.s, extra_headers.len,
			    LUMP_RPL_HDR ) == NULL ) {
	    LM_ERR( "sca_reply: failed to add Retry-After header" );
	    return( -1 );
	}
    }

    if ( scam->sl_api->freply( msg, status_code, &status_str ) < 0 ) {
	LM_ERR( "Failed to send \"%d %s\" reply to %.*s",
		status_code, status_msg,
		get_from( msg )->body.len, get_from( msg )->body.s );
	return( -1 );
    }

    return( 0 );
}
