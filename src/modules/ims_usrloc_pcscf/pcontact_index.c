/*
 * pcontact_index.c
 */

#include "pcontact_index.h"
#include "../../core/hashes.h"
#ifdef UNIT_TEST
#define LM_ERR(fmt, ...) ((void)0)
#else
#include "../../core/dprint.h"
#endif
#ifndef UNIT_TEST
#include "usrloc.h"
#endif
#include <string.h>
#include <strings.h>

#ifdef UNIT_TEST
#include <stdlib.h>
#define PCSCF_IDX_MALLOC malloc
#define PCSCF_IDX_FREE free
#else
#define PCSCF_IDX_MALLOC shm_malloc
#define PCSCF_IDX_FREE shm_free
#endif

typedef struct pcscf_temp_gruu_lru_entry
{
	str temp_gruu;
	pcontact_t *contact;
	time_t expires;
} pcscf_temp_gruu_lru_entry_t;

static pcscf_temp_gruu_lru_entry_t *pcscf_temp_gruu_lru = NULL;
static int pcscf_temp_gruu_lru_capacity = 0;
static int pcscf_temp_gruu_lru_next = 0;

int pcscf_temp_gruu_lru_init(int size)
{
	if(size <= 0)
		size = 64;
	if(pcscf_temp_gruu_lru && pcscf_temp_gruu_lru_capacity == size)
		return 0;

	pcscf_temp_gruu_lru_destroy();
	pcscf_temp_gruu_lru = (pcscf_temp_gruu_lru_entry_t *)PCSCF_IDX_MALLOC(
			sizeof(pcscf_temp_gruu_lru_entry_t) * size);
	if(!pcscf_temp_gruu_lru) {
		LM_ERR("failed to allocate temp GRUU LRU\n");
		return -1;
	}
	memset(pcscf_temp_gruu_lru, 0, sizeof(pcscf_temp_gruu_lru_entry_t) * size);
	pcscf_temp_gruu_lru_capacity = size;
	pcscf_temp_gruu_lru_next = 0;
	return 0;
}

void pcscf_temp_gruu_lru_destroy(void)
{
	int i;

	if(!pcscf_temp_gruu_lru)
		return;

	for(i = 0; i < pcscf_temp_gruu_lru_capacity; i++) {
		if(pcscf_temp_gruu_lru[i].temp_gruu.s) {
			PCSCF_IDX_FREE(pcscf_temp_gruu_lru[i].temp_gruu.s);
			pcscf_temp_gruu_lru[i].temp_gruu.s = NULL;
			pcscf_temp_gruu_lru[i].temp_gruu.len = 0;
		}
	}
	PCSCF_IDX_FREE(pcscf_temp_gruu_lru);
	pcscf_temp_gruu_lru = NULL;
	pcscf_temp_gruu_lru_capacity = 0;
	pcscf_temp_gruu_lru_next = 0;
}

int pcscf_temp_gruu_lru_add(str *temp_gruu, pcontact_t *c, time_t expires)
{
	pcscf_temp_gruu_lru_entry_t *e;
	char *dup;

	if(!temp_gruu || !temp_gruu->s || temp_gruu->len <= 0 || !c)
		return -1;

	if(!pcscf_temp_gruu_lru && pcscf_temp_gruu_lru_init(64) < 0)
		return -1;

	e = &pcscf_temp_gruu_lru[pcscf_temp_gruu_lru_next];
	if(e->temp_gruu.s) {
		PCSCF_IDX_FREE(e->temp_gruu.s);
		e->temp_gruu.s = NULL;
		e->temp_gruu.len = 0;
	}

	dup = (char *)PCSCF_IDX_MALLOC(temp_gruu->len + 1);
	if(!dup)
		return -1;
	memcpy(dup, temp_gruu->s, temp_gruu->len);
	dup[temp_gruu->len] = '\0';

	e->temp_gruu.s = dup;
	e->temp_gruu.len = temp_gruu->len;
	e->contact = c;
	e->expires = expires;

	pcscf_temp_gruu_lru_next++;
	if(pcscf_temp_gruu_lru_next >= pcscf_temp_gruu_lru_capacity)
		pcscf_temp_gruu_lru_next = 0;

	return 0;
}

pcontact_t *pcscf_temp_gruu_lru_get(str *temp_gruu)
{
	int i;
	time_t now;

	if(!temp_gruu || !temp_gruu->s || temp_gruu->len <= 0
			|| !pcscf_temp_gruu_lru)
		return NULL;

	now = time(NULL);
	for(i = 0; i < pcscf_temp_gruu_lru_capacity; i++) {
		if(pcscf_temp_gruu_lru[i].temp_gruu.s == NULL)
			continue;
		if(pcscf_temp_gruu_lru[i].expires > 0
				&& pcscf_temp_gruu_lru[i].expires < now)
			continue;
		if(pcscf_temp_gruu_lru[i].temp_gruu.len == temp_gruu->len
				&& strncasecmp(pcscf_temp_gruu_lru[i].temp_gruu.s, temp_gruu->s,
						   temp_gruu->len)
						   == 0) {
			return pcscf_temp_gruu_lru[i].contact;
		}
	}

	return NULL;
}

int pcscf_index_init(pcscf_index_t *idx)
{
	if(!idx)
		return -1;
	memset(idx->table, 0, sizeof(idx->table));
	idx->count = 0;
	return 0;
}

static unsigned int pcscf_index_hash(str *key)
{
	if(!key || !key->s)
		return 0;
	return (unsigned int)(get_hash1_case_raw(key->s, key->len)
						  % PCSCF_INDEX_SIZE);
}

