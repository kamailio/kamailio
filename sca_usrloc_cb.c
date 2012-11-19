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
#include <sys/types.h>
#include <stdlib.h>

#include "sca_common.h"

#include "sca.h"
#include "sca_appearance.h"
#include "sca_event.h"
#include "sca_subscribe.h"
#include "sca_usrloc_cb.h"

    static const char *
sca_name_from_contact_event_type( int type )
{
    static const char	*name = "unknown contact change event";

    switch ( type ) {
    case UL_CONTACT_INSERT:
	name = "insert";
	break;

    case UL_CONTACT_UPDATE:
	name = "update";
	break;

    case UL_CONTACT_EXPIRE:
	name = "expire";
	break;

    case UL_CONTACT_DELETE:
	name = "delete";
	break;

    default:
	break;
    }

    return( name );
}

    void
sca_contact_change_cb( ucontact_t *c, int type, void *param )
{
    const char		*event_name = sca_name_from_contact_event_type( type );

    LM_INFO( "ADMORTEN DEBUG: contact change: %s %.*s", event_name,
	    STR_FMT( &c->c ));

    if ( type == UL_CONTACT_INSERT || type == UL_CONTACT_UPDATE ) {
	return;
    }

    if ( !sca_uri_is_shared_appearance( sca, &c->aor )) {
	LM_DBG( "%.*s is not a shared appearance line", STR_FMT( &c->aor ));
	return;
    }

    if ( sca_subscription_delete_subscriber_for_event( sca, &c->c,
		&SCA_EVENT_NAME_CALL_INFO, &c->aor ) < 0 ) {
	LM_ERR( "Failed to delete %.*s %.*s subscription on %s",
		STR_FMT( &c->c ), STR_FMT( &SCA_EVENT_NAME_CALL_INFO ),
		event_name );
    }
}
