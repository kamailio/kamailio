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

#ifndef _PEERSTATE_CACHE_H
#define _PEERSTATE_CACHE_H

#include "../../core/str.h"
#include "peerstate.h"

int init_peerstate_cache(int expire_time, int cleanup_interval, int hash_size);

/**
 * @brief Update peer state based on dialog event
 * 
 * @param peer Peer identifier
 * @param candidate_state Next candidate state (RINGING, INUSE, or NOT_INUSE)
 * @param early_enabled Whether EARLY event is processed (1) or not (0)
 * @param old_state Output: previous state (only valid if return > 0)
 * @return int 1 if state changed, 0 if no change, -1 on error
 */
int update_peer_state_dialog(str *peer, ps_state_t candidate_state,
		int early_enabled, ps_state_t *prev_state);

/**
 * @brief Update peer state based on usrloc event
 * 
 * @param peer Peer identifier
 * @param candidate_state Next candidate state (NOT_INUSE for register, UNAVAILABLE for unregister)
 * @param old_state Output: previous state (only valid if return > 0)
 * @return int 1 if state changed, 0 if no change, -1 on error
 */
int update_peer_state_usrloc(
		str *peer, ps_state_t candidate_state, ps_state_t *prev_state);

/**
 * @brief Get peer state from cache
 * 
 * @param peer Peer identifier
 * @param state Output: peer state
 * @param call_count Output: active call count
 * @param is_registered Output: registration status
 * @return int 0 on success, -1 if not found
 */
int get_peer_info(
		str *peer, ps_state_t *state, int *call_count, int *is_registered);

/**
 * @brief Get cache statistics
 * 
 * @param total_peers Output: total number of peers
 * @param incall_count Output: peers in call (INUSE or RINGING)
 * @param registered_count Output: registered peers
 */
void get_cache_stats(
		int *total_peers, int *incall_count, int *registered_count);

/**
 * @brief Get list of all peers in specific state
 * 
 * @param state Target state
 * @param peers Output: array of peer strings
 * @param count Output: number of peers
 * @return int 0 on success, -1 on error
 */
int get_peers_by_state(ps_state_t state, str **peers, int *count);

/**
 * @brief Get list of all peers (state independent)
 *
 * @param peers Output: array of peer strings
 * @param count Output: number of peers
 * @return int 0 on success, -1 on error
 */
int get_all_peers(str **peers, int *count);

/**
 * @brief Destroy cache system and free all resources
 */
void destroy_peerstate_cache(void);


#endif /* _PEERSTATE_CACHE_H */