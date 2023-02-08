/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * Copyright (C) 2019 Aleksandar Yosifov
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

#include "udomain.h"
#include <string.h>
#include "../../core/hashes.h"
#include "../../core/parser/parse_methods.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/dprint.h"
#include "../../lib/srdb1/db.h"
#include "../../core/socket_info.h"
#include "../../core/ut.h"
#include "ims_usrloc_pcscf_mod.h"            /* usrloc module parameters */
#include "usrloc.h"
#include "utime.h"
#include "ul_callback.h"
#include "usrloc_db.h"
#include "../../core/parser/parse_uri.h"

#include "../../lib/ims/useful_defs.h"
#include "../../modules/presence/presence.h"
extern int expires_grace;
extern int audit_expired_pcontacts_timeout;
extern int db_mode;
extern int db_mode_ext;
extern int match_contact_host_port;

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
	*(p++) = *ksr_stats_namesep;
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
	(*_c)->sl = sl;
        LM_DBG("Putting contact into slot [%d]\n", sl);
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
	struct pcontact* ptr, *tmp;
	int i;

	for(i=0; i<_d->size; i++)
	{
		lock_ulslot(_d, i);

		ptr = _d->table[i].first;

		while(ptr) {
		    tmp = ptr;
			ptr = ptr->next;
		    timer_pcontact(tmp);
		}
		
		unlock_ulslot(_d, i);
	}
}

void lock_udomain(udomain_t* _d, str* via_host, unsigned short via_port, unsigned short via_proto)
{
	unsigned int sl;
	
	sl = get_hash_slot(_d, via_host, via_port, via_proto);

#ifdef GEN_LOCK_T_PREFERED
	lock_get(_d->table[sl].lock);
#else
	ul_lock_idx(_d->table[sl].lockidx);
#endif
}

void unlock_udomain(udomain_t* _d, str* via_host, unsigned short via_port, unsigned short via_proto)
{
	unsigned int sl;
	sl = get_hash_slot(_d, via_host, via_port, via_proto);
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

//TODO: this should be removed...
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

/**
 * assume locked before calling this - lock the contact slot...
 * @param _d
 * @param _ci
 * @param _c
 * @return 
 */
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
			}
			shm_free(_c->service_routes);
			_c->service_routes=0;
			_c->num_service_routes=0;
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

	//update Rx reg session information
	if (_ci->rx_regsession_id && _ci->rx_regsession_id->len>0 && _ci->rx_regsession_id->s) {
		if (_c->rx_session_id.len > 0 && _c->rx_session_id.s) {
			_c->rx_session_id.len = 0;
			shm_free(_c->rx_session_id.s);
		}
		_c->rx_session_id.s = shm_malloc(_ci->rx_regsession_id->len);
		if (!_c->rx_session_id.s) {
			LM_ERR("no more shm_mem\n");
			return -1;
		}
		memcpy(_c->rx_session_id.s, _ci->rx_regsession_id->s, _ci->rx_regsession_id->len);
		_c->rx_session_id.len = _ci->rx_regsession_id->len;
	}

	//TODO: update path, etc

	if (((db_mode == WRITE_THROUGH) || (db_mode == DB_ONLY)) && (db_update_pcontact(_c) != 0)){
	        LM_ERR("Error updating record in DB");
	        return -1;
	}

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

	if (((db_mode == WRITE_THROUGH) || (db_mode == DB_ONLY)) && db_insert_pcontact(*_c) != 0) {
		LM_ERR("error inserting contact into db");
		goto error;
	}

    return 0;

error:
    return -1;
}

/*
 * search for P-CSCF contact in usrloc
 * @udomain_t* _d - domain to search in
 * @str* _contact - contact to search for - should be a SIP URI
 * @struct pcontact** _c - contact to return to if found (null if not found)
 * @int reverse_search - reverse search for a contact in the memory
 * @return 0 if found <>0 if not
 */
