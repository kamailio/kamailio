/* 
 * $Id$ 
 *
 * MySQL module core functions
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005 iptelorg GmbH
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libpq-fe.h>
#include <netinet/in.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../db/db_pool.h"
#include "../../ut.h"
#include "pg_con.h"
#include "pg_type.h"
#include "db_mod.h"
#include "res.h"
#include "dbase.h"


#define SELECTALL "select * "
#define SELECT    "select "
#define FROM      "from "
#define ORDER     "order by "
#define WHERE     "where "
#define AND       " and "
#define INSERT    "insert into "
#define VALUES    ") values ("
#define DELETE    "delete from "
#define UPDATE    "update "
#define SET       "set "


struct pg_params {
	int n;
	int cur;
	const char** data;
	int* len;
	int* formats;
};


static void free_pg_params(struct pg_params* ptr)
{
	if (!ptr) return;
	if (ptr->data) pkg_free(ptr->data);
	if (ptr->len) pkg_free(ptr->len);
	if (ptr->formats) pkg_free(ptr->formats);
	pkg_free(ptr);
}


static struct pg_params* new_pg_params(int n)
{
	struct pg_params* ptr;
	
	ptr = (struct pg_params*)pkg_malloc(sizeof(struct pg_params));
	if (!ptr) goto error;

	ptr->formats = (int*)pkg_malloc(sizeof(int) * n);
	if (!ptr->formats) goto error;

	ptr->data = (const char**)pkg_malloc(sizeof(const char*) * n);
	if (!ptr->data) goto error;

	ptr->len = (int*)pkg_malloc(sizeof(int) * n);
	if (!ptr->len) goto error;
	
	memset((char*)ptr->data, 0, sizeof(const char*) * n);
	memset(ptr->len, 0, sizeof(int) * n);
	ptr->n = n;
	ptr->cur = 0;
	return ptr;

 error:
	LOG(L_ERR, "postgres:new_pg_params: No memory left\n");
	free_pg_params(ptr);
	return 0;
}


static inline int params_add(struct pg_params* p, db_con_t* con, db_val_t* vals, int n)
{
	int i, i1, i2;
	db_val_t* val;

	if (!p) {
		LOG(L_ERR, "postgres:params_add: Invalid parameter value\n");
		return -1;
	}

	if (p->cur + n > p->n) {
		LOG(L_ERR, "postgres:params_add: Arrays too short (bug in postgres module)\n");
		return -1;
	}

	for(i = 0; i < n; i++) {
		val = &vals[i];
		p->formats[p->cur] = 1;
		switch(val->type) {
		case DB_INT:      
			if (!val->nul) {
				val->val.int_val = ntohl(val->val.int_val);
				p->data[p->cur] = (const char*)&val->val.int_val;
				p->len[p->cur] = 4;
			}
			break;
			
		case DB_DOUBLE:
			if (!val->nul) {
				     /* Change the byte order of 8-byte value to network
				      * byte order if necessary
				      */
				i1 = htonl(val->val.int8_val >> 32);
				i2 = htonl(val->val.int8_val & 0xffffffff);
				val->val.int_val = i1;
				(&val->val.int_val)[1] = i2;
				p->data[p->cur] = (const char*)&val->val.int_val;
				p->len[p->cur] = 8;
			}
			break;
			
		case DB_STRING:
			p->formats[p->cur] = 0;
			if (!val->nul) {
				p->data[p->cur] = val->val.string_val;
			}
			break;
			
		case DB_STR:
			if (!val->nul) {
				p->data[p->cur] = val->val.str_val.s;
				p->len[p->cur] = val->val.str_val.len;
			}
			break;
			
		case DB_DATETIME:
			if (!val->nul) {
				if (CON_FLAGS(con) & PG_INT8_TIMESTAMP) {
					val->val.int8_val = ((long long)val->val.time_val - PG_EPOCH_TIME) * 1000000;
				} else {
					val->val.double_val = (double)val->val.time_val - (double)PG_EPOCH_TIME;
					
				}
				i1 = htonl(val->val.int8_val >> 32);
				i2 = htonl(val->val.int8_val & 0xffffffff);
				val->val.int_val = i1;
				(&val->val.int_val)[1] = i2;
				p->data[p->cur] = (const char*)&val->val.int_val;
				p->len[p->cur] = 8;
			}
			break;
			
		case DB_BLOB:
			if (!val->nul) {
				p->data[p->cur] = val->val.blob_val.s;
				p->len[p->cur] = val->val.blob_val.len;
			}
			break;
			
		case DB_BITMAP: 
			if (!val->nul) {
				(&val->val.int_val)[1] = htonl(val->val.int_val);
				val->val.int_val = htonl(32);
				p->data[p->cur] = (const char*)&val->val.int_val;
				p->len[p->cur] = 8;
			}
			break;
		}
		
		p->cur++;
	}
	
	return 0;
}

