#include "sca_common.h"

#include "sca_rpc.h"

#include "sca.h"
#include "sca_appearance.h"
#include "sca_call_info.h"
#include "sca_event.h"
#include "sca_hash.h"
#include "sca_notify.h"
#include "sca_subscribe.h"

const char *sca_rpc_show_all_subscriptions_doc[] = {
	"Show all shared call appearance subscriptions",
	NULL
};
const char *sca_rpc_show_subscription_doc[] = {
	"Show details of a single shared call appearance subscription",
	NULL
};
const char *sca_rpc_show_subscribers_doc[] = {
	"Show contact URIs for all call-info subscribers",
	NULL
};
const char *sca_rpc_deactivate_all_subscriptions_doc[] = {
	"Send NOTIFYs with Subscription-State: "
		"terminated;reason=deactivated to all subscribers",
	NULL
};
const char *sca_rpc_deactivate_subscription_doc[] = {
	"Send NOTIFY with Subscription-State: "
		"terminated;reason=deactivated to a single subscriber",
	NULL
};
const char *sca_rpc_show_all_appearances_doc[] = {
	"Show appearance state for all SCA accounts of record (AoR)",
	NULL
};
const char *sca_rpc_show_appearance_doc[] = {
	"Show appearance state for a single SCA account of record (AoR)",
	NULL
};
const char *sca_rpc_seize_appearance_doc[] = {
	"Seize an appearance on an SCA line",
	NULL
};
const char *sca_rpc_update_appearance_doc[] = {
	"Update the state of a seized appearance on an SCA line",
	NULL
};
const char *sca_rpc_release_appearance_doc[] = {
	"Release a seized or active SCA appearance",
	NULL
};

    void
sca_rpc_show_all_subscriptions( rpc_t *rpc, void *ctx )
{
    sca_hash_table	*ht;
    sca_hash_entry	*ent;
    sca_subscription	*sub;
    str			sub_state = STR_NULL;
    int			i;
    int			rc = 0;

    if (( ht = sca->subscriptions ) == NULL ) {
	rpc->fault( ctx, 500, "Empty subscription table!" );
	return;
    }

    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( ent = ht->slots[ i ].entries; ent != NULL; ent = ent->next ) {
	    sub = (sca_subscription *)ent->value;
	    sca_subscription_state_to_str( sub->state, &sub_state );

	    rc = rpc->printf( ctx, "%d: %.*s %.*s %s %d %.*s", i,
				STR_FMT( &sub->target_aor ),
				STR_FMT( &sub->subscriber ),
				sca_event_name_from_type( sub->event ),
				sub->expires,
				STR_FMT( &sub_state ));

	    if ( rc < 0 ) {
		/* make sure we unlock below */
		break;
	    }
	}

	sca_hash_table_unlock_index( ht, i );

	if ( rc < 0 ) {
	    return;
	}
    }
}

    void
sca_rpc_deactivate_all_subscriptions( rpc_t *rpc, void *ctx )
{
    sca_hash_table	*ht;
    sca_hash_entry	*ent;
    sca_subscription	*sub;
    int			i;
    int			rc = 0;

    if (( ht = sca->subscriptions ) == NULL ) {
	rpc->fault( ctx, 500, "Empty subscription table!" );
    }

    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( ent = ht->slots[ i ].entries; ent != NULL; ent = ent->next ) {
	    sub = (sca_subscription *)ent->value;
	    sub->state = SCA_SUBSCRIPTION_STATE_TERMINATED_DEACTIVATED;
	    sub->expires = 0;
	    sub->dialog.notify_cseq += 1;

	    rpc->printf( ctx, "Deactivating %s subscription from %.*s",
			sca_event_name_from_type( sub->event ),
			STR_FMT( &sub->subscriber ));
	    if ( rc < 0 ) {
		/* make sure we unlock below */
		break;
	    }

	    rc = sca_notify_subscriber( sca, sub,
			SCA_CALL_INFO_APPEARANCE_INDEX_ANY );
	    if ( rc < 0 ) {
		/* make sure we unlock below */
		break;
	    }
	}

	sca_hash_table_unlock_index( ht, i );

	if ( rc < 0 ) {
	    return;
	}
    }
}

    void
sca_rpc_deactivate_subscription( rpc_t *rpc, void *ctx )
{
    rpc->fault( ctx, 600, "Not implemented" );
}

    void
