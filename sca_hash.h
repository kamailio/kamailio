#ifndef SCA_HASH_H
#define SCA_HASH_H

struct _sca_hash_slot;
struct _sca_hash_entry {
    void			*value;
    int				(*compare)( str *, void * );
    void			(*description)( void * );
    void			(*free_entry)( void * );
    struct _sca_hash_slot	*slot;
    struct _sca_hash_entry	*prev;
    struct _sca_hash_entry	*next;
};
typedef struct _sca_hash_entry	sca_hash_entry;

struct _sca_hash_slot {
    gen_lock_t			lock;
    sca_hash_entry		*entries;
    sca_hash_entry		**last_entry;
};
typedef struct _sca_hash_slot	sca_hash_slot;

struct _sca_hash_table {
    unsigned int		size;	/* power of two */
    sca_hash_slot		*slots;
};
typedef struct _sca_hash_table	sca_hash_table;

#define sca_hash_table_index_for_key( ht1, str1 ) \
	core_hash((str1), NULL, (ht1)->size)

#define sca_hash_table_slot_for_index( ht1, idx1 ) \
	&(ht1)->slots[ (idx1) ]

#define sca_hash_table_lock_index( ht1, idx1 ) \
	lock_get( &(ht1)->slots[ (idx1) ].lock )

#define sca_hash_table_unlock_index( ht1, idx1 ) \
	lock_release( &(ht1)->slots[ (idx1) ].lock )
	

/* hash table operations */
int	sca_hash_table_create( sca_hash_table **, unsigned int );
void	sca_hash_table_print( sca_hash_table * );
void	sca_hash_table_free( sca_hash_table * );

void	sca_hash_entry_free( sca_hash_entry * );

/* key-value operations */
int	sca_hash_table_slot_kv_insert_unsafe( sca_hash_slot *, void *,
				int (*)(str *, void *),
				void (*)(void *), void (*)(void *));
int	sca_hash_table_slot_kv_insert( sca_hash_slot *, void *,
				int (*)(str *, void *),
				void (*)(void *), void (*)(void *));
int	sca_hash_table_index_kv_insert( sca_hash_table *, int, void *,
				int (*)(str *, void *),
				void (*)(void *), void (*)(void *));
int	sca_hash_table_kv_insert( sca_hash_table *, str *, void *,
				int (*)(str *, void *),
				void (*)(void *), void (*)(void *));
void	*sca_hash_table_slot_kv_find_unsafe( sca_hash_slot *, str * );
void	*sca_hash_table_slot_kv_find( sca_hash_slot *, str * );
void	*sca_hash_table_index_kv_find_unsafe( sca_hash_table *, int, str * );
void	*sca_hash_table_index_kv_find( sca_hash_table *, int, str * );
void	*sca_hash_table_kv_find( sca_hash_table *, str * );

sca_hash_entry	*sca_hash_table_slot_kv_find_entry_unsafe( sca_hash_slot *,
							    str * );
sca_hash_entry *sca_hash_table_slot_unlink_entry_unsafe( sca_hash_slot *,
						 	sca_hash_entry * );

int	sca_hash_table_slot_kv_delete( sca_hash_slot *, str * );
int	sca_hash_table_index_kv_delete( sca_hash_table *, int, str * );
int	sca_hash_table_kv_delete( sca_hash_table *, str * );

#endif /* SCA_HASH_H */
