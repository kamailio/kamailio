#include "db_row.h"
#include "defs.h"
#include <mysql/mysql.h>
#include "../../mem.h"
#include "../../dprint.h"


int convert_row(db_con_t* _h, db_res_t* _res, db_row_t* _r)
{
	int i;
#ifndef PARANOID
	if (!_h) return FALSE;
	if (!_r) return FALSE;
	if (!_n) return FALSE;
#endif
        _r->values = (db_val_t*)pkg_malloc(sizeof(db_val_t) * _res->col.n);
	_r->n = _res->col.n;
	if (!_r->values) {
		log(L_ERR, "convert_row(): No memory left\n");
		return FALSE;
	}

	for(i = 0; i < _res->col.n; i++) {
		if (str2val(_res->col.types[i], &(_r->values[i]), _h->row[i]) == FALSE) {
			log(L_ERR, "Error while converting value\n");
			free_row(_r);
			return FALSE;
		}
	}
}


int free_row(db_row_t* _r)
{
#ifndef PARANOID
	if (!_r) return FALSE;
#endif
	pkg_free(_r->values);
}

