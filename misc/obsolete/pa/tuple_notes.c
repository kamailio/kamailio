#include "presentity.h"
#include "pa_mod.h"
#include "tuple_notes.h"
#include <cds/logger.h>


/* DB manipulation */

static int db_add_tuple_note(presentity_t *p, presence_tuple_t *t, presence_note_t *n)
{
	db_key_t cols[20];
	db_val_t vals[20];
	int n_updates = 0;

	if (!use_db) return 0;
	
	/* set data */
	
	cols[n_updates] = col_pres_id;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = p->pres_id;
	n_updates++;
	
	cols[n_updates] = col_tupleid;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = t->data.id;
	n_updates++;

	cols[n_updates] = col_note;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->value;
	n_updates++;
	
	cols[n_updates] = col_lang;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->lang;
	n_updates++;
	
	/* run update */
	
	if (pa_dbf.use_table(pa_db, tuple_notes_table) < 0) {
		LOG(L_ERR, "db_add_pres_note: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.insert(pa_db, cols, vals, n_updates) < 0) {
		LOG(L_ERR, "db_add_pres_note: Can't insert record\n");
		return -1;
	}
	
	return 0;
}

int db_add_tuple_notes(presentity_t *p, presence_tuple_t *t)
{
	presence_note_t *n;
	
	n = t->data.first_note;
	while (n) {
		db_add_tuple_note(p, t, n);
		n = n->next;
	}
	return 0;
}

int db_remove_tuple_notes(presentity_t *p, presence_tuple_t *t)
{
	db_key_t keys[] = { col_pres_id, col_tupleid };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, { .str_val = p->pres_id } },
		{ DB_STR, 0, { .str_val = t->data.id } }
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(pa_db, tuple_notes_table) < 0) {
		LOG(L_ERR, "db_remove_tuple_notes: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 2) < 0) {
		LOG(L_ERR, "db_remove_tuple_notes: Can't delete record\n");
		return -1;
	}
	
	return 0;
}

int db_update_tuple_notes(presentity_t *p, presence_tuple_t *t)
{
	db_remove_tuple_notes(p, t);
	db_add_tuple_notes(p, t);
	return 0;
}

int db_read_tuple_notes(presentity_t *p, presence_tuple_t *t, db_con_t* db)
{
	db_key_t keys[] = { col_pres_id, col_tupleid };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, { .str_val = p->pres_id } },
		{ DB_STR, 0, { .str_val = t->data.id } }
	};

	int i;
	int r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { col_note, col_lang };
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(db, tuple_notes_table) < 0) {
		LOG(L_ERR, "db_read_tuple_notes: Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.query (db, keys, ops, k_vals,
			result_cols, 2, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		LOG(L_ERR, "db_read_tuple_notes(): Error while querying notes\n");
		return -1;
	}

	if (!res) return 0; /* ? */
	
	for (i = 0; i < res->n; i++) {
		presence_note_t *n = NULL;
		db_row_t *row = &res->rows[i];
		db_val_t *row_vals = ROW_VALUES(row);
		str note = STR_NULL;
		str lang = STR_NULL;
		
#define get_str_val(i,dst)	do{if(!row_vals[i].nul){dst.s=(char*)row_vals[i].val.string_val;dst.len=strlen(dst.s);}}while(0)

		get_str_val(0, note);
		get_str_val(1, lang);
		
#undef get_str_val		

		n = create_presence_note(&note, &lang);
		if (n) add_tuple_note_no_wb(t, n);
	}
	
	pa_dbf.free_result(db, res);
	
	return r;

}

/* in memory operations */

void add_tuple_note_no_wb(presence_tuple_t *t, presence_note_t *n) 
{
	DOUBLE_LINKED_LIST_ADD(t->data.first_note, t->data.last_note, n);
}

void free_tuple_notes(presence_tuple_t *t)
{
	presence_note_t *n, *nn;
	
	/* remove all notes for this tuple */
	n = t->data.first_note;
	while (n) {
		nn = n->next;
		free_presence_note(n);
		n = nn;
	}
	
	t->data.first_note = NULL;
	t->data.last_note = NULL;
}

