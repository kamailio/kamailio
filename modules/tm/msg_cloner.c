#include "msg_cloner.h"

struct sip_msg* sip_msg_cloner( struct sip_msg *org_msg )
{
   struct sip_msg   *new_msg;

   /* clones the sip_msg structure */
   new_msg = sh_malloc( sizeof( struct sip_msg) );
   memcpy( new_msg , org_msg , sizeof( struct sip_msg) );


}



