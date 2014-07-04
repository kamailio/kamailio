/*
 * $Id$
 *
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * History:
 * ---------
 *  2005-02-20  first version (cristian)
 *  2005-02-27  ported to 0.9.0 (bogdan)
 */


#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>


#include "../../dprint.h"
#include "../../route.h"
//#include "../../db/db.h"
#include "../../mem/shm_mem.h"

#include "dr_load.h"
#include "routing.h"
#include "prefix_tree.h"
#include "dr_time.h"
#include "parse.h"


#define DST_ID_DRD_COL   "gwid"
#define ADDRESS_DRD_COL  "address"
#define STRIP_DRD_COL    "strip"
#define PREFIX_DRD_COL   "pri_prefix"
#define TYPE_DRD_COL     "type"
#define ATTRS_DRD_COL    "attrs"
static str dst_id_drd_col = str_init(DST_ID_DRD_COL);
static str address_drd_col = str_init(ADDRESS_DRD_COL);
static str strip_drd_col = str_init(STRIP_DRD_COL);
static str prefix_drd_col = str_init(PREFIX_DRD_COL);
static str type_drd_col = str_init(TYPE_DRD_COL);
static str attrs_drd_col = str_init(ATTRS_DRD_COL);

#define RULE_ID_DRR_COL   "ruleid"
#define GROUP_DRR_COL     "groupid"
#define PREFIX_DRR_COL    "prefix"
#define TIME_DRR_COL      "timerec"
#define PRIORITY_DRR_COL  "priority"
#define ROUTEID_DRR_COL   "routeid"
#define DSTLIST_DRR_COL   "gwlist"
static str rule_id_drr_col = str_init(RULE_ID_DRR_COL);
static str group_drr_col = str_init(GROUP_DRR_COL);
static str prefix_drr_col = str_init(PREFIX_DRR_COL);
static str time_drr_col = str_init(TIME_DRR_COL);
static str priority_drr_col = str_init(PRIORITY_DRR_COL);
static str routeid_drr_col = str_init(ROUTEID_DRR_COL);
static str dstlist_drr_col = str_init(DSTLIST_DRR_COL);

#define ID_DRL_COL     "id"
#define GWLIST_DRL_CAL "gwlist"
static str id_drl_col = str_init(ID_DRL_COL);
static str gwlist_drl_col = str_init(GWLIST_DRL_CAL);

struct dr_gwl_tmp {
	unsigned int id;
	char *gwlist;
	struct dr_gwl_tmp *next;
};


static struct dr_gwl_tmp* dr_gw_lists = NULL;

#define check_val( _val, _type, _not_null, _is_empty_str) \
	do{\
		if ((_val)->type!=_type) { \
			LM_ERR("bad colum type\n");\
			goto error;\
		} \
		if (_not_null && (_val)->nul) { \
			LM_ERR("nul column\n");\
			goto error;\
		} \
		if (_is_empty_str && VAL_STRING(_val)==0) { \
			LM_ERR("empty str column\n");\
			goto error;\
		} \
	}while(0)


#define TR_SEPARATOR '|'

#define load_TR_value( _p,_s, _tr, _func, _err, _done) \
	do{ \
		_s = strchr(_p, (int)TR_SEPARATOR); \
		if (_s) \
			*_s = 0; \
		/*DBG("----parsing tr param <%s>\n",_p);*/\
		if(_s != _p) {\
			if( _func( _tr, _p)) {\
				if (_s) *_s = TR_SEPARATOR; \
				goto _err; \
			} \
		} \
		if (_s) { \
			*_s = TR_SEPARATOR; \
			_p = _s+1;\
			if ( *(_p)==0 ) \
				goto _done; \
		} else {\
			goto _done; \
		}\
	} while(0)

extern int dr_fetch_rows;


