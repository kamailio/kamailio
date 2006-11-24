/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
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
 * 2005-02-25 incoming socket is saved in ucontact record (bogdan)
 * 2006-11-23 switched to better hash functions and fixed hash size (andrei)
 *   
 */

#include "udomain.h"
#include <string.h>
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "../../ut.h"
#include "../../parser/parse_param.h"
#include "../../parser/parse_uri.h"
#include "../../resolve.h"
#include "../../socket_info.h"
#include "ul_mod.h"            /* usrloc module parameters */
#include "notify.h"
#include "reg_avps.h"
#include "../../hashes.h"

/* #define HASH_STRING_OPTIMIZE */




/*
 * Hash function
 */
static inline int hash_func(udomain_t* _d, unsigned char* _s, int _l)
{
#ifdef HASH_STRING_OPTIMIZE
	return get_hash1_raw((char*)_s, _l) % UDOMAIN_HASH_SIZE;
#else
	return get_hash1_raw2((char*)_s, _l) % UDOMAIN_HASH_SIZE;
#endif
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


/*
 * Create a new domain structure
 * _n is pointer to str representing
 * name of the domain, the string is
 * not copied, it should point to str
 * structure stored in domain list
 */
int new_udomain(str* _n, udomain_t** _d)
{
	int i;
	
	     /* Must be always in shared memory, since
	      * the cache is accessed from timer which
	      * lives in a separate process
	      */
	*_d = (udomain_t*)shm_malloc(sizeof(udomain_t));
	if (!(*_d)) {
		LOG(L_ERR, "new_udomain(): No memory left\n");
		return -1;
	}
	memset(*_d, 0, sizeof(udomain_t));
	
	(*_d)->table = (hslot_t*)shm_malloc(sizeof(hslot_t) * UDOMAIN_HASH_SIZE);
	if (!(*_d)->table) {
		LOG(L_ERR, "new_udomain(): No memory left 2\n");
		shm_free(*_d);
		return -2;
	}

	(*_d)->name = _n;
	
	for(i = 0; i < UDOMAIN_HASH_SIZE; i++) {
		if (init_slot(*_d, &((*_d)->table[i])) < 0) {
			LOG(L_ERR, "new_udomain(): Error while initializing hash table\n");
			shm_free((*_d)->table);
			shm_free(*_d);
			return -3;
		}
	}

	lock_init(&(*_d)->lock);
	(*_d)->users = 0;
	(*_d)->expired = 0;
	
	return 0;
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
		for(i = 0; i < UDOMAIN_HASH_SIZE; i++) {
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
	fprintf(_f, "size : %d\n", UDOMAIN_HASH_SIZE);
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


static struct socket_info* find_socket(str* received)
{
	struct sip_uri puri;
	param_hooks_t hooks;
	struct hostent* he;
	struct ip_addr ip;
	struct socket_info* si;
	param_t* params;
	unsigned short port;
	char* buf;
	int error;

	if (!received) return 0;

	si = 0;
	if (parse_uri(received->s, received->len, &puri) < 0) {
		LOG(L_ERR, "find_socket: Error while parsing received URI\n");
		return 0;
	}
	
	if (parse_params(&puri.params, CLASS_URI, &hooks, &params) < 0) {
		LOG(L_ERR, "find_socket: Error while parsing received URI parameters\n");
		return 0;
	}

	if (!hooks.uri.dstip || !hooks.uri.dstip->body.s || !hooks.uri.dstip->body.len) goto end;

	buf = (char*)pkg_malloc(hooks.uri.dstip->body.len + 1);
	if (!buf) {
		LOG(L_ERR, "find_socket: No memory left\n");
		goto end;
	}
	memcpy(buf, hooks.uri.dstip->body.s, hooks.uri.dstip->body.len);
	buf[hooks.uri.dstip->body.len] = '\0';

	he = resolvehost(buf);
	if (he == 0) {
		LOG(L_ERR, "find_socket: Unable to resolve '%s'\n", buf);
		pkg_free(buf);
		goto end;
	}
	pkg_free(buf);

	if (hooks.uri.dstport && hooks.uri.dstport->body.s && hooks.uri.dstport->body.len) {
		port = str2s(hooks.uri.dstport->body.s, hooks.uri.dstport->body.len, &error);
		if (error != 0) {
			LOG(L_ERR, "find_socket: Unable to convert port number\n");		
			goto end;
		}
	} else {
		port = 0;
	}

	hostent2ip_addr(&ip, he, 0);
	si = find_si(&ip, port, puri.proto);
	if (si == 0) {
		LOG(L_ERR, "find_socket: Unable to find socket, using the default one\n");
		goto end;
	}
	
 end:
	if (params) free_params(params);
	return si;
}



int preload_udomain(db_con_t* _c, udomain_t* _d)
{
	char b[256];
	db_key_t columns[11];
	db_res_t* res;
	db_row_t* row;
	int i, cseq;
	unsigned int flags;
	struct socket_info* sock;
	str uid, contact, callid, ua, received, instance, aor;
	str* rec;
	time_t expires;
	qvalue_t q;

	urecord_t* r;
	ucontact_t* c;

	columns[0] = uid_col.s;
	columns[1] = contact_col.s;
	columns[2] = expires_col.s;
	columns[3] = q_col.s;
	columns[4] = callid_col.s;
	columns[5] = cseq_col.s;
	columns[6] = flags_col.s;
	columns[7] = user_agent_col.s;
	columns[8] = received_col.s;
	columns[9] = instance_col.s;
	columns[10] = aor_col.s;
	
	memcpy(b, _d->name->s, _d->name->len);
	b[_d->name->len] = '\0';

	if (ul_dbf.use_table(_c, b) < 0) {
		LOG(L_ERR, "preload_udomain(): Error in use_table\n");
		return -1;
	}

	if (ul_dbf.query(_c, 0, 0, 0, columns, 0, 11, 0, &res) < 0) {
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
		
		uid.s      = (char*)VAL_STRING(ROW_VALUES(row));
		if (uid.s == 0) {
			LOG(L_CRIT, "preload_udomain: ERROR: bad uid "
							"record in table %s\n", b);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		} else {
			uid.len = strlen(uid.s);
		}

		contact.s = (char*)VAL_STRING(ROW_VALUES(row) + 1);
		if (contact.s == 0) {
			LOG(L_CRIT, "preload_udomain: ERROR: bad contact "
							"record in table %s\n", b);
			LOG(L_CRIT, "preload_udomain: ERROR: for username %.*s\n",
							uid.len, uid.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		} else {
			contact.len = strlen(contact.s);
		}
		expires     = VAL_TIME  (ROW_VALUES(row) + 2);
		q           = double2q(VAL_DOUBLE(ROW_VALUES(row) + 3));
		cseq        = VAL_INT   (ROW_VALUES(row) + 5);
		callid.s    = (char*)VAL_STRING(ROW_VALUES(row) + 4);
		if (callid.s == 0) {
			LOG(L_CRIT, "preload_udomain: ERROR: bad callid record in"
							" table %s\n", b);
			LOG(L_CRIT, "preload_udomain: ERROR: for username %.*s,"
							" contact %.*s\n",
							uid.len, uid.s, contact.len, contact.s);
			LOG(L_CRIT, "preload_udomain: ERROR: skipping...\n");
			continue;
		} else {
			callid.len  = strlen(callid.s);
		}

		flags  = VAL_BITMAP(ROW_VALUES(row) + 6);

		ua.s  = (char*)VAL_STRING(ROW_VALUES(row) + 7);
		if (ua.s) {
			ua.len = strlen(ua.s);
		} else {
			ua.len = 0;
		}

		if (!VAL_NULL(ROW_VALUES(row) + 8)) {
			received.s  = (char*)VAL_STRING(ROW_VALUES(row) + 8);
			if (received.s) {
				received.len = strlen(received.s);
				rec = &received;

				sock = find_socket(&received);
			} else {
				received.len = 0;
				rec = 0;
				sock = 0;
			}
		} else {
			received.s = 0;
			received.len = 0;
			rec = 0;
			sock = 0;
		}

		if (!VAL_NULL(ROW_VALUES(row) + 9)) {
			instance.s  = (char*)VAL_STRING(ROW_VALUES(row) + 9);
			if (instance.s) {
				instance.len = strlen(instance.s);
			} else {
				instance.len = 0;
			}
		} else {
			instance.s = 0;
			instance.len = 0;
		}

		if (!VAL_NULL(ROW_VALUES(row) + 10)) {
			aor.s  = (char*)VAL_STRING(ROW_VALUES(row) + 10);
			if (aor.s) {
				aor.len = strlen(aor.s);
			} else {
				aor.len = 0;
			}
		} else {
			aor.s = 0;
			aor.len = 0;
		}

		if (get_urecord(_d, &uid, &r) > 0) {
			if (mem_insert_urecord(_d, &uid, &r) < 0) {
				LOG(L_ERR, "preload_udomain(): Can't create a record\n");
				ul_dbf.free_result(_c, res);
				unlock_udomain(_d);
				return -2;
			}
		}
		
		if (mem_insert_ucontact(r, &aor, &contact, expires, q, &callid, cseq, flags, &c, &ua, rec, sock, &instance) < 0) {
			LOG(L_ERR, "preload_udomain(): Error while inserting contact\n");
			ul_dbf.free_result(_c, res);
			unlock_udomain(_d);
			return -3;
		}

		db_read_reg_avps(_c, c);

		     /* We have to do this, because insert_ucontact sets state to CS_NEW
		      * and we have the contact in the database already
			  * we also store zombies in database so we have to restore
			  * the correct state
		      */
		c->state = CS_SYNC;
	}

	ul_dbf.free_result(_c, res);
	unlock_udomain(_d);
	return 0;
}


/*
 * Insert a new record into domain
 */
int mem_insert_urecord(udomain_t* _d, str* _uid, struct urecord** _r)
{
	int sl;
	
	if (new_urecord(_d->name, _uid, _r) < 0) {
		LOG(L_ERR, "insert_urecord(): Error while creating urecord\n");
		return -1;
	}

	sl = hash_func(_d, (unsigned char*)_uid->s, _uid->len);
	slot_add(&_d->table[sl], *_r);
	udomain_add(_d, *_r);
	_d->users++;
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
		_d->users--; /* FIXME */
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
/*	process_del_list(_d->name); */
/*	process_ins_list(_d->name); */
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
int insert_urecord(udomain_t* _d, str* _uid, struct urecord** _r)
{
	if (mem_insert_urecord(_d, _uid, _r) < 0) {
		LOG(L_ERR, "insert_urecord(): Error while inserting record\n");
		return -1;
	}
	return 0;
}


/*
 * Obtain a urecord pointer if the urecord exists in domain
 */
int get_urecord(udomain_t* _d, str* _uid, struct urecord** _r)
{
	int sl, i;
	urecord_t* r;

	sl = hash_func(_d, (unsigned char*)_uid->s, _uid->len);

	r = _d->table[sl].first;

	for(i = 0; i < _d->table[sl].n; i++) {
		if ((r->uid.len == _uid->len) && !memcmp(r->uid.s, _uid->s, _uid->len)) {
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
int delete_urecord(udomain_t* _d, str* _uid)
{
	struct ucontact* c, *t;
	struct urecord* r;

	if (get_urecord(_d, _uid, &r) > 0) {
		return 0;
	}
		
	c = r->contacts;
	while(c) {
		t = c;
		c = c->next;
		if (delete_ucontact(r, t) < 0) {
			LOG(L_ERR, "delete_urecord(): Error while deleting contact\n");
			return -1;
		}
	}
	release_urecord(r);
	return 0;
}
