/*
 * Copyright (C) 2001-2003 FhG Fokus
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

/*! \file
 *  \brief USRLOC - Userloc domain handling functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */

#include "udomain.h"
#include <string.h>
#include "../../parser/parse_methods.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db.h"
#include "../../socket_info.h"
#include "../../ut.h"
#include "../../hashes.h"
#include "ul_mod.h"            /* usrloc module parameters */
#include "usrloc.h"
#include "utime.h"
#include "usrloc.h"

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


/*!
 * \brief Create a new domain structure
 * \param  _n is pointer to str representing name of the domain, the string is
 * not copied, it should point to str structure stored in domain list
 * \param _s is hash table size
 * \param _d new created domain
 * \return 0 on success, -1 on failure
 */
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
	if ( (name=build_stat_name(_n,"users"))==0 || register_stat("usrloc",
	name, &(*_d)->users, STAT_NO_RESET|STAT_SHM_NAME)!=0 ) {
		LM_ERR("failed to add stat variable\n");
		goto error2;
	}
	if ( (name=build_stat_name(_n,"contacts"))==0 || register_stat("usrloc",
	name, &(*_d)->contacts, STAT_NO_RESET|STAT_SHM_NAME)!=0 ) {
		LM_ERR("failed to add stat variable\n");
		goto error2;
	}
	if ( (name=build_stat_name(_n,"expires"))==0 || register_stat("usrloc",
	name, &(*_d)->expires, STAT_SHM_NAME)!=0 ) {
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


/*!
 * \brief Free all memory allocated for the domain
 * \param _d freed domain
 */
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


/*!
 * \brief Returns a static dummy urecord for temporary usage
 * \param _d domain (needed for the name)
 * \param _aor address of record
 * \param _r new created urecord
 */
static inline void get_static_urecord(udomain_t* _d, str* _aor,
														struct urecord** _r)
{
	static struct urecord r;

	memset( &r, 0, sizeof(struct urecord) );
	r.aor = *_aor;
	r.aorhash = ul_get_aorhash(_aor);
	r.domain = _d->name;
	*_r = &r;
}


/*!
 * \brief Debugging helper function
 */
void print_udomain(FILE* _f, udomain_t* _d)
{
	int i;
	int max=0, slot=0, n=0;
	struct urecord* r;
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
			print_urecord(_f, r);
			r = r->next;
		}
	}
	fprintf(_f, "\nMax slot: %d (%d/%d)\n", max, slot, n);
	fprintf(_f, "\n---/Domain---\n");
}


/*!
 * \brief Convert database values into ucontact_info
 *
 * Convert database values into ucontact_info, 
 * expects 12 rows (contact, expirs, q, callid, cseq, flags,
 * ua, received, path, socket, methods, last_modified)
 * \param vals database values
 * \param contact contact
 * \param rcon restore connection id
 * \return pointer to the ucontact_info on success, 0 on failure
 */