static int add_tmp_gw_list(unsigned int id, char *list)
{
	struct dr_gwl_tmp *tmp;
	unsigned int list_len;

	list_len = strlen(list) + 1;
	tmp = (struct dr_gwl_tmp*)pkg_malloc(sizeof(struct dr_gwl_tmp) + list_len);
	if (tmp==NULL) {
		LM_ERR("no more pkg mem\n");
		return -1;
	}
	tmp->id = id;
	tmp->gwlist = (char*)(tmp+1);
	memcpy(tmp->gwlist, list, list_len);

	tmp->next = dr_gw_lists;
	dr_gw_lists = tmp;
	return 0;
}

static char* get_tmp_gw_list(unsigned int id)
{
	struct dr_gwl_tmp *tmp;

	for( tmp=dr_gw_lists ; tmp ; tmp=tmp->next )
		if (tmp->id == id) return tmp->gwlist;
	return NULL;
}

static void free_tmp_gw_list(void)
{
	struct dr_gwl_tmp *tmp, *tmp1;

	for( tmp=dr_gw_lists ; tmp ; ) {
		tmp1 = tmp;
		tmp = tmp->next;
		pkg_free(tmp1);
	}
	dr_gw_lists = NULL;
}


static inline tmrec_t* parse_time_def(char *time_str)
{
	tmrec_t *time_rec;
	char *p,*s;

	p = time_str;
	time_rec = 0;

	time_rec = (tmrec_t*)shm_malloc(sizeof(tmrec_t));
	if (time_rec==0) {
		LM_ERR("no more pkg mem\n");
		goto error;
	}
	memset( time_rec, 0, sizeof(tmrec_t));

	/* empty definition? */
	if ( time_str==0 || *time_str==0 )
		goto done;

	load_TR_value( p, s, time_rec, tr_parse_dtstart, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_duration, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_freq, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_until, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_interval, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_byday, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_bymday, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_byyday, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_byweekno, parse_error, done);
	load_TR_value( p, s, time_rec, tr_parse_bymonth, parse_error, done);

	/* success */
done:
	return time_rec;
parse_error:
	LM_ERR("parse error in <%s> around position %i\n",
		time_str, (int)(long)(p-time_str));
error:
	if (time_rec)
		tmrec_free( time_rec );
	return 0;
}


static int add_rule(rt_data_t *rdata, char *grplst, str *prefix, rt_info_t *rule)
{
	long int t;
	char *tmp;
	char *ep;
	int n;

	tmp=grplst;
	n=0;
	/* parse the grplst */
	while(tmp && (*tmp!=0)) {
		errno = 0;
		t = strtol(tmp, &ep, 10);
		if (ep == tmp) {
			LM_ERR("bad grp id '%c' (%d)[%s]\n",
				*ep, (int)(ep-grplst), grplst);
			goto error;
		}
		if ((!IS_SPACE(*ep)) && (*ep != SEP) && (*ep != SEP1) && (*ep!=0)) {
			LM_ERR("bad char %c (%d) [%s]\n",
					*ep, (int)(ep-grplst), grplst);
			goto error;
		}
		if (errno == ERANGE && (t== LONG_MAX || t== LONG_MIN)) {
			LM_ERR("out of bounds\n");
			goto error;
		}
		n++;
		/* add rule -> has prefix? */
		if (prefix->len) {
			/* add the routing rule */
			if ( add_prefix(rdata->pt, prefix, rule, (unsigned int)t)!=0 ) {
				LM_ERR("failed to add prefix route\n");
					goto error;
			}
		} else {
			if ( add_rt_info( &rdata->noprefix, rule, (unsigned int)t)!=0 ) {
				LM_ERR("failed to add prefixless route\n");
					goto error;
			}
		}
		/* keep parsing */
		if(IS_SPACE(*ep))
			EAT_SPACE(ep);
		if(ep && (*ep == SEP || *ep == SEP1))
			ep++;
		tmp = ep;
	}

	if(n==0) {
		LM_ERR("no id in grp list [%s]\n",
			grplst);
		goto error;
	}

	return 0;
error:
	return -1;
}