static inline void free_params(struct pg_params* p)
{
	if (p->data) pkg_free(p->data);
	if (p->len) pkg_free(p->len);
	if (p->formats) pkg_free(p->formats);
}


/*
 * Initialize database module
 * No function should be called before this
 */
db_con_t* db_init(const char* url)
{
	struct db_id* id;
	struct pg_con* con;
	db_con_t* res;

	id = 0;
	res = 0;

	if (!url) {
		LOG(L_ERR, "postgres:db_init: Invalid parameter value\n");
		return 0;
	}

	res = pkg_malloc(sizeof(db_con_t) + sizeof(struct pg_con*));
	if (!res) {
		LOG(L_ERR, "postgres:db_init: No memory left\n");
		return 0;
	}
	memset(res, 0, sizeof(db_con_t) + sizeof(struct pg_con*));

	id = new_db_id(url);
	if (!id) {
		LOG(L_ERR, "postgres:db_init: Cannot parse URL '%s'\n", url);
		goto err;
	}

	     /* Find the connection in the pool */
	con = (struct pg_con*)pool_get(id);
	if (!con) {
		DBG("postgres:db_init: Connection '%s' not found in pool\n", url);
		     /* Not in the pool yet */
		con = new_connection(id);
		if (!con) {
			goto err;
		}
		pool_insert((struct pool_con*)con);
	} else {
		DBG("postgres:db_init: Connection '%s' found in pool\n", url);
	}

	res->tail = (unsigned long)con;
	return res;

 err:
	if (id) free_db_id(id);
	if (res) pkg_free(res);
	return 0;
}


/*
 * Shut down database module
 * No function should be called after this
 */
void db_close(db_con_t* handle)
{
	struct pool_con* con;

	if (!handle) {
		LOG(L_ERR, "postgres:db_close: Invalid parameter value\n");
		return;
	}

	con = (struct pool_con*)handle->tail;
	if (pool_remove(con) != 0) {
		free_connection((struct pg_con*)con);
	}

	pkg_free(handle);
}


static int calc_param_len(start, num)
{
	int max, len, order;

	if (!num) return 0;

	max = start + num - 1;
	len = num; /* $ */
	
	order = 0;
	while(max) {
		order++;
		max /= 10;
	}

	return len + order * num;
}

/*
 * Append a constant string, uses sizeof to figure the length
 * of the string
 */
#define append(buf, ptr)                                  \
    do {                                                  \
        if ((buf).len < (sizeof(ptr) - 1)) goto shortbuf; \
        memcpy((buf).s, (ptr), sizeof(ptr) - 1);          \
        (buf).s += sizeof(ptr) - 1;                       \
        (buf).len -= sizeof(ptr) - 1;                     \
    } while(0);


/*
 * Append zero terminated string, uses strlen to obtain the
 * length of the string
 */
#define append_str(buf, op)                  \
    do {                                     \
	int len;                             \
        len = strlen(op);                    \
        if ((buf).len < len) goto shortbuf;  \
        memcpy((buf).s, (op), len);          \
        (buf).s += len;                      \
        (buf).len -= len;                    \
    } while(0);


/*
 * Append a parameter, accepts the number of the
 * parameter to be appended
 */
#define append_param(buf, num)                  \
    do {                                        \
        const char* c;                          \
        int len;                                \
        c = int2str((num), &len);               \
        if ((buf).len < len + 1) goto shortbuf; \
        *(buf).s='$'; (buf).s++; (buf).len--;   \
        memcpy((buf).s, c, len);                \
        (buf).s += len; (buf).len -= len;       \
    } while(0); 


/*
 * Calculate the length of buffer needed to hold the insert query
 */
static unsigned int calc_insert_len(db_con_t* con, db_key_t* keys, int n)
{
	int i;
	unsigned int len;

	if (!n) return 0;

	len = sizeof(INSERT) - 1;
	len += strlen(CON_TABLE(con)); /* Table name */
	len += 2; /* _( */
	for(i = 0; i < n; i++) {
		len += strlen(keys[i]); /* Key names */
	}
	len += n - 1; /* , */
	len += sizeof(VALUES);
	len += calc_param_len(1, n);
	len += n - 1;
	len += 1; /* ) */
	return len;
}


