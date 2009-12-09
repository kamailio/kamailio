#include "presentity.h"
#include "pa_mod.h"
#include "pres_notes.h"
#include <cds/logger.h>

/* DB manipulation */

static int db_add_pres_note(presentity_t *p, pa_presence_note_t *n)
{
	db_key_t cols[20];
	db_val_t vals[20];
	int n_updates = 0;

	if (!use_db) return 0;
	
	/* set data */
	
	cols[n_updates] = col_dbid;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->dbid;
	n_updates++;
	
	cols[n_updates] = col_pres_id;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = p->pres_id;
	n_updates++;
	
	cols[n_updates] = col_etag;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->etag;
	n_updates++;

	cols[n_updates] = col_note;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->data.value;
	n_updates++;
	
	cols[n_updates] = col_lang;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->data.lang;
	n_updates++;
	
	cols[n_updates] = col_expires;
	vals[n_updates].type = DB_DATETIME;
	vals[n_updates].nul = 0;
	vals[n_updates].val.time_val = n->expires;
	n_updates++;	
	
	/* run update */
	
	if (pa_dbf.use_table(pa_db, presentity_notes_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.insert(pa_db, cols, vals, n_updates) < 0) {
		ERR("Can't insert record\n");
		return -1;
	}
	
	return 0;
}

int db_update_pres_note(presentity_t *p, pa_presence_note_t *n)
{
	db_key_t cols[20];
	db_val_t vals[20];
	int n_updates = 0;
	
	db_key_t keys[] = { col_pres_id, col_etag, col_dbid };
	db_op_t ops[] = { OP_EQ, OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = p->pres_id } },
		{ DB_STR, 0, { .str_val = n->etag } },
		{ DB_STR, 0, { .str_val = n->dbid } }
	};
	
	if (!use_db) return 0;
	
	cols[n_updates] = col_note;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->data.value;
	n_updates++;
	
	cols[n_updates] = col_lang;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->data.lang;
	n_updates++;
	
	cols[n_updates] = col_expires;
	vals[n_updates].type = DB_DATETIME;
	vals[n_updates].nul = 0;
	vals[n_updates].val.time_val = n->expires;
	n_updates++;	

	if (pa_dbf.use_table(pa_db, presentity_notes_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.update(pa_db, keys, ops, k_vals, 
				cols, vals, 3, n_updates) < 0) {
		ERR("Can't update record\n");
		return -1;
	}

	return 0;
}

static int db_remove_pres_note(presentity_t *p, pa_presence_note_t *n)
{
	db_key_t keys[] = { col_pres_id, col_etag, col_dbid };
	db_op_t ops[] = { OP_EQ, OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = p->pres_id } },
		{ DB_STR, 0, { .str_val = n->etag } },
		{ DB_STR, 0, { .str_val = n->dbid } }
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(pa_db, presentity_notes_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 3) < 0) {
		ERR("Can't delete record\n");
		return -1;
	}
	
	return 0;
}

int db_read_notes(presentity_t *p, db_con_t* db)
{
	db_key_t keys[] = { col_pres_id };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = p->pres_id } } };

	int i;
	int r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { col_dbid, col_etag, 
		col_note, col_lang, col_expires
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(db, presentity_notes_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.query (db, keys, ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		ERR("Error while querying presence notes\n");
		return -1;
	}

	if (!res) return 0; /* ? */
	
	for (i = 0; i < res->n; i++) {
		pa_presence_note_t *n = NULL;
		db_row_t *row = &res->rows[i];
		db_val_t *row_vals = ROW_VALUES(row);
		str dbid = STR_NULL; 
		str etag = STR_NULL;
		str note = STR_NULL;
		str lang = STR_NULL;
		time_t expires = 0;
		
#define get_str_val(i,dst)	do{if(!row_vals[i].nul){dst.s=(char*)row_vals[i].val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_time_val(i,dst)	do{if(!row_vals[i].nul){dst=row_vals[i].val.time_val;}}while(0)

		get_str_val(0, dbid);
		get_str_val(1, etag);
		get_str_val(2, note);
		get_str_val(3, lang);
		get_time_val(4, expires);
		
#undef get_str_val		
#undef get_time_val		

		n = create_pres_note(&etag, &note, &lang, expires, &dbid);
		if (n) DOUBLE_LINKED_LIST_ADD(p->data.first_note, 
					p->data.last_note, (presence_note_t*)n);
	}
	
	pa_dbf.free_result(db, res);
	
	return r;
}

/* in memory presence notes manipulation */

void add_pres_note(presentity_t *_p, pa_presence_note_t *n)
{
	DOUBLE_LINKED_LIST_ADD(_p->data.first_note, 
					_p->data.last_note, (presence_note_t*)n);
	if (use_db) db_add_pres_note(_p, n); 
}

void remove_pres_note(presentity_t *_p, pa_presence_note_t *n)
{
	DOUBLE_LINKED_LIST_REMOVE(_p->data.first_note, 
			_p->data.last_note, (presence_note_t*)n);
	
	if (use_db) db_remove_pres_note(_p, n);
	free_pres_note(n);
}
	
void free_pres_note(pa_presence_note_t *n)
{
	if (n) {
		str_free_content(&n->data.value);
		str_free_content(&n->data.lang);
		mem_free(n);
	}
}

int remove_pres_notes(presentity_t *p, str *etag)
{
	pa_presence_note_t *n, *nn, *prev;
	int found = 0;

	prev = NULL;
	n = (pa_presence_note_t *)p->data.first_note;
	while (n) {
		nn = (pa_presence_note_t *)n->data.next;
		if (str_case_equals(&n->etag, etag) == 0) {
			/* remove this */
			found++;
			remove_pres_note(p, n);
		}
		else prev = n;
		n = nn;
	}

	return found;
}

pa_presence_note_t *create_pres_note(str *etag, str *note, str *lang, 
		time_t expires, str *dbid)
{
	int size;
	pa_presence_note_t *pan;

	if (!dbid) {
		ERR("invalid parameters\n"); 
		return NULL;
	}
	
	size = sizeof(pa_presence_note_t);
	if (dbid) size += dbid->len;
	if (etag) size += etag->len;

	pan = (pa_presence_note_t*)mem_alloc(size);
	if (!pan) {
		ERR("can't allocate memory (%d)\n", size);
		return pan;
	}
	pan->data.next = NULL;
	pan->data.prev = NULL;
	pan->expires = expires;
	str_dup(&pan->data.value, note);
	str_dup(&pan->data.lang, lang);

	pan->dbid.s = (char*)pan + sizeof(*pan);
	if (dbid) str_cpy(&pan->dbid, dbid); 
	else pan->dbid.len = 0;
	pan->etag.s = after_str_ptr(&pan->dbid);
	str_cpy(&pan->etag, etag);
	return pan;
}

pa_presence_note_t *presence_note2pa(presence_note_t *n, str *etag, time_t expires)
{
	dbid_t id;
	str s;
	
	generate_dbid(id);
	s.len = dbid_strlen(id);
	s.s = dbid_strptr(id);
	return create_pres_note(etag, &n->value, &n->lang, expires, &s);
}

