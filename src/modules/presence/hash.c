/*
 * presence module - presence server implementation
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*! \file
 * \brief Kamailio presence module
 * \ingroup presence 
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../../core/mem/shm_mem.h"
#include "../../core/hashes.h"
#include "../../core/dprint.h"
#include "../../core/str.h"
#include "../pua/hash.h"
#include "presence.h"
#include "hash.h"
#include "notify.h"

/* matching mode when removing subscriptions from memory */
extern int pres_subs_remove_match;

/**
 * create the subscription hash table in shared memory
 * - hash_size: number of slots
 */
shtable_t new_shtable(int hash_size)
{
	shtable_t htable = NULL;
	int i, j;

	i = 0;
	htable = (subs_entry_t *)shm_malloc(hash_size * sizeof(subs_entry_t));
	if(htable == NULL) {
		ERR_MEM(SHARE_MEM);
	}
	memset(htable, 0, hash_size * sizeof(subs_entry_t));
	for(i = 0; i < hash_size; i++) {
		if(lock_init(&htable[i].lock) == 0) {
			LM_ERR("initializing lock [%d]\n", i);
			goto error;
		}
		htable[i].entries = (subs_t *)shm_malloc(sizeof(subs_t));
		if(htable[i].entries == NULL) {
			lock_destroy(&htable[i].lock);
			ERR_MEM(SHARE_MEM);
		}
		memset(htable[i].entries, 0, sizeof(subs_t));
		htable[i].entries->next = NULL;
	}

	return htable;

error:
	if(htable) {
		for(j = 0; j < i; j++) {
			lock_destroy(&htable[j].lock);
			shm_free(htable[j].entries);
		}
		shm_free(htable);
	}
	return NULL;
}

void destroy_shtable(shtable_t htable, int hash_size)
{
	int i;

	if(htable == NULL)
		return;

	for(i = 0; i < hash_size; i++) {
		lock_destroy(&htable[i].lock);
		free_subs_list(htable[i].entries->next, SHM_MEM_TYPE, 1);
		shm_free(htable[i].entries);
		htable[i].entries = NULL;
	}
	shm_free(htable);
	htable = NULL;
}

subs_t *search_shtable(shtable_t htable, str callid, str to_tag, str from_tag,
		unsigned int hash_code)
{
	subs_t *s;

	s = htable[hash_code].entries ? htable[hash_code].entries->next : NULL;

	while(s) {
		if(s->callid.len == callid.len
				&& strncmp(s->callid.s, callid.s, callid.len) == 0
				&& s->to_tag.len == to_tag.len
				&& strncmp(s->to_tag.s, to_tag.s, to_tag.len) == 0
				&& s->from_tag.len == from_tag.len
				&& strncmp(s->from_tag.s, from_tag.s, from_tag.len) == 0)
			return s;
		s = s->next;
	}

	return NULL;
}

subs_t *mem_copy_subs(subs_t *s, int mem_type)
{
	int size;
	subs_t *dest;

	size = sizeof(subs_t)
		   + (s->pres_uri.len + s->to_user.len + s->to_domain.len
					 + s->from_user.len + s->from_domain.len + s->callid.len
					 + s->to_tag.len + s->from_tag.len + s->sockinfo_str.len
					 + s->event_id.len + s->local_contact.len + s->contact.len
					 + s->record_route.len + s->reason.len + s->watcher_user.len
					 + s->watcher_domain.len + s->user_agent.len + 1)
					 * sizeof(char);

	if(mem_type & PKG_MEM_TYPE)
		dest = (subs_t *)pkg_malloc(size);
	else
		dest = (subs_t *)shm_malloc(size);

	if(dest == NULL) {
		ERR_MEM((mem_type == PKG_MEM_TYPE) ? PKG_MEM_STR : SHARE_MEM);
	}
	memset(dest, 0, size);
	size = sizeof(subs_t);

	CONT_COPY(dest, dest->pres_uri, s->pres_uri);
	CONT_COPY(dest, dest->to_user, s->to_user);
	CONT_COPY(dest, dest->to_domain, s->to_domain);
	CONT_COPY(dest, dest->from_user, s->from_user);
	CONT_COPY(dest, dest->from_domain, s->from_domain);
	CONT_COPY(dest, dest->watcher_user, s->watcher_user);
	CONT_COPY(dest, dest->watcher_domain, s->watcher_domain);
	CONT_COPY(dest, dest->to_tag, s->to_tag);
	CONT_COPY(dest, dest->from_tag, s->from_tag);
	CONT_COPY(dest, dest->callid, s->callid);
	CONT_COPY(dest, dest->sockinfo_str, s->sockinfo_str);
	CONT_COPY(dest, dest->local_contact, s->local_contact);
	CONT_COPY(dest, dest->contact, s->contact);
	CONT_COPY(dest, dest->record_route, s->record_route);
	CONT_COPY(dest, dest->user_agent, s->user_agent);
	if(s->event_id.s)
		CONT_COPY(dest, dest->event_id, s->event_id);
	if(s->reason.s)
		CONT_COPY(dest, dest->reason, s->reason);

	dest->event = s->event;
	dest->local_cseq = s->local_cseq;
	dest->remote_cseq = s->remote_cseq;
	dest->status = s->status;
	dest->version = s->version;
	dest->send_on_cback = s->send_on_cback;
	dest->expires = s->expires;
	dest->db_flag = s->db_flag;
	dest->flags = s->flags;

	return dest;

error:
	return NULL;
}