/*
 * Calculate the length of buffer needed to hold the delete query
 */
static unsigned int calc_delete_len(db_con_t* con, db_key_t* keys, int n)
{
	int i;
	unsigned int len;

	len = sizeof(DELETE) - 1;
	len += strlen(CON_TABLE(con));
	if (n) {
		len += 1; /* _ */
		len += sizeof(WHERE) - 1;
		len += n * 2; /* <= */
		len += (sizeof(AND) - 1) * (n - 1);
		for(i = 0; i < n; i++) {
			len += strlen(keys[i]);
		}
		len += calc_param_len(1, n);
	}
	return len;
}

static unsigned int calc_select_len(db_con_t* con, db_key_t* cols, db_key_t* keys, int n, int ncol, db_key_t order)
{
	int i;
	unsigned int len;

	if (!cols) {
		len = sizeof(SELECTALL) - 1;
	} else {
		len = sizeof(SELECT);
		for(i = 0; i < ncol; i++) {
			len += strlen(cols[i]);
		}
		len += ncol - 1; /* , */
		len++; /* space */
	}
	len += sizeof(FROM) - 1;
	len += strlen(CON_TABLE(con));
	len += 1; /* _ */
	if (n) {
		len += sizeof(WHERE) - 1;
		len += n * 2; /* <= */
		len += (sizeof(AND) - 1) * (n - 1);
		for(i = 0; i < n; i++) {
			len += strlen(keys[i]);
		}
		len += calc_param_len(1, n);
		len++; /* space */
	}
	if (order) {
		len += sizeof(ORDER);
		len += strlen(order);
	}
	return len;
}

static unsigned int calc_update_len(db_con_t* con, db_key_t* ukeys, db_key_t* keys, int un, int n)
{
	int i;
	unsigned int len;

	if (!un) return 0;

	len = sizeof(UPDATE) - 1;
	len += strlen(CON_TABLE(con));
	len += 1; /* _ */
	len += sizeof(SET) - 1;
	len += un;  /* = */
	for (i = 0; i < un; i++) {
		len += strlen(ukeys[i]);
	}
	len += calc_param_len(1, un);
	len += un; /* , and last space */
	
	if (n) {
		len += sizeof(WHERE) - 1;
		len += n * 2; /* <= */
		len += (sizeof(AND) - 1) * (n - 1);
		for(i = 0; i < n; i++) {
			len += strlen(keys[i]);
		}
		len += calc_param_len(1 + un, n);
	}
	return len;
}


char* print_insert(db_con_t* con, db_key_t* keys, int n)
{
	unsigned int len;
	int i;
	char* s;
	str p;

	if (!n || !keys) {
		LOG(L_ERR, "postgres:print_insert: Nothing to insert\n");
		return 0;
	}

	len = calc_insert_len(con, keys, n);
	
	s = (char*)pkg_malloc(len + 1);
	if (!s) {
		LOG(L_ERR, "postgres:print_insert: Unable to allocate %d of memory\n", len);
		return 0;
	}
	p.s = s;
	p.len = len;
	
	append(p, INSERT);
	append_str(p, CON_TABLE(con));
	append(p, " (");

	append_str(p, keys[0]);
	for(i = 1; i < n; i++) {
		append(p, ",");
		append_str(p, keys[i]);
	}
	append(p, VALUES);

	append_param(p, 1);
	for(i = 1; i < n; i++) {
		append(p, ",");
		append_param(p, i + 1);
	}
	append(p, ")");
	*p.s = '\0';
	return s;

 shortbuf:
	LOG(L_ERR, "postgres:print_insert: Buffer too short (bug in postgres module)\n");
	pkg_free(s);
	return 0;
}



