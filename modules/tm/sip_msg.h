#ifndef _SIP_MSG_H
#define _SIP_MSG_H

#include "../../msg_parser.h"

#include "sh_malloc.h"


struct sip_msg*  sip_msg_cloner( struct sip_msg *org_msg );
void                     sip_msg_free( struct sip_msg *org_msg );

char*   translate_pointer( char* new_buf , char *org_buf , char* p);
struct via_body* via_body_cloner( char* new_buf , char *org_buf , struct via_body *org_via);
struct hdr_field* header_cloner( struct sip_msg *new_msg , struct sip_msg *org_msg, struct hdr_field *hdr);

#endif
