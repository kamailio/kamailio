#include "sca_common.h"

#include "sca_db.h"
#include "sca_subscribe.h"

const char	    *sca_subscriptions_column_names[] = {
			"subscriber", "aor", "event", "expires", "state",
			"app_idx", "call_id", "from_tag", "to_tag",
			"notify_cseq", "subscribe_cseq",
			NULL
		    };

    void
sca_db_subscriptions_get_value_for_column( int column, db_val_t *row_values,
	void *column_value )
{
    assert( column_value != NULL );
    assert( row_values != NULL );
    assert( column >= 0 && column < SCA_DB_SUBS_BOUNDARY );

    switch ( column ) {
    case SCA_DB_SUBS_SUBSCRIBER_COL:
    case SCA_DB_SUBS_AOR_COL:
    case SCA_DB_SUBS_CALL_ID_COL:
    case SCA_DB_SUBS_FROM_TAG_COL:
    case SCA_DB_SUBS_TO_TAG_COL:
	((str *)column_value)->s = (char *)row_values[ column ].val.string_val;
	((str *)column_value)->len = strlen(((str *)column_value)->s );
	break;

    case SCA_DB_SUBS_EXPIRES_COL:
	*((time_t *)column_value) = row_values[ column ].val.time_val;
	break;

    case SCA_DB_SUBS_EVENT_COL:
    case SCA_DB_SUBS_STATE_COL:
    case SCA_DB_SUBS_NOTIFY_CSEQ_COL:
    case SCA_DB_SUBS_SUBSCRIBE_CSEQ_COL:
	*((int *)column_value) = row_values[ column ].val.int_val;
	break;

    default:
	column_value = NULL;
    }
}

    str **
sca_db_subscriptions_columns( void )
{
    static str		subscriber = STR_STATIC_INIT( "subscriber" );
    static str		aor = STR_STATIC_INIT( "aor" );
    static str		event = STR_STATIC_INIT( "event" );
    static str		expires = STR_STATIC_INIT( "expires" );
    static str		state = STR_STATIC_INIT( "state" );
    static str		app_idx = STR_STATIC_INIT( "app_idx" );
    static str		call_id = STR_STATIC_INIT( "call_id" );
    static str		from_tag = STR_STATIC_INIT( "from_tag" );
    static str		to_tag = STR_STATIC_INIT( "to_tag" );
    static str		notify_cseq = STR_STATIC_INIT( "notify_cseq" );
    static str		subscribe_cseq = STR_STATIC_INIT( "subscribe_cseq" );

    static str		*subs_columns[] = {
			    &subscriber,
			    &aor,
			    &event,
			    &expires,
			    &state,
			    &app_idx,
			    &call_id,
			    &from_tag,
			    &to_tag,
			    &notify_cseq,
			    &subscribe_cseq,
			    NULL
			};

    return( subs_columns );
}
