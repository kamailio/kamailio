#ifndef _TM_H
#define _TM_H

#include <stdio.h>
#include <stdlib.h>

#include "h_table.h"

int request_received(  struct s_table* hash_table, char* incoming_req_uri, char *from, char* to, char* tag, 
	char* call_id, char* cseq_nr ,char* cseq_method, char* outgoing_req_uri);

int response_received( struct s_table* hash_table, char* reply_code, char* incoming_url , char* via, char* label , 
		char* from , char* to , char* tag , char* call_id , char* cseq_nr ,char* cseq_method );

#endif
