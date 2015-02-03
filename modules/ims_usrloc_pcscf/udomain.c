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
#include "usrloc_db.h"
#include "../../parser/parse_uri.h"

#include "../../lib/ims/useful_defs.h"

extern int db_mode;
extern unsigned int hashing_type;
extern int lookup_check_received;
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
	(*_c)->sl = sl;
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

void lock_udomain(udomain_t* _d, str* _aor, str* _received_host, unsigned short received_port)
{
	unsigned int sl;
	
	sl = get_hash_slot(_d, _aor, _received_host, received_port);

#ifdef GEN_LOCK_T_PREFERED
	lock_get(_d->table[sl].lock);
#else
	ul_lock_idx(_d->table[sl].lockidx);
#endif
}

void unlock_udomain(udomain_t* _d, str* _aor, str* _received_host, unsigned short received_port)
{
	unsigned int sl;
	sl = get_hash_slot(_d, _aor, _received_host, received_port);
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

	// update received info (if info is available):
	if (_ci->received_host.len > 0) {
		if (_c->received_host.s)
			shm_free(_c->received_host.s);
		STR_SHM_DUP(_c->received_host, _ci->received_host, "update_pcontact");
	}
	if (_ci->received_port > 0) _c->received_port = _ci->received_port;
	if (_ci->received_proto > 0) _c->received_proto = _ci->received_proto;

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

	if (db_mode == WRITE_THROUGH && db_update_pcontact(_c) != 0) {
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

	if (db_mode == WRITE_THROUGH && db_insert_pcontact(*_c) != 0) {
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
 * @struct pontact** _c - contact to return to if found (null if not found)
 * @return 0 if found <>0 if not
 */
int get_pcontact(udomain_t* _d, str* _contact, str* _received_host, int received_port, struct pcontact** _c) {
	unsigned int sl, i, aorhash;
	struct pcontact* c;
	ppublic_t* impu;
	struct sip_uri needle_uri, impu_uri;
	int port_match = 0;
	str alias_host = {0, 0};
	struct sip_uri contact_uri;
	
	if (parse_uri(_contact->s, _contact->len, &needle_uri) != 0 ) {
		LM_ERR("failed to parse search URI [%.*s]\n", _contact->len, _contact->s);
		return 1;
	}

	LM_DBG("Searching for contact in P-CSCF usrloc [%.*s]\n",
				_contact->len,
				_contact->s);
	
	/* search in cache */
	aorhash = get_aor_hash(_d, _contact, _received_host, received_port);
	sl = aorhash & (_d->size - 1);
	c = _d->table[sl].first;

	for (i = 0; i < _d->table[sl].n; i++) {

		if ((c->aorhash == aorhash) && (c->aor.len == _contact->len)
				&& !memcmp(c->aor.s, _contact->s, _contact->len)) {
			*_c = c;
			return 0;
		}
		
		if(match_contact_host_port) {
		    LM_DBG("Comparing needle user@host:port [%.*s@%.*s:%d] and contact_user@contact_host:port [%.*s@%.*s:%d]\n", needle_uri.user.len, needle_uri.user.s, 
			    needle_uri.host.len, needle_uri.host.s, needle_uri.port_no,
			    c->contact_user.len, c->contact_user.s, c->contact_host.len, c->contact_host.s, c->contact_port);

		    if((needle_uri.user.len == c->contact_user.len && (memcmp(needle_uri.user.s, c->contact_user.s, needle_uri.user.len) ==0)) &&
			    (needle_uri.host.len == c->contact_host.len && (memcmp(needle_uri.host.s, c->contact_host.s, needle_uri.host.len) ==0)) &&
			    (needle_uri.port_no == c->contact_port)) {
			LM_DBG("Match!!\n");
			*_c = c;
			return 0;
		    }
		}
		
		LM_DBG("Searching for [%.*s] and comparing to [%.*s]\n", _contact->len, _contact->s, c->aor.len, c->aor.s);

		/* hosts HAVE to match */
		if (lookup_check_received && ((needle_uri.host.len != c->received_host.len) || (memcmp(needle_uri.host.s, c->received_host.s, needle_uri.host.len)!=0))) {
			//can't possibly match
			LM_DBG("Lookup failed for [%.*s <=> %.*s]\n", needle_uri.host.len, needle_uri.host.s, c->received_host.len, c->received_host.s);
			c = c->next;
			continue;
		}

		/* one of the ports must match, either the initial registered port, the received port, or one if the security ports (server) */
		if ((needle_uri.port_no != c->contact_port) && (needle_uri.port_no != c->received_port)) {
			//check security ports
			if (c->security) {
				switch (c->security->type) {
				case SECURITY_IPSEC: {
					ipsec_t* ipsec = c->security->data.ipsec;
					if (ipsec) {
						LM_DBG("security server port is %d\n", ipsec->port_us);
						LM_DBG("security client port is %d\n", ipsec->port_uc);
						if (ipsec->port_us == needle_uri.port_no) {
							LM_DBG("security port mathes contact\n");
							port_match = 1;
						}
					}
					break;
				}
				case SECURITY_TLS:
				case SECURITY_NONE:
					LM_WARN("not implemented\n");
					break;
				}
			}
			if (!port_match && c->security_temp) {
				switch (c->security_temp->type) {
				case SECURITY_IPSEC: {
					ipsec_t* ipsec = c->security_temp->data.ipsec;
					if (ipsec) {
						LM_DBG("temp security server port is %d\n", ipsec->port_us);
						LM_DBG("temp security client port is %d\n", ipsec->port_uc);
						if (ipsec->port_us == needle_uri.port_no) {
							LM_DBG("temp security port mathes contact\n");
							port_match = 1;
						}
					}
					break;
				}
				case SECURITY_TLS:
				case SECURITY_NONE:
					LM_WARN("not implemented\n");
					break;
				}
			}
		} else {
			port_match = 1;
		}

		if (!port_match){
			LM_DBG("Port don't match: %d (contact) %d (received) != %d!\n",
		c->contact_port, c->received_port, needle_uri.port_no);
			c = c->next;
			continue;
		}

		/* user parts must match (if not wildcarded) with either primary contact OR with any userpart in the implicit set (associated URIs).. */
		if((needle_uri.user.len == c->contact_user.len) && (memcmp(needle_uri.user.s, c->contact_user.s,needle_uri.user.len) == 0)) {
		    LM_DBG("Needle user part matches contact user part therefore this is a match\n");
		    *_c = c;
		    return 0;
		} else if ((needle_uri.user.len == 1) && (memcmp(needle_uri.user.s, "*", 1) == 0)) { /*wild card*/
		    LM_DBG("This a wild card user part - we must check if hosts match or needle host matches alias\n");
		    if(memcmp(needle_uri.host.s, c->contact_host.s, needle_uri.host.len) == 0) {
			LM_DBG("Needle host matches contact host therefore this is a match\n");
			*_c = c;
			return 0;
		    } else if ((parse_uri(c->aor.s, c->aor.len, &contact_uri) == 0) && ((get_alias_host_from_contact(&contact_uri.params, &alias_host)) == 0) &&
			    (memcmp(needle_uri.host.s, alias_host.s, alias_host.len) == 0)) {
			LM_DBG("Needle host matches contact alias therefore this is a match\n");
			*_c = c;
			return 0;
			
		    }
		}

		/* check impus user parts */
		impu = c->head;
		while (impu) {
			if (parse_uri(impu->public_identity.s, impu->public_identity.len, &impu_uri) != 0) {
				LM_ERR("failed to parse IMPU URI [%.*s]...continuing\n", impu->public_identity.len, impu->public_identity.s);
				impu = impu->next;
				continue;
			}
			LM_DBG("comparing first %d chars of impu [%.*s] for contact userpart [%.*s]\n",
					needle_uri.user.len,
					impu->public_identity.len, impu->public_identity.s,
					needle_uri.user.len, needle_uri.user.s);
			if (needle_uri.user.len == impu_uri.user.len && (memcmp(needle_uri.user.s, impu_uri.user.s, impu_uri.user.len)==0)) {
				*_c = c;
				return 0;
			}
			impu = impu->next;
		}

		c = c->next;
	}
	return 1; /* Nothing found */
}

int get_pcontact_by_src(udomain_t* _d, str * _host, unsigned short _port, unsigned short _proto, struct pcontact** _c) {
	char c_contact[256], *p;
	str s_contact;
	int ret;

	memset(c_contact, 0, 256);
	strncpy(c_contact, "sip:*@", 6);	//prepend *@ to host to wildcard on user search
	p = c_contact + 6;
	memcpy(p, _host->s, _host->len);
	p = p + _host->len;
	*p = ':';
	p++;
	sprintf(p, "%d", _port);
	s_contact.s = c_contact;
	s_contact.len = strlen(c_contact);
	
	LM_DBG("Trying to find contact by src with URI: [%.*s]\n", s_contact.len, s_contact.s);
	ret = get_pcontact(_d, &s_contact, _host, _port, _c);

	return ret;
}

int update_security(udomain_t* _d, security_type _t, security_t* _s, struct pcontact* _c) {
	if (db_mode == WRITE_THROUGH && db_update_pcontact_security(_c, _t, _s) != 0) {
		LM_ERR("Error updating security for contact in DB\n");
		return -1;
	}
	_c->security = _s;
	return 0;
}

int update_temp_security(udomain_t* _d, security_type _t, security_t* _s, struct pcontact* _c) {
	if (db_mode == WRITE_THROUGH && db_update_pcontact_security_temp(_c, _t, _s) != 0) {
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

int delete_pcontact(udomain_t* _d, str* _aor, str* _received_host, int _received_port, struct pcontact* _c)
{
	if (_c==0) {
		if (get_pcontact(_d, _aor, _received_host, _received_port, &_c) > 0) {
			return 0;
		}
	}

	if (exists_ulcb_type(PCSCF_CONTACT_DELETE)) {
		run_ul_callbacks(PCSCF_CONTACT_DELETE, _c);
	}

	if (db_mode == WRITE_THROUGH && db_delete_pcontact(_c) != 0) {
		LM_ERR("Error deleting contact from DB");
		return -1;
	}

	mem_delete_pcontact(_d, _c);

	return 0;
}

/*!
 * \brief Convert database values into pcontact_info
 *
 * Convert database values into pcontact_info,
 * expects 12 rows (aor, contact, received, received_port, received_proto, rx_session_id_col
 * reg_state, expires, socket, service_routes_col, public_ids, path
 * \param vals database values
 * \param contact contact
 * \return pointer to the ucontact_info on success, 0 on failure
 */
static inline pcontact_info_t* dbrow2info( db_val_t *vals, str *contact)
{
	static pcontact_info_t ci;
	static str received, path, rx_session_id, implicit_impus, tmpstr, service_routes;
	static str *impu_list, *service_route_list;
	int flag=0, n;
	char *p, *q=0;

	memset( &ci, 0, sizeof(pcontact_info_t));

	received.s = (char*) VAL_STRING(vals + 2);
	if (VAL_NULL(vals+2) || !received.s || !received.s[0]) {
		received.len = 0;
		received.s = 0;
	} else {
		received.len = strlen(received.s);
	}
	ci.received_host = received;
	ci.received_port = VAL_INT(vals + 3);
	ci.received_proto = VAL_INT(vals + 4);

	rx_session_id.s = (char*) VAL_STRING(vals + 5);
	if (VAL_NULL(vals+5) || !rx_session_id.s || !rx_session_id.s[0]) {
		rx_session_id.len = 0;
		rx_session_id.s = 0;
	} else {
		rx_session_id.len = strlen(rx_session_id.s);
	}
	ci.rx_regsession_id = &rx_session_id;
	if (VAL_NULL(vals + 6)) {
		LM_ERR("empty registration state in DB\n");
		return 0;
	}
	ci.reg_state = VAL_INT(vals + 6);
	if (VAL_NULL(vals + 7)) {
		LM_ERR("empty expire\n");
		return 0;
	}
	ci.expires = VAL_TIME(vals + 7);
	path.s  = (char*)VAL_STRING(vals+11);
		if (VAL_NULL(vals+11) || !path.s || !path.s[0]) {
			path.len = 0;
			path.s = 0;
		} else {
			path.len = strlen(path.s);
		}
	ci.path = &path;

	//public IDs - implicit set
	implicit_impus.s = (char*) VAL_STRING(vals + 10);
	if (!VAL_NULL(vals + 10) && implicit_impus.s && implicit_impus.s[0]) {
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
	service_routes.s = (char*) VAL_STRING(vals + 9);
	if (!VAL_NULL(vals + 9) && service_routes.s && service_routes.s[0]) {
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
	db_key_t columns[18];
	db1_res_t* res = NULL;
	str aor, contact;
	int i, n;

	pcontact_t* c;

	LM_DBG("pre-loading domain from DB\n");

	columns[0] = &domain_col;
	columns[1] = &aor_col;
	columns[2] = &contact_col;
	columns[3] = &received_col;
	columns[4] = &received_port_col;
	columns[5] = &received_proto_col;
	columns[6] = &rx_session_id_col;
	columns[7] = &reg_state_col;
	columns[8] = &expires_col;
	columns[9] = &socket_col;
	columns[10] = &service_routes_col;
	columns[11] = &public_ids_col;
	columns[12] = &path_col;

	if (ul_dbf.use_table(_c, _d->name) < 0) {
		LM_ERR("sql use_table failed\n");
		return -1;
	}

#ifdef EXTRA_DEBUG
	LM_NOTICE("load start time [%d]\n", (int)time(NULL));
#endif

	if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, 13, 0, 0) < 0) {
			LM_ERR("db_query (1) failed\n");
			return -1;
		}
		if(ul_dbf.fetch_result(_c, &res, ul_fetch_rows)<0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, 13, 0, &res) < 0) {
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

			ci = dbrow2info( ROW_VALUES(row)+1, &contact);
			if (ci==0) {
				LM_ERR("usrloc record for %.*s in table %s\n",
						aor.len, aor.s, _d->name->s);
				continue;
			}
			lock_udomain(_d, &aor, &ci->received_host, ci->received_port);

			if ( (mem_insert_pcontact(_d, &aor, ci, &c)) != 0) {
				LM_ERR("inserting contact failed\n");
				unlock_udomain(_d, &aor, &ci->received_host, ci->received_port);
				goto error1;
			}
			unlock_udomain(_d, &aor, &ci->received_host, ci->received_port);
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


