#ifndef CASS_H
#define CASS_H

/* Facing the C code. Begin. */
#ifdef __cplusplus
extern "C" {
#endif

int insert_wrap(char* host, int port, char* keyspace, char* column_family, char* key, char* column, char** value);
int retrieve_wrap(char* host, int port, char* keyspace, char* column_family, char* key, char* column, char** value);

  /* Facing the C code. End. */
#ifdef __cplusplus
}
#endif


#endif
