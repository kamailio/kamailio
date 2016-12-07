/*
 * $Id$
 *
 * Usrloc module interface
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
 */

#include <stdio.h>
#include "../../dprint.h"
#include "../../ut.h"
#include "../../globals.h"
#include "dlist.h"
#include "utime.h"
#include "ul_mod.h"
#include "ul_rpc.h"


static inline void rpc_find_domain(str* _name, udomain_t** _d)
{
	dlist_t* ptr;
	
	ptr = root;
	while(ptr) {
		if ((ptr->name.len == _name->len) &&
			!memcmp(ptr->name.s, _name->s, _name->len)) {
			break;
		}
		ptr = ptr->next;
	}
	
	if (ptr) {
		*_d = ptr->d;
	} else {
		*_d = 0;
	}
}

static inline int add_contact(udomain_t* _d, str* _u, str* _c, time_t _e, qvalue_t _q, int _f, int sid)
{
	urecord_t* r;
	ucontact_t* c = 0;
	int res;
	str cid;
	str ua;
	str aor = STR_NULL;
	
	if (_e == 0 && !(_f & FL_PERMANENT)) {
		LOG(L_ERR, "rpc_add_contact(): expires == 0 and not persistent contact, giving up\n");
		return -1;
	}
	
	get_act_time();
	
	res = get_urecord(_d, _u, &r);
	if (res < 0) {
		LOG(L_ERR, "rpc_add_contact(): Error while getting record\n");
		return -2;
	}
	
	if (res >  0) { /* Record not found */
		if (insert_urecord(_d, _u, &r) < 0) {
			LOG(L_ERR, "rpc_add_contact(): Error while creating new urecord\n");
			return -3;
		}
	} else {
		if (get_ucontact(r, _c, &c) < 0) {
			LOG(L_ERR, "rpc_add_contact(): Error while obtaining ucontact\n");
			return -4;
		}
	}
	
	cid.s = "RPC-Call-ID";
	cid.len = strlen(cid.s);
	
	ua.s = "SER-RPC";
	ua.len = strlen(ua.s);
	
	if (c) {
		if (update_ucontact(c, &aor, _c, _e + act_time, _q, &cid, 42, _f, FL_NONE, &ua, 0, 0, 0, 
				    sid == -1 ? server_id : sid) < 0) {
			LOG(L_ERR, "rpc_add_contact(): Error while updating contact\n");
			release_urecord(r);
			return -5;
		}
	} else {
		if (insert_ucontact(r, &aor, _c, _e + act_time, _q, &cid, 42, _f, &c, &ua, 0, 0, 0,
							sid == -1 ? server_id : sid) < 0) {
			LOG(L_ERR, "rpc_add_contact(): Error while inserting contact\n");
			release_urecord(r);
			return -6;
		}
	}
	
	release_urecord(r);
	return 0;
}


static const char* rpc_stats_doc[2] = {
	"Print usrloc statistics",
	0
};

static void rpc_stats(rpc_t* rpc, void* c)
{
	dlist_t* ptr;
	void* handle;
	
	ptr = root;
	while(ptr) {
		rpc->add(c, "{", &handle);
		rpc->struct_add(handle, "Sdd",
						"domain", ptr->d->name,
						"users", ptr->d->users,
						"expired", ptr->d->expired);
		ptr = ptr->next;
	}
}


static const char* rpc_delete_uid_doc[2] = {
	"Delete all registered contacts for address of record.",
	0
};


static void rpc_delete_uid(rpc_t* rpc, void* c)
{
	udomain_t* d;
	str uid, t;
	
	if (rpc->scan(c, "SS", &t, &uid) < 2) return;
	
	rpc_find_domain(&t, &d);
	if (d) {
		lock_udomain(d);
		if (delete_urecord(d, &uid) < 0) {
			ERR("Error while deleting user %.*s\n", uid.len, uid.s);
			unlock_udomain(d);
			rpc->fault(c, 500, "Error While Deleting Record");
			return;
		}
		unlock_udomain(d);
	} else {
		rpc->fault(c, 400, "Table Not Found");
	}
}


static const char* rpc_delete_contact_doc[2] = {
	"Delete a contact if it exists.",
	0
};


static void rpc_delete_contact(rpc_t* rpc, void* ctx)
{
	udomain_t* d;
	urecord_t* r;
	ucontact_t* con;
	str uid, t, c;
	int res;
	
	if (rpc->scan(ctx, "SSS", &t, &uid, &c) < 3) return;
	
	rpc_find_domain(&t, &d);
	
	if (d) {
		lock_udomain(d);
		
		res = get_urecord(d, &uid, &r);
		if (res < 0) {
			rpc->fault(ctx, 500, "Error While Searching Table");
			ERR("Error while looking for uid %.*s in table %.*s\n", uid.len, uid.s, t.len, t.s);
			unlock_udomain(d);
			return;
		}
		
		if (res > 0) {
			rpc->fault(ctx, 404, "AOR Not Found");
			unlock_udomain(d);
			return;
		}
		
		res = get_ucontact(r, &c, &con);
		if (res < 0) {
			rpc->fault(ctx, 500, "Error While Searching for Contact");
			ERR("Error while looking for contact %.*s\n", c.len, c.s);
			unlock_udomain(d);
			return;
		}
		
		if (res > 0) {
			rpc->fault(ctx, 404, "Contact Not Found");
			unlock_udomain(d);
			return;
		}
		
		if (delete_ucontact(r, con) < 0) {
			rpc->fault(ctx, 500, "Error While Deleting Contact");
			unlock_udomain(d);
			return;
		}
		
		release_urecord(r);
		unlock_udomain(d);
	} else {
		rpc->fault(ctx, 404, "Table Not Found");
	}
}


