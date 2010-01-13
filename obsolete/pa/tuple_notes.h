#ifndef __TUPLE_NOTES_H
#define __TUPLE_NOTES_H

#include "presentity.h"

int db_read_tuple_notes(presentity_t *p, presence_tuple_t *t, db_con_t* db);
int db_add_tuple_notes(presentity_t *p, presence_tuple_t *t); /* add all notes for tuple into DB */
int db_remove_tuple_notes(presentity_t *p, presence_tuple_t *t); /* remove all notes for tuple */
int db_update_tuple_notes(presentity_t *p, presence_tuple_t *t);

/* adds note to tuple in memory, not in DB (use update)! */
void add_tuple_note_no_wb(presence_tuple_t *t, presence_note_t *n);

/* frees all notes for given tuple (in memory only, not DB) */
void free_tuple_notes(presence_tuple_t *t);


#endif
