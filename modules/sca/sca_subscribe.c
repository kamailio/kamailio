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
#include <errno.h>

#include "sca.h"
#include "sca_appearance.h"
#include "sca_call_info.h"
#include "sca_event.h"
#include "sca_notify.h"
#include "sca_reply.h"
#include "sca_subscribe.h"
#include "sca_util.h"

#include "../../modules/tm/tm_load.h"

extern int	errno;


static int sca_subscription_copy_subscription_key( sca_subscription *, str * );
int	   sca_subscription_save_unsafe( sca_mod *, sca_subscription *,
					 int, int );
void	   sca_subscription_print( void * );

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
    int			state;
    int			i;

    assert( scam != NULL );
    assert( scam->subscriptions != NULL );

    LM_INFO( "SCA: purging expired subscriptions" );

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

		if ( sca_notify_subscriber( scam, sub, sub->index ) < 0 ) {
		    LM_ERR( "Failed to send subscription expired "
			    "NOTIFY %s subscriber %.*s",
			    sca_event_name_from_type( sub->event ),
			    STR_FMT( &sub->subscriber ));

		    /* remove from subscribers list anyway */
		}
		if ( sub->event == SCA_EVENT_TYPE_LINE_SEIZE ) {
		    /* only notify if the line is just seized */
		    state = sca_appearance_state_for_index( sca,
				    &sub->target_aor, sub->index );
		    if ( state == SCA_APPEARANCE_STATE_SEIZED ) {
			if ( sca_appearance_release_index( sca,
				    &sub->target_aor, sub->index ) < 0 ) {
			    LM_ERR( "Failed to release seized %.*s "
				    "appearance-index %d",
				    STR_FMT( &sub->target_aor ), sub->index );
			}

			if ( sca_notify_call_info_subscribers( sca,
					    &sub->target_aor ) < 0 ) {
			    LM_ERR( "SCA %s NOTIFY to all %.*s "
				    "subscribers failed",
				    sca_event_name_from_type( sub->event ),
				    STR_FMT( &sub->target_aor ));
			    /*
			     * fall through anyway. the state should propagate
			     * to subscribers when they renew call-info.
			     */	
			}
		    }
		}
	    }

	    /*
	     * XXX should be in a separate subscription deletion routine.
	     * will need to detect whether subscriber has active appearances,
	     * send notifies to others in group if necessary.
	     */

	    LM_INFO( "%s subscription from %.*s expired, deleting",
			sca_event_name_from_type( sub->event ),
			STR_FMT( &sub->subscriber ));

	    sca_hash_table_slot_unlink_entry_unsafe( &ht->slots[ i ], ent );
	    sca_hash_entry_free( ent );
	}

	sca_hash_table_unlock_index( ht, i );
    }
}

    int
sca_subscription_from_db_row_values( db_val_t *values, sca_subscription *sub )
{
    assert( values != NULL );
    assert( sub != NULL );

    /* XXX condense to loop with preprocessor macros when there's time */
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_SUBSCRIBER_COL,
						values, &sub->subscriber );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_AOR_COL,
						values, &sub->target_aor );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_EVENT_COL, values,
						&sub->event );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_EXPIRES_COL, values,
						&sub->expires );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_STATE_COL, values,
						&sub->state );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_APP_IDX_COL, values,
						&sub->index );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_CALL_ID_COL, values,
						&sub->dialog.call_id );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_FROM_TAG_COL, values,
						&sub->dialog.from_tag );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_TO_TAG_COL, values,
						&sub->dialog.to_tag );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_RECORD_ROUTE_COL,
						values, &sub->rr );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_NOTIFY_CSEQ_COL,
					    values, &sub->dialog.notify_cseq );
    sca_db_subscriptions_get_value_for_column( SCA_DB_SUBS_SUBSCRIBE_CSEQ_COL,
					values, &sub->dialog.subscribe_cseq );

    return( 0 );
}

    int
sca_subscription_to_db_row_values( sca_subscription *sub, db_val_t *values )
{
    int		notify_cseq, subscribe_cseq;

    assert( sub != NULL );
    assert( values != NULL );

    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_SUBSCRIBER_COL,
						values, &sub->subscriber );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_AOR_COL,
						values, &sub->target_aor );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_EVENT_COL, values,
						&sub->event );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_EXPIRES_COL, values,
						&sub->expires );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_STATE_COL, values,
						&sub->state );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_APP_IDX_COL, values,
						&sub->index );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_CALL_ID_COL, values,
						&sub->dialog.call_id );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_FROM_TAG_COL, values,
						&sub->dialog.from_tag );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_TO_TAG_COL, values,
						&sub->dialog.to_tag );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_RECORD_ROUTE_COL,
						values, &sub->rr );

    notify_cseq = sub->dialog.notify_cseq + 1;
    subscribe_cseq = sub->dialog.subscribe_cseq + 1;
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_NOTIFY_CSEQ_COL,
						values, &notify_cseq );
    sca_db_subscriptions_set_value_for_column( SCA_DB_SUBS_SUBSCRIBE_CSEQ_COL,
						values, &subscribe_cseq );

    return( 0 );
}

    int
