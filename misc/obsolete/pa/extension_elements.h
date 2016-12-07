#ifndef __EXTENSION_ELEMENTS_H
#define __EXTENSION_ELEMENTS_H

#include "presentity.h"

/* EXTENSION ELEMENT functions */

int db_update_extension_element(presentity_t *p, pa_extension_element_t *n);
void add_extension_element(presentity_t *_p, pa_extension_element_t *n);
void free_pa_extension_element(pa_extension_element_t *n);
void remove_extension_element(presentity_t *_p, pa_extension_element_t *n);
int remove_extension_elements(presentity_t *p, str *etag);
pa_extension_element_t *create_pa_extension_element(str *etag, str *element, time_t expires, str *dbid);
int db_read_extension_elements(presentity_t *p, db_con_t* db);
pa_extension_element_t *extension_element2pa(extension_element_t *n, str *etag, time_t expires);


#endif