static inline ucontact_info_t* dbrow2info(db_val_t *vals, str *contact, int rcon)
{
	static ucontact_info_t ci;
	static str callid, ua, received, host, path;
	int port, proto;
	char *p;

	memset( &ci, 0, sizeof(ucontact_info_t));

	contact->s = (char*)VAL_STRING(vals);
	if (VAL_NULL(vals) || contact->s==0 || contact->s[0]==0) {
		LM_CRIT("bad contact\n");
		return 0;
	}
	contact->len = strlen(contact->s);

	if (VAL_NULL(vals+1)) {
		LM_CRIT("empty expire\n");
		return 0;
	}
	ci.expires = UL_DB_EXPIRES_GET(vals+1);

	if (VAL_NULL(vals+2)) {
		LM_CRIT("empty q\n");
		return 0;
	}
	ci.q = double2q(VAL_DOUBLE(vals+2));

	if (VAL_NULL(vals+4)) {
		LM_CRIT("empty cseq_nr\n");
		return 0;
	}
	ci.cseq = VAL_INT(vals+4);

	callid.s = (char*)VAL_STRING(vals+3);
	if (VAL_NULL(vals+3) || !callid.s || !callid.s[0]) {
		LM_CRIT("bad callid\n");
		return 0;
	}
	callid.len  = strlen(callid.s);
	ci.callid = &callid;

	if (VAL_NULL(vals+5)) {
		LM_CRIT("empty flag\n");
		return 0;
	}
	ci.flags  = VAL_BITMAP(vals+5);

	if (VAL_NULL(vals+6)) {
		LM_CRIT("empty cflag\n");
		return 0;
	}
	ci.cflags  = VAL_BITMAP(vals+6);

	ua.s  = (char*)VAL_STRING(vals+7);
	if (VAL_NULL(vals+7) || !ua.s || !ua.s[0]) {
		ua.s = 0;
		ua.len = 0;
	} else {
		ua.len = strlen(ua.s);
	}
	ci.user_agent = &ua;

	received.s  = (char*)VAL_STRING(vals+8);
	if (VAL_NULL(vals+8) || !received.s || !received.s[0]) {
		received.len = 0;
		received.s = 0;
	} else {
		received.len = strlen(received.s);
	}
	ci.received = received;
	
	path.s  = (char*)VAL_STRING(vals+9);
		if (VAL_NULL(vals+9) || !path.s || !path.s[0]) {
			path.len = 0;
			path.s = 0;
		} else {
			path.len = strlen(path.s);
		}
	ci.path= &path;

	/* socket name */
	p  = (char*)VAL_STRING(vals+10);
	if (VAL_NULL(vals+10) || p==0 || p[0]==0){
		ci.sock = 0;
	} else {
		if (parse_phostport( p, &host.s, &host.len, 
		&port, &proto)!=0) {
			LM_ERR("bad socket <%s>\n", p);
			return 0;
		}
		ci.sock = grep_sock_info( &host, (unsigned short)port, proto);
		if (ci.sock==0) {
			LM_DBG("non-local socket <%s>...ignoring\n", p);
		}
	}

	/* supported methods */
	if (VAL_NULL(vals+11)) {
		ci.methods = ALL_METHODS;
	} else {
		ci.methods = VAL_BITMAP(vals+11);
	}

	/* last modified time */
	if (!VAL_NULL(vals+12)) {
		ci.last_modified = UL_DB_EXPIRES_GET(vals+12);
	}

	/* record internal uid */
	if (!VAL_NULL(vals+13)) {
		ci.ruid.s = (char*)VAL_STRING(vals+13);
		ci.ruid.len = strlen(ci.ruid.s);
	}

	/* sip instance */
	if (!VAL_NULL(vals+14)) {
		ci.instance.s = (char*)VAL_STRING(vals+14);
		ci.instance.len = strlen(ci.instance.s);
	}

	/* reg-id */
	if (!VAL_NULL(vals+15)) {
		ci.reg_id = VAL_UINT(vals+15);
	}

	/* server_id */
	if (!VAL_NULL(vals+16)) {
		ci.server_id = VAL_UINT(vals+16);
	}

	/* tcp connection id (not restored always) */
	ci.tcpconn_id = -1;
	if(rcon==1 && !VAL_NULL(vals+17)) {
		ci.tcpconn_id = VAL_UINT(vals+17);
	}

	/* keepalive */
	if (!VAL_NULL(vals+18)) {
		ci.keepalive = VAL_UINT(vals+18);
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
	char uri[MAX_URI_SIZE];
	ucontact_info_t *ci;
	db_row_t *row;
	db_key_t columns[21];
	db1_res_t* res = NULL;
	str user, contact;
	char* domain;
	int i;
	int n;

	urecord_t* r;
	ucontact_t* c;

	columns[0] = &user_col;
	columns[1] = &contact_col;
	columns[2] = &expires_col;
	columns[3] = &q_col;
	columns[4] = &callid_col;
	columns[5] = &cseq_col;
	columns[6] = &flags_col;
	columns[7] = &cflags_col;
	columns[8] = &user_agent_col;
	columns[9] = &received_col;
	columns[10] = &path_col;
	columns[11] = &sock_col;
	columns[12] = &methods_col;
	columns[13] = &last_mod_col;
	columns[14] = &ruid_col;
	columns[15] = &instance_col;
	columns[16] = &reg_id_col;
	columns[17] = &srv_id_col;
	columns[18] = &con_id_col;
	columns[19] = &keepalive_col;
	columns[20] = &domain_col;

	if (ul_dbf.use_table(_c, _d->name) < 0) {
		LM_ERR("sql use_table failed\n");
		return -1;
	}

#ifdef EXTRA_DEBUG
	LM_NOTICE("load start time [%d]\n", (int)time(NULL));
#endif

	if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, (use_domain)?(21):(20), 0,
		0) < 0) {
			LM_ERR("db_query (1) failed\n");
			return -1;
		}
		if(ul_dbf.fetch_result(_c, &res, ul_fetch_rows)<0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if (ul_dbf.query(_c, 0, 0, 0, columns, 0, (use_domain)?(21):(20), 0,
		&res) < 0) {
			LM_ERR("db_query failed\n");
			return -1;
		}
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("table is empty\n");
		ul_dbf.free_result(_c, res);
		return 0;
	}


	n = 0;
	do {
		LM_DBG("loading records - cycle [%d]\n", ++n);
		for(i = 0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;

			user.s = (char*)VAL_STRING(ROW_VALUES(row));
			if (VAL_NULL(ROW_VALUES(row)) || user.s==0 || user.s[0]==0) {
				LM_CRIT("empty username record in table %s...skipping\n",
						_d->name->s);
				continue;
			}
			user.len = strlen(user.s);

			ci = dbrow2info(ROW_VALUES(row)+1, &contact, 0);
			if (ci==0) {
				LM_ERR("sipping record for %.*s in table %s\n",
						user.len, user.s, _d->name->s);
				continue;
			}

			if (use_domain) {
				domain = (char*)VAL_STRING(ROW_VALUES(row) + 20);
				if (VAL_NULL(ROW_VALUES(row)+17) || domain==0 || domain[0]==0){
					LM_CRIT("empty domain record for user %.*s...skipping\n",
							user.len, user.s);
					continue;
				}
				/* user.s cannot be NULL - checked previosly */
				user.len = snprintf(uri, MAX_URI_SIZE, "%.*s@%s",
					user.len, user.s, domain);
				user.s = uri;
				if (user.s[user.len]!=0) {
					LM_CRIT("URI '%.*s@%s' longer than %d\n", user.len, user.s,
							domain,	MAX_URI_SIZE);
					continue;
				}
			}

		
			lock_udomain(_d, &user);
			if (get_urecord(_d, &user, &r) > 0) {
				if (mem_insert_urecord(_d, &user, &r) < 0) {
					LM_ERR("failed to create a record\n");
					unlock_udomain(_d, &user);
					goto error;
				}
			}

			if ( (c=mem_insert_ucontact(r, &contact, ci)) == 0) {
				LM_ERR("inserting contact failed\n");
				unlock_udomain(_d, &user);
				goto error1;
			}

			/* We have to do this, because insert_ucontact sets state to CS_NEW
			 * and we have the contact in the database already */
			c->state = CS_SYNC;
			unlock_udomain(_d, &user);
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
	free_ucontact(c);
error:
	ul_dbf.free_result(_c, res);
	return -1;
}


/*!
 * \brief Loads from DB all contacts for an AOR
 * \param _c database connection
 * \param _d domain
 * \param _aor address of record
 * \return pointer to the record on success, 0 on errors or if nothing is found
 */
urecord_t* db_load_urecord(db1_con_t* _c, udomain_t* _d, str *_aor)
{
	char tname_buf[64];
	str tname;
	ucontact_info_t *ci;
	db_key_t columns[19];
	db_key_t keys[2];
	db_key_t order;
	db_val_t vals[2];
	db1_res_t* res = NULL;
	db_row_t *row;
	str aname;
	str avalue;
	sr_xval_t aval;
	str contact;
	char *domain;
	int i;

	urecord_t* r;
	ucontact_t* c;

	keys[0] = &user_col;
	vals[0].type = DB1_STR;
	vals[0].nul = 0;
	if (use_domain) {
		keys[1] = &domain_col;
		vals[1].type = DB1_STR;
		vals[1].nul = 0;
		domain = memchr(_aor->s, '@', _aor->len);
		vals[0].val.str_val.s   = _aor->s;
		if (domain==0) {
			vals[0].val.str_val.len = 0;
			vals[1].val.str_val = *_aor;
		} else {
			vals[0].val.str_val.len = domain - _aor->s;
			vals[1].val.str_val.s   = domain+1;
			vals[1].val.str_val.len = _aor->s + _aor->len - domain - 1;
		}
	} else {
		vals[0].val.str_val = *_aor;
	}

	columns[0] = &contact_col;
	columns[1] = &expires_col;
	columns[2] = &q_col;
	columns[3] = &callid_col;
	columns[4] = &cseq_col;
	columns[5] = &flags_col;
	columns[6] = &cflags_col;
	columns[7] = &user_agent_col;
	columns[8] = &received_col;
	columns[9] = &path_col;
	columns[10] = &sock_col;
	columns[11] = &methods_col;
	columns[12] = &last_mod_col;
	columns[13] = &ruid_col;
	columns[14] = &instance_col;
	columns[15] = &reg_id_col;
	columns[16] = &srv_id_col;
	columns[17] = &con_id_col;
	columns[18] = &keepalive_col;

	if (desc_time_order)
		order = &last_mod_col;
	else
		order = &q_col;

	if (ul_dbf.use_table(_c, _d->name) < 0) {
		LM_ERR("failed to use table %.*s\n", _d->name->len, _d->name->s);
		return 0;
	}

	if (ul_dbf.query(_c, keys, 0, vals, columns, (use_domain)?2:1, 16, order,
				&res) < 0) {
		LM_ERR("db_query failed\n");
		return 0;
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("aor %.*s not found in table %.*s\n",_aor->len, _aor->s, _d->name->len, _d->name->s);
		ul_dbf.free_result(_c, res);
		return 0;
	}

	r = 0;

	for(i = 0; i < RES_ROW_N(res); i++) {
		ci = dbrow2info(ROW_VALUES(RES_ROWS(res) + i), &contact, 1);
		if (ci==0) {
			LM_ERR("skipping record for %.*s in table %s\n",
					_aor->len, _aor->s, _d->name->s);
			continue;
		}
		
		if ( r==0 )
			get_static_urecord( _d, _aor, &r);

		if ( (c=mem_insert_ucontact(r, &contact, ci)) == 0) {
			LM_ERR("mem_insert failed\n");
			free_urecord(r);
			ul_dbf.free_result(_c, res);
			return 0;
		}

		/* We have to do this, because insert_ucontact sets state to CS_NEW
		 * and we have the contact in the database already */
		c->state = CS_SYNC;
	}

	ul_dbf.free_result(_c, res);

	/* Retrieve ul attributes */
	if(ul_xavp_contact_name.s==NULL) {
		/* feature disabled by mod param */
		goto done;
	}

	if(_d->name->len + 6>=64) {
		LM_ERR("attributes table name is too big\n");
		goto done;
	}
	strncpy(tname_buf, _d->name->s, _d->name->len);
	tname_buf[_d->name->len] = '\0';
	strcat(tname_buf, "_attrs");
	tname.s = tname_buf;
	tname.len = _d->name->len + 6;

	keys[0] = &ulattrs_ruid_col;
	vals[0].type = DB1_STR;
	vals[0].nul = 0;
	columns[0] = &ulattrs_aname_col;
	columns[1] = &ulattrs_atype_col;
	columns[2] = &ulattrs_avalue_col;

	if (ul_dbf.use_table(ul_dbh, &tname) < 0) {
		LM_ERR("sql use_table failed for %.*s\n", tname.len, tname.s);
		goto done;
	}

	for (c = r->contacts; c != NULL; c = c->next) {
		vals[0].val.str_val.s = c->ruid.s;
		vals[0].val.str_val.len = c->ruid.len;

		if (ul_dbf.query(ul_dbh, keys, 0, vals, columns, 1, 3, 0, &res) < 0) {
			LM_ERR("db_query failed\n");
			continue;
		}

		if (RES_ROW_N(res) == 0) {
			LM_DBG("location attrs table is empty\n");
			ul_dbf.free_result(ul_dbh, res);
			continue;
		}

		for(i = 0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;

			aname.s = (char*)VAL_STRING(ROW_VALUES(row));
			aname.len = strlen(aname.s);
			avalue.s = (char*)VAL_STRING(ROW_VALUES(row) + 2);
			avalue.len = strlen(avalue.s);
			memset(&aval, 0, sizeof(sr_xval_t));
			if(VAL_INT(ROW_VALUES(row)+1)==0) {
				/* string value */
				aval.v.s = avalue;
				aval.type = SR_XTYPE_STR;
			} else if(VAL_INT(ROW_VALUES(row)+1)==1) {
				/* int value */
				str2sint(&avalue, &aval.v.i);
				aval.type = SR_XTYPE_INT;
			} else {
				/* unknown type - ignore */
				continue;
			}

			/* add xavp to contact */
			if(c->xavp==NULL) {
				if(xavp_add_xavp_value(&ul_xavp_contact_name, &aname,
							&aval, &c->xavp)==NULL)
					LM_INFO("cannot add first xavp to contact - ignoring\n");
			} else {
				if(c->xavp->val.type==SR_XTYPE_XAVP) {
					if(xavp_add_value(&aname, &aval, &c->xavp->val.v.xavp)==NULL)
						LM_INFO("cannot add values to contact xavp\n");
				}
			}
		}
		ul_dbf.free_result(ul_dbh, res);
	}


done:
	return r;
}

/*!
 * \brief Loads from DB all contacts for a RUID
 * \param _c database connection
 * \param _d domain
 * \param _aor address of record
 * \return pointer to the record on success, 0 on errors or if nothing is found
 */
urecord_t* db_load_urecord_by_ruid(db1_con_t* _c, udomain_t* _d, str *_ruid)
{
	ucontact_info_t *ci;
	db_key_t columns[21];
	db_key_t keys[1];
	db_key_t order;
	db_val_t vals[1];
	db1_res_t* res = NULL;
	db_row_t *row;
	str contact;
	str aor;
	static char aorbuf[512];
	str domain;

	urecord_t* r;
	ucontact_t* c;

	keys[0] = &ruid_col;
	vals[0].type = DB1_STR;
	vals[0].nul = 0;
	vals[0].val.str_val = *_ruid;

	columns[0] = &contact_col;
	columns[1] = &expires_col;
	columns[2] = &q_col;
	columns[3] = &callid_col;
	columns[4] = &cseq_col;
	columns[5] = &flags_col;
	columns[6] = &cflags_col;
	columns[7] = &user_agent_col;
	columns[8] = &received_col;
	columns[9] = &path_col;
	columns[10] = &sock_col;
	columns[11] = &methods_col;
	columns[12] = &last_mod_col;
	columns[13] = &ruid_col;
	columns[14] = &instance_col;
	columns[15] = &reg_id_col;
	columns[16] = &srv_id_col;
	columns[17] = &con_id_col;
	columns[18] = &keepalive_col;
	columns[19] = &user_col;
	columns[20] = &domain_col;

	if (desc_time_order)
		order = &last_mod_col;
	else
		order = &q_col;

	if (ul_dbf.use_table(_c, _d->name) < 0) {
		LM_ERR("failed to use table %.*s\n", _d->name->len, _d->name->s);
		return 0;
	}

	if (ul_dbf.query(_c, keys, 0, vals, columns, 1, 18, order,
				&res) < 0) {
		LM_ERR("db_query failed\n");
		return 0;
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("aor %.*s not found in table %.*s\n",_ruid->len, _ruid->s,
				_d->name->len, _d->name->s);
		ul_dbf.free_result(_c, res);
		return 0;
	}

	r = 0;

	/* use first row - shouldn't be more */
	row = RES_ROWS(res);

	ci = dbrow2info(ROW_VALUES(RES_ROWS(res)), &contact, 1);
	if (ci==0) {
		LM_ERR("skipping record for %.*s in table %s\n",
				_ruid->len, _ruid->s, _d->name->s);
		goto done;
	}

	aor.s = (char*)VAL_STRING(ROW_VALUES(row) + 19);
	aor.len = strlen(aor.s);

	if (use_domain) {
		domain.s = (char*)VAL_STRING(ROW_VALUES(row) + 20);
		if (VAL_NULL(ROW_VALUES(row)+20) || domain.s==0 || domain.s[0]==0){
			LM_CRIT("empty domain record for user %.*s...skipping\n",
					aor.len, aor.s);
			goto done;
		}
		domain.len = strlen(domain.s);
		if(aor.len + domain.len + 2 >= 512) {
			LM_ERR("AoR is too big\n");
			goto done;
		}
		memcpy(aorbuf, aor.s, aor.len);
		aorbuf[aor.len] = '@';
		memcpy(aorbuf + aor.len + 1, domain.s, domain.len);
		aor.len += 1 + domain.len;
		aor.s = aorbuf;
		aor.s[aor.len] = '\0';
	}
	get_static_urecord( _d, &aor, &r);

	if ( (c=mem_insert_ucontact(r, &contact, ci)) == 0) {
		LM_ERR("mem_insert failed\n");
		free_urecord(r);
		ul_dbf.free_result(_c, res);
		return 0;
	}

	/* We have to do this, because insert_ucontact sets state to CS_NEW
	 * and we have the contact in the database already */
	c->state = CS_SYNC;

done:
	ul_dbf.free_result(_c, res);
	return r;
}


/*!
 * \brief Timer function to cleanup expired contacts, DB_ONLY db_mode
 * \param _d cleaned domain
 * \return 0 on success, -1 on failure
 */
int db_timer_udomain(udomain_t* _d)
{
	db_key_t keys[2];
	db_op_t  ops[2];
	db_val_t vals[2];

	keys[0] = &expires_col;
	ops[0] = "<";
	vals[0].nul = 0;
	UL_DB_EXPIRES_SET(&vals[0], act_time + 1);

	keys[1] = &expires_col;
	ops[1] = OP_NEQ;
	vals[1].nul = 0;
	UL_DB_EXPIRES_SET(&vals[1], 0);

	if (ul_dbf.use_table(ul_dbh, _d->name) < 0) {
		LM_ERR("use_table failed\n");
		return -1;
	}

	if (ul_dbf.delete(ul_dbh, keys, ops, vals, 2) < 0) {
		LM_ERR("failed to delete from table %s\n",_d->name->s);
		return -1;
	}

	return 0;
}


/*!
 * \brief performs a dummy query just to see if DB is ok
 * \param con database connection
 * \param d domain
 */
int testdb_udomain(db1_con_t* con, udomain_t* d)
{
	db_key_t key[2], col[1];
	db_val_t val[2];
	db1_res_t* res = NULL;

	if(ul_dbf.use_table(con, d->name) < 0) {
		LM_ERR("failed to change table\n");
		return -1;
	}

	key[0] = &user_col;
	key[1] = &domain_col;

	col[0] = &user_col;

	VAL_TYPE(val) = DB1_STRING;
	VAL_NULL(val) = 0;
	VAL_STRING(val) = "dummy_user";
	
	VAL_TYPE(val+1) = DB1_STRING;
	VAL_NULL(val+1) = 0;
	VAL_STRING(val+1) = "dummy_domain";

	if(ul_dbf.query(con, key, 0, val, col, (use_domain)?2:1, 1, 0, &res)<0) {
		if(res) ul_dbf.free_result( con, res);
		LM_ERR("failure in db_query\n");
		return -1;
	}

	ul_dbf.free_result( con, res);
	return 0;
}


/*!
 * \brief Insert a new record into domain in memory
 * \param _d domain the record belongs to
 * \param _aor address of record
 * \param _r new created record
 * \return 0 on success, -1 on failure
 */
int mem_insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	int sl;
	
	if (new_urecord(_d->name, _aor, _r) < 0) {
		LM_ERR("creating urecord failed\n");
		return -1;
	}

	sl = ((*_r)->aorhash)&(_d->size-1);
	slot_add(&_d->table[sl], *_r);
	update_stat( _d->users, 1);
	return 0;
}


