#ifndef __DBCON_MYSQL_H__
#define __DBCON_MYSQL_H__

#include "location.h"
#include "contact.h"

int  db_init         (const char* _sqlurl);
void db_close        (void); 
int  query_location  (const char* _tab, const char* _aor, location_t** _res);
int  insert_location (const char* _tab, const location_t* _loc);
int  insert_contact  (const char* _tab, const contact_t* _con);
int  delete_location (const char* _tab, const location_t* _loc);
int  update_location (const char* _tab, const location_t* _loc);
int  update_contact  (const char* _tab, const contact_t* _con);
int parse_sql_url(char* _url, char** _user, char** _pass,
		  char** _host, char** _port, char** _db);


#define SQL_BUF_LEN 1024


#endif
