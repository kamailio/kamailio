/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../dprint.h"
#include "avpops_parse.h"
#include "avpops_db.h"


static db1_con_t  *db_hdl=0;     /* DB handler */
static db_func_t avpops_dbf;    /* DB functions */
static str       def_table;    /* default DB table */
static str      **db_columns;  /* array with names of DB columns */

static db_key_t   keys_cmp[3]; /* array of keys and values used in selection */
static db_val_t   vals_cmp[3]; /* statement as in "select" and "delete" */

/* linked list with all defined DB schemes */
static struct db_scheme  *db_scheme_list=0;


int avpops_db_bind(const str* db_url)
{
	if (db_bind_mod(db_url, &avpops_dbf ))
	{
		LM_CRIT("cannot bind to database module! "
			"Did you load a database module ?\n");
		return -1;
	}

	if (!DB_CAPABILITY(avpops_dbf, DB_CAP_ALL))
	{
		LM_CRIT("database modules does not "
			"provide all functions needed by avpops module\n");
		return -1;
	}

	return 0;

}


int avpops_db_init(const str* db_url, const str* db_table, str** db_cols)
{
	
	db_hdl = avpops_dbf.init(db_url);
	if (db_hdl==0)
	{
		LM_ERR("cannot initialize database connection\n");
		goto error;
	}
	if (avpops_dbf.use_table(db_hdl, db_table)<0)
	{
		LM_ERR("cannot select table \"%.*s\"\n", db_table->len, db_table->s);
		goto error;
	}
	def_table.s = db_table->s;
	def_table.len = db_table->len;
	db_columns = db_cols;

	return 0;
error:
	if (db_hdl)
	{
		avpops_dbf.close(db_hdl);
		db_hdl=0;
	}
	return -1;
}


int avp_add_db_scheme( modparam_t type, void* val)
{
	struct db_scheme *scheme;

	scheme = (struct db_scheme*)pkg_malloc( sizeof(struct db_scheme) );
	if (scheme==0)
	{
		LM_ERR("no more pkg memory\n");
		goto error;
	}
	memset( scheme, 0, sizeof(struct db_scheme));

	/* parse the scheme */
	if ( parse_avp_db_scheme( (char*)val, scheme)!=0 )
	{
		LM_ERR("failed to parse scheme\n");
		goto error;
	}

	/* check for duplicates */
	if ( avp_get_db_scheme(&scheme->name)!=0 )
	{
		LM_ERR("duplicated scheme name <%.*s>\n",
			scheme->name.len,scheme->name.s);
		goto error;
	}

	/* print scheme */
	LM_DBG("new scheme <%.*s> added\n"
		"\t\tuuid_col=<%.*s>\n\t\tusername_col=<%.*s>\n"
		"\t\tdomain_col=<%.*s>\n\t\tvalue_col=<%.*s>\n"
		"\t\tdb_flags=%d\n\t\ttable=<%.*s>\n",
		scheme->name.len,scheme->name.s,
		scheme->uuid_col.len, scheme->uuid_col.s, scheme->username_col.len,
		scheme->username_col.s, scheme->domain_col.len, scheme->domain_col.s,
		scheme->value_col.len, scheme->value_col.s, scheme->db_flags,
		scheme->table.len, scheme->table.s);

	scheme->next = db_scheme_list;
	db_scheme_list = scheme;

	return 0;
error:
	return -1;
}


struct db_scheme *avp_get_db_scheme (str *name)
{
	struct db_scheme *scheme;

	for( scheme=db_scheme_list ; scheme ; scheme=scheme->next )
		if ( name->len==scheme->name.len &&
		!strcasecmp( name->s, scheme->name.s) )
			return scheme;
	return 0;
}


static inline int set_table( const str *table, char *func)
{
	if (table && table->s)
	{
		if ( avpops_dbf.use_table( db_hdl, table)<0 )
		{
			LM_ERR("db-%s: cannot set table \"%.*s\"\n", func, table->len, table->s);
			return -1;
		}
	} else {
		if ( avpops_dbf.use_table( db_hdl, &def_table)<0 )
		{
			LM_ERR("db-%s: cannot set table \"%.*s\"\n", func, def_table.len, def_table.s);
			return -1;
		}
	}
	return 0;
}