subs_t *mem_copy_subs_noc(subs_t *s)
{
	int size;
	subs_t *dest;

	size = sizeof(subs_t)
		   + (s->pres_uri.len + s->to_user.len + s->to_domain.len
					 + s->from_user.len + s->from_domain.len + s->callid.len
					 + s->to_tag.len + s->from_tag.len + s->sockinfo_str.len
					 + s->event_id.len + s->local_contact.len + s->reason.len
					 + s->watcher_user.len + s->watcher_domain.len
					 + s->user_agent.len + 1)
					 * sizeof(char);

	dest = (subs_t *)shm_malloc(size);
	if(dest == NULL) {
		ERR_MEM(SHARE_MEM);
	}
	memset(dest, 0, size);
	size = sizeof(subs_t);

	CONT_COPY(dest, dest->pres_uri, s->pres_uri);
	CONT_COPY(dest, dest->to_user, s->to_user);
	CONT_COPY(dest, dest->to_domain, s->to_domain);
	CONT_COPY(dest, dest->from_user, s->from_user);
	CONT_COPY(dest, dest->from_domain, s->from_domain);
	CONT_COPY(dest, dest->watcher_user, s->watcher_user);
	CONT_COPY(dest, dest->watcher_domain, s->watcher_domain);
	CONT_COPY(dest, dest->to_tag, s->to_tag);
	CONT_COPY(dest, dest->from_tag, s->from_tag);
	CONT_COPY(dest, dest->callid, s->callid);
	CONT_COPY(dest, dest->sockinfo_str, s->sockinfo_str);
	CONT_COPY(dest, dest->local_contact, s->local_contact);
	CONT_COPY(dest, dest->user_agent, s->user_agent);
	if(s->event_id.s)
		CONT_COPY(dest, dest->event_id, s->event_id);
	if(s->reason.s)
		CONT_COPY(dest, dest->reason, s->reason);

	dest->event = s->event;
	dest->local_cseq = s->local_cseq;
	dest->remote_cseq = s->remote_cseq;
	dest->status = s->status;
	dest->version = s->version;
	dest->send_on_cback = s->send_on_cback;
	dest->expires = s->expires;
	dest->db_flag = s->db_flag;
	dest->flags = s->flags;

	dest->contact.s = (char *)shm_malloc(s->contact.len * sizeof(char));
	if(dest->contact.s == NULL) {
		ERR_MEM(SHARE_MEM);
	}
	memcpy(dest->contact.s, s->contact.s, s->contact.len);
	dest->contact.len = s->contact.len;

	dest->record_route.s =
			(char *)shm_malloc((s->record_route.len + 1) * sizeof(char));
	if(dest->record_route.s == NULL) {
		ERR_MEM(SHARE_MEM);
	}
	memcpy(dest->record_route.s, s->record_route.s, s->record_route.len);
	dest->record_route.len = s->record_route.len;

	return dest;

error:
	if(dest)
		shm_free(dest);
	return NULL;
}

int insert_shtable(shtable_t htable, unsigned int hash_code, subs_t *subs)
{
	subs_t *new_rec = NULL;

	if(pres_delete_same_subs) {
		subs_t *rec = NULL, *prev_rec = NULL;

		lock_get(&htable[hash_code].lock);
		/* search if there is another record with the same pres_uri & callid */
		rec = htable[hash_code].entries->next;
		while(rec) {
			if(subs->pres_uri.len == rec->pres_uri.len
					&& subs->callid.len == rec->callid.len
					&& memcmp(subs->pres_uri.s, rec->pres_uri.s,
							   subs->pres_uri.len)
							   == 0
					&& memcmp(subs->callid.s, rec->callid.s, subs->callid.len)
							   == 0) {
				LM_NOTICE("Found another record with the same pres_uri[%.*s] "
						  "and callid[%.*s]\n",
						subs->pres_uri.len, subs->pres_uri.s, subs->callid.len,
						subs->callid.s);
				/* delete this record */

				if(prev_rec) {
					prev_rec->next = rec->next;
				} else {
					htable[hash_code].entries->next = rec->next;
				}

				if(pres_subs_dbmode != NO_DB) {
					delete_db_subs(&rec->to_tag, &rec->from_tag, &rec->callid);
				}

				if(rec->contact.s != NULL) {
					shm_free(rec->contact.s);
				}

				shm_free(rec);
				break;
			}
			prev_rec = rec;
			rec = rec->next;
		}
		lock_release(&htable[hash_code].lock);
	}

	new_rec = mem_copy_subs_noc(subs);
	if(new_rec == NULL) {
		LM_ERR("copying in share memory a subs_t structure\n");
		return -1;
	}
	new_rec->expires += (int)time(NULL);

	lock_get(&htable[hash_code].lock);
	new_rec->next = htable[hash_code].entries->next;
	htable[hash_code].entries->next = new_rec;
	lock_release(&htable[hash_code].lock);

	return 0;
}

