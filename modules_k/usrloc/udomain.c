/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 * 2003-03-11 changed to the new locking scheme: locking.h (andrei)
 * 2003-03-12 added replication mark and zombie state (nils)
 * 2004-06-07 updated to the new DB api (andrei)
 * 2004-08-23  hash function changed to process characters as unsigned
 *             -> no negative results occur (jku)
 *   
 */

#include "udomain.h"
#include <string.h>
#include "../../parser/parse_methods.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "../../socket_info.h"
#include "../../ut.h"
#include "ul_mod.h"            /* usrloc module parameters */
#include "notify.h"


/*
 * Hash function
 */
static inline int hash_func(udomain_t* _d, unsigned char* _s, int _l)
{
	int res = 0, i;
	
	for(i = 0; i < _l; i++) {
		res += _s[i];
	}
	
	return res % _d->size;
}


/*
 * Add a record to list of all records in a domain
 */
static inline void udomain_add(udomain_t* _d, urecord_t* _r)
{
	if (_d->d_ll.n == 0) {
		_d->d_ll.first = _r;
		_d->d_ll.last = _r;
	} else {
		_r->d_ll.prev = _d->d_ll.last;
		_d->d_ll.last->d_ll.next = _r;
		_d->d_ll.last = _r;
	}
	_d->d_ll.n++;
}


/*
 * Remove a record from list of all records in a domain
 */
static inline void udomain_remove(udomain_t* _d, urecord_t* _r)
{
	if (_d->d_ll.n == 0) return;

	if (_r->d_ll.prev) {
		_r->d_ll.prev->d_ll.next = _r->d_ll.next;
	} else {
		_d->d_ll.first = _r->d_ll.next;
	}

	if (_r->d_ll.next) {
		_r->d_ll.next->d_ll.prev = _r->d_ll.prev;
	} else {
		_d->d_ll.last = _r->d_ll.prev;
	}

	_r->d_ll.prev = _r->d_ll.next = 0;
	_d->d_ll.n--;
}


