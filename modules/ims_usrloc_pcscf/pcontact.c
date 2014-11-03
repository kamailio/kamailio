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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */

#include "pcontact.h"
#include <string.h>
#include "../../hashes.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "ul_mod.h"
#include "usrloc.h"
#include "utime.h"
#include "ul_callback.h"
#include "usrloc.h"
#include "../../lib/ims/useful_defs.h"
#include "usrloc_db.h"
#include "../../parser/parse_uri.h"

extern int db_mode;
extern int hashing_type;

/*! retransmission detection interval in seconds */
int cseq_delay = 20;

void insert_ppublic(struct pcontact* _c, ppublic_t* _p)
{
	LM_DBG("linking IMPU <%.*s> to contact <%.*s>\n",
			_p->public_identity.len,_p->public_identity.s,
			_c->aor.len, _c->aor.s);

	if (_c->head == 0) {//first entry
		_c->head = _c->tail = _p;
	} else {
		_p->prev = _c->tail;
		_c->tail->next = _p;
		_c->tail = _p;
	}
}
int new_ppublic(str* public_identity, int is_default, ppublic_t** _p)
{
	*_p = (ppublic_t*)shm_malloc(sizeof(ppublic_t));
	if (!*_p) {
		LM_ERR("no more shm memory\n");
		return -1;
	}
	(*_p)->next=(*_p)->prev=0;
	(*_p)->public_identity.s = (char*)shm_malloc(public_identity->len);
	if (!(*_p)->public_identity.s) {
		LM_ERR("no more shm memory\n");
		if (*_p) {
			shm_free(*_p);
		}
		return -1;
	}

	(*_p)->is_default = is_default;
	memcpy((*_p)->public_identity.s, public_identity->s, public_identity->len);
	(*_p)->public_identity.len = public_identity->len;
	return 0;
}

void free_ppublic(ppublic_t* _p)
{
	if (!_p)
		return;
	if (_p->public_identity.s) {
		shm_free(_p->public_identity.s);
	}
	shm_free(_p);
}

int new_pcontact(struct udomain* _d, str* _contact, struct pcontact_info* _ci, struct pcontact** _c)
{
	int i;
	ppublic_t* ppublic_ptr;
	int is_default = 1;
	struct sip_uri sip_uri;


	*_c = (pcontact_t*)shm_malloc(sizeof(pcontact_t));
	if (*_c == 0) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset(*_c, 0, sizeof(pcontact_t));

	LM_DBG("New contact [<%.*s>] with %d associated IMPUs in state: [%s]\n",
			_contact->len, _contact->s,
			_ci->num_public_ids,
			reg_state_to_string(_ci->reg_state));

	(*_c)->aor.s = (char*) shm_malloc(_contact->len);
	if ((*_c)->aor.s == 0) {
		LM_ERR("no more shared memory\n");
		shm_free(*_c);
		*_c = 0;
		return -2;
	}
	memcpy((*_c)->aor.s, _contact->s, _contact->len);
	(*_c)->aor.len = _contact->len;
	(*_c)->domain = (str*)_d;

	if (parse_uri((*_c)->aor.s, (*_c)->aor.len, &sip_uri) != 0) {
		LM_ERR("unable to determine contact host from uri [%.*s\n", (*_c)->aor.len, (*_c)->aor.s);
		shm_free((*_c)->aor.s);
		shm_free(*_c);
		*_c = 0;
		return -2;
	}
	(*_c)->contact_host.s = sip_uri.host.s;
	(*_c)->contact_host.len = sip_uri.host.len;
	(*_c)->contact_port = sip_uri.port_no;
	(*_c)->contact_user.s = sip_uri.user.s;
	(*_c)->contact_user.len = sip_uri.user.len;

	(*_c)->expires = _ci->expires;
	(*_c)->reg_state = _ci->reg_state;

	// Add received Info:
	if (_ci->received_host.len > 0 && _ci->received_host.s) {
		(*_c)->received_host.s = (char*) shm_malloc(_ci->received_host.len);
		if ((*_c)->received_host.s == 0) {
			LM_ERR("no more share memory\n");
			shm_free((*_c)->aor.s);
			shm_free(*_c);
			*_c = 0;
			return -2;
		}
		memcpy((*_c)->received_host.s, _ci->received_host.s, _ci->received_host.len);
		(*_c)->received_host.len = _ci->received_host.len;
		(*_c)->received_port = _ci->received_port;
		(*_c)->received_proto = _ci->received_proto;
	}

	if (hashing_type==0) {
		(*_c)->aorhash = core_hash(_contact, 0, 0);
	} else if (hashing_type==1) {
		if ((*_c)->received_host.len > 0 && memcmp((*_c)->contact_host.s, (*_c)->received_host.s, (*_c)->contact_host.len) != 0) {
		    LM_DBG("Looks like this contact is natted - contact URI: [%.*s] but came from [%.*s]\n", (*_c)->contact_host.len, 
		    (*_c)->contact_host.s, (*_c)->received_host.len, (*_c)->received_host.s);
		    (*_c)->aorhash = core_hash(&(*_c)->received_host, 0, 0);
		} else 
		    (*_c)->aorhash = core_hash(&(*_c)->contact_host, 0, 0);
	} else {
		if ((*_c)->received_host.len > 0) {
			(*_c)->aorhash = core_hash(&(*_c)->received_host, 0, 0);	
		} else {
			(*_c)->aorhash = core_hash(&(*_c)->contact_host, 0, 0);
		}
	}

	//setup public ids
	for (i=0; i<_ci->num_public_ids; i++) {
		if (i>0) is_default = 0; //only the first one is default - P-Associated-uri (first one is default)
		if (new_ppublic(&_ci->public_ids[i], is_default, &ppublic_ptr)!=0) {
			LM_ERR("unable to create new ppublic\n");
		} else {
			insert_ppublic(*_c, ppublic_ptr);
		}
	}
	//add the service routes
	if (_ci->num_service_routes > 0) {
		(*_c)->service_routes = shm_malloc(_ci->num_service_routes * sizeof(str));
		if (!(*_c)->service_routes) {
			LM_ERR("no more shm mem\n");
			goto out_of_memory;
		} else {
			for (i = 0; i < _ci->num_service_routes; i++) {
				STR_SHM_DUP((*_c)->service_routes[i], _ci->service_routes[i], "new_pcontact");
			}
			(*_c)->num_service_routes = _ci->num_service_routes;
		}
	}
	return 0;

out_of_memory:
	return -1;
}

