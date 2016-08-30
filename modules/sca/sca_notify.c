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

#include <assert.h>

#include "sca.h"
#include "sca_call_info.h"
#include "sca_event.h"
#include "sca_notify.h"
#include "sca_util.h"

#include "../../modules/tm/tm_load.h"


const str		SCA_METHOD_NOTIFY = STR_STATIC_INIT( "NOTIFY" );

    static void
sca_notify_reply_cb( struct cell *t, int cb_type, struct tmcb_params *cbp )
{
    struct sip_msg	*notify_reply = NULL;
    str			to_aor = STR_NULL;
    str			*contact_uri;

    if ( cbp == NULL ) {
	LM_ERR( "Empty parameters passed to NOTIFY callback!" );
	return;
    }
    if (( notify_reply = cbp->rpl ) == NULL ) {
	LM_ERR( "Empty reply passed to NOTIFY callback!" );
	return;
    }

    contact_uri = &t->uac[ 0 ].uri;
    if ( notify_reply != FAKED_REPLY && REPLY_CLASS( notify_reply ) == 2 ) {
	LM_DBG( "NOTIFY %.*s returned %d", STR_FMT( contact_uri ),
					notify_reply->REPLY_STATUS );
	return;
    }

    /*
     * after this, we've either gotten an error from the client, or a faked
     * reply from the proxy. remove the subscription in either case. it's
     * possible the client will return 481 (no such transaction), but that's
     * still grounds for us to remove the subscription, since the dialog
     * we have associated with the subscription is no longer valid.
     */
    if ( notify_reply == FAKED_REPLY ) {
	LM_ERR( "NOTIFY %.*s resulted in FAKED_REPLY from proxy: "
		"failed to deliver NOTIFY to client", STR_FMT( contact_uri ));
    } else {
	LM_ERR( "NOTIFY %.*s returned %d %.*s removing call-info "
		"subscription for %.*s", STR_FMT( contact_uri ),
		notify_reply->REPLY_STATUS,
		STR_FMT( &notify_reply->first_line.u.reply.reason ),
		STR_FMT( contact_uri ));
    }
    return;

    if ( sca_uri_extract_aor( &t->to, &to_aor ) < 0 ) {
	LM_ERR( "Failed to extract AoR from %.*s", STR_FMT( &t->to ));
	return;
    }
    /* t->to is the entire To header: "To: sip:....", so move to_aor.s ahead */
    if ( memcmp( to_aor.s, "To: ", strlen( "To: " )) == 0 ) {
	to_aor.s += strlen( "To: " );
	to_aor.len -= strlen( "To: " );
    }

    if ( sca_subscription_delete_subscriber_for_event( sca, contact_uri,
		&SCA_EVENT_NAME_CALL_INFO, &to_aor ) < 0 ) {
	LM_ERR( "Failed to delete %.*s %.*s subscription",
		STR_FMT( contact_uri ), STR_FMT( &SCA_EVENT_NAME_CALL_INFO ));
    }
}

    static dlg_t *
sca_notify_dlg_for_subscription( sca_subscription *sub )
{
    dlg_t		*dlg = NULL;

    dlg = (dlg_t *)pkg_malloc( sizeof( dlg_t ));
    if ( dlg == NULL ) {
	LM_ERR( "pkg_malloc dlg_t for %.*s failed: out of memory",
		STR_FMT( &sub->subscriber ));
	goto error;
    }
    memset( dlg, 0, sizeof( dlg_t ));

    dlg->loc_seq.value = sub->dialog.notify_cseq;
    dlg->loc_seq.is_set = 1;

    dlg->id.call_id = sub->dialog.call_id;
    dlg->id.rem_tag = sub->dialog.from_tag;
    dlg->id.loc_tag = sub->dialog.to_tag;

    /* RURI */
    dlg->rem_target = sub->subscriber;

    /* To and From URIs are both the SCA AoR in an SCA NOTIFY */
    dlg->loc_uri = sub->target_aor;
    dlg->rem_uri = sub->target_aor;

    /* restore route */
    if ( !SCA_STR_EMPTY( &sub->rr )) {
	if ( parse_rr_body( sub->rr.s, sub->rr.len, &dlg->route_set ) < 0 ) {
	    LM_ERR( "sca_notify_dlg_for_subscription: failed to parse "
		    "%.*s subscription's Record-Route info",
		    STR_FMT( &sub->subscriber ));
	    goto error;
	}
    }

    /*
     * the dialog state in an SCA NOTIFY should always be confirmed,
     * since we generated the dialog to-tag in our response to the
     * subscriber's SUBSCRIBE request.
     */
    dlg->state = DLG_CONFIRMED;

    return( dlg );

error:
    if ( dlg != NULL ) {
	pkg_free( dlg );
    }

    return( NULL );
}

    static int
