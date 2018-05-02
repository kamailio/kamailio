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
 *  \brief USRLOC - List of registered domains
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */


#include "dlist.h"
#include <stdlib.h>	       /* abort */
#include <string.h>            /* strlen, memcmp */
#include <stdio.h>             /* printf */
#include "../../core/ut.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/dprint.h"
#include "../../core/ip_addr.h"
#include "../../core/socket_info.h"
#include "udomain.h"           /* new_udomain, free_udomain */
#include "usrloc.h"
#include "utime.h"
#include "usrloc_mod.h"


/*! \brief Global list of all registered domains */
dlist_t* root = 0;

unsigned int _ul_max_partition = 0;

void ul_set_max_partition(unsigned int m)
{
	_ul_max_partition = m;
}

/*!
 * \brief Find domain with the given name
 * \param _n domain name
 * \param _d pointer to domain
 * \return 0 if the domain was found and 1 of not
 */
static inline int find_dlist(str* _n, dlist_t** _d)
{
	dlist_t* ptr;

	ptr = root;
	while(ptr) {
		if ((_n->len == ptr->name.len) &&
		    !memcmp(_n->s, ptr->name.s, _n->len)) {
			*_d = ptr;
			return 0;
		}
		
		ptr = ptr->next;
	}
	
	return 1;
}

/*!
 * \brief Get all contacts from the database, in partitions if wanted
 * \see get_all_ucontacts
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \param GAU options
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
static inline int get_all_db_ucontacts(void *buf, int len, unsigned int flags,
								unsigned int part_idx, unsigned int part_max,
								int options)
{
	struct socket_info *sock;
	unsigned int dbflags;
	db1_res_t* res = NULL;
	db_row_t *row;
	dlist_t *dom;
	int port, proto;
	char *p;
	str addr;
	str recv;
	str path;
	str ruid;
	str host;
	unsigned int aorhash;
	int i;
	void *cp;
	int shortage, needed;
	db_key_t keys1[4]; /* where */
	db_val_t vals1[4];
	db_op_t  ops1[4];
	db_key_t keys2[6]; /* select */
	int n[2] = {2,6}; /* number of dynamic values used on key1/key2 */

	cp = buf;
	shortage = 0;
	/* Reserve space for terminating 0000 */
	len -= sizeof(addr.len);

	aorhash = 0;

	/* select fields */
	keys2[0] = &received_col;
	keys2[1] = &contact_col;
	keys2[2] = &sock_col;
	keys2[3] = &cflags_col;
	keys2[4] = &path_col;
	keys2[5] = &ruid_col;

	/* where fields */
	keys1[0] = &expires_col;
	ops1[0] = OP_GT;
	vals1[0].nul = 0;
	UL_DB_EXPIRES_SET(&vals1[0], time(0));

	keys1[1] = &partition_col;
	ops1[1] = OP_EQ;
	vals1[1].type = DB1_INT;
	vals1[1].nul = 0;
	if(_ul_max_partition>0)
		vals1[1].val.int_val = part_idx;
	else
		vals1[1].val.int_val = 0;

	if (flags & nat_bflag) {
		keys1[n[0]] = &keepalive_col;
		ops1[n[0]] = OP_EQ;
		vals1[n[0]].type = DB1_INT;
		vals1[n[0]].nul = 0;
		vals1[n[0]].val.int_val = 1;
		n[0]++;
	}
	if(options&GAU_OPT_SERVER_ID) {
		keys1[n[0]] = &srv_id_col;
		ops1[n[0]] = OP_EQ;
		vals1[n[0]].type = DB1_INT;
		vals1[n[0]].nul = 0;
		vals1[n[0]].val.int_val = server_id;
		n[0]++;
	}

	/* TODO: use part_idx and part_max on keys1 */

	for (dom = root; dom!=NULL ; dom=dom->next) {
		if (ul_dbf.use_table(ul_dbh, dom->d->name) < 0) {
			LM_ERR("sql use_table failed\n");
			return -1;
		}
		if (ul_dbf.query(ul_dbh, keys1, ops1, vals1, keys2,
							n[0], n[1], NULL, &res) <0 ) {
			LM_ERR("query error\n");
			return -1;
		}
		if( RES_ROW_N(res)==0 ) {
			ul_dbf.free_result(ul_dbh, res);
			continue;
		}

		for(i = 0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;


			/* received */
			recv.s = (char*)VAL_STRING(ROW_VALUES(row));
			if ( VAL_NULL(ROW_VALUES(row)) || recv.s==0 || recv.s[0]==0 ) {
				recv.s = NULL;
				recv.len = 0;
			}
			else {
				recv.len = strlen(recv.s);
			}

			/* contact */
			addr.s = (char*)VAL_STRING(ROW_VALUES(row)+1);
			if (VAL_NULL(ROW_VALUES(row)+1) || addr.s==0 || addr.s[0]==0) {
				LM_ERR("empty contact -> skipping\n");
				continue;
			}
			else {
				addr.len = strlen(addr.s);
			}

			/* path */
			path.s = (char*)VAL_STRING(ROW_VALUES(row)+4);
			if (VAL_NULL(ROW_VALUES(row)+4) || path.s==0 || path.s[0]==0){
				path.s = NULL;
				path.len = 0;
			} else {
				path.len = strlen(path.s);
			}

			/* ruid */
			ruid.s = (char*)VAL_STRING(ROW_VALUES(row)+5);
			if (VAL_NULL(ROW_VALUES(row)+5) || ruid.s==0 || ruid.s[0]==0){
				ruid.s = NULL;
				ruid.len = 0;
			} else {
				ruid.len = strlen(ruid.s);
			}

			needed = (int)(sizeof(addr.len) + addr.len
					+ sizeof(recv.len) + recv.len
					+ sizeof(sock) + sizeof(dbflags)
					+ sizeof(path.len) + path.len
					+ sizeof(ruid.len) + ruid.len
					+ sizeof(aorhash));
			if (len < needed) {
				shortage += needed ;
				continue;
			}

			/* write contact */
			memcpy(cp, &addr.len, sizeof(addr.len));
			cp = (char*)cp + sizeof(addr.len);
			memcpy(cp, addr.s, addr.len);
			cp = (char*)cp + addr.len;

			/* write received */
			memcpy(cp, &recv.len, sizeof(recv.len));
			cp = (char*)cp + sizeof(recv.len);
			/* copy received only if exist */
			if(recv.len){
				memcpy(cp, recv.s, recv.len);
				cp = (char*)cp + recv.len;
			}

			/* sock */
			p  = (char*)VAL_STRING(ROW_VALUES(row) + 2);
			if (VAL_NULL(ROW_VALUES(row)+2) || p==0 || p[0]==0){
				sock = 0;
			} else {
				if (parse_phostport( p, &host.s, &host.len,
				&port, &proto)!=0) {
					LM_ERR("bad socket <%s>...set to 0\n", p);
					sock = 0;
				} else {
					sock = grep_sock_info( &host, (unsigned short)port, proto);
					if (sock==0) {
						LM_DBG("non-local socket <%s>...set to 0\n", p);
					}
				}
			}

			/* flags */
			dbflags = VAL_BITMAP(ROW_VALUES(row) + 3);

			/* write sock and flags */
			memcpy(cp, &sock, sizeof(sock));
			cp = (char*)cp + sizeof(sock);
			memcpy(cp, &dbflags, sizeof(dbflags));
			cp = (char*)cp + sizeof(dbflags);

			/* write path */
			memcpy(cp, &path.len, sizeof(path.len));
			cp = (char*)cp + sizeof(path.len);
			/* copy path only if exist */
			if(path.len){
				memcpy(cp, path.s, path.len);
				cp = (char*)cp + path.len;
			}

			/* write ruid */
			memcpy(cp, &ruid.len, sizeof(ruid.len));
			cp = (char*)cp + sizeof(ruid.len);
			/* copy ruid only if exist */
			if(ruid.len){
				memcpy(cp, ruid.s, ruid.len);
				cp = (char*)cp + ruid.len;
			}
			/* aorhash not used for db-only records, but it is added
			 * (as 0) to match the struct used for mem records */
			memcpy(cp, &aorhash, sizeof(aorhash));
			cp = (char*)cp + sizeof(aorhash);

			len -= needed;
		} /* row cycle */

		ul_dbf.free_result(ul_dbh, res);
	} /* domain cycle */

	/* len < 0 is possible, if size of the buffer < sizeof(c->c.len) */
	if (len >= 0)
		memset(cp, 0, sizeof(addr.len));

	/* Shouldn't happen */
	if (shortage > 0 && len > shortage) {
		abort();
	}

	shortage -= len;

	return shortage > 0 ? shortage : 0;
}


