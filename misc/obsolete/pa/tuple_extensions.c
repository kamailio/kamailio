#include "presentity.h"
#include "pa_mod.h"
#include "tuple_extensions.h"
#include <cds/logger.h>


/* DB manipulation */

static int db_add_tuple_extension(presentity_t *p, presence_tuple_t *t, extension_element_t *n, int is_status_extension)
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

	cols[n_updates] = col_element;
	vals[n_updates].type = DB_BLOB;
	vals[n_updates].nul = 0;
	vals[n_updates].val.blob_val = n->element;
	n_updates++;
	
	cols[n_updates] = col_status_extension;
	vals[n_updates].type = DB_INT;
	vals[n_updates].nul = 0;
	vals[n_updates].val.int_val = is_status_extension;
	n_updates++;
	
	/* run update */
	
	if (pa_dbf.use_table(pa_db, tuple_extensions_table) < 0) {
		LOG(L_ERR, "db_add_pres_note: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.insert(pa_db, cols, vals, n_updates) < 0) {
		LOG(L_ERR, "db_add_pres_note: Can't insert record\n");
		return -1;
	}
	
	return 0;
}

int db_add_tuple_extensions(presentity_t *p, presence_tuple_t *t)
{
	extension_element_t *n;
	
	n = t->data.first_unknown_element;
	while (n) {
		db_add_tuple_extension(p, t, n, 0);
		n = n->next;
	}
	
	n = t->data.status.first_unknown_element;
	while (n) {
		db_add_tuple_extension(p, t, n, 1);
		n = n->next;
	}
	return 0;
}

int db_remove_tuple_extensions(presentity_t *p, presence_tuple_t *t)
{
	db_key_t keys[] = { col_pres_id, col_tupleid };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { 
		{ DB_STR, 0, { .str_val = p->pres_id } },
		{ DB_STR, 0, { .str_val = t->data.id } }
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(pa_db, tuple_extensions_table) < 0) {
		LOG(L_ERR, "db_remove_tuple_extensions: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 2) < 0) {
		LOG(L_ERR, "db_remove_tuple_extensions: Can't delete record\n");
		return -1;
	}
	
	return 0;
}

int db_update_tuple_extensions(presentity_t *p, presence_tuple_t *t)
{
	db_remove_tuple_extensions(p, t);
	db_add_tuple_extensions(p, t);
	return 0;
}

int db_read_tuple_extensions(presentity_t *p, presence_tuple_t *t, db_con_t* db)
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
	db_key_t result_cols[] = { col_element, col_status_extension };
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(db, tuple_extensions_table) < 0) {
		LOG(L_ERR, "db_read_tuple_extensions: Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.query (db, keys, ops, k_vals,
			result_cols, 2, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		LOG(L_ERR, "db_read_tuple_extensions(): Error while querying extensions\n");
		return -1;
	}

	if (!res) return 0; /* ? */
	
	for (i = 0; i < res->n; i++) {
		extension_element_t *n = NULL;
		db_row_t *row = &res->rows[i];
		db_val_t *row_vals = ROW_VALUES(row);
		str element = STR_NULL;
		int status_extension = 0;
		
#define get_str_val(i,dst)	do{if(!row_vals[i].nul){dst.s=(char*)row_vals[i].val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_int_val(rvi,dst)	do{if(!rvi.nul){dst=rvi.val.int_val;}else dst=0;}while(0)

		get_str_val(0, element);
		get_int_val(row_vals[1], status_extension);
		
#undef get_str_val		
#undef get_int_val		

		n = create_extension_element(&element);
		if (n) add_tuple_extension_no_wb(t, n, status_extension);
	}
	
	pa_dbf.free_result(db, res);
	
	return r;

}

/* in memory operations */

void add_tuple_extension_no_wb(presence_tuple_t *t, extension_element_t *n, int is_status_extension) 
{
	if (is_status_extension)
		DOUBLE_LINKED_LIST_ADD(t->data.status.first_unknown_element, 
				t->data.status.last_unknown_element, n);
	else DOUBLE_LINKED_LIST_ADD(t->data.first_unknown_element, 
			t->data.last_unknown_element, n);
}

void free_tuple_extensions(presence_tuple_t *t)
{
	extension_element_t *n, *nn;

	/* remove all extensions for this tuple */
	n = t->data.first_unknown_element;
	while (n) {
		nn = n->next;
		free_extension_element(n);
		n = nn;
	}
	
	/* remove all status extensions for this tuple */
	n = t->data.status.first_unknown_element;
	while (n) {
		nn = n->next;
		free_extension_element(n);
		n = nn;
	}
	
	t->data.first_unknown_element = NULL;
	t->data.last_unknown_element = NULL;
	t->data.status.first_unknown_element = NULL;
	t->data.status.last_unknown_element = NULL;
}

