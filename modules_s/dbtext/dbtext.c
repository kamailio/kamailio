/* 
 *
 * DBText module interface
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * DBText module interface
 *  
 * 2003-01-30 created by Daniel
 * 
 */

#include <stdio.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "dbtext.h"
#include "dbt_lib.h"

static int mod_init(void);
void destroy(void);

/**** dev test ****/
int make_demo();
static int print_res(db_res_t* _r);

struct module_exports exports = {	
	"mysql",
	(char*[]) {
		"~db_use_table",
		"~db_init",
		"~db_close",
		"~db_query",
		"~db_raw_query",
		"~db_free_query",
		"~db_insert",
		"~db_delete",
		"~db_update"
	},
	(cmd_function[]) {
		(cmd_function)use_table,
		(cmd_function)dbt_init,
		(cmd_function)dbt_close,
		(cmd_function)dbt_query,
		(cmd_function)dbt_raw_query,
		(cmd_function)dbt_free_query,
		(cmd_function)dbt_insert,
		(cmd_function)dbt_delete,
		(cmd_function)dbt_update
	},
	(int[]) {
		2, 1, 2, 2, 2, 2, 2, 2, 2
	},
	(fixup_function[]) {
		0, 0, 0, 0, 0, 0, 0, 0, 0
	},
	9, /* number of functions*/

	0,   /* Module parameter names */
	0,   /* Module parameter types */
	0,   /* Module parameter variable pointers */
	0,   /* Number of module paramers */

	mod_init, /* module initialization function */
	0,        /* response function*/
	destroy,  /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	if(dbt_init_cache())
		return -1;
	//return make_demo();
	
	return 0;
}

void destroy(void)
{
	DBG("DBT:destroy ...\n");
	dbt_cache_print(0);
	dbt_cache_destroy();
}


/**** dev test ****/

static int print_res(db_res_t* _r)
{
	int i, j;

	printf("\nprint_res\n\n");
	
	for(i = 0; i < RES_COL_N(_r); i++) {
		printf("%s ", RES_NAMES(_r)[i]);
	}
	printf("\n");
	for(i = 0; i < RES_ROW_N(_r); i++) {
		for(j = 0; j < RES_COL_N(_r); j++) {
			if (RES_ROWS(_r)[i].values[j].nul == 1) {
				printf("NULL ");
				continue;
			}
			switch(RES_ROWS(_r)[i].values[j].type) {
			case DB_INT:
				printf("%d ", RES_ROWS(_r)[i].values[j].val.int_val);
				break;
			case DB_DOUBLE:
				printf("%f ", RES_ROWS(_r)[i].values[j].val.double_val);
				break;
			case DB_DATETIME:
				printf("%s ", ctime(&(RES_ROWS(_r)[i].values[j].val.time_val)));
				break;
			case DB_STRING:
				printf("%s ", RES_ROWS(_r)[i].values[j].val.string_val);
				break;
			case DB_STR:
				printf("%.*s ", 
				       RES_ROWS(_r)[i].values[j].val.str_val.len,
				       RES_ROWS(_r)[i].values[j].val.str_val.s);
				break;

			case DB_BLOB:
				printf("%.*s ",
				       RES_ROWS(_r)[i].values[j].val.blob_val.len,
				       RES_ROWS(_r)[i].values[j].val.blob_val.s);
				break;
			}
			
		}
		printf("\n");
	}
			
	return 1;
}


int make_demo()
{
	db_key_t keys0[] = {"id"};
	db_key_t keys1[] = {"name", "flag"};
	db_key_t keys2[] = {/** "id", **/ "name", "flag"};
	db_op_t db_ops[1] = { OP_LEQ };	
	db_val_t vals0[] = {
                { DB_INT  , 0, { .int_val = 2 }}
	};
	
	db_val_t vals1[] = {
                { DB_STR  , 0, { .str_val = {"YYYYY", 5} }},
                { DB_DOUBLE  , 0, { .double_val = 9.75} }
	};
	
	db_val_t vals2[] = {
                /** { DB_INT  , 0, { .int_val = 4 }}, **/
                { DB_STR  , 0, { .str_val = {"xxx", 3} }},
                { DB_DOUBLE  , 0, { .double_val = 3.25} }
	};
	
	db_con_t* _h = NULL;
	db_res_t *db_res = NULL;

	char *_sqlurl = "/tmp/dbtext";

	_h = dbt_init( _sqlurl);
	if(!_h)
	{
		DBG("DBT:mod_init: error init db\n");
		return -1;
	}

	if(use_table(_h, "test1"))
	{
		DBG("DBT:mod_init: error use table\n");
		return -1;
	}
		
	if(dbt_query(_h, keys0, db_ops, vals0, keys1, 1, 2, NULL, &db_res))
	//if(dbt_query(_h, 0, 0, 0, 0, 0, 0, 0, &db_res))
	{
		DBG("DBT:mod_init: error query\n");
		return -1;
	}
	print_res(db_res);
	
	dbt_cache_print(1);
	
	if(dbt_insert(_h, keys2, vals2, 2))
	{
		DBG("DBT:mod_init: error insert\n");
		return -1;
	}

	dbt_cache_print(1);
	
	//if(dbt_query(_h, 0, 0, 0, 0, 0, 0, 0, &db_res))
	//{
	//	DBG("DBT:mod_init: error query 2\n");
	//	return -1;
	//}
	//print_res(db_res);
	
	//if(dbt_update(_h, keys0, db_ops, vals0, keys1, vals1, 1, 2))
	if(dbt_update(_h, 0, 0, 0, keys1, vals1, 1, 2))
	{
		DBG("DBT:mod_init: error updating\n");
		return -1;
	}

	sleep(2);

	if(dbt_delete(_h, keys0, db_ops, vals0, 1))
	//if(dbt_delete(_h, 0, 0, 0, 0))
	{
		DBG("DBT:mod_init: error delte\n");
		return -1;
	}
	
	if(use_table(_h, "test3"))
	{
		DBG("DBT:mod_init: error use table 3\n");
		return -1;
	}
	
	if(dbt_query(_h, 0, 0, 0, 0, 0, 0, 0, &db_res))
	{
		DBG("DBT:mod_init: error query 3\n");
		return -1;
	}
	print_res(db_res);

	dbt_cache_print(1);
	
	return 0;
	
}
