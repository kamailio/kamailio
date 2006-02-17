#include <stdio.h>
#include "dlg_mod_internal.h"
#include "dlg_storage.h"
#include "dlg_utils.h"
#include <cds/hash_table.h>

hash_table_t *dlg_table = NULL;

static db_con_t* dlg_db = NULL; /* database connection handle */
static db_func_t dlg_dbf;	/* database functions */
static int db_initialized = 0;

/*static void close_dlg_db_connection(db_con_t* db)
{
	if (db && dlg_dbf.close) dlg_dbf.close(db);
}*/


int init_dlg_storage(int db_mode, const str *db_url)
{
	/* test db connection and initialize it, ... */
	dlg_table = (hash_table_t*)shm_malloc(sizeof(hash_table_t));
	if (!dlg_table) {
		ERR("can't allocate memory\n");
		return -1;
	}
	
	db_initialized = 0;
	if (db_mode > 0) {
		if (!db_url) {
			ERR("no db_url specified but use_db=1\n");
			db_mode = 0;
		}
	}
	if (db_mode > 0) {
		if (bind_dbmod(db_url->s, &dlg_dbf) < 0) {
			ERR("Can't bind database module via url %.*s\n", FMT_STR(*db_url));
			return -1;
		}

		if (!DB_CAPABILITY(dlg_dbf, DB_CAP_ALL)) { /* ? */
			ERR("Database module does not implement all functions needed by the module\n");
			return -1;
		}
		db_initialized = 1;
	}

	/* initialize hash table */
	ht_init(dlg_table, 
			(hash_func_t)hash_dlg_id, 
			(key_cmp_func_t)cmp_dlg_ids, 
			16603);

	
	/* reload data from DB */
	return 0;
}
	
int init_dlg_storage_child(int db_mode, const str *db_url)
{
	if (db_mode > 0) {
		if (dlg_dbf.init)
			dlg_db = dlg_dbf.init(db_url->s);
		if (!dlg_db) {
			ERR("Error while connecting database\n");
			return -1;
		}
	}
	return 0;
}

void destroy_dlg_storage()
{
	/* free db connection */

	/* ??? if (db_mode) close_dlg_db_connection(dlg_db); */
	dlg_db = NULL;

	/* destroy hash table */
	if (dlg_table) {
		ht_destroy(dlg_table);
		shm_free(dlg_table);
	}
}
	

/* ----- Hash Table operations ----- */

void add_dialog_to_ht(dialog_info_t *info)
{
	ht_add(dlg_table, &info->dialog->id, info);
}

dialog_info_t *find_dialog_in_ht(dlg_id_t *id)
{
	dialog_info_t *info = NULL;
	dlg_id_t tmp_id;
	
	info = (dialog_info_t*)ht_find(dlg_table, id);
	if (info) return info;

	/* try to find dialog in other direction (swap tags) */
	tmp_id.call_id = id->call_id;
	tmp_id.rem_tag = id->loc_tag;
	tmp_id.loc_tag = id->rem_tag;
	info = (dialog_info_t*)ht_find(dlg_table, id);
	if (info) return info;

	/* try to find it as unconfirmed dialog */
	tmp_id.loc_tag = id->loc_tag;
	tmp_id.rem_tag.len = 0;
	info = (dialog_info_t*)ht_find(dlg_table, id);
	if (info) return info;

	/* try to find it as unconfirmed dialog in other direction? */
	tmp_id.loc_tag.len = 0;
	tmp_id.rem_tag = id->loc_tag;
	info = (dialog_info_t*)ht_find(dlg_table, id);
	if (info) return info;
	
	return info;
}

/* ----- Global functions ----- */

dialog_info_t *find_dialog(dlg_id_t *id)
{
	return find_dialog_in_ht(id);
}
	
int add_dialog(dialog_info_t *info)
{
	add_dialog_to_ht(info);
	return 0;
}
