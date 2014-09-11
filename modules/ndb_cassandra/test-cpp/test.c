#include <stdio.h>
#include <stdlib.h>

#include "thrift_wrapper.h"

#define host "127.0.0.1"
#define port 9160

int main()
{  
  char *keyspace = NULL;
  char *value_in = NULL;  
  char *value_out = NULL;  
  char *column_family = NULL;
  char *key = NULL;
  char *column = NULL;

  /* Setting the values. */
  keyspace = "indigital";
  value_in = "Luis Martin Gil";
  column_family = "employees";
  key = "lmartin";
  column = "name_ext";

  /* Doing the insert. */
  printf("Insert.   %s['%s']['%s'] <== '%s' ", column_family, key, column, value_in);
  if (insert_wrap(host, port, keyspace, column_family, key, column, &value_in) > 0) {printf("SUCCESS\n");}
  else {printf("FAILURE\n");}

  printf ("-----------------\n");

  /* Doing the retrieve. */
  printf("Retrieve. %s['%s']['%s'] ==> ", column_family, key, column);
  if (retrieve_wrap(host, port, keyspace, column_family, key, column, &value_out) > 0) {
    if (value_out) {
      printf("'%s' SUCCESS\n", value_out);
      free(value_out);
    }
  } else {printf("FAILURE\n");} 

  return 1;
}
