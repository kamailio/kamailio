#ifndef _CPL_DB_H
#define _CPL_DB_H

int write_to_db(char *usr, char *bin_s, int bin_len, char *xml_s, int xml_len);

extern char* DB_URL;
extern char* DB_TABLE;

#endif
