/*
 * $Id$
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2004-10-04  first version (ramona)
 *  2004-11-11  added support for db schemes for avp_db_load (ramona)
 */


#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../db/db.h"
#include "../../dprint.h"
#include "avpops_parse.h"
#include "avpops_db.h"


static db_con_t  *db_hdl=0;     /* DB handler */
static db_func_t avpops_dbf;    /* DB functions */
static char      *def_table;    /* default DB table */
static char      **db_columns;  /* array with names of DB columns */

static db_key_t   keys_cmp[3]; /* array of keys and values used in selection */
static db_val_t   vals_cmp[3]; /* statement as in "select" and "delete" */

/* linked list with all defined DB schemes */
static struct db_scheme  *db_scheme_list=0;


int avpops_db_bind(char* db_url)
{
	if (bind_dbmod(db_url, &avpops_dbf ))
	{
		LOG(L_CRIT, "ERROR:avpops_db_bind: cannot bind to database module! "
			"Did you load a database module ?\n");
		return -1;
	}

	if (!DB_CAPABILITY(avpops_dbf, DB_CAP_ALL))
	{
		LOG(L_CRIT, "ERROR:avpops_db_bind: Database modules does not "
			"provide all functions needed by avpops module\n");
		return -1;
	}

	return 0;

}


int avpops_db_init(char* db_url, char* db_table, char **db_cols)
{
	db_hdl = avpops_dbf.init(db_url);
	if (db_hdl==0)
	{
		LOG(L_CRIT,"ERROR:avpops_db_init: cannot initialize database "
			"connection\n");
		goto error;
	}
	if (avpops_dbf.use_table(db_hdl, db_table)<0)
	{
		LOG(L_CRIT,"ERROR:avpops_db_init: cannot select table \"%s\"\n",
			db_table);
		goto error;
	}
	def_table = db_table;
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
		LOG(L_ERR,"ERROR:avpops:avp_add_db_scheme: no more pkg memory\n");
		goto error;
	}
	memset( scheme, 0, sizeof(struct db_scheme));

	/* parse the scheme */
	if ( parse_avp_db_scheme( (char*)val, scheme)!=0 )
	{
		LOG(L_ERR,"ERROR:avpops:avp_add_db_scheme: falied to parse scheme\n");
		goto error;
	}

	/* check for duplicates */
	if ( avp_get_db_scheme(scheme->name)!=0 )
	{
		LOG(L_ERR,"ERROR:avpops:avp_add_db_scheme: duplicated scheme name "
			"<%s>\n",scheme->name);
		goto error;
	}

	/* print scheme */
	DBG("DEBUG:avpops:avp_add_db_scheme: new scheme <%s> added\n"
		"\t\tuuid_col=<%s>\n\t\tusername_col=<%s>\n"
		"\t\tdomain_col=<%s>\n\t\tvalue_col=<%s>\n"
		"\t\tdb_flags=%d\n\t\ttable=<%s>\n",
		scheme->name,
		scheme->uuid_col, scheme->username_col,
		scheme->domain_col, scheme->value_col,
		scheme->db_flags, scheme->table	);

	scheme->next = db_scheme_list;
	db_scheme_list = scheme;

	return 0;
error:
	return -1;
}


struct db_scheme *avp_get_db_scheme (char *name)
{
	struct db_scheme *scheme;

	for( scheme=db_scheme_list ; scheme ; scheme=scheme->next )
		if ( !strcasecmp( name, scheme->name) )
			return scheme;
	return 0;
}


static inline int set_table( char *table, char *func)
{
	static int default_set = 1;

	if (table)
	{
		if ( avpops_dbf.use_table( db_hdl, table)<0 )
		{
			LOG(L_ERR,"ERROR:avpops:db-%s: cannot set table \"%s\"\n",
				func, table);
			return -1;
		}
		default_set = 0;
	} else if (!default_set){
		if ( avpops_dbf.use_table( db_hdl, def_table)<0 )
		{
			LOG(L_ERR,"ERROR:avpops:db-%s: cannot set table \"%s\"\n",
				func, def_table);
			return -1;
		}
		default_set = 1;
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
			(scheme&&scheme->uuid_col)?scheme->uuid_col:db_columns[0];
		vals_cmp[ nr_keys_cmp ].type = DB_STR;
		vals_cmp[ nr_keys_cmp ].nul  = 0;
		vals_cmp[ nr_keys_cmp ].val.str_val = *uuid;
		nr_keys_cmp++;
	} else {
		if (username)
		{
			/* username column */
			keys_cmp[ nr_keys_cmp ] =
			(scheme&&scheme->username_col)?scheme->username_col:db_columns[4];
			vals_cmp[ nr_keys_cmp ].type = DB_STR;
			vals_cmp[ nr_keys_cmp ].nul  = 0;
			vals_cmp[ nr_keys_cmp ].val.str_val = *username;
			nr_keys_cmp++;
		}
		if (domain)
		{
			/* domain column */
			keys_cmp[ nr_keys_cmp ] =
				(scheme&&scheme->domain_col)?scheme->domain_col:db_columns[5];
			vals_cmp[ nr_keys_cmp ].type = DB_STR;
			vals_cmp[ nr_keys_cmp ].nul  = 0;
			vals_cmp[ nr_keys_cmp ].val.str_val = *domain;
			nr_keys_cmp++;
		}
	}
	if (attr && scheme==0)
	{
		/* attribute name column */
		keys_cmp[ nr_keys_cmp ] = db_columns[1];
		vals_cmp[ nr_keys_cmp ].type = DB_STRING;
		vals_cmp[ nr_keys_cmp ].nul  = 0;
		vals_cmp[ nr_keys_cmp ].val.string_val = attr;
		nr_keys_cmp++;
	}
	return nr_keys_cmp;
}


