/**
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>

#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../ut.h"
#include "../../lib/srdb1/db.h"

#include "ht_db.h"

/** database connection */
db1_con_t *ht_db_con = NULL;
db_func_t ht_dbf;

str ht_array_size_suffix = str_init("::size");

/** db parameters */
str ht_db_url   = {0, 0};
str ht_db_name_column   = str_init("key_name");
str ht_db_ktype_column  = str_init("key_type");
str ht_db_vtype_column  = str_init("value_type");
str ht_db_value_column  = str_init("key_value");
str ht_db_expires_column= str_init("expires");
int ht_fetch_rows = 100;

/**
 * init module parameters
 */
int ht_db_init_params(void)
{
	if(ht_db_url.s==0)
		return 0;

	if(ht_fetch_rows<=0)
		ht_fetch_rows = 100;
	if(ht_array_size_suffix.s==NULL || ht_array_size_suffix.len<=0)
		ht_array_size_suffix.s = "::size";

	return 0;
}

/**
 * initialize database connection
 */
int ht_db_init_con(void)
{
	/* binding to DB module */
	if(db_bind_mod(&ht_db_url, &ht_dbf))
	{
		LM_ERR("database module not found\n");
		return -1;
	}

	if (!DB_CAPABILITY(ht_dbf, DB_CAP_ALL))
	{
		LM_ERR("database module does not "
		    "implement all functions needed by the module\n");
		return -1;
	}
	return 0;
}

/**
 * open database connection
 */
int ht_db_open_con(void)
{
	/* open a connection with the database */
	ht_db_con = ht_dbf.init(&ht_db_url);
	if(ht_db_con==NULL)
	{
		LM_ERR("failed to connect to the database\n");        
		return -1;
	}
	
	LM_DBG("database connection opened successfully\n");
	return 0;
}

/**
 * close database connection
 */
int ht_db_close_con(void)
{
	if (ht_db_con!=NULL && ht_dbf.close!=NULL)
		ht_dbf.close(ht_db_con);
	ht_db_con=NULL;
	return 0;
}

#define HT_NAME_BUF_SIZE	256
static char ht_name_buf[HT_NAME_BUF_SIZE];

/**
 * load content of a db table in hash table
 */
