#ifndef _MSG_CLONER_H
#define _MSG_CLONER_H

#include "../../msg_parser.h"

#define sh_malloc( size )  malloc(size)
#define sh_free( ptr )        free(ptr)


struct sip_msg* sip_msg_cloner( struct sip_msg *org_msg );

#endif
