#include "presentity.h"
#include "extension_elements.h"
#include "pa_mod.h"
#include <cds/logger.h>
#include <cds/list.h>

/* DB manipulation */

static int db_add_extension_element(presentity_t *p, pa_extension_element_t *n)
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

	cols[n_updates] = col_element;
	vals[n_updates].type = DB_BLOB;
	vals[n_updates].nul = 0;
	vals[n_updates].val.blob_val = n->data.element;
	n_updates++;
	
	cols[n_updates] = col_expires;
	vals[n_updates].type = DB_DATETIME;
	vals[n_updates].nul = 0;
	vals[n_updates].val.time_val = n->expires;
	n_updates++;	
	
	/* run update */
	
	if (pa_dbf.use_table(pa_db, extension_elements_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.insert(pa_db, cols, vals, n_updates) < 0) {
		ERR("Can't insert record\n");
		return -1;
	}
	
	return 0;
}

static int db_remove_extension_element(presentity_t *p, pa_extension_element_t *n)
{
	db_key_t keys[] = { col_pres_id, col_etag, col_dbid };
	db_op_t ops[] = { OP_EQ, OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = p->pres_id } },
		{ DB_STR, 0, { .str_val = n->etag } },
		{ DB_STR, 0, { .str_val = n->dbid } }
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(pa_db, extension_elements_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 3) < 0) {
		ERR("Can't delete record\n");
		return -1;
	}
	
	return 0;
}

int db_update_extension_element(presentity_t *p, pa_extension_element_t *n)
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

	cols[n_updates] = col_element;
	vals[n_updates].type = DB_BLOB;
	vals[n_updates].nul = 0;
	vals[n_updates].val.blob_val = n->data.element;
	n_updates++;
	
	cols[n_updates] = col_expires;
	vals[n_updates].type = DB_DATETIME;
	vals[n_updates].nul = 0;
	vals[n_updates].val.time_val = n->expires;
	n_updates++;	
	
	if (pa_dbf.use_table(pa_db, extension_elements_table) < 0) {
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

int db_read_extension_elements(presentity_t *p, db_con_t* db)
{
	db_key_t keys[] = { col_pres_id };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = p->pres_id } } };

	int i;
	int r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { col_dbid, col_etag, 
		col_element, col_dbid, col_expires
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(db, extension_elements_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.query (db, keys, ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		ERR("Error while querying presence extension_elements\n");
		return -1;
	}

	if (!res) return 0; /* ? */
	
	for (i = 0; i < res->n; i++) {
		pa_extension_element_t *n = NULL;
		db_row_t *row = &res->rows[i];
		db_val_t *row_vals = ROW_VALUES(row);
		str dbid = STR_NULL; 
		str etag = STR_NULL;
		str extension_element = STR_NULL;
		str id = STR_NULL;
		time_t expires = 0;
		
#define get_str_val(i,dst)	do{if(!row_vals[i].nul){dst.s=(char*)row_vals[i].val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_blob_val(i,dst)	do{if(!row_vals[i].nul){dst=row_vals[i].val.blob_val;}}while(0)
#define get_time_val(i,dst)	do{if(!row_vals[i].nul){dst=row_vals[i].val.time_val;}}while(0)

		get_str_val(0, dbid);
		get_str_val(1, etag);
		get_blob_val(2, extension_element);
		get_str_val(3, id);
		get_time_val(4, expires);
		
#undef get_str_val		
#undef get_time_val		

		n = create_pa_extension_element(&etag, &extension_element, expires, &dbid);
		if (n) DOUBLE_LINKED_LIST_ADD(p->data.first_unknown_element, 
				p->data.last_unknown_element, (extension_element_t*)n);	
	}
	
	pa_dbf.free_result(db, res);
	
	return r;
}

/* in memory presence extension_elements manipulation */

void add_extension_element(presentity_t *_p, pa_extension_element_t *n)
{
	DOUBLE_LINKED_LIST_ADD(_p->data.first_unknown_element, 
		_p->data.last_unknown_element, (extension_element_t*)n);	
	if (use_db) db_add_extension_element(_p, n); 
}

void remove_extension_element(presentity_t *_p, pa_extension_element_t *n)
{
	DOUBLE_LINKED_LIST_REMOVE(_p->data.first_unknown_element, 
			_p->data.last_unknown_element, 
			(extension_element_t*)n);

	if (use_db) db_remove_extension_element(_p, n);
	free_pa_extension_element(n);
}
	
void free_pa_extension_element(pa_extension_element_t *n)
{
	if (n) mem_free(n);
}

int remove_extension_elements(presentity_t *p, str *etag)
{
	pa_extension_element_t *n, *nn;
	int found = 0;

	n = (pa_extension_element_t*)p->data.first_unknown_element;
	while (n) {
		nn = (pa_extension_element_t*)n->data.next;
		if (str_case_equals(&n->etag, etag) == 0) {
			/* remove this */
			found++;
			remove_extension_element(p, n);
		}
		n = nn;
	}

	return found;
}

pa_extension_element_t *create_pa_extension_element(str *etag, 
		str *extension_element, 
		time_t expires, str *dbid)
{
	pa_extension_element_t *pan;
	int size;
	
	if (!dbid) {
		ERR("invalid parameters\n"); 
		return NULL;
	}
	
	size = sizeof(pa_extension_element_t);
	if (dbid) size += dbid->len;
	if (etag) size += etag->len;
	if (extension_element) size += extension_element->len;

	pan = (pa_extension_element_t*)mem_alloc(size);
	if (!pan) {
		ERR("can't allocate memory: %d\n", size);
		return pan;
	}

	memset(pan, 0, sizeof(*pan));
	
	pan->expires = expires;
	
	pan->dbid.s = (char*)pan + sizeof(pa_extension_element_t);
	if (dbid) str_cpy(&pan->dbid, dbid);
	else pan->dbid.len = 0;
	
	pan->etag.s = after_str_ptr(&pan->dbid);
	str_cpy(&pan->etag, etag);
	
	pan->data.element.s = after_str_ptr(&pan->etag);
	str_cpy(&pan->data.element, extension_element);

	return pan;
}

pa_extension_element_t *extension_element2pa(extension_element_t *n, str *etag, time_t expires)
{
	dbid_t id;
	str s;
	
	generate_dbid(id);
	s.len = dbid_strlen(id);
	s.s = dbid_strptr(id);
	return create_pa_extension_element(etag, &n->element, expires, &s);
}