int ht_db_load_table(ht_t *ht, str *dbtable, int mode)
{
	db_key_t db_cols[5] = {&ht_db_name_column, &ht_db_ktype_column,
		&ht_db_vtype_column, &ht_db_value_column, &ht_db_expires_column};
	db_key_t db_ord = &ht_db_name_column;
	db1_res_t* db_res = NULL;
	str kname;
	str pname;
	str hname;
	str kvalue;
	int ktype;
	int vtype;
	int last_ktype;
	int n;
	int_str val;
	int_str expires;
	int i;
	int ret;
	int cnt;
	int now;
	int ncols;

	if(ht_db_con==NULL)
	{
		LM_ERR("no db connection\n");
		return -1;
	}

	if (ht_dbf.use_table(ht_db_con, dbtable) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	LM_DBG("=============== loading hash table [%.*s] from database [%.*s]\n",
			ht->name.len, ht->name.s, dbtable->len, dbtable->s);
	cnt = 0;
	ncols = 4;
	if(ht->htexpire > 0 && ht_db_expires_flag!=0)
		ncols = 5;

	if (DB_CAPABILITY(ht_dbf, DB_CAP_FETCH)) {
		if(ht_dbf.query(ht_db_con,0,0,0,db_cols,0,ncols,db_ord,0) < 0)
		{
			LM_ERR("Error while querying db\n");
			return -1;
		}
		if(ht_dbf.fetch_result(ht_db_con, &db_res, ht_fetch_rows)<0)
		{
			LM_ERR("Error while fetching result\n");
			if (db_res)
				ht_dbf.free_result(ht_db_con, db_res);
			goto error;
		} else {
			if(RES_ROW_N(db_res)==0)
			{
				LM_DBG("Nothing to be loaded in hash table\n");
				return 0;
			}
		}
	} else {
		if((ret=ht_dbf.query(ht_db_con, NULL, NULL, NULL, db_cols,
				0, ncols, db_ord, &db_res))!=0
			|| RES_ROW_N(db_res)<=0 )
		{
			if( ret==0)
			{
				ht_dbf.free_result(ht_db_con, db_res);
				return 0;
			} else {
				goto error;
			}
		}
	}

	pname.len = 0;
	pname.s = "";
	n = 0;
	last_ktype = 0;
	now = (int)time(NULL);
	do {
		for(i=0; i<RES_ROW_N(db_res); i++)
		{
			if(VAL_NULL(&RES_ROWS(db_res)[i].values[0])) {
				LM_ERR("key value must not be null\n");
				goto error;
			}

			if(RES_ROWS(db_res)[i].values[0].type!=DB1_STRING) {
				LM_ERR("key type must be string (type=%d)\n",
						RES_ROWS(db_res)[i].values[0].type);
				goto error;
			}

			kname.s = (char*)(RES_ROWS(db_res)[i].values[0].val.string_val);
			if(kname.s==NULL) {
				LM_ERR("null key in row %d\n", i);
				goto error;
			}
			kname.len = strlen(kname.s);

			expires.n = 0;
			if(ht->htexpire > 0 && ht_db_expires_flag!=0) {
				expires.n = RES_ROWS(db_res)[i].values[4].val.int_val;
				if (expires.n > 0 && expires.n < now) {
					LM_DBG("skipping expired entry [%.*s] (%d)\n", kname.len,
							kname.s, expires.n-now);
					continue;
				}
			}

			cnt++;
			switch(RES_ROWS(db_res)[i].values[1].type)
			{
			case DB1_INT:
				ktype = RES_ROWS(db_res)[i].values[1].val.int_val;
				break;
			case DB1_BIGINT:
				ktype = RES_ROWS(db_res)[i].values[1].val.ll_val;
				break;
			default:
				LM_ERR("Wrong db type [%d] for key_type column\n",
					RES_ROWS(db_res)[i].values[1].type);
				goto error;
			}
			if(last_ktype==1)
			{
				if(pname.len>0
						&& (pname.len!=kname.len
							|| strncmp(pname.s, kname.s, pname.len)!=0))
				{
					/* new key name, last was an array => add its size */
					snprintf(ht_name_buf, HT_NAME_BUF_SIZE, "%.*s%.*s",
						pname.len, pname.s, ht_array_size_suffix.len,
						ht_array_size_suffix.s);
					hname.s = ht_name_buf;
					hname.len = strlen(ht_name_buf);
					val.n = n;

					if(ht_set_cell(ht, &hname, 0, &val, mode))
					{
						LM_ERR("error adding array size to hash table.\n");
						goto error;
					}
					pname.len = 0;
					pname.s = "";
					n = 0;
				}
			}
			last_ktype = ktype;
			pname = kname;
			if(ktype==1)
			{
				snprintf(ht_name_buf, HT_NAME_BUF_SIZE, "%.*s[%d]",
						kname.len, kname.s, n);
				hname.s = ht_name_buf;
				hname.len = strlen(ht_name_buf);
				n++;
			} else {
				hname = kname;
			}
			switch(RES_ROWS(db_res)[i].values[2].type)
			{
			case DB1_INT:
				vtype = RES_ROWS(db_res)[i].values[2].val.int_val;
				break;
			case DB1_BIGINT:
				vtype = RES_ROWS(db_res)[i].values[2].val.ll_val;
				break;
			default:
				LM_ERR("Wrong db type [%d] for value_type column\n",
					RES_ROWS(db_res)[i].values[2].type);
				goto error;
			}

			/* add to hash */
			if(vtype==1)
			{
				switch(RES_ROWS(db_res)[i].values[3].type)
				{
				case DB1_STR:
					kvalue = RES_ROWS(db_res)[i].values[3].val.str_val;
					if(kvalue.s==NULL) {
						LM_ERR("null value in row %d\n", i);
						goto error;
					}
					str2sint(&kvalue, &val.n);
					break;
				case DB1_STRING:
					kvalue.s = (char*)(RES_ROWS(db_res)[i].values[3].val.string_val);
					if(kvalue.s==NULL) {
						LM_ERR("null value in row %d\n", i);
						goto error;
					}
					kvalue.len = strlen(kvalue.s);
					str2sint(&kvalue, &val.n);
					break;
				case DB1_INT:
					val.n = RES_ROWS(db_res)[i].values[3].val.int_val;
					break;
				case DB1_BIGINT:
					val.n = RES_ROWS(db_res)[i].values[3].val.ll_val;
					break;
				default:
					LM_ERR("Wrong db type [%d] for key_value column\n",
						RES_ROWS(db_res)[i].values[3].type);
					goto error;
				}
			} else {
				switch(RES_ROWS(db_res)[i].values[3].type)
				{
				case DB1_STR:
					kvalue = RES_ROWS(db_res)[i].values[3].val.str_val;
					if(kvalue.s==NULL) {
						LM_ERR("null value in row %d\n", i);
						goto error;
					}
					val.s = kvalue;
					break;
				case DB1_STRING:
					kvalue.s = (char*)(RES_ROWS(db_res)[i].values[3].val.string_val);
					if(kvalue.s==NULL) {
						LM_ERR("null value in row %d\n", i);
						goto error;
					}
					kvalue.len = strlen(kvalue.s);
					val.s = kvalue;
					break;
				case DB1_INT:
					kvalue.s = int2str(RES_ROWS(db_res)[i].values[3].val.int_val, &kvalue.len);
					val.s = kvalue;
					break;
				case DB1_BIGINT:
					kvalue.s = int2str(RES_ROWS(db_res)[i].values[3].val.ll_val, &kvalue.len);
					val.s = kvalue;
					break;
				default:
					LM_ERR("Wrong db type [%d] for key_value column\n",
						RES_ROWS(db_res)[i].values[3].type);
					goto error;
				}
			}
				
			if(ht_set_cell(ht, &hname, (vtype)?0:AVP_VAL_STR, &val, mode))
			{
				LM_ERR("error adding to hash table\n");
				goto error;
			}

			/* set expiry */
			if (ht->htexpire > 0 && expires.n > 0) {
				expires.n -= now;
				if(ht_set_cell_expire(ht, &hname, 0, &expires)) {
					LM_ERR("error setting expires to hash entry [%*.s]\n", hname.len, hname.s);
					goto error;
				}
			}
	 	}
		if (DB_CAPABILITY(ht_dbf, DB_CAP_FETCH)) {
			if(ht_dbf.fetch_result(ht_db_con, &db_res, ht_fetch_rows)<0) {
				LM_ERR("Error while fetching!\n");
				goto error;
			}
		} else {
			break;
		}
	}  while(RES_ROW_N(db_res)>0);

	if(last_ktype==1)
	{
		snprintf(ht_name_buf, HT_NAME_BUF_SIZE, "%.*s%.*s",
			pname.len, pname.s, ht_array_size_suffix.len,
			ht_array_size_suffix.s);
		hname.s = ht_name_buf;
		hname.len = strlen(ht_name_buf);
		val.n = n;
		if(ht_set_cell(ht, &hname, 0, &val, mode))
		{
			LM_ERR("error adding array size to hash table.\n");
			goto error;
		}
	}

	ht_dbf.free_result(ht_db_con, db_res);
	LM_DBG("loaded %d values in hash table\n", cnt);

	return 0;
error:
	ht_dbf.free_result(ht_db_con, db_res);
	return -1;

}

/**
 * save hash table content back to database
 */
int ht_db_save_table(ht_t *ht, str *dbtable)
{
	db_key_t db_cols[5] = {&ht_db_name_column, &ht_db_ktype_column,
		&ht_db_vtype_column, &ht_db_value_column, &ht_db_expires_column};
	db_val_t db_vals[5];
	ht_cell_t *it;
	str tmp;
	int i;
	time_t now;
	int ncols;

	if(ht_db_con==NULL)
	{
		LM_ERR("no db connection\n");
		return -1;
	}

	if (ht_dbf.use_table(ht_db_con, dbtable) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	LM_DBG("save the content of hash table [%.*s] to database in [%.*s]\n",
			ht->name.len, ht->name.s, dbtable->len, dbtable->s);

	now = time(NULL);

	for(i=0; i<ht->htsize; i++)
	{
		ht_slot_lock(ht, i);
		it = ht->entries[i].first;
		while(it)
		{
			if(it->flags&AVP_VAL_STR) {
				LM_DBG("entry key: [%.*s] value: [%.*s] (str)\n",
					it->name.len, it->name.s, it->value.s.len, it->value.s.s);
			} else {
				LM_DBG("entry key: [%.*s] value: [%d] (int)\n",
					it->name.len, it->name.s, it->value.n);
			}

			if(ht->htexpire > 0) {
				if (it->expire <= now) {
					LM_DBG("skipping expired entry");
					it = it->next;
					continue;
				}
			}

			db_vals[0].type = DB1_STR;
			db_vals[0].nul  = 0;
			db_vals[0].val.str_val.s   = it->name.s;
			db_vals[0].val.str_val.len = it->name.len;

			db_vals[1].type = DB1_INT;
			db_vals[1].nul = 0;
			db_vals[1].val.int_val = 0;

			db_vals[2].type = DB1_INT;
			db_vals[2].nul = 0;

			db_vals[3].type = DB1_STR;
			db_vals[3].nul  = 0;
			if(it->flags&AVP_VAL_STR) {
				db_vals[2].val.int_val = 0;
				db_vals[3].val.str_val.s   = it->value.s.s;
				db_vals[3].val.str_val.len = it->value.s.len;
			} else {
				db_vals[2].val.int_val = 1;
				tmp.s = sint2str((long)it->value.n, &tmp.len);
				db_vals[3].val.str_val.s   = tmp.s;
				db_vals[3].val.str_val.len = tmp.len;
			}
			ncols = 4;

			if(ht_db_expires_flag!=0 && ht->htexpire > 0) {
				db_vals[4].type = DB1_INT;
				db_vals[4].nul = 0;
				db_vals[4].val.int_val = (int)it->expire;
				ncols = 5;
			}
			if(ht_dbf.insert(ht_db_con, db_cols, db_vals, ncols) < 0)
			{
				LM_ERR("failed to store key [%.*s] in table [%.*s]\n",
						it->name.len, it->name.s,
						dbtable->len, dbtable->s);
			}
			it = it->next;
		}
		ht_slot_unlock(ht, i);
	}
	return 0;
}


/**
 * delete databse table
 */
int ht_db_delete_records(str *dbtable)
{
	if(ht_db_con==NULL)
	{
		LM_ERR("no db connection\n");
		return -1;
	}

	if (ht_dbf.use_table(ht_db_con, dbtable) < 0)
	{
		LM_ERR("failed to use_table\n");
		return -1;
	}

	if(ht_dbf.delete(ht_db_con, NULL, NULL, NULL, 0) < 0)
		LM_ERR("failed to delete db records in [%.*s]\n",
				dbtable->len, dbtable->s);
	return 0;
}