static inline int prepare_selection( str *uuid, str *username, str *domain,
										char *attr, struct db_scheme *scheme)
{
	unsigned int nr_keys_cmp;

	nr_keys_cmp = 0;
	if (uuid)
	{
		/* uuid column */
		keys_cmp[ nr_keys_cmp ] =
			(scheme&&scheme->uuid_col.s)?&scheme->uuid_col:db_columns[0];
		vals_cmp[ nr_keys_cmp ].type = DB1_STR;
		vals_cmp[ nr_keys_cmp ].nul  = 0;
		vals_cmp[ nr_keys_cmp ].val.str_val = *uuid;
		nr_keys_cmp++;
	} else {
		if (username)
		{
			/* username column */
			keys_cmp[ nr_keys_cmp ] =
			(scheme&&scheme->username_col.s)?&scheme->username_col:db_columns[4];
			vals_cmp[ nr_keys_cmp ].type = DB1_STR;
			vals_cmp[ nr_keys_cmp ].nul  = 0;
			vals_cmp[ nr_keys_cmp ].val.str_val = *username;
			nr_keys_cmp++;
		}
		if (domain)
		{
			/* domain column */
			keys_cmp[ nr_keys_cmp ] =
			(scheme&&scheme->domain_col.s)?&scheme->domain_col:db_columns[5];
			vals_cmp[ nr_keys_cmp ].type = DB1_STR;
			vals_cmp[ nr_keys_cmp ].nul  = 0;
			vals_cmp[ nr_keys_cmp ].val.str_val = *domain;
			nr_keys_cmp++;
		}
	}
	if (attr && scheme==0)
	{
		/* attribute name column */
		keys_cmp[ nr_keys_cmp ] = db_columns[1];
		vals_cmp[ nr_keys_cmp ].type = DB1_STRING;
		vals_cmp[ nr_keys_cmp ].nul  = 0;
		vals_cmp[ nr_keys_cmp ].val.string_val = attr;
		nr_keys_cmp++;
	}
	return nr_keys_cmp;
}


db1_res_t *db_load_avp( str *uuid, str *username, str *domain,
							char *attr, const str *table, struct db_scheme *scheme)
{
	static db_key_t   keys_ret[3];
	unsigned int      nr_keys_cmp;
	unsigned int      nr_keys_ret;
	db1_res_t          *res = NULL;

	/* prepare DB query */
	nr_keys_cmp = prepare_selection( uuid, username, domain, attr, scheme);

	/* set table */
	if (set_table( scheme?&scheme->table:table ,"load")!=0)
		return 0;

	/* return keys */
	if (scheme==0)
	{
		keys_ret[0] = db_columns[2]; /*value*/
		keys_ret[1] = db_columns[1]; /*attribute*/
		keys_ret[2] = db_columns[3]; /*type*/
		nr_keys_ret = 3;
	} else {
		/* value */
		keys_ret[0] = scheme->value_col.s?&scheme->value_col:db_columns[2];
		nr_keys_ret = 1;
	}

	/* do the DB query */
	if ( avpops_dbf.query( db_hdl, keys_cmp, 0/*op*/, vals_cmp, keys_ret,
			nr_keys_cmp, nr_keys_ret, 0/*order*/, &res) < 0)
		return 0;

	return res;
}


void db_close_query( db1_res_t *res )
{
	LM_DBG("close avp query\n");
	avpops_dbf.free_result( db_hdl, res);
}


int db_store_avp( db_key_t *keys, db_val_t *vals, int n, const str *table)
{
	int r;
	if (set_table( table ,"store")!=0)
		return -1;

	r = avpops_dbf.insert( db_hdl, keys, vals, n);
	if (r<0)
	{
		LM_ERR("insert failed\n");
		return -1;
	}
	return 0;
}