/*!
 * \brief Remove a record from domain in memory
 * \param _d domain the record belongs to
 * \param _r deleted record
 */
void mem_delete_urecord(udomain_t* _d, struct urecord* _r)
{
	slot_rem(_r->slot, _r);
	free_urecord(_r);
	update_stat( _d->users, -1);
}


/*!
 * \brief Run timer handler for given domain
 * \param _d domain
 */
void mem_timer_udomain(udomain_t* _d, int istart, int istep)
{
	struct urecord* ptr, *t;
	int i;

	for(i=istart; i<_d->size; i+=istep)
	{
		lock_ulslot(_d, i);

		ptr = _d->table[i].first;

		while(ptr) {
			timer_urecord(ptr);
			/* Remove the entire record if it is empty */
			if (ptr->contacts == 0) {
				t = ptr;
				ptr = ptr->next;
				mem_delete_urecord(_d, t);
			} else {
				ptr = ptr->next;
			}
		}
		unlock_ulslot(_d, i);
	}
}


/*!
 * \brief Get lock for a domain
 * \param _d domain
 * \param _aor adress of record, used as hash source for the lock slot
 */
void lock_udomain(udomain_t* _d, str* _aor)
{
	unsigned int sl;
	if (db_mode!=DB_ONLY)
	{
		sl = ul_get_aorhash(_aor) & (_d->size - 1);

#ifdef GEN_LOCK_T_PREFERED
		lock_get(_d->table[sl].lock);
#else
		ul_lock_idx(_d->table[sl].lockidx);
#endif
	}
}