int delete_shtable(shtable_t htable, unsigned int hash_code, subs_t *subs)
{
	subs_t *s = NULL, *ps = NULL;
	int found = -1;

	lock_get(&htable[hash_code].lock);

	ps = htable[hash_code].entries;
	s = ps ? ps->next : NULL;

	while(s) {
		if(pres_subs_remove_match == 0) {
			/* match on to-tag only (unique, local generated - faster) */
			if(s->to_tag.len == subs->to_tag.len
					&& strncmp(s->to_tag.s, subs->to_tag.s, subs->to_tag.len)
							   == 0) {
				found = 0;
			}
		} else {
			/* match on all dialog attributes (distributed systems) */
			if(s->callid.len == subs->callid.len
					&& s->to_tag.len == subs->to_tag.len
					&& s->from_tag.len == subs->from_tag.len
					&& strncmp(s->callid.s, subs->callid.s, subs->callid.len)
							   == 0
					&& strncmp(s->to_tag.s, subs->to_tag.s, subs->to_tag.len)
							   == 0
					&& strncmp(s->from_tag.s, subs->from_tag.s,
							   subs->from_tag.len)
							   == 0) {
				found = 0;
			}
		}
		if(found == 0) {
			found = s->local_cseq + 1;
			ps->next = s->next;
			if(s->contact.s != NULL) {
				shm_free(s->contact.s);
				s->contact.s = NULL;
			}
			if(s->record_route.s != NULL) {
				shm_free(s->record_route.s);
				s->record_route.s = NULL;
			}
			if(s) {
				shm_free(s);
				s = NULL;
			}
			break;
		}
		ps = s;
		s = s->next;
	}
	lock_release(&htable[hash_code].lock);
	return found;
}

void free_subs_list(subs_t *s_array, int mem_type, int ic)
{
	subs_t *s;

	while(s_array) {
		s = s_array;
		s_array = s_array->next;
		if(mem_type & PKG_MEM_TYPE) {
			if(ic) {
				pkg_free(s->contact.s);
				s->contact.s = NULL;
			}
			pkg_free(s);
			s = NULL;
		} else {
			if(ic) {
				shm_free(s->contact.s);
				s->contact.s = NULL;
			}
			shm_free(s);
			s = NULL;
		}
	}
}

int update_shtable(
		shtable_t htable, unsigned int hash_code, subs_t *subs, int type)
{
	subs_t *s;

	lock_get(&htable[hash_code].lock);

	s = search_shtable(
			htable, subs->callid, subs->to_tag, subs->from_tag, hash_code);
	if(s == NULL) {
		LM_DBG("record not found in hash table\n");
		lock_release(&htable[hash_code].lock);
		return -1;
	}

	if(type & REMOTE_TYPE) {
		s->expires = subs->expires + (int)time(NULL);
		s->remote_cseq = subs->remote_cseq;
	} else {
		subs->local_cseq = ++s->local_cseq;
		subs->version = ++s->version;
	}

	if(presence_sip_uri_match(&s->contact, &subs->contact)) {
		shm_free(s->contact.s);
		s->contact.s = (char *)shm_malloc(subs->contact.len * sizeof(char));
		if(s->contact.s == NULL) {
			lock_release(&htable[hash_code].lock);
			LM_ERR("no more shared memory\n");
			return -1;
		}
		memcpy(s->contact.s, subs->contact.s, subs->contact.len);
		s->contact.len = subs->contact.len;
	}

	shm_free(s->record_route.s);
	s->record_route.s =
			(char *)shm_malloc(subs->record_route.len * sizeof(char));
	if(s->record_route.s == NULL) {
		lock_release(&htable[hash_code].lock);
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memcpy(s->record_route.s, subs->record_route.s, subs->record_route.len);
	s->record_route.len = subs->record_route.len;

	s->status = subs->status;
	s->event = subs->event;
	subs->db_flag = s->db_flag;

	if(s->db_flag & NO_UPDATEDB_FLAG)
		s->db_flag = UPDATEDB_FLAG;

	lock_release(&htable[hash_code].lock);

	return 0;
}

phtable_t *new_phtable(void)
{
	phtable_t *htable = NULL;
	int i, j;

	i = 0;
	htable = (phtable_t *)shm_malloc(phtable_size * sizeof(phtable_t));
	if(htable == NULL) {
		ERR_MEM(SHARE_MEM);
	}
	memset(htable, 0, phtable_size * sizeof(phtable_t));

	for(i = 0; i < phtable_size; i++) {
		if(lock_init(&htable[i].lock) == 0) {
			LM_ERR("initializing lock [%d]\n", i);
			goto error;
		}
		htable[i].entries = (pres_entry_t *)shm_malloc(sizeof(pres_entry_t));
		if(htable[i].entries == NULL) {
			ERR_MEM(SHARE_MEM);
		}
		memset(htable[i].entries, 0, sizeof(pres_entry_t));
		htable[i].entries->next = NULL;
	}

	return htable;

error:
	if(htable) {
		for(j = 0; j < i; j++) {
			if(htable[i].entries)
				shm_free(htable[i].entries);
			else
				break;
			lock_destroy(&htable[i].lock);
		}
		shm_free(htable);
	}
	return NULL;
}

void destroy_phtable(void)
{
	int i;
	pres_entry_t *p, *prev_p;

	if(pres_htable == NULL)
		return;

	for(i = 0; i < phtable_size; i++) {
		lock_destroy(&pres_htable[i].lock);
		p = pres_htable[i].entries;
		while(p) {
			prev_p = p;
			p = p->next;
			if(prev_p->sphere)
				shm_free(prev_p->sphere);
			shm_free(prev_p);
		}
	}
	shm_free(pres_htable);
}
/* entry must be locked before calling this function */

pres_entry_t *search_phtable(str *pres_uri, int event, unsigned int hash_code)
{
	pres_entry_t *p;

	LM_DBG("pres_uri= %.*s\n", pres_uri->len, pres_uri->s);
	p = pres_htable[hash_code].entries->next;
	while(p) {
		if(p->event == event && p->pres_uri.len == pres_uri->len
				&& presence_sip_uri_match(&p->pres_uri, pres_uri) == 0)
			return p;
		p = p->next;
	}
	return NULL;
}

int insert_phtable(str *pres_uri, int event, char *sphere)
{
	unsigned int hash_code;
	pres_entry_t *p = NULL;
	int size;

	hash_code = core_case_hash(pres_uri, NULL, phtable_size);

	lock_get(&pres_htable[hash_code].lock);

	p = search_phtable(pres_uri, event, hash_code);
	if(p) {
		p->publ_count++;
		lock_release(&pres_htable[hash_code].lock);
		return 0;
	}
	size = sizeof(pres_entry_t) + pres_uri->len * sizeof(char);

	p = (pres_entry_t *)shm_malloc(size);
	if(p == NULL) {
		lock_release(&pres_htable[hash_code].lock);
		ERR_MEM(SHARE_MEM);
	}
	memset(p, 0, size);

	size = sizeof(pres_entry_t);
	p->pres_uri.s = (char *)p + size;
	memcpy(p->pres_uri.s, pres_uri->s, pres_uri->len);
	p->pres_uri.len = pres_uri->len;

	if(sphere) {
		p->sphere = (char *)shm_malloc((strlen(sphere) + 1) * sizeof(char));
		if(p->sphere == NULL) {
			lock_release(&pres_htable[hash_code].lock);
			shm_free(p);
			ERR_MEM(SHARE_MEM);
		}
		strcpy(p->sphere, sphere);
	}

	p->event = event;
	p->publ_count = 1;

	/* link the item in the hash table */
	p->next = pres_htable[hash_code].entries->next;
	pres_htable[hash_code].entries->next = p;

	lock_release(&pres_htable[hash_code].lock);

	return 0;

error:
	return -1;
}

int delete_phtable(str *pres_uri, int event)
{
	unsigned int hash_code;
	pres_entry_t *p = NULL, *prev_p = NULL;

	hash_code = core_case_hash(pres_uri, NULL, phtable_size);

	lock_get(&pres_htable[hash_code].lock);

	p = search_phtable(pres_uri, event, hash_code);
	if(p == NULL) {
		LM_DBG("record not found\n");
		lock_release(&pres_htable[hash_code].lock);
		return 0;
	}

	p->publ_count--;
	if(p->publ_count == 0) {
		/* delete record */
		prev_p = pres_htable[hash_code].entries;
		while(prev_p->next) {
			if(prev_p->next == p)
				break;
			prev_p = prev_p->next;
		}
		if(prev_p->next == NULL) {
			LM_ERR("record not found\n");
			lock_release(&pres_htable[hash_code].lock);
			return -1;
		}
		prev_p->next = p->next;
		if(p->sphere)
			shm_free(p->sphere);

		shm_free(p);
	}
	lock_release(&pres_htable[hash_code].lock);

	return 0;
}

int update_phtable(presentity_t *presentity, str *pres_uri, str *body)
{
	char *sphere = NULL;
	unsigned int hash_code;
	pres_entry_t *p;
	int ret = 0;
	str *xcap_doc = NULL;

	/* get new sphere */
	sphere = extract_sphere(body);
	if(sphere == NULL) {
		LM_DBG("no sphere defined in new body\n");
		return 0;
	}

	/* search for record in hash table */
	hash_code = core_case_hash(pres_uri, NULL, phtable_size);

	lock_get(&pres_htable[hash_code].lock);

	p = search_phtable(pres_uri, presentity->event->evp->type, hash_code);
	if(p == NULL) {
		lock_release(&pres_htable[hash_code].lock);
		goto done;
	}

	if(p->sphere) {
		if(strcmp(p->sphere, sphere) != 0) {
			/* new sphere definition */
			shm_free(p->sphere);
		} else {
			/* no change in sphere definition */
			lock_release(&pres_htable[hash_code].lock);
			pkg_free(sphere);
			return 0;
		}
	}


	p->sphere = (char *)shm_malloc((strlen(sphere) + 1) * sizeof(char));
	if(p->sphere == NULL) {
		lock_release(&pres_htable[hash_code].lock);
		ret = -1;
		goto done;
	}
	strcpy(p->sphere, sphere);

	lock_release(&pres_htable[hash_code].lock);

	/* call for watchers status update */

	if(presentity->event->get_rules_doc(
			   &presentity->user, &presentity->domain, &xcap_doc)
			< 0) {
		LM_ERR("failed to retrieve xcap document\n");
		ret = -1;
		goto done;
	}

	update_watchers_status(pres_uri, presentity->event, xcap_doc);


done:

	if(xcap_doc) {
		if(xcap_doc->s)
			pkg_free(xcap_doc->s);
		pkg_free(xcap_doc);
	}

	if(sphere)
		pkg_free(sphere);
	return ret;
}

/**
 * ==============================
 *  in-memory presentity records
 * ==============================
 */

static ps_ptable_t *_ps_ptable = NULL;

ps_ptable_t *ps_ptable_get(void)
{
	return _ps_ptable;
}

#define PS_PRESENTITY_FIELD_COPY(field)                       \
	do {                                                      \
		if(pt->field.s) {                                     \
			ptn->field.s = p;                                 \
			memcpy(ptn->field.s, pt->field.s, pt->field.len); \
		}                                                     \
		ptn->field.len = pt->field.len;                       \
		p += pt->field.len + 1;                               \
	} while(0)

/**
 *
 */
ps_presentity_t *ps_presentity_new(ps_presentity_t *pt, int mtype)
{
	uint32_t bsize = 0;
	ps_presentity_t *ptn = NULL;
	char *p = NULL;

	if(pt == NULL) {
		return NULL;
	}
	bsize = sizeof(ps_presentity_t) + pt->user.len + 1 + pt->domain.len + 1
			+ pt->etag.len + 1 + pt->event.len + 1 + pt->ruid.len + 1
			+ pt->sender.len + 1 + pt->body.len + 1;
	if(mtype == 0) {
		ptn = (ps_presentity_t *)shm_malloc(bsize);
	} else {
		ptn = (ps_presentity_t *)pkg_malloc(bsize);
	}
	if(ptn == NULL) {
		if(mtype == 0) {
			SHM_MEM_ERROR;
		} else {
			PKG_MEM_ERROR;
		}
		return NULL;
	}
	memset(ptn, 0, bsize);

	ptn->bsize = bsize;
	ptn->hashid = core_case_hash(&pt->user, &pt->domain, 0);
	ptn->expires = pt->expires;
	ptn->received_time = pt->received_time;
	ptn->priority = pt->priority;

	p = (char *)ptn + sizeof(ps_presentity_t);
	PS_PRESENTITY_FIELD_COPY(user);
	PS_PRESENTITY_FIELD_COPY(domain);
	PS_PRESENTITY_FIELD_COPY(etag);
	PS_PRESENTITY_FIELD_COPY(event);
	PS_PRESENTITY_FIELD_COPY(ruid);
	PS_PRESENTITY_FIELD_COPY(sender);
	PS_PRESENTITY_FIELD_COPY(body);

	return ptn;
}

/**
 *
 */
void ps_presentity_free(ps_presentity_t *pt, int mtype)
{
	if(pt == NULL) {
		return;
	}
	if(mtype == 0) {
		shm_free(pt);
	} else {
		pkg_free(pt);
	}
}

/**
 *
 */
void ps_presentity_list_free(ps_presentity_t *pt, int mtype)
{
	ps_presentity_t *ptc = NULL;
	ps_presentity_t *ptn = NULL;

	if(pt == NULL) {
		return;
	}

	ptn = pt;
	while(ptn != NULL) {
		ptc = ptn;
		ptn = ptn->next;
		ps_presentity_free(ptc, mtype);
	}
}

#define PS_PRESENTITY_FIELD_SHIFT(field) \
	do {                                 \
		if(pt->field.s) {                \
			ptn->field.s = p;            \
		}                                \
		p += pt->field.len + 1;          \
	} while(0)

/**
 *
 */
ps_presentity_t *ps_presentity_dup(ps_presentity_t *pt, int mtype)
{
	ps_presentity_t *ptn = NULL;
	char *p = NULL;

	if(pt == NULL) {
		return NULL;
	}
	if(mtype == 0) {
		ptn = (ps_presentity_t *)shm_malloc(pt->bsize);
	} else {
		ptn = (ps_presentity_t *)pkg_malloc(pt->bsize);
	}
	if(ptn == NULL) {
		if(mtype == 0) {
			SHM_MEM_ERROR;
		} else {
			PKG_MEM_ERROR;
		}
		return NULL;
	}

	memcpy((void *)ptn, pt, pt->bsize);

	p = (char *)ptn + sizeof(ps_presentity_t);
	PS_PRESENTITY_FIELD_SHIFT(user);
	PS_PRESENTITY_FIELD_SHIFT(domain);
	PS_PRESENTITY_FIELD_SHIFT(etag);
	PS_PRESENTITY_FIELD_SHIFT(event);
	PS_PRESENTITY_FIELD_SHIFT(ruid);
	PS_PRESENTITY_FIELD_SHIFT(sender);
	PS_PRESENTITY_FIELD_SHIFT(body);

	ptn->next = NULL;
	ptn->prev = NULL;

	return ptn;
}

/**
 * match presentity with various conditions
 *   0 - only user and domain
 *   1 - match also event
 *   2 - match also etag
 */
int ps_presentity_match(ps_presentity_t *pta, ps_presentity_t *ptb, int mmode)
{
	if(pta->hashid != ptb->hashid) {
		return 0;
	}

	if(pta->user.len != ptb->user.len || pta->domain.len != ptb->domain.len) {
		return 0;
	}

	if(mmode > 0) {
		if(pta->event.len != ptb->event.len) {
			return 0;
		}
	}

	if(mmode > 1) {
		if(pta->etag.len != ptb->etag.len) {
			return 0;
		}
	}

	if(strncmp(pta->user.s, ptb->user.s, pta->user.len) != 0) {
		return 0;
	}

	if(strncmp(pta->domain.s, ptb->domain.s, pta->domain.len) != 0) {
		return 0;
	}

	if(mmode > 0) {
		if(strncmp(pta->event.s, ptb->event.s, pta->event.len) != 0) {
			return 0;
		}
	}

	if(mmode > 1) {
		if(strncmp(pta->etag.s, ptb->etag.s, pta->etag.len) != 0) {
			return 0;
		}
	}
	return 1;
}

/**
 *
 */
int ps_ptable_init(int ssize)
{
	size_t tsize = 0;
	int i = 0;

	if(_ps_ptable != NULL) {
		return 0;
	}
	tsize = sizeof(ps_ptable_t) + (ssize * sizeof(ps_pslot_t));
	_ps_ptable = (ps_ptable_t *)shm_malloc(tsize);
	if(_ps_ptable == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(_ps_ptable, 0, tsize);
	_ps_ptable->ssize = ssize;
	_ps_ptable->slots =
			(ps_pslot_t *)((char *)_ps_ptable + sizeof(ps_ptable_t));
	for(i = 0; i < ssize; i++) {
		if(lock_init(&_ps_ptable->slots[i].lock) == 0) {
			LM_ERR("initializing lock on slot [%d]\n", i);
			goto error;
		}
	}

	return 0;

error:
	i--;
	while(i >= 0) {
		lock_destroy(&_ps_ptable->slots[i].lock);
		i--;
	}
	shm_free(_ps_ptable);
	_ps_ptable = NULL;
	return -1;
}

/**
 *
 */
void ps_ptable_destroy(void)
{
	int i = 0;
	ps_presentity_t *pt = NULL;
	ps_presentity_t *ptn = NULL;

	if(_ps_ptable == NULL) {
		return;
	}
	for(i = 0; i < _ps_ptable->ssize; i++) {
		lock_destroy(&_ps_ptable->slots[i].lock);
		pt = _ps_ptable->slots[i].plist;
		while(pt != NULL) {
			ptn = pt->next;
			ps_presentity_free(pt, 0);
			pt = ptn;
		}
	}
	shm_free(_ps_ptable);
	_ps_ptable = NULL;
	return;
}

/**
 *
 */
int ps_ptable_insert(ps_presentity_t *pt)
{
	ps_presentity_t ptc;
	ps_presentity_t *ptn = NULL;
	uint32_t idx = 0;

	/* copy struct to fill in missing fields */
	memcpy(&ptc, pt, sizeof(ps_presentity_t));

	ptc.hashid = core_case_hash(&pt->user, &pt->domain, 0);

	if(ptc.ruid.s == NULL) {
		if(sruid_next(&pres_sruid) < 0) {
			return -1;
		}
		ptc.ruid = pres_sruid.uid;
	}

	ptn = ps_presentity_new(&ptc, 0);
	if(ptn == NULL) {
		return -1;
	}

	idx = core_hash_idx(ptn->hashid, _ps_ptable->ssize);

	lock_get(&_ps_ptable->slots[idx].lock);
	if(_ps_ptable->slots[idx].plist == NULL) {
		_ps_ptable->slots[idx].plist = ptn;
	} else {
		_ps_ptable->slots[idx].plist->prev = ptn;
		ptn->next = _ps_ptable->slots[idx].plist;
		_ps_ptable->slots[idx].plist = ptn;
	}
	lock_release(&_ps_ptable->slots[idx].lock);

	return 0;
}

/**
 *
 */
int ps_ptable_replace(ps_presentity_t *ptm, ps_presentity_t *pt)
{
	ps_presentity_t ptc;
	ps_presentity_t ptv;
	ps_presentity_t *ptn = NULL;
	uint32_t idx = 0;

	/* copy struct to fill in missing fields */
	memcpy(&ptc, ptm, sizeof(ps_presentity_t));
	memcpy(&ptv, pt, sizeof(ps_presentity_t));

	ptc.hashid = core_case_hash(&pt->user, &pt->domain, 0);
	ptv.hashid = core_case_hash(&pt->user, &pt->domain, 0);

	if(ptv.ruid.s == NULL) {
		if(sruid_next(&pres_sruid) < 0) {
			return -1;
		}
		ptv.ruid = pres_sruid.uid;
	}

	idx = core_hash_idx(ptc.hashid, _ps_ptable->ssize);

	lock_get(&_ps_ptable->slots[idx].lock);
	ptn = _ps_ptable->slots[idx].plist;
	while(ptn != NULL) {
		if(ps_presentity_match(ptn, &ptc, 2) == 1) {
			if(ptn->next) {
				ptn->next->prev = ptn->prev;
			}
			if(ptn->prev) {
				ptn->prev->next = ptn->next;
			} else {
				_ps_ptable->slots[idx].plist = ptn->next;
			}
			break;
		}
		ptn = ptn->next;
	}

	if(ptn != NULL) {
		ps_presentity_free(ptn, 0);
	}

	ptn = ps_presentity_new(&ptv, 0);
	if(ptn == NULL) {
		lock_release(&_ps_ptable->slots[idx].lock);
		return -1;
	}

	if(_ps_ptable->slots[idx].plist == NULL) {
		_ps_ptable->slots[idx].plist = ptn;
	} else {
		_ps_ptable->slots[idx].plist->prev = ptn;
		ptn->next = _ps_ptable->slots[idx].plist;
		_ps_ptable->slots[idx].plist = ptn;
	}
	lock_release(&_ps_ptable->slots[idx].lock);

	return 0;
}

/**
 *
 */
int ps_ptable_update(ps_presentity_t *ptm, ps_presentity_t *pt)
{
	ps_presentity_t ptc;
	ps_presentity_t ptv;
	ps_presentity_t *ptn = NULL;
	uint32_t idx = 0;

	/* copy struct to fill in missing fields */
	memcpy(&ptc, ptm, sizeof(ps_presentity_t));
	memcpy(&ptv, pt, sizeof(ps_presentity_t));

	ptc.hashid = core_case_hash(&ptm->user, &ptm->domain, 0);
	ptv.hashid = core_case_hash(&pt->user, &pt->domain, 0);

	if(ptv.ruid.s == NULL) {
		if(sruid_next(&pres_sruid) < 0) {
			return -1;
		}
		ptv.ruid = pres_sruid.uid;
	}

	idx = core_hash_idx(ptc.hashid, _ps_ptable->ssize);

	lock_get(&_ps_ptable->slots[idx].lock);
	ptn = _ps_ptable->slots[idx].plist;
	while(ptn != NULL) {
		if(ps_presentity_match(ptn, &ptc, 2) == 1) {
			if(ptn->next) {
				ptn->next->prev = ptn->prev;
			}
			if(ptn->prev) {
				ptn->prev->next = ptn->next;
			} else {
				_ps_ptable->slots[idx].plist = ptn->next;
			}
			break;
		}
		ptn = ptn->next;
	}

	if(ptn == NULL) {
		lock_release(&_ps_ptable->slots[idx].lock);
		return 0; /* affected items */
	}
	ps_presentity_free(ptn, 0);

	ptn = ps_presentity_new(&ptv, 0);
	if(ptn == NULL) {
		lock_release(&_ps_ptable->slots[idx].lock);
		return -1;
	}

	if(_ps_ptable->slots[idx].plist == NULL) {
		_ps_ptable->slots[idx].plist = ptn;
	} else {
		_ps_ptable->slots[idx].plist->prev = ptn;
		ptn->next = _ps_ptable->slots[idx].plist;
		_ps_ptable->slots[idx].plist = ptn;
	}
	lock_release(&_ps_ptable->slots[idx].lock);

	return 1; /* affected items */
}

/**
 *
 */
int ps_ptable_remove(ps_presentity_t *pt)
{
	ps_presentity_t ptc;
	ps_presentity_t *ptn = NULL;
	uint32_t idx = 0;

	/* copy struct to fill in missing fields */
	memcpy(&ptc, pt, sizeof(ps_presentity_t));

	ptc.hashid = core_case_hash(&pt->user, &pt->domain, 0);
	idx = core_hash_idx(ptc.hashid, _ps_ptable->ssize);

	lock_get(&_ps_ptable->slots[idx].lock);
	ptn = _ps_ptable->slots[idx].plist;
	while(ptn != NULL) {
		if(ps_presentity_match(ptn, &ptc, 2) == 1) {
			if(ptn->next) {
				ptn->next->prev = ptn->prev;
			}
			if(ptn->prev) {
				ptn->prev->next = ptn->next;
			} else {
				_ps_ptable->slots[idx].plist = ptn->next;
			}
			break;
		}
		ptn = ptn->next;
	}
	lock_release(&_ps_ptable->slots[idx].lock);

	if(ptn != NULL) {
		ps_presentity_free(ptn, 0);
	}
	return 0;
}

/**
 *
 */
ps_presentity_t *ps_ptable_get_list(str *user, str *domain)
{
	ps_presentity_t ptc;
	ps_presentity_t *ptn = NULL;
	ps_presentity_t *ptl = NULL;
	ps_presentity_t *ptd = NULL;
	ps_presentity_t *pte = NULL;
	uint32_t idx = 0;

	memset(&ptc, 0, sizeof(ps_presentity_t));

	ptc.user = *user;
	ptc.domain = *domain;
	ptc.hashid = core_case_hash(&ptc.user, &ptc.domain, 0);
	idx = core_hash_idx(ptc.hashid, _ps_ptable->ssize);

	lock_get(&_ps_ptable->slots[idx].lock);
	ptn = _ps_ptable->slots[idx].plist;
	while(ptn != NULL) {
		if(ps_presentity_match(ptn, &ptc, 0) == 1) {
			ptd = ps_presentity_dup(ptn, 1);
			if(ptd == NULL) {
				break;
			}
			if(pte == NULL) {
				ptl = ptd;
			} else {
				pte->next = ptd;
				ptd->prev = pte;
			}
			pte = ptd;
		}
		ptn = ptn->next;
	}
	lock_release(&_ps_ptable->slots[idx].lock);

	if(ptd == NULL && ptl != NULL) {
		ps_presentity_list_free(ptl, 1);
		return NULL;
	}

	return ptl;
}

/**
 *
 */
ps_presentity_t *ps_ptable_search(ps_presentity_t *ptm, int mmode, int rmode)
{
	ps_presentity_t *ptn = NULL;
	ps_presentity_t *ptl = NULL;
	ps_presentity_t *ptd = NULL;
	ps_presentity_t *pte = NULL;
	uint32_t idx = 0;
	int pmax = 0;

	if(ptm->user.s == NULL || ptm->domain.s == NULL) {
		LM_WARN("no user or domain for presentity\n");
		return NULL;
	}

	ptm->hashid = core_case_hash(&ptm->user, &ptm->domain, 0);
	idx = core_hash_idx(ptm->hashid, _ps_ptable->ssize);

	lock_get(&_ps_ptable->slots[idx].lock);
	ptn = _ps_ptable->slots[idx].plist;
	while(ptn != NULL) {
		if((ps_presentity_match(ptn, ptm, mmode) == 1)
				&& (ptm->expires == 0 || ptn->expires > ptm->expires)) {
			ptd = ps_presentity_dup(ptn, 1);
			if(ptd == NULL) {
				break;
			}
			if(pte == NULL) {
				ptl = ptd;
			} else {
				pte->next = ptd;
				ptd->prev = pte;
			}
			pte = ptd;
		}
		ptn = ptn->next;
	}
	lock_release(&_ps_ptable->slots[idx].lock);

	if(ptd == NULL && ptl != NULL) {
		ps_presentity_list_free(ptl, 1);
		return NULL;
	}

	if(rmode == 1) {
		/* order list by priority */
		pte = NULL;
		while(ptl != NULL) {
			pmax = 0;
			ptn = ptl;
			ptd = ptl;
			while(ptn != NULL) {
				if(ptn->priority >= pmax) {
					pmax = ptn->priority;
					ptd = ptn;
				}
				ptn = ptn->next;
			}
			if(ptd == ptl) {
				ptl = ptl->next;
				if(ptl) {
					ptl->prev = NULL;
				}
				ptd->next = pte;
				if(pte) {
					pte->prev = ptd;
				}
				pte = ptd;
			} else {
				if(ptd->prev) {
					ptd->prev->next = ptd->next;
				}
				if(ptd->next) {
					ptd->next->prev = ptd->prev;
				}
				ptd->next = pte;
				ptd->prev = NULL;
				if(pte) {
					pte->prev = ptd;
				}
				pte = ptd;
			}
		}
		return pte;
	}

	/* default ordered by received time */
	return ptl;
}

/**
 *
 */
ps_presentity_t *ps_ptable_get_item(
		str *user, str *domain, str *event, str *etag)
{
	ps_presentity_t ptc;
	ps_presentity_t *ptn = NULL;
	ps_presentity_t *ptd = NULL;
	uint32_t idx = 0;

	memset(&ptc, 0, sizeof(ps_presentity_t));

	ptc.user = *user;
	ptc.domain = *domain;
	ptc.event = *event;
	ptc.etag = *etag;
	ptc.hashid = core_case_hash(&ptc.user, &ptc.domain, 0);
	idx = core_hash_idx(ptc.hashid, _ps_ptable->ssize);

	lock_get(&_ps_ptable->slots[idx].lock);
	ptn = _ps_ptable->slots[idx].plist;
	while(ptn != NULL) {
		if(ps_presentity_match(ptn, &ptc, 2) == 1) {
			ptd = ps_presentity_dup(ptn, 1);
			break;
		}
		ptn = ptn->next;
	}
	lock_release(&_ps_ptable->slots[idx].lock);

	return ptd;
}

/**
 *
 */
ps_presentity_t *ps_ptable_get_expired(int eval)
{
	ps_presentity_t *ptn = NULL;
	ps_presentity_t *ptl = NULL;
	ps_presentity_t *ptd = NULL;
	ps_presentity_t *pte = NULL;
	int i = 0;

	if(_ps_ptable == NULL) {
		return NULL;
	}

	for(i = 0; i < _ps_ptable->ssize; i++) {
		lock_get(&_ps_ptable->slots[i].lock);
		ptn = _ps_ptable->slots[i].plist;
		while(ptn != NULL) {
			if(ptn->expires > 0 && ptn->expires <= eval) {
				ptd = ps_presentity_dup(ptn, 1);
				if(ptd == NULL) {
					break;
				}
				if(pte == NULL) {
					ptl = ptd;
				} else {
					pte->next = ptd;
					ptd->prev = pte;
				}
				pte = ptd;
			}
			ptn = ptn->next;
		}
		lock_release(&_ps_ptable->slots[i].lock);
	}

	if(ptd == NULL && ptl != NULL) {
		ps_presentity_list_free(ptl, 1);
		return NULL;
	}

	return ptl;
}
