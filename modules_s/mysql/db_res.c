/* 
 * $Id$ 
 */

#include "db_res.h"
#include "defs.h"
#include <stdlib.h>
#include "../../dprint.h"
#include "../../mem/mem.h"


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
	r = (db_res_t*)pkg_malloc(sizeof(db_res_t));
	if (!r) {
		log(L_ERR, "new_result(): No memory left\n");
		return NULL;
	}
	RES_NAMES(r) = NULL;
	RES_TYPES(r) = NULL;
	RES_COL_N(r) = 0;
	RES_ROWS(r) = NULL;
	RES_ROW_N(r) = 0;
	return r;
}


/*
 * Fill the structure with data from database
 */
int convert_result(db_con_t* _h, db_res_t* _r)
{
#ifdef PARANOID
	if ((!_h) || (!_r)) {
		log(L_ERR, "convert_result(): Invalid parameter\n");
		return FALSE;
	}
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
	if ((!_h) || (!_r)) {
		log(L_ERR, "get_columns(): Invalid parameter\n");
		return FALSE;
	}
#endif
	n = mysql_field_count(CON_CONNECTION(_h));
	if (!n) {
		log(L_ERR, "get_names(): No columns\n");
		return FALSE;
	}
	
        RES_NAMES(_r) = (db_key_t*)pkg_malloc(sizeof(db_key_t) * n);
	RES_TYPES(_r) = (db_type_t*)pkg_malloc(sizeof(db_type_t) * n);
	if ((!RES_NAMES(_r)) || (!RES_TYPES(_r))) {
		log(L_ERR, "get_names(): No memory left\n");
		pkg_free(RES_NAMES(_r));
		pkg_free(RES_TYPES(_r));
		return FALSE;
	}
	RES_COL_N(_r) = n;

	fields = mysql_fetch_fields(CON_RESULT(_h));
	for(i = 0; i < n; i++) {
		RES_NAMES(_r)[i] = fields[i].name;
		switch(fields[i].type) {
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_TIMESTAMP:
			RES_TYPES(_r)[i] = DB_INT;
			break;

		case FIELD_TYPE_FLOAT:
		case FIELD_TYPE_DOUBLE:
			RES_TYPES(_r)[i] = DB_DOUBLE;
			break;

		case FIELD_TYPE_DATETIME:
			RES_TYPES(_r)[i] = DB_DATETIME;
			break;
			
		default:
			RES_TYPES(_r)[i] = DB_STRING;
			break;
		}		
	}
	return TRUE;
}


static int convert_rows(db_con_t* _h, db_res_t* _r)
{
	int n, i;
#ifdef PARANOID
	if ((!_h) || (!_r)) {
		log(L_ERR, "convert_rows(): Invalid parameter\n");
		return FALSE;
	}
#endif
	n = mysql_num_rows(CON_RESULT(_h));
	RES_ROW_N(_r) = n;
	if (!n) {
		RES_ROWS(_r) = NULL;
		return TRUE;
	}
	RES_ROWS(_r) = (struct db_row*)pkg_malloc(sizeof(db_row_t) * n);
	if (!RES_ROWS(_r)) {
		log(L_ERR, "convert_rows(): No memory left\n");
		return FALSE;
	}

	for(i = 0; i < n; i++) {
		CON_ROW(_h) = mysql_fetch_row(CON_RESULT(_h));
		if (!CON_ROW(_h)) {
			log(L_ERR, "convert_rows(): %s\n", mysql_error(CON_CONNECTION(_h)));
			RES_ROW_N(_r) = i;
			free_rows(_r);
			return FALSE;
		}
		if (convert_row(_h, _r, &(RES_ROWS(_r)[i])) == FALSE) {
			log(L_ERR, "convert_rows(): Error while converting row #%d\n", i);
			RES_ROW_N(_r) = i;
			free_rows(_r);
			return FALSE;
		}
	}
	return TRUE;
}


int free_result(db_res_t* _r)
{
#ifdef PARANOID
	if (!_r) {
		log(L_ERR, "free_result(): Invalid parameter\n");
		return FALSE;
	}
#endif
	free_columns(_r);
	free_rows(_r);
	pkg_free(_r);
	return TRUE;
}


static int free_columns(db_res_t* _r)
{
#ifdef PARANOID
	if (!_r) {
		log(L_ERR, "free_columns(): Invalid parameter\n");
		return FALSE;
	}
#endif
	if (RES_NAMES(_r)) pkg_free(RES_NAMES(_r));
	if (RES_TYPES(_r)) pkg_free(RES_TYPES(_r));
	return TRUE;
}


static int free_rows(db_res_t* _r)
{
	int i;
#ifdef PARANOID
	if (!_r) {
		log(L_ERR, "free_rows(): Invalid parameter value\n");
		return FALSE;
	}
#endif
	for(i = 0; i < RES_ROW_N(_r); i++) {
		free_row(&(RES_ROWS(_r)[i]));
	}
	if (RES_ROWS(_r)) pkg_free(RES_ROWS(_r));
	return TRUE;
}
