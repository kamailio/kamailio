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

#include "../../lib/kcore/strcommon.h"

#include "sca.h"
#include "sca_appearance.h"
#include "sca_hash.h"
#include "sca_notify.h"
#include "sca_util.h"

const str SCA_APPEARANCE_INDEX_STR = STR_STATIC_INIT( "appearance-index" );
const str SCA_APPEARANCE_STATE_STR = STR_STATIC_INIT( "appearance-state" );
const str SCA_APPEARANCE_URI_STR = STR_STATIC_INIT( "appearance-uri" );

const str SCA_APPEARANCE_STATE_STR_IDLE = STR_STATIC_INIT( "idle" );
const str SCA_APPEARANCE_STATE_STR_SEIZED = STR_STATIC_INIT( "seized" );
const str SCA_APPEARANCE_STATE_STR_PROGRESSING = STR_STATIC_INIT("progressing");
const str SCA_APPEARANCE_STATE_STR_ALERTING = STR_STATIC_INIT( "alerting" );
const str SCA_APPEARANCE_STATE_STR_ACTIVE = STR_STATIC_INIT( "active" );
const str SCA_APPEARANCE_STATE_STR_HELD = STR_STATIC_INIT( "held" );
const str SCA_APPEARANCE_STATE_STR_HELD_PRIVATE = STR_STATIC_INIT("held-private");
const str SCA_APPEARANCE_STATE_STR_UNKNOWN = STR_STATIC_INIT( "unknown" );


/* STR_ACTIVE is repeated, once for ACTIVE_PENDING, once for ACTIVE */
const str	*state_names[] = {
			&SCA_APPEARANCE_STATE_STR_IDLE,
			&SCA_APPEARANCE_STATE_STR_SEIZED,
			&SCA_APPEARANCE_STATE_STR_PROGRESSING,
			&SCA_APPEARANCE_STATE_STR_ALERTING,
			&SCA_APPEARANCE_STATE_STR_ACTIVE,
			&SCA_APPEARANCE_STATE_STR_ACTIVE,
			&SCA_APPEARANCE_STATE_STR_HELD,
			&SCA_APPEARANCE_STATE_STR_HELD_PRIVATE,
		};
#define SCA_APPEARANCE_STATE_NAME_COUNT \
	( sizeof( state_names ) / sizeof( state_names[ 0 ] ))

    void
sca_appearance_state_to_str( int state, str *state_str )
{
    assert( state_str != NULL );

    if ( state >= SCA_APPEARANCE_STATE_NAME_COUNT || state < 0 ) {
	state_str->len = SCA_APPEARANCE_STATE_STR_UNKNOWN.len;
	state_str->s = SCA_APPEARANCE_STATE_STR_UNKNOWN.s;

	return;
    }

    state_str->len = state_names[ state ]->len;
    state_str->s = state_names[ state ]->s;
}

    int
sca_appearance_state_from_str( str *state_str )
{
    int			state;

    assert( state_str != NULL );

    for ( state = 0; state < SCA_APPEARANCE_STATE_NAME_COUNT; state++ ) {
	if ( SCA_STR_EQ( state_str, state_names[ state ] )) {
	    break;
	}
    }
    if ( state >= SCA_APPEARANCE_STATE_NAME_COUNT ) {
	state = SCA_APPEARANCE_STATE_UNKNOWN;
    }

    return( state );
}

    sca_appearance *
sca_appearance_create( int appearance_index, str *owner_uri )
{
    sca_appearance	*new_appearance = NULL;

    /*
     * we use multiple shm_malloc calls here because uri, owner,
     * dialog and callee are mutable. could also shm_malloc a big
     * block and divide it among the strs....
     */

    new_appearance = (sca_appearance *)shm_malloc( sizeof( sca_appearance ));
    if ( new_appearance == NULL ) {
	LM_ERR( "Failed to shm_malloc new sca_appearance for %.*s, index %d",
		STR_FMT( owner_uri ), appearance_index );
	goto error;
    }
    memset( new_appearance, 0, sizeof( sca_appearance ));

    new_appearance->owner.s = (char *)shm_malloc( owner_uri->len );
    if ( new_appearance->owner.s == NULL ) {
	LM_ERR( "Failed to shm_malloc space for owner %.*s, index %d",
		STR_FMT( owner_uri ), appearance_index );
	goto error;
    }
    SCA_STR_COPY( &new_appearance->owner, owner_uri );

    new_appearance->index = appearance_index;
    new_appearance->times.ctime = time( NULL );
    sca_appearance_update_state_unsafe( new_appearance,
				SCA_APPEARANCE_STATE_IDLE );
    new_appearance->next = NULL;

    return( new_appearance );

error:
    if ( new_appearance != NULL ) {
	if ( !SCA_STR_EMPTY( &new_appearance->owner )) {
	    shm_free( new_appearance->owner.s );
	}

	shm_free( new_appearance );
    }

    return( NULL );
}

    void
sca_appearance_free( sca_appearance *appearance )
{
    if ( appearance != NULL ) {
	if ( appearance->owner.s != NULL ) {
	    shm_free( appearance->owner.s );
	}
	if ( appearance->uri.s != NULL ) {
	    shm_free( appearance->uri.s );
	}
	if ( appearance->dialog.id.s != NULL ) {
	    shm_free( appearance->dialog.id.s );
	}

        if ( appearance->prev_owner.s != NULL ) {
            shm_free( appearance->prev_owner.s );
        }
        if ( appearance->prev_callee.s != NULL ) {
            shm_free( appearance->prev_callee.s );
        }
        if ( appearance->prev_dialog.id.s != NULL ) {
            shm_free( appearance->prev_dialog.id.s );
        }

	shm_free( appearance );
    }
}

