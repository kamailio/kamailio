#include "presentity.h"
#include "pa_mod.h"
#include <cds/logger.h>

static void add_pres_note_no_wb(presentity_t *_p, pa_presence_note_t *n)
{
	pa_presence_note_t *notes = _p->notes;
	n->next = notes;
	_p->notes = n;
	if (notes) notes->prev = n;
}

/* DB manipulation */

static int db_add_pres_note(presentity_t *p, pa_presence_note_t *n)
{
	db_key_t cols[20];
	db_val_t vals[20];
	int n_updates = 0;

	if (!use_db) return 0;
	
	/* set data */
	
	cols[n_updates] = "dbid";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->dbid;
	n_updates++;
	
	cols[n_updates] = "presid";
	vals[n_updates].type = DB_INT;
	vals[n_updates].nul = 0;
	vals[n_updates].val.int_val = p->presid;
	n_updates++;
	
	cols[n_updates] = "etag";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->etag;
	n_updates++;

	cols[n_updates] = "note";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->note;
	n_updates++;
	
	cols[n_updates] = "lang";
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = n->lang;
	n_updates++;
	
	cols[n_updates] = "expires";
	vals[n_updates].type = DB_DATETIME;
	vals[n_updates].nul = 0;
	vals[n_updates].val.time_val = n->expires;
	n_updates++;	
	
	/* run update */
	
	if (pa_dbf.use_table(pa_db, presentity_notes_table) < 0) {
		LOG(L_ERR, "db_add_pres_note: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.insert(pa_db, cols, vals, n_updates) < 0) {
		LOG(L_ERR, "db_add_pres_note: Can't insert record\n");
		return -1;
	}
	
	return 0;
}

int db_remove_pres_notes(presentity_t *p)
{
	db_key_t keys[] = { "presid" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_INT, 0, { .int_val = p->presid } } };
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(pa_db, presentity_notes_table) < 0) {
		LOG(L_ERR, "db_remove_pres_notes: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 1) < 0) {
		LOG(L_ERR, "db_remove_pres_notes: Can't delete record\n");
		return -1;
	}
	
	return 0;
}

static int db_remove_pres_note(presentity_t *p, pa_presence_note_t *n)
{
	db_key_t keys[] = { "presid", "etag", "dbid" };
	db_op_t ops[] = { OP_EQ, OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_INT, 0, { .int_val = p->presid } },
		{ DB_STR, 0, { .str_val = n->etag } },
		{ DB_STR, 0, { .str_val = n->dbid } }
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(pa_db, presentity_notes_table) < 0) {
		LOG(L_ERR, "db_remove_pres_note: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 3) < 0) {
		LOG(L_ERR, "db_remove_pres_note: Can't delete record\n");
		return -1;
	}
	
	return 0;
}

int db_read_notes(presentity_t *p, db_con_t* db)
{
	db_key_t keys[] = { "presid" };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_INT, 0, { .int_val = p->presid } } };

	int i;
	int r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { "dbid", "etag", 
		"note", "lang", "expires"
	};
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(db, presentity_notes_table) < 0) {
		LOG(L_ERR, "db_read_notes: Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.query (db, keys, ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		LOG(L_ERR, "db_read_notes(): Error while querying presence notes\n");
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
		if (n) add_pres_note_no_wb(p, n);
	}
	
	pa_dbf.free_result(db, res);
	
	return r;
}

/* in memory presence notes manipulation */

void add_pres_note(presentity_t *_p, pa_presence_note_t *n)
{
	add_pres_note_no_wb(_p, n);
	if (use_db) db_add_pres_note(_p, n); 
}

void remove_pres_note(presentity_t *_p, pa_presence_note_t *n)
{
	pa_presence_note_t *notes = _p->notes;
	
	if (notes == n) _p->notes = n->next;
	if (n->prev) n->prev->next = n->next;
	if (n->next) n->next->prev = n->prev;

	if (use_db) db_remove_pres_note(_p, n);
	free_pres_note(n);
}
	
void free_pres_note(pa_presence_note_t *n)
{
	if (n) {
		str_free_content(&n->etag);
		str_free_content(&n->note);
		str_free_content(&n->lang);
		str_free_content(&n->dbid);
		shm_free(n);
	}
}

int remove_pres_notes(presentity_t *p, str *etag)
{
	pa_presence_note_t *n, *nn, *prev;
	int found = 0;

	prev = NULL;
	n = p->notes;
	while (n) {
		nn = n->next;
		if (str_strcasecmp(&n->etag, etag) == 0) {
			/* remove this */
			found++;
			if (prev) prev->next = nn;
			else p->notes = nn;
			remove_pres_note(p, n);
		}
		else prev = n;
		n = nn;
	}

	return found;
}

/* generate ID for given data */
static void generate_dbid(str *id, void *data)
{
	char tmp[256];
	if (id) {
		snprintf(tmp, sizeof(tmp), "%py%xy%x", 
				data, (int)time(NULL), rand());
		str_dup_zt(id, tmp);
	}
}

pa_presence_note_t *create_pres_note(str *etag, str *note, str *lang, time_t expires, str *dbid)
{
	pa_presence_note_t *pan = (pa_presence_note_t*)shm_malloc(sizeof(pa_presence_note_t));
	if (!pan) return pan;
	pan->next = NULL;
	pan->prev = NULL;
	pan->expires = expires;
	str_dup(&pan->etag, etag);
	str_dup(&pan->note, note);
	str_dup(&pan->lang, lang);
	if (dbid) str_dup(&pan->dbid, dbid);
	else generate_dbid(&pan->dbid, pan);
	return pan;
}

