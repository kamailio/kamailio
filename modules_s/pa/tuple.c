#include "presentity.h"
#include <cds/dbid.h>
#include "paerrno.h"
#include "pa_mod.h"
#include "tuple.h"
#include "tuple_notes.h"
#include "tuple_extensions.h"

void add_presence_tuple_no_wb(presentity_t *_p, presence_tuple_t *_t);

/*
 * Create a new presence_tuple
 */
int new_presence_tuple(str* _contact, time_t expires, 
		presence_tuple_t ** _t, int is_published, str *id,
		str *published_id, str *etag)
{
	presence_tuple_t* tuple;
	int size = 0;
	int len;
	dbid_t tmp;

	if (!_t) {
		paerrno = PA_INTERNAL_ERROR;
		ERR("Invalid parameter value\n");
		return -1;
	}

	if (!id) {
		/* always needed (for PIDF documents, ...) */
		generate_dbid(tmp);
		len = dbid_strlen(tmp);
	}
	else len = id->len;

	size = sizeof(presence_tuple_t);
	if (etag) size += etag->len;
	if (published_id) size += published_id->len;
	if (!is_published) {
		if (_contact) size += _contact->len;
		/* Non-published tuples have contact allocated
		 * together with other data! */
	}
	size += len;
	tuple = (presence_tuple_t*)mem_alloc(size);
	if (!tuple) {
		paerrno = PA_NO_MEMORY;
		ERR("No memory left: size=%d\n", size);
		return -1;
	}
	memset(tuple, 0, sizeof(presence_tuple_t));

	tuple->data.status.basic = presence_tuple_undefined_status;
	
	tuple->data.id.s = ((char*)tuple) + sizeof(presence_tuple_t);
	if (id) str_cpy(&tuple->data.id, id);
	else dbid_strcpy(&tuple->data.id, tmp, len);
	
	tuple->etag.s = after_str_ptr(&tuple->data.id);
	if (etag) str_cpy(&tuple->etag, etag);
	else tuple->etag.len = 0;
	
	tuple->published_id.s = after_str_ptr(&tuple->etag);
	if (published_id) str_cpy(&tuple->published_id, published_id);
	else tuple->published_id.len = 0;
	
	if (is_published) {
		str_dup(&tuple->data.contact, _contact);
		/* published contacts can change */
	}
	else {
		/* non-published contacts can NOT change !!! */
		tuple->data.contact.s = after_str_ptr(&tuple->published_id);
		if (_contact) str_cpy(&tuple->data.contact, _contact);
		else tuple->data.contact.len = 0;
	}
	
	
	tuple->expires = expires;
	tuple->data.priority = default_priority;
	tuple->is_published = is_published;

	*_t = tuple;

	return 0;
}

int db_read_tuples(presentity_t *_p, db_con_t* db)
{
	db_key_t keys[] = { col_pres_id };
	db_op_t ops[] = { OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = _p->pres_id } } };

	int i;
	int r = 0;
	db_res_t *res = NULL;
	db_key_t result_cols[] = { col_basic, col_expires, col_priority, 
		col_contact, col_tupleid, col_etag, 
		col_published_id
	} ;
	
	if (!use_db) return 0;

	if (pa_dbf.use_table(db, presentity_contact_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}
	
	if (pa_dbf.query (db, keys, ops, k_vals,
			result_cols, 1, sizeof(result_cols) / sizeof(db_key_t), 
			0, &res) < 0) {
		ERR("Error while querying DB\n");
		return -1;
	}

	if (!res) return 0; /* ? */
	
	for (i = 0; i < res->n; i++) {
		presence_tuple_t *tuple = NULL;
		db_row_t *row = &res->rows[i];
		db_val_t *row_vals = ROW_VALUES(row);
		str contact = STR_NULL;
		basic_tuple_status_t basic = presence_tuple_undefined_status;
		str id = STR_NULL; 
		str etag = STR_NULL;
		str published_id = STR_NULL;
		
		time_t expires = 0;
		double priority = row_vals[2].val.double_val;
		
#define get_str_val(i,dst)	do{if(!row_vals[i].nul){dst.s=(char*)row_vals[i].val.string_val;dst.len=strlen(dst.s);}}while(0)
#define get_int_val(i,dst)	do{if(!row_vals[i].nul){dst=row_vals[i].val.int_val;}}while(0)
#define get_time_val(i,dst)	do{if(!row_vals[i].nul){dst=row_vals[i].val.time_val;}}while(0)

		get_int_val(0, basic);
		get_time_val(1, expires);
		get_str_val(3, contact);
		get_str_val(4, id);
		get_str_val(5, etag);
		get_str_val(6, published_id);
		
#undef get_str_val		
#undef get_time_val		

		r = new_presence_tuple(&contact, expires, &tuple, 1, &id, 
				&published_id, &etag) | r;
		if (tuple) {
			tuple->data.status.basic = basic;
			LOG(L_DBG, "read tuple %.*s\n", id.len, id.s);
			tuple->data.priority = priority;

			db_read_tuple_notes(_p, tuple, db);
			db_read_tuple_extensions(_p, tuple, db);
			
			add_presence_tuple_no_wb(_p, tuple);
		}
	}
	
	pa_dbf.free_result(db, res);

	return r;
}