/*
 * assumes slot for app_entries is locked.
 *
 * appearance-index values are 1-indexed.
 * return values: 
 *	 -1:	error
 *	>=1:	index reserved for claimant
 */
    static int
sca_appearance_list_next_available_index_unsafe( sca_appearance_list *app_list )
{
    sca_appearance	*app_cur;
    int			idx = 1;

    assert( app_list != NULL );

    for ( app_cur = app_list->appearances; app_cur != NULL;
			app_cur = app_cur->next, idx++ ) {
	if ( idx < app_cur->index ) {
	    break;
	}
    }

    return( idx );
}

    static sca_appearance_list *
sca_appearance_list_create( sca_mod *scam, str *aor )
{
    sca_appearance_list	*app_list;
    int			len;

    len = sizeof( sca_appearance_list ) + aor->len;
    app_list = (sca_appearance_list *)shm_malloc( len );
    if ( app_list == NULL ) {
	LM_ERR( "Failed to shm_malloc sca_appearance_list for %.*s",
		STR_FMT( aor ));
	return( NULL );
    }
    memset( app_list, 0, sizeof( sca_appearance_list ));

    len = sizeof( sca_appearance_list );
    app_list->aor.s = (char *)app_list + len;
    SCA_STR_COPY( &app_list->aor, aor );

    return( app_list );
}

    sca_appearance_list *
sca_appearance_list_for_line( sca_mod *scam, str *aor )
{
    //sca_appearance_list

    return( NULL );
}

    void
sca_appearance_list_insert_appearance( sca_appearance_list *app_list,
	sca_appearance *app )
{
    sca_appearance	**cur;

    assert( app_list != NULL );
    assert( app != NULL );

    app->appearance_list = app_list;

    for ( cur = &app_list->appearances; *cur != NULL; cur = &(*cur)->next ) {
	if ( app->index < (*cur)->index ) {
	    break;
	}
    }

    app->next = *cur;
    *cur = app;
}

    sca_appearance *
sca_appearance_list_unlink_index( sca_appearance_list *app_list, int idx )
{
    sca_appearance	*app = NULL;
    sca_appearance	**cur;

    assert( app_list != NULL );
    assert( idx > 0 );

    for ( cur = &app_list->appearances; *cur != NULL; cur = &(*cur)->next ) {
	if ((*cur)->index == idx ) {
	    app = *cur;
	    app->appearance_list = NULL;

	    *cur = (*cur)->next;

	    break;
	}
    }

    if ( app == NULL ) {
	LM_ERR( "Tried to remove inactive %.*s appearance at index %d",
		STR_FMT( &app_list->aor ), idx );
    }

    return( app );
}

    int
sca_appearance_list_unlink_appearance( sca_appearance_list *app_list,
	sca_appearance **app )
{
    sca_appearance	**cur;
    int			rc = 0;

    assert( app_list != NULL );
    assert( app != NULL && *app != NULL );

    for ( cur = &app_list->appearances; *cur != NULL; cur = &(*cur)->next ) {
	if ( *cur == *app ) {
	    *cur = (*cur)->next;

	    (*app)->appearance_list = NULL;
	    (*app)->next = NULL;

	    rc = 1;

	    break;
	}
    }

    return( rc );
}

    int
sca_appearance_list_aor_cmp( str *aor, void *cmp_value )
{
    sca_appearance_list	*app_list = (sca_appearance_list *)cmp_value;
    int			cmp;

    if (( cmp = aor->len - app_list->aor.len ) != 0 ) {
	return( cmp );
    }

    return( memcmp( aor->s, app_list->aor.s, aor->len ));
}

    void
sca_appearance_list_print( void *value )
{
    sca_appearance_list *app_list = (sca_appearance_list *)value;
    sca_appearance	*app;
    str			state_str = STR_NULL;

    LM_INFO( "Appearance state for AoR %.*s:", STR_FMT( &app_list->aor ));

    for ( app = app_list->appearances; app != NULL; app = app->next ) {
	sca_appearance_state_to_str( app->state, &state_str );
	LM_INFO( "index: %d, state: %.*s, uri: %.*s, owner: %.*s, "
		"callee: %.*s, dialog: %.*s;%.*s;%.*s",
		app->index, STR_FMT( &state_str ),
		STR_FMT( &app->uri ), STR_FMT( &app->owner ),
		STR_FMT( &app->callee ), STR_FMT( &app->dialog.call_id ),
		STR_FMT( &app->dialog.from_tag ),
		STR_FMT( &app->dialog.to_tag ));
    }
}

    void
sca_appearance_list_free( void *value )
{
    sca_appearance_list	*app_list = (sca_appearance_list *)value;
    sca_appearance	*app, *app_tmp;

    LM_DBG( "Freeing appearance list for AoR %.*s", STR_FMT( &app_list->aor ));

    for ( app = app_list->appearances; app != NULL; app = app_tmp ) {
	app_tmp = app->next;

	shm_free( app );
    }

    shm_free( app_list );
}

    int
