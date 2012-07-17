#include "sca_common.h"

#include <assert.h>
#include <errno.h>

#include "sca.h"
#include "sca_appearance.h"
#include "sca_call_info.h"
#include "sca_event.h"
#include "sca_notify.h"
#include "sca_reply.h"
#include "sca_subscribe.h"

#include "../../modules/tm/tm_load.h"

extern int	errno;


const str SCA_METHOD_SUBSCRIBE = STR_STATIC_INIT( "SUBSCRIBE" );

struct sca_sub_state_table {
    int		state;
    char	*state_name;
} state_table[] = {
    { SCA_SUBSCRIPTION_STATE_ACTIVE,		"active" },
    { SCA_SUBSCRIPTION_STATE_PENDING,		"pending" },
    { SCA_SUBSCRIPTION_STATE_TERMINATED, 	"terminated" },
    { SCA_SUBSCRIPTION_STATE_TERMINATED_DEACTIVATED, "terminated;reason=deactivated" },
    { SCA_SUBSCRIPTION_STATE_TERMINATED_GIVEUP, "terminated;reason=giveup" },
    { SCA_SUBSCRIPTION_STATE_TERMINATED_NORESOURCE, "terminated;reason=noresource" },
    { SCA_SUBSCRIPTION_STATE_TERMINATED_PROBATION, "terminated;reason=probation" },
    { SCA_SUBSCRIPTION_STATE_TERMINATED_REJECTED, "terminated;reason=rejected" },
    { SCA_SUBSCRIPTION_STATE_TERMINATED_TIMEOUT, "terminated;reason=timeout" },
    
    { -1, NULL },
};

    void
sca_subscription_state_to_str( int state, str *state_str_out )
{
    assert( state >= 0 );
    assert( state < ( sizeof( state_table ) / sizeof( state_table[ 0 ] )));
    assert( state_str_out != NULL );

    state_str_out->len = strlen( state_table[ state ].state_name );
    state_str_out->s = state_table[ state ].state_name;
}

    void
sca_subscription_purge_expired( unsigned int ticks, void *param )
{
    sca_mod		*scam = (sca_mod *)param;
    sca_hash_table	*ht;
    sca_hash_entry	*ent, *ent_tmp;
    sca_subscription	*sub;
    time_t		now = time( NULL );
    int			i;

    assert( scam != NULL );
    assert( scam->subscriptions != NULL );

    LM_INFO( "ADMORTEN: EXPIRED PURGE TICK" );

    ht = scam->subscriptions;
    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( ent = ht->slots[ i ].entries; ent != NULL; ent = ent_tmp ) {
	    ent_tmp = ent->next;

	    sub = (sca_subscription *)ent->value;
	    if ( sub == NULL || sub->expires > now ) {
		continue;
	    }

	    if ( !SCA_SUBSCRIPTION_IS_TERMINATED( sub )) {
		sub->state = SCA_SUBSCRIPTION_STATE_TERMINATED_TIMEOUT;
		sub->expires = 0;
		sub->dialog.notify_cseq += 1;

		if ( sca_notify_subscriber( scam, sub,
				SCA_CALL_INFO_APPEARANCE_INDEX_ALL ) < 0 ) {
		    LM_ERR( "Failed to send subscription expired "
			    "NOTIFY %s subscriber %.*s",
			    sca_event_name_from_type( sub->event ),
			    STR_FMT( &sub->subscriber ));

		    /* remove from subscribers list anyway */
		}
	    }

	    /*
	     * XXX should be in a separate subscription deletion routine.
	     * will need to detect whether subscriber has active appearances,
	     * send notifies to others in group if necessary.
	     */

	    LM_INFO( "ADMORTEN: Deleting expired %s subscription from %.*s",
			sca_event_name_from_type( sub->event ),
			STR_FMT( &sub->subscriber ));

	    /* unlink from hash table entries list */
	    sca_hash_table_slot_unlink_entry_unsafe( &ht->slots[ i ], ent );
	}

	sca_hash_table_unlock_index( ht, i );
    }
}

    sca_subscription *