db_res_t *db_load_avp( str *uuid, str *username, str *domain,
							char *attr, char *table, struct db_scheme *scheme)
{
	static db_key_t   keys_ret[3];
	unsigned int      nr_keys_cmp;
	unsigned int      nr_keys_ret;
	db_res_t          *res = NULL;

	/* prepare DB query */
	nr_keys_cmp = prepare_selection( uuid, username, domain, attr, scheme);

	/* set table */
	if (set_table( scheme?scheme->table:table ,"load")!=0)
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
		keys_ret[0] = scheme->value_col?scheme->value_col:db_columns[2];
		nr_keys_ret = 1;
	}

	/* do the DB query */
	if ( avpops_dbf.query( db_hdl, keys_cmp, 0/*op*/, vals_cmp, keys_ret,
			nr_keys_cmp, nr_keys_ret, 0/*order*/, &res) < 0)
		return 0;

	return res;
}


void db_close_query( db_res_t *res )
{
	DBG("close avp query\n");
	avpops_dbf.free_result( db_hdl, res);
}


int db_store_avp( db_key_t *keys, db_val_t *vals, int n, char *table)
{
	int r;

	if (set_table( table ,"store")!=0)
		return -1;

	r = avpops_dbf.insert( db_hdl, keys, vals, n);
	if (r<0)
	{
		LOG(L_ERR,"ERROR:avpops:db_store: insert failed\n");
		return -1;
	}
	return 0;
}



int db_delete_avp( str *uuid, str *username, str *domain, char *attr,
																char *table)
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

int db_query_avp(struct sip_msg *msg, char *query, itemname_list_t* dest)
{
	int_str avp_val;
	int_str avp_name;
	unsigned short avp_type;
	db_res_t* db_res = NULL;
	int i, j;
	itemname_list_t* crt;
	
	if(query==NULL)
	{
		LOG(L_ERR,"avpops:db_query_avp: error - bad parameter\n");
		return -1;
	}
	
	if(avpops_dbf.raw_query(db_hdl, query, &db_res)!=0)
	{
		LOG(L_ERR,"avpops:db_query_avp: error - cannot do the query\n");
		return -1;
	}

	if(db_res==NULL || RES_ROW_N(db_res)<=0 || RES_COL_N(db_res)<=0)
	{
		DBG("avpops:db_query_avp: no result after query\n");
		db_close_query( db_res );
		return 1;
	}

	DBG("avpops:db_query_avp: rows [%d]\n", RES_ROW_N(db_res));
	/* reverse order of rows so that first row get's in front of avp list */
	for(i = RES_ROW_N(db_res)-1; i >= 0; i--) 
	{
		DBG("avpops:db_query_avp: row [%d]\n", i);
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
				if(xl_get_avp_name(msg, &crt->sname, &avp_name, &avp_type)!=0)
				{
					LOG(L_ERR,
					"avpops:db_query_avp:error - cant get avp name [%d/%d]\n",
					i, j);
					goto next_avp;
				}
			}
			switch(RES_ROWS(db_res)[i].values[j].type)
			{
				case DB_STRING:
					avp_type |= AVP_VAL_STR;
					avp_val.s.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.string_val;
					avp_val.s.len=strlen(avp_val.s.s);
					if(avp_val.s.len<=0)
						goto next_avp;
				break;
				case DB_STR:
					avp_type |= AVP_VAL_STR;
					avp_val.s.len=
						RES_ROWS(db_res)[i].values[j].val.str_val.len;
					avp_val.s.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.str_val.s;
					if(avp_val.s.len<=0)
						goto next_avp;
				break;
				case DB_BLOB:
					avp_type |= AVP_VAL_STR;
					avp_val.s.len=
						RES_ROWS(db_res)[i].values[j].val.blob_val.len;
					avp_val.s.s=
						(char*)RES_ROWS(db_res)[i].values[j].val.blob_val.s;
					if(avp_val.s.len<=0)
						goto next_avp;
				break;
				case DB_INT:
					avp_val.n
						= (int)RES_ROWS(db_res)[i].values[j].val.int_val;
				break;
				case DB_DATETIME:
					avp_val.n
						= (int)RES_ROWS(db_res)[i].values[j].val.time_val;
				break;
				case DB_BITMAP:
					avp_val.n
						= (int)RES_ROWS(db_res)[i].values[j].val.bitmap_val;
				break;
				default:
					goto next_avp;
			}
			if(add_avp(avp_type, avp_name, avp_val)!=0)
			{
				LOG(L_ERR,"avpops:db_query_avp: error - unable to add avp\n");
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
