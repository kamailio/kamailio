#ifndef __DB_CON_H__
#define __DB_CON_H__

/*
 * Database connection data type for database module
 *
 * $Id$
 */

#include <mysql/mysql.h>

/*
 * This structure represents a database connection
 * and pointer to this structure is used as a connection
 * handle
 */
typedef struct {
	char* table;          /* Default table to use */
	MYSQL*      con;      /* Mysql Connection */
	MYSQL_RES*  res;      /* Result of previous operation */
	MYSQL_ROW   row;      /* Actual row in the result */
	int         connected;
} db_con_t;

#define CON_CONNECTED(cn) ((cn)->connected)
#define CON_TABLE(cn) ((cn)->table)
#define CON_CONNECTION(cn) ((cn)->con)
#define CON_RESULT(cn) ((cn)->res)
#define CON_ROW(cn) ((cn)->row)

int use_table(db_con_t* _h, const char* _t);


#endif