sca_subscriptions_restore_from_db( sca_mod *scam )
{
    db1_con_t		*db_con;
    db_key_t		result_columns[ SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS ];
    db1_res_t		*result = NULL;
    db_row_t		*rows = NULL;
    db_val_t		*row_values = NULL;
    sca_subscription	sub;
    str			sub_key = STR_NULL;
    str			**column_names;
    int			num_rows;
    int			i;
    int			idx;
    int			rc = -1;
    time_t		now = time( NULL );

    db_con = scam->db_api->init( scam->cfg->db_url );
    if ( db_con == NULL ) {
	LM_ERR( "sca_subscriptions_restore_from_db: failed to connect "
		"to DB %.*s", STR_FMT( scam->cfg->db_url ));
	return( -1 );
    }

    scam->db_api->use_table( db_con, scam->cfg->subs_table );

    column_names = sca_db_subscriptions_columns();
    if ( column_names == NULL ) {
	LM_ERR( "sca_subscriptions_restore_from_db: failed to get "
		"column names for SCA subscriptions table" );
	goto done;
    }

    for ( i = 0; i < SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS; i++ ) {
	result_columns[ i ] = column_names[ i ];
    }

    rc = db_fetch_query( scam->db_api, SCA_DB_DEFAULT_FETCH_ROW_COUNT,
			db_con, NULL, NULL, NULL, result_columns,
			0, SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS,
			0, &result );
    switch ( rc ) {
    default:
    case -1:
	LM_ERR( "sca_subscriptions_restore_from_db: query failed" );
	goto done;

    case 0:
	LM_WARN( "sca_subscriptions_restore_from_db: DB module does "
		 "not support fetch, query returning all values..." );
	/* fall through */

    case 1:
	break;
    }

    do {
	rows = RES_ROWS( result );
	num_rows = RES_ROW_N( result );

	for ( i = 0; i < num_rows; i++ ) {
	    memset( &sub, 0, sizeof( sca_subscription ));

	    row_values = ROW_VALUES( rows + i );

	    sub.expires = row_values[ SCA_DB_SUBS_EXPIRES_COL ].val.time_val;
	    if ( sub.expires < now ) {
		continue;
	    }

	    if ( sca_subscription_from_db_row_values( row_values, &sub ) < 0 ) {
		LM_ERR( "sca_subscriptions_restore_from_db: skipping bad result "
			"at index %d", i );
		continue;
	    }

	    if ( sca_subscription_copy_subscription_key( &sub, &sub_key ) < 0 ) {
		LM_ERR( "sca_subscriptions_restore_from_db: failed to copy "
			"subscription key %.*s%s", STR_FMT( &sub.subscriber ),
			sca_event_name_from_type( sub.event ));
		continue;
	    }

	    idx = sca_hash_table_index_for_key( sca->subscriptions, &sub_key );
	    pkg_free( sub_key.s );

	    sca_hash_table_lock_index( sca->subscriptions, idx );

	    if ( sca_subscription_save_unsafe( scam, &sub, idx,
			    SCA_SUBSCRIPTION_CREATE_OPT_RAW_EXPIRES ) < 0 ) {
		LM_ERR( "sca_subscriptions_restore_from_db: failed to restore "
			"%s subscription from %.*s to the hash table",
			sca_event_name_from_type( sub.event ),
			STR_FMT( &sub.subscriber ));

		/* fall through to unlock index */
	    }

	    sca_hash_table_unlock_index( sca->subscriptions, idx );
	}
    } while ( db_fetch_next( scam->db_api, SCA_DB_DEFAULT_FETCH_ROW_COUNT,
		db_con, &result ) == 1 && num_rows > 0 );

    scam->db_api->free_result( db_con, result );

    /* clear all records from table, let timer process repopulate it */
    if ( scam->db_api->delete( db_con, NULL, NULL, NULL, 0 ) < 0 ) {
	LM_ERR( "sca_subscriptions_restore_from_db: failed to delete "
		"records from table after restoring" );
	goto done;
    }

    rc = 0;

done:
    scam->db_api->close( db_con );
    db_con = NULL;

    return( rc );
}

    static int