sca_notify_append_subscription_state_header( sca_subscription *sub,
	char *hdrbuf, int maxlen )
{
    str		state_str = STR_NULL;
    int		len, total = 0;
    int		ttl = sub->expires - time( NULL );
    
    if ( ttl < 0 ) {
	ttl = 0;
    }

    sca_subscription_state_to_str( sub->state, &state_str );
    len = snprintf( hdrbuf, maxlen, "Subscription-State: %s", state_str.s );
    if ( len >= maxlen ) {
	goto error;
    }
    total += len;

    if ( ttl > 0 ) {
	len = snprintf( hdrbuf + total, maxlen - total, ";expires=%d", ttl );
	if ( len >= maxlen ) {
	    goto error;
	}
	total += len;
    }

    len = snprintf( hdrbuf + total, maxlen - total, "%s", CRLF );
    total += len;

    return( total );

error:
    LM_ERR( "Cannot append Subscription-State header: buffer too small" );
    return( -1 );
}

    static int
sca_notify_append_contact_header( sca_subscription *sub,
	char *hdrbuf, int maxlen )
{
    int		len = strlen( "Contact: " );

    if ( len + sub->target_aor.len + strlen( CRLF ) >= maxlen ) {
	LM_ERR( "Cannot append Contact header: buffer too small" );
	return( -1 );
    }

    memcpy( hdrbuf, "Contact: ", len );
    memcpy( hdrbuf + len, sub->target_aor.s, sub->target_aor.len );
    len += sub->target_aor.len;
    memcpy( hdrbuf + len, CRLF, strlen( CRLF ));
    len += strlen( CRLF );

    return( len );
}

    static int