sca_rpc_show_subscription( rpc_t *rpc, void *ctx )
{
    sca_hash_table	*ht = NULL;
    sca_hash_slot	*slot;
    sca_hash_entry	*ent;
    sca_subscription	*sub;
    str			sub_key = STR_NULL;
    str			aor = STR_NULL;
    str			contact = STR_NULL;
    str			event_name = STR_NULL;
    int			event_type;
    int			idx = -1;
    int			rc = 0;
    char		keybuf[ 1024 ];
    char		*usage = "usage: sca.show_subscription user@domain "
				 "{ call-info | line-seize } [user@IP]";
    char		*err_msg = NULL;
    int			err_code = 0;

    /* AoR is required */
    if ( rpc->scan( ctx, "SS", &aor, &event_name ) != 2 ) {
	rpc->fault( ctx, 500, usage );
    }

    event_type = sca_event_from_str( &event_name );
    if ( event_type == SCA_EVENT_TYPE_UNKNOWN ) {
	err_code = 500;
	err_msg = usage;
	goto done;
    }

    if (( ht = sca->subscriptions ) == NULL ) {
	rpc->fault( ctx, 500, "Empty subscription table!" );
	return;
    }

    sub_key.s = keybuf;
    if ( aor.len + event_name.len >= sizeof( keybuf )) {
	rpc->fault( ctx, 500, "AoR length + event name length: too long" );
    }
    SCA_STR_COPY( &sub_key, &aor );
    SCA_STR_APPEND( &sub_key, &event_name );

LM_INFO( "ADMORTEN: sub_key: %.*s", STR_FMT( &sub_key ));

    idx = sca_hash_table_index_for_key( ht, &sub_key );
LM_INFO( "ADMORTEN: hash table index for %.*s: %d", STR_FMT( &sub_key ), idx );
    sca_hash_table_lock_index( ht, idx );

    /* Contact is optional */
    if ( rpc->scan( ctx, "*S", &contact ) == 1 ) {
	slot = sca_hash_table_slot_for_index( ht, idx );

	/* we lock above */
	sub = sca_hash_table_slot_kv_find_unsafe( slot, &contact );
	if ( sub == NULL ) {
	    err_code = 404;
	    err_msg = "No matching subscriptions found";
	    goto done;
	}

	rc = rpc->printf( ctx, "%.*s %s %.*s %d",
			    STR_FMT( &sub->target_aor ),
			    sca_event_name_from_type( sub->event ),
			    STR_FMT( &sub->subscriber ),
			    sub->expires );
    } else {
	for ( ent = ht->slots[ idx ].entries; ent != NULL; ent = ent->next ) {
	    sub = (sca_subscription *)ent->value;
	    rc = rpc->printf( ctx, "%.*s %s %.*s %d",
				STR_FMT( &sub->target_aor ),
				sca_event_name_from_type( sub->event ),
				STR_FMT( &sub->subscriber ),
				sub->expires );

	    if ( rc < 0 ) {
		/* make sure we unlock below */
		break;
	    }
	}
    }

done:
    if ( ht && idx >= 0 ) {
	sca_hash_table_unlock_index( ht, idx );
    }

    if ( err_code != 0 ) {
	rpc->fault( ctx, err_code, err_msg );
    }
}

    void
sca_rpc_show_subscribers( rpc_t *rpc, void *ctx )
{
    rpc->fault( ctx, 600, "Not yet implemented" );
}

    void
sca_rpc_show_all_appearances( rpc_t *rpc, void *ctx )
{
    sca_hash_table	*ht;
    sca_hash_entry	*ent;
    sca_appearance_list	*app_list;
    sca_appearance	*app;
    str			state_str = STR_NULL;
    int			i;
    int			rc = 0;

    if (( ht = sca->appearances ) == NULL ) {
	return;
    }

    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( ent = ht->slots[ i ].entries; ent != NULL; ent = ent->next ) {
	    app_list = (sca_appearance_list *)ent->value;
	    for ( app = app_list->appearances; app != NULL; app = app->next ) {
		sca_appearance_state_to_str( app->state, &state_str );
		rc = rpc->printf( ctx, "%d: %.*s %d %.*s %.*s %.*s", i,
				STR_FMT( &app_list->aor ),
				app->index,
				STR_FMT( &state_str ),
				STR_FMT( &app->owner ),
				STR_FMT( &app->dialog.id ));
		if ( rc < 0 ) {
		    /* make sure we unlock below */
		    goto error;
		}
	    }
	}

	sca_hash_table_unlock_index( ht, i );
    }

    return;

error:
    sca_hash_table_unlock_index( ht, i );
    return;
}

    void
sca_rpc_show_appearance( rpc_t *rpc, void *ctx )
{
    rpc->fault( ctx, 600, "Not yet implemented" );
}

    void
