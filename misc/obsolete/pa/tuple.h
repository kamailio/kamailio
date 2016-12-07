#ifndef __TUPLE_H
#define __TUPLE_H

/* PA tuple functions (data structures are defined in presentity.h) */

#include "presentity.h"

/* Create a new presence_tuple */
int new_presence_tuple(str* _contact, time_t expires, 
		presence_tuple_t ** _t, int is_published, str *id,
		str *published_id, str *etag);

/* add presence tuple to presentity and to database */
void add_presence_tuple(presentity_t *_p, presence_tuple_t *_t);

/* Remove tuple from presentity and from database too */
void remove_presence_tuple(presentity_t *_p, presence_tuple_t *_t);

/* Free all memory associated with a presence_tuple */
void free_presence_tuple(presence_tuple_t * _t);

/* Find a tuple for contact _contact on presentity _p - only registered contacts ! */
int find_registered_presence_tuple(str* _contact, presentity_t *_p, presence_tuple_t ** _t);

/* Find tuple with given id */
int find_presence_tuple_id(str* id, presentity_t *_p, presence_tuple_t ** _t);

/* Find published tuple with given ID (ID used for publication, not tuple ID!) */
presence_tuple_t *find_published_tuple(presentity_t *presentity, str *etag, str *id);

/** Function reads all tuples from DB for given presentity */
int db_read_tuples(presentity_t *_p, db_con_t* db);

/* update tuple status in database */
int db_update_presence_tuple(presentity_t *_p, presence_tuple_t *t, int update_notes_and_ext);

/* creates new tuple from given information (needed for publishing */
presence_tuple_t *presence_tuple_info2pa(presence_tuple_info_t *i, str *etag, time_t expires);

/* updates published information */
void update_tuple(presentity_t *p, presence_tuple_t *t, presence_tuple_info_t *i, time_t expires);

#endif