int get_pcontact_from_cache(udomain_t* _d, pcontact_info_t* contact_info, struct pcontact** _c, int reverse_search) {
	unsigned int sl, i, j, aorhash, params_len, has_rinstance=0;
	struct pcontact* c;
	struct sip_uri needle_uri;
	int serviceroutematch;
	char *params, *sep;
	str rinstance = {0, 0};

	LM_DBG("Searching for contact with AOR [%.*s] in P-CSCF usrloc based on VIA [%d://%.*s:%d]"
			" Received [%d://%.*s:%d], Search flag is %d, reverse_search %d\n",
		contact_info->aor.len, contact_info->aor.s, contact_info->via_prot, contact_info->via_host.len,
		contact_info->via_host.s, contact_info->via_port,
		contact_info->received_proto, contact_info->received_host.len, contact_info->received_host.s,
		contact_info->received_port, contact_info->searchflag,
		reverse_search);

	/* parse the uri in the NOTIFY */
	if (contact_info->aor.len>0 && contact_info->aor.s){
		LM_DBG("Have an AOR to search for\n");
		if (parse_uri(contact_info->aor.s, contact_info->aor.len, &needle_uri) != 0) {
			LM_ERR("Unable to parse contact aor in get_pcontact [%.*s]\n", contact_info->aor.len, contact_info->aor.s);
			return 1;
		}
		LM_DBG("checking for rinstance");
		/*check for alias - NAT */
		params = needle_uri.sip_params.s;
		params_len = needle_uri.sip_params.len;

		while (params_len >= RINSTANCE_LEN) {
			if (strncmp(params, RINSTANCE, RINSTANCE_LEN) == 0) {
				has_rinstance = 1;
				break;
			}
			sep = memchr(params, 59 /* ; */, params_len);
			if (sep == NULL) {
				LM_DBG("no rinstance param\n");
				break;
			} else {
				params_len = params_len - (sep - params + 1);
				params = sep + 1;
			}
		}
		if (has_rinstance) {
			rinstance.s = params + RINSTANCE_LEN;
			rinstance.len = params_len - RINSTANCE_LEN;
			sep = (char*)memchr(rinstance.s, 59 /* ; */, rinstance.len);
			if (sep != NULL){
				rinstance.len = (sep-rinstance.s);
			}
			LM_DBG("rinstance found [%.*s]\n", rinstance.len, rinstance.s);
		}
	}
             
    
	/* search in cache */
	aorhash = get_aor_hash(_d, &contact_info->via_host, contact_info->via_port, contact_info->via_prot);
	sl = aorhash & (_d->size - 1);
        
	LM_DBG("get_pcontact slot is [%d]\n", sl);
	c = reverse_search ? _d->table[sl].last : _d->table[sl].first;

	for (i = 0; i < _d->table[sl].n; i++) {
		LM_DBG("comparing contact with aorhash [%u], aor [%.*s]\n", c->aorhash, c->aor.len, c->aor.s);
		LM_DBG("  contact host [%.*s:%d]\n", c->contact_host.len, c->contact_host.s, c->contact_port);
		LM_DBG("contact received [%d:%.*s:%d]\n", c->received_proto, c->received_host.len, c->received_host.s, c->received_port);

		if(c->aorhash == aorhash){
			int check1_passed = 0;
			int check2_passed = 0;
			ip_addr_t c_ip_addr;
			ip_addr_t ci_ip_addr;
			LM_DBG("mached a record by aorhash: %u\n", aorhash);

			// convert 'contact->contact host' ip string to ip_addr_t
			if (str2ipxbuf(&c->contact_host, &c_ip_addr) < 0){
				LM_ERR("Unable to convert c->contact_host [%.*s]\n", c->contact_host.len, c->contact_host.s);
				return 1;
			}

			// convert 'contact info->via host' ip string to ip_addr_t
			if(str2ipxbuf(&contact_info->via_host, &ci_ip_addr) < 0){
				LM_ERR("Unable to convert contact_info->via_host [%.*s]\n", contact_info->via_host.len, contact_info->via_host.s);
				return 1;
			}

			// compare 'contact->contact host' and 'contact info->via host'
			if(ip_addr_cmp(&c_ip_addr, &ci_ip_addr) &&
				(c->contact_port == contact_info->via_port) &&
				!(contact_info->searchflag & SEARCH_RECEIVED))
			{
				LM_DBG("matched contact ip address and port\n");
				check1_passed = 1;
			}

			if(contact_info->searchflag & SEARCH_RECEIVED){
				LM_DBG("continuing to match on received details\n");
				// convert 'contact->received host' ip string to ip_addr_t
				if (str2ipxbuf(&c->received_host, &c_ip_addr) < 0){
					LM_ERR("Unable to convert c->received_host [%.*s]\n", c->received_host.len, c->received_host.s);
					return 1;
				}

				// convert 'contact info->received host' ip string to ip_addr_t
				if(str2ipxbuf(&contact_info->received_host, &ci_ip_addr) < 0){
					LM_ERR("Unable to convert contact_info->received_host [%.*s]\n",
							contact_info->received_host.len, contact_info->received_host.s);
					return 1;
				}

				// compare 'contact->received host' and 'contact info->received host'
				if(ip_addr_cmp(&c_ip_addr, &ci_ip_addr) &&
					((c->received_port == contact_info->received_port) ||
					 (c->contact_port == contact_info->received_port))){ /*volte comes from a different port.... typically uses 4060*/
					check2_passed = 1;
				}
			}

			if(check1_passed || check2_passed){
				LM_DBG("found contact with URI [%.*s]\n", c->aor.len, c->aor.s);
				if (has_rinstance) {
					LM_DBG("confirming rinstance is the same - search has [%.*s] and proposed found contact has [%.*s]",
							rinstance.len, rinstance.s,
							c->rinstance.len, c->rinstance.s);
 		            if ((rinstance.len != c->rinstance.len) || (memcmp(rinstance.s, c->rinstance.s, rinstance.len) != 0) ) {
						LM_DBG("rinstance does not match - no match here...\n");
						c = reverse_search ? c->prev : c->next;
						continue;
					}
				}
                if ((contact_info->aor.len>0) && (needle_uri.user.len != 0)){
                   if ((needle_uri.user.len != c->contact_user.len) ||
                       (memcmp(needle_uri.user.s, c->contact_user.s, needle_uri.user.len) != 0)) {
                                   LM_ERR("user name does not match - no match here...\n");
                                   LM_DBG("found pcontact username [%d]: [%.*s]\n", i, c->contact_user.len, c->contact_user.s);
                                   LM_DBG("incoming contact username: [%.*s]\n", needle_uri.user.len, needle_uri.user.s);
                                   c = c->next;
                                   continue;
                   }
                   if ((contact_info->aor.len >= 4) && (memcmp(contact_info->aor.s, c->aor.s, 4) != 0)) {   // do not mix up sip- and tel-URIs.
                                LM_ERR("scheme does not match - no match here...\n");
                                LM_DBG("found pcontact scheme [%d]: [%.*s]\n", i, 4, c->aor.s);
                                LM_DBG("incoming contact scheme: [%.*s]\n", 4, contact_info->aor.s);
                                c = c->next;
                                continue;
                   }
                }
                else{
                    LM_DBG("No user name present - abort user name check\n");
                }
				
			
				if ((contact_info->extra_search_criteria & SEARCH_SERVICE_ROUTES) && contact_info->num_service_routes > 0) {
					LM_DBG("have %d service routes to search for\n", contact_info->num_service_routes);
					if (contact_info->num_service_routes != c->num_service_routes) {
						c = reverse_search ? c->prev : c->next;
						LM_DBG("number of service routes do not match - failing\n");
						continue;
					} 
				
					serviceroutematch = 1;
					for (j=0; j<contact_info->num_service_routes; j++) {
						if (contact_info->service_routes[j].len != c->service_routes[j].len
								|| memcmp(contact_info->service_routes[j].s, c->service_routes[j].s, c->service_routes[j].len) != 0) {
							LM_DBG("service route at position %d does not match - looking for [%.*s] and contact has [%.*s]..."
									" continuing to next contact check\n",
							    j,
								contact_info->service_routes[j].len, contact_info->service_routes[j].s,
								c->service_routes[j].len, c->service_routes[j].s);
							serviceroutematch = 0;
							break;
						}
					}
					if (serviceroutematch == 0) {
						c = reverse_search ? c->prev : c->next;
						continue;
					}
				}
			
				//finally check state being searched for
				if ( (contact_info->reg_state != PCONTACT_ANY) && ((contact_info->reg_state & c->reg_state) == 0)) {
					LM_DBG("can't find contact for requested reg state [%d] - (have [%d])\n", contact_info->reg_state, c->reg_state);
					c = reverse_search ? c->prev : c->next;
					continue;
				}
				LM_DBG("contact found in memory\n");
				*_c = c;
				return 0;
			}
		}
		c = reverse_search ? c->prev : c->next;
	}
        
	LM_DBG("contact not found in memory\n");
	// Default: Not found.
	*_c = NULL;
        
	return 1; /* Nothing found */
}