void pcscf_index_destroy(pcscf_index_t *idx)
{
	int i;
	pcscf_index_entry_t *e, *tmp;
	if(!idx)
		return;
	for(i = 0; i < PCSCF_INDEX_SIZE; i++) {
		e = idx->table[i];
		while(e) {
			tmp = e->next;
			if(e->key.s)
				PCSCF_IDX_FREE(e->key.s);
			PCSCF_IDX_FREE(e);
			e = tmp;
		}
		idx->table[i] = NULL;
	}
	idx->count = 0;
}

int pcscf_index_add(pcscf_index_t *idx, str *key, pcontact_t *c)
{
	unsigned int h;
	pcscf_index_entry_t *e;
	char *ks;
	if(!idx || !key || !key->s)
		return -1;
	e = (pcscf_index_entry_t *)PCSCF_IDX_MALLOC(sizeof(pcscf_index_entry_t));
	if(!e)
		return -1;
	memset(e, 0, sizeof(*e));
	ks = (char *)PCSCF_IDX_MALLOC(key->len + 1);
	if(!ks) {
		PCSCF_IDX_FREE(e);
		return -1;
	}
	memcpy(ks, key->s, key->len);
	ks[key->len] = '\0';
	e->key.s = ks;
	e->key.len = key->len;
	e->contact = c;
	h = pcscf_index_hash(key);
	/* insert at head */
	e->next = idx->table[h];
	if(e->next)
		e->next->prev = e;
	idx->table[h] = e;
	idx->count++;
	return 0;
}

pcscf_index_entry_t *pcscf_index_get(pcscf_index_t *idx, str *key)
{
	unsigned int h;
	pcscf_index_entry_t *e;
	if(!idx || !key || !key->s)
		return NULL;
	h = pcscf_index_hash(key);
	e = idx->table[h];
	while(e) {
		if((e->key.len == key->len)
				&& (strncasecmp(e->key.s, key->s, key->len) == 0)) {
			return e;
		}
		e = e->next;
	}
	return NULL;
}

int pcscf_index_remove_key(pcscf_index_t *idx, str *key)
{
	pcscf_index_entry_t *e;
	if(!idx || !key || !key->s)
		return -1;
	e = pcscf_index_get(idx, key);
	if(!e)
		return -1;
	/* unlink */
	if(e->prev)
		e->prev->next = e->next;
	else {
		/* head of bucket */
		unsigned int h = pcscf_index_hash(key);
		idx->table[h] = e->next;
	}
	if(e->next)
		e->next->prev = e->prev;
	if(e->key.s)
		PCSCF_IDX_FREE(e->key.s);
	PCSCF_IDX_FREE(e);
	idx->count--;
	return 0;
}

int pcscf_index_replace(
		pcscf_index_t *idx, str *old_key, str *new_key, pcontact_t *c)
{
	pcscf_index_entry_t *e;
	if(!idx || !new_key || !new_key->s)
		return -1;
	if(old_key) {
		e = pcscf_index_get(idx, old_key);
		if(e) {
			/* replace key content */
			if(e->key.s)
				PCSCF_IDX_FREE(e->key.s);
			e->key.s = (char *)PCSCF_IDX_MALLOC(new_key->len + 1);
			if(!e->key.s)
				return -1;
			memcpy(e->key.s, new_key->s, new_key->len);
			e->key.s[new_key->len] = '\0';
			e->key.len = new_key->len;
			e->contact = c;
			return 0;
		}
	}
	/* not found - add new */
	return pcscf_index_add(idx, new_key, c);
}

int pcscf_index_remove_contact(pcscf_index_t *idx, pcontact_t *c)
{
	int i;
	pcscf_index_entry_t *e, *tmp;
	int removed = 0;
	if(!idx || !c)
		return -1;
	for(i = 0; i < PCSCF_INDEX_SIZE; i++) {
		e = idx->table[i];
		while(e) {
			tmp = e->next;
			if(e->contact == c) {
				/* unlink */
				if(e->prev)
					e->prev->next = e->next;
				else
					idx->table[i] = e->next;
				if(e->next)
					e->next->prev = e->prev;
				if(e->key.s)
					PCSCF_IDX_FREE(e->key.s);
				PCSCF_IDX_FREE(e);
				idx->count--;
				removed++;
			}
			e = tmp;
		}
	}
	return (removed > 0) ? 0 : -1;
}

#ifndef UNIT_TEST
int pcscf_index_sync_contact(udomain_t *d, pcontact_t *c)
{
	ppublic_t *p;

	if(!d || !c)
		return -1;

	/* Remove all existing index entries for this contact, then re-add */
	pcscf_index_remove_contact(&d->impu_idx, c);
	pcscf_index_remove_contact(&d->pub_gruu_idx, c);
	pcscf_index_remove_contact(&d->temp_gruu_idx, c);

	if(c->aor.len && c->aor.s) {
		if(pcscf_index_add(&d->impu_idx, &c->aor, c) < 0)
			return -1;
	}
	for(p = c->head; p; p = p->next) {
		if(p->public_identity.len && p->public_identity.s) {
			if(pcscf_index_add(&d->impu_idx, &p->public_identity, c) < 0)
				return -1;
		}
	}
	if(c->pub_gruu.len && c->pub_gruu.s) {
		if(pcscf_index_add(&d->pub_gruu_idx, &c->pub_gruu, c) < 0)
			return -1;
	}
	if(c->temp_gruu.len && c->temp_gruu.s) {
		if(pcscf_index_add(&d->temp_gruu_idx, &c->temp_gruu, c) < 0)
			return -1;
	}
	return 0;
}
#endif
