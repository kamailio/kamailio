/*
 *
 * Peerstate Module - Peer State Tracking
 *
 * Copyright (C) 2025 Serdar Gucluer (Netgsm ICT Inc. - www.netgsm.com.tr)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#include <string.h>
#include <time.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/str_hash.h"
#include "../../core/dprint.h"
#include "../../core/timer.h"
#include "../../core/timer_proc.h"
#include "peerstate_cache.h"

struct ps_peer_state
{
	str peer;
	ps_state_t state;
	int call_count;
	unsigned char is_registered;
	time_t timestamp;
	struct ps_peer_state *next;
};

static int peer_state_hash_size = 0;

static struct ps_peer_state **peer_state_hash_table = NULL;
static gen_lock_t *peer_state_lock = NULL;

static int cache_expire_time = 0;
static int cache_cleanup_interval = 0;

static void cleanup_expired_entries(unsigned int ticks, void *param)
{
	int i;
	int removed = 0;
	struct ps_peer_state *p, *prev, *next;
	time_t now = time(NULL);

	if(!peer_state_hash_table || cache_expire_time == 0)
		return;
	lock_get(peer_state_lock);

	for(i = 0; i < peer_state_hash_size; i++) {
		prev = NULL;
		p = peer_state_hash_table[i];

		while(p) {
			next = p->next;
			if((now - p->timestamp) > cache_expire_time) {
				LM_DBG("Removing expired peer '%.*s'\n", p->peer.len,
						p->peer.s);
				if(prev)
					prev->next = next;
				else
					peer_state_hash_table[i] = next;
				shm_free(p);
				removed++;
			} else
				prev = p;
			p = next;
		}
	}

	lock_release(peer_state_lock);
	if(removed > 0)
		LM_INFO("Cleanup: removed %d expired entries\n", removed);
}

int init_peerstate_cache(int expire_time, int cleanup_interval, int hash_size)
{
	if(expire_time < 0) {
		LM_ERR("invalid expire_time: %d\n", expire_time);
		return -1;
	} else if(expire_time > 0 && expire_time < 60) {
		LM_WARN("expire_time too small, setting to 60 seconds for performance "
				"concerns\n");
		expire_time = 60;
	}

	if(cleanup_interval < 0) {
		LM_ERR("invalid cleanup_interval: %d\n", cleanup_interval);
		return -1;
	} else if(cleanup_interval > 0 && cleanup_interval < 10) {
		LM_WARN("cleanup_interval too small, setting to 10 seconds for "
				"performance concerns\n");
		cleanup_interval = 10;
	}

	if(expire_time > 0 && cleanup_interval > expire_time) {
		LM_WARN("cleanup_interval (%d) > expire_time (%d), adjusting to %d\n",
				cleanup_interval, expire_time, expire_time);
		cleanup_interval = expire_time;
	}

	cache_expire_time = expire_time;
	cache_cleanup_interval = cleanup_interval;
	peer_state_hash_size = hash_size;

	peer_state_hash_table = (struct ps_peer_state **)shm_malloc(
			peer_state_hash_size * sizeof(struct ps_peer_state *));
	if(!peer_state_hash_table) {
		SHM_MEM_ERROR;
		return -1;
	}

	int i;
	for(i = 0; i < peer_state_hash_size; i++) {
		peer_state_hash_table[i] = NULL;
	}

	peer_state_lock = lock_alloc();
	if(!peer_state_lock) {
		LM_ERR("failed to allocate lock\n");
		shm_free(peer_state_hash_table);
		return -1;
	}

	if(!lock_init(peer_state_lock)) {
		LM_ERR("failed to initialize lock\n");
		lock_dealloc(peer_state_lock);
		shm_free(peer_state_hash_table);
		return -1;
	}

	if(cache_expire_time > 0 && cache_cleanup_interval > 0) {
		if(register_timer(cleanup_expired_entries, NULL, cache_cleanup_interval)
				< 0)
			LM_WARN("failed to register cleanup timer\n");
		else
			LM_DBG("Cleanup timer registered (expire=%ds, interval=%ds)\n",
					cache_expire_time, cache_cleanup_interval);
	}

	LM_DBG("Peerstate cache initialized (size=%d, expire=%ds, cleanup=%ds)\n",
			peer_state_hash_size, cache_expire_time, cache_cleanup_interval);
	return 0;
}

/**
 * @brief Update peer state based on dialog event
 */