sca_subscription_create( str *aor, int event, str *subscriber,
	unsigned int cseq, int expire_delta,
	str *call_id, str *from_tag, str *to_tag )
{
    sca_subscription		*sub = NULL;
    int				len = 0;

    len += sizeof( sca_subscription );
    len += sizeof( char ) * ( aor->len + subscriber->len );
    len += sizeof( char ) * ( call_id->len + from_tag->len + to_tag->len );

    sub = (sca_subscription *)shm_malloc( len );
    if ( sub == NULL ) {
	LM_ERR( "Failed to create %s subscription for %.*s: out of memory",
		sca_event_name_from_type( event ), STR_FMT( subscriber ));
	return( NULL );
    }
    memset( sub, 0, len );

    sub->event = event;
    sub->state = SCA_SUBSCRIPTION_STATE_ACTIVE;
    sub->expires = time( NULL ) + expire_delta;
    sub->dialog.subscribe_cseq = cseq;
    sub->dialog.notify_cseq = 0;

    len = sizeof( sca_subscription );

    sub->subscriber.s = (char *)sub + len;
    SCA_STR_COPY( &sub->subscriber, subscriber );
    len += subscriber->len;

    sub->target_aor.s = (char *)sub + len;
    SCA_STR_COPY( &sub->target_aor, aor );
    len += aor->len;

    /*
     * dialog.id holds call-id + from-tag + to-tag; dialog.call_id,
     * dialog.from_tag, and dialog.to_tag point to offsets within
     * dialog.id.
     */
    sub->dialog.id.s = (char *)sub + len;
    SCA_STR_COPY( &sub->dialog.id, call_id );
    SCA_STR_APPEND( &sub->dialog.id, from_tag );
    SCA_STR_APPEND( &sub->dialog.id, to_tag );

    sub->dialog.call_id.s = sub->dialog.id.s;
    sub->dialog.call_id.len = call_id->len;

    sub->dialog.from_tag.s = sub->dialog.id.s + call_id->len;
    sub->dialog.from_tag.len = from_tag->len;

    sub->dialog.to_tag.s = sub->dialog.id.s + call_id->len + from_tag->len;
    sub->dialog.to_tag.len = to_tag->len;

    return( sub );
}

    int
sca_subscription_subscriber_cmp( str *subscriber, void *cmp_value )
{
    sca_subscription	*sub = (sca_subscription *)cmp_value;
    int			cmp;

    if (( cmp = subscriber->len - sub->subscriber.len ) != 0 ) {
	return( cmp );
    }
    
    return( memcmp( subscriber->s, sub->subscriber.s, subscriber->len ));
}

    void
sca_subscription_free( void *value )
{
    sca_subscription		*sub = (sca_subscription *)value;

    LM_DBG( "Freeing %s subscription from %.*s",
	    sca_event_name_from_type( sub->event ),
	    STR_FMT( &sub->subscriber ));

    shm_free( sub );
}

    void
sca_subscription_print( void *value )
{
    sca_subscription		*sub = (sca_subscription *)value;

    LM_INFO( "%.*s %s %.*s, dialog %.*s;%.*s;%.*s",
		STR_FMT( &sub->target_aor ),
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ),
		STR_FMT( &sub->dialog.call_id ),
		STR_FMT( &sub->dialog.from_tag ),
		STR_FMT( &sub->dialog.to_tag ));
}

    static sca_subscription *
sca_subscription_save( sca_mod *scam, sca_subscription *sub, int save_idx )
{
    sca_subscription		*new_sub = NULL;
    int				rc = -1;

    new_sub = sca_subscription_create( &sub->target_aor, sub->event,
			    	       &sub->subscriber,
				       sub->dialog.subscribe_cseq,
				       sub->expires,
				       &sub->dialog.call_id,
				       &sub->dialog.from_tag,
				       &sub->dialog.to_tag );
    if ( new_sub == NULL ) {
	return( NULL );
    }

    if ( save_idx >= 0 ) {
	rc = sca_hash_table_index_kv_insert( scam->subscriptions,
				    save_idx, new_sub,
				    sca_subscription_subscriber_cmp,
				    sca_subscription_print,
				    sca_subscription_free );
    } else {
	rc = sca_hash_table_kv_insert( scam->subscriptions,
				    &sub->dialog.id, new_sub,
				    sca_subscription_subscriber_cmp,
				    sca_subscription_print,
				    sca_subscription_free );
    }

    if ( rc < 0 ) {
	shm_free( new_sub );
	new_sub = NULL;
    }

    return( new_sub );
}

    static int
