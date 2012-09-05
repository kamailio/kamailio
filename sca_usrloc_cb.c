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