/*!
 * \brief Release lock for a domain
 * \param _d domain
 * \param _aor address of record, uses as hash source for the lock slot
 */
void unlock_udomain(udomain_t* _d, str* _aor)
{
	unsigned int sl;
	if (db_mode!=DB_ONLY)
	{
		sl = ul_get_aorhash(_aor) & (_d->size - 1);
#ifdef GEN_LOCK_T_PREFERED
		lock_release(_d->table[sl].lock);
#else
		ul_release_idx(_d->table[sl].lockidx);
#endif
	}
}

/*!
 * \brief  Get lock for a slot
 * \param _d domain
 * \param i slot number
 */
void lock_ulslot(udomain_t* _d, int i)
{
	if (db_mode!=DB_ONLY)
#ifdef GEN_LOCK_T_PREFERED
		lock_get(_d->table[i].lock);
#else
		ul_lock_idx(_d->table[i].lockidx);
#endif
}


/*!
 * \brief Release lock for a slot
 * \param _d domain
 * \param i slot number
 */
void unlock_ulslot(udomain_t* _d, int i)
{
	if (db_mode!=DB_ONLY)
#ifdef GEN_LOCK_T_PREFERED
		lock_release(_d->table[i].lock);
#else
		ul_release_idx(_d->table[i].lockidx);
#endif
}