sca_appearance_register( sca_mod *scam, str *aor )
{
    sca_appearance_list	*app_list;
    int			rc = -1;

    assert( scam != NULL );
    assert( aor != NULL );

    if ( sca_uri_is_shared_appearance( scam, aor )) {
	/* we've already registered */
	rc = 0;
	goto done;
    }

    app_list = sca_appearance_list_create( scam, aor );
    if ( app_list == NULL ) {
	goto done;
    }

    if ( sca_hash_table_kv_insert( scam->appearances, aor, app_list,
		    sca_appearance_list_aor_cmp,
		    sca_appearance_list_print,
		    sca_appearance_list_free ) < 0 ) {
	LM_ERR( "sca_appearance_register: failed to insert appearance list "
		"for %.*s", STR_FMT( aor ));
	goto done;
    }

    rc = 1;

done:
    return( rc );
}

    int
sca_appearance_unregister( sca_mod *scam, str *aor )
{
    int			rc = 0;

    assert( scam != NULL );
    assert( aor != NULL );

    if ( sca_uri_is_shared_appearance( scam, aor )) {
	if (( rc = sca_hash_table_kv_delete( scam->appearances, aor )) == 0 ) {
	    rc = 1;
	    LM_INFO( "unregistered SCA AoR %.*s", STR_FMT( aor ));
	}
    }

    return( rc );
}

    sca_appearance *
sca_appearance_seize_index_unsafe( sca_mod *scam, str *aor, str *owner_uri,
	int app_idx, int slot_idx, int *seize_error )
{
    sca_appearance_list	*app_list;
    sca_appearance	*app = NULL;
    sca_hash_slot	*slot;
    int			error = SCA_APPEARANCE_ERR_UNKNOWN;

    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    app_list = sca_hash_table_slot_kv_find_unsafe( slot, aor );
    if ( app_list == NULL ) {
	LM_ERR( "sca_appearance_seize_index_unsafe: no appearance list for "
		"%.*s", STR_FMT( aor ));
	goto done;
    }

    if ( app_idx <= 0 ) {
        app_idx = sca_appearance_list_next_available_index_unsafe( app_list );
    }

    for ( app = app_list->appearances; app != NULL; app = app->next ) {
	if ( app->index >= app_idx ) {
	    break;
	}
    }
    if ( app != NULL && app->index == app_idx ) {
	/* attempt to seize in-use appearance-index */
	error = SCA_APPEARANCE_ERR_INDEX_UNAVAILABLE;
	app = NULL;
	goto done;
    }

    app = sca_appearance_create( app_idx, owner_uri );
    if ( app == NULL ) {
        LM_ERR( "Failed to create new appearance for %.*s at index %d",
                STR_FMT( owner_uri ), app_idx );
	error = SCA_APPEARANCE_ERR_MALLOC;
        goto done;
    }
    sca_appearance_update_state_unsafe( app, SCA_APPEARANCE_STATE_SEIZED );

    sca_appearance_list_insert_appearance( app_list, app );

    error = SCA_APPEARANCE_OK;

done:
    if ( seize_error ) {
	*seize_error = error;
    }

    return( app );
}

    int
sca_appearance_seize_index( sca_mod *scam, str *aor, int idx, str *owner_uri )
{
    sca_appearance	*app;
    int			slot_idx;
    int			app_idx = -1;
    int			error = SCA_APPEARANCE_OK;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    sca_hash_table_lock_index( scam->appearances, slot_idx );

    app = sca_appearance_seize_index_unsafe( scam, aor, owner_uri,
						idx, slot_idx, &error );
    if ( app != NULL ) {
	app_idx = app->index;
    }

    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    if ( error == SCA_APPEARANCE_ERR_INDEX_UNAVAILABLE ) {
	app_idx = SCA_APPEARANCE_INDEX_UNAVAILABLE;
    }

    return( app_idx );
}

    sca_appearance *
sca_appearance_seize_next_available_unsafe( sca_mod *scam, str *aor,
	str *owner_uri, int slot_idx )
{
    sca_appearance_list	*app_list;
    sca_appearance	*app = NULL;
    sca_hash_slot	*slot;
    int			idx = -1;

    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    app_list = sca_hash_table_slot_kv_find_unsafe( slot, aor );
    if ( app_list == NULL ) {
	app_list = sca_appearance_list_create( scam, aor );
	if ( app_list == NULL ) {
	    goto done;
	}

	if ( sca_hash_table_slot_kv_insert_unsafe( slot, app_list,
			sca_appearance_list_aor_cmp,
			sca_appearance_list_print,
			sca_appearance_list_free ) < 0 ) {
	    LM_ERR( "Failed to insert appearance list for %.*s",
			STR_FMT( aor ));
	    goto done;
	}
    }

    /* XXX this grows without bound. add modparam to set a hard limit */
    idx = sca_appearance_list_next_available_index_unsafe( app_list );
    /* XXX check idx > any configured max appearance index */

    app = sca_appearance_create( idx, owner_uri );
    if ( app == NULL ) {
	LM_ERR( "Failed to create new appearance for %.*s at index %d",
		STR_FMT( owner_uri ), idx );
	goto done;
    }
    sca_appearance_update_state_unsafe( app, SCA_APPEARANCE_STATE_SEIZED );

    sca_appearance_list_insert_appearance( app_list, app );

done:
    return( app );
}

    int
sca_appearance_seize_next_available_index( sca_mod *scam, str *aor,
	str *owner_uri )
{
    sca_appearance	*app;
    int			slot_idx;
    int			idx = -1;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    sca_hash_table_lock_index( scam->appearances, slot_idx );

    app = sca_appearance_seize_next_available_unsafe( scam, aor,
					owner_uri, slot_idx );
    if ( app != NULL ) {
	idx = app->index;
    }

    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    return( idx );
}

    void
