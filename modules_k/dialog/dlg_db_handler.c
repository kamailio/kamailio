/*
 * $Id$
 *
 * Copyright (C) 2007 Voice System SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2007-05-10  initial version (ancuta)
 * 2007-07-06 additional information saved in the database: cseq, contact, 
 *  		   route set and socket_info for both caller and callee (ancuta)
 */

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../db/db.h"
#include "../../socket_info.h"
#include "dlg_hash.h"
#include "dlg_db_handler.h"


char* call_id_column		=	CALL_ID_COL;
char* from_uri_column		=	FROM_URI_COL;
char* from_tag_column		=	FROM_TAG_COL;
char* to_uri_column			=	TO_URI_COL;
char* to_tag_column			=	TO_TAG_COL;
char* h_id_column			=	HASH_ID_COL;
char* h_entry_column		=	HASH_ENTRY_COL;
char* state_column			=	STATE_COL;
char* start_time_column		=	START_TIME_COL;
char* timeout_column		=	TIMEOUT_COL;
char* to_cseq_column		=	TO_CSEQ_COL;
char* from_cseq_column		=	FROM_CSEQ_COL;
char* to_route_column		=	TO_ROUTE_COL;
char* from_route_column		=	FROM_ROUTE_COL;
char* to_contact_column		=	TO_CONTACT_COL;
char* from_contact_column	=	FROM_CONTACT_COL;
char* to_sock_column		=	TO_SOCK_COL;
char* from_sock_column		=	FROM_SOCK_COL;
char* dialog_table_name		=	DIALOG_TABLE_NAME;
int dlg_db_mode				=	DB_MODE_NONE;

static db_con_t* dialog_db_handle    = 0; /* database connection handle */
static db_func_t dialog_dbf;
static time_t clock_time;



#define SET_STR_VALUE(_val, _str)\
	do{\
			VAL_STR((_val)).s 		= (_str).s;\
			VAL_STR((_val)).len 	= (_str).len;\
	}while(0);

#define SET_NULL_FLAG(_vals, _i, _max, _flag)\
	do{\
		for((_i) = 0;(_i)<(_max); (_i)++)\
			VAL_NULL((_vals)+(_i)) = (_flag);\
	}while(0);

#define SET_PROPER_NULL_FLAG(_str, _vals, _index)\
	do{\
		if( (_str).len == 0)\
			VAL_NULL( (_vals)+(_index) ) = 1;\
		else\
			VAL_NULL( (_vals)+(_index) ) = 0;\
	}while(0);

#define GET_STR_VALUE(_res, _values, _index)\
	do{\
		(_res).s = VAL_STR((_values)+ (_index)).s;\
		(_res).len = strlen(VAL_STR((_values)+ (_index)).s);\
	}while(0);


static int load_dialog_info_from_db(int dlg_hash_size);


int dlg_connect_db(char *db_url)
{
	if (dialog_db_handle) {
		LM_CRIT("BUG - db connection found already open\n");
		return -1;
	}
	if ((dialog_db_handle = dialog_dbf.init(db_url)) == 0)
		return -1;
	return 0;
}


int init_dlg_db( char *db_url, int dlg_hash_size , int db_update_period)
{
	int ver;
	str table;

	time(&clock_time);

	/* Find a database module */
	if (bind_dbmod(db_url, &dialog_dbf) < 0){
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}

	if (dlg_connect_db(db_url)!=0){
		LM_ERR("unable to connect to the database\n");
		return -1;
	}

	table.s = dialog_table_name;
	table.len = strlen(table.s);
	ver = table_version(&dialog_dbf, dialog_db_handle, &table );
	if (ver < 0) {
		LM_ERR("failed to query table version\n");
		return -1;
	} else if (ver != DLG_TABLE_VERSION) {
		LM_ERR("Invalid table version (found %d , required %d)\n"
			"(use openser_mysql.sh reinstall)\n",
			ver, DLG_TABLE_VERSION );
		return -1;
	}

	if( (dlg_db_mode==DB_MODE_DELAYED) && 
	(register_timer( dialog_update_db, 0, db_update_period)<0 )) {
		LM_ERR("failed to register update db\n");
		return -1;
	}

	if( (load_dialog_info_from_db(dlg_hash_size) ) !=0 ){
		LM_ERR("unable to load the dialog data\n");
		return -1;
	}

	dialog_dbf.close(dialog_db_handle);
	dialog_db_handle = 0;

	return 0;
}



