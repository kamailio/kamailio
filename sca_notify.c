#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_call_info.h"
#include "sca_event.h"
#include "sca_notify.h"

#include "../../modules/tm/tm_load.h"


const str		SCA_METHOD_NOTIFY = STR_STATIC_INIT( "NOTIFY" );

    static void
sca_notify_reply_cb( struct cell *t, int cb_type, struct tmcb_params *cbp )
{
    struct sip_msg	*notify_reply = NULL;

    if ( cbp == NULL ) {
	LM_ERR( "Empty parameters passed to NOTIFY callback!" );
	return;
    }
    if (( notify_reply = cbp->rpl ) == NULL ) {
	LM_ERR( "Empty reply passed to NOTIFY callback!" );
	return;
    }
    if ( notify_reply == FAKED_REPLY ) {
	/* XXX should hook this and remove subscriber */
	LM_ERR( "NOTIFY resulted in FAKED_REPLY from proxy" );
	return;
    }

    LM_INFO( "NOTIFY returned status %d", notify_reply->REPLY_STATUS );
}

    static dlg_t *
sca_notify_dlg_for_subscription( sca_subscription *sub )
{
    dlg_t		*dlg;

    dlg = (dlg_t *)pkg_malloc( sizeof( dlg_t ));
    if ( dlg == NULL ) {
	LM_ERR( "pkg_malloc dlg_t for %.*s failed: out of memory",
		STR_FMT( &sub->subscriber ));
	return( NULL );
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

    /*
     * the dialog state in an SCA NOTIFY should always be confirmed,
     * since we generated the dialog to-tag in our response to the
     * subscriber's SUBSCRIBE request.
     */
    dlg->state = DLG_CONFIRMED;

    return( dlg );
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

#define SCA_HEADERS_MAX_LEN	4096
    int
sca_notify_subscriber( sca_mod *scam, sca_subscription *sub, int app_idx )
{
    uac_req_t		request;
    dlg_t		*dlg = NULL;
    str			headers = STR_NULL;
    char		hdrbuf[ SCA_HEADERS_MAX_LEN ];
    int			len;
    int			rc = -1;

    headers.s = hdrbuf;

    dlg = sca_notify_dlg_for_subscription( sub );
    if ( dlg == NULL ) {
	LM_ERR( "Failed to create dlg_t for %s NOTIFY to %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto done;
    }

    /* XXX move header construction to separate routine */
    len = sca_notify_append_contact_header( sub, hdrbuf + headers.len,
		sizeof( hdrbuf ) - headers.len );
    if ( len < 0 ) {
	LM_ERR( "Failed to add Contact header to %s NOTIFY for %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto done;
    }
    headers.len += len;
    
    if ( app_idx == SCA_CALL_INFO_APPEARANCE_INDEX_ANY ) {
	/* add Call-Info header with appearance state */
	if (( len = sca_call_info_build_header( scam, sub,
			hdrbuf + headers.len,
			sizeof( hdrbuf ) - headers.len )) < 0 ) {
	    LM_ERR( "Failed to build Call-Info Headers for %s NOTIFY to %.*s",
		    sca_event_name_from_type( sub->event ),
		    STR_FMT( &sub->subscriber ));
	    goto done;
	}
    } else {
	/* just add Call-Info header with single appearance index */
	len = sca_call_info_append_header_for_appearance_index( sub, app_idx,
			hdrbuf + headers.len, sizeof( hdrbuf ) - headers.len );
	if ( len < 0 ) {
	    goto done;
	}
    }
	
    headers.len += len;

    LM_INFO( "ADMORTEN: Call-Info Header for %s NOTIFY to %.*s: \"%.*s\"",
	    sca_event_name_from_type( sub->event ), STR_FMT( &sub->subscriber ),
	    STR_FMT( &headers ));

    len = sca_event_append_header_for_type( sub->event,
		hdrbuf + headers.len, sizeof( hdrbuf ) - headers.len );
    if ( len < 0 ) {
	LM_ERR( "Failed to add Event header to %s NOTIFY for %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto done;
    }
    headers.len += len;

    len = sca_notify_append_subscription_state_header( sub,
		hdrbuf + headers.len, sizeof( hdrbuf ) - headers.len );
    if ( len < 0 ) {
	LM_ERR( "Failed to add Subscription-State header to %s NOTIFY for "
		"%.*s", sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	goto done;
    }
    headers.len += len;

    set_uac_req( &request, (str *)&SCA_METHOD_NOTIFY, &headers, NULL, dlg,
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
	pkg_free( dlg );
    }

    return( rc );
}

/* send a call-info NOTIFY to all subscribers to a given SCA AoR. */
    int
sca_notify_call_info_subscribers( sca_mod *scam, str *subscription_aor )
{
    sca_hash_slot		*slot;
    sca_hash_entry		*e;
    sca_subscription		*sub;
    str				hash_key = STR_NULL;
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
	    LM_INFO( "ADMORTEN: %.*s (%d) does not match %.*s (%d)",
			STR_FMT( subscription_aor ), subscription_aor->len,
			STR_FMT( &sub->target_aor ), sub->target_aor.len );
	    continue;
	}

	/* XXX would like this to be wrapped in one location */
	sub->dialog.notify_cseq += 1;

	/*
	 * XXX this rebuilds the same headers repeatedly. use a static
	 * buffer instead.
	 */
	if ( sca_notify_subscriber( scam, sub,
		SCA_CALL_INFO_APPEARANCE_INDEX_ANY ) < 0 ) {
	    goto done;
	}
    }
    rc = 1;
    
done:
    sca_hash_table_unlock_index( scam->subscriptions, slot_idx );

    return( rc );
}