char* print_select(db_con_t* con, db_key_t* cols, db_key_t* keys, int n, int ncol, db_op_t* ops, db_key_t order)
{
	unsigned int len;
	int i;
	char* s;
	str p;

	len = calc_select_len(con, cols, keys, n, ncol, order);

	s = (char*)pkg_malloc(len + 1);
	if (!s) {
		LOG(L_ERR, "postrgres:print_select: Unable to allocate %d of memory\n", len);
		return 0;
	}
	p.s = s;
	p.len = len;
	
	if (!cols || !ncol) {
		append(p, SELECTALL);
	} else {
		append(p, SELECT);
		append_str(p, cols[0]);
		for(i = 1; i < ncol; i++) {
			append(p, ",");
			append_str(p, cols[i]);
		}
		append(p, " ");
	}
	append(p, FROM);
	append_str(p, CON_TABLE(con));
	append(p, " ");
	if (n) {
		append(p, WHERE);
		append_str(p, keys[0]);
		if (ops) {
			append_str(p, *ops);
			ops++;
		} else {
			append(p, "=");
		}
		append_param(p, 1);
		for(i = 1; i < n; i++) {
			append(p, AND);
			append_str(p, keys[i]);
			if (ops) {
				append_str(p, *ops);
				ops++;
			} else {
				append(p, "=");
			}
			append_param(p, i + 1);
		}
		append(p, " ");
	}
	if (order) {
		append(p, ORDER);
		append_str(p, order);
	}

	*p.s = '\0'; /* Zero termination */
	return s;

 shortbuf:
	LOG(L_ERR, "postgres:print_select: Buffer too short (bug in postgres module)\n");
	pkg_free(s);
	return 0;
}


char* print_delete(db_con_t* con, db_key_t* keys, db_op_t* ops, int n)
{
	unsigned int len;
	int i;
	char* s;
	str p;

	len = calc_delete_len(con, keys, n);

	s = (char*)pkg_malloc(len + 1);
	if (!s) {
		LOG(L_ERR, "postrgres:print_delete: Unable to allocate %d of memory\n", len);
		return 0;
	}
	p.s = s;
	p.len = len;

	append(p, DELETE);
	append_str(p, CON_TABLE(con));
	append(p, " ");
	if (n) {
		append(p, WHERE);
		append_str(p, keys[0]);
		if (ops) {
			append_str(p, *ops);
			ops++;
		} else {
			append(p, "=");
		}
		append_param(p, 1);
		for(i = 1; i < n; i++) {
			append(p, AND);
			append_str(p, keys[i]);
			if (ops) {
				append_str(p, *ops);
				ops++;
			} else {
				append(p, "=");
			}
			append_param(p, i + 1);
		}
	}

	*p.s = '\0';
	return s;

 shortbuf:
	LOG(L_ERR, "postgres:print_delete: Buffer too short (bug in postgres module)\n");
	pkg_free(s);
	return 0;
}


char* print_update(db_con_t* con, db_key_t* ukeys, db_key_t* keys, db_op_t* ops, int un, int n)
{
	unsigned int len, param_no;
	char* s;
	int i;
	str p;

	if (!un) {
		LOG(L_ERR, "postgres:print_update: Nothing to update\n");
		return 0;
	}

	param_no = 1;
	len = calc_update_len(con, ukeys, keys, un, n);

	s = (char*)pkg_malloc(len + 1);
	if (!s) {
		LOG(L_ERR, "postrgres:print_update: Unable to allocate %d of memory\n", len);
		return 0;
	}
	p.s = s;
	p.len = len;

	append(p, UPDATE);
	append_str(p, CON_TABLE(con));
	append(p, " " SET);

	append_str(p, ukeys[0]);
	append(p, "=");
	append_param(p, param_no++);

	for(i = 1; i < un; i++) {
		append(p, ",");
		append_str(p, ukeys[i]);
		append(p, "=");
		append_param(p, param_no++);
	}
	append(p, " ");

	if (n) {
		append(p, WHERE);
		append_str(p, keys[0]);
		if (ops) {
			append_str(p, *ops);
			ops++;
		} else {
			append(p, "=");
		}
		append_param(p, param_no++);
		
		for(i = 1; i < n; i++) {
			append(p, AND);
			append_str(p, keys[i]);
			if (ops) {
				append_str(p, *ops);
				ops++;
			} else {
				append(p, "=");
			}
			append_param(p, param_no++);
		}
	}	

	*p.s = '\0';
	return s;

 shortbuf:
	LOG(L_ERR, "postgres:print_update: Buffer too short (bug in postgres module)\n");
	pkg_free(s);
	return 0; 
}

/*
 * Return values: 1 Query failed, bad connection
 *                0 Query succeeded
 *               -1 Query failed due to some other reason
 */
static int submit_query(db_res_t** res, db_con_t* con, const char* query, struct pg_params* params)
{
	PGresult* pgres;

	DBG("postgres: Executing '%s'\n", query);
	if (params) {
	        pgres = PQexecParams(CON_CONNECTION(con), query,
				     params->n, 0,
				     params->data, params->len,
				     params->formats, 1);
	} else {
		pgres = PQexecParams(CON_CONNECTION(con), query, 0, 0, 0, 0, 0, 1);
	}
	switch(PQresultStatus(pgres)) {
	case PGRES_EMPTY_QUERY:
		LOG(L_ERR, "postgres:submit_query:BUG: db_raw_query received an empty query\n");
		goto error;
		
	case PGRES_COMMAND_OK:
	case PGRES_NONFATAL_ERROR:
	case PGRES_TUPLES_OK:
		     /* Success */
		break;
		
	case PGRES_COPY_OUT:
	case PGRES_COPY_IN:
		LOG(L_ERR, "postgres:submit_query: Unsupported transfer mode\n");
		goto error;

	case PGRES_BAD_RESPONSE:
	case PGRES_FATAL_ERROR:
		LOG(L_ERR, "postgres: Error: %s", PQresultErrorMessage(pgres));
		if (PQstatus(CON_CONNECTION(con)) != CONNECTION_BAD) {
				LOG(L_ERR, "postgres: Unknown error occurred, giving up\n");
				goto error;
		}
		LOG(L_ERR, "postgres:submit_query: Bad connection\n");
		PQclear(pgres);
		return 1;
	}

	if (res) {
		*res = new_result(pgres);
		if (!(*res)) goto error;
	} else {
		PQclear(pgres);
	}
	return 0;

 error:
	PQclear(pgres);
	return -1;
}


static int reconnect(db_con_t* con)
{
	int attempts_left = reconnect_attempts;
	while(attempts_left) {
		LOG(L_ERR, "postgres: Trying to recover the connection\n");
		PQreset(CON_CONNECTION(con));
		if (PQstatus(CON_CONNECTION(con)) == CONNECTION_OK) {
			LOG(L_ERR, "postgres: Successfuly reconnected\n");
			return 0;
		}
		LOG(L_ERR, "postgres: Reconnect attempt failed\n");
		attempts_left--;
	}
	LOG(L_ERR, "postgres: No more reconnect attempts left, giving up\n");
	return -1;
}


/*
 * Query table for specified rows
 * con:   structure representing database connection
 * keys:  key names
 * ops:   operators
 * vals:  values of the keys that must match
 * cols:  column names to return
 * n:     number of key=values pairs to compare
 * ncol:  number of columns to return
 * order: order by the specified column
 * res:   query result
 */
int db_query(db_con_t* con, db_key_t* keys, db_op_t* ops,
	     db_val_t* vals, db_key_t* cols, int n, int ncols,
	     db_key_t order, db_res_t** res)
{
	int ret;
	char* select;
	struct pg_params* params;
	
	params = 0;
	select = 0;

	if (!con) {
		LOG(L_ERR, "postgres:db_query: Invalid parameter value\n");
		return -1;
	}

	select = print_select(con, cols, keys, n, ncols, ops, order);
	if (!select) goto err;

	params = new_pg_params(n);
	if (!params) goto err;
	if (params_add(params, con, vals, n) < 0) goto err;

	do {
		ret = submit_query(res, con, select, params);
		if (ret < 0) goto err;                       /* Unknown error, bail out */
		if (ret > 0) {                               /* Disconnected, try to reconnect */
			if (reconnect(con) < 0) goto err;    /* Failed to reconnect */
			else continue;                       /* Try one more time (ret is > 0) */
		}
	} while(ret != 0);
	
	if (res && convert_result(*res, con) < 0) {
		free_result(*res);
		goto err;
	}

	free_pg_params(params);
	pkg_free(select);
	return 0;

 err:
	if (params) free_pg_params(params);
	if (select) pkg_free(select);
	return -1;
}


/*
 * Execute a raw SQL query
 */
int db_raw_query(db_con_t* con, char* query, db_res_t** res)
{
	int ret;

	if (!con || !query) {
		LOG(L_ERR, "postgres:db_raw_query: Invalid parameter value\n");
		return -1;
	}

	do {
		ret = submit_query(res, con, query, 0);
		if (ret < 0) return -1;                      /* Unknown error, bail out */
		if (ret > 0) {                               /* Disconnected, try to reconnect */
			if (reconnect(con) < 0) return -1;   /* Failed to reconnect */
			else continue;                       /* Try one more time (ret is > 0) */
		}
	} while(ret != 0);
	
	if (res && (convert_result(*res, con) < 0)) {
		free_result(*res);
		return -1;
	}
	return 0;
}


/*
 * Insert a row into specified table
 * con:  structure representing database connection
 * keys: key names
 * vals: values of the keys
 * n:    number of key=value pairs
 */
int db_insert(db_con_t* con, db_key_t* keys, db_val_t* vals, int n)
{
	int ret;
	char* insert;
	struct pg_params* params;

	if (!con || !keys || !vals || !n) {
		LOG(L_ERR, "postgres:db_insert: Invalid parameter value\n");
		return -1;
	}

	params = 0;
	insert = 0;

	insert = print_insert(con, keys, n);
	if (!insert) goto err;

	params = new_pg_params(n);
	if (!params) goto err;
	if (params_add(params, con, vals, n) < 0) goto err;

	do {
		ret = submit_query(0, con, insert, params);
		if (ret < 0) goto err;                       /* Unknown error, bail out */
		if (ret > 0) {                               /* Disconnected, try to reconnect */
			if (reconnect(con) < 0) goto err;    /* Failed to reconnect */
			else continue;                       /* Try one more time (ret is > 0) */
		}
	} while(ret != 0);
	
	free_pg_params(params);
	pkg_free(insert);
	return 0;

 err:
	if (params) free_pg_params(params);
	if (insert) pkg_free(insert);
	return -1;
}


/*
 * Delete a row from the specified table
 * con : structure representing database connection
 * keys: key names
 * ops : operators
 * vals: values of the keys that must match
 * n   : number of key=value pairs
 */
int db_delete(db_con_t* con, db_key_t* keys, db_op_t* ops, db_val_t* vals, int n)
{
	int ret;
	char* delete;
	struct pg_params* params;

	if (!con) {
		LOG(L_ERR, "postgres:db_insert: Invalid parameter value\n");
		return -1;
	}

	params = 0;
	delete = 0;

        delete = print_delete(con, keys, ops, n);
	if (!delete) goto err;

	params = new_pg_params(n);
	if (!params) goto err;
	if (params_add(params, con, vals, n) < 0) goto err;

	do {
		ret = submit_query(0, con, delete, params);
		if (ret < 0) goto err;                       /* Unknown error, bail out */
		if (ret > 0) {                               /* Disconnected, try to reconnect */
			if (reconnect(con) < 0) goto err;    /* Failed to reconnect */
			else continue;                       /* Try one more time (ret is > 0) */
		}
	} while(ret != 0);
	
	free_pg_params(params);
	pkg_free(delete);
	return 0;

 err:
	if (params) free_pg_params(params);
	if (delete) pkg_free(delete);
	return -1;
}


/*
 * Update some rows in the specified table
 * con  : structure representing database connection
 * keys : key names
 * ops  : operators
 * vals : values of the keys that must match
 * ucols: updated columns
 * uvals: updated values of the columns
 * n    : number of key=value pairs
 * un   : number of columns to update
 */
int db_update(db_con_t* con, db_key_t* keys, db_op_t* ops, db_val_t* vals,
	      db_key_t* ucols, db_val_t* uvals, int n, int un)
{
	int ret;
	char* update;
	struct pg_params* params;

	if (!con || !ucols || !uvals || !un) {
		LOG(L_ERR, "db_update: Invalid parameter value\n");
		return -1;
	}

	params = 0;
	update = 0;

	update = print_update(con, ucols, keys, ops, un, n);
	if (!update) goto err;

	params = new_pg_params(n + un);
	if (!params) goto err;
	if (params_add(params, con, uvals, un) < 0) goto err;
	if (params_add(params, con, vals, n) < 0) goto err;

	do {
		ret = submit_query(0, con, update, params);
		if (ret < 0) goto err;                       /* Unknown error, bail out */
		if (ret > 0) {                               /* Disconnected, try to reconnect */
			if (reconnect(con) < 0) goto err;    /* Failed to reconnect */
			else continue;                       /* Try one more time (ret is > 0) */
		}
	} while(ret != 0);

	free_pg_params(params);
	pkg_free(update);
	return 0;

 err:
	if (params) free_pg_params(params);
	if (update) pkg_free(update);
	return -1;
}


/*
 * Release a result set from memory
 */
int db_free_result(db_con_t* con, db_res_t* res)
{
	free_result(res);
	return 0;
}
