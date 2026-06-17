/*
 * pcontact_index.h
 */
#ifndef PCSCF_PCONTACT_INDEX_H
#define PCSCF_PCONTACT_INDEX_H

#include <time.h>
#include "../../core/str.h"

/* avoid heavy usrloc includes in UNIT_TEST builds - provide minimal pcontact_t */
#ifdef UNIT_TEST
typedef struct pcontact
{
	int _dummy;
} pcontact_t;
#else
typedef struct pcontact pcontact_t;
#endif

#define PCSCF_INDEX_SIZE 256

typedef struct pcscf_index_entry
{
	str key;
	pcontact_t *contact;
	struct pcscf_index_entry *next;
	struct pcscf_index_entry *prev;
} pcscf_index_entry_t;

/* actual struct tag is declared in usrloc.h as forward-decl */
struct pcscf_index
{
	pcscf_index_entry_t *table[PCSCF_INDEX_SIZE];
	int count;
};

typedef struct pcscf_index pcscf_index_t;

/* API */
int pcscf_index_init(pcscf_index_t *idx);
void pcscf_index_destroy(pcscf_index_t *idx);
int pcscf_index_add(pcscf_index_t *idx, str *key, pcontact_t *c);
int pcscf_index_replace(
		pcscf_index_t *idx, str *old_key, str *new_key, pcontact_t *c);
pcscf_index_entry_t *pcscf_index_get(pcscf_index_t *idx, str *key);
int pcscf_index_remove_key(pcscf_index_t *idx, str *key);
int pcscf_index_remove_contact(pcscf_index_t *idx, pcontact_t *c);
/* synchronize index entries for a contact across all indexes */
struct udomain;
int pcscf_index_sync_contact(struct udomain *d, pcontact_t *c);
int pcscf_temp_gruu_lru_init(int size);
void pcscf_temp_gruu_lru_destroy(void);
int pcscf_temp_gruu_lru_add(str *temp_gruu, pcontact_t *c, time_t expires);
pcontact_t *pcscf_temp_gruu_lru_get(str *temp_gruu);

#endif