sca_subscription_db_update_subscriber( db1_con_t *db_con,
	sca_subscription *sub )
{
    db_key_t		query_columns[ 1 ];
    db_val_t		query_values[ 1 ];
    int			query_column_idx = 0;
    db_key_t		update_columns[ 6 ];
    db_val_t		update_values[ 6 ];
    int			update_column_idx = 0;

    assert( db_con != NULL );
    assert( sub != NULL );

    /* bind the lookup key and value */
    SCA_DB_BIND_STR_VALUE( sub->subscriber, &SCA_DB_SUBSCRIBER_COL_NAME,
			    query_columns, query_values, query_column_idx );

    /* bind updated keys and values */
    SCA_DB_BIND_INT_VALUE( sub->expires, &SCA_DB_EXPIRES_COL_NAME,
			    update_columns, update_values, update_column_idx );
    SCA_DB_BIND_STR_VALUE( sub->dialog.call_id, &SCA_DB_CALL_ID_COL_NAME,
			    update_columns, update_values, update_column_idx );
    SCA_DB_BIND_STR_VALUE( sub->dialog.from_tag, &SCA_DB_FROM_TAG_COL_NAME,
			    update_columns, update_values, update_column_idx );
    SCA_DB_BIND_STR_VALUE( sub->dialog.to_tag, &SCA_DB_TO_TAG_COL_NAME,
			    update_columns, update_values, update_column_idx );
    SCA_DB_BIND_INT_VALUE( sub->dialog.notify_cseq + 1,
			    &SCA_DB_NOTIFY_CSEQ_COL_NAME,
			    update_columns, update_values, update_column_idx );
    SCA_DB_BIND_INT_VALUE( sub->dialog.subscribe_cseq + 1,
			    &SCA_DB_SUBSCRIBE_CSEQ_COL_NAME,
			    update_columns, update_values, update_column_idx );
			    

    if ( sca->db_api->update( db_con, query_columns,NULL, query_values,
			      update_columns, update_values,
			      query_column_idx, update_column_idx ) < 0 ) {
	LM_ERR( "sca_subscription_db_update_subscriber: failed to update "
		"%s subscriber %.*s in DB",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	return( -1 );
    }

    /* no further DB updates until subscription is changed */  
    sub->db_cmd_flag = SCA_DB_FLAG_NONE;

    return( 0 );
}

    static int
sca_subscription_db_insert_subscriber( db1_con_t *db_con,
	sca_subscription *sub )
{
    db_key_t		insert_columns[ SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS ];
    db_val_t		insert_values[ SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS ];
    str			**column_names;
    int			i;

    assert( db_con != NULL );
    assert( sub != NULL );

    column_names = sca_db_subscriptions_columns();
    if ( column_names == NULL ) {
	LM_ERR( "sca_subscriptions_restore_from_db: failed to get "
		"column names for SCA subscriptions table" );
	return( -1 );
    }

    for ( i = 0; i < SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS; i++ ) {
	insert_columns[ i ] = column_names[ i ];
    }

    /* XXX array boundary checking */
    if ( sca_subscription_to_db_row_values( sub, insert_values ) != 0 ) {
	LM_ERR( "sca_subscription_db_insert_subscriber: failed to set "
		"DB row values for INSERT of %s subscriber %.*s",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	return( -1 );
    }

    if ( sca->db_api->insert( db_con, insert_columns, insert_values,
			      SCA_DB_SUBSCRIPTIONS_NUM_COLUMNS ) < 0 ) {
	LM_ERR( "sca_subscription_db_insert_subscriber: failed to insert "
		"%s subscriber %.*s in DB subscription table",
		sca_event_name_from_type( sub->event ),
		STR_FMT( &sub->subscriber ));
	return( -1 );
    }

    /* no further DB actions needed until subscription is changed */
    sub->db_cmd_flag = SCA_DB_FLAG_NONE;

    return( 0 );
}

    int
sca_subscription_db_delete_expired( db1_con_t *db_con )
{
    db_key_t		delete_columns[ 1 ];
    db_val_t		delete_values[ 1 ];
    db_op_t		delete_ops[ 1 ];
    time_t		now = time( NULL );
    int			kv_count = 0;

    delete_columns[ 0 ] = (str *)&SCA_DB_EXPIRES_COL_NAME;
    delete_ops[ 0 ] = OP_LT;

    SCA_DB_BIND_INT_VALUE( now, &SCA_DB_EXPIRES_COL_NAME,
			    delete_columns, delete_values, kv_count );

    if ( sca->db_api->delete( db_con, delete_columns, delete_ops,
				delete_values, kv_count ) < 0 ) {
	LM_ERR( "sca_subscription_db_delete_expired: failed to delete "
		"subscriptions expired before %ld", (long int)now );
	return( -1 );
    }

    return( 0 );
}

    int
sca_subscription_db_update( void )
{
    db1_con_t		*db_con = NULL;
    sca_hash_table	*ht;
    sca_hash_entry	*entry;
    sca_subscription	*sub;
    int			i;
    int			rc = -1;
    time_t		now = time( NULL );

    db_con = sca_db_get_connection();
    if ( db_con == NULL ){
	LM_ERR( "sca_subscription_db_update: failed to connect to DB %.*s",
		STR_FMT( sca->cfg->db_url ));
	goto done;
    }
    if ( sca->db_api->use_table( db_con, sca->cfg->subs_table ) < 0 ) {
	LM_ERR( "sca_subscription_db_update: failed to in-use table "
		"for DB %.*s", STR_FMT( sca->cfg->db_url ));
	goto done;
    }

    ht = sca->subscriptions;
    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( entry = ht->slots[ i ].entries; entry != NULL;
			entry = entry->next ) {
	    sub = (sca_subscription *)entry->value;

	    if ( sub == NULL || sub->expires < now ) {
		continue;
	    }

	    /* we only do call-info subscriptions for now */
	    if ( sub->event != SCA_EVENT_TYPE_CALL_INFO ) {
		continue;
	    }

	    if ( SCA_SUBSCRIPTION_IS_TERMINATED( sub )) {
		continue;
	    }

	    if ( sub->db_cmd_flag == SCA_DB_FLAG_INSERT ) {
		if ( sca_subscription_db_insert_subscriber( db_con, sub ) < 0) {
		    LM_ERR( "sca_subscription_db_update: failed to insert "
			    "%s subscriber %.*s into subscription DB",
			    sca_event_name_from_type( sub->event ),
			    STR_FMT( &sub->subscriber ));
		}
	    } else if ( sub->db_cmd_flag == SCA_DB_FLAG_UPDATE ) {
		if ( sca_subscription_db_update_subscriber( db_con, sub ) < 0) {
		    LM_ERR( "sca_subscription_db_update: failed to insert "
			    "%s subscriber %.*s into subscription DB",
			    sca_event_name_from_type( sub->event ),
			    STR_FMT( &sub->subscriber ));
		}
	    }
	}

	sca_hash_table_unlock_index( ht, i );
    }

    rc = sca_subscription_db_delete_expired( db_con );

done:
    return( rc );
}

    void
sca_subscription_db_update_timer( unsigned int ticks, void *param )
{
    if ( sca_subscription_db_update() != 0 ) {
	LM_ERR( "sca_subscription_db_update_timer: failed to update "
		"subscriptions in DB %.*s", STR_FMT( sca->cfg->db_url ));
    }
}

    int
sca_subscription_aor_has_subscribers( int event, str *aor )
{
    sca_hash_slot	*slot;
    sca_hash_entry	*e;
    sca_subscription	*sub;
    str			sub_key = STR_NULL;
    char		*event_name;
    int			len;
    int			subscribers = 0;
    int			slot_idx = -1;

    event_name = sca_event_name_from_type( event );
    len = aor->len + strlen( event_name );
    sub_key.s = (char *)pkg_malloc( len );
    if ( sub_key.s == NULL ) {
	LM_ERR( "Failed to pkg_malloc key to look up %s "
		"subscription for %.*s", event_name, STR_FMT( aor ));
	return( -1 );
    }
    SCA_STR_COPY( &sub_key, aor );
    SCA_STR_APPEND_CSTR( &sub_key, event_name );

    slot_idx = sca_hash_table_index_for_key( sca->subscriptions, &sub_key );
    pkg_free( sub_key.s );
    sub_key.len = 0;

    slot = sca_hash_table_slot_for_index( sca->subscriptions, slot_idx );
    sca_hash_table_lock_index( sca->subscriptions, slot_idx );

    for ( e = slot->entries; e != NULL; e = e->next ) {
	sub = (sca_subscription *)e->value;

	if ( SCA_STR_EQ( &sub->target_aor, aor )) {
	    subscribers = 1;
	    break;
	}
    }

    sca_hash_table_unlock_index( sca->subscriptions, slot_idx );

    return( subscribers );
}

    sca_subscription *
sca_subscription_create( str *aor, int event, str *subscriber,
	unsigned int notify_cseq, unsigned int subscribe_cseq, int expire_delta,
	str *call_id, str *from_tag, str *to_tag, str *rr, int opts )
{
    sca_subscription		*sub = NULL;
    int				len = 0;

    len += sizeof( sca_subscription );
    len += sizeof( char ) * ( aor->len + subscriber->len );
    if ( !SCA_STR_EMPTY( rr )) {
	len += sizeof( char ) * rr->len;
    }

    sub = (sca_subscription *)shm_malloc( len );
    if ( sub == NULL ) {
	LM_ERR( "Failed to create %s subscription for %.*s: out of memory",
		sca_event_name_from_type( event ), STR_FMT( subscriber ));
	goto error;
    }
    memset( sub, 0, len );

    sub->event = event;
    sub->state = SCA_SUBSCRIPTION_STATE_ACTIVE;
    sub->index = SCA_CALL_INFO_APPEARANCE_INDEX_ANY;
    if ( opts & SCA_SUBSCRIPTION_CREATE_OPT_RAW_EXPIRES ) {
	sub->expires = expire_delta;
    } else {
	sub->expires = time( NULL ) + expire_delta;
    }
    sub->dialog.notify_cseq = notify_cseq;
    sub->dialog.subscribe_cseq = subscribe_cseq;
    sub->db_cmd_flag = SCA_DB_FLAG_INSERT;

    len = sizeof( sca_subscription );

    sub->subscriber.s = (char *)sub + len;
    SCA_STR_COPY( &sub->subscriber, subscriber );
    len += subscriber->len;

    sub->target_aor.s = (char *)sub + len;
    SCA_STR_COPY( &sub->target_aor, aor );
    len += aor->len;

    if ( !SCA_STR_EMPTY( rr )) {
	sub->rr.s = (char *)sub + len;
	SCA_STR_COPY( &sub->rr, rr );
	len += rr->len;
    }

    /*
     * dialog.id holds call-id + from-tag + to-tag; dialog.call_id,
     * dialog.from_tag, and dialog.to_tag point to offsets within
     * dialog.id.
     *
     * we shm_malloc this separately in case we need to update in-memory
     * dialog saved for this subscriber. this is likely to happen if the
     * subscriber goes off-line for some reason.
     */
    len = sizeof( char ) * ( call_id->len + from_tag->len + to_tag->len );
    sub->dialog.id.s = (char *)shm_malloc( len );
    if ( sub->dialog.id.s == NULL ) {
	LM_ERR( "Failed to shm_malloc space for %.*s %s subscription dialog: "
		"out of memory", STR_FMT( &sub->subscriber ),
		sca_event_name_from_type( sub->event ));
	goto error;
    }
    sub->dialog.id.len = len;

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

error:
    if ( sub != NULL ) {
	if ( sub->dialog.id.s != NULL ) {
	    shm_free( sub->dialog.id.s );
	}
	shm_free( sub );
    }

    return( NULL );
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

    if ( sub == NULL ) {
	return;
    }

    LM_DBG( "Freeing %s subscription from %.*s",
	    sca_event_name_from_type( sub->event ),
	    STR_FMT( &sub->subscriber ));

    if ( !SCA_STR_EMPTY( &sub->dialog.id )) {
	shm_free( sub->dialog.id.s );
    }

    shm_free( sub );
}

    void
sca_subscription_print( void *value )
{
    sca_subscription		*sub = (sca_subscription *)value;

    LM_DBG( "%.*s %s (%d) %.*s, expires: %ld, index: %d, "
	     "dialog %.*s;%.*s;%.*s, record_route: %.*s, "
	     "notify_cseq: %d, subscribe_cseq: %d",
		STR_FMT( &sub->target_aor ),
		sca_event_name_from_type( sub->event ),
		sub->event,
		STR_FMT( &sub->subscriber ),
		(long int)sub->expires, sub->index,
		STR_FMT( &sub->dialog.call_id ),
		STR_FMT( &sub->dialog.from_tag ),
		STR_FMT( &sub->dialog.to_tag ),
		SCA_STR_EMPTY( &sub->rr ) ? 4 : sub->rr.len,
		SCA_STR_EMPTY( &sub->rr ) ? "null" : sub->rr.s,
		sub->dialog.notify_cseq,
		sub->dialog.subscribe_cseq );
}

    int
sca_subscription_save_unsafe( sca_mod *scam, sca_subscription *sub,
	int save_idx, int opts )
{
    sca_subscription		*new_sub = NULL;
    sca_hash_slot		*slot;
    int				rc = -1;

    assert( save_idx >= 0 );

    new_sub = sca_subscription_create( &sub->target_aor, sub->event,
			    	       &sub->subscriber,
				       sub->dialog.notify_cseq,
				       sub->dialog.subscribe_cseq,
				       sub->expires,
				       &sub->dialog.call_id,
				       &sub->dialog.from_tag,
				       &sub->dialog.to_tag,
				       &sub->rr, opts );
    if ( new_sub == NULL ) {
	return( -1 );
    }
    if ( sub->index != SCA_CALL_INFO_APPEARANCE_INDEX_ANY ) {
	new_sub->index = sub->index;
    }

    if ( sca_appearance_register( scam, &sub->target_aor ) < 0 ) {
	LM_ERR( "sca_subscription_save: sca_appearance_register failed, "
		"still saving subscription from %.*s",
		STR_FMT( &sub->subscriber ));
    }

    slot = sca_hash_table_slot_for_index( scam->subscriptions, save_idx );
    rc = sca_hash_table_slot_kv_insert_unsafe( slot, new_sub,
				sca_subscription_subscriber_cmp,
				sca_subscription_print,
				sca_subscription_free );
    if ( rc < 0 ) {
	shm_free( new_sub );
	new_sub = NULL;
    }

    return( rc );
}

    static int
sca_subscription_update_unsafe( sca_mod *scam, sca_subscription *saved_sub,
			 sca_subscription *update_sub, int sub_idx )
{
    int			rc = -1;
    int			len;
    char		*dlg_id_tmp;

    if ( sub_idx < 0 || sub_idx > scam->subscriptions->size ) {
	LM_ERR( "Invalid hash table index %d", sub_idx );
	goto done;
    }

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

    if ( !STR_EQ( saved_sub->dialog.call_id, update_sub->dialog.call_id ) ||
	    !STR_EQ( saved_sub->dialog.from_tag,
			update_sub->dialog.from_tag ) ||
	    !STR_EQ( saved_sub->dialog.to_tag, update_sub->dialog.to_tag )) {
	/*
	 * mismatched dialog. we assume a subscriber can hold only one
	 * subscription per event at any given time, so we replace the old
	 * one with the new.
	 */
	assert( !SCA_STR_EMPTY( &saved_sub->dialog.id ));

	/* this is allocated separately from the rest of the subscription */
	
	len = sizeof( char * ) * ( update_sub->dialog.call_id.len +
				    update_sub->dialog.from_tag.len +
				    update_sub->dialog.to_tag.len );

	dlg_id_tmp = (char *)shm_malloc( len );
	if ( dlg_id_tmp == NULL ) {
	    LM_ERR( "Failed to replace %.*s %s subscription dialog: "
		    "shm_malloc failed", STR_FMT( &update_sub->subscriber ),
		    sca_event_name_from_type( update_sub->event ));
	    /* XXX should remove subscription entirely here? */
	} else {
	    shm_free( saved_sub->dialog.id.s );
	    saved_sub->dialog.id.s = dlg_id_tmp;
	    saved_sub->dialog.id.len = len;

	    SCA_STR_COPY( &saved_sub->dialog.id, &update_sub->dialog.call_id );
	    SCA_STR_APPEND( &saved_sub->dialog.id,
			    &update_sub->dialog.from_tag );
	    SCA_STR_APPEND( &saved_sub->dialog.id, &update_sub->dialog.to_tag );

	    saved_sub->dialog.call_id.s = saved_sub->dialog.id.s;
	    saved_sub->dialog.call_id.len = update_sub->dialog.call_id.len;

	    saved_sub->dialog.from_tag.s = saved_sub->dialog.id.s +
					   update_sub->dialog.call_id.len;
	    saved_sub->dialog.from_tag.len = update_sub->dialog.from_tag.len;

	    saved_sub->dialog.to_tag.s = saved_sub->dialog.id.s +
					 update_sub->dialog.call_id.len +
					 update_sub->dialog.from_tag.len;
	    saved_sub->dialog.to_tag.len = update_sub->dialog.to_tag.len;
	}
    }

    saved_sub->state = update_sub->state;
    saved_sub->dialog.subscribe_cseq = update_sub->dialog.subscribe_cseq;
    saved_sub->dialog.notify_cseq += 1;
    saved_sub->expires = time( NULL ) + update_sub->expires;

    /* flag subscription for DB update only if we've already inserted */ 
    if ( saved_sub->db_cmd_flag == SCA_DB_FLAG_NONE ) {
	saved_sub->db_cmd_flag = SCA_DB_FLAG_UPDATE;
    }

    if ( update_sub->index != SCA_CALL_INFO_APPEARANCE_INDEX_ANY ) {
	saved_sub->index = update_sub->index;
    }

    /* set notify_cseq in update_sub, since we use it to send the NOTIFY */
    update_sub->dialog.notify_cseq = saved_sub->dialog.notify_cseq;

    /* ensure we send the NOTIFY back through the same path as the SUBSCRIBE */
    if ( SCA_STR_EMPTY( &update_sub->rr ) && !SCA_STR_EMPTY( &saved_sub->rr )) {
	update_sub->rr.s = (char *)pkg_malloc( saved_sub->rr.len );
	if ( update_sub->rr.s == NULL ) {
	    LM_ERR( "sca_subscription_update_unsafe: pkg_malloc record-route "
		    "value %.*s failed", STR_FMT( &saved_sub->rr ));
	    goto done;
	}

	SCA_STR_COPY( &update_sub->rr, &saved_sub->rr );
    }

    rc = 1;

done:
    return( rc );
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
sca_subscription_delete_subscriber_for_event( sca_mod *scam, str *subscriber,
	str *event, str *aor )
{
    sca_hash_slot	*slot;
    sca_hash_entry	*ent;
    str			subkey = STR_NULL;
    char		skbuf[ 1024 ];
    int			slot_idx;
    int			len;

    len = aor->len;
    len += event->len;

    if ( len >= sizeof( skbuf )) {
	LM_ERR( "Subscription key %.*s%.*s: too long",
		STR_FMT( aor ), STR_FMT( event ));
	return( -1 );
    }

    subkey.s = skbuf;
    SCA_STR_COPY( &subkey, aor );
    SCA_STR_APPEND( &subkey, event );

    slot_idx = sca_hash_table_index_for_key( scam->subscriptions, &subkey );

    slot = sca_hash_table_slot_for_index( sca->subscriptions, slot_idx );
    sca_hash_table_lock_index( scam->subscriptions, slot_idx );

    ent = sca_hash_table_slot_kv_find_entry_unsafe( slot, subscriber );
    if ( ent != NULL ) {
        ent = sca_hash_table_slot_unlink_entry_unsafe( slot, ent );
    }

    sca_hash_table_unlock_index( sca->subscriptions, slot_idx );

    if ( ent != NULL ) {
	sca_hash_entry_free( ent );
    }

    return( 1 );
}

/* caller must pkg_free req_sub->rr.s and req_sub->dialog.to_tag.s */
    int
sca_subscription_from_request( sca_mod *scam, sip_msg_t *msg, int event_type,
	sca_subscription *req_sub )
{
    struct to_body		tmp_to = { 0 };
    struct to_body		*to, *from;
    str				contact_uri;
    str				to_tag = STR_NULL;
    unsigned int		expires = 0, max_expires;
    unsigned int		cseq;

    assert( req_sub != NULL );

    memset( req_sub, 0, sizeof( sca_subscription ));

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
	max_expires = scam->cfg->line_seize_max_expires;
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

    if ( sca_get_msg_contact_uri( msg, &contact_uri ) < 0 ) {
	/* above logs error */
	goto error;
    }

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
    if ( to_tag.s == NULL ) {
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

	if ( !SCA_HEADER_EMPTY( msg->record_route )) {
	    if ( print_rr_body( msg->record_route, &req_sub->rr,
				0, NULL ) < 0 ) {
		LM_ERR( "Failed to parse Record-Route header %.*s in "
			"SUBSCRIBE %.*s from %.*s",
			STR_FMT( &msg->record_route->body ),
			STR_FMT( &REQ_LINE( msg ).uri ),
			STR_FMT( &contact_uri ));
		goto error;
	    }
	}
    }

    req_sub->subscriber = contact_uri;
    if ( sca_uri_extract_aor( &REQ_LINE( msg ).uri, &req_sub->target_aor) < 0) {
	LM_ERR( "Failed to extract AoR from RURI %.*s",
		STR_FMT( &REQ_LINE( msg ).uri ));
	goto error;
    }
    req_sub->event = event_type;
    req_sub->index = SCA_CALL_INFO_APPEARANCE_INDEX_ANY;
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

    req_sub->dialog.to_tag.s = pkg_malloc( to_tag.len );
    if ( req_sub->dialog.to_tag.s == NULL ) {
	LM_ERR( "Failed to pkg_malloc space for to-tag %.*s",
		STR_FMT( &to_tag ));
	goto error;
    }
    SCA_STR_COPY( &req_sub->dialog.to_tag, &to_tag );
    
    req_sub->dialog.subscribe_cseq = 0;
    req_sub->dialog.notify_cseq = 0;

    free_to_params( &tmp_to );

    return( 1 );

error:
    free_to_params( &tmp_to );

    if ( !SCA_STR_EMPTY( &req_sub->rr )) {
	pkg_free( req_sub->rr.s );
	req_sub->rr.s = NULL;
    }

    return( -1 );
}

    int
sca_handle_subscribe( sip_msg_t *msg, char *p1, char *p2 )
{
    sca_subscription	req_sub;
    sca_subscription	*sub = NULL;
    sca_call_info	call_info;
    hdr_field_t		*call_info_hdr = NULL;
    str			sub_key = STR_NULL;
    str			*to_tag = NULL;
    char		*status_text;
    int			event_type;
    int			status;
    int			app_idx = SCA_CALL_INFO_APPEARANCE_INDEX_ANY;
    int			idx = -1;
    int			rc = -1;
    int			released = 0;

    if ( parse_headers( msg, HDR_EOH_F, 0 ) < 0 ) {
	LM_ERR( "header parsing failed: bad request" );
	SCA_SUB_REPLY_ERROR( sca, 400, "Bad Request", msg );
	return( -1 );
    }

    if ( !STR_EQ( REQ_LINE( msg ).method, SCA_METHOD_SUBSCRIBE )) {
	LM_ERR( "bad request method %.*s", STR_FMT( &REQ_LINE( msg ).method ));
	SCA_SUB_REPLY_ERROR( sca, 500, "Internal server error - config", msg );
	return( -1 );
    }

    if ( SCA_HEADER_EMPTY( msg->event )) {
	SCA_SUB_REPLY_ERROR( sca, 400, "Missing Event", msg );
	return( -1 );
    }

    event_type = sca_event_from_str( &msg->event->body );
    if ( event_type == SCA_EVENT_TYPE_UNKNOWN ) {
	SCA_SUB_REPLY_ERROR( sca, 400, "Bad Event", msg );
	return( -1 );
    }

    if ( sca_subscription_from_request( sca, msg, event_type, &req_sub ) < 0 ) {
	SCA_SUB_REPLY_ERROR( sca, 400,
		"Bad Shared Call Appearance Request", msg );
	return( -1 );
    }
    if ( sca_subscription_copy_subscription_key( &req_sub, &sub_key ) < 0 ) {
	SCA_SUB_REPLY_ERROR( sca, 500, "Internal Server Error - "
			    "copy dialog id", msg );
	goto done;
    }
    sca_subscription_print( &req_sub );

    /* check to see if the message has a to-tag */
    to_tag = &(get_to( msg )->tag_value);

    /* XXX should lock starting here and use unsafe methods below? */

    /* ensure we only calculate the hash table index once */
    idx = sca_hash_table_index_for_key( sca->subscriptions,
					&sub_key );
    /* pkg_malloc'd in sca_subscription_copy_subscription_key above */
    pkg_free( sub_key.s );

    if ( req_sub.event == SCA_EVENT_TYPE_LINE_SEIZE ) {
	call_info_hdr = sca_call_info_header_find( msg->headers );
	if ( call_info_hdr ) {
	    if ( sca_call_info_body_parse( &call_info_hdr->body,
		    &call_info ) < 0 ) {
		SCA_SUB_REPLY_ERROR( sca, 400, "Bad Request - "
				"Invalid Call-Info header", msg );
		goto done;
	    }
	    req_sub.index = app_idx = call_info.index;
	} else {
	    SCA_SUB_REPLY_ERROR( sca, 400, "Bad Request - "
			    "missing Call-Info header", msg );
	    goto done;
	}
    }

    sca_hash_table_lock_index( sca->subscriptions, idx );

    sub = sca_hash_table_index_kv_find_unsafe( sca->subscriptions, idx,
					&req_sub.subscriber ); 

    if ( sub != NULL ) {
	/* this will remove the subscription if expires == 0 */
	if ( sca_subscription_update_unsafe( sca, sub, &req_sub, idx ) < 0 ) {
	    SCA_SUB_REPLY_ERROR( sca, 500, "Internal Server Error - "
				"update subscription", msg );
	    goto done;
	}

	if ( req_sub.event == SCA_EVENT_TYPE_LINE_SEIZE ) {
	    if ( req_sub.expires == 0 ) {
		/* release the seized appearance */
		if ( call_info_hdr == NULL ) {
		    SCA_SUB_REPLY_ERROR( sca, 400, "Bad Request - "
				    "missing Call-Info header", msg );
		    goto done;
		}
	
		if ( sca_appearance_release_index( sca, &req_sub.target_aor,
			call_info.index ) != SCA_APPEARANCE_OK ) {
		    SCA_SUB_REPLY_ERROR( sca, 500, "Internal Server Error - "
					"release seized line", msg );
		    goto done;
		}
	    } else if ( SCA_STR_EMPTY( to_tag )) {
		/* don't seize new index if this is a line-seize reSUBSCRIBE */
		app_idx = sca_appearance_seize_index( sca, &req_sub.target_aor,
				app_idx, &req_sub.subscriber );
		if ( app_idx == SCA_APPEARANCE_INDEX_UNAVAILABLE ) {
		    SCA_SUB_REPLY_ERROR( sca, 480,
				"Temporarily Unavailable", msg );
		    goto done;
		} else if ( app_idx < 0 ) {
		    SCA_SUB_REPLY_ERROR( sca, 500,
			"Internal Server Error - seize appearance index", msg );
		    goto done;
		}
		req_sub.index = app_idx;
	    }
	} else {
	    if ( SCA_STR_EMPTY( to_tag )) {
		/*
		 * if the subscriber owns any active appearances, clear them.
		 * we assume that an out-of-dialog SUBSCRIBE for a subscriber
		 * with active appearances is indicative of a reboot.
		 */
		released = sca_appearance_owner_release_all(
						&req_sub.target_aor,
						&req_sub.subscriber );
		if ( released ) {
		    LM_INFO( "sca_handle_subscribe: released %d appearances "
				"for subscriber %.*s", released,
				STR_FMT( &req_sub.subscriber ));
		}
	    }
	}
    } else {
	/* in-dialog request, but we didn't find it. */
	if ( !SCA_STR_EMPTY( to_tag )) {
	    SCA_SUB_REPLY_ERROR( sca, 481,
		    "Call Leg/Transaction Does Not Exist", msg );
	    goto done;
	}

	if ( req_sub.expires > 0 ) {
	    if ( req_sub.event == SCA_EVENT_TYPE_LINE_SEIZE ) {
		app_idx = sca_appearance_seize_index( sca, &req_sub.target_aor,
				app_idx, &req_sub.subscriber );
		if ( app_idx == SCA_APPEARANCE_INDEX_UNAVAILABLE ) {
		    SCA_SUB_REPLY_ERROR( sca, 480,
					"Temporarily Unavailable", msg );
		    goto done;
		} else if ( app_idx < 0 ) {
		    SCA_SUB_REPLY_ERROR( sca, 500, "Internal Server Error - "
					"seize appearance index", msg );
		    goto done;
		}
		req_sub.index = app_idx;
	    }

	    if ( sca_subscription_save_unsafe( sca, &req_sub, idx,
				SCA_SUBSCRIPTION_CREATE_OPT_DEFAULT ) < 0 ) {
		SCA_SUB_REPLY_ERROR( sca, 500, "Internal Server Error - "
				    "save subscription", msg );
		goto done;
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

    sca_hash_table_unlock_index( sca->subscriptions, idx );
    idx = -1;

    status = sca_ok_status_for_event( event_type );
    status_text = sca_ok_text_for_event( event_type );
    if ( sca_subscription_reply( sca, status, status_text, event_type,
		    req_sub.expires, msg ) < 0 ) {
	SCA_SUB_REPLY_ERROR( sca, 500, "Internal server error", msg );
	goto done;
    }

    if ( sca_notify_subscriber( sca, &req_sub, app_idx ) < 0 ) {
	LM_ERR( "SCA %s SUBSCRIBE+NOTIFY for %.*s failed",
		sca_event_name_from_type( req_sub.event ),
		STR_FMT( &req_sub.subscriber ));
	/*
	 * XXX - what does subscriber do in this case? drop subscription?
	 * sub is already saved/updated in hash table. let it rot?
	 */
	goto done;
    }

    if ( req_sub.event == SCA_EVENT_TYPE_LINE_SEIZE || released ) {
	if ( sca_notify_call_info_subscribers( sca, &req_sub.target_aor) < 0 ) {
	    LM_ERR( "SCA %s NOTIFY to all %.*s subscribers failed",
		    sca_event_name_from_type( req_sub.event ),
		    STR_FMT( &req_sub.target_aor ));
	    goto done;
	}
    }

    rc = 1;

done:
    if ( idx >= 0 ) {
	sca_hash_table_unlock_index( sca->subscriptions, idx );
    }

    if ( req_sub.dialog.to_tag.s != NULL ) {
	pkg_free( req_sub.dialog.to_tag.s );
    }
    if ( req_sub.rr.s != NULL ) {
	pkg_free( req_sub.rr.s );
    }

    return( rc );
}

    int
sca_subscription_reply( sca_mod *scam, int status_code, char *status_msg,
	int event_type, int expires, sip_msg_t *msg )
{
    str			extra_headers = STR_NULL;
    char		hdr_buf[ 1024 ];
    int			len;

    if ( event_type != SCA_EVENT_TYPE_CALL_INFO &&
		event_type != SCA_EVENT_TYPE_LINE_SEIZE ) {
	LM_ERR( "sca_subscription_reply: unrecognized event type %d",
		event_type );
	return( -1 );
    }

    if ( status_code < 300 ) {
	/* Add Event, Contact, Allow-Events and Expires headers */
	extra_headers.s = hdr_buf;
	len = snprintf( extra_headers.s, sizeof( hdr_buf ),
		"Event: %s%s", sca_event_name_from_type( event_type ), CRLF );
	if ( len >= sizeof( hdr_buf ) || len < 0 ) {
	    LM_ERR( "sca_subscription_reply: extra headers too long" );
	    return( -1 );
	}
	extra_headers.len = len;

	SCA_STR_APPEND_CSTR( &extra_headers, "Contact: " );
	SCA_STR_APPEND( &extra_headers, &REQ_LINE( msg ).uri );
	SCA_STR_APPEND_CSTR( &extra_headers, CRLF );

	SCA_STR_COPY_CSTR( &extra_headers,
		"Allow-Events: call-info, line-seize" CRLF );

	len = snprintf( extra_headers.s + extra_headers.len,
		sizeof( hdr_buf ) - extra_headers.len,
		"Expires: %d%s", expires, CRLF );
	if ( len >= (sizeof( hdr_buf ) - extra_headers.len) || len < 0 ) {
	    LM_ERR( "sca_subscription_reply: extra headers too long" );
	    return( -1 );
	}
	extra_headers.len += len;
    } else if ( status_code == 480 ) {
	/* tell loser of line-seize SUBSCRIBE race to try again shortly */
	extra_headers.s = hdr_buf;
	len = snprintf( extra_headers.s, sizeof( hdr_buf ),
			"Retry-After: %d%s", 1, CRLF );
	extra_headers.len = len;
    }

    return( sca_reply( scam, status_code, status_msg, &extra_headers, msg ));
}

/*
 * return values:
 *	-1: error
 *	 0: no subscription found to terminate
 *	 1: subscription terminated
 */
    int
sca_subscription_terminate( sca_mod *scam, str *aor, int event,
	str *subscriber, int termination_state, int opts )
{
    sca_hash_slot	*slot;
    sca_hash_entry	*ent;
    sca_subscription	*sub;
    str			sub_key = STR_NULL;
    char		*event_name;
    int			slot_idx;
    int			len;

    if ( !(opts & SCA_SUBSCRIPTION_TERMINATE_OPT_UNSUBSCRIBE)) {
	LM_ERR( "sca_subscription_terminate: invalid opts 0x%x", opts );
	return( -1 );
    }

    event_name = sca_event_name_from_type( event );
    len = aor->len + strlen( event_name );
    sub_key.s = (char *)pkg_malloc( len );
    if ( sub_key.s == NULL ) {
	LM_ERR( "Failed to pkg_malloc key to look up %s "
		"subscription for %.*s", event_name, STR_FMT( aor ));
	return( -1 );
    }
    SCA_STR_COPY( &sub_key, aor );
    SCA_STR_APPEND_CSTR( &sub_key, event_name );

    slot_idx = sca_hash_table_index_for_key( scam->subscriptions, &sub_key );
    pkg_free( sub_key.s );
    sub_key.len = 0;

    slot = sca_hash_table_slot_for_index( sca->subscriptions, slot_idx );
    sca_hash_table_lock_index( scam->subscriptions, slot_idx );

    ent = sca_hash_table_slot_kv_find_entry_unsafe( slot, subscriber );
    if ( ent != NULL ) {
	ent = sca_hash_table_slot_unlink_entry_unsafe( slot, ent );
    }

    sca_hash_table_unlock_index( sca->subscriptions, slot_idx );

    if ( ent == NULL ) {
	LM_DBG( "No %s subscription for %.*s", event_name,
		STR_FMT( subscriber ));
	return( 0 );
    }

    sub = (sca_subscription *)ent->value;
    sub->expires = 0;
    sub->dialog.notify_cseq += 1;
    sub->state = termination_state;

    sca_subscription_print( sub );

    if ( sca_notify_subscriber( sca, sub, sub->index ) < 0 ) {
	LM_ERR( "SCA %s NOTIFY to %.*s failed",
		event_name, STR_FMT( &sub->subscriber ));

	/* fall through, we might be able to notify the others */
    }

    if (( opts & SCA_SUBSCRIPTION_TERMINATE_OPT_RELEASE_APPEARANCE ) &&
		sub->index != SCA_CALL_INFO_APPEARANCE_INDEX_ANY ) {
	if ( sca_appearance_release_index( sca, &sub->target_aor,
		sub->index ) == SCA_APPEARANCE_OK ) {
	    if ( sca_notify_call_info_subscribers( sca, &sub->target_aor) < 0) {
		LM_ERR( "SCA %s NOTIFY to all %.*s subscribers failed",
			event_name, STR_FMT( &sub->target_aor ));
		/* fall through, not much we can do about it */
	    }
	}
    }

    if ( ent ) {
	sca_hash_entry_free( ent );
    }

    return( 1 );
}