/*!
 * \brief Create and insert a new record
 * \param _d domain to insert the new record
 * \param _aor address of the record
 * \param _r new created record
 * \return return 0 on success, -1 on failure
 */
int insert_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	if (db_mode!=DB_ONLY) {
		if (mem_insert_urecord(_d, _aor, _r) < 0) {
			LM_ERR("inserting record failed\n");
			return -1;
		}
	} else {
		get_static_urecord( _d, _aor, _r);
	}
	return 0;
}


/*!
 * \brief Obtain a urecord pointer if the urecord exists in domain
 * \param _d domain to search the record
 * \param _aor address of record
 * \param _r new created record
 * \return 0 if a record was found, 1 if nothing could be found
 */
int get_urecord(udomain_t* _d, str* _aor, struct urecord** _r)
{
	unsigned int sl, i, aorhash;
	urecord_t* r;

	if (db_mode!=DB_ONLY) {
		/* search in cache */
		aorhash = ul_get_aorhash(_aor);
		sl = aorhash&(_d->size-1);
		r = _d->table[sl].first;

		for(i = 0; r!=NULL && i < _d->table[sl].n; i++) {
			if((r->aorhash==aorhash) && (r->aor.len==_aor->len)
						&& !memcmp(r->aor.s,_aor->s,_aor->len)){
				*_r = r;
				return 0;
			}

			r = r->next;
		}
	} else {
		/* search in DB */
		r = db_load_urecord( ul_dbh, _d, _aor);
		if (r) {
			*_r = r;
			return 0;
		}
	}

	return 1;   /* Nothing found */
}