sca_notify_build_headers_from_info( str *hdrs, int max_hdrs_len, sca_mod *scam,
	sca_subscription *sub, int app_idx )
{
    int		len = 0;

    assert( hdrs != NULL );

    len = sca_notify_append_contact_header( sub, hdrs->s, max_hdrs_len );
    if ( len < 0 ) {
	LM_ERR( "Failed to add Contact header to %s NOTIFY for %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto error;
    }
    hdrs->len = len;
    
    if ( app_idx == SCA_CALL_INFO_APPEARANCE_INDEX_ANY ) {
	/* add Call-Info header with appearance state */
	if (( len = sca_call_info_build_header( scam, sub,
			hdrs->s + hdrs->len,
			max_hdrs_len - hdrs->len )) < 0 ) {
	    LM_ERR( "Failed to build Call-Info Headers for %s NOTIFY to %.*s",
		    sca_event_name_from_type( sub->event ),
		    STR_FMT( &sub->subscriber ));
	    goto error;
	}
    } else {
	/* just add Call-Info header with single appearance index */
	len = sca_call_info_append_header_for_appearance_index( sub, app_idx,
			hdrs->s + hdrs->len, max_hdrs_len - hdrs->len );
	if ( len < 0 ) {
	    goto error;
	}
    }
	
    hdrs->len += len;

    len = sca_event_append_header_for_type( sub->event,
		hdrs->s + hdrs->len, max_hdrs_len - hdrs->len );
    if ( len < 0 ) {
	LM_ERR( "Failed to add Event header to %s NOTIFY for %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto error;
    }
    hdrs->len += len;

    len = sca_notify_append_subscription_state_header( sub,
		hdrs->s + hdrs->len, max_hdrs_len - hdrs->len );
    if ( len < 0 ) {
	LM_ERR( "Failed to add Subscription-State header to %s NOTIFY for "
		"%.*s", sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto error;
    }
    hdrs->len += len;

    return( hdrs->len );

error:
    return( -1 );
}
	
    static int
sca_notify_subscriber_internal( sca_mod *scam, sca_subscription *sub,
	str *headers )
{
    uac_req_t		request;
    dlg_t		*dlg = NULL;
    int			rc = -1;

    dlg = sca_notify_dlg_for_subscription( sub );
    if ( dlg == NULL ) {
	LM_ERR( "Failed to create dlg_t for %s NOTIFY to %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto done;
    }

    set_uac_req( &request, (str *)&SCA_METHOD_NOTIFY, headers, NULL, dlg,
			TMCB_LOCAL_COMPLETED, sca_notify_reply_cb, scam );
    rc = scam->tm_api->t_request_within( &request );
    if ( rc < 0 ) {
	LM_ERR( "Failed to send in-dialog %s NOTIFY to %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
    }
    /* fall through, return rc from t_request_within */

done:
    if ( dlg != NULL ) {
	if ( dlg->route_set != NULL ) {
	    free_rr( &dlg->route_set );
	}

	pkg_free( dlg );
    }

    return( rc );
}

#define SCA_HEADERS_MAX_LEN	4096
    int
sca_notify_subscriber( sca_mod *scam, sca_subscription *sub, int app_idx )
{
    str			headers = STR_NULL;
    char		hdrbuf[ SCA_HEADERS_MAX_LEN ];

    headers.s = hdrbuf;

    if ( sca_notify_build_headers_from_info( &headers, sizeof( hdrbuf ),
		scam, sub, app_idx ) < 0 ) {
	LM_ERR( "Failed to build NOTIFY headers" );
	return( -1 );
    }

    return( sca_notify_subscriber_internal( scam, sub, &headers ));
}

/* send a call-info NOTIFY to all subscribers to a given SCA AoR. */
    int
sca_notify_call_info_subscribers( sca_mod *scam, str *subscription_aor )
{
    sca_hash_slot		*slot;
    sca_hash_entry		*e;
    sca_subscription		*sub;
    str				headers = STR_NULL;
    str				hash_key = STR_NULL;
    char			hdrbuf[ SCA_HEADERS_MAX_LEN ];
    char			keybuf[ 512 ];
    char			*event_name;
    int				slot_idx;
    int				rc = -1;

    assert( scam->subscriptions != NULL );
    assert( !SCA_STR_EMPTY( subscription_aor ));

    event_name = sca_event_name_from_type( SCA_EVENT_TYPE_CALL_INFO );

    if ( subscription_aor->len + strlen( event_name ) >= sizeof( keybuf )) {
	LM_ERR( "Hash key %.*s + %s is too long",
		STR_FMT( subscription_aor ), event_name );
	return( -1 );
    }
    hash_key.s = keybuf;
    SCA_STR_COPY( &hash_key, subscription_aor );
    SCA_STR_APPEND_CSTR( &hash_key, event_name );

    slot_idx = sca_hash_table_index_for_key( scam->subscriptions, &hash_key );
    slot = sca_hash_table_slot_for_index( scam->subscriptions, slot_idx );

    sca_hash_table_lock_index( scam->subscriptions, slot_idx );

    for ( e = slot->entries; e != NULL; e = e->next ) {
	sub = (sca_subscription *)e->value;
	if ( !SCA_STR_EQ( subscription_aor, &sub->target_aor )) {
	    continue;
	}

	if ( headers.len == 0 ) {
	    headers.s = hdrbuf;

	    if ( sca_notify_build_headers_from_info( &headers, sizeof( hdrbuf ),
			scam, sub, SCA_CALL_INFO_APPEARANCE_INDEX_ANY ) < 0 ) {
		LM_ERR( "Failed to build NOTIFY headers" );
		goto done;
	    }
	}


	/* XXX would like this to be wrapped in one location */
	sub->dialog.notify_cseq += 1;

	if ( sca_notify_subscriber_internal( scam, sub, &headers ) < 0 ) {
	    goto done;
	}
    }
    rc = 1;
    
done:
    sca_hash_table_unlock_index( scam->subscriptions, slot_idx );

    return( rc );
}
