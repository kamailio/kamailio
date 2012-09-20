#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_hash.h"

    int
sca_hash_table_create( sca_hash_table **ht, unsigned int size )
{
    int		i;

    assert( ht != NULL );

    *ht = shm_malloc( sizeof( sca_hash_table ));
    if ( *ht == NULL ) {
	LM_ERR( "Failed to shm_malloc space for hash table" );
	return( -1 );
    }

    (*ht)->size = size;
    (*ht)->slots = (sca_hash_slot *)shm_malloc( size * sizeof(sca_hash_slot));
    if ((*ht)->slots == NULL ) {
	LM_ERR( "Failed to shm_malloc hash table slots" );
	shm_free( *ht ); *ht = NULL;
	return( -1 );
    }
    memset((*ht)->slots, 0, size * sizeof( sca_hash_slot ));

    for ( i = 0; i < (*ht)->size; i++ ) {
	if ( lock_init( &(*ht)->slots[ i ].lock ) == NULL ) {
	    LM_ERR( "Failed to initialized lock in hash table slot %d", i );
	    shm_free( *ht ); *ht = NULL;
	    return( -1 );
	}
    }

    return( 0 );
}

    int
sca_hash_table_slot_kv_insert_unsafe( sca_hash_slot *slot, void *value,
	int (*e_compare)( str *, void * ), void (*e_description)( void * ),
	void (*e_free)( void * ))
{
    sca_hash_entry	*new_entry;

    assert( slot != NULL );
    assert( value != NULL );
    assert( e_free != NULL );

    if ( slot->entries == NULL ) {
	slot->last_entry = &slot->entries;
    }

    new_entry = (sca_hash_entry *)shm_malloc( sizeof( sca_hash_entry ));
    if ( new_entry == NULL ) {
	LM_ERR( "Failed to shm_malloc new hash table entry for slot %p", slot );
	return( -1 );
    }
    new_entry->value = value;
    new_entry->compare = e_compare;
    new_entry->description = e_description;
    new_entry->free_entry = e_free;
    new_entry->next = NULL;

    new_entry->slot = slot;
    new_entry->prev = *slot->last_entry;

    if ( *slot->last_entry == NULL ) {
	*slot->last_entry = new_entry;
    } else {
	(*slot->last_entry)->next = new_entry;
	slot->last_entry = &(*slot->last_entry)->next;
    }

    return( 0 );
}

    int
sca_hash_table_slot_kv_insert( sca_hash_slot *slot, void *value,
	int (*e_compare)( str *, void * ), void (*e_description)( void * ),
	void (*e_free)( void * ))
{
    int		rc;

    lock_get( &slot->lock );

    rc = sca_hash_table_slot_kv_insert_unsafe( slot, value, e_compare,
    		e_description, e_free );

    lock_release( &slot->lock );

    return( rc );
}


    int
sca_hash_table_index_kv_insert( sca_hash_table *ht, int slot_idx, void *value,
	int (*e_compare)( str *, void * ), void (*e_description)( void * ),
	void (*e_free)( void * ))
{
    assert( ht != NULL );
    assert( ht->slots != NULL );
    assert( slot_idx >= 0 && slot_idx < ht->size );

    return( sca_hash_table_slot_kv_insert( &ht->slots[ slot_idx ], value,
					   e_compare, e_description,
					   e_free ));
}


    int
sca_hash_table_kv_insert( sca_hash_table *ht, str *key, void *value,
	int (*e_compare)(str *, void *), void (*e_description)(void *),
	void (*e_free)(void *))
{
    int		hash_idx;
    int		rc;

    assert( ht != NULL && !SCA_STR_EMPTY( key ) && value != NULL );

    hash_idx = core_hash( key, NULL, ht->size );
    rc = sca_hash_table_index_kv_insert( ht, hash_idx, value, e_compare,
					 e_description, e_free );

    return( rc );
}

    void *
sca_hash_table_slot_kv_find_unsafe( sca_hash_slot *slot, str *key )
{
    sca_hash_entry	*e;
    void		*value = NULL;

    assert( slot != NULL && !SCA_STR_EMPTY( key ));

    e = slot->entries;
    if ( e ) {
	if ( e->compare( key, e->value ) == 0 ) {
	    value = e->value;
	} else if ( e->next ) {
	    for ( ; e != NULL; e = e->next ) {
		if ( e->compare( key, e->value ) == 0 ) {
		    value = e->value;
		    break;
		}
	    }
	}
    }

    return( value );
}

    void *
sca_hash_table_slot_kv_find( sca_hash_slot *slot, str *key )
{
    void		*value;

    lock_get( &slot->lock );
    value = sca_hash_table_slot_kv_find_unsafe( slot, key );
    lock_release( &slot->lock );

    return( value );
}

    void *
