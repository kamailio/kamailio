/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include "udomain.h"
#include <string.h>
#include "../../hashes.h"
#include "../../parser/parse_methods.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db.h"
#include "../../socket_info.h"
#include "../../ut.h"
#include "ul_mod.h"            /* usrloc module parameters */
#include "usrloc.h"
#include "utime.h"
#include "usrloc.h"

#include "../../lib/ims/useful_defs.h"

#ifdef STATISTICS
static char *build_stat_name( str* domain, char *var_name)
{
	int n;
	char *s;
	char *p;

	n = domain->len + 1 + strlen(var_name) + 1;
	s = (char*)shm_malloc( n );
	if (s==0) {
		LM_ERR("no more shm mem\n");
		return 0;
	}
	memcpy( s, domain->s, domain->len);
	p = s + domain->len;
	*(p++) = '-';
	memcpy( p , var_name, strlen(var_name));
	p += strlen(var_name);
	*(p++) = 0;
	return s;
}
#endif

int new_udomain(str* _n, int _s, udomain_t** _d)
{
	int i;
#ifdef STATISTICS
	char *name;
#endif

	/* Must be always in shared memory, since
	 * the cache is accessed from timer which
	 * lives in a separate process
	 */
	*_d = (udomain_t*)shm_malloc(sizeof(udomain_t));
	if (!(*_d)) {
		LM_ERR("new_udomain(): No memory left\n");
		goto error0;
	}
	memset(*_d, 0, sizeof(udomain_t));

	(*_d)->table = (hslot_t*)shm_malloc(sizeof(hslot_t) * _s);
	if (!(*_d)->table) {
		LM_ERR("no memory left 2\n");
		goto error1;
	}

	(*_d)->name = _n;

	for(i = 0; i < _s; i++) {
		init_slot(*_d, &((*_d)->table[i]), i);
	}

	(*_d)->size = _s;

#ifdef STATISTICS
	/* register the statistics */
	if ( (name=build_stat_name(_n,"contacts"))==0 || register_stat("usrloc",
	name, &(*_d)->contacts, STAT_NO_RESET|STAT_SHM_NAME)!=0 ) {
		LM_ERR("failed to add stat variable\n");
		goto error2;
	}
	if ( (name=build_stat_name(_n,"expires"))==0 || register_stat("usrloc",
	name, &(*_d)->expired, STAT_SHM_NAME)!=0 ) {
		LM_ERR("failed to add stat variable\n");
		goto error2;
	}
#endif

	return 0;
#ifdef STATISTICS
error2:
	shm_free((*_d)->table);
#endif
error1:
	shm_free(*_d);
error0:
	return -1;
}

void free_udomain(udomain_t* _d)
{
	int i;

	if (_d->table) {
		for(i = 0; i < _d->size; i++) {
			lock_ulslot(_d, i);
			deinit_slot(_d->table + i);
			unlock_ulslot(_d, i);
		}
		shm_free(_d->table);
	}
	shm_free(_d);
}

void print_udomain(FILE* _f, udomain_t* _d)
{
	int i;
	int max=0, slot=0, n=0;
	struct pcontact* r;
	fprintf(_f, "---Domain---\n");
	fprintf(_f, "name : '%.*s'\n", _d->name->len, ZSW(_d->name->s));
	fprintf(_f, "size : %d\n", _d->size);
	fprintf(_f, "table: %p\n", _d->table);
	/*fprintf(_f, "lock : %d\n", _d->lock); -- can be a structure --andrei*/
	fprintf(_f, "\n");
	for(i=0; i<_d->size; i++)
	{
		r = _d->table[i].first;
		n += _d->table[i].n;
		if(max<_d->table[i].n){
			max= _d->table[i].n;
			slot = i;
		}
		while(r) {
			print_pcontact(_f, r);
			r = r->next;
		}
	}
	fprintf(_f, "\nMax slot: %d (%d/%d)\n", max, slot, n);
	fprintf(_f, "\n---/Domain---\n");
}


