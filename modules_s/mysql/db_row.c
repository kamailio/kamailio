/* 
 * $Id$ 
 *
 * MySQL module row related functions
 */

#include "../../db/db_row.h"
#include "defs.h"
#include <mysql/mysql.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "con_mysql.h"

int convert_row(db_con_t* _h, db_res_t* _res, db_row_t* _r)
{
	unsigned long* lengths;
	int i;
#ifndef PARANOID
	if ((!_h) || (!_r) || (!_n)) {
		log(L_ERR, "convert_row(): Invalid parameter value\n");
		return -1;
	}
#endif

        ROW_VALUES(_r) = (db_val_t*)pkg_malloc(sizeof(db_val_t) * RES_COL_N(_res));
	ROW_N(_r) = RES_COL_N(_res);
	if (!ROW_VALUES(_r)) {
		LOG(L_ERR, "convert_row(): No memory left\n");
		return -1;
	}

	lengths = mysql_fetch_lengths(CON_RESULT(_h));

	for(i = 0; i < RES_COL_N(_res); i++) {
		if (str2val(RES_TYPES(_res)[i], &(ROW_VALUES(_r)[i]), 
			    ((MYSQL_ROW)CON_ROW(_h))[i], lengths[i]) < 0) {
			LOG(L_ERR, "convert_row(): Error while converting value\n");
			free_row(_r);
			return -3;
		}
	}
	return 0;
}


int free_row(db_row_t* _r)
{
#ifndef PARANOID
	if (!_r) {
		LOG(L_ERR, "free_row(): Invalid parameter value\n");
		return -1;
	}
#endif
	if (ROW_VALUES(_r)) pkg_free(ROW_VALUES(_r));
	return 0;
}

