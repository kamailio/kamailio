#include "db_res.h"
#include "defs.h"
#include "../../mem.h"
#include "../../dprint.h"


static int get_columns  (db_con_t* _h, db_res_t* _r);
static int convert_rows (db_con_t* _h, db_res_t* _r);
static int free_columns (db_res_t* _r);
static int free_rows    (db_res_t* _r);


/*
 * Create a new result structure and initialize it
 */
db_res_t* new_result(void)
{
	db_res_t* r;
	r = (db_res_t*)pkg_malloc(sizeof(db_res_t*));
	if (!r) {
		log(L_ERR, "new_result(): No memory left\n");
		return NULL;
	}
	r->col.names = NULL;
	r->col.types = NULL;
	r->col.n = 0;
	r->rows = NULL;
	r->n = 0;
	return r;
}


/*
 * Fill the structure with data from database
 */
int convert_result(db_con_t* _h, db_res_t* _r)
{
#ifdef PARANOID
	if (!_h) return FALSE;
	if (!_r) return FALSE;
#endif
	if (get_columns(_h, _r) == FALSE) {
		log(L_ERR, "convert_result(): Error while getting column names\n");
		return FALSE;
	}

	if (convert_rows(_h, _r) == FALSE) {
		log(L_ERR, "convert_result(): Error while converting rows\n");
		free_columns(_r);
		return FALSE;
	}
	return TRUE;
}


static int get_columns(db_con_t* _h, db_res_t* _r)
{
	int n, i;
	MYSQL_FIELD* fields;
#ifdef PARANOID
	if (!_h) return FALSE;
	if (!_r) return FALSE;
#endif
	n = mysql_num_fields(_h->res);
	if (!n) {
		log(L_ERR, "get_names(): No columns\n");
		return FALSE;
	}
	
	_r->col.names = (db_key_t*)pkg_malloc(sizeof(db_key_t) * n);
	_r->col.types = (db_type_t*)pkg_malloc(sizeof(db_type_t) * n);
	if ((!_r->col.names) || (!_r->col.types)) {
		log(L_ERR, "get_names(): No memory left\n");
		pkg_free(_r->col.names);
		pkg_free(_r->col.types);
		return FALSE;
	}
	_r->col.n = n;

	fields = mysql_fetch_fields(_h->res);
	for(i = 0; i < n; i++) {
		_r->col.names[i] = fields[i].name;
		switch(fields[i].type) {
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_TIMESTAMP:
			_r->col.types[i] = DB_INT;
			break;

		case FIELD_TYPE_FLOAT:
			_r->col.types[i] = DB_FLOAT;
			break;

		case FIELD_TYPE_DATETIME:
			_r->col.types[i] = DB_DATETIME;
			break;
			
		default:
			_r->col.types[i] = DB_STRING;
			break;
		}		
	}
	return TRUE;
}


static int convert_rows(db_con_t* _h, db_res_t* _r)
{
	int n, i;
#ifdef PARANOID
	if (!_h) return FALSE;
	if (!_r) return FALSE;
#endif
	n = mysql_num_rows(_h->res);
	if (!n) {
		log(L_ERR, "convert_rows(): No rows found\n");
		return FALSE;
	}
	_r->rows = (struct db_row*)pkg_malloc(sizeof(db_row_t) * n);
	if (!_r->rows) {
		log(L_ERR, "convert_rows(): No memory left\n");
		return FALSE;
	}
	_r->n = n;

	for(i = 0; i < n; i++) {
		_h->row = mysql_fetch_row(_h->res);
		if (!_h->row) {
			log(L_ERR, "convert_rows(): %s\n", mysql_error(&(_h->con)));
			_r->n = i;
			free_rows(_r);
			return FALSE;
		}
		if (convert_row(_h, _r, &(_r->rows[i])) == FALSE) {
			log(L_ERR, "convert_rows(): Error while converting row #%d\n", i);
			_r->n = i;
			free_rows(_r);
			return FALSE;
		}
	}
	return TRUE;
}


int free_result(db_res_t* _r)
{
#ifdef PARANOID
	if (!_r) return FALSE;
#endif
	free_columns(_r);
	free_rows(_r);
	pkg_free(_r);
	return TRUE;
}


static int free_columns(db_res_t* _r)
{
#ifdef PARANOID
	if (!_r) return FALSE;
#endif
	pkg_free(_r->col.names);
	pkg_free(_r->col.types);
	return TRUE;
}


static int free_rows(db_res_t* _r)
{
	int i;
#ifdef PARANOID
	if (!_r) return FALSE;
#endif
	for(i = 0; i < _r->n; i++) {
		free_row(&(_r->rows[i]));
	}
	pkg_free(_r->rows);
	return TRUE;
}