int update_security(udomain_t* _d, security_type _t, security_t* _s, struct pcontact* _c) {
	if (((db_mode == WRITE_THROUGH) || (db_mode == DB_ONLY)) && db_update_pcontact_security(_c, _t, _s) != 0) {
		LM_ERR("Error updating security for contact in DB\n");
		return -1;
	}
	_c->security = _s;
	return 0;
}

int update_temp_security(udomain_t* _d, security_type _t, security_t* _s, struct pcontact* _c) {
	if (((db_mode == WRITE_THROUGH) || (db_mode == DB_ONLY)) && db_update_pcontact_security_temp(_c, _t, _s) != 0) {
		LM_ERR("Error updating temp security for contact in DB\n");
		return -1;
	}
	_c->security_temp = _s;
	return 0;
}


int assert_identity(udomain_t* _d, str * _host, unsigned short _port, unsigned short _proto, str * _identity) {
	int i;
	struct pcontact* c;
	// Public identities of this contact
	struct ppublic * p;

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
						for (p = c->head; p; p = p->next) {
							LM_DBG("Public identity: %.*s\n", p->public_identity.len, p->public_identity.s);
							/* Check length: */
							if (_identity->len == p->public_identity.len) {
								/* Check contents: */
								if (strncasecmp(_identity->s, p->public_identity.s, _identity->len) == 0) {
									LM_DBG("Match!\n");
									return 1;
								}
							} else LM_DBG("Length does not match.\n");
						}
					}
				}
			}
			c = c->next;
		}
	}
	return 0; /* Nothing found */
}

int delete_pcontact(udomain_t* _d, /*str* _aor, str* _received_host, int _received_port,*/ struct pcontact* _c)
{
	if (_c==0) {
		return 0;
	}

	if (exists_ulcb_type(PCSCF_CONTACT_DELETE)) {
		run_ul_callbacks(PCSCF_CONTACT_DELETE, _c);
	}
	if (((db_mode == WRITE_THROUGH) || (db_mode == DB_ONLY)) && db_delete_pcontact(_c) != 0) {
		LM_ERR("Error deleting contact from DB");
		return -1;
	}

	mem_delete_pcontact(_d, _c);

	return 0;
}