sca_appearance_update_state_unsafe( sca_appearance *app, int state )
{
    assert( app != NULL );

    app->state = state;
    app->times.mtime = time( NULL );
}

    int
sca_appearance_update_owner_unsafe( sca_appearance *app, str *owner )
{
    assert( app != NULL );
    assert( owner != NULL );

    if ( !SCA_STR_EMPTY( &app->owner )) {
	if ( app->prev_owner.s != NULL ) {
	    shm_free( app->prev_owner.s );
	}
	app->prev_owner.s = app->owner.s;
	app->prev_owner.len = app->owner.len;
    }

    app->owner.s = (char *)shm_malloc( owner->len );
    if ( app->owner.s == NULL ) {
	LM_ERR( "sca_appearance_update_owner_unsafe: shm_malloc for new "
		"owner %.*s failed: out of memory", STR_FMT( owner ));
	goto error;
    }
    SCA_STR_COPY( &app->owner, owner );

    return( 1 );

error:
    /* restore owner */
    app->owner.s = app->prev_owner.s;
    app->owner.len = app->prev_owner.len;
    memset( &app->prev_owner, 0, sizeof( str ));

    return( -1 );
}

    int
sca_appearance_update_callee_unsafe( sca_appearance *app, str *callee )
{
    assert( app != NULL );
    assert( callee != NULL );

    if ( !SCA_STR_EMPTY( &app->callee )) {
	if ( app->prev_callee.s != NULL ) {
	    shm_free( app->prev_callee.s );
	}
	app->prev_callee.s = app->callee.s;
	app->prev_callee.len = app->callee.len;
    }

    app->callee.s = (char *)shm_malloc( callee->len );
    if ( app->callee.s == NULL ) {
	LM_ERR( "sca_appearance_update_owner_unsafe: shm_malloc for new "
		"callee %.*s failed: out of memory", STR_FMT( callee ));
	goto error;
    }
    SCA_STR_COPY( &app->callee, callee );

    return( 1 );

error:
    /* restore callee */
    app->callee.s = app->prev_callee.s;
    app->callee.len = app->prev_callee.len;
    memset( &app->prev_callee, 0, sizeof( str ));

    return( -1 );
}
    int
sca_appearance_update_dialog_unsafe( sca_appearance *app, str *call_id,
	str *from_tag, str *to_tag )
{
    int		len;

    assert( app != NULL );
    assert( call_id != NULL );
    assert( from_tag != NULL );

    if ( !SCA_STR_EMPTY( &app->dialog.id )) {
	if ( app->prev_dialog.id.s != NULL ) {
	    shm_free( app->prev_dialog.id.s );
	}
	app->prev_dialog.id.s = app->dialog.id.s;
	app->prev_dialog.id.len = app->dialog.id.len;

	app->prev_dialog.call_id.s = app->dialog.call_id.s;
	app->prev_dialog.call_id.len = app->dialog.call_id.len;

	app->prev_dialog.from_tag.s = app->dialog.from_tag.s;
	app->prev_dialog.from_tag.len = app->dialog.from_tag.len;

	app->prev_dialog.to_tag.s = app->dialog.to_tag.s;
	app->prev_dialog.to_tag.len = app->dialog.to_tag.len;
    }

    len = call_id->len + from_tag->len;
    if ( !SCA_STR_EMPTY( to_tag )) {
	len += to_tag->len;
    }

    app->dialog.id.s = (char *)shm_malloc( len );
    if ( app->dialog.id.s == NULL ) {
	LM_ERR( "sca_appearance_update_dialog_unsafe: shm_malloc new dialog "
		"failed: out of memory" );
	goto error;
    }
    SCA_STR_COPY( &app->dialog.id, call_id );
    SCA_STR_APPEND( &app->dialog.id, from_tag );

    app->dialog.call_id.s = app->dialog.id.s;
    app->dialog.call_id.len = call_id->len;

    app->dialog.from_tag.s = app->dialog.id.s + call_id->len;
    app->dialog.from_tag.len = from_tag->len;

    app->dialog.to_tag.s = app->dialog.id.s + call_id->len + from_tag->len;
    app->dialog.to_tag.len = to_tag->len;

    return( 1 );

error:
    /* restore dialog */
    app->prev_dialog.id.s = app->dialog.id.s;
    app->prev_dialog.id.len = app->dialog.id.len;

    app->prev_dialog.call_id.s = app->dialog.call_id.s;
    app->prev_dialog.call_id.len = app->dialog.call_id.len;

    app->prev_dialog.from_tag.s = app->dialog.from_tag.s;
    app->prev_dialog.from_tag.len = app->dialog.from_tag.len;

    app->prev_dialog.to_tag.s = app->dialog.to_tag.s;
    app->prev_dialog.to_tag.len = app->dialog.to_tag.len;

    memset( &app->prev_dialog, 0, sizeof( sca_dialog ));

    return( -1 );
}

    int
