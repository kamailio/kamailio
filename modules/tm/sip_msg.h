#ifndef _SIP_MSG_H
#define _SIP_MSG_H

#include "../../msg_parser.h"

#define sh_malloc( size )  malloc(size)
#define sh_free( ptr )        free(ptr)


struct sip_msg*  sip_msg_cloner( struct sip_msg *org_msg );
void                     sip_msg_free( struct sip_msg *org_msg );

#endif