/*!
 * \brief Get all contacts from the memory, in partitions if wanted
 * \see get_all_ucontacts
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \param GAU options
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
static inline int get_all_mem_ucontacts(void *buf, int len, unsigned int flags,
								unsigned int part_idx, unsigned int part_max,
								int options)
{
	dlist_t *p;
	urecord_t *r;
	ucontact_t *c;
	void *cp;
	int shortage;
	int needed;
	int i = 0;
	cp = buf;
	shortage = 0;
	time_t tnow = 0;


	if(ul_keepalive_timeout>0)
		tnow = time(NULL);

	/* Reserve space for terminating 0000 */
	len -= sizeof(c->c.len);

	for (p = root; p != NULL; p = p->next) {

		for(i=0; i<p->d->size; i++) {

			if ( (i % part_max) != part_idx )
				continue;

			lock_ulslot(p->d, i);
			if(p->d->table[i].n<=0)
			{
				unlock_ulslot(p->d, i);
				continue;
			}
			for (r = p->d->table[i].first; r != NULL; r = r->next) {
				for (c = r->contacts; c != NULL; c = c->next) {
					if (c->c.len <= 0)
						continue;
					/*
					 * List only contacts that have all requested
					 * flags set
					 */
					if ((c->cflags & flags) != flags)
						continue;

					if(options&GAU_OPT_SERVER_ID && server_id!=c->server_id)
						continue;

					if(ul_keepalive_timeout>0 && c->last_keepalive>0)
					{
						if(c->sock!=NULL && c->sock->proto==PROTO_UDP)
						{
							if(c->last_keepalive+ul_keepalive_timeout < tnow)
							{
								/* set contact as expired in 10s */
								if(c->expires > tnow + 10)
									c->expires = tnow + 10;
								continue;
							}
						}
					}

					needed = (int)(sizeof(c->c.len) + c->c.len
							+ sizeof(c->received.len) + c->received.len
							+ sizeof(c->sock) + sizeof(c->cflags)
							+ sizeof(c->path.len) + c->path.len
							+ sizeof(c->ruid.len) + c->ruid.len
							+ sizeof(r->aorhash));
					if (len >= needed) {
						memcpy(cp, &c->c.len, sizeof(c->c.len));
						cp = (char*)cp + sizeof(c->c.len);
						memcpy(cp, c->c.s, c->c.len);
						cp = (char*)cp + c->c.len;
						memcpy(cp,&c->received.len,sizeof(c->received.len));
						cp = (char*)cp + sizeof(c->received.len);
						memcpy(cp, c->received.s, c->received.len);
						cp = (char*)cp + c->received.len;
						memcpy(cp, &c->sock, sizeof(c->sock));
						cp = (char*)cp + sizeof(c->sock);
						memcpy(cp, &c->cflags, sizeof(c->cflags));
						cp = (char*)cp + sizeof(c->cflags);
						memcpy(cp, &c->path.len, sizeof(c->path.len));
						cp = (char*)cp + sizeof(c->path.len);
						memcpy(cp, c->path.s, c->path.len);
						cp = (char*)cp + c->path.len;
						memcpy(cp, &c->ruid.len, sizeof(c->ruid.len));
						cp = (char*)cp + sizeof(c->ruid.len);
						memcpy(cp, c->ruid.s, c->ruid.len);
						cp = (char*)cp + c->ruid.len;
						memcpy(cp, &r->aorhash, sizeof(r->aorhash));
						cp = (char*)cp + sizeof(r->aorhash);
						len -= needed;
					} else {
						shortage += needed;
					}
				}
			}
			unlock_ulslot(p->d, i);
		}
	}
	/* len < 0 is possible, if size of the buffer < sizeof(c->c.len) */
	if (len >= 0)
		memset(cp, 0, sizeof(c->c.len));

	/* Shouldn't happen */
	if (shortage > 0 && len > shortage) {
		abort();
	}

	shortage -= len;

	return shortage > 0 ? shortage : 0;
}