int db_delete_avp( str *uuid, str *username, str *domain, char *attr,
															const str *table)
{
	unsigned int  nr_keys_cmp;

	/* prepare DB query */
	nr_keys_cmp = prepare_selection( uuid, username, domain, attr, 0);

	/* set table */
	if (set_table( table ,"delete")!=0)
		return -1;

	/* do the DB query */
	if ( avpops_dbf.delete( db_hdl, keys_cmp, 0, vals_cmp, nr_keys_cmp) < 0)
		return 0;

	return 0;
}

int db_query_avp(struct sip_msg *msg, char *query, pvname_list_t* dest)
{
	int_str avp_val;
	int_str avp_name;
	unsigned short avp_type;
	db1_res_t* db_res = NULL;
	int i, j;
	pvname_list_t* crt;
	static str query_str;
	
	if(query==NULL)
	{
		LM_ERR("bad parameter\n");
		return -1;
	}
	
	query_str.s = query;
	query_str.len = strlen(query);
	
	if(avpops_dbf.raw_query(db_hdl, &query_str, &db_res)!=0)
	{
		LM_ERR("cannot do the query\n");
		return -1;
	}

	if(db_res==NULL || RES_ROW_N(db_res)<=0 || RES_COL_N(db_res)<=0)
	{
		LM_DBG("no result after query\n");
		db_close_query( db_res );
		return -2;
	}

	LM_DBG("rows [%d]\n", RES_ROW_N(db_res));
	/* reverse order of rows so that first row get's in front of avp list */
	for(i = RES_ROW_N(db_res)-1; i >= 0; i--) 
	{
		LM_DBG("row [%d]\n", i);
		crt = dest;
		for(j = 0; j < RES_COL_N(db_res); j++) 
		{
			if(RES_ROWS(db_res)[i].values[j].nul)
				goto next_avp;
			avp_type = 0;
			if(crt==NULL)
			{
				avp_name.n = j+1;
			} else {
				if(pv_get_avp_name(msg, &crt->sname.pvp, &avp_name,
							&avp_type)!=0)
				{
					LM_ERR("cant get avp name [%d/%d]\n", i, j);
					goto next_avp;
				}
			}
			switch(RES_ROWS(db_res)[i].values[j].type)
			{
				case DB1_STRING:
					avp_type |= AVP_VAL_STR;
					avp_val.s.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.string_val;
					avp_val.s.len=strlen(avp_val.s.s);
					if(avp_val.s.len<=0)
						goto next_avp;
				break;
				case DB1_STR:
					avp_type |= AVP_VAL_STR;
					avp_val.s.len=
						RES_ROWS(db_res)[i].values[j].val.str_val.len;
					avp_val.s.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.str_val.s;
					if(avp_val.s.len<=0)
						goto next_avp;
				break;
				case DB1_BLOB:
					avp_type |= AVP_VAL_STR;
					avp_val.s.len=
						RES_ROWS(db_res)[i].values[j].val.blob_val.len;
					avp_val.s.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.blob_val.s;
					if(avp_val.s.len<=0)
						goto next_avp;
				break;
				case DB1_INT:
					avp_val.n
						= (int)RES_ROWS(db_res)[i].values[j].val.int_val;
				break;
				case DB1_BIGINT:
					avp_val.n
						= (int)RES_ROWS(db_res)[i].values[j].val.ll_val;
				break;
				case DB1_DATETIME:
					avp_val.n
						= (int)RES_ROWS(db_res)[i].values[j].val.time_val;
				break;
				case DB1_BITMAP:
					avp_val.n
						= (int)RES_ROWS(db_res)[i].values[j].val.bitmap_val;
				break;
				default:
					goto next_avp;
			}
			if(add_avp(avp_type, avp_name, avp_val)!=0)
			{
				LM_ERR("unable to add avp\n");
				db_close_query( db_res );
				return -1;
			}
next_avp:
			if(crt)
			{
				crt = crt->next;
				if(crt==NULL)
					break;
			}
		}
	}

	db_close_query( db_res );
	return 0;
}