static int set_tuple_db_data(presentity_t *_p, presence_tuple_t *tuple,
		db_key_t *cols, db_val_t *vals, int *col_cnt)
{
	int n_updates = 0;

	cols[n_updates] = col_tupleid;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->data.id;
	n_updates++;
	
	cols[n_updates] = col_pres_id;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = _p->pres_id;
	n_updates++;
	
	cols[n_updates] = col_basic;
	vals[n_updates].type = DB_INT;
	vals[n_updates].nul = 0;
	vals[n_updates].val.int_val = tuple->data.status.basic;
	n_updates++;

	cols[n_updates] = col_contact;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->data.contact;
	n_updates++;	
	
	cols[n_updates] = col_etag;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->etag;
	n_updates++;	

	cols[n_updates] = col_published_id;
	vals[n_updates].type = DB_STR;
	vals[n_updates].nul = 0;
	vals[n_updates].val.str_val = tuple->published_id;
	n_updates++;	

	if (tuple->data.priority != 0.0) {
		cols[n_updates] = col_priority;
		vals[n_updates].type = DB_DOUBLE;
		vals[n_updates].nul = 0;
		vals[n_updates].val.double_val = tuple->data.priority;
		n_updates++;
	}
	if (tuple->expires != 0) {
		cols[n_updates] = col_expires;
		vals[n_updates].type = DB_DATETIME;
		vals[n_updates].nul = 0;
		vals[n_updates].val.time_val = tuple->expires;
		n_updates++;
	}
	*col_cnt = n_updates;
	return 0;
}

static int db_add_presence_tuple(presentity_t *_p, presence_tuple_t *t)
{
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	int n_query_cols = 0;
	int res;

	if (!use_db) return 0;
	if (!t->is_published) return 0; /* store only published tuples */
	
	if (set_tuple_db_data(_p, t, query_cols, 
				query_vals, &n_query_cols) != 0) {
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		LOG(L_ERR, "db_add_presence_tuple: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.insert(pa_db, query_cols, query_vals, n_query_cols) < 0) {
		LOG(L_ERR, "db_add_presence_tuple: Can't insert record\n");
		return -1;
	}
		
	res = 0;
	if (db_add_tuple_notes(_p, t) < 0) {
		res = -2;
		ERR("can't add tuple notes into DB\n");
	}
	if (db_add_tuple_extensions(_p, t) < 0) {
		res = -3;
		ERR("can't add tuple extensions into DB\n");
	}

	return res;
}

static int db_remove_presence_tuple(presentity_t *_p, presence_tuple_t *t)
{
	db_key_t keys[] = { col_pres_id, col_tupleid };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = _p->pres_id } },
		{ DB_STR, 0, { .str_val = t->data.id } } };
	
	if (!use_db) return 0;
	if (!t->is_published) return 0; /* store only published tuples */

	db_remove_tuple_notes(_p, t);
	db_remove_tuple_extensions(_p, t);
	
	if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		LOG(L_ERR, "db_remove_presence_tuple: Error in use_table\n");
		return -1;
	}

	if (pa_dbf.delete(pa_db, keys, ops, k_vals, 2) < 0) {
		LOG(L_ERR, "db_remove_presence_tuple: Can't delete record\n");
		return -1;
	}
	
	return 0;
}

int db_update_presence_tuple(presentity_t *_p, presence_tuple_t *t, int update_notes_and_ext)
{
	db_key_t keys[] = { col_pres_id, col_tupleid };
	db_op_t ops[] = { OP_EQ, OP_EQ };
	db_val_t k_vals[] = { { DB_STR, 0, { .str_val = _p->pres_id } },
		{ DB_STR, 0, { .str_val = t->data.id } } };
	
	db_key_t query_cols[20];
	db_val_t query_vals[20];
	int n_query_cols = 0;

	if (!use_db) return 0;
	if (!t->is_published) return 0; /* store only published tuples */

	if (set_tuple_db_data(_p, t, query_cols, 
				query_vals, &n_query_cols) != 0) {
		return -1;
	}
	
	if (pa_dbf.use_table(pa_db, presentity_contact_table) < 0) {
		ERR("Error in use_table\n");
		return -1;
	}

	if (pa_dbf.update(pa_db, keys, ops, k_vals, 
				query_cols, query_vals, 2, n_query_cols) < 0) {
		ERR("Can't update record\n");
		return -1;
	}

	if (update_notes_and_ext) {
		db_update_tuple_notes(_p, t);
		db_update_tuple_extensions(_p, t);
	}
	
	return 0;
}

void add_presence_tuple_no_wb(presentity_t *_p, presence_tuple_t *_t)
{
	DOUBLE_LINKED_LIST_ADD(_p->data.first_tuple,
			_p->data.last_tuple, (presence_tuple_info_t*)_t);
}