static const char* rpc_dump_doc[2] = {
	"Print all registered contacts.",
	0
};


static void rpc_dump(rpc_t* rpc, void* c)
{
	rpc->fault(c, 500, "Not Yet Implemented");
}

static const char* rpc_dump_file_doc[2] = {
	"Print all registered contacts into a file.",
	0
};


static void rpc_dump_file(rpc_t* rpc, void* c)
{
	str filename;
	FILE *file;
	
	if (rpc->scan(c, "S", &filename) < 1) {
		return;
	}
	
	DBG("dumping to file '%.*s'.\n", filename.len, ZSW(filename.s));
	if (! (file = fopen(filename.s, "w"))) {
		ERR("failed to open file `%s'.\n", filename.s);
		rpc->fault(rpc, 500, "failed to open file `%s'.\n", filename.s);
		return;
	}
	print_all_udomains(file);
	fclose(file);
}


static const char* rpc_flush_doc[2] = {
	"Flush cache into database.",
	0
};

static void rpc_flush(rpc_t* rpc, void* c)
{
        synchronize_all_udomains();
}


static const char* rpc_add_contact_doc[2] = {
	"Create a new contact.",
	0
};


static void rpc_add_contact(rpc_t* rpc, void* c)
{
	udomain_t* d;
	int expires, flags, sid;
	double q;
	qvalue_t qval;
	
	str table, uid, contact;
	
	if (rpc->scan(c, "SSSdfd", &table, &uid, &contact, &expires, &q, &flags) < 6) return;
	qval = double2q(q);
	if (rpc->scan(c, "d", &sid) < 1) sid = -1;

	rpc_find_domain(&table, &d);
	if (d) {
		lock_udomain(d);
		
		if (add_contact(d, &uid, &contact, expires, qval, flags, sid) < 0) {
			unlock_udomain(d);
			ERR("Error while adding contact ('%.*s','%.*s') in table '%.*s'\n",
				uid.len, ZSW(uid.s), contact.len, ZSW(contact.s), table.len, ZSW(table.s));
			rpc->fault(c, 500, "Error while adding Contact");
			return;
		}
		unlock_udomain(d);
	} else {
		rpc->fault(c, 400, "Table Not Found");
	}
}


/*
 * Build Contact HF for reply
 */
static inline int print_contacts(rpc_t* rpc, void* ctx, ucontact_t* _c)
{
	int cnt = 0;
	void* handle;
	
	while(_c) {
		if (VALID_CONTACT(_c, act_time)) {
			cnt++;

            if (rpc->add(ctx, "{", &handle) < 0) return -1;
            rpc->struct_add(handle, "SfdSS",
							"contact", &_c->c,
							"q", q2double(_c->q),
							"expires", (int)(_c->expires - act_time),
							"ua", &_c->user_agent,
							"recv", &_c->received);
		}
		
		_c = _c->next;
	}
	
	return cnt;
}


static const char* rpc_show_contacts_doc[2] = {
	"List all registered contacts for address of record",
	0
};

static void rpc_show_contacts(rpc_t* rpc, void* c)
{
	udomain_t* d;
	urecord_t* r;
	int res;
	str t, uid;
	
	if (rpc->scan(c, "SS", &t, &uid) < 2) return;
	
	rpc_find_domain(&t, &d);
	if (d) {
		lock_udomain(d);
		
		res = get_urecord(d, &uid, &r);
		if (res < 0) {
			rpc->fault(c, 500, "Error While Searching AOR");
			ERR("Error while looking for username %.*s in table %.*s\n", uid.len, uid.s, t.len, t.s);
			unlock_udomain(d);
			return;
		}
		
		if (res > 0) {
			rpc->fault(c, 404, "AOR Not Found");
			unlock_udomain(d);
			return;
		}
		
		get_act_time();
		
		if (!print_contacts(rpc, c, r->contacts)) {
			unlock_udomain(d);
			rpc->fault(c, 404, "No Registered Contacts Found");
			return;
		}
		
		unlock_udomain(d);
	} else {
		rpc->fault(c, 400, "Table Not Found");
	}
}



rpc_export_t ul_rpc[] = {
	{"usrloc.stats",           rpc_stats,           rpc_stats_doc,          RET_ARRAY},
	{"usrloc.delete_uid",      rpc_delete_uid,      rpc_delete_uid_doc,     0},
	{"usrloc.delete_contact",  rpc_delete_contact,  rpc_delete_contact_doc, 0},
	{"usrloc.dump",            rpc_dump,            rpc_dump_doc,           0},
	{"usrloc.dump_file",       rpc_dump_file,       rpc_dump_file_doc,      0},
	{"usrloc.flush",           rpc_flush,           rpc_flush_doc,          0},
	{"usrloc.add_contact",     rpc_add_contact,     rpc_add_contact_doc,    0},
	{"usrloc.show_contacts",   rpc_show_contacts,   rpc_show_contacts_doc,  RET_ARRAY},
	{0, 0, 0, 0}
};