sca_appearance_update_unsafe( sca_appearance *app, int state, str *display,
	str *uri, sca_dialog *dialog, str *owner, str *callee )
{
    int			rc = SCA_APPEARANCE_OK;
    int			len;

    if ( state != SCA_APPEARANCE_STATE_UNKNOWN ) {
	sca_appearance_update_state_unsafe( app, state );
    }

    if ( !SCA_STR_EMPTY( uri )) {
	if ( !SCA_STR_EMPTY( &app->uri )) {
	    /* the uri str's s member is shm_malloc'd separately */
	    shm_free( app->uri.s );
	    memset( &app->uri, 0, sizeof( str ));
	}

	/* +2 for left & right carets surrounding URI */
	len = uri->len + 2;
	if ( !SCA_STR_EMPTY( display )) {
	    /* cheaper to scan string than shm_malloc 2x display? */
	    len += sca_uri_display_escapes_count( display );
	    /* +1 for space between display & uri */
	    len += display->len + 1;
	}
	app->uri.s = (char *)shm_malloc( len );
	if ( app->uri.s == NULL ) {
	    LM_ERR( "shm_malloc %d bytes returned NULL", uri->len );
	    rc = SCA_APPEARANCE_ERR_MALLOC;
	    goto done;
	}

	if ( !SCA_STR_EMPTY( display )) {
	    /* copy escaped display information... */
	    app->uri.len = escape_common( app->uri.s, display->s,
					  display->len );

	    /* ... and add a space between it and the uri */
	    *(app->uri.s + app->uri.len) = ' ';
	    app->uri.len++;
	}

	*(app->uri.s + app->uri.len) = '<';
	app->uri.len++;

	SCA_STR_APPEND( &app->uri, uri );

	*(app->uri.s + app->uri.len) = '>';
	app->uri.len++;
    }

    if ( !SCA_DIALOG_EMPTY( dialog )) {
	if ( !SCA_STR_EQ( &dialog->id, &app->dialog.id )) {
	    if ( app->dialog.id.s != NULL ) {
		shm_free( app->dialog.id.s );
	    }

	    app->dialog.id.s = (char *)shm_malloc( dialog->id.len );
	    if ( app->dialog.id.s == NULL ) {
		LM_ERR( "sca_appearance_update_unsafe: shm_malloc dialog id "
			"failed: out of shared memory" );
		/* XXX this seems bad enough to abort... */
		return( -1 );
	    }
	    SCA_STR_COPY( &app->dialog.id, &dialog->id );

	    app->dialog.call_id.s = app->dialog.id.s;
	    app->dialog.call_id.len = dialog->call_id.len;

	    app->dialog.from_tag.s = app->dialog.id.s + dialog->call_id.len;
	    app->dialog.from_tag.len = dialog->from_tag.len;

	    if ( !SCA_STR_EMPTY( &dialog->to_tag )) {
		app->dialog.to_tag.s = app->dialog.id.s +
					dialog->call_id.len +
					dialog->from_tag.len;
		app->dialog.to_tag.len = dialog->to_tag.len;
	    } else {
		app->dialog.to_tag.s = NULL;
		app->dialog.to_tag.len = 0;
	    }
	}
    }

    /* note these two blocks could be condensed and inlined */
    if ( !SCA_STR_EMPTY( owner )) {
	if ( !SCA_STR_EQ( &app->owner, owner )) {
	    if ( app->owner.s != NULL ) {
		shm_free( app->owner.s );
	    }

	    app->owner.s = (char *)shm_malloc( owner->len );
	    if ( app->owner.s == NULL ) {
		LM_ERR( "sca_appearance_update_unsafe: shm_malloc "
			"appearance owner URI failed: out of shared memory" );
		return( -1 );
	    }
	    SCA_STR_COPY( &app->owner, owner );
	}
    }

    if ( !SCA_STR_EMPTY( callee )) {
	if ( !SCA_STR_EQ( &app->callee, callee )) {
	    if ( app->callee.s != NULL ) {
		shm_free( app->callee.s );
	    }

	    app->callee.s = (char *)shm_malloc( callee->len );
	    if ( app->callee.s == NULL ) {
		LM_ERR( "sca_appearance_update_unsafe: shm_malloc "
			"appearance callee URI failed: out of shared memory" );
		return( -1 );
	    }
	    SCA_STR_COPY( &app->callee, callee );
	}
    }

done:
    return( rc );
}

    int
sca_uri_is_shared_appearance( sca_mod *scam, str *aor )
{
    sca_hash_slot	*slot;
    sca_appearance_list	*app_list;
    int			slot_idx;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    sca_hash_table_lock_index( scam->appearances, slot_idx );
    app_list = sca_hash_table_slot_kv_find_unsafe( slot, aor );
    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    if ( app_list == NULL ) {
	return( 0 );
    }

    return( 1 );
}

    int
sca_uri_lock_shared_appearance( sca_mod *scam, str *aor )
{
    sca_hash_slot	*slot;
    sca_appearance_list	*app_list;
    int			slot_idx;

    if ( SCA_STR_EMPTY( aor )) {
	return( -1 );
    }

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    sca_hash_table_lock_index( scam->appearances, slot_idx );
    app_list = sca_hash_table_slot_kv_find_unsafe( slot, aor );

    if ( app_list == NULL ) {
	sca_hash_table_unlock_index( scam->appearances, slot_idx );
	slot_idx = -1;
    }

    return( slot_idx );
}

    int
sca_uri_lock_if_shared_appearance( sca_mod *scam, str *aor, int *slot_idx )
{
    sca_hash_slot	*slot;
    sca_appearance_list	*app_list;

    assert( slot_idx != NULL );

    if ( SCA_STR_EMPTY( aor )) {
	*slot_idx = -1;
	return( 0 );
    }

    *slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, *slot_idx );

    sca_hash_table_lock_index( scam->appearances, *slot_idx );
    app_list = sca_hash_table_slot_kv_find_unsafe( slot, aor );

    if ( app_list == NULL ) {
	sca_hash_table_unlock_index( scam->appearances, *slot_idx );
	*slot_idx = -1;

	return( 0 );
    }

    return( 1 );
}

    int
