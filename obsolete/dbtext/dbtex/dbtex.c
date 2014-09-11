/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "../../sr_module.h"
#include <stdio.h>
#include "../../db/db.h"

/*
 * database URL - the path to the directory where the tables are located
 */
#define DB_URL   "/tmp/dbtext"

/**
 * table name - the file name - it must be located in database directory
 */
#define DB_TABLE "location"

#define TRUE 1
#define FALSE 0


/*
 * Database module client example
 */

static int mod_init();

struct module_exports exports= {	
	"DBTExample", 
	(char*[]) {
	},
	(cmd_function[]) {
	},
	(int[]) {
	},
	(fixup_function[]) {
	},
	0,       /* number of functions*/
	0,   /* Module parameter names */
	0,   /* Module parameter types */
	0,   /* Module parameter variable pointers */
	0,   /* Number of module parameters */

	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int print_res(db_res_t* _r)
{
	int i, j;

	for(i = 0; i < RES_COL_N(_r); i++) {
		printf("%s ", RES_NAMES(_r)[i]);
	}
	printf("\n");

	for(i = 0; i < RES_ROW_N(_r); i++) {
		for(j = 0; j < RES_COL_N(_r); j++) {
			if (RES_ROWS(_r)[i].values[j].nul == TRUE) {
				printf("NULL ");
				continue;
			}
			switch(RES_ROWS(_r)[i].values[j].type) {
			case DB_INT:
				printf("%d ", RES_ROWS(_r)[i].values[j].val.int_val);
				break;
			case DB_FLOAT:
				printf("%f ", RES_ROWS(_r)[i].values[j].val.float_val);
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
			
	return TRUE;
}





int mod_init()
{
	     /*
	      * Column names of table location
	      */
	db_key_t keys1[] = {"user", "contact", "q", "expire", "opaque" };
	db_key_t keys2[] = {"user", "q"};
	db_key_t keys3[] = {"user", "contact"};
	db_key_t keys4[] = {"contact", "q"};

	db_val_t vals1[] = { 
		{ DB_STRING  , 0, { .string_val = "foo@bar.com" }              },
		{ DB_STR     , 0, { .str_val    = { "real@foo.bar.com", 18 } } },
		{ DB_DOUBLE  , 0, { .double_val = 1.2 }                        },
		{ DB_DATETIME, 0, { .time_val   = 439826493 }                  },
		{ DB_BLOB    , 0, { .blob_val   = { "hdslgkhas\0glksf", 17 } } }
	};

	db_val_t vals2[] = { 
		{ DB_STRING  , 0, { .string_val = "foo2@bar2.com" }              },
		{ DB_STR     , 0, { .str_val    = { "real2@foo.bar2.com", 18 } } },
		{ DB_DOUBLE  , 0, { .double_val = 1.3 }                          },
		{ DB_DATETIME, 0, { .time_val   = 12345 }                        },
		{ DB_BLOB    , 0, { .blob_val   = { "\0a\0balkdfj", 10 }       } }
	};

	db_val_t vals3[] = { 
		{ DB_STRING  , 0, { .string_val = "foo3@bar3.com" }              },
		{ DB_STR     , 0, { .str_val    = { "real3@foo.bar3.com", 18 } } },
		{ DB_DOUBLE  , 0, { .double_val = 1.5 }                          },
		{ DB_DATETIME, 0, { .time_val   = 123456 }                       },
		{ DB_BLOB    , 0, { .blob_val   = { "halgkasdg\'", 10 }        } }
	};

	db_val_t vals4[] = {
		{ DB_STRING, 0, { .string_val = "foo2@bar2.com" } },
		{ DB_DOUBLE, 0, { .double_val = 1.30 }            }
	};
		  
	db_val_t vals5[] = {
		{ DB_STRING, 0, { .string_val = "foo3@bar3.com" }      },
		{ DB_STRING, 0, { .string_val = "real3@foo.bar3.com" } }
	};

	db_val_t vals6[] = {
		{ DB_STRING, 0, { .string_val = "different@address.com" } },
		{ DB_DOUBLE, 0, { .double_val = 2.5 }                     }
	};

	db_con_t* h;
	db_res_t* res;

	fprintf(stderr, "DBExample - registering...\n");

	     /* The first call must be bind_dbmod
	      * This call will search for functions
	      * exported by a database module
	      */
	if (bind_dbmod()) {
		fprintf(stderr, "Error while binding database module, did you forget to load a database module ?\n");
		return -1;
	}

	     /*
	      * Create a database connection
	      * DB_URL is database URL of form
	      * /path/to/dbtext/database
	      * The function returns handle, that
	      * represents a database connection
	      */
	h = db_init(DB_URL);
	if (!h) {
		fprintf(stderr, "Error while initializing database connection\n");
		return -1;
	}

	     /* 
	      * Specify a table name, that will
	      * be used for manipulations
	      */
	if (db_use_table(h, DB_TABLE) < 0) {
		fprintf(stderr, "Error while calling db_use_table\n");
		return -1;
	}

	     /* If you do not specify any keys and values to be
	      * matched, all rows will be deleted
	      */
	if (db_delete(h, NULL, NULL, NULL, 0) < 0) {
		fprintf(stderr, "Error while flushing table\n");
		return -1;
	}

	if (db_insert(h, keys1, vals1, 5) < 0) {
		fprintf(stderr, "Error while inserting line 1\n");
		return -1;
	}

	if (db_insert(h, keys1, vals2, 5) < 0) {
		fprintf(stderr, "Error while inserting line 2\n");
		return -1;
	}

	if (db_insert(h, keys1, vals3, 5) < 0) {
		fprintf(stderr, "Error while inserting line 3\n");
		return -1;
	}

	     /*
	      * Let's delete middle line with
	      * user = foo2@bar2.com and q = 1.3
	      */
	if (db_delete(h, keys2, NULL, vals4, 2) < 0) {
		fprintf(stderr, "Error while deleting line\n");
		return -1;
	}

	     /*
	      * Modify last line
	      */
	if (db_update(h, keys3, NULL, vals5, keys4, vals6, 2, 2) < 0) {
		fprintf(stderr, "Error while modifying table\n");
		return -1;
	}

	     /*
	      * Last but not least, dump the result of db_query
	      */

	if (db_query(h, NULL, NULL, NULL, NULL, 0, 0, NULL, &res) < 0) {
		fprintf(stderr, "Error while querying table\n");
		return -1;
	}


	print_res(res);

	     /*
	      * Free the result because we don't need it
	      * anymore
	      */
	if (db_free_query(h, res) < 0) {
		fprintf(stderr, "Error while freeing result of query\n");
		return -1;
	}

	     /*
	      * Close existing database connection
	      * and free previously allocated 
	      * memory
	      */
	db_close(h);
	return 0;
}