int update_peer_state_dialog(str *peer, ps_state_t candidate_state,
		int early_enabled, ps_state_t *prev_state)
{
	unsigned int hash;
	struct ps_peer_state *p;
	ps_state_t old_state, new_state;
	int old_count, new_count;
	int state_changed = 0;

	if(!peer || !peer->s || peer->len == 0) {
		LM_ERR("invalid peer parameter\n");
		return -1;
	}

	if(!peer_state_hash_table) {
		LM_ERR("cache not initialized\n");
		return -1;
	}

	hash = get_hash1_raw(peer->s, peer->len) % peer_state_hash_size;
	lock_get(peer_state_lock);

	// Find or create peer entry
	p = peer_state_hash_table[hash];
	while(p) {
		if(p->peer.len == peer->len
				&& memcmp(p->peer.s, peer->s, peer->len) == 0)
			break;
		p = p->next;
	}

	// Create new entry if not found
	if(!p) {
		p = (struct ps_peer_state *)shm_malloc(
				sizeof(struct ps_peer_state) + peer->len);
		if(!p) {
			SHM_MEM_ERROR;
			lock_release(peer_state_lock);
			return -1;
		}

		p->peer.s = (char *)p + sizeof(struct ps_peer_state);
		p->peer.len = peer->len;
		memcpy(p->peer.s, peer->s, peer->len);
		p->state = NOT_INUSE;
		p->call_count = 0;
		p->timestamp = time(NULL);
		p->next = peer_state_hash_table[hash];
		peer_state_hash_table[hash] = p;

		LM_DBG("Created new peer entry '%.*s'\n", peer->len, peer->s);
	}

	// Save old values
	old_state = p->state;
	old_count = p->call_count;

	// Process based on candidate state
	if(candidate_state == RINGING) {
		// EARLY event occurred
		// Always increment counter for new dialog
		p->call_count++;
		new_count = p->call_count;

		// Update state based on priority: INUSE > RINGING > NOT_INUSE
		switch(old_state) {
			case INUSE:
				// Don't downgrade from INUSE to RINGING
				new_state = INUSE;
				break;
			case RINGING:
				// Already ringing, stay ringing
				new_state = RINGING;
				break;
			case NOT_INUSE:
			case UNAVAILABLE:
			default:
				// NOT_INUSE or UNAVAILABLE -> upgrade to RINGING
				new_state = RINGING;
				break;
		}

		p->state = new_state;
		p->timestamp = time(NULL);

		LM_DBG("EARLY: peer='%.*s', old_state=%s, new_state=%s, old_count=%d, "
			   "new_count=%d, is_registered=%d\n",
				peer->len, peer->s, PS_STATE_TO_STR(old_state),
				PS_STATE_TO_STR(new_state), old_count, new_count,
				p->is_registered);
	} else if(candidate_state == INUSE) {
		// CONFIRMED event occurred
		// Check if EARLY was enabled
		if(early_enabled)
			new_count =
					p->call_count; // EARLY was enabled, count already incremented, Don't increment again (same dialog transitioning)
		else {
			// EARLY was disabled, this is first event for this dialog
			if(p->call_count == 0) {
				// First dialog event, increment
				p->call_count++;
				new_count = p->call_count;
			} else {
				// Already has calls, don't increment
				// (shouldn't happen but safety check)
				new_count = p->call_count;
			}
		}

		// Always upgrade to INUSE
		new_state = INUSE;

		p->state = new_state;
		p->timestamp = time(NULL);

		LM_DBG("CONFIRMED: peer='%.*s', old_state=%s, new_state=%s, "
			   "old_count=%d, new_count=%d, early_enabled=%d, "
			   "is_registered=%d\n",
				peer->len, peer->s, PS_STATE_TO_STR(old_state),
				PS_STATE_TO_STR(new_state), old_count, new_count, early_enabled,
				p->is_registered);
	} else if(candidate_state == NOT_INUSE) {
		// FAILED / EXPIRED / TERMINATED event occurred
		// Always decrement counter
		if(p->call_count > 0)
			p->call_count--;
		else
			LM_WARN("BUG: call_count already zero for peer '%.*s'\n", peer->len,
					peer->s);

		new_count = p->call_count;

		// Determine new state based on remaining calls and registration
		if(new_count > 0)
			new_state = old_state; // Still has active calls, keep current state
		else {
			// No more active calls, Check registration flag
			if(p->is_registered)
				new_state = NOT_INUSE;
			else
				new_state = UNAVAILABLE;
		}

		p->state = new_state;
		p->timestamp = time(NULL);

		LM_DBG("ENDED: peer='%.*s', old_state=%s, new_state=%s, old_count=%d, "
			   "new_count=%d, is_registered=%d\n",
				peer->len, peer->s, PS_STATE_TO_STR(old_state),
				PS_STATE_TO_STR(new_state), old_count, new_count,
				p->is_registered);
	} else {
		// Invalid candidate state
		LM_ERR("Invalid candidate_state=%d for peer '%.*s'\n", candidate_state,
				peer->len, peer->s);
		lock_release(peer_state_lock);
		return -1;
	}

	// Check if state actually changed
	if(old_state != new_state) {
		state_changed = 1;
		if(prev_state)
			*prev_state = old_state;
	} else
		state_changed = 0;

	lock_release(peer_state_lock);

	return state_changed;
}