void destroy_dlg_db(void)
{
	/* close the DB connection */
	if (dialog_db_handle) {
		dialog_dbf.close(dialog_db_handle);
		dialog_db_handle = 0;
	}
}



static int use_dialog_table(void)
{
	if(!dialog_db_handle){
		LM_ERR("invalid database handle\n");
		return -1;
	}

	if (dialog_dbf.use_table(dialog_db_handle, dialog_table_name) < 0) {
		LM_ERR("Error in use_table\n");
		return -1;
	}

	return 0;
}



static int select_entire_dialog_table(db_res_t ** res)
{
	db_key_t query_cols[DIALOG_TABLE_COL_NO] = {	h_entry_column,
			h_id_column,		call_id_column,		from_uri_column,
			from_tag_column,	to_uri_column,		to_tag_column,
			start_time_column,	state_column,		timeout_column,
			from_cseq_column,	to_cseq_column,		from_route_column,
			to_route_column, 	from_contact_column, to_contact_column,
			from_sock_column,	to_sock_column};

	if(use_dialog_table() != 0){
		return -1;
	}

	/*select the whole tabel and all the columns*/
	if(dialog_dbf.query(dialog_db_handle,0,0,0,query_cols, 0, 
	DIALOG_TABLE_COL_NO, 0, res) < 0) {
		LM_ERR("Error while querying database\n");
		return -1;
	}

	return 0;
}



struct socket_info * create_socket_info(db_val_t * vals, int n){

	struct socket_info * sock;
	str host, p;
	int port, proto;

	/* socket name */
	p.s  = (VAL_STR(vals+n)).s;
	p.len = strlen(p.s);

	if (VAL_NULL(vals+n) || p.s==0 || p.s[0]==0){
		sock = 0;
	} else {
		if (parse_phostport( p.s, p.len, &host.s, &host.len, 
		&port, &proto)!=0) {
			LM_ERR("bad socket <%.*s>\n", p.len, p.s);
			return 0;
		}
		sock = grep_sock_info( &host, (unsigned short)port, proto);
		if (sock==0) {
			LM_WARN("non-local socket <%.*s>...ignoring\n", p.len, p.s);
			}
	}

	return sock;
}



static int load_dialog_info_from_db(int dlg_hash_size)
{
	db_res_t * res;
	db_val_t * values;
	db_row_t * rows;
	int i, nr_rows;
	struct dlg_cell *dlg;
	str callid, from_uri, to_uri, from_tag, to_tag;
	str cseq1, cseq2, contact1, contact2, rroute1, rroute2;
	unsigned int next_id;
	

	res = 0;
	if((nr_rows = select_entire_dialog_table(&res)) < 0)
		goto end;

	nr_rows = RES_ROW_N(res);

	LM_DBG("the database has information about %i dialogs\n", nr_rows);

	rows = RES_ROWS(res);
	
	/*for every row---dialog*/
	for(i=0; i<nr_rows; i++){

		values = ROW_VALUES(rows + i);

		/*restore the dialog info*/
		GET_STR_VALUE(callid, values, 2);
		GET_STR_VALUE(from_uri, values, 3);
		GET_STR_VALUE(from_tag, values, 4);
		GET_STR_VALUE(to_uri, values, 5);

		if((dlg=build_new_dlg( &callid, &from_uri, &to_uri, &from_tag))==0){
			LM_ERR("failed to build new dialog\n");
			goto error;
		}

		if(dlg->h_entry != VAL_INT(values)){
			LM_ERR("inconsistent hash data in the dialog database: "
				"you may have restarted openser using a different hash_size:"
				"please erase %s database and restart\n", dialog_table_name);
			shm_free(dlg);
			goto error;
		}

		/*link the dialog*/
		link_dlg(dlg, 0);

		dlg->h_id = VAL_INT(values+1);
		next_id = d_table->entries[dlg->h_entry].next_id;

		d_table->entries[dlg->h_entry].next_id =
			(next_id < dlg->h_id) ? dlg->h_id : next_id;

		GET_STR_VALUE(to_tag, values, 6);

		dlg->start_ts	= VAL_INT(values+7);
		dlg->state 		= VAL_INT(values+8);
		dlg->tl.timeout = VAL_INT(values+9) - clock_time;

		/*restore the timer values */
		insert_dlg_timer(&(dlg->tl), (int)dlg->tl.timeout);
		LM_DBG("current dialog timeout is %u\n", dlg->tl.timeout);

		GET_STR_VALUE(cseq1, values, 10);
		GET_STR_VALUE(cseq2, values, 11);
		GET_STR_VALUE(rroute1, values, 12);
		GET_STR_VALUE(rroute2, values, 13);
		GET_STR_VALUE(contact1, values, 14);
		GET_STR_VALUE(contact2, values, 15);

		if ( (dlg_set_leg_info( dlg, &from_tag, &rroute1, &contact1,
		&cseq1, DLG_CALLER_LEG)!=0) ||
		(dlg_set_leg_info( dlg, &to_tag, &rroute2, &contact2,
		&cseq2, DLG_CALLEE_LEG)!=0) ) {
			LM_ERR("dlg_set_leg_info failed\n");
			unref_dlg(dlg,1);
			continue;
		}

		dlg->bind_addr[DLG_CALLER_LEG] = create_socket_info(values, 16);
		dlg->bind_addr[DLG_CALLEE_LEG] = create_socket_info(values, 17);
	
		dlg->lifetime = 0;
		dlg->flags = 0;
	}

end:
	dialog_dbf.free_result(dialog_db_handle, res);
	return 0;
error:
	dialog_dbf.free_result(dialog_db_handle, res);
	return -1;

}



