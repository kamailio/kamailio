/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_event.h"
#include "sca_reply.h"

    int
sca_reply( sca_mod *scam, int status_code, char *status_msg,
	str *extra_headers, sip_msg_t *msg )
{
    str		status_str = STR_NULL;

    assert( scam != NULL && scam->sl_api != NULL );
    assert( msg != NULL );

    status_str.s = status_msg;
    status_str.len = strlen( status_msg );

    if ( extra_headers && extra_headers->len ) {
        if ( add_lump_rpl( msg, extra_headers->s, extra_headers->len,
                        LUMP_RPL_HDR ) == NULL ) {
            LM_ERR("sca_subscription_reply: failed to add Retry-After header");
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