rt_data_t* dr_load_routing_info( db_func_t *dr_dbf, db1_con_t* db_hdl,
							str *drd_table, str *drl_table, str* drr_table )
{
	int    int_vals[4];
	char * str_vals[5];
	str tmp;
	db_key_t columns[7];
	db1_res_t* res;
	db_row_t* row;
	rt_info_t *ri;
	rt_data_t *rdata;
	tmrec_t   *time_rec;
	unsigned int id;
	str s_id;
	int i,n;

	res = 0;
	ri = 0;
	rdata = 0;

	/* init new data structure */
	if ( (rdata=build_rt_data())==0 ) {
		LM_ERR("failed to build rdata\n");
		goto error;
	}

	/* read the destinations */
	if (dr_dbf->use_table( db_hdl, drd_table) < 0) {
		LM_ERR("cannot select table \"%.*s\"\n", drd_table->len,drd_table->s);
		goto error;
	}

	columns[0] = &dst_id_drd_col;
	columns[1] = &address_drd_col;
	columns[2] = &strip_drd_col;
	columns[3] = &prefix_drd_col;
	columns[4] = &type_drd_col;
	columns[5] = &attrs_drd_col;

	if (DB_CAPABILITY(*dr_dbf, DB_CAP_FETCH)) {
		if ( dr_dbf->query( db_hdl, 0, 0, 0, columns, 0, 6, 0, 0 ) < 0) {
			LM_ERR("DB query failed\n");
			goto error;
		}
		if(dr_dbf->fetch_result(db_hdl, &res, dr_fetch_rows)<0) {
			LM_ERR("Error fetching rows\n");
			goto error;
		}
	} else {
		if ( dr_dbf->query( db_hdl, 0, 0, 0, columns, 0, 6, 0, &res) < 0) {
			LM_ERR("DB query failed\n");
			goto error;
		}
	}

	if (RES_ROW_N(res) == 0) {
		LM_WARN("table \"%.*s\" empty\n", drd_table->len,drd_table->s );
	}
	LM_DBG("%d records found in %.*s\n",
		RES_ROW_N(res), drd_table->len,drd_table->s);
	n = 0;
	do {
		for(i=0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;
			/* DST_ID column */
			check_val( ROW_VALUES(row), DB1_INT, 1, 0);
			int_vals[0] = VAL_INT   (ROW_VALUES(row));
			/* ADDRESS column */
			check_val( ROW_VALUES(row)+1, DB1_STRING, 1, 1);
			str_vals[0] = (char*)VAL_STRING(ROW_VALUES(row)+1);
			/* STRIP column */
			check_val( ROW_VALUES(row)+2, DB1_INT, 1, 0);
			int_vals[1] = VAL_INT   (ROW_VALUES(row)+2);
			/* PREFIX column */
			check_val( ROW_VALUES(row)+3, DB1_STRING, 0, 0);
			str_vals[1] = (char*)VAL_STRING(ROW_VALUES(row)+3);
			/* TYPE column */
			check_val( ROW_VALUES(row)+4, DB1_INT, 1, 0);
			int_vals[2] = VAL_INT(ROW_VALUES(row)+4);
			/* ATTRS column */
			check_val( ROW_VALUES(row)+5, DB1_STRING, 0, 0);
			str_vals[2] = (char*)VAL_STRING(ROW_VALUES(row)+5);

			/* add the destinaton definition in */
			if ( add_dst( rdata, int_vals[0], str_vals[0], int_vals[1],
					str_vals[1], int_vals[2], str_vals[2])<0 ) {
				LM_ERR("failed to add destination id %d -> skipping\n",
					int_vals[0]);
				continue;
			}
			n++;
		}
		if (DB_CAPABILITY(*dr_dbf, DB_CAP_FETCH)) {
			if(dr_dbf->fetch_result(db_hdl, &res, dr_fetch_rows)<0) {
				LM_ERR( "fetching rows (1)\n");
				goto error;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(res)>0);

	dr_dbf->free_result(db_hdl, res);
	res = 0;

	if (n==0) {
		LM_WARN("no valid "
			"destinations set -> ignoring the routing rules\n");
		return rdata;
	}

	/* read the gw lists, if any */
	if (dr_dbf->use_table( db_hdl, drl_table) < 0) {
		LM_ERR("cannot select table \"%.*s\"\n", drl_table->len,drl_table->s);
		goto error;
	}

	columns[0] = &id_drl_col;
	columns[1] = &gwlist_drl_col;

	if (DB_CAPABILITY(*dr_dbf, DB_CAP_FETCH)) {
		if ( dr_dbf->query( db_hdl, 0, 0, 0, columns, 0, 2, 0, 0 ) < 0) {
			LM_ERR("DB query failed\n");
			goto error;
		}
		if(dr_dbf->fetch_result(db_hdl, &res, dr_fetch_rows)<0) {
			LM_ERR("Error fetching rows\n");
			goto error;
		}
	} else {
		if ( dr_dbf->query( db_hdl, 0, 0, 0, columns, 0, 2, 0, &res) < 0) {
			LM_ERR("DB query failed\n");
			goto error;
		}
	}

	if (RES_ROW_N(res) == 0) {
		LM_DBG("table \"%.*s\" empty\n", drl_table->len,drl_table->s );
	} else {
		LM_DBG("%d records found in %.*s\n",
			RES_ROW_N(res), drl_table->len,drl_table->s);
		do {
			for(i=0; i < RES_ROW_N(res); i++) {
				row = RES_ROWS(res) + i;
				/* ID column */
				check_val( ROW_VALUES(row), DB1_INT, 1, 0);
				int_vals[0] = VAL_INT   (ROW_VALUES(row));
				/* GWLIST column */
				check_val( ROW_VALUES(row)+1, DB1_STRING, 1, 1);
				str_vals[0] = (char*)VAL_STRING(ROW_VALUES(row)+1);

				if (add_tmp_gw_list(int_vals[0], str_vals[0])!=0) {
					LM_ERR("failed to add temporary GW list\n");
					goto error;
				}
			}
			if (DB_CAPABILITY(*dr_dbf, DB_CAP_FETCH)) {
				if(dr_dbf->fetch_result(db_hdl, &res, dr_fetch_rows)<0) {
					LM_ERR( "fetching rows (1)\n");
					goto error;
				}
			} else {
				break;
			}
		} while(RES_ROW_N(res)>0);
	}
	dr_dbf->free_result(db_hdl, res);
	res = 0;

	/* read the routing rules */
	if (dr_dbf->use_table( db_hdl, drr_table) < 0) {
		LM_ERR("cannot select table \"%.*s\"\n", drr_table->len, drr_table->s);
		goto error;
	}

	columns[0] = &rule_id_drr_col;
	columns[1] = &group_drr_col;
	columns[2] = &prefix_drr_col;
	columns[3] = &time_drr_col;
	columns[4] = &priority_drr_col;
	columns[5] = &routeid_drr_col;
	columns[6] = &dstlist_drr_col;

	if (DB_CAPABILITY(*dr_dbf, DB_CAP_FETCH)) {
		if ( dr_dbf->query( db_hdl, 0, 0, 0, columns, 0, 7, 0, 0) < 0) {
			LM_ERR("DB query failed\n");
			goto error;
		}
		if(dr_dbf->fetch_result(db_hdl, &res, dr_fetch_rows)<0) {
			LM_ERR("Error fetching rows\n");
			goto error;
		}
	} else {
		if ( dr_dbf->query( db_hdl, 0, 0, 0, columns, 0, 7, 0, &res) < 0) {
			LM_ERR("DB query failed\n");
			goto error;
		}
	}

	if (RES_ROW_N(res) == 0) {
		LM_WARN("table \"%.*s\" is empty\n", drr_table->len, drr_table->s);
	}

	LM_DBG("%d records found in %.*s\n", RES_ROW_N(res),
		drr_table->len, drr_table->s);

	n = 0;
	do {
		for(i=0; i < RES_ROW_N(res); i++) {
			row = RES_ROWS(res) + i;
			/* RULE_ID column */
			check_val( ROW_VALUES(row), DB1_INT, 1, 0);
			int_vals[0] = VAL_INT (ROW_VALUES(row));
			/* GROUP column */
			check_val( ROW_VALUES(row)+1, DB1_STRING, 1, 1);
			str_vals[0] = (char*)VAL_STRING(ROW_VALUES(row)+1);
			/* PREFIX column - it may be null or empty */
			check_val( ROW_VALUES(row)+2, DB1_STRING, 0, 0);
			if ((ROW_VALUES(row)+2)->nul || VAL_STRING(ROW_VALUES(row)+2)==0){
				tmp.s = NULL;
				tmp.len = 0;
			} else {
				str_vals[1] = (char*)VAL_STRING(ROW_VALUES(row)+2);
				tmp.s = str_vals[1];
				tmp.len = strlen(str_vals[1]);
			}
			/* TIME column */
			check_val( ROW_VALUES(row)+3, DB1_STRING, 1, 1);
			str_vals[2] = (char*)VAL_STRING(ROW_VALUES(row)+3);
			/* PRIORITY column */
			check_val( ROW_VALUES(row)+4, DB1_INT, 1, 0);
			int_vals[2] = VAL_INT   (ROW_VALUES(row)+4);
			/* ROUTE_ID column */
			check_val( ROW_VALUES(row)+5, DB1_STRING, 1, 0);
			str_vals[3] = (char*)VAL_STRING(ROW_VALUES(row)+5);
			/* DSTLIST column */
			check_val( ROW_VALUES(row)+6, DB1_STRING, 1, 1);
			str_vals[4] = (char*)VAL_STRING(ROW_VALUES(row)+6);
			/* parse the time definition */
			if ((time_rec=parse_time_def(str_vals[2]))==0) {
				LM_ERR("bad time definition <%s> for rule id %d -> skipping\n",
					str_vals[2], int_vals[0]);
				continue;
			}
			/* lookup for the script route ID */
			if (str_vals[3][0] && str_vals[3][0]!='0') {
				int_vals[3] =  route_lookup(&main_rt, str_vals[3]);
				if (int_vals[3]==-1) {
					LM_WARN("route <%s> does not exist\n",str_vals[3]);
					int_vals[3] = 0;
				}
			} else {
				int_vals[3] = 0;
			}
			/* is gw_list a list or a list id? */
			if (str_vals[4][0]=='#') {
				s_id.s = str_vals[4]+1;
				s_id.len = strlen(s_id.s);
				if ( str2int( &s_id, &id)!=0 ||
				(str_vals[4]=get_tmp_gw_list(id))==NULL ) {
					LM_ERR("invalid reference to a GW list <%s> -> skipping\n",
						str_vals[4]);
					continue;
				}
			}
			/* build the routing rule */
			if ((ri = build_rt_info( int_vals[2], time_rec, int_vals[3],
					str_vals[4], rdata->pgw_l))== 0 ) {
				LM_ERR("failed to add routing info for rule id %d -> "
					"skipping\n", int_vals[0]);
				tmrec_free( time_rec );
				continue;
			}
			/* add the rule */
			if (add_rule( rdata, str_vals[0], &tmp, ri)!=0) {
				LM_ERR("failed to add rule id %d -> skipping\n", int_vals[0]);
				free_rt_info( ri );
				continue;
			}
			n++;
		}
		if (DB_CAPABILITY(*dr_dbf, DB_CAP_FETCH)) {
			if(dr_dbf->fetch_result(db_hdl, &res, dr_fetch_rows)<0) {
				LM_ERR( "fetching rows (1)\n");
				goto error;
			}
		} else {
			break;
		}
	} while(RES_ROW_N(res)>0);

	dr_dbf->free_result(db_hdl, res);
	res = 0;

	free_tmp_gw_list();

	if (n==0) {
		LM_WARN("no valid routing rules -> discarding all destinations\n");
		free_rt_data( rdata, 0 );
	}

	return rdata;
error:
	if (res)
		dr_dbf->free_result(db_hdl, res);
	if (rdata)
		free_rt_data( rdata, 1 );
	rdata = NULL;
	return 0;
}
