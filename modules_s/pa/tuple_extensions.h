#ifndef __TUPLE_EXTENSIONS_H
#define __TUPLE_EXTENSIONS_H

#include "presentity.h"

int db_read_tuple_extensions(presentity_t *p, presence_tuple_t *t, db_con_t* db);
int db_add_tuple_extensions(presentity_t *p, presence_tuple_t *t); /* add all exts for tuple into DB */
int db_remove_tuple_extensions(presentity_t *p, presence_tuple_t *t); /* remove all exts for tuple */
int db_update_tuple_extensions(presentity_t *p, presence_tuple_t *t);

/* adds extension element to tuple in memory, not in DB (use update)! */
void add_tuple_extension_no_wb(presence_tuple_t *t, extension_element_t *n, int is_status_extension);

/* frees all notes for given tuple (in memory only, not DB) */
void free_tuple_extensions(presence_tuple_t *t);


#endif