int unreg_pending_contacts_cb(udomain_t* _d, pcontact_t* _c, int type)
{
	pcontact_t*		c;
	pcontact_info_t	contact_info;
	unsigned int	aorhash, sl, i;

	contact_info.via_host = _c->via_host;
	contact_info.via_port = SIP_PORT;
	contact_info.via_prot = _c->via_proto;
	contact_info.reg_state = PCONTACT_ANY;

	LM_DBG("Searching for contact in P-CSCF usrloc based on VIA [%d://%.*s:%d], reg state 0x%02X\n",
			contact_info.via_prot, contact_info.via_host.len, contact_info.via_host.s, contact_info.via_port, contact_info.reg_state);
	
	aorhash = get_aor_hash(_d, &contact_info.via_host, contact_info.via_port, contact_info.via_prot);
	sl = aorhash & (_d->size - 1);
        
    LM_DBG("get_pcontact slot is [%d]\n", sl);
	c = _d->table[sl].first;

	for(i = 0; i < _d->table[sl].n; i++){
		LM_DBG("comparing contact with aorhash [%u], aor [%.*s]\n", c->aorhash, c->aor.len, c->aor.s);
		LM_DBG("contact host [%.*s:%d]\n", c->contact_host.len, c->contact_host.s, c->contact_port);

		if(c->aorhash == aorhash){
			ip_addr_t c_ip_addr;
			ip_addr_t ci_ip_addr;

			// convert 'contact->contact host' ip string to ip_addr_t
			if (str2ipxbuf(&c->contact_host, &c_ip_addr) < 0){
				LM_ERR("Unable to convert c->contact_host [%.*s]\n", c->contact_host.len, c->contact_host.s);
				return 1;
			}

			// convert 'contact info->via host' ip string to ip_addr_t
			if(str2ipxbuf(&contact_info.via_host, &ci_ip_addr) < 0){
				LM_ERR("Unable to convert contact_info.via_host [%.*s]\n", contact_info.via_host.len, contact_info.via_host.s);
				return 1;
			}

			// compare 'contact->contact host' and 'contact info->via host'
			if(ip_addr_cmp(&c_ip_addr, &ci_ip_addr) && (c->contact_port == contact_info.via_port)){
				LM_DBG("found contact with URI [%.*s]\n", c->aor.len, c->aor.s);

				// finally check state being searched for
				if((contact_info.reg_state != PCONTACT_ANY) && ((contact_info.reg_state & c->reg_state) == 0)){
					LM_DBG("can't find contact for requested reg state [%d] - (have [%d])\n", contact_info.reg_state, c->reg_state);
					c = c->next;
					continue;
				}

				// check for equal ipsec parameters
				if(c->security_temp == NULL || _c->security_temp == NULL){
					LM_DBG("Invalid temp security\n");
					c = c->next;
					continue;
				}

				if(c->security_temp->type != SECURITY_IPSEC){
					LM_DBG("Invalid temp security type\n");
					c = c->next;
					continue;
				}

				if(c->security_temp->data.ipsec == NULL || _c->security_temp->data.ipsec == NULL){
					LM_DBG("Invalid ipsec\n");
					c = c->next;
					continue;
				}

				LM_DBG("=========== c->reg_state 0x%02X, %u-%u | %u-%u | %u-%u | %u-%u | %u-%u | %u-%u | %u-%u | %u-%u |",
					c->reg_state,
					c->security_temp->data.ipsec->port_pc, _c->security_temp->data.ipsec->port_pc,
				    c->security_temp->data.ipsec->port_ps, _c->security_temp->data.ipsec->port_ps,
				    c->security_temp->data.ipsec->port_uc, _c->security_temp->data.ipsec->port_uc,
				    c->security_temp->data.ipsec->port_us, _c->security_temp->data.ipsec->port_us,
				    c->security_temp->data.ipsec->spi_pc, _c->security_temp->data.ipsec->spi_pc,
				    c->security_temp->data.ipsec->spi_ps, _c->security_temp->data.ipsec->spi_ps,
				    c->security_temp->data.ipsec->spi_uc, _c->security_temp->data.ipsec->spi_uc,
				    c->security_temp->data.ipsec->spi_us, _c->security_temp->data.ipsec->spi_us);

				if(c->security_temp->data.ipsec->port_pc == _c->security_temp->data.ipsec->port_pc &&
				   c->security_temp->data.ipsec->port_ps == _c->security_temp->data.ipsec->port_ps &&
				   c->security_temp->data.ipsec->port_uc == _c->security_temp->data.ipsec->port_uc &&
				   c->security_temp->data.ipsec->port_us == _c->security_temp->data.ipsec->port_us &&
				   c->security_temp->data.ipsec->spi_pc == _c->security_temp->data.ipsec->spi_pc &&
				   c->security_temp->data.ipsec->spi_ps == _c->security_temp->data.ipsec->spi_ps &&
				   c->security_temp->data.ipsec->spi_uc == _c->security_temp->data.ipsec->spi_uc &&
				   c->security_temp->data.ipsec->spi_us == _c->security_temp->data.ipsec->spi_us){
					// deregister user callback only for contacts with exact sec parameters like registerd contact
					delete_ulcb(c, type);
				}
			}
		}
		c = c->next;
	}

	return 0;
}

/*!
 * \brief Convert database values into pcontact_info
 *
 * Convert database values into pcontact_info,
 * expects 15 rows (aor, host, port, protocol, received, received_port, received_proto, rx_session_id_col
 * reg_state, expires, socket, service_routes_col, public_ids, path
 * \param vals database values
 * \param contact contact
 * \return pointer to the ucontact_info on success, 0 on failure
 */