sca_rpc_seize_appearance( rpc_t *rpc, void *ctx )
{
    str			aor = STR_NULL;
    str			owner = STR_NULL;
    int			app_idx;
    char		*usage = "usage: sca.seize_appearance sip:user@domain";

    /* AoR & Contact are required */
    if ( rpc->scan( ctx, "SS", &aor, &owner ) != 2 ) {
	rpc->fault( ctx, 500, usage );
	return;
    }

    app_idx = sca_appearance_seize_next_available_index( sca, &aor, &owner );
    if ( app_idx < 0 ) {
	rpc->fault( ctx, 500, "Failed to seize line" );
	return;
    }

    rpc->printf( ctx, "Seized %.*s appearance-index %d for %.*s",
		STR_FMT( &aor ), app_idx, STR_FMT( &owner ));

    if ( sca_notify_call_info_subscribers( sca, &aor ) < 0 ) {
	rpc->fault( ctx, 500, "Failed to NOTIFY subscribers to %.*s",
		    STR_FMT( &aor ));
    }
}

    void
sca_rpc_update_appearance( rpc_t *rpc, void *ctx )
{
    str			aor = STR_NULL;
    str			app_state_str = STR_NULL;
    str			app_uri = STR_NULL, *app_uri_p = NULL;
    int			app_idx;
    int			app_state;
    int			rc;
    char		*usage = "Usage: sca.update_appearance "
				 "sip:user@domain appearance-index "
				 "appearance-state [appearance-uri]";
    
    if ( rpc->scan( ctx, "SdS", &aor, &app_idx, &app_state_str ) < 3 ) {
	rpc->fault( ctx, 500, "%s", usage );
	return;
    }
    if ( rpc->scan( ctx, "*S", &app_uri ) == 1 ) {
	if ( !SCA_STR_EMPTY( &app_uri )) {
	    app_uri_p = &app_uri;
	}
    }

    app_state = sca_appearance_state_from_str( &app_state_str );
    if ( app_state == SCA_APPEARANCE_STATE_UNKNOWN ) {
	rpc->fault( ctx, 500, "%.*s: invalid state", STR_FMT( &app_state_str ));
	return;
    }

    rc = sca_appearance_update_index( sca, &aor, app_idx,
				      app_state, NULL, app_uri_p, NULL );
    if ( rc != SCA_APPEARANCE_OK ) {
	rpc->fault( ctx, 500, "Failed to update %.*s appearance-index %d",
		    STR_FMT( &aor ), app_idx );
	return;
    }

    if ( sca_notify_call_info_subscribers( sca, &aor ) < 0 ) {
	rpc->fault( ctx, 500, "Failed to NOTIFY subscribers to %.*s",
		    STR_FMT( &aor ));
    }
}

    void
sca_rpc_release_appearance( rpc_t *rpc, void *ctx )
{
    sca_hash_table	*ht = NULL;
    sca_hash_entry	*ent;
    sca_appearance_list	*app_list = NULL;
    sca_appearance	*app = NULL;
    str			aor = STR_NULL;
    int			idx = -1;
    int			app_idx;
    char		*usage = "usage: sca.release_appearance user@domain "
				 "appearance-index";
    char		*err_msg = NULL;
    int			err_code = 0;

    /* AoR & appearance-index are required */
    if ( rpc->scan( ctx, "Sd", &aor, &app_idx ) != 2 ) {
	rpc->fault( ctx, 500, usage );
	return;
    }

    if (( ht = sca->appearances ) == NULL ) {
	rpc->fault( ctx, 500, "No active appearances" );
	return;
    }

    idx = sca_hash_table_index_for_key( ht, &aor );
    sca_hash_table_lock_index( ht, idx );

    for ( ent = ht->slots[ idx ].entries; ent != NULL; ent = ent->next ) {
	if ( ent->compare( &aor, ent->value ) == 0 ) {
	    app_list = (sca_appearance_list *)ent->value;
	    break;
	}
    }
    if ( app_list == NULL ) {
	rpc->fault( ctx, 500, "No appearances for %.*s", STR_FMT( &aor ));
	goto done;
    }

    app = sca_appearance_list_unlink_index( app_list, app_idx );
    if ( app == NULL ) {
	rpc->fault( ctx, 500, "%.*s appearance index %d is not in use",
		STR_FMT( &aor ), app_idx );
	goto done;
    }
    sca_appearance_free( app );

done:
    if ( ht && idx >= 0 ) {
	sca_hash_table_unlock_index( ht, idx );
    }

    if ( app != NULL ) {
	if ( sca_notify_call_info_subscribers( sca, &aor ) < 0 ) {
	    rpc->fault( ctx, 500, "Failed to NOTIFY subscribers to %.*s",
			STR_FMT( &aor ));
	}
    }

    if ( err_code != 0 ) {
	rpc->fault( ctx, err_code, err_msg );
    }
}
