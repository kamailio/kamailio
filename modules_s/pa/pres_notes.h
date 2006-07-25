#ifndef __PA_PRES_NOTES_H
#define __PA_PRES_NOTES_H

#include "presentity.h"

int db_update_pres_note(presentity_t *p, pa_presence_note_t *n);
void add_pres_note(presentity_t *_p, pa_presence_note_t *n);
void free_pres_note(pa_presence_note_t *n);
void remove_pres_note(presentity_t *_p, pa_presence_note_t *n);
int remove_pres_notes(presentity_t *p, str *etag);
pa_presence_note_t *create_pres_note(str *etag, str *note, str *lang, time_t expires, str *dbid);
int db_read_notes(presentity_t *p, db_con_t* db);

pa_presence_note_t *presence_note2pa(presence_note_t *n, str *etag, time_t expires);

#endif