void add_presence_tuple(presentity_t *_p, presence_tuple_t *_t)
{
	add_presence_tuple_no_wb(_p, _t);
	if (use_db) db_add_presence_tuple(_p, _t); 
}

void remove_presence_tuple(presentity_t *_p, presence_tuple_t *_t)
{
	DOUBLE_LINKED_LIST_REMOVE(_p->data.first_tuple,
			_p->data.last_tuple, (presence_tuple_info_t*)_t);
	if (use_db) db_remove_presence_tuple(_p, _t);
}

/*
 * Free all memory associated with a presence_tuple
 */
void free_presence_tuple(presence_tuple_t * _t)
{
	if (_t) {
		free_tuple_notes(_t);
		free_tuple_extensions(_t);
		if (_t->is_published) {
			/* Warning: not-published tuples have contact allocated
			 * together with other data => contact can't change! */
			str_free_content(&_t->data.contact);
		}

		mem_free(_t);
	}
}

/*
 * Find a presence_tuple for contact _contact on presentity _p
 */
int find_registered_presence_tuple(str* _contact, presentity_t *_p, presence_tuple_t ** _t)
{
	presence_tuple_t *tuple;
	if (!_contact || !_contact->len || !_p || !_t) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "find_presence_tuple(): Invalid parameter value\n");
		return -1;
	}
	tuple = get_first_tuple(_p);
	while (tuple) {
		/* only contacts from usrloc should have unique contact - published
		 * may be more times !!! */
		if (!tuple->is_published) {
			if (str_nocase_equals(&tuple->data.contact, _contact) == 0) {
				*_t = tuple;
				return 0;
			}
		}
		tuple = (presence_tuple_t*)tuple->data.next;
	}
	return 1;
}

/*
 * Find a presence_tuple on presentity _p
 */
int find_presence_tuple_id(str* id, presentity_t *_p, presence_tuple_t ** _t)
{
	presence_tuple_t *tuple;
	if (!id || !id->len || !_p || !_t) {
		paerrno = PA_INTERNAL_ERROR;
		LOG(L_ERR, "find_presence_tuple_id(): Invalid parameter value\n");
		return -1;
	}
	tuple = get_first_tuple(_p);
	while (tuple) {
		if (str_case_equals(&tuple->data.id, id) == 0) {
			*_t = tuple;
			return 0;
		}
		tuple = (presence_tuple_t*)tuple->data.next;
	}
	return 1;
}

presence_tuple_t *find_published_tuple(presentity_t *presentity, str *etag, str *id)
{
	presence_tuple_t *tuple = get_first_tuple(presentity);
	while (tuple) {
		if (str_case_equals(&tuple->etag, etag) == 0) {
			if (str_case_equals(&tuple->published_id, id) == 0)
				return tuple;
		}
		tuple = get_next_tuple(tuple);
	}
	return NULL;
}

static inline void dup_tuple_notes(presence_tuple_t *dst, presence_tuple_info_t *src)
{
	presence_note_t *n, *nn;
	n = src->first_note;
	while (n) {
		nn = create_presence_note(&n->value, &n->lang);
		if (nn) add_tuple_note_no_wb(dst, nn);
		n = n->next;
	}
}
	
static inline void dup_tuple_extensions(presence_tuple_t *dst, presence_tuple_info_t *src)
{
	extension_element_t *e, *ne;

	e = src->first_unknown_element;
	while (e) {
		ne = create_extension_element(&e->element);
		if (ne) add_tuple_extension_no_wb(dst, ne, 0);
		
		e = e->next;
	}
	
	/* add new extensions for tuple status */
	e = src->status.first_unknown_element;
	while (e) {
		ne = create_extension_element(&e->element);
		if (ne) add_tuple_extension_no_wb(dst, ne, 1);
		
		e = e->next;
	}
}

presence_tuple_t *presence_tuple_info2pa(presence_tuple_info_t *i, str *etag, time_t expires)
{
	presence_tuple_t *t = NULL;
	int res;
			
	/* ID for the tuple is newly generated ! */
	res = new_presence_tuple(&i->contact, expires, &t, 1, NULL, &i->id, etag);
	if (res != 0) return NULL;
	t->data.priority = i->priority;
	t->data.status.basic = i->status.basic;

	/* add notes for tuple */
	dup_tuple_notes(t, i);

	/* add all extension elements */
	dup_tuple_extensions(t, i);

	return t;
}

void update_tuple(presentity_t *p, presence_tuple_t *t, presence_tuple_info_t *i, time_t expires)
{
	t->expires = expires;
	t->data.priority = i->priority;
	t->data.status.basic = i->status.basic;
	
	str_free_content(&t->data.contact);
	str_dup(&t->data.contact, &i->contact);

	/* remove all old notes and extension elements for this tuple */
	free_tuple_notes(t);
	free_tuple_extensions(t);
		
	/* add new notes and new extension elemens for tuple */
	dup_tuple_notes(t, i);
	dup_tuple_extensions(t, i);

	if (use_db) db_update_presence_tuple(p, t, 1);
}
