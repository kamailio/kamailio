#include "hash_func.h"

int hash( char* call_id , char* cseq_nr )
{
   int     hash_code = 0;
   char* p;

    if ( call_id )
      for( p=call_id ; (*p)!=0 ; hash_code+=(*p) , p++ );
    if ( cseq_nr )
      for( p=cseq_nr ; *p!=0 ; hash_code+=*p , p++ );

   hash_code %= table_entries;
}