sca_subscription_update( sca_mod *scam, sca_subscription *saved_sub,
			 sca_subscription *update_sub, int sub_idx )
{
    int			rc = -1;

    if ( sub_idx < 0 || sub_idx > scam->subscriptions->size ) {
	LM_ERR( "Invalid hash table index %d", sub_idx );
	goto done;
    }

    /* BEGIN LOCK */
    sca_hash_table_lock_index( scam->subscriptions, sub_idx );

    /* sanity checks first */
    if ( saved_sub->event != update_sub->event ) {
	LM_ERR( "Event mismatch for in-dialog SUBSCRIBE from %.*s: "
		"%s != %s", STR_FMT( &update_sub->subscriber ),
		sca_event_name_from_type( saved_sub->event ),
		sca_event_name_from_type( update_sub->event ));
	goto done;
    }
    if ( !STR_EQ( saved_sub->subscriber, update_sub->subscriber )) {
	LM_ERR( "Contact mismatch for in-dialog SUBSCRIBE from %.*s: "
		"%.*s != %.*s", STR_FMT( &update_sub->subscriber ),
		STR_FMT( &update_sub->subscriber ),
		STR_FMT( &saved_sub->subscriber ));
	goto done;
    }
    if ( !STR_EQ( saved_sub->target_aor, update_sub->target_aor )) {
	LM_ERR( "AoR mismatch for in-dialog SUBSCRIBE from %.*s: "
		"%.*s != %.*s", STR_FMT( &update_sub->subscriber ),
		STR_FMT( &update_sub->target_aor ),
		STR_FMT( &saved_sub->target_aor ));
	goto done;
    }

    saved_sub->state = update_sub->state;
    saved_sub->dialog.subscribe_cseq = update_sub->dialog.subscribe_cseq;
    saved_sub->dialog.notify_cseq += 1;
    saved_sub->expires = time( NULL ) + update_sub->expires;

    rc = 1;

done:
    sca_hash_table_unlock_index( scam->subscriptions, sub_idx );
    /* END LOCK */

    return( rc );
}

    static int
sca_subscription_delete( sca_mod *scam, sca_subscription *delete_sub,
	int sub_idx )
{
    return( sca_hash_table_index_kv_delete( scam->subscriptions, sub_idx,
					    &delete_sub->subscriber ));
}

    static int
sca_subscription_copy_subscription_key( sca_subscription *sub, str *key_out )
{
    char			*event_name;
    int				len;

    assert( sub != NULL );
    assert( key_out != NULL );

    len = sub->target_aor.len;
    event_name = sca_event_name_from_type( sub->event );
    len += strlen( event_name );

    key_out->s = (char *)pkg_malloc( len );
    if ( key_out->s == NULL ) {
	LM_ERR( "Failed to pkg_malloc space for subscription key" );
	return( -1 );
    }
    
    SCA_STR_COPY( key_out, &sub->target_aor );
    SCA_STR_APPEND_CSTR( key_out, event_name );

    return( key_out->len );
}

    int