void free_pcontact(pcontact_t* _c) {
	ppublic_t* p, *tmp;
	int i;

	if (!_c)
		return;

	/*free callbacks*/
	if (_c->cbs.first) {
		destroy_ul_callbacks_list(_c->cbs.first);
	}

	LM_DBG("freeing pcontact: <%.*s>\n", _c->aor.len, _c->aor.s);
	//free ppublics
	p = _c->head;
	while (p) {
		LM_DBG("freeing linked IMPI: <%.*s>\n", p->public_identity.len, p->public_identity.s);
		tmp = p->next;
		free_ppublic(p);
		p = tmp;
	}
	//free service_routes
	if (_c->service_routes) { //remove old service routes
		for (i = 0; i < _c->num_service_routes; i++) {
			if (_c->service_routes[i].s)
				shm_free(_c->service_routes[i].s);
			shm_free(_c->service_routes);
			_c->service_routes = 0;
			_c->num_service_routes = 0;
		}
	}
	//free URI
	if (_c->aor.s) {
		shm_free(_c->aor.s);
	}

	//free received host
	if (_c->received_host.s) {
		shm_free(_c->received_host.s);
	}

	if (_c->rx_session_id.len > 0 && _c->rx_session_id.s)
		shm_free(_c->rx_session_id.s);
	shm_free(_c);
}

static inline void nodb_timer(pcontact_t* _c)
{
	LM_DBG("Running nodb timer on <%.*s>, "
			"Reg state: %s, "
			"Expires: %d, "
			"Expires in: %d seconds, "
			"Received: %.*s:%d, "
			"Proto: %d\n",
			_c->aor.len, _c->aor.s,
			reg_state_to_string(_c->reg_state),
			(int)_c->expires,
			(int)(_c->expires - time(NULL)),
			_c->received_host.len, _c->received_host.s,
			_c->received_port,
			_c->received_proto);

	get_act_time();
	if ((_c->expires - act_time) <= -10) {//we've allowed some grace time TODO: add as parameter
		LM_DBG("pcscf contact <%.*s> has expired and will be removed\n", _c->aor.len, _c->aor.s);
		if (exists_ulcb_type(PCSCF_CONTACT_EXPIRE)) {
			run_ul_callbacks(PCSCF_CONTACT_EXPIRE, _c);
		}

		if (db_mode == WRITE_THROUGH && db_delete_pcontact(_c) != 0) {
			LM_ERR("Error deleting ims_usrloc_pcscf record in DB");
		}

		update_stat(_c->slot->d->expired, 1);
		mem_delete_pcontact(_c->slot->d, _c);
		return;
	}

	//TODO: this is just for tmp debugging
//	p = _c->head;
//	while (p) {
//		if (p->is_default)
//			LM_DBG("public identity %i (default): <%.*s>\n", i, p->public_identity.len, p->public_identity.s);
//		else
//			LM_DBG("public identity %i: <%.*s>\n", i, p->public_identity.len, p->public_identity.s);
//		i++;
//		p=p->next;
//	}
//
//	LM_DBG("There are %i service routes as follows:\n", _c->num_service_routes);
//	for (i=0; i<_c->num_service_routes; i++) {
//		LM_DBG("service route %i: <%.*s>\n", i+1, _c->service_routes[i].len, _c->service_routes[i].s);
//	}
}

void timer_pcontact(pcontact_t* _r)
{
	nodb_timer(_r);
}

void print_pcontact(FILE* _f, pcontact_t* _r)
{

}
