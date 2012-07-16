#include <sys/types.h>
#include <stdlib.h>

#include "sca_common.h"

#include "sca.h"
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
    sca_mod		*scam = (sca_mod *)param;

    LM_INFO( "contact change: %s %.*s", event_name, STR_FMT( &c->c ));

    assert( scam != NULL );
}