/*this is only called from destroy_dlg, where the cell's entry lock is acquired*/
int remove_dialog_from_db(struct dlg_cell * cell)
{
	db_val_t values[2];
	db_key_t match_keys[2] = { h_entry_column, h_id_column};

	/*if the dialog hasn 't been yet inserted in the database*/
	LM_DBG("trying to remove a dialog, update_flag is %i\n", cell->flags);
	if (cell->flags & DLG_FLAG_NEW) 
		return 0;

	if (use_dialog_table()!=0)
		return -1;

	VAL_TYPE(values) = VAL_TYPE(values+1) = DB_INT;
	VAL_NULL(values) = VAL_NULL(values+1) = 0;

	VAL_INT(values) 	= cell->h_entry;
	VAL_INT(values+1) 	= cell->h_id;

	if(dialog_dbf.delete(dialog_db_handle, match_keys, 0, values, 2) < 0) {
		LM_ERR("failed to delete database information\n");
		return -1;
	}

	LM_DBG("callid was %.*s\n", cell->callid.len, cell->callid.s );

	return 0;
}



int update_dialog_dbinfo(struct dlg_cell * cell)
{
	int i;
	struct dlg_entry entry;
	db_val_t values[DIALOG_TABLE_COL_NO];

	db_key_t insert_keys[DIALOG_TABLE_COL_NO] = { h_entry_column,
			h_id_column,        call_id_column,     from_uri_column,
			from_tag_column,    to_uri_column,      to_tag_column,
			from_sock_column,   to_sock_column,
			start_time_column,  state_column,       timeout_column,
			from_cseq_column,   to_cseq_column,     from_route_column,
			to_route_column,    from_contact_column,to_contact_column};

	if(use_dialog_table()!=0)
		return -1;
	
	if((cell->flags & DLG_FLAG_NEW) != 0){
		/* save all the current dialogs information*/
		VAL_TYPE(values) = VAL_TYPE(values+1) = VAL_TYPE(values+9) = 
		VAL_TYPE(values+10) = VAL_TYPE(values+11) = DB_INT;

		VAL_TYPE(values+2) = VAL_TYPE(values+3) = VAL_TYPE(values+4) = 
		VAL_TYPE(values+5) = VAL_TYPE(values+6) = VAL_TYPE(values+7) = 
		VAL_TYPE(values+8) = VAL_TYPE(values+12) = VAL_TYPE(values+13) = 
		VAL_TYPE(values+14) = VAL_TYPE(values+15) = VAL_TYPE(values+16)=
		VAL_TYPE(values+17) = DB_STR;

		SET_NULL_FLAG(values, i, DIALOG_TABLE_COL_NO-4, 0);

		/* lock the entry */
		entry = (d_table->entries)[cell->h_entry];
		dlg_lock( d_table, &entry);

		VAL_INT(values)			= cell->h_entry;
		VAL_INT(values+1)		= cell->h_id;
		VAL_INT(values+9)		= cell->start_ts;
		VAL_INT(values+10)		= cell->state;
		VAL_INT(values+11)		= cell->tl.timeout - get_ticks() + 
							(unsigned int)clock_time;

		SET_STR_VALUE(values+2, cell->callid);
		SET_STR_VALUE(values+3, cell->from_uri);
		SET_STR_VALUE(values+4, cell->tag[DLG_CALLER_LEG]);
		SET_STR_VALUE(values+5, cell->to_uri);
		SET_STR_VALUE(values+6, cell->tag[DLG_CALLEE_LEG]);
		SET_PROPER_NULL_FLAG(cell->tag[DLG_CALLEE_LEG], values, 6);

		LM_DBG("sock_info is %.*s\n", 
			cell->bind_addr[DLG_CALLER_LEG]->sock_str.len,
			cell->bind_addr[DLG_CALLEE_LEG]->sock_str.s);

		SET_STR_VALUE(values+7, cell->bind_addr[DLG_CALLER_LEG]->sock_str);
		SET_STR_VALUE(values+8, cell->bind_addr[DLG_CALLEE_LEG]->sock_str);

		SET_STR_VALUE(values+12, cell->cseq[DLG_CALLER_LEG]);
		SET_STR_VALUE(values+13, cell->cseq[DLG_CALLEE_LEG]);
		SET_STR_VALUE(values+14, cell->route_set[DLG_CALLER_LEG]);
		SET_STR_VALUE(values+15, cell->route_set[DLG_CALLEE_LEG]);
		SET_STR_VALUE(values+16, cell->contact[DLG_CALLER_LEG]);
		SET_STR_VALUE(values+17, cell->contact[DLG_CALLEE_LEG]);

		SET_PROPER_NULL_FLAG(cell->route_set[DLG_CALLER_LEG], 	values, 14);
		SET_PROPER_NULL_FLAG(cell->route_set[DLG_CALLEE_LEG], 	values, 15);
		SET_PROPER_NULL_FLAG(cell->contact[DLG_CALLER_LEG], 	values, 16);
		SET_PROPER_NULL_FLAG(cell->contact[DLG_CALLEE_LEG], 	values, 17);


		if((dialog_dbf.insert(dialog_db_handle, insert_keys, values, 
								DIALOG_TABLE_COL_NO)) !=0){
			LM_ERR("could not add another dialog to db\n");
			goto error;
		}
		cell->flags &= ~(DLG_FLAG_NEW|DLG_FLAG_CHANGED);
		
	} else if((cell->flags & DLG_FLAG_CHANGED) != 0) {
		/* save only dialog's state and timeout */
		VAL_TYPE(values) = VAL_TYPE(values+1) = 
		VAL_TYPE(values+10) = VAL_TYPE(values+11) = DB_INT;

		VAL_TYPE(values+12) = VAL_TYPE(values+13) =DB_STR;

		/* lock the entry */
		entry = (d_table->entries)[cell->h_entry];
		dlg_lock( d_table, &entry);

		VAL_INT(values)			= cell->h_entry;
		VAL_INT(values+1)		= cell->h_id;
		VAL_INT(values+10)		= cell->state;
		VAL_INT(values+11)		= cell->tl.timeout - get_ticks() + 
							(unsigned int)clock_time;
		SET_STR_VALUE(values+12, cell->cseq[DLG_CALLER_LEG]);
		SET_STR_VALUE(values+13, cell->cseq[DLG_CALLEE_LEG]);


		VAL_NULL(values) = VAL_NULL(values+1) = 
		VAL_NULL(values+10) = VAL_NULL(values+11) = 
		VAL_NULL(values+12) = VAL_NULL(values+13) = 0;

		if((dialog_dbf.update(dialog_db_handle, (insert_keys), 0, 
						(values), (insert_keys+10), (values+10), 2, 4)) !=0){
			LM_ERR("could not update database info\n");
			goto error;
		}
		cell->flags &= ~(DLG_FLAG_CHANGED);
	} else {
		return 0;
	}

	dlg_unlock( d_table, &entry);
	return 0;

error:
	dlg_unlock( d_table, &entry);
	return -1;
}