inline int time2str(time_t _v, char* _s, int* _l)
{
	struct tm* t;
	int l;

	if ((!_s) || (!_l) || (*_l < 2)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	*_s++ = '\'';

	/* Convert time_t structure to format accepted by the database */
	t = localtime(&_v);
	l = strftime(_s, *_l -1, "%Y-%m-%d %H:%M:%S", t);

	if (l == 0) {
		LM_ERR("Error during time conversion\n");
		/* the value of _s is now unspecified */
		_s = NULL;
		_l = 0;
		return -1;
	}
	*_l = l;

	*(_s + l) = '\'';
	*_l = l + 2;
	return 0;
}

int mem_insert_pcontact(struct udomain* _d, str* _contact, struct pcontact_info* _ci, struct pcontact** _c){
	int sl;

	if (new_pcontact(_d->name, _contact, _ci, _c) < 0) {
		LM_ERR("creating pcontact failed\n");
		return -1;
	}

	sl = ((*_c)->aorhash) & (_d->size - 1);
	slot_add(&_d->table[sl], *_c);
	update_stat(_d->contacts, 1);
	return 0;
}

void mem_delete_pcontact(udomain_t* _d, struct pcontact* _c)
{
	slot_rem(_c->slot, _c);
	free_pcontact(_c);
	update_stat( _d->contacts, -1);
}

void mem_timer_udomain(udomain_t* _d)
{
	struct pcontact* ptr;
	int i;

	for(i=0; i<_d->size; i++)
	{
		lock_ulslot(_d, i);

		ptr = _d->table[i].first;

		while(ptr) {
			timer_pcontact(ptr);
			/* Remove the entire record if it is empty */
			//if (ptr->contacts == 0) {
			//	t = ptr;
			//	ptr = ptr->next;
			//	mem_delete_pcontact(_d, t);
			//}//// else {
			//	ptr = ptr->next;
			//}
			ptr = ptr->next;
		}
		unlock_ulslot(_d, i);
	}
}

void lock_udomain(udomain_t* _d, str* _aor)
{
	unsigned int sl;
	sl = core_hash(_aor, 0, _d->size);

#ifdef GEN_LOCK_T_PREFERED
	lock_get(_d->table[sl].lock);
#else
	ul_lock_idx(_d->table[sl].lockidx);
#endif
}

void unlock_udomain(udomain_t* _d, str* _aor)
{
	unsigned int sl;
	sl = core_hash(_aor, 0, _d->size);
#ifdef GEN_LOCK_T_PREFERED
	lock_release(_d->table[sl].lock);
#else
	ul_release_idx(_d->table[sl].lockidx);
#endif
}

void lock_ulslot(udomain_t* _d, int i)
{
#ifdef GEN_LOCK_T_PREFERED
	lock_get(_d->table[i].lock);
#else
	ul_lock_idx(_d->table[i].lockidx);
#endif
}


void unlock_ulslot(udomain_t* _d, int i)
{
#ifdef GEN_LOCK_T_PREFERED
	lock_release(_d->table[i].lock);
#else
	ul_release_idx(_d->table[i].lockidx);
#endif
}

int update_rx_regsession(struct udomain* _d, str* session_id, struct pcontact* _c) {
	if (session_id->len > 0 && session_id->s) {
		if (_c->rx_session_id.len > 0 && _c->rx_session_id.s) {
			_c->rx_session_id.len = 0;
			shm_free(_c->rx_session_id.s);
		}
		_c->rx_session_id.s = shm_malloc(session_id->len);
		if (!_c->rx_session_id.s) {
			LM_ERR("no more shm_mem\n");
			return -1;
		}
		memcpy(_c->rx_session_id.s, session_id->s, session_id->len);
		_c->rx_session_id.len = session_id->len;
	} else {
		return -1;
	}
	return 0;
}

int update_pcontact(struct udomain* _d, struct pcontact_info* _ci, struct pcontact* _c) //TODO: should prob move this to pcontact
{
	int is_default = 1;
	ppublic_t* ppublic_ptr;
	int i;

	_c->reg_state = _ci->reg_state;

	if (_ci->expires > 0) {
		_c->expires = _ci->expires;
	}

	if (_ci->num_service_routes > 0 && _ci->service_routes) {
		//replace all existing service routes
		if (_c->service_routes) { //remove old service routes
			for (i=0; i<_c->num_service_routes; i++) {
				if (_c->service_routes[i].s)
					shm_free(_c->service_routes[i].s);
				shm_free(_c->service_routes);
				_c->service_routes=0;
				_c->num_service_routes=0;
			}
		}
		//now add the new service routes
		if (_ci->num_service_routes > 0) {
			_c->service_routes = shm_malloc(_ci->num_service_routes*sizeof(str));
			if (!_c->service_routes) {
				LM_ERR("no more shm mem trying to allocate [%ld bytes]\n", _ci->num_service_routes*sizeof(str));
				goto out_of_memory;
			} else {
				for (i=0; i<_ci->num_service_routes; i++) {
					STR_SHM_DUP(_c->service_routes[i], _ci->service_routes[i], "update_pcontact");
				}
				_c->num_service_routes = _ci->num_service_routes;
			}
		}
	}

	if (_ci->num_public_ids > 0 && _ci->public_ids) {
		if (_c->head) {
			LM_DBG("ppublic's already exist.... .not updating\n");
		} else {
			for (i = 0; i < _ci->num_public_ids; i++) {
				if (i > 0)
					is_default = 0; //only the first one is default - P-Associated-uri (first one is default)
				if (new_ppublic(&_ci->public_ids[i], is_default, &ppublic_ptr) != 0) {
					LM_ERR("unable to create new ppublic\n");
				} else {
					insert_ppublic(_c, ppublic_ptr);
				}
			}
		}
	}

	// update received info:
	if (_ci->received_host.len > 0) {
		if (_c->received_host.s)
			shm_free(_c->received_host.s);
		STR_SHM_DUP(_c->received_host, _ci->received_host, "update_pcontact");
	}
	if (_ci->received_port > 0) _c->received_port = _ci->received_port;
	if (_ci->received_proto > 0) _c->received_proto = _ci->received_proto;

	//TODO: update path, etc
	run_ul_callbacks(PCSCF_CONTACT_UPDATE, _c);
	return 0;

out_of_memory:
	return -1;
}

int insert_pcontact(struct udomain* _d, str* _contact, struct pcontact_info* _ci, struct pcontact** _c) {

    if (mem_insert_pcontact(_d, _contact, _ci, _c)){
        LM_ERR("inserting pcontact failed\n");
        goto error;
    }
    if (exists_ulcb_type(PCSCF_CONTACT_INSERT)) {
		run_ul_create_callbacks(*_c);
	}
    return 0;

error:
    return -1;
}

int get_pcontact(udomain_t* _d, str* _contact, struct pcontact** _c) {
	unsigned int sl, i, aorhash;
	struct pcontact* c;

	/* search in cache */
	aorhash = core_hash(_contact, 0, 0);
	sl = aorhash & (_d->size - 1);
	c = _d->table[sl].first;

	for (i = 0; i < _d->table[sl].n; i++) {
		if ((c->aorhash == aorhash) && (c->aor.len == _contact->len)
				&& !memcmp(c->aor.s, _contact->s, _contact->len)) {
			*_c = c;
			return 0;
		}

		c = c->next;
	}
	return 1; /* Nothing found */
}

int get_pcontact_by_src(udomain_t* _d, str * _host, unsigned short _port, unsigned short _proto, struct pcontact** _c) {
	int i;
	struct pcontact* c;

	if (_d->table) {
		for(i = 0; i < _d->size; i++) {
		}
	}


	for(i=0; i<_d->size; i++)
	{
		c = _d->table[i].first;
		while(c) {
			LM_DBG("Port %d (search %d), Proto %d (search %d), reg_state %s (search %s)\n",
				c->received_port, _port, c->received_proto, _proto, 
				reg_state_to_string(c->reg_state), reg_state_to_string(PCONTACT_REGISTERED)
				);
			// First check, if Proto and Port matches:
			if ((c->reg_state == PCONTACT_REGISTERED) && (c->received_port == _port) && (c->received_proto == _proto)) {
				LM_DBG("Received host len %d (search %d)\n", c->received_host.len, _host->len);
				// Then check the length:
				if (c->received_host.len == _host->len) {
					LM_DBG("Received host %.*s (search %.*s)\n",
						c->received_host.len, c->received_host.s,
						_host->len, _host->s);

					// Finally really compare the "received_host"
					if (!memcmp(c->received_host.s, _host->s, _host->len)) {
						*_c = c;
						return 0;
					}
				}
			}
			c = c->next;
		}
	}
	return 1; /* Nothing found */
}

int delete_pcontact(udomain_t* _d, str* _aor, struct pcontact* _c)
{
	if (_c==0) {
		if (get_pcontact(_d, _aor, &_c) > 0) {
			return 0;
		}
	}
	if (exists_ulcb_type(PCSCF_CONTACT_DELETE)) {
		run_ul_callbacks(PCSCF_CONTACT_DELETE, _c);
	}
	mem_delete_pcontact(_d, _c);

	return 0;
}
