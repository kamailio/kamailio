/*
 * $Id$
 */


#ifndef _HASH_H
#define _HASH_H

#include "../../str.h"
#include "h_table.h"

int new_hash( str  call_id, str cseq_nr );
int old_hash( str  call_id, str cseq_nr );

#define hash( cid, cseq) new_hash( cid, cseq )

#endif
