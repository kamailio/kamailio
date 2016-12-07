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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "../../lib/srdb2/db.h"
#include "../../ut.h"
#include "../../parser/parse_param.h"
#include "../../parser/parse_uri.h"
#include "../../resolve.h"
#include "../../socket_info.h"
#include "ul_mod.h"            /* usrloc module parameters */
#include "notify.h"
#include "reg_avps.h"
#include "reg_avps_db.h"
#include "utime.h"
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



int preload_udomain(udomain_t* _d)
{
	db_fld_t columns[] = {
		{.name = uid_col.s,        .type = DB_STR},
		{.name = contact_col.s,    .type = DB_STR},
		{.name = expires_col.s,    .type = DB_DATETIME},
		{.name = q_col.s,          .type = DB_DOUBLE},
		{.name = callid_col.s,     .type = DB_STR},
		{.name = cseq_col.s,       .type = DB_INT},
		{.name = flags_col.s,      .type = DB_BITMAP},
		{.name = user_agent_col.s, .type = DB_STR},
		{.name = received_col.s,   .type = DB_STR},
		{.name = instance_col.s,   .type = DB_STR},
		{.name = aor_col.s,        .type = DB_STR},
		{.name = server_id_col.s,  .type = DB_INT},
		{.name = avp_column,       .type = DB_STR}, /* Must be the last element in the array */
		{.name = NULL}
	};

	db_res_t* res = NULL;
	db_rec_t* rec;
	db_cmd_t* get_all = NULL;

	struct socket_info* sock;
	str callid, ua, instance, aor;
	str* receivedp;
	qvalue_t q;
	unsigned int flags;
	urecord_t* r;
	ucontact_t* c;

	get_all = db_cmd(DB_GET, db, _d->name->s, columns, NULL, NULL);
	if (get_all == NULL) {
		ERR("usrloc: Error while compiling DB_GET command\n");
		return -1;
	}
	if (db_setopt(get_all, "fetch_all", 0) < 0) {
		ERR("usrloc: Error while disabling 'fetch_all' database option\n");
	}

	if (db_exec(&res, get_all) < 0) goto error;

	rec = db_first(res);
	if (rec == NULL) {
		DBG("preload_udomain(): Table is empty\n");
		db_res_free(res);
		db_cmd_free(get_all);
		return 0;
	}

	lock_udomain(_d);
	get_act_time();

	for(; rec != NULL; rec = db_next(res)) {
		/* UID column must never be NULL */
		if (rec->fld[0].flags & DB_NULL) {
			LOG(L_CRIT, "preload_udomain: ERROR: bad uid "
				"record in table %.*s, skipping...\n", 
				_d->name->len, _d->name->s);
			continue;
		}

		/* Contact column must never be NULL */
		if (rec->fld[1].flags & DB_NULL) {
			LOG(L_CRIT, "ERROR: Bad contact for uid %.*s in table %.*s, skipping\n",
				rec->fld[0].v.lstr.len, rec->fld[0].v.lstr.s,
				_d->name->len, _d->name->s);
			continue;
		}

		/* We only skip expired contacts if db_skip_delete is enabled. If
		 * db_skip_delete is disabled then we must load expired contacts
		 * in memory so that the timer can delete them later.
		 */
		if (db_skip_delete && (rec->fld[2].v.time < act_time)) {
			DBG("preload_udomain: Skipping expired contact\n");
			continue;
		}

		q = double2q(rec->fld[3].v.dbl);

		if (rec->fld[4].flags & DB_NULL) {
			callid.s = NULL;
			callid.len = 0;
		} else {
			callid = rec->fld[4].v.lstr;
		}

		if (rec->fld[7].flags & DB_NULL) {
			ua.s = NULL;
			ua.len = 0;
		} else {
			ua = rec->fld[7].v.lstr;
		}

		if (rec->fld[8].flags & DB_NULL) {
			receivedp = 0;
			sock = 0;
		} else {
			receivedp = &rec->fld[8].v.lstr;
			sock = find_socket(receivedp);
		}

		if (rec->fld[9].flags & DB_NULL) {
			instance.s = NULL;
			instance.len = 0;
		} else {
			instance = rec->fld[9].v.lstr;
		}

		if (rec->fld[10].flags & DB_NULL) {
			aor.s = NULL;
			aor.len = 0;
		} else {
			aor = rec->fld[10].v.lstr;
		}

		if (get_urecord(_d, &rec->fld[0].v.lstr, &r) > 0) {
			if (mem_insert_urecord(_d, &rec->fld[0].v.lstr, &r) < 0) {
				LOG(L_ERR, "preload_udomain(): Can't create a record\n");
				unlock_udomain(_d);
				goto error;
			}
		}

		flags = rec->fld[6].v.bitmap;
		if (rec->fld[11].v.int4 != server_id) {
			/* FIXME: this should not be hardcoded here this way */
			/* This is a records from another SIP server instance, mark
			 * it as in memory only because the other SIP server is responsible
			 * for updating the record in database
			 */
			flags |= FL_MEM;
		}

		if (mem_insert_ucontact(r, &aor, &rec->fld[1].v.lstr, rec->fld[2].v.int4, 
								q, &callid, rec->fld[5].v.int4, flags, &c, &ua, receivedp, 
								sock, &instance, rec->fld[11].v.int4) < 0) {
			LOG(L_ERR, "preload_udomain(): Error while inserting contact\n");
			unlock_udomain(_d);
			goto error;
		}

		if (use_reg_avps() && ((rec->fld[12].flags & DB_NULL) != DB_NULL)) {
			c->avps = deserialize_avps(&rec->fld[12].v.lstr);
				
		}

		     /* We have to do this, because insert_ucontact sets state to CS_NEW
		      * and we have the contact in the database already
			  * we also store zombies in database so we have to restore
			  * the correct state
		      */
		c->state = CS_SYNC;
	}

	unlock_udomain(_d);
	db_res_free(res);
	db_cmd_free(get_all);
	return 0;

 error:
	if (res) db_res_free(res);
	if (get_all) db_cmd_free(get_all);
	return -1;
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
	cur_cmd = _d->db_cmd_idx;
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