/*!
 * \brief Obtain a urecord pointer if the urecord exists in domain (lock slot)
 * \param _d domain to search the record
 * \param _aorhash hash id for address of record
 * \param _ruid record internal unique id
 * \param _r store pointer to location record
 * \param _c store pointer to contact structure
 * \return 0 if a record was found, -1 if nothing could be found
 */
int get_urecord_by_ruid(udomain_t* _d, unsigned int _aorhash,
		str *_ruid, struct urecord** _r, struct ucontact** _c)
{
	unsigned int sl, i;
	urecord_t* r;
	ucontact_t* c;

	sl = _aorhash&(_d->size-1);
	lock_ulslot(_d, sl);

	if (db_mode!=DB_ONLY) {
		/* search in cache */
		r = _d->table[sl].first;

		for(i = 0; i < _d->table[sl].n; i++) {
			if(r->aorhash==_aorhash) {
				c = r->contacts;
				while(c) {
					if(c->ruid.len==_ruid->len
							&& !memcmp(c->ruid.s, _ruid->s, _ruid->len)) {
						*_r = r;
						*_c = c;
						return 0;
					}
					c = c->next;
				}
			}
			r = r->next;
		}
	} else {
		/* search in DB */
		r = db_load_urecord_by_ruid(ul_dbh, _d, _ruid);
		if (r) {
			if(r->aorhash==_aorhash) {
				c = r->contacts;
				while(c) {
					if(c->ruid.len==_ruid->len
							&& !memcmp(c->ruid.s, _ruid->s, _ruid->len)) {
						*_r = r;
						*_c = c;
						return 0;
					}
					c = c->next;
				}
			}
		}
	}

	unlock_ulslot(_d, (_aorhash & (_d->size - 1)));
	return -1;   /* Nothing found */
}