sca_subscription_from_request( sca_mod *scam, sip_msg_t *msg, int event_type,
	sca_subscription *req_sub )
{
    struct to_body		tmp_to, *to, *from;
    contact_body_t		*contact_body;
    str				*contact_uri;
    str				to_tag = STR_NULL;
    unsigned int		expires = 0, max_expires;
    unsigned int		cseq;

    assert( req_sub != NULL );

    /* parse required info first */
    if ( !SCA_HEADER_EMPTY( msg->expires )) {
	if ( parse_expires( msg->expires ) < 0 ) {
	    LM_ERR( "Failed to parse Expires header" );
	    goto error;
	}

	expires = ((exp_body_t *)msg->expires->parsed)->val;
    }

    switch ( event_type ) {
    case SCA_EVENT_TYPE_CALL_INFO:
    default:
	max_expires = scam->cfg->call_info_max_expires;
	break;

    case SCA_EVENT_TYPE_LINE_SEIZE:
	max_expires = scam->cfg->call_info_max_expires;
	break;
    }

    if ( expires && expires > max_expires ) {
	expires = max_expires;
    }

    if ( SCA_HEADER_EMPTY( msg->to )) {
	LM_ERR( "Empty To header" );
	goto error;
    }
    if ( SCA_HEADER_EMPTY( msg->callid )) {
	LM_ERR( "Empty Call-ID header" );
	goto error;
    }

    /* XXX move to static inline function */
    if ( SCA_HEADER_EMPTY( msg->cseq )) {
	LM_ERR( "Empty CSeq header" );
	goto error;
    }
    if ( str2int( &(get_cseq( msg )->number), &cseq ) != 0 ) {
	LM_ERR( "Bad Cseq header: %.*s",
		msg->cseq->body.len, msg->cseq->body.s );
	goto error;
    }

    /* XXX move Contact parsing to static inline function? */
    if ( SCA_HEADER_EMPTY( msg->contact )) {
	LM_ERR( "Empty Contact header" );
	goto error;
    }
    if ( parse_contact( msg->contact ) < 0 ) {
	LM_ERR( "Failed to parse Contact header: %.*s",
		msg->contact->body.len, msg->contact->body.s );
	goto error;
    }
    if (( contact_body = (contact_body_t *)msg->contact->parsed ) == NULL ) {
	LM_ERR( "Invalid Contact header: %.*s",
		msg->contact->body.len, msg->contact->body.s );
	goto error;
    }
    if ( contact_body->star ) {
	LM_ERR( "Invalid Contact header: contact header must be an AoR" );
	goto error;
    }
    if ( contact_body->contacts == NULL ) {
	LM_ERR( "Invalid Contact header: parser found no contacts" );
	goto error;
    }
    if ( contact_body->contacts->next != NULL ) {
	LM_ERR( "Invalid Contact header: contact may only contain one AoR" );
	goto error;
    }
    contact_uri = &contact_body->contacts->uri;

    if ( SCA_HEADER_EMPTY( msg->from )) {
	LM_ERR( "Empty From header" );
	goto error;
    }
    if ( parse_from_header( msg ) < 0 ) {
	LM_ERR( "Bad From header" );
	goto error;
    }
    from = (struct to_body *)msg->from->parsed;
    if ( SCA_STR_EMPTY( &from->tag_value )) {
	LM_ERR( "No from-tag in From header" );
	goto error;
    }

    if (( to = (struct to_body *)msg->to->parsed ) == NULL ) {
	parse_to( msg->to->body.s,
		  msg->to->body.s + msg->to->body.len + 1, /* end of buffer */
		  &tmp_to );

	if ( tmp_to.error != PARSE_OK ) {
	    LM_ERR( "Bad To header" );
	    goto error;
	}
	to = &tmp_to;
    }

    to_tag = to->tag_value;
    if ( SCA_STR_EMPTY( &to_tag )) {
	/*
	 * XXX need hook to detect when we have a subscription and the
	 * subscriber sends an out-of-dialog SUBSCRIBE, which indicates the
	 * old subscription should be dumped & appropriate NOTIFYs sent.
	 */
	if ( scam->sl_api->get_reply_totag( msg, &to_tag ) < 0 ) {
	    LM_ERR( "Failed to generate to-tag for reply to SUBSCRIBE %.*s", 
			STR_FMT( &REQ_LINE( msg ).uri ));
	    goto error;
	}
    }

    req_sub->subscriber = contact_body->contacts->uri;
    req_sub->target_aor = REQ_LINE( msg ).uri;
    req_sub->event = event_type;
    req_sub->expires = expires;
    if ( req_sub->expires > 0 ) {
	req_sub->state = SCA_SUBSCRIPTION_STATE_ACTIVE;
	expires += time( NULL );
    } else {
	/* subscriber requested subscription termination, see rfc3265 3.2.4 */
	req_sub->state = SCA_SUBSCRIPTION_STATE_TERMINATED;
    }

    req_sub->dialog.id.s = NULL;
    req_sub->dialog.id.len = 0;
    req_sub->dialog.call_id = msg->callid->body;
    req_sub->dialog.from_tag = from->tag_value;
    req_sub->dialog.to_tag = to_tag;

    //free_to_params( &tmp_to );
    return( 1 );

error:
    //free_to_params( &tmp_to );
    return( -1 );
}

    int