/**
 * @brief Update peer state based on usrloc event
 */
int update_peer_state_usrloc(
		str *peer, ps_state_t candidate_state, ps_state_t *prev_state)
{
	unsigned int hash;
	struct ps_peer_state *p;
	ps_state_t old_state, new_state;
	int call_count;
	int state_changed = 0;

	if(!peer || !peer->s || peer->len == 0) {
		LM_ERR("invalid peer parameter\n");
		return -1;
	}

	if(!peer_state_hash_table) {
		LM_ERR("cache not initialized\n");
		return -1;
	}

	hash = get_hash1_raw(peer->s, peer->len) % peer_state_hash_size;
	lock_get(peer_state_lock);

	// Find or create peer entry
	p = peer_state_hash_table[hash];
	while(p) {
		if(p->peer.len == peer->len
				&& memcmp(p->peer.s, peer->s, peer->len) == 0)
			break;
		p = p->next;
	}

	// Create new entry if not found
	if(!p) {
		p = (struct ps_peer_state *)shm_malloc(
				sizeof(struct ps_peer_state) + peer->len);
		if(!p) {
			SHM_MEM_ERROR;
			lock_release(peer_state_lock);
			return -1;
		}

		p->peer.s = (char *)p + sizeof(struct ps_peer_state);
		p->peer.len = peer->len;
		memcpy(p->peer.s, peer->s, peer->len);
		p->state = NOT_INUSE;
		p->call_count = 0;
		p->timestamp = time(NULL);
		p->next = peer_state_hash_table[hash];
		peer_state_hash_table[hash] = p;

		LM_DBG("Created new peer entry '%.*s'\n", peer->len, peer->s);
	}

	// Save old values
	old_state = p->state;
	call_count = p->call_count;

	if(candidate_state == NOT_INUSE) {
		// REGISTERED event occurred
		// Update registration flag
		p->is_registered = 1;

		if(old_state == UNAVAILABLE)
			new_state = NOT_INUSE; // Peer was unavailable, now registered
		else
			new_state =
					old_state; // Registration doesn't interrupt active calls, NOT_INUSE, INUSE, or RINGING - no change
		p->state = new_state;
		p->timestamp = time(NULL);

		LM_DBG("REGISTERED: peer='%.*s', old_state=%s, new_state=%s, count=%d, "
			   "is_registered=%d\n",
				peer->len, peer->s, PS_STATE_TO_STR(old_state),
				PS_STATE_TO_STR(new_state), call_count, p->is_registered);
	} else if(candidate_state == UNAVAILABLE) {
		// UNREGISTERED event occurred
		// Update registration flag
		p->is_registered = 0;

		switch(old_state) {
			case INUSE:
			case RINGING:
				// Keep current state during active call or ringing
				new_state = old_state;
				break;
			case NOT_INUSE:
			case UNAVAILABLE:
			default:
				// Mark as unavailable
				new_state = UNAVAILABLE;
				break;
		}

		p->state = new_state;
		p->timestamp = time(NULL);

		LM_DBG("UNREGISTERED: peer='%.*s', old_state=%s, new_state=%s, "
			   "count=%d, is_registered=%d\n",
				peer->len, peer->s, PS_STATE_TO_STR(old_state),
				PS_STATE_TO_STR(new_state), call_count, p->is_registered);
	} else {
		// Invalid candidate state for usrloc event
		LM_ERR("Invalid usrloc candidate_state=%d for peer '%.*s'\n",
				candidate_state, peer->len, peer->s);
		lock_release(peer_state_lock);
		return -1;
	}

	if(old_state != new_state) {
		state_changed = 1;
		if(prev_state)
			*prev_state = old_state;
	} else
		state_changed = 0;

	lock_release(peer_state_lock);

	return state_changed;
}