/*!
 * \brief Delete a urecord from domain
 * \param _d domain where the record should be deleted
 * \param _aor address of record
 * \param _r deleted record
 * \return 0 on success, -1 if the record could not be deleted
 */
int delete_urecord(udomain_t* _d, str* _aor, struct urecord* _r)
{
	struct ucontact* c, *t;

	if (db_mode==DB_ONLY) {
		if (_r==0)
			get_static_urecord( _d, _aor, &_r);
		if (db_delete_urecord(_r)<0) {
			LM_ERR("DB delete failed\n");
			return -1;
		}
		free_urecord(_r);
		return 0;
	}

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
			LM_ERR("deleting contact failed\n");
			return -1;
		}
	}
	release_urecord(_r);
	return 0;
}


/*!
 * \brief Load all location attributes from an udomain
 *
 * Load all location attributes from a udomain, useful to populate the
 * memory cache on startup.
 * \param _d loaded domain
 * \return 0 on success, -1 on failure
 */
int uldb_preload_attrs(udomain_t *_d)
{
	char uri[MAX_URI_SIZE];
	str  suri;
	char tname_buf[64];
	str tname;
	db_row_t *row;
	db_key_t columns[6];
	db1_res_t* res = NULL;
	str user = {0};
	str domain = {0};
	str ruid;
	str aname;
	str avalue;
	sr_xval_t aval;
	int i;
	int n;

	urecord_t* r;
	ucontact_t* c;

	if(ul_xavp_contact_name.s==NULL) {
		/* feature disabled by mod param */
		return 0;
	}

	if(_d->name->len + 6>=64) {
		LM_ERR("attributes table name is too big\n");
		return -1;
	}
	strncpy(tname_buf, _d->name->s, _d->name->len);
	tname_buf[_d->name->len] = '\0';
	strcat(tname_buf, "_attrs");
	tname.s = tname_buf;
	tname.len = _d->name->len + 6;

	columns[0] = &ulattrs_user_col;
	columns[1] = &ulattrs_ruid_col;
	columns[2] = &ulattrs_aname_col;
	columns[3] = &ulattrs_atype_col;
	columns[4] = &ulattrs_avalue_col;
	columns[5] = &ulattrs_domain_col;

	if (ul_dbf.use_table(ul_dbh, &tname) < 0) {
		LM_ERR("sql use_table failed for %.*s\n", tname.len, tname.s);
		return -1;
	}

#ifdef EXTRA_DEBUG
	LM_NOTICE("load start time [%d]\n", (int)time(NULL));
#endif

	if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
		if (ul_dbf.query(ul_dbh, 0, 0, 0, columns, 0, (use_domain)?(6):(5), 0,
					0) < 0) {
			LM_ERR("db_query (1) failed\n");
			return -1;
		}
		if(ul_dbf.fetch_result(ul_dbh, &res, ul_fetch_rows)<0) {
			LM_ERR("fetching rows failed\n");
			return -1;
		}
	} else {
		if (ul_dbf.query(ul_dbh, 0, 0, 0, columns, 0, (use_domain)?(6):(5), 0,
		&res) < 0) {
			LM_ERR("db_query failed\n");
			return -1;
		}
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("location attrs table is empty\n");
		ul_dbf.free_result(ul_dbh, res);
		return 0;
	}


	n = 0;
	do {
		LM_DBG("loading records - cycle [%d]\n", ++n);
		for(i = 0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;

			user.s = (char*)VAL_STRING(ROW_VALUES(row));
			if (VAL_NULL(ROW_VALUES(row)) || user.s==0 || user.s[0]==0) {
				LM_CRIT("empty username record in table %s...skipping\n",
						_d->name->s);
				continue;
			}
			user.len = strlen(user.s);

			ruid.s = (char*)VAL_STRING(ROW_VALUES(row) + 1);
			ruid.len = strlen(ruid.s);
			aname.s = (char*)VAL_STRING(ROW_VALUES(row) + 2);
			aname.len = strlen(aname.s);
			avalue.s = (char*)VAL_STRING(ROW_VALUES(row) + 4);
			avalue.len = strlen(avalue.s);
			memset(&aval, 0, sizeof(sr_xval_t));
			if(VAL_INT(ROW_VALUES(row)+3)==0) {
				/* string value */
				aval.v.s = avalue;
				aval.type = SR_XTYPE_STR;
			} else if(VAL_INT(ROW_VALUES(row)+3)==1) {
				/* int value */
				str2sint(&avalue, &aval.v.i);
				aval.type = SR_XTYPE_INT;
			} else {
				/* unknown type - ignore */
				continue;
			}

			if (use_domain) {
				domain.s = (char*)VAL_STRING(ROW_VALUES(row) + 5);
				if (VAL_NULL(ROW_VALUES(row)+5) || domain.s==0 || domain.s[0]==0){
					LM_CRIT("empty domain record for user %.*s...skipping\n",
							user.len, user.s);
					continue;
				}
				domain.len = strlen(domain.s);
				/* user.s cannot be NULL - checked previosly */
				suri.len = snprintf(uri, MAX_URI_SIZE, "%.*s@%s",
					user.len, user.s, domain.s);
				suri.s = uri;
				if (suri.s[suri.len]!=0) {
					LM_CRIT("URI '%.*s@%s' longer than %d\n", user.len, user.s,
							domain.s, MAX_URI_SIZE);
					continue;
				}
			} else {
				suri = user;
			}

			if (get_urecord_by_ruid(_d, ul_get_aorhash(&suri), &ruid, &r, &c) < 0) {
				/* delete attrs records from db table */
				LM_INFO("no contact record for this ruid\n");
				uldb_delete_attrs(_d->name, &user, &domain, &ruid);
			} else {
				/* add xavp to contact */
				if(c->xavp==NULL) {
					if(xavp_add_xavp_value(&ul_xavp_contact_name, &aname,
								&aval, &c->xavp)==NULL)
						LM_INFO("cannot add first xavp to contact - ignoring\n");
				} else {
					if(c->xavp->val.type==SR_XTYPE_XAVP) {
						if(xavp_add_value(&aname, &aval, &c->xavp->val.v.xavp)==NULL)
							LM_INFO("cannot add values to contact xavp\n");
					}
				}
				/* get_urecord_by_ruid() locks the slot */
				unlock_udomain(_d, &suri);
			}
		}

		if (DB_CAPABILITY(ul_dbf, DB_CAP_FETCH)) {
			if(ul_dbf.fetch_result(ul_dbh, &res, ul_fetch_rows)<0) {
				LM_ERR("fetching rows (1) failed\n");
				ul_dbf.free_result(ul_dbh, res);
				return -1;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(res)>0);

	ul_dbf.free_result(ul_dbh, res);

#ifdef EXTRA_DEBUG
	LM_NOTICE("load end time [%d]\n", (int)time(NULL));
#endif

	return 0;
}
