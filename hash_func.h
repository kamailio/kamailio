/*
 * $Id$
 */


#ifndef _HASH_H
#define _HASH_H

#include "str.h"

/* always use a power of 2 for hash table size */
#define T_TABLE_POWER    10
#define TABLE_ENTRIES    (1 << (T_TABLE_POWER))

int new_hash( str  call_id, str cseq_nr );
int old_hash( str  call_id, str cseq_nr );

int init_hash();

#define hash( cid, cseq) new_hash( cid, cseq )

#endif