/**
 * @brief Get peer info from cache
 */
int get_peer_info(
		str *peer, ps_state_t *state, int *call_count, int *is_registered)
{
	unsigned int hash;
	struct ps_peer_state *p;

	if(!peer || !peer->s || peer->len == 0)
		return -1;
	if(!peer_state_hash_table)
		return -1;

	hash = get_hash1_raw(peer->s, peer->len) % peer_state_hash_size;

	lock_get(peer_state_lock);

	p = peer_state_hash_table[hash];
	while(p) {
		if(p->peer.len == peer->len
				&& memcmp(p->peer.s, peer->s, peer->len) == 0) {
			if(state)
				*state = p->state;
			if(call_count)
				*call_count = p->call_count;
			if(is_registered)
				*is_registered = p->is_registered;
			lock_release(peer_state_lock);
			return 0;
		}
		p = p->next;
	}

	lock_release(peer_state_lock);
	return -1;
}

/**
 * @brief Get cache statistics
 */
void get_cache_stats(int *total_peers, int *incall_count, int *registered_count)
{
	int i;
	int total = 0, incall = 0, registered = 0;
	struct ps_peer_state *p;

	if(!peer_state_hash_table) {
		if(total_peers)
			*total_peers = 0;
		if(incall_count)
			*incall_count = 0;
		if(registered_count)
			*registered_count = 0;
		return;
	}

	lock_get(peer_state_lock);

	for(i = 0; i < peer_state_hash_size; i++) {
		p = peer_state_hash_table[i];
		while(p) {
			total++;
			if(p->state == INUSE || p->state == RINGING)
				incall++;
			if(p->is_registered)
				registered++;
			p = p->next;
		}
	}

	lock_release(peer_state_lock);

	if(total_peers)
		*total_peers = total;
	if(incall_count)
		*incall_count = incall;
	if(registered_count)
		*registered_count = registered;
}

/**
 * @brief Get list of peers by state
 */