sca_appearance_state_for_index( sca_mod *scam, str *aor, int idx )
{
    sca_hash_slot	*slot;
    sca_appearance_list	*app_list;
    sca_appearance	*app;
    int			slot_idx;
    int			state = SCA_APPEARANCE_STATE_UNKNOWN;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );
    
    sca_hash_table_lock_index( scam->appearances, slot_idx );

    app_list = sca_hash_table_slot_kv_find_unsafe( slot, aor );
    if ( app_list == NULL ) {
	LM_DBG( "%.*s has no in-use appearances", STR_FMT( aor ));
	goto done;
    }

    for ( app = app_list->appearances; app != NULL; app = app->next ) {
	if ( app->index == idx ) {
	    break;
	}
    }
    if ( app == NULL ) {
	LM_WARN( "%.*s appearance-index %d is not in use",
		STR_FMT( aor ), idx );
	goto done;
    }

    state = app->state;

done:
    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    return( state );
}

    int
sca_appearance_update_index( sca_mod *scam, str *aor, int idx,
	int state, str *display, str *uri, sca_dialog *dialog )
{
    sca_hash_slot	*slot;
    sca_appearance_list	*app_list;
    sca_appearance	*app;
    str			state_str = STR_NULL;
    int			len;
    int			slot_idx;
    int			rc = SCA_APPEARANCE_ERR_UNKNOWN;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    sca_hash_table_lock_index( scam->appearances, slot_idx );

    sca_appearance_state_to_str( state, &state_str );

    app_list = sca_hash_table_slot_kv_find_unsafe( slot, aor );
    if ( app_list == NULL ) {
	LM_WARN( "Cannot update %.*s index %d to state %.*s: %.*s has no "
		 "in-use appearances", STR_FMT( aor ), idx,
		 STR_FMT( &state_str ), STR_FMT( aor ));
	rc = SCA_APPEARANCE_ERR_NOT_IN_USE;
	goto done;
    }

    for ( app = app_list->appearances; app != NULL; app = app->next ) {
	if ( app->index == idx ) {
	    break;
	} else if ( idx == 0 ) {
	    if ( SCA_STR_EQ( &dialog->id, &app->dialog.id )) {
		break;
	    }
	}
    }
    if ( app == NULL ) {
	LM_WARN( "Cannot update %.*s index %d to %.*s: index %d not in use",
		 STR_FMT( aor ), idx, STR_FMT( &state_str ), idx );
	rc = SCA_APPEARANCE_ERR_INDEX_INVALID;
	goto done;
    }

    if ( state != SCA_APPEARANCE_STATE_UNKNOWN && app->state != state ) {
	sca_appearance_update_state_unsafe( app, state );
    }

    if ( !SCA_STR_EMPTY( uri )) {
	if ( !SCA_STR_EMPTY( &app->uri )) {
	    /* the uri str's s member is shm_malloc'd separately */
	    shm_free( app->uri.s );
	    memset( &app->uri, 0, sizeof( str ));
	}

	/* +2 for left & right carets surrounding URI */
	len = uri->len + 2;
	if ( !SCA_STR_EMPTY( display )) {
	    /* cheaper to scan string than shm_malloc 2x display? */
	    len += sca_uri_display_escapes_count( display );
	    /* +1 for space between display & uri */
	    len += display->len + 1;
	}
	app->uri.s = (char *)shm_malloc( len );
	if ( app->uri.s == NULL ) {
	    LM_ERR( "Failed to update %.*s index %d uri to %.*s: "
		    "shm_malloc %d bytes returned NULL",
		    STR_FMT( aor ), idx, STR_FMT( uri ), uri->len );
	    rc = SCA_APPEARANCE_ERR_MALLOC;
	    goto done;
	}

	if ( !SCA_STR_EMPTY( display )) {
	    /* copy escaped display information... */
	    app->uri.len = escape_common( app->uri.s, display->s,
					  display->len );

	    /* ... and add a space between it and the uri */
	    *(app->uri.s + app->uri.len) = ' ';
	    app->uri.len++;
	}

	*(app->uri.s + app->uri.len) = '<';
	app->uri.len++;

	SCA_STR_APPEND( &app->uri, uri );

	*(app->uri.s + app->uri.len) = '>';
	app->uri.len++;
    }

    if ( !SCA_DIALOG_EMPTY( dialog )) {
	if ( !SCA_STR_EQ( &dialog->id, &app->dialog.id )) {
	    if ( app->dialog.id.s != NULL ) {
		shm_free( app->dialog.id.s );
	    }

	    app->dialog.id.s = (char *)shm_malloc( dialog->id.len );
	    SCA_STR_COPY( &app->dialog.id, &dialog->id );

	    app->dialog.call_id.s = app->dialog.id.s;
	    app->dialog.call_id.len = dialog->call_id.len;

	    app->dialog.from_tag.s = app->dialog.id.s + dialog->call_id.len;
	    app->dialog.from_tag.len = dialog->from_tag.len;

	    if ( !SCA_STR_EMPTY( &dialog->to_tag )) {
		app->dialog.to_tag.s = app->dialog.id.s +
					dialog->call_id.len +
					dialog->from_tag.len;
		app->dialog.to_tag.len = dialog->to_tag.len;
	    } else {
		app->dialog.to_tag.s = NULL;
		app->dialog.to_tag.len = 0;
	    }
	}
    }

    rc = SCA_APPEARANCE_OK;

done:
    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    return( rc );
}

    int