static inline pcontact_info_t* dbrow2info( db_val_t *vals, str *contact)
{
	static pcontact_info_t ci;
	static str host, received, path, rx_session_id, implicit_impus, tmpstr, service_routes;
	static str *impu_list, *service_route_list;
	int flag=0, n;
	char *p, *q=0;

	memset( &ci, 0, sizeof(pcontact_info_t));

	host.s = (char*) VAL_STRING(vals + 1);
	if (VAL_NULL(vals+1) || !host.s || !host.s[0]) {
		host.len = 0;
		host.s = 0;
	} else {
		host.len = strlen(host.s);
	}
	ci.via_host = host;
	ci.via_port = VAL_INT(vals + 2);
	ci.via_prot = VAL_INT(vals + 3);
	received.s = (char*) VAL_STRING(vals + 4);
	if (VAL_NULL(vals+4) || !received.s || !received.s[0]) {
		LM_DBG("Empty received for contact [%.*s]....\n", contact->len, contact->s);	/*this could happen if you have been notified about a contact from S-CSCF*/
		received.len = 0;
		received.s = 0;
	} else {
		received.len = strlen(received.s);
	}
	ci.received_host = received;
	ci.received_port = VAL_INT(vals + 5);
	ci.received_proto = VAL_INT(vals + 6);

	rx_session_id.s = (char*) VAL_STRING(vals + 7);
	if (VAL_NULL(vals+7) || !rx_session_id.s || !rx_session_id.s[0]) {
		rx_session_id.len = 0;
		rx_session_id.s = 0;
	} else {
		rx_session_id.len = strlen(rx_session_id.s);
	}
	ci.rx_regsession_id = &rx_session_id;
	if (VAL_NULL(vals + 8)) {
		LM_ERR("empty registration state in DB\n");
		return 0;
	}
	ci.reg_state = VAL_INT(vals + 8);
	
        if (VAL_NULL(vals + 9)) {
		LM_ERR("empty expire\n");
		return 0;
	}
	ci.expires = VAL_TIME(vals + 9);
	path.s  = (char*)VAL_STRING(vals+13);
		if (VAL_NULL(vals+13) || !path.s || !path.s[0]) {
			path.len = 0;
			path.s = 0;
		} else {
			path.len = strlen(path.s);
		}
	ci.path = &path;

	//public IDs - implicit set
	implicit_impus.s = (char*) VAL_STRING(vals + 12);
	if (!VAL_NULL(vals + 12) && implicit_impus.s && implicit_impus.s[0]) {
		//how many
		n=0;
		p = implicit_impus.s;
		while (*p) {
			if ((*p) == '<') {
				n++;
			}
			p++;
		}
		impu_list = pkg_malloc(sizeof(str) * n);

		n=0;
		p = implicit_impus.s;
		while (*p) {
			if (*p == '<') {
				q = p + 1;
				flag = 1;
			}
			if (*p == '>') {
				if (flag) {
					tmpstr.s = q;
					tmpstr.len = p - q;
					impu_list[n++] = tmpstr;
				}
				flag = 0;
			}
			p++;
		}
		ci.num_public_ids = n;
		ci.public_ids = impu_list;
	}

	//service routes
	service_routes.s = (char*) VAL_STRING(vals + 11);
	if (!VAL_NULL(vals + 11) && service_routes.s && service_routes.s[0]) {
		//how many
		n = 0;
		p = service_routes.s;
		while (*p) {
			if ((*p) == '<') {
				n++;
			}
			p++;
		}
		service_route_list = pkg_malloc(sizeof(str) * n);

		n = 0;
		p = service_routes.s;
		while (*p) {
			if (*p == '<') {
				q = p + 1;
				flag = 1;
			}
			if (*p == '>') {
				if (flag) {
					tmpstr.s = q;
					tmpstr.len = p - q;
					service_route_list[n++] = tmpstr;
				}
				flag = 0;
			}
			p++;
		}
		ci.num_service_routes = n;
		ci.service_routes = service_route_list;
	}

	return &ci;
}

/*!
 * \brief Load all records from a udomain
 *
 * Load all records from a udomain, useful to populate the
 * memory cache on startup.
 * \param _c database connection
 * \param _d loaded domain
 * \return 0 on success, -1 on failure
 */
