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
	const char* table;    /* Default table to use */
	MYSQL       con;      /* Mysql Connection */
	MYSQL_RES*  res;      /* Result of previous operation */
	MYSQL_ROW   row;      /* Actual row in the result */
	int         connected;
} db_con_t;


#define CONNECTED(con) do {                                  \
                            return (con->connected == TRUE); \
                          } while(0)


int use_table(db_con_t* _h, const char* _t);


#endif