sca_appearance_release_index( sca_mod *scam, str *aor, int idx )
{
    sca_hash_slot	*slot;
    sca_hash_entry	*ent;
    sca_appearance_list *app_list = NULL;
    sca_appearance	*app;
    int			slot_idx;
    int			rc = SCA_APPEARANCE_ERR_UNKNOWN;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    sca_hash_table_lock_index( scam->appearances, slot_idx );

    app_list = NULL;
    for ( ent = slot->entries; ent != NULL; ent = ent->next ) {
	if ( ent->compare( aor, ent->value ) == 0 ) {
	    app_list = (sca_appearance_list *)ent->value;
	    break;
	}
    }
    if ( app_list == NULL ) {
	LM_ERR( "No appearances for %.*s", STR_FMT( aor ));
	rc = SCA_APPEARANCE_ERR_NOT_IN_USE;
	goto done;
    }

    app = sca_appearance_list_unlink_index( app_list, idx );
    if ( app == NULL ) {
	LM_ERR( "Failed to unlink %.*s appearance-index %d: invalid index",
		STR_FMT( aor ), idx );
	rc = SCA_APPEARANCE_ERR_INDEX_INVALID;
	goto done;
    }
    sca_appearance_free( app );

    rc = SCA_APPEARANCE_OK;
    
done:
    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    return( rc );
}

    int
sca_appearance_owner_release_all( str *aor, str *owner )
{
    sca_appearance_list	*app_list = NULL;
    sca_appearance	*app, **cur_app, **tmp_app;
    sca_hash_slot	*slot;
    sca_hash_entry	*ent;
    int			slot_idx = -1;
    int			released = -1;

    slot_idx = sca_uri_lock_shared_appearance( sca, aor );
    slot = sca_hash_table_slot_for_index( sca->appearances, slot_idx );

    for ( ent = slot->entries; ent != NULL; ent = ent->next ) {
	if ( ent->compare( aor, ent->value ) == 0 ) {
	    app_list = (sca_appearance_list *)ent->value;
	    break;
	}
    }

    released = 0;

    if ( app_list == NULL ) {
	LM_DBG( "sca_appearance_owner_release_all: No appearances for %.*s",
		STR_FMT( aor ));
	goto done;
    }

    for ( cur_app = &app_list->appearances; *cur_app != NULL;
		cur_app = tmp_app ) {
	tmp_app = &(*cur_app)->next;

	if ( !SCA_STR_EQ( owner, &(*cur_app)->owner )) {
	    continue;
	}

	app = *cur_app;
	*cur_app = (*cur_app)->next;
	tmp_app = cur_app;

	if ( app ) {
	    sca_appearance_free( app );
	    released++;
	}
    }

done:
    if ( slot_idx >= 0 ) {
	sca_hash_table_unlock_index( sca->appearances, slot_idx );
    }

    return( released );
}

    sca_appearance *
sca_appearance_for_index_unsafe( sca_mod *scam, str *aor, int app_idx,
	int slot_idx )
{
    sca_appearance_list	*app_list;
    sca_appearance	*app = NULL;
    sca_hash_slot	*slot;
    sca_hash_entry	*ent;

    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    app_list = NULL;
    for ( ent = slot->entries; ent != NULL; ent = ent->next ) {
	if ( ent->compare( aor, ent->value ) == 0 ) {
	    app_list = (sca_appearance_list *)ent->value;
	    break;
	}
    }
    if ( app_list == NULL ) {
	LM_ERR( "No appearances for %.*s", STR_FMT( aor ));
	return( NULL );
    }

    for ( app = app_list->appearances; app != NULL; app = app->next ) {
	if ( app->index == app_idx ) {
	    break;
	}
    }
    
    return( app );
}

    sca_appearance *
sca_appearance_for_dialog_unsafe( sca_mod *scam, str *aor, sca_dialog *dialog,
	int slot_idx )
{
    sca_appearance_list	*app_list;
    sca_appearance	*app = NULL;
    sca_hash_slot	*slot;
    sca_hash_entry	*ent;

    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    app_list = NULL;
    for ( ent = slot->entries; ent != NULL; ent = ent->next ) {
	if ( ent->compare( aor, ent->value ) == 0 ) {
	    app_list = (sca_appearance_list *)ent->value;
	    break;
	}
    }
    if ( app_list == NULL ) {
	LM_ERR( "No appearances for %.*s", STR_FMT( aor ));
	return( NULL );
    }

    for ( app = app_list->appearances; app != NULL; app = app->next ) {
	if ( SCA_STR_EQ( &app->dialog.call_id, &dialog->call_id ) &&
		SCA_STR_EQ( &app->dialog.from_tag, &dialog->from_tag )) {
#ifdef notdef
	    if ( !SCA_STR_EMPTY( &app->dialog.to_tag ) &&
		    !SCA_STR_EMPTY( &dialog->to_tag ) &&
		    !SCA_STR_EQ( &app->dialog.to_tag, &dialog->to_tag )) {
		continue;
	    }
#endif /* notdef */
	    break;
	}
    }
    
    return( app );
} 

    sca_appearance *
sca_appearance_for_tags_unsafe( sca_mod *scam, str *aor,
	str *call_id, str *from_tag, str *to_tag, int slot_idx )
{
    sca_dialog		dialog;
    char		dlg_buf[ 1024 ];

    dialog.id.s = dlg_buf;
    if ( sca_dialog_build_from_tags( &dialog, sizeof( dlg_buf ),
		call_id, from_tag, to_tag ) < 0 ) {
	LM_ERR( "sca_appearance_for_tags_unsafe: failed to build dialog "
		"from tags" );
	return( NULL );
    }

    return( sca_appearance_for_dialog_unsafe( scam, aor, &dialog, slot_idx ));
}

    sca_appearance  *