sca_hash_table_index_kv_find_unsafe(sca_hash_table *ht, int slot_idx, str *key)
{
    assert( ht != NULL && !SCA_STR_EMPTY( key ));
    assert( slot_idx >= 0 && slot_idx < ht->size );

    return( sca_hash_table_slot_kv_find_unsafe( &ht->slots[ slot_idx ], key ));
}

    void *
sca_hash_table_index_kv_find( sca_hash_table *ht, int slot_idx, str *key )
{
    assert( ht != NULL && !SCA_STR_EMPTY( key ));
    assert( slot_idx >= 0 && slot_idx < ht->size );

    return( sca_hash_table_slot_kv_find( &ht->slots[ slot_idx ], key ));
}

    void *
sca_hash_table_kv_find( sca_hash_table *ht, str *key )
{
    int		slot_idx;

    slot_idx = core_hash( key, NULL, ht->size );

    return( sca_hash_table_index_kv_find( ht, slot_idx, key ));
}

    sca_hash_entry *
sca_hash_table_slot_kv_find_entry_unsafe( sca_hash_slot *slot, str *key )
{
    sca_hash_entry	*e = NULL;

    assert( slot != NULL && !SCA_STR_EMPTY( key ));

    e = slot->entries;
    if ( e ) {
	if ( e->compare( key, e->value ) != 0 && e->next ) {
	    for ( ; e != NULL; e = e->next ) {
		if ( e->compare( key, e->value ) == 0 ) {
		    break;
		}
	    }
	}
    }

    return( e );
}

    sca_hash_entry *
sca_hash_table_slot_kv_find_entry( sca_hash_slot *slot, str *key )
{
    sca_hash_entry	*e;

    lock_get( &slot->lock );
    e = sca_hash_table_slot_kv_find_entry( slot, key );
    lock_release( &slot->lock );

    return( e );		
}

    void
sca_hash_entry_free( sca_hash_entry *e )
{
    if ( e ) {
	e->free_entry( e->value );
	shm_free( e );
    }
}

    sca_hash_entry *
sca_hash_table_slot_unlink_entry_unsafe( sca_hash_slot *slot,
	sca_hash_entry *e )
{
    if ( e ) {
	if ( e->prev ) {
	    e->prev->next = e->next;
	}
	if ( e == *slot->last_entry ) {
	    slot->last_entry = &e->prev;
	}
	if ( e == slot->entries ) {
	    slot->entries = e->next;
	}
    }

    return( e );
}

    int
sca_hash_table_slot_kv_delete_unsafe( sca_hash_slot *slot, str *key )
{
    sca_hash_entry	*e;

    e = sca_hash_table_slot_kv_find_unsafe( slot, key );
    if ( e == NULL ) {
	return( -1 );
    }

    e = sca_hash_table_slot_unlink_entry_unsafe( slot, e );
    if ( e ) { 
	e->free_entry( e->value );
	shm_free( e );
    }

    return( 0 );
}

    int
sca_hash_table_slot_kv_delete( sca_hash_slot *slot, str *key )
{
    int			rc;

    lock_get( &slot->lock );
    rc = sca_hash_table_slot_kv_delete_unsafe( slot, key );	
    lock_release( &slot->lock );

    return( rc );
}

    int
sca_hash_table_index_kv_delete( sca_hash_table *ht, int slot_idx, str *key )
{
    return( sca_hash_table_slot_kv_delete( &ht->slots[ slot_idx ], key ));
}

    int
sca_hash_table_kv_delete( sca_hash_table *ht, str *key )
{
    int		slot_idx;

    slot_idx = core_hash( key, NULL, ht->size );

    return( sca_hash_table_index_kv_delete( ht, slot_idx, key ));
}

    static void
sca_hash_slot_print( sca_hash_slot *hs )
{
    sca_hash_entry	*e;

    for ( e = hs->entries; e != NULL; e = e->next ) {
	if ( e->description != NULL ) {
	    e->description( e->value );
	} else {
	    LM_DBG( "0x%p", e->value );
	}
    }
}

    void
sca_hash_table_print( sca_hash_table *ht )
{
    unsigned int	i;

    for ( i = 0; i < ht->size; i++ ) {
	LM_DBG( "SLOT %d:", i );
	sca_hash_slot_print( &ht->slots[ i ] );
    }
}

    void
sca_hash_table_free( sca_hash_table *ht )
{
    sca_hash_entry	*e, *e_tmp;
    unsigned int	i;

    if ( ht == NULL ) {
	return;
    }

    for ( i = 0; i < ht->size; i++ ) {
	if ( ht->slots[ i ].entries == NULL ) {
	    continue;
	}

	sca_hash_table_lock_index( ht, i );

	for ( e = ht->slots[ i ].entries; e != NULL; e = e_tmp ) {
	    e_tmp = e->next;

	    e->free_entry( e->value );

	    shm_free( e );
	}

	sca_hash_table_unlock_index( ht, i );

	lock_destroy( &ht->slots[ i ].lock );
	lock_dealloc( &ht->slots[ i ].lock );
    }

    shm_free( ht->slots );
    shm_free( ht );
}
