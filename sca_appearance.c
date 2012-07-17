#include "sca_common.h"

#include "sca.h"
#include "sca_appearance.h"
#include "sca_hash.h"

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


const str	*state_names[] = {
			&SCA_APPEARANCE_STATE_STR_IDLE,
			&SCA_APPEARANCE_STATE_STR_SEIZED,
			&SCA_APPEARANCE_STATE_STR_PROGRESSING,
			&SCA_APPEARANCE_STATE_STR_ALERTING,
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
    new_appearance->state = SCA_APPEARANCE_STATE_IDLE;
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
LM_INFO( "ADMORTEN: idx: %d, app_cur->index: %d", idx, app_cur->index );
	if ( idx < app_cur->index ) {
LM_INFO( "ADMORTEN: app_cur->index - idx == %d", app_cur->index - idx );
	    break;
	}
    }
LM_INFO( "ADMORTEN: returning %d", idx );
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
    sca_appearance	*app;
    sca_appearance	**cur;
    sca_appearance	**prev = NULL;

    assert( app_list != NULL );
    assert( idx > 0 );

    for ( cur = &app_list->appearances; *cur != NULL;
			prev = cur, cur = &(*cur)->next ) {
	if ( idx == (*cur)->index ) {
	    break;
	}
    }
    if ( *cur == NULL ) {
	LM_ERR( "Tried to remove inactive %.*s appearance at index %d",
		STR_FMT( &app_list->aor ), idx );
	return( NULL );
    }
    app = *cur;

    if ( prev == NULL ) {
	app_list->appearances = (*cur)->next;
    } else {
	(*prev)->next = (*cur)->next;
    }

    return( app );
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
sca_appearance_seize_next_available_index( sca_mod *scam, str *aor,
	str *owner_uri )
{
    sca_appearance_list	*app_list;
    sca_appearance	*app;
    sca_hash_slot	*slot;
    int			slot_idx;
    int			idx = -1;

    slot_idx = sca_hash_table_index_for_key( scam->appearances, aor );
    slot = sca_hash_table_slot_for_index( scam->appearances, slot_idx );

    sca_hash_table_lock_index( scam->appearances, slot_idx );

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
    app->state = SCA_APPEARANCE_STATE_SEIZED;

    sca_appearance_list_insert_appearance( app_list, app );

done:
    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    return( idx );
}

    int
sca_appearance_update_index( sca_mod *scam, str *aor, int idx,
	int state, str *uri /* XXX will need to update dialog too */ )
{
    sca_hash_slot	*slot;
    sca_appearance_list	*app_list;
    sca_appearance	*app;
    str			state_str = STR_NULL;
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
	}
    }
    if ( app == NULL ) {
	LM_WARN( "Cannot update %.*s index %d to %.*s: index %d not in use",
		 STR_FMT( aor ), idx, STR_FMT( &state_str ), idx );
	rc = SCA_APPEARANCE_ERR_INVALID_INDEX;
	goto done;
    }

    app->state = state;
    if ( !SCA_STR_EMPTY( uri )) {
	if ( !SCA_STR_EMPTY( &app->uri )) {
	    /* the uri str's s member is shm_malloc'd separately */
	    shm_free( app->uri.s );
	    memset( &app->uri, 0, sizeof( str ));
	}
	app->uri.s = (char *)shm_malloc( uri->len );
	if ( app->uri.s == NULL ) {
	    LM_ERR( "Failed to update %.*s index %d uri to %.*s: "
		    "shm_malloc %d bytes returned NULL",
		    STR_FMT( aor ), idx, STR_FMT( uri ), uri->len );
	    rc = SCA_APPEARANCE_ERR_MALLOC;
	    goto done;
	}

	SCA_STR_COPY( &app->uri, uri );
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
	rc = SCA_APPEARANCE_ERR_INVALID_INDEX;
	goto done;
    }
    sca_appearance_free( app );

    rc = SCA_APPEARANCE_OK;
    
done:
    sca_hash_table_unlock_index( scam->appearances, slot_idx );

    return( rc );
}