int preload_udomain(db1_con_t* _c, udomain_t* _d)
{
	pcontact_info_t *ci;
	db_row_t *row;
	db_key_t columns[15];
	db1_res_t* res = NULL;
	str aor;
	int i, n;

	pcontact_t* c;

	LM_DBG("pre-loading domain from DB\n");

	columns[0] = &domain_col;
	columns[1] = &aor_col;
	columns[2] = &host_col;
	columns[3] = &port_col;
	columns[4] = &protocol_col;
	columns[5] = &received_col;
	columns[6] = &received_port_col;
	columns[7] = &received_proto_col;
	columns[8] = &rx_session_id_col;
	columns[9] = &reg_state_col;
	columns[10] = &expires_col;
	columns[11] = &socket_col;
	columns[12] = &service_routes_col;
	columns[13] = &public_ids_col;
	columns[14] = &path_col;

	if (ul_dbf.use_table(_c, _d->name) < 0) {
		LM_ERR("sql use_table failed\n");
		return -1;
	}

#ifdef EXTRA_DEBUG
	LM_NOTICE("load start time [%d]\n", (int)time(NULL));
#endif

	if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, 15, 0, 0) < 0) {
			LM_ERR("db_query (1) failed\n");
			return -1;
		}
		if(ul_dbf.fetch_result(_c, &res, ul_fetch_rows)<0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, 15, 0, &res) < 0) {
			LM_ERR("db_query failed\n");
			return -1;
		}
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("table is empty\n");
		ul_dbf.free_result(_c, res);
		return 0;
	}

	LM_DBG("%d rows returned in preload\n", RES_ROW_N(res));

	n = 0;
	do {
		LM_DBG("loading records - cycle [%d]\n", ++n);
		for(i = 0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;

			aor.s = (char*) VAL_STRING(ROW_VALUES(row) + 1);
			if (VAL_NULL(ROW_VALUES(row) + 1) || aor.s == 0 || aor.s[0] == 0) {
				LM_CRIT("empty aor record in table %s...skipping\n", _d->name->s);
				continue;
			}
			aor.len = strlen(aor.s);
			ci = dbrow2info(ROW_VALUES(row) + 1, &aor);
			if (!ci) {
				LM_WARN("Failed to get contact info from DB.... continuing...\n");
				continue;
			}
			lock_udomain(_d, &ci->via_host, ci->via_port, ci->via_prot);

			if ( (mem_insert_pcontact(_d, &aor, ci, &c)) != 0) {
				LM_ERR("inserting contact failed\n");
				unlock_udomain(_d, &ci->via_host, ci->via_port, ci->via_prot);
				if(ci->public_ids){
					pkg_free(ci->public_ids);
				}
				if(ci->service_routes){
					pkg_free(ci->service_routes);	
				}			
				goto error1;
			}
			//c->flags = c->flags|(1<<FLAG_READFROMDB);
			//TODO: need to subscribe to s-cscf for first public identity
			unlock_udomain(_d, &ci->via_host, ci->via_port, ci->via_prot);
			if(ci->public_ids){
				pkg_free(ci->public_ids);
			}
			if(ci->service_routes){
				pkg_free(ci->service_routes);	
			}			
		}

		if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
			if(ul_dbf.fetch_result(_c, &res, ul_fetch_rows)<0) {
				LM_ERR("fetching rows (1) failed\n");
				ul_dbf.free_result(_c, res);
				return -1;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(res)>0);

	ul_dbf.free_result(_c, res);

#ifdef EXTRA_DEBUG
	LM_NOTICE("load end time [%d]\n", (int)time(NULL));
#endif

	return 0;
error1:
	free_pcontact(c);

	ul_dbf.free_result(_c, res);
	return -1;
}

