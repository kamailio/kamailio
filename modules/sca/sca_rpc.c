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
 *
 *
 */
#include "sca_common.h"

#include <sys/types.h>
#include <time.h>

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
const char *sca_rpc_subscription_count_doc[] = {
	"Show count of call-info or line-seize subscriptions",
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
    sip_uri_t		aor_uri, sub_uri;
    str			sub_state = STR_NULL;
    time_t		now;
    int			i;
    int			rc = 0;

    if (( ht = sca->subscriptions ) == NULL ) {
	rpc->fault( ctx, 500, "Empty subscription table!" );
	return;
    }

    now = time( NULL );

    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( ent = ht->slots[ i ].entries; ent != NULL; ent = ent->next ) {
	    sub = (sca_subscription *)ent->value;
	    sca_subscription_state_to_str( sub->state, &sub_state );

	    rc = parse_uri( sub->target_aor.s, sub->target_aor.len, &aor_uri );
	    if ( rc >= 0 ) {
		rc = parse_uri( sub->subscriber.s, sub->subscriber.len,
				&sub_uri );
	    }
	    if ( rc >= 0 ) {
		rc = rpc->rpl_printf( ctx, "%.*s %.*s%s%.*s %s %ld %.*s",
				    STR_FMT( &aor_uri.user ),
				    STR_FMT( &sub_uri.host ),
				    (sub_uri.port.len ? ":" : "" ),
				    STR_FMT( &sub_uri.port ),
				    sca_event_name_from_type( sub->event ),
				    (long)(sub->expires - now),
				    STR_FMT( &sub_state ));
	    } else {
		LM_ERR( "sca_rpc_show_all_subscriptions: parse_uri %.*s "
			"failed, dumping unparsed info",
			STR_FMT( &sub->target_aor ));
		rc = rpc->rpl_printf( ctx, "%.*s %.*s %s %ld %.*s",
				    STR_FMT( &sub->target_aor ),
				    STR_FMT( &sub->subscriber ),
				    sca_event_name_from_type( sub->event ),
				    (long)sub->expires,
				    STR_FMT( &sub_state ));
	    }

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
sca_rpc_subscription_count( rpc_t *rpc, void *ctx )
{
    sca_hash_table	*ht;
    sca_hash_entry	*ent;
    sca_subscription	*sub;
    str			event_name = STR_NULL;
    char		*usage = "usage: sca.subscription_count "
				 "{ call-info | line-seize }";
    unsigned long	sub_count = 0;
    int			i;
    int			event_type;

    if (( ht = sca->subscriptions ) == NULL ) {
	rpc->fault( ctx, 500, "Empty subscription table!" );
	return;
    }

    /* AoR is required */
    if ( rpc->scan( ctx, "S", &event_name ) != 1 ) {
	rpc->fault( ctx, 500, usage );
	return;
    }

    event_type = sca_event_from_str( &event_name );
    if ( event_type == SCA_EVENT_TYPE_UNKNOWN ) {
	rpc->fault( ctx, 500, usage );
	return;
    }

    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( ent = ht->slots[ i ].entries; ent != NULL; ent = ent->next ) {
	    sub = (sca_subscription *)ent->value;

	    if ( event_type == sub->event ) {
		sub_count++;
	    }
	}
	sca_hash_table_unlock_index( ht, i );
    }

    rpc->rpl_printf( ctx, "%ld %.*s", sub_count, STR_FMT( &event_name ));
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

	    rpc->rpl_printf( ctx, "Deactivating %s subscription from %.*s",
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
    sca_hash_entry	*ent;
    sca_subscription	*sub;
    str			sub_key = STR_NULL;
    str			aor = STR_NULL;
    str			contact = STR_NULL;
    str			event_name = STR_NULL;
    int			event_type;
    int			idx = -1;
    int			rc = 0, opt_rc;
    char		keybuf[ 1024 ];
    char		*usage = "usage: sca.show_subscription sip:user@domain "
				 "{ call-info | line-seize } [sip:user@IP]";
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

    idx = sca_hash_table_index_for_key( ht, &sub_key );
    sca_hash_table_lock_index( ht, idx );

    /* Contact is optional */
    opt_rc = rpc->scan( ctx, "*S", &contact );

    for ( ent = ht->slots[ idx ].entries; ent != NULL; ent = ent->next ) {
	sub = (sca_subscription *)ent->value;
	if ( ent->compare( &aor, &sub->target_aor ) != 0 ) {
	    continue;
	}

	if ( opt_rc == 1 ) {
	    if ( !SCA_STR_EQ( &contact, &sub->subscriber )) {
		continue;
	    }
	}

	rc = rpc->rpl_printf( ctx, "%.*s %s %.*s %d",
			    STR_FMT( &sub->target_aor ),
			    sca_event_name_from_type( sub->event ),
			    STR_FMT( &sub->subscriber ),
			    sub->expires );

	if ( rc < 0 ) {
	    /* make sure we unlock below */
	    break;
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
		rc = rpc->rpl_printf( ctx, "%.*s %d %.*s %ld %.*s %.*s "
				"%.*s %.*s %.*s",
				STR_FMT( &app_list->aor ),
				app->index,
				STR_FMT( &state_str ),
				(long)app->times.mtime,
				STR_FMT( &app->owner ),
				STR_FMT( &app->callee ),
				STR_FMT( &app->dialog.call_id ),
				STR_FMT( &app->dialog.from_tag ),
				STR_FMT( &app->dialog.to_tag ));

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

    rpc->rpl_printf( ctx, "Seized %.*s appearance-index %d for %.*s",
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
	app_uri_p = &app_uri;
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
