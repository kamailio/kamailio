#include "msg_cloner.h"

struct via_body* via_body_cloner( struct via_body *org_via);


struct sip_msg* sip_msg_cloner( struct sip_msg *org_msg )
{
   struct sip_msg   *new_msg;

   /* clones the sip_msg structure */
   new_msg = (struct sip_msg*)sh_malloc( sizeof( struct sip_msg) );
   memcpy( new_msg , org_msg , sizeof( struct sip_msg) );

   /* first_line (struct msg_start type) */
   if ( org_msg->first_line.type==SIP_REQUEST )
   {
      /* method (str type) */
      new_msg->first_line.u.request.method.s = (char*)sh_malloc( org_msg->first_line.u.request.method.len );
      memcpy( new_msg->first_line.u.request.method.s , org_msg->first_line.u.request.method.s , org_msg->first_line.u.request.method.len );
      /* uri (str type) */
      new_msg->first_line.u.request.uri.s = (char*)sh_malloc( org_msg->first_line.u.request.uri.len );
      memcpy( new_msg->first_line.u.request.uri.s , org_msg->first_line.u.request.uri.s , org_msg->first_line.u.request.uri.len );
      /* version (str type) */
      new_msg->first_line.u.request.version.s = (char*)sh_malloc( org_msg->first_line.u.request.version.len );
      memcpy( new_msg->first_line.u.request.version.s , org_msg->first_line.u.request.version.s , org_msg->first_line.u.request.version.len );
   }
   else if ( org_msg->first_line.type==SIP_REPLY )
   {
      /* version (str type) */
      new_msg->first_line.u.reply.version.s = (char*)sh_malloc( org_msg->first_line.u.reply.version.len );
      memcpy( new_msg->first_line.u.reply.version.s , org_msg->first_line.u.reply.version.s , org_msg->first_line.u.reply.version.len );
      /* status (str type) */
      new_msg->first_line.u.reply.status.s = (char*)sh_malloc( org_msg->first_line.u.reply.status.len );
      memcpy( new_msg->first_line.u.reply.status.s , org_msg->first_line.u.reply.status.s , org_msg->first_line.u.reply.status.len );
      /* reason (str type) */
      new_msg->first_line.u.reply.reason.s = (char*)sh_malloc( org_msg->first_line.u.reply.reason.len );
      memcpy( new_msg->first_line.u.reply.reason.s , org_msg->first_line.u.reply.reason.s , org_msg->first_line.u.reply.reason.len );
   }




}




struct via_body* via_body_cloner( struct via_body *org_via)
{
   struct via_body *new_via;

   /* clones the via_body structure */
   new_via = (struct via_body*)sh_malloc( sizeof( struct via_body) );
   memcpy( new_via , org_via , sizeof( struct via_body) );

   /* hdr (str type) */
   new_via->hdr.s = (char*)sh_malloc( org_via->hdr.len );
   memcpy( new_via->hdr.s , org_via->hdr.s , org_via->hdr.len );
   /* name (str type) */
   new_via->name.s = (char*)sh_malloc( org_via->name.len );
   memcpy( new_via->name.s , org_via->name.s , org_via->name.len );
   /* version (str type) */
   new_via->version.s = (char*)sh_malloc( org_via->version.len );
   memcpy( new_via->version.s , org_via->version.s , org_via->version.len );
   /* transport (str type) */
   new_via->transport.s = (char*)sh_malloc( org_via->transport.len );
   memcpy( new_via->transport.s , org_via->transport.s , org_via->transport.len );
   /* host (str type) */
   new_via->host.s = (char*)sh_malloc( org_via->host.len );
   memcpy( new_via->host.s , org_via->host.s , org_via->host.len );
   /* port_str (str type) */
   new_via->port_str.s = (char*)sh_malloc( org_via->port_str.len );
   memcpy( new_via->port_str.s , org_via->port_str.s , org_via->port_str.len );
   /* params (str type) */
   new_via->params.s = (char*)sh_malloc( org_via->params.len );
   memcpy( new_via->params.s , org_via->params.s , org_via->params.len );
   /* comment (str type) */
   new_via->comment.s = (char*)sh_malloc( org_via->comment.len );
   memcpy( new_via->comment.s , org_via->comment.s , org_via->comment.len );

   if ( new_via->next )
      new_via->next = via_body_cloner( org_via->next );
}