int db_load_pcontact(udomain_t* _d, str *_aor, int insert_cache, struct pcontact** _c, pcontact_info_t* contact_info)
{
	pcontact_info_t *ci;
        db_key_t columns[15];
        db_key_t keys[2];
        db_val_t vals[2];
        db_op_t op[2];
        db1_res_t* res = NULL;
        db_row_t *row;
        int i;
        str aor, port={0,0};
        pcontact_t* c = NULL;

        keys[0] = &aor_col;
        vals[0].type = DB1_STR;
        vals[0].nul = 0;
        vals[0].val.str_val = *_aor;
        op[0] = OP_EQ;
        op[1] = OP_EQ;


        columns[0] = &domain_col;
        columns[1] = &aor_col;
        columns[2] = &host_col;
        columns[3] = &port_col;
        columns[4] = &protocol_col;
        columns[5] = &received_col;
        columns[6] = &received_port_col;
        columns[7] = &received_proto_col;
        columns[8] = &rx_session_id_col;
        columns[9] = &reg_state_col;
        columns[10] = &expires_col;
        columns[11] = &socket_col;
        columns[12] = &service_routes_col;
        columns[13] = &public_ids_col;
        columns[14] = &path_col;



        if (_aor->len>0 && _aor->s){
                LM_DBG("Querying database for P-CSCF contact [%.*s]\n", _aor->len, _aor->s);
        } else {
                LM_DBG("Querying database for P-CSCF received_host [%.*s] and received_port [%d]\n", contact_info->received_host.len, contact_info->received_host.s, contact_info->received_port);
                keys[0] = &received_col;
                vals[0].type = DB1_STR;
                vals[0].nul = 0;
                vals[0].val.str_val = contact_info->received_host;
                keys[1] = &received_port_col;
                vals[1].type = DB1_INT;
                vals[1].nul = 0;
                port.s = int2str(contact_info->received_port, &port.len);
                vals[1].val.int_val = contact_info->received_port;

        }

        if (ul_dbf.use_table(ul_dbh, _d->name) < 0) {
                LM_ERR("sql use_table failed\n");
                return -1;
        }


        if (ul_dbf.query(ul_dbh, keys, op, vals, columns, port.s ? 2 : 1, 15, 0, &res) < 0) {
                if (!port.s) {
                        LM_ERR("Unable to query DB for location associated with aor [%.*s]\n", _aor->len, _aor->s);
                }
                else {
                        LM_ERR("Unable to query DB for location associated with host [%.*s] and port [%.*s]\n", contact_info->received_host.len, contact_info->received_host.s, port.len, port.s);
                }
                ul_dbf.free_result(ul_dbh, res);
                return 0;
        }
 
	if (RES_ROW_N(res) == 0) {
                if (!port.s) {
                        LM_DBG("aor [%.*s] not found in table %.*s\n",_aor->len, _aor->s, _d->name->len, _d->name->s);
                }
                else {
                        LM_DBG("host [%.*s] and port [%.*s] not found in table %.*s\n", contact_info->received_host.len, contact_info->received_host.s, port.len, port.s, _d->name->len, _d->name->s);
                }

		ul_dbf.free_result(ul_dbh, res);
                return 0;
        }
        LM_DBG("Handling Result for query received\n");

        for(i = 0; i < RES_ROW_N(res); i++) {
                row = RES_ROWS(res) + i;

                aor.s = (char*) VAL_STRING(ROW_VALUES(row) + 1);
                if (VAL_NULL(ROW_VALUES(row) + 1) || aor.s == 0 || aor.s[0] == 0) {
                        LM_ERR("empty aor record in table %s...skipping\n", _d->name->s);
                        continue;
                }
                aor.len = strlen(aor.s);

                if ((_aor->len==0 && !_aor->s) && (VAL_NULL(ROW_VALUES(row) + 5) || VAL_NULL(ROW_VALUES(row) + 6))){
                        LM_ERR("empty received_host or received_port record in table %s...skipping\n", _d->name->s);
                        continue;
                }
                LM_DBG("Convert database values extracted with AOR.");
                ci = dbrow2info(ROW_VALUES(row) + 1, &aor);
                if (!ci) {
                        LM_WARN("Failed to get contact info from DB.... continuing...\n");
                        continue;
                }

                if(!(insert_cache)){
                       (*_c)->expires = ci->expires;
                      ul_dbf.free_result(ul_dbh, res);
					if(ci->public_ids){
						pkg_free(ci->public_ids);
					}
					if(ci->service_routes){
						pkg_free(ci->service_routes);	
					}		
					LM_DBG("Freed memory in db_load_pcontact");
                      return 1;
                }
				if(ci->reg_state==PCONTACT_REGISTERED){
					if ( (mem_insert_pcontact(_d, &aor, ci, &c)) != 0) {
							if(ci->public_ids){
								pkg_free(ci->public_ids);
							}
							if(ci->service_routes){
								pkg_free(ci->service_routes);	
							}			
							LM_ERR("inserting contact failed\n");
							goto error;
					}
				}else {
					if(ci->public_ids){
						pkg_free(ci->public_ids);
					}
					if(ci->service_routes){
						pkg_free(ci->service_routes);	
					}
					LM_ERR("inserting contact failed\n");
					goto error1;
				}
                if (exists_ulcb_type(PCSCF_CONTACT_INSERT)) {
                        run_ul_create_callbacks(c);
                }

                register_ulcb(c, PCSCF_CONTACT_DELETE | PCSCF_CONTACT_EXPIRE | PCSCF_CONTACT_UPDATE, cbp_registrar->callback, NULL);

                if (c->rx_session_id.len > 0){
                        register_ulcb(c, PCSCF_CONTACT_DELETE | PCSCF_CONTACT_EXPIRE, cbp_qos->callback, NULL);
                }
		
		//c->flags = c->flags|(1<<FLAG_READFROMDB);
		//TODO: need to subscribe to s-cscf for first public identity
                
                LM_DBG("inserting contact done\n");
                *_c = c;
				if(ci->public_ids){
					pkg_free(ci->public_ids);
				}
				if(ci->service_routes){
					pkg_free(ci->service_routes);	
				}			

	}

        ul_dbf.free_result(ul_dbh, res);
	return 1;

error:
               free_pcontact(c);
error1:
               ul_dbf.free_result(ul_dbh, res);
               return 0;
}


