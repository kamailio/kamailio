/*
 * $Id$
 */


#ifndef _SIP_MSG_H
#define _SIP_MSG_H

#include "../../msg_parser.h"

#include "sh_malloc.h"

#define sip_msg_cloner(p_msg) \
    sip_msg_cloner_1(p_msg)

#define sip_msg_free(p_msg) \
    sip_msg_free_1(p_msg)


struct sip_msg*  sip_msg_cloner_1( struct sip_msg *org_msg );
struct sip_msg*  sip_msg_cloner_2( struct sip_msg *org_msg );
void                     sip_msg_free_1( struct sip_msg *org_msg );
void                     sip_msg_free_2( struct sip_msg *org_msg );

char*   translate_pointer( char* new_buf , char *org_buf , char* p);
struct via_body* via_body_cloner( char* new_buf , char *org_buf , struct via_body *org_via);
struct hdr_field* header_cloner( struct sip_msg *new_msg , struct sip_msg *org_msg, struct hdr_field *hdr);

#endif