/*!
 * \brief Get all contacts from the usrloc, in partitions if wanted
 *
 * Return list of all contacts for all currently registered
 * users in all domains. The caller must provide buffer of
 * sufficient length for fitting all those contacts. In the
 * case when buffer was exhausted, the function returns
 * estimated amount of additional space needed, in this
 * case the caller is expected to repeat the call using
 * this value as the hint.
 *
 * Information is packed into the buffer as follows:
 *
 * +------------+----------+-----+------+-----+
 * |contact1.len|contact1.s|sock1|flags1|path1|
 * +------------+----------+-----+------+-----+
 * |contact2.len|contact2.s|sock2|flags2|path1|
 * +------------+----------+-----+------+-----+
 * |..........................................|
 * +------------+----------+-----+------+-----+
 * |contactN.len|contactN.s|sockN|flagsN|pathN|
 * +------------+----------+-----+------+-----+
 * |000000000000|
 * +------------+
 *
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \param GAU options
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
int get_all_ucontacts(void *buf, int len, unsigned int flags,
								unsigned int part_idx, unsigned int part_max,
								int options)
{
	if (db_mode==DB_ONLY)
		return get_all_db_ucontacts( buf, len, flags, part_idx, part_max, options);
	else
		return get_all_mem_ucontacts( buf, len, flags, part_idx, part_max, options);
}



/**
 *
 */
int ul_refresh_keepalive(unsigned int _aorhash, str *_ruid)
{
	dlist_t *p;
	urecord_t *r;
	ucontact_t *c;
	int i;

	/* todo: get location domain via param */

	for (p = root; p != NULL; p = p->next)
	{
		i = _aorhash&(p->d->size-1);
		lock_ulslot(p->d, i);
		if(p->d->table[i].n<=0)
		{
			unlock_ulslot(p->d, i);
			continue;
		}
		for (r = p->d->table[i].first; r != NULL; r = r->next)
		{
			if(r->aorhash==_aorhash)
			{
				for (c = r->contacts; c != NULL; c = c->next)
				{
					if (c->c.len <= 0 || c->ruid.len<=0)
						continue;
					if(c->ruid.len==_ruid->len
							&& !memcmp(c->ruid.s, _ruid->s, _ruid->len))
					{
						/* found */
						c->last_keepalive = time(NULL);
						LM_DBG("updated keepalive for [%.*s:%u] to %u\n",
								_ruid->len, _ruid->s, _aorhash,
								(unsigned int)c->last_keepalive);
						unlock_ulslot(p->d, i);
						return 0;
					}
				}
			}
		}
		unlock_ulslot(p->d, i);
	}

	return 0;
}

/*!
 * \brief Create a new domain structure
 * \return 0 if everything went OK, otherwise value < 0 is returned
 *
 * \note The structure is NOT created in shared memory so the
 * function must be called before the server forks if it should
 * be available to all processes
 */
static inline int new_dlist(str* _n, dlist_t** _d)
{
	dlist_t* ptr;

	/* Domains are created before ser forks,
	 * so we can create them using pkg_malloc
	 */
	ptr = (dlist_t*)shm_malloc(sizeof(dlist_t));
	if (ptr == 0) {
		LM_ERR("no more share memory\n");
		return -1;
	}
	memset(ptr, 0, sizeof(dlist_t));

	/* copy domain name as null terminated string */
	ptr->name.s = (char*)shm_malloc(_n->len+1);
	if (ptr->name.s == 0) {
		LM_ERR("no more memory left\n");
		shm_free(ptr);
		return -2;
	}

	memcpy(ptr->name.s, _n->s, _n->len);
	ptr->name.len = _n->len;
	ptr->name.s[ptr->name.len] = 0;

	if (new_udomain(&(ptr->name), ul_hash_size, &(ptr->d)) < 0) {
		LM_ERR("creating domain structure failed\n");
		shm_free(ptr->name.s);
		shm_free(ptr);
		return -3;
	}

	*_d = ptr;
	return 0;
}

/*!
 * \brief Registers a new domain with usrloc
 *
 * Find and return a usrloc domain (location table)
 * \param _n domain name
 * \param _d usrloc domain
 * \return 0 on success, -1 on failure
 */
int get_udomain(const char* _n, udomain_t** _d)
{
	dlist_t* d;
	str s;

	s.s = (char*)_n;
	s.len = strlen(_n);

	if (find_dlist(&s, &d) == 0) {
		*_d = d->d;
		return 0;
	}
	*_d = NULL;
	return -1;
}