sca_appearance_unlink_by_tags( sca_mod *scam, str *aor,
	str *call_id, str *from_tag, str *to_tag )
{
    sca_appearance	*app = NULL, *unl_app;
    int			slot_idx = -1;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    sca_hash_table_lock_index( scam->appearances, slot_idx );

    app = sca_appearance_for_tags_unsafe( scam, aor, call_id, from_tag,
					to_tag, slot_idx );
    if ( app == NULL ) {
	LM_ERR( "sca_appearance_unlink_by_tags: no appearances found for %.*s "
		"with dialog %.*s;%.*s;%.*s", STR_FMT( aor ),
		 STR_FMT( call_id ), STR_FMT( from_tag ),  STR_FMT( to_tag ));
	goto done;
    }

    unl_app = sca_appearance_list_unlink_index( app->appearance_list,
						app->index );
    if ( unl_app == NULL || unl_app != app ) {
	LM_ERR( "sca_appearance_unlink_by_tags: failed to unlink %.*s "
		"appearance-index %d", STR_FMT( aor ), app->index );
	app = NULL;
	goto done;
    }

done:
    if ( slot_idx >= 0 ) {
	sca_hash_table_unlock_index( scam->appearances, slot_idx );
    }

    return( app );
}

    void
sca_appearance_purge_stale( unsigned int ticks, void *param )
{
    struct notify_list {
	struct notify_list	*next;
	str			aor;
    };

    sca_mod		*scam = (sca_mod *)param;
    sca_hash_table	*ht;
    sca_hash_entry	*ent;
    sca_appearance_list	*app_list;
    sca_appearance	**cur_app, **tmp_app, *app = NULL;
    struct notify_list	*notify_list = NULL, *tmp_nl;
    int			i;
    int			unlinked;
    time_t		now, ttl;

    LM_INFO( "SCA: purging stale appearances" );

    assert( scam != NULL );
    assert( scam->appearances != NULL );

    now = time( NULL );

    ht = scam->appearances;
    for ( i = 0; i < ht->size; i++ ) {
	sca_hash_table_lock_index( ht, i );

	for ( ent = ht->slots[ i ].entries; ent != NULL; ent = ent->next ) {
	    app_list = (sca_appearance_list *)ent->value;
	    if ( app_list == NULL ) {
		continue;
	    }

	    unlinked = 0;

	    for ( cur_app = &app_list->appearances; *cur_app != NULL;
			cur_app = tmp_app ) {
		tmp_app = &(*cur_app)->next;

		switch ((*cur_app)->state ) {
		case SCA_APPEARANCE_STATE_ACTIVE_PENDING:
		    ttl = SCA_APPEARANCE_STATE_PENDING_TTL;
		    break;

		case SCA_APPEARANCE_STATE_SEIZED:
		    ttl = SCA_APPEARANCE_STATE_SEIZED_TTL;
		    break;

		default:
		    /* XXX for now just skip other appearances */
		    ttl = now + 60;
		    break;
		}
		if (( now - (*cur_app)->times.mtime ) < ttl ) {
		    continue;
		}

		/* unlink stale appearance */
		app = *cur_app;
		*cur_app = (*cur_app)->next;
		tmp_app = cur_app;

		if ( app ) {
		    sca_appearance_free( app );
		}

		if ( unlinked ) {
		    /* we've already added this AoR to the NOTIFY list */
		    continue;
		}
		unlinked++;

		/*
		 * can't notify while slot is locked. make a list of AoRs to
		 * notify after unlocking.
		 */
		tmp_nl = (struct notify_list *)pkg_malloc(
				sizeof( struct notify_list ));
		if ( tmp_nl == NULL ) {
		    LM_ERR( "sca_appearance_purge_stale: failed to pkg_malloc "
			    "notify list entry for %.*s",
			    STR_FMT( &app_list->aor ));
		    continue;
		}

		tmp_nl->aor.s = (char *)pkg_malloc( app_list->aor.len );
		if ( tmp_nl->aor.s == NULL ) {
		    LM_ERR( "sca_appearance_purge_stale: failed to pkg_malloc "
			    "space for copy of %.*s",
			    STR_FMT( &app_list->aor ));
		    pkg_free( tmp_nl );
		    continue;
		}
		SCA_STR_COPY( &tmp_nl->aor, &app_list->aor );

		/* simple insert-at-head. order doesn't matter. */
		tmp_nl->next = notify_list;
		notify_list = tmp_nl;
	    }
	}

	sca_hash_table_unlock_index( ht, i );

	for ( ; notify_list != NULL; notify_list = tmp_nl ) {
	    tmp_nl = notify_list->next;

	    LM_INFO( "sca_appearance_purge_stale: notifying %.*s call-info "
		    "subscribers", STR_FMT( &notify_list->aor ));

	    if ( sca_notify_call_info_subscribers( scam,
			&notify_list->aor ) < 0 ) {
		LM_ERR( "sca_appearance_purge_stale: failed to send "
			"call-info NOTIFY %.*s subscribers",
			STR_FMT( &notify_list->aor ));
		/* fall through, free memory anyway */
	    }

	    if ( notify_list->aor.s ) {
		pkg_free( notify_list->aor.s );
	    }
	    pkg_free( notify_list );
	}
    }
}
