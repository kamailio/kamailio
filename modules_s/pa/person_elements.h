#ifndef __PERSON_ELEMENTS_H
#define __PERSON_ELEMENTS_H

#include "presentity.h"

/* PERSON ELEMENT functions - will be replaced by "extension" elements */

int db_update_person_element(presentity_t *p, pa_person_element_t *n);
void add_person_element(presentity_t *_p, pa_person_element_t *n);
void free_person_element(pa_person_element_t *n);
void remove_person_element(presentity_t *_p, pa_person_element_t *n);
int remove_person_elements(presentity_t *p, str *etag);
pa_person_element_t *create_person_element(str *etag, str *element, str *id, time_t expires, str *dbid);
int db_read_person_elements(presentity_t *p, db_con_t* db);
pa_person_element_t *person_element2pa(person_t *n, str *etag, time_t expires);


#endif