sca_handle_subscribe( sip_msg_t *msg, char *p1, char *p2 )
{
    sca_subscription	req_sub;
    sca_subscription	*sub = NULL;
    sca_call_info	call_info;
    hdr_field_t		*call_info_hdr;
    str			sub_key = STR_NULL;
    str			*to_tag = NULL;
    char		*status_text;
    int			event_type;
    int			status;
    int			idx, app_idx = SCA_CALL_INFO_APPEARANCE_INDEX_ALL;

    if ( parse_headers( msg, HDR_EOH_F, 0 ) < 0 ) {
	LM_ERR( "header parsing failed: bad request" );
	SCA_REPLY_ERROR( sca, 400, "Bad Request", msg );
	return( -1 );
    }

    if ( !STR_EQ( REQ_LINE( msg ).method, SCA_METHOD_SUBSCRIBE )) {
	LM_ERR( "bad request method %.*s", STR_FMT( &REQ_LINE( msg ).method ));
	SCA_REPLY_ERROR( sca, 500, "Internal server error - config", msg );
	return( -1 );
    }

    if ( SCA_HEADER_EMPTY( msg->event )) {
	SCA_REPLY_ERROR( sca, 400, "Missing Event", msg );
	return( -1 );
    }

    event_type = sca_event_from_str( &msg->event->body );
    if ( event_type == SCA_EVENT_TYPE_UNKNOWN ) {
	SCA_REPLY_ERROR( sca, 400, "Bad Event", msg );
	return( -1 );
    }

    if ( sca_subscription_from_request( sca, msg, event_type, &req_sub ) < 0 ) {
	SCA_REPLY_ERROR( sca, 400, "Bad Shared Call Appearance Request", msg );
	return( -1 );
    }
    if ( sca_subscription_copy_subscription_key( &req_sub, &sub_key ) < 0 ) {
	SCA_REPLY_ERROR( sca, 500,
			"Internal Server Error - copy dialog id", msg );
	return( -1 );
    }
    sca_subscription_print( &req_sub );

    /* check to see if the message has a to-tag */
    to_tag = &(get_to( msg )->tag_value);

    /* XXX should lock starting here and use unsafe methods below? */

    /* ensure we only calculate the hash table index once */
    idx = sca_hash_table_index_for_key( sca->subscriptions,
					&sub_key );
    sub = sca_hash_table_index_kv_find( sca->subscriptions, idx,
					&req_sub.subscriber ); 

    if ( sub != NULL ) {
	/* this will remove the subscription if expires == 0 */
	if ( sca_subscription_update( sca, sub, &req_sub, idx ) < 0 ) {
	    SCA_REPLY_ERROR( sca, 500,
		    "Internal Server Error - update subscription", msg );
	    goto error;
	}

	if ( req_sub.event == SCA_EVENT_TYPE_LINE_SEIZE &&
			req_sub.expires == 0 ) {
	    /* release the seized appearance */
	    call_info_hdr = sca_call_info_header_find( msg->headers );
	    if ( call_info_hdr ) {
		if ( sca_call_info_body_parse( &call_info_hdr->body,
			&call_info ) < 0 ) {
		    SCA_REPLY_ERROR( sca, 400, "Bad Request - "
				    "Invalid Call-Info header", msg );
		    goto error;
		}

		if ( sca_appearance_release_index( sca, &req_sub.target_aor,
			call_info.index ) != SCA_APPEARANCE_OK ) {
		    SCA_REPLY_ERROR( sca, 500, "Internal Server Error - "
				    "release seized line", msg );
		    goto error;
		}
	    }
	}
    } else {
	/* in-dialog request, but we didn't find it. */
	if ( !SCA_STR_EMPTY( to_tag )) {
	    SCA_REPLY_ERROR( sca, 481,
		    "Call Leg/Transaction Does Not Exist", msg );
	    goto error;
	}

	if ( req_sub.expires > 0 ) {
	    if ( req_sub.event == SCA_EVENT_TYPE_LINE_SEIZE ) {
		app_idx = sca_appearance_seize_next_available_index( sca,
				&req_sub.target_aor, &req_sub.subscriber );
		if ( app_idx < 0 ) {
		    SCA_REPLY_ERROR( sca, 500, "Internal Server Error - "
					"seize appearance index", msg );
		    goto error;
		}
	    }

	    sub = sca_subscription_save( sca, &req_sub, idx );
	    if ( sub == NULL ) {
		SCA_REPLY_ERROR( sca, 500,
			"Internal Server Error - save subscription", msg );
		goto error;
	    }
	} else {
	    /*
	     * we got an in-dialog SUBSCRIBE with an "Expires: 0" header,
	     * but the dialog wasn't in our table. just reply with the
	     * subscription info we got, without saving or creating anything.
	     */
	    sub = &req_sub;
	}
    }

    /* pkg_malloc'd in sca_subscription_copy_subscription_key() */
    pkg_free( sub_key.s );

    status = sca_ok_status_for_event( event_type );
    status_text = sca_ok_text_for_event( event_type );
    if ( sca_reply( sca, status, status_text, event_type,
		    req_sub.expires, msg ) < 0 ) {
	SCA_REPLY_ERROR( sca, 500, "Internal server error", msg );
	goto error;
    }

    /* XXX this should be locked; could use a filled-in req_sub */
    if ( sca_notify_subscriber( sca, sub, app_idx ) < 0 ) {
	LM_ERR( "SCA %s SUBSCRIBE+NOTIFY for %.*s failed",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	/*
	 * XXX - what does subscriber do in this case? drop subscription?
	 * sub is already saved/updated in hash table. let it rot?
	 */
	goto error;
    }

    if ( req_sub.event == SCA_EVENT_TYPE_LINE_SEIZE ) {
	if ( sca_notify_call_info_subscribers( sca, &req_sub.target_aor) < 0 ) {
	    LM_ERR( "SCA %s NOTIFY to all %.*s subscribers failed",
		    sca_event_name_from_type( req_sub.event ),
		    STR_FMT( &req_sub.target_aor ));
	    goto error;
	}
    }

    return( 1 );

error:
    return( -1 );
}