int get_peers_by_state(ps_state_t target_state, str **peers, int *count)
{
	int i, found = 0;
	struct ps_peer_state *p;
	str *result = NULL;
	int capacity = 100;

	if(!peer_state_hash_table || !peers || !count)
		return -1;

	result = (str *)pkg_malloc(capacity * sizeof(str));
	if(!result) {
		PKG_MEM_ERROR;
		return -1;
	}

	lock_get(peer_state_lock);

	for(i = 0; i < peer_state_hash_size; i++) {
		p = peer_state_hash_table[i];
		while(p) {
			if(p->state == target_state) {
				if(found >= capacity) {
					capacity *= 2;
					str *new_result =
							(str *)pkg_realloc(result, capacity * sizeof(str));
					if(!new_result) {
						PKG_MEM_ERROR;
						lock_release(peer_state_lock);
						pkg_free(result);
						return -1;
					}
					result = new_result;
				}

				result[found].s = (char *)pkg_malloc(p->peer.len + 1);
				if(!result[found].s) {
					PKG_MEM_ERROR;
					lock_release(peer_state_lock);
					for(int j = 0; j < found; j++)
						pkg_free(result[j].s);
					pkg_free(result);
					return -1;
				}

				memcpy(result[found].s, p->peer.s, p->peer.len);
				result[found].s[p->peer.len] = '\0';
				result[found].len = p->peer.len;
				found++;
			}
			p = p->next;
		}
	}

	lock_release(peer_state_lock);

	*peers = result;
	*count = found;
	return 0;
}

/**
 * @brief Get list of all peers
 */
int get_all_peers(str **peers, int *count)
{
	int i, found = 0;
	struct ps_peer_state *p;
	str *result = NULL;
	int capacity = 100;

	if(!peer_state_hash_table || !peers || !count)
		return -1;

	result = (str *)pkg_malloc(capacity * sizeof(str));
	if(!result) {
		PKG_MEM_ERROR;
		return -1;
	}

	lock_get(peer_state_lock);

	for(i = 0; i < peer_state_hash_size; i++) {
		p = peer_state_hash_table[i];
		while(p) {
			if(found >= capacity) {
				capacity *= 2;
				str *new_result =
						(str *)pkg_realloc(result, capacity * sizeof(str));
				if(!new_result) {
					PKG_MEM_ERROR;
					lock_release(peer_state_lock);
					pkg_free(result);
					return -1;
				}
				result = new_result;
			}

			result[found].s = (char *)pkg_malloc(p->peer.len + 1);
			if(!result[found].s) {
				PKG_MEM_ERROR;
				lock_release(peer_state_lock);
				for(int j = 0; j < found; j++)
					pkg_free(result[j].s);
				pkg_free(result);
				return -1;
			}

			memcpy(result[found].s, p->peer.s, p->peer.len);
			result[found].s[p->peer.len] = '\0';
			result[found].len = p->peer.len;
			found++;

			p = p->next;
		}
	}

	lock_release(peer_state_lock);

	*peers = result;
	*count = found;
	return 0;
}

/**
 * @brief Destroy cache system
 */
void destroy_peerstate_cache(void)
{
	int i;
	struct ps_peer_state *p, *next;

	if(!peer_state_hash_table) {
		LM_DBG("Cache not initialized, nothing to destroy\n");
		return;
	}

	if(!peer_state_lock) {
		LM_DBG("Cache lock not initialized\n");
		return;
	}

	lock_get(peer_state_lock);

	/* Free all peer state entries */
	for(i = 0; i < peer_state_hash_size; i++) {
		p = peer_state_hash_table[i];
		while(p) {
			next = p->next;
			shm_free(p);
			p = next;
		}
		peer_state_hash_table[i] = NULL;
	}

	lock_release(peer_state_lock);

	/* Free hash table */
	shm_free(peer_state_hash_table);
	peer_state_hash_table = NULL;

	/* Destroy and free lock */
	lock_destroy(peer_state_lock);
	lock_dealloc(peer_state_lock);
	peer_state_lock = NULL;

	LM_DBG("Cache system destroyed\n");
}