/*!
 * \brief Registers a new domain with usrloc
 *
 * Registers a new domain with usrloc. If the domain exists,
 * a pointer to existing structure will be returned, otherwise
 * a new domain will be created
 * \param _n domain name
 * \param _d new created domain
 * \return 0 on success, -1 on failure
 */
int register_udomain(const char* _n, udomain_t** _d)
{
	dlist_t* d;
	str s;
	db1_con_t* con;

	s.s = (char*)_n;
	s.len = strlen(_n);

	if (find_dlist(&s, &d) == 0) {
		*_d = d->d;
		return 0;
	}
	
	if (new_dlist(&s, &d) < 0) {
		LM_ERR("failed to create new domain\n");
		return -1;
	}

	/* Test tables from database if we are gonna
	 * to use database
	 */
	if (db_mode != NO_DB) {
		con = ul_dbf.init(&db_url);
		if (!con) {
			LM_ERR("failed to open database connection\n");
			goto err;
		}

		if(db_check_table_version(&ul_dbf, con, &s, UL_TABLE_VERSION) < 0) {
			LM_ERR("error during table version check.\n");
			goto err;
		}
		/* test if DB really exists */
		if (testdb_udomain(con, d->d) < 0) {
			LM_ERR("testing domain '%.*s' failed\n", s.len, ZSW(s.s));
			goto err;
		}

		ul_dbf.close(con);
	}

	d->next = root;
	root = d;
	
	*_d = d->d;
	return 0;

err:
	if (con) ul_dbf.close(con);
	free_udomain(d->d);
	shm_free(d->name.s);
	shm_free(d);
	return -1;
}


/*!
 * \brief Free all allocated memory for domains
 */
void free_all_udomains(void)
{
	dlist_t* ptr;

	while(root) {
		ptr = root;
		root = root->next;

		free_udomain(ptr->d);
		shm_free(ptr->name.s);
		shm_free(ptr);
	}
}


/*!
 * \brief Print all domains, just for debugging
 * \param _f output file
 */
void print_all_udomains(FILE* _f)
{
	dlist_t* ptr;
	
	ptr = root;

	fprintf(_f, "===Domain list===\n");
	while(ptr) {
		print_udomain(_f, ptr->d);
		ptr = ptr->next;
	}
	fprintf(_f, "===/Domain list===\n");
}


/*!
 * \brief Loops through all domains summing up the number of users
 * \return the number of users, could be zero
 */
unsigned long get_number_of_users(void)
{
	long numberOfUsers = 0;

	dlist_t* current_dlist;
	
	current_dlist = root;

	while (current_dlist)
	{
		numberOfUsers += get_stat_val(current_dlist->d->users); 
		current_dlist  = current_dlist->next;
	}

	return numberOfUsers;
}


/*!
 * \brief Run timer handler of all domains
 * \return 0 if all timer return 0, != 0 otherwise
 */
int synchronize_all_udomains(int istart, int istep)
{
	int res = 0;
	dlist_t* ptr;

	get_act_time(); /* Get and save actual time */

	if (db_mode==DB_ONLY) {
		for( ptr=root ; ptr ; ptr=ptr->next)
			res |= db_timer_udomain(ptr->d);
	} else {
		for( ptr=root ; ptr ; ptr=ptr->next)
			mem_timer_udomain(ptr->d, istart, istep);
	}

	return res;
}

/*!
 * \brief Run timer handler to clean all domains in db
 * \return 0 if all timer return 0, != 0 otherwise
 */
int ul_db_clean_udomains(void)
{
	int res = 0;
	dlist_t* ptr;

	get_act_time(); /* Get and save actual time */

	for( ptr=root ; ptr ; ptr=ptr->next)
		res |= db_timer_udomain(ptr->d);

	return res;
}

/*!
 * \brief Find a particular domain, small wrapper around find_dlist
 * \param _d domain name
 * \param _p pointer to domain if found
 * \return 1 if domain was found, 0 otherwise
 */
int find_domain(str* _d, udomain_t** _p)
{
	dlist_t* d;

	if (find_dlist(_d, &d) == 0) {
	        *_p = d->d;
		return 0;
	}

	return 1;
}