void dialog_update_db(unsigned int ticks, void * param)
{
	int index, i;
	db_val_t values[DIALOG_TABLE_COL_NO];
	struct dlg_entry entry;
	struct dlg_cell  * cell; 
	
	db_key_t insert_keys[DIALOG_TABLE_COL_NO] = {		h_entry_column,
			h_id_column,		call_id_column,		from_uri_column,
			from_tag_column,	to_uri_column,		to_tag_column,
			from_sock_column,	to_sock_column,
			start_time_column,	state_column,		timeout_column,
			from_cseq_column,	to_cseq_column,		from_route_column,
			to_route_column, 	from_contact_column, to_contact_column};

	if(use_dialog_table()!=0)
		return;

	/*save the current dialogs information*/
	VAL_TYPE(values) = VAL_TYPE(values+1) = VAL_TYPE(values+9) = 
	VAL_TYPE(values+10) = VAL_TYPE(values+11) = DB_INT;

	VAL_TYPE(values+2) = VAL_TYPE(values+3) = VAL_TYPE(values+4) = 
	VAL_TYPE(values+5) = VAL_TYPE(values+6) = VAL_TYPE(values+7) = 
	VAL_TYPE(values+8) = VAL_TYPE(values+12) = VAL_TYPE(values+13) = 
	VAL_TYPE(values+14) = VAL_TYPE(values+15) = VAL_TYPE(values+16) = 
	VAL_TYPE(values+17) = DB_STR;

	SET_NULL_FLAG(values, i, DIALOG_TABLE_COL_NO-4, 0);

	LM_DBG("saving current_info \n");
	
	for(index = 0; index< d_table->size; index++){

		/* lock the whole entry */
		entry = (d_table->entries)[index];
		dlg_lock( d_table, &entry);

		for(cell = entry.first; cell != NULL; cell = cell->next){

			if( (cell->flags & DLG_FLAG_NEW) != 0 ) {

				VAL_INT(values)			= cell->h_entry;
				VAL_INT(values+1)		= cell->h_id;

				VAL_INT(values+9)		= cell->start_ts;
				VAL_INT(values+10)		= cell->state;
				VAL_INT(values+11)		= cell->tl.timeout - get_ticks() +
									(unsigned int)clock_time;

				SET_STR_VALUE(values+2, cell->callid);
				SET_STR_VALUE(values+3, cell->from_uri);
				SET_STR_VALUE(values+4, cell->tag[DLG_CALLER_LEG]);
				SET_STR_VALUE(values+5, cell->to_uri);
				SET_STR_VALUE(values+6, cell->tag[DLG_CALLEE_LEG]);
				SET_PROPER_NULL_FLAG(cell->tag[DLG_CALLEE_LEG], values, 6);

				SET_STR_VALUE(values+7,
					cell->bind_addr[DLG_CALLER_LEG]->sock_str);
				SET_STR_VALUE(values+8,
					cell->bind_addr[DLG_CALLEE_LEG]->sock_str);
				
				SET_STR_VALUE(values+12, cell->cseq[DLG_CALLER_LEG]);
				SET_STR_VALUE(values+13, cell->cseq[DLG_CALLEE_LEG]);

				SET_STR_VALUE(values+14, cell->route_set[DLG_CALLER_LEG]);
				SET_STR_VALUE(values+15, cell->route_set[DLG_CALLEE_LEG]);
				SET_STR_VALUE(values+16, cell->contact[DLG_CALLER_LEG]);
				SET_STR_VALUE(values+17, cell->contact[DLG_CALLEE_LEG]);
			
				SET_PROPER_NULL_FLAG(cell->route_set[DLG_CALLER_LEG],
					values, 14);
				SET_PROPER_NULL_FLAG(cell->route_set[DLG_CALLEE_LEG],
					values, 15);
				SET_PROPER_NULL_FLAG(cell->contact[DLG_CALLER_LEG],
					values, 16);
				SET_PROPER_NULL_FLAG(cell->contact[DLG_CALLEE_LEG],
					values, 17);

				if((dialog_dbf.insert(dialog_db_handle, insert_keys, 
				values, DIALOG_TABLE_COL_NO)) !=0){
					LM_ERR("could not add another dialog to db\n");
					goto error;
				}

				cell->flags &= ~(DLG_FLAG_NEW |DLG_FLAG_CHANGED);

			} else if( (cell->flags & DLG_FLAG_CHANGED)!=0 ){

				VAL_INT(values)			= cell->h_entry;
				VAL_INT(values+1)		= cell->h_id;

				VAL_INT(values+10)		= cell->state;
				VAL_INT(values+11)		= cell->tl.timeout - get_ticks() +
									(unsigned int)clock_time;
				SET_STR_VALUE(values+12, cell->cseq[0]);
				SET_STR_VALUE(values+13, cell->cseq[DLG_CALLEE_LEG]);


				if((dialog_dbf.update(dialog_db_handle, (insert_keys), 0, 
				(values), (insert_keys+10), (values+10), 2, 4)) !=0) {
					LM_ERR("could not update database info\n");
					goto error;
				}

				cell->flags &= ~DLG_FLAG_CHANGED;

			}

		}
		dlg_unlock( d_table, &entry);

	}

	return;

error:
	dlg_unlock( d_table, &entry);
}