static char *build_stat_name( str* domain, char *var_name)
{
	int n;
	char *s;
	char *p;

	n = domain->len + 1 + strlen(var_name) + 1;
	s = (char*)shm_malloc( n );
	if (s==0) {
		LOG(L_ERR,"ERROR:usrloc:build_stat_name: no more shm mem\n");
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


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 * _s is hash table size
 */
int new_udomain(str* _n, int _s, udomain_t** _d)
{
	int i;
	char *name;
	
	/* Must be always in shared memory, since
	 * the cache is accessed from timer which
	 * lives in a separate process
	 */
	*_d = (udomain_t*)shm_malloc(sizeof(udomain_t));
	if (!(*_d)) {
		LOG(L_ERR, "new_udomain(): No memory left\n");
		goto error0;
	}
	memset(*_d, 0, sizeof(udomain_t));
	
	(*_d)->table = (hslot_t*)shm_malloc(sizeof(hslot_t) * _s);
	if (!(*_d)->table) {
		LOG(L_ERR, "new_udomain(): No memory left 2\n");
		goto error1;
	}

	(*_d)->name = _n;
	
	for(i = 0; i < _s; i++) {
		if (init_slot(*_d, &((*_d)->table[i])) < 0) {
			LOG(L_ERR, "new_udomain(): Error while initializing hash table\n");
			goto error2;
		}
	}

	(*_d)->size = _s;
	lock_init(&(*_d)->lock);

	/* register the statistics */
	if ( (name=build_stat_name(_n,"users"))==0 || register_stat("usrloc",
	name, &(*_d)->users, STAT_NO_RESET|STAT_NO_SYNC|STAT_SHM_NAME)!=0 ) {
		LOG(L_ERR,"ERROR:usrloc:new_udomain: failed to add stat variable\n");
		goto error2;
	}
	if ( (name=build_stat_name(_n,"contacts"))==0 || register_stat("usrloc",
	name, &(*_d)->contacts, STAT_NO_RESET|STAT_NO_SYNC|STAT_SHM_NAME)!=0 ) {
		LOG(L_ERR,"ERROR:usrloc:new_udomain: failed to add stat variable\n");
		goto error2;
	}
	if ( (name=build_stat_name(_n,"expires"))==0 || register_stat("usrloc",
	name, &(*_d)->expires, STAT_NO_SYNC|STAT_SHM_NAME)!=0 ) {
		LOG(L_ERR,"ERROR:usrloc:new_udomain: failed to add stat variable\n");
		goto error2;
	}

	return 0;
error2:
	shm_free((*_d)->table);
error1:
	shm_free(*_d);
error0:
	return -1;
}


/*
 * Free all memory allocated for
 * the domain
 */
void free_udomain(udomain_t* _d)
{
	int i;
	
	lock_udomain(_d);
	if (_d->table) {
		for(i = 0; i < _d->size; i++) {
			deinit_slot(_d->table + i);
		}
		shm_free(_d->table);
	}
	unlock_udomain(_d);
	lock_destroy(&_d->lock);/* destroy the lock (required for SYSV sems!)*/

        shm_free(_d);
}


/*
 * Just for debugging
 */
void print_udomain(FILE* _f, udomain_t* _d)
{
	struct urecord* r;
	fprintf(_f, "---Domain---\n");
	fprintf(_f, "name : '%.*s'\n", _d->name->len, ZSW(_d->name->s));
	fprintf(_f, "size : %d\n", _d->size);
	fprintf(_f, "table: %p\n", _d->table);
	fprintf(_f, "d_ll {\n");
	fprintf(_f, "    n    : %d\n", _d->d_ll.n);
	fprintf(_f, "    first: %p\n", _d->d_ll.first);
	fprintf(_f, "    last : %p\n", _d->d_ll.last);
	fprintf(_f, "}\n");
	/*fprintf(_f, "lock : %d\n", _d->lock); -- can be a structure --andrei*/
	if (_d->d_ll.n > 0) {
		fprintf(_f, "\n");
		r = _d->d_ll.first;
		while(r) {
			print_urecord(_f, r);
			r = r->d_ll.next;
		}


		fprintf(_f, "\n");
	}
	fprintf(_f, "---/Domain---\n");
}



int preload_udomain(db_con_t* _c, udomain_t* _d)
{
	ucontact_info_t ci;
	char uri[MAX_URI_SIZE];
	db_key_t columns[13];
	db_res_t* res;
	db_row_t* row;
	int i;
	int port, proto;
	char *p;

	str user, contact, callid, ua, received, host, path;
	char* domain;
	int ver;

	urecord_t* r;
	ucontact_t* c;

	ver = 0;
	ver =  table_version(&ul_dbf, _c, _d->name);
	if(ver!=UL_TABLE_VERSION)
	{
		LOG(L_ERR,"usrloc:preload_udomain: Wrong version v%d for table <%.*s>,"
				" expected v%d\n", ver, _d->name->len, _d->name->s,
				UL_TABLE_VERSION);
		return -1;
	}

	columns[0] = user_col.s;
	columns[1] = contact_col.s;
	columns[2] = expires_col.s;
	columns[3] = q_col.s;
	columns[4] = callid_col.s;
	columns[5] = cseq_col.s;
	columns[6] = flags_col.s;
	columns[7] = user_agent_col.s;
	columns[8] = received_col.s;
	columns[9] = path_col.s;
	columns[10] = sock_col.s;
	columns[11] = methods_col.s;
	columns[12] = domain_col.s;

	if (ul_dbf.use_table(_c, _d->name->s) < 0) {
		LOG(L_ERR, "preload_udomain(): Error in use_table\n");
		return -1;
	}

	if (ul_dbf.query(_c, 0, 0, 0, columns, 0, (use_domain) ? (13) : (12), 0,
				&res) < 0) {
		LOG(L_ERR, "preload_udomain(): Error while doing db_query\n");
		return -1;
	}

	if (RES_ROW_N(res) == 0) {
		DBG("preload_udomain(): Table is empty\n");
		ul_dbf.free_result(_c, res);
		return 0;
	}

	lock_udomain(_d);

	for(i = 0; i < RES_ROW_N(res); i++) {
		row = RES_ROWS(res) + i;
		memset( &ci, 0, sizeof(ucontact_info_t));

		user.s = (char*)VAL_STRING(ROW_VALUES(row));
		if (VAL_NULL(ROW_VALUES(row)) || user.s==0 || user.s[0]==0) {
			LOG(L_CRIT, "preload_udomain: ERROR: empty username "
				"record in table %s\n", _d->name->s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		}
		user.len = strlen(user.s);

		contact.s = (char*)VAL_STRING(ROW_VALUES(row) + 1);
		if (VAL_NULL(ROW_VALUES(row)+1) || contact.s==0 || contact.s[0]==0) {
			LOG(L_CRIT, "preload_udomain: ERROR: bad contact "
				"record in table %s for user %.*s\n", _d->name->s,
				user.len, user.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		}
		contact.len = strlen(contact.s);

		if (VAL_NULL(ROW_VALUES(row)+2)) {
			LOG(L_CRIT, "preload_udomain: ERROR: empty expire "
				"record in table %s for user %.*s\n", _d->name->s,
				user.len, user.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		}
		ci.expires = VAL_TIME(ROW_VALUES(row) + 2);

		if (VAL_NULL(ROW_VALUES(row)+3)) {
			LOG(L_CRIT, "preload_udomain: ERROR: empty q "
				"record in table %s for user %.*s\n", _d->name->s,
				user.len, user.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		}
		ci.q = double2q(VAL_DOUBLE(ROW_VALUES(row) + 3));

		if (VAL_NULL(ROW_VALUES(row)+5)) {
			LOG(L_CRIT, "preload_udomain: ERROR: empty cseq_nr "
				"record in table %s for user %.*s\n", _d->name->s,
				user.len, user.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		}
		ci.cseq = VAL_INT(ROW_VALUES(row) + 5);

		callid.s = (char*)VAL_STRING(ROW_VALUES(row) + 4);
		if (VAL_NULL(ROW_VALUES(row)+4) || !callid.s || !callid.s[0]) {
			LOG(L_CRIT, "preload_udomain: ERROR: bad callid "
				"record in table %s for user %.*s\n", _d->name->s,
				user.len, user.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		}
		callid.len  = strlen(callid.s);
		ci.callid = &callid;

		if (VAL_NULL(ROW_VALUES(row)+6)) {
			LOG(L_CRIT, "preload_udomain: ERROR: empty flag "
				"record in table %s for user %.*s\n", _d->name->s,
				user.len, user.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		}
		ci.flags1  = VAL_BITMAP(ROW_VALUES(row) + 6);

		ua.s  = (char*)VAL_STRING(ROW_VALUES(row) + 7);
		if (VAL_NULL(ROW_VALUES(row)+7) || !ua.s || !ua.s[0]==0) {
			ua.s = 0;
			ua.len = 0;
		} else {
			ua.len = strlen(ua.s);
		}
		ci.user_agent = &ua;

		received.s  = (char*)VAL_STRING(ROW_VALUES(row) + 8);
		if (VAL_NULL(ROW_VALUES(row)+8) || !received.s || !received.s[0]) {
			received.len = 0;
			received.s = 0;
		} else {
			received.len = strlen(received.s);
		}
		ci.received = &received;
		
		path.s  = (char*)VAL_STRING(ROW_VALUES(row) + 9);
		if (VAL_NULL(ROW_VALUES(row)+9) || !path.s || !path.s[0]) {
			path.len = 0;
			path.s = 0;
		} else {
			path.len = strlen(path.s);
		}
		ci.path= &path;

		/* socket name */
		p  = (char*)VAL_STRING(ROW_VALUES(row) + 10);
		if (VAL_NULL(ROW_VALUES(row)+10) || p==0 || p[0]==0){
			ci.sock = 0;
		} else {
			if (parse_phostport( p, strlen(p), &host.s, &host.len, 
			&port, &proto)!=0) {
				LOG(L_ERR,"ERROR:usrloc:preload_udomain: bad socket <%s> in "
						"table %s for user %.*s\n", p, _d->name->s,
						user.len, user.s);
				LOG(L_CRIT,"preload_udomain: ERROR: skipping...\n");
				continue;
			}
			ci.sock = grep_sock_info( &host, (unsigned short)port, proto);
			if (ci.sock==0) {
				LOG(L_WARN,"ERROR:usrloc:preload_udomain: non-local socket "
						"<%s> in table %s for user %.*s...ignoring\n",
						p , _d->name->s, user.len, user.s);
			}
		}

		/* supported methods */
		if (VAL_NULL(ROW_VALUES(row)+11)) {
			ci.methods = ALL_METHODS;
		} else {
			ci.methods = VAL_BITMAP(ROW_VALUES(row) + 11);
		}

		if (use_domain) {
			domain = (char*)VAL_STRING(ROW_VALUES(row) + 12);
			if (VAL_NULL(ROW_VALUES(row)+12) || domain==0 || domain[0]==0) {
				LOG(L_CRIT, "preload_udomain: ERROR: empty domain "
					"record in table %s for user %.*s\n", _d->name->s,
					user.len, user.s);
				LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
				continue;
			}
			/* user.s cannot be NULL - checked previosly */
			user.len = snprintf(uri, MAX_URI_SIZE, "%.*s@%s",
				user.len, user.s, domain);
			user.s = uri;
			if (user.s[user.len]!=0) {
				LOG(L_CRIT,"preload_udomain(): URI '%.*s@%s' longer than %d\n",
					user.len, user.s, domain,MAX_URI_SIZE);
				LOG(L_CRIT,"preload_udomain: ERROR: skipping...\n");
				continue;
			}
		}

		if (get_urecord(_d, &user, &r) > 0) {
			if (mem_insert_urecord(_d, &user, &r) < 0) {
				LOG(L_ERR, "preload_udomain(): Can't create a record\n");
				goto error;
			}
		}

		if ( (c=mem_insert_ucontact(r, &contact, &ci)) < 0) {
			LOG(L_ERR, "preload_udomain(): Error while inserting contact\n");
			goto error1;
		}

		/* We have to do this, because insert_ucontact sets state to CS_NEW
		 * and we have the contact in the database already */
		c->state = CS_SYNC;
	}

	ul_dbf.free_result(_c, res);
	unlock_udomain(_d);
	return 0;
error1:
	free_ucontact(c);
error:
	ul_dbf.free_result(_c, res);
	unlock_udomain(_d);
	return -1;
}



/*
 * Insert a new record into domain
 */
int mem_insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	int sl;
	
	if (new_urecord(_d->name, _aor, _r) < 0) {
		LOG(L_ERR, "insert_urecord(): Error while creating urecord\n");
		return -1;
	}

	sl = hash_func(_d, (unsigned char*)_aor->s, _aor->len);
	slot_add(&_d->table[sl], *_r);
	udomain_add(_d, *_r);
	update_stat( _d->users, 1);
	return 0;
}


/*
 * Remove a record from domain
 */
void mem_delete_urecord(udomain_t* _d, struct urecord* _r)
{
	if (_r->watchers == 0) {
		udomain_remove(_d, _r);
		slot_rem(_r->slot, _r);
		free_urecord(_r);
		update_stat( _d->users, -1);
	}
}


int timer_udomain(udomain_t* _d)
{
	struct urecord* ptr, *t;

	lock_udomain(_d);

	ptr = _d->d_ll.first;

	while(ptr) {
		if (timer_urecord(ptr) < 0) {
			LOG(L_ERR, "timer_udomain(): Error in timer_urecord\n");
			unlock_udomain(_d);
			return -1;
		}
		
		     /* Remove the entire record
		      * if it is empty
		      */
		if (ptr->contacts == 0) {
			t = ptr;
			ptr = ptr->d_ll.next;
			mem_delete_urecord(_d, t);
		} else {
			ptr = ptr->d_ll.next;
		}
	}
	
	unlock_udomain(_d);
	return 0;
}


/*
 * Get lock
 */
void lock_udomain(udomain_t* _d)
{
	lock_get(&_d->lock);
}


/*
 * Release lock
 */
void unlock_udomain(udomain_t* _d)
{
	lock_release(&_d->lock);
}


/*
 * Create and insert a new record
 */
int insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	if (mem_insert_urecord(_d, _aor, _r) < 0) {
		LOG(L_ERR, "insert_urecord(): Error while inserting record\n");
		return -1;
	}
	return 0;
}


/*
 * Obtain a urecord pointer if the urecord exists in domain
 */
int get_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	int sl, i;
	urecord_t* r;

	sl = hash_func(_d, (unsigned char*)_aor->s, _aor->len);

	r = _d->table[sl].first;

	for(i = 0; i < _d->table[sl].n; i++) {
		if ((r->aor.len == _aor->len) && !memcmp(r->aor.s, _aor->s, _aor->len)) {
			*_r = r;
			return 0;
		}

		r = r->s_ll.next;
	}

	return 1;   /* Nothing found */
}


/*
 * Delete a urecord from domain
 */
int delete_urecord(udomain_t* _d, str* _aor, struct urecord* _r)
{
	struct ucontact* c, *t;

	if (_r==0) {
		if (get_urecord(_d, _aor, &_r) > 0) {
			return 0;
		}
	}

	c = _r->contacts;
	while(c) {
		t = c;
		c = c->next;
		if (delete_ucontact(_r, t) < 0) {
			LOG(L_ERR, "delete_urecord(): Error while deleting contact\n");
			return -1;
		}
	}
	release_urecord(_r);
	return 0;
}