int get_pcontact(udomain_t* _d, pcontact_info_t* contact_info, struct pcontact** _c, int reverse_search) {

    int ret = get_pcontact_from_cache(_d,  contact_info,  _c, reverse_search);

    if (ret && (db_mode == DB_ONLY)){
            LM_DBG("contact not found in cache for contact_info->received_port [%d]\n", contact_info->received_port);
           if (contact_info->searchflag == SEARCH_RECEIVED){
                   	LM_DBG("Trying contact_info.extra_search_criteria = 0\n");
                        contact_info->extra_search_criteria = 0;
                        ret = get_pcontact_from_cache(_d,  contact_info,  _c, reverse_search);
                        if (ret == 0){
                                return ret;
                        }
                        LM_DBG("contact not found in cache for contact_info->via_port [%d]\n", contact_info->via_port);
                        contact_info->extra_search_criteria = SEARCH_SERVICE_ROUTES;

                        contact_info->searchflag = SEARCH_NORMAL;
                        LM_DBG("Trying contact_info.searchflag = SEARCH_NORMAL\n");
                        ret = get_pcontact_from_cache(_d,  contact_info,  _c, reverse_search);
                        if (ret == 0){
                                 return ret;
                        }
                        else {
                                LM_DBG("Trying contact_info.extra_search_criteria = 0\n");
                                contact_info->extra_search_criteria = 0;
                                ret = get_pcontact_from_cache(_d,  contact_info,  _c, reverse_search);
                                if (ret == 0){
                                         return ret;
                                }
                                LM_DBG("contact not found in cache for contact_info->via_port [%d]\n", contact_info->via_port);
                                contact_info->extra_search_criteria = SEARCH_SERVICE_ROUTES;
                                contact_info->searchflag = SEARCH_RECEIVED;
                        }
           }
                        else {
                        LM_DBG("Trying contact_info.extra_search_criteria = 0\n");
                        contact_info->extra_search_criteria = 0;
                        ret = get_pcontact_from_cache(_d,  contact_info,  _c, reverse_search);
                        if (ret == 0){
                                return ret;
                        }
                        LM_DBG("contact not found in cache for contact_info->via_port [%d]\n", contact_info->via_port);
                        contact_info->extra_search_criteria = SEARCH_SERVICE_ROUTES;
           }
           if (db_load_pcontact(_d, &contact_info->aor, 1/*insert_cache*/, _c, contact_info)){
                   LM_DBG("loaded location from db for  AOR [%.*s]\n", contact_info->aor.len, contact_info->aor.s);
                   return 0;
           } else {
                   LM_DBG("download location DB failed for  AOR [%.*s]\n", contact_info->aor.len, contact_info->aor.s);
                   return 1;
           }
    }

    return ret;

}


int audit_usrloc_expired_pcontacts(udomain_t* _d) {

        db1_res_t* location_rs = NULL;
        pcontact_info_t *ci;
        db_key_t columns[15];
        db_key_t keys[1];
        db_val_t vals[1];
        db_op_t op[1];
        db_row_t *row;
        int i;
        str aor;
        pcontact_t* c = NULL;

        keys[0] = &expires_col;
        vals[0].type = DB1_DATETIME;
        vals[0].nul = 0;
        vals[0].val.time_val = time(0) - expires_grace - audit_expired_pcontacts_timeout;;
        op[0] = OP_LT;


        columns[0] = &domain_col;
        columns[1] = &aor_col;
        columns[2] = &host_col;
        columns[3] = &port_col;
        columns[4] = &protocol_col;
        columns[5] = &received_col;
        columns[6] = &received_port_col;
        columns[7] = &received_proto_col;
        columns[8] = &rx_session_id_col;
        columns[9] = &reg_state_col;
        columns[10] = &expires_col;
        columns[11] = &socket_col;
        columns[12] = &service_routes_col;
        columns[13] = &public_ids_col;
        columns[14] = &path_col;

        if (ul_dbf.use_table(ul_dbh, _d->name) < 0) {
                LM_ERR("sql use_table failed\n");
                return -1;
        }

        if (ul_dbf.query(ul_dbh, keys, op, vals, columns, 1, 15, 0, &location_rs) < 0) {
            LM_ERR("Unable to query DB for expired pcontacts\n");
            ul_dbf.free_result(ul_dbh, location_rs);
        } else {
             if (RES_ROW_N(location_rs) == 0) {
                 LM_DBG("no expired pcontacts found in DB\n");
                 ul_dbf.free_result(ul_dbh, location_rs);
                 goto done;
             }

             for(i = 0; i < RES_ROW_N(location_rs); i++) {
                 row = RES_ROWS(location_rs) + i;

                 aor.s = (char*) VAL_STRING(ROW_VALUES(row) + 1);

                 if (VAL_NULL(ROW_VALUES(row) + 1) || aor.s == 0 || aor.s[0] == 0) {
                     LM_ERR("empty aor record in table %s...skipping\n", _d->name->s);
                     continue;
                 }
                 aor.len = strlen(aor.s);
                 ci = dbrow2info(ROW_VALUES(row) + 1, &aor);
                 if (!ci) {
                     LM_ERR("Failed to get contact info from DB.... continuing...\n");
                     continue;
                 }
                 ci->aor = aor;
                 ci->searchflag = SEARCH_NORMAL;
                 ci->reg_state = PCONTACT_ANY;
                 lock_udomain(_d, &ci->via_host, ci->via_port, ci->via_prot);
                 if (get_pcontact_from_cache(_d, ci, &c, 0) == 0){
                     LM_DBG("found pcontact [%.*s] in cache.....should have been cleared by expiry handler\n", aor.len, aor.s);
                 }
                 else{
                    // insert pcontact
                    if (!(db_load_pcontact(_d, &aor, 1/*insert_cache*/, &c, ci))){                   
                        LM_ERR("could not insert pcontact [%.*s] into cache\n", aor.len, aor.s);
                    }
                 }
                 unlock_udomain(_d, &ci->via_host, ci->via_port, ci->via_prot);
	         if(ci->public_ids){
                     pkg_free(ci->public_ids);
                 }
         	 if(ci->service_routes){
                     pkg_free(ci->service_routes);	
                 }			
             }
             ul_dbf.free_result(ul_dbh, location_rs);
        }
done:
   return 0;
}

