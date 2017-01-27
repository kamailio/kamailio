/**
 * Copyright (C) 2017 Carsten Bock, ng-voice GmbH (carsten@ng-voice.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include "cdp_functions.h"
#include "peermanager.h"
#include "peerstatemachine.h"
#include "receiver.h"
#include "../../core/str.h"
#include "../../core/dprint.h"

extern dp_config *config;
extern peer_list_t *peer_list;
extern gen_lock_t *peer_list_lock;
extern char *dp_states[];

int check_peer(str * peer_fqdn) {
	peer * p;
	p = get_peer_by_fqdn(peer_fqdn);
	if (p && !p->disabled &&  (p->state == I_Open || p->state == R_Open)) {
		return 1;
	} else {
		return -1;
	}	
}

int check_application(int vendorid, int application) 
{
	peer *i, *j;
	int c;

	lock_get(peer_list_lock);
	i = peer_list->head;
	while (i) {
		lock_get(i->lock);
		if (i && !i->disabled &&  (i->state == I_Open || i->state == R_Open)) {
			for (c = 0; c < i->applications_cnt; c++) {
				if (((vendorid <= 0) || (vendorid == i->applications[c].vendor)) && (i->applications[c].id == application)) {
					lock_release(i->lock);
					lock_release(peer_list_lock);
					return 1;
				}
			}
		}
		j=i;
		i = i->next;
		lock_release(j->lock);
	}
	lock_release(peer_list_lock);
	return -1;
}

