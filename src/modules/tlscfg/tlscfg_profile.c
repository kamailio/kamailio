/*
 * tlscfg module - TLS profile management companion for the tls module
 *
 * Copyright (C) 2026 Aurora Innovation
 *
 * Author: Daniel Donoghue
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 */

/**
 * @file tlscfg_profile.c
 * @brief In-memory TLS profile index — shared memory implementation
 * @ingroup tlscfg
 */

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"

#include "tlscfg_profile.h"

#include <string.h>
#include <time.h>

static tlscfg_data_t *_tlscfg_data = NULL;

/* ---- shared memory string helpers ---- */

static int shm_str_dup(str *dst, const str *src)
{
	dst->s = shm_malloc(src->len + 1);
	if(dst->s == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memcpy(dst->s, src->s, src->len);
	dst->s[src->len] = '\0';
	dst->len = src->len;
	return 0;
}

static void shm_str_free(str *s)
{
	if(s && s->s) {
		shm_free(s->s);
		s->s = NULL;
		s->len = 0;
	}
}

/* ---- free helpers ---- */

static void kv_free(tlscfg_kv_t *kv)
{
	if(kv) {
		shm_str_free(&kv->key);
		shm_str_free(&kv->value);
		shm_free(kv);
	}
}

static void profile_free(tlscfg_profile_t *p)
{
	tlscfg_kv_t *kv, *next;

	if(p) {
		shm_str_free(&p->profile_id);
		shm_str_free(&p->section_header);
		for(kv = p->kvs; kv; kv = next) {
			next = kv->next;
			kv_free(kv);
		}
		shm_free(p);
	}
}

/* ---- shared data init/destroy ---- */

int tlscfg_data_init(void)
{
	_tlscfg_data = shm_mallocxz(sizeof(tlscfg_data_t));
	if(_tlscfg_data == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	_tlscfg_data->lock = lock_alloc();
	if(_tlscfg_data->lock == NULL) {
		LM_ERR("failed to allocate lock\n");
		shm_free(_tlscfg_data);
		_tlscfg_data = NULL;
		return -1;
	}
	if(lock_init(_tlscfg_data->lock) == NULL) {
		LM_ERR("failed to init lock\n");
		lock_dealloc(_tlscfg_data->lock);
		shm_free(_tlscfg_data);
		_tlscfg_data = NULL;
		return -1;
	}
	_tlscfg_data->profiles = NULL;
	_tlscfg_data->dirty = 0;
	_tlscfg_data->last_mutation = 0;
	_tlscfg_data->config_mtime = 0;
	return 0;
}

void tlscfg_data_destroy(void)
{
	tlscfg_profile_t *p, *next;

	if(_tlscfg_data) {
		if(_tlscfg_data->lock) {
			lock_destroy(_tlscfg_data->lock);
			lock_dealloc(_tlscfg_data->lock);
		}
		for(p = _tlscfg_data->profiles; p; p = next) {
			next = p->next;
			profile_free(p);
		}
		shm_free(_tlscfg_data);
		_tlscfg_data = NULL;
	}
}

tlscfg_data_t *tlscfg_data_get(void)
{
	return _tlscfg_data;
}

/* ---- profile operations (caller MUST hold data->lock) ---- */

tlscfg_profile_t *tlscfg_profile_find(str *profile_id)
{
	tlscfg_profile_t *p;

	if(!_tlscfg_data)
		return NULL;
	for(p = _tlscfg_data->profiles; p; p = p->next) {
		if(p->profile_id.len == profile_id->len
				&& strncmp(p->profile_id.s, profile_id->s, profile_id->len)
						   == 0) {
			return p;
		}
	}
	return NULL;
}

int tlscfg_profile_add(str *profile_id, str *section_header)
{
	tlscfg_profile_t *p;

	if(!_tlscfg_data)
		return -1;

	if(tlscfg_profile_find(profile_id) != NULL) {
		LM_ERR("profile '%.*s' already exists\n", profile_id->len,
				profile_id->s);
		return -1;
	}

	p = shm_mallocxz(sizeof(tlscfg_profile_t));
	if(p == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}

	if(shm_str_dup(&p->profile_id, profile_id) < 0) {
		shm_free(p);
		return -1;
	}
	if(shm_str_dup(&p->section_header, section_header) < 0) {
		shm_str_free(&p->profile_id);
		shm_free(p);
		return -1;
	}

	if(section_header->len >= 7
			&& strncasecmp(section_header->s + 1, "client", 6) == 0) {
		p->profile_type = TLSCFG_TYPE_CLIENT;
	} else {
		p->profile_type = TLSCFG_TYPE_SERVER;
	}

	p->enabled = 1;
	p->kvs = NULL;
	p->next = _tlscfg_data->profiles;
	_tlscfg_data->profiles = p;

	_tlscfg_data->dirty = 1;
	_tlscfg_data->last_mutation = time(NULL);

	LM_INFO("added profile '%.*s' (%.*s)\n", profile_id->len, profile_id->s,
			section_header->len, section_header->s);
	return 0;
}

int tlscfg_profile_remove(str *profile_id)
{
	tlscfg_profile_t *p, *prev;

	if(!_tlscfg_data)
		return -1;

	prev = NULL;
	for(p = _tlscfg_data->profiles; p; prev = p, p = p->next) {
		if(p->profile_id.len == profile_id->len
				&& strncmp(p->profile_id.s, profile_id->s, profile_id->len)
						   == 0) {
			if(prev) {
				prev->next = p->next;
			} else {
				_tlscfg_data->profiles = p->next;
			}
			_tlscfg_data->dirty = 1;
			_tlscfg_data->last_mutation = time(NULL);
			profile_free(p);
			LM_INFO("removed profile '%.*s'\n", profile_id->len, profile_id->s);
			return 0;
		}
	}

	LM_ERR("profile '%.*s' not found\n", profile_id->len, profile_id->s);
	return -1;
}

tlscfg_kv_t *tlscfg_kv_find(tlscfg_profile_t *p, str *key)
{
	tlscfg_kv_t *kv;

	for(kv = p->kvs; kv; kv = kv->next) {
		if(kv->key.len == key->len
				&& strncasecmp(kv->key.s, key->s, key->len) == 0) {
			return kv;
		}
	}
	return NULL;
}

int tlscfg_profile_set(str *profile_id, str *key, str *value)
{
	tlscfg_profile_t *p;
	tlscfg_kv_t *kv;

	if(!_tlscfg_data)
		return -1;

	p = tlscfg_profile_find(profile_id);
	if(p == NULL) {
		LM_ERR("profile '%.*s' not found\n", profile_id->len, profile_id->s);
		return -1;
	}

	kv = tlscfg_kv_find(p, key);
	if(kv) {
		str new_val;
		if(shm_str_dup(&new_val, value) < 0) {
			return -1;
		}
		shm_str_free(&kv->value);
		kv->value = new_val;
	} else {
		kv = shm_mallocxz(sizeof(tlscfg_kv_t));
		if(kv == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		if(shm_str_dup(&kv->key, key) < 0) {
			shm_free(kv);
			return -1;
		}
		if(shm_str_dup(&kv->value, value) < 0) {
			shm_str_free(&kv->key);
			shm_free(kv);
			return -1;
		}
		kv->next = p->kvs;
		p->kvs = kv;
	}

	_tlscfg_data->dirty = 1;
	_tlscfg_data->last_mutation = time(NULL);
	return 0;
}

int tlscfg_profile_unset(str *profile_id, str *key)
{
	tlscfg_profile_t *p;
	tlscfg_kv_t *kv, *prev;

	if(!_tlscfg_data)
		return -1;

	p = tlscfg_profile_find(profile_id);
	if(p == NULL)
		return -1;

	prev = NULL;
	for(kv = p->kvs; kv; prev = kv, kv = kv->next) {
		if(kv->key.len == key->len
				&& strncasecmp(kv->key.s, key->s, key->len) == 0) {
			if(prev) {
				prev->next = kv->next;
			} else {
				p->kvs = kv->next;
			}
			kv_free(kv);
			_tlscfg_data->dirty = 1;
			_tlscfg_data->last_mutation = time(NULL);
			return 0;
		}
	}

	return -1;
}

int tlscfg_profile_set_id(str *old_id, str *new_id)
{
	tlscfg_profile_t *p;
	str tmp;

	if(!_tlscfg_data)
		return -1;

	p = tlscfg_profile_find(old_id);
	if(p == NULL) {
		LM_ERR("profile '%.*s' not found\n", old_id->len, old_id->s);
		return -1;
	}

	if(tlscfg_profile_find(new_id) != NULL) {
		LM_ERR("profile '%.*s' already exists\n", new_id->len, new_id->s);
		return -1;
	}

	if(shm_str_dup(&tmp, new_id) < 0) {
		return -1;
	}

	shm_str_free(&p->profile_id);
	p->profile_id = tmp;

	_tlscfg_data->dirty = 1;
	_tlscfg_data->last_mutation = time(NULL);

	LM_INFO("renamed profile '%.*s' -> '%.*s'\n", old_id->len, old_id->s,
			new_id->len, new_id->s);
	return 0;
}

int tlscfg_profile_set_enabled(str *profile_id, int enabled)
{
	tlscfg_profile_t *p;

	if(!_tlscfg_data)
		return -1;

	p = tlscfg_profile_find(profile_id);
	if(p == NULL)
		return -1;

	p->enabled = enabled ? 1 : 0;
	_tlscfg_data->dirty = 1;
	_tlscfg_data->last_mutation = time(NULL);
	return 0;
}

void tlscfg_profile_clear(void)
{
	tlscfg_profile_t *p, *next;

	if(!_tlscfg_data)
		return;
	for(p = _tlscfg_data->profiles; p; p = next) {
		next = p->next;
		profile_free(p);
	}
	_tlscfg_data->profiles = NULL;
}
