/*
 * $Id$
 */


#include "sip_msg.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../ut.h"


#define ROUND4(s) (((s)%4)?((s)+4)/4*4:(s))

#define  lump_len( _lump)  (ROUND4(sizeof(struct lump)) + \
               ROUND4( ((_lump)->op==LUMP_ADD)?(_lump)->len:0 ))

#define  lump_clone( _new,_old,_ptr)    \
      {(_new) = (struct lump*)(_ptr);\
       memcpy( (_new), (_old), sizeof(struct lump) );\
       (_ptr)+=ROUND4(sizeof(struct lump));\
       if ( (_old)->op==LUMP_ADD) {\
          (_new)->u.value = (char*)(_ptr);\
          memcpy( (_new)->u.value , (_old)->u.value , (_old)->len);\
          (_ptr)+=ROUND4((_old)->len);}\
      }

struct via_body* via_body_cloner_2( char* new_buf , char *org_buf , struct via_body *org_via, char **p);

struct sip_msg*  sip_msg_cloner( struct sip_msg *org_msg )
{
   unsigned int         len;
   struct hdr_field    *hdr,*new_hdr,*last_hdr;
   struct via_body    *via;
   struct via_param *prm;
   struct to_param   *to_prm,*new_to_prm;
   struct sip_msg     *new_msg;
   struct lump          *lump_chain, *lump_tmp, **lump_anchor, **lump_anchor2;
   char                       *p,*foo;


   /*computing the length of entire sip_msg structure*/
   len = ROUND4(sizeof( struct sip_msg ));
   /*we will keep only the original msg*/
   len += ROUND4(org_msg->len);
   /*the new uri (if any)*/
   if (org_msg->new_uri.s && org_msg->new_uri.len)
      len+= ROUND4(org_msg->new_uri.len);
   /*all the headers*/
   for( hdr=org_msg->headers ; hdr ; hdr=hdr->next )
   {
      /*size of header struct*/
      len += ROUND4(sizeof( struct hdr_field));
      switch (hdr->type)
      {
         case HDR_CSEQ:
                   len+=ROUND4(sizeof(struct cseq_body));
                   break;
         case HDR_TO:
                   len+=ROUND4(sizeof(struct to_body));
                   /*to param*/
                   to_prm = ((struct to_body*)(hdr->parsed))->param_lst;
                   for(;to_prm;to_prm=to_prm->next)
                      len+=ROUND4(sizeof(struct to_param ));
                   break;
         case HDR_VIA:
                   for (via=(struct via_body*)hdr->parsed;via;via=via->next)
                   {
                      len+=ROUND4(sizeof(struct via_body));
                      /*via param*/
                      for(prm=via->param_lst;prm;prm=prm->next)
                         len+=ROUND4(sizeof(struct via_param ));
                   }
                   break;
      }
   }
   /* length of the data lump structures */
   if (org_msg->first_line.type==SIP_REQUEST)
      lump_chain = org_msg->add_rm;
   else
      lump_chain = org_msg->repl_add_rm;
   while (lump_chain)
   {
      len += lump_len( lump_chain );
      lump_tmp = lump_chain->before;
      while ( lump_tmp )
      {
         len += lump_len( lump_tmp );
         lump_tmp = lump_tmp->before;
      }
      lump_tmp = lump_chain->after;
      while ( lump_tmp )
      {
         len += lump_len( lump_tmp );
         lump_tmp = lump_tmp->after;
      }
      lump_chain = lump_chain->next;
   }

   p=(char *)sh_malloc(len);foo=p;
   if (!p)
   {
      LOG(L_ERR , "ERROR: sip_msg_cloner_2: cannot allocate memory\n" );
      return 0;
   }

   /*filling up the new structure*/
   new_msg = (struct sip_msg*)p;
   /*sip msg structure*/
   memcpy( new_msg , org_msg , sizeof(struct sip_msg) );
   p += ROUND4(sizeof(struct sip_msg));
   new_msg->add_rm = new_msg->repl_add_rm = 0;
   /*new_uri*/
   if (org_msg->new_uri.s && org_msg->new_uri.len)
   {
      new_msg->new_uri.s = p;
      memcpy( p , org_msg->new_uri.s , org_msg->new_uri.len);
      p += ROUND4(org_msg->new_uri.len);
   }
   /*message buffers(org and scratch pad)*/
   memcpy( p , org_msg->orig , org_msg->len);
   new_msg->orig = new_msg->buf = p;
   p += ROUND4(new_msg->len);
   /*unparsed and eoh pointer*/
   new_msg->unparsed = translate_pointer( new_msg->buf ,
	org_msg->buf , org_msg->unparsed );
   new_msg->eoh = translate_pointer( new_msg->buf ,
	org_msg->buf , org_msg->eoh );
   /* first line, updating the pointers*/
   if ( org_msg->first_line.type==SIP_REQUEST )
   {
      new_msg->first_line.u.request.method.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.request.method.s );
      new_msg->first_line.u.request.uri.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.request.uri.s );
      new_msg->first_line.u.request.version.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.request.version.s );
   }
   else if ( org_msg->first_line.type==SIP_REPLY )
   {
      new_msg->first_line.u.reply.version.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.reply.version.s );
      new_msg->first_line.u.reply.status.s =  translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.reply.status.s );
      new_msg->first_line.u.reply.reason.s =  translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.reply.reason.s );
    }

   /*headers list*/
   new_msg->via1=0;
   new_msg->via2=0;
   for( hdr=org_msg->headers,last_hdr=0 ; hdr ; hdr=hdr->next )
   {
      new_hdr = (struct hdr_field*)p;
      memcpy(new_hdr, hdr, sizeof(struct hdr_field) );
      p += ROUND4(sizeof( struct hdr_field));
      new_hdr->name.s =  translate_pointer( new_msg->buf , org_msg->buf , hdr->name.s );
      new_hdr->body.s =  translate_pointer( new_msg->buf , org_msg->buf , hdr->body.s );

     switch (hdr->type)
      {
         case HDR_VIA:
                if ( !new_msg->via1 )
                   {
                       new_msg->h_via1 = new_hdr;
                       new_msg->via1 = via_body_cloner_2( new_msg->buf , org_msg->buf , (struct via_body*)hdr->parsed , &p);
                       new_hdr->parsed  = (void*)new_msg->via1;
                   if ( new_msg->via1->next )
                            new_msg->via2 = new_msg->via1->next;
                   }
                else if ( !new_msg->via2 && new_msg->via1 )
                   {
                       new_msg->h_via2 = new_hdr;
                       if ( new_msg->via1->next )
                           new_hdr->parsed = (void*)new_msg->via1->next;
                       else{
                           new_msg->via2 = via_body_cloner_2( new_msg->buf , org_msg->buf , (struct via_body*)hdr->parsed , &p);
                           new_hdr->parsed  = (void*)new_msg->via2;
                       }
                   }
                else if ( new_msg->via2 && new_msg->via1 )
                   {
                       new_hdr->parsed  = new_msg->via1 = via_body_cloner_2( new_msg->buf , org_msg->buf , (struct via_body*)hdr->parsed , &p);
                   }
                   break;
         case HDR_CSEQ:
                   new_hdr->parsed = p;
                   p +=ROUND4(sizeof(struct cseq_body));
                   memcpy( new_hdr->parsed , hdr->parsed , sizeof(struct cseq_body) );
                   ((struct cseq_body*)new_hdr->parsed)->number.s =  translate_pointer( new_msg->buf , org_msg->buf , ((struct cseq_body*)hdr->parsed)->number.s );
                   ((struct cseq_body*)new_hdr->parsed)->method.s =  translate_pointer( new_msg->buf , org_msg->buf , ((struct cseq_body*)hdr->parsed)->method.s );
                   new_msg->cseq = new_hdr;
                   break;
         case HDR_TO:
                   new_hdr->parsed = p;
                   p +=ROUND4(sizeof(struct to_body));
                   memcpy( new_hdr->parsed , hdr->parsed ,
                    sizeof(struct to_body) );
                   ((struct to_body*)new_hdr->parsed)->body.s =
                        translate_pointer( new_msg->buf , org_msg->buf ,
                        ((struct to_body*)hdr->parsed)->body.s );
                   if ( ((struct to_body*)hdr->parsed)->tag_value.s )
                         ((struct to_body*)new_hdr->parsed)->tag_value.s
                         = translate_pointer( new_msg->buf , org_msg->buf ,
                            ((struct to_body*)hdr->parsed)->tag_value.s );
                    /*to params*/
                   to_prm = ((struct to_body*)(hdr->parsed))->param_lst;
                   for(;to_prm;to_prm=to_prm->next)
                   {
                      /*alloc*/
                      new_to_prm = (struct to_param*)p;
                      p +=ROUND4(sizeof(struct to_param ));
                      /*coping*/
                      memcpy( new_to_prm, to_prm, sizeof(struct to_param ));
                      ((struct to_body*)new_hdr->parsed)->param_lst = 0;
                      new_to_prm->name.s = translate_pointer( new_msg->buf,
                         org_msg->buf , to_prm->name.s );
                      new_to_prm->value.s = translate_pointer( new_msg->buf,
                         org_msg->buf , to_prm->value.s );
                      /*linking*/
                      if ( !((struct to_body*)new_hdr->parsed)->param_lst )
                         ((struct to_body*)new_hdr->parsed)->param_lst
                            = new_to_prm;
                      else
                         ((struct to_body*)new_hdr->parsed)->last_param->next
                            = new_to_prm;
                      ((struct to_body*)new_hdr->parsed)->last_param
                         = new_to_prm;
                   }
                   new_msg->to = new_hdr;
                   break;
         case HDR_CALLID:
                   new_msg->callid = new_hdr;
                   break;
         case HDR_FROM:
                   new_msg->from = new_hdr;
                   break;
         case HDR_CONTACT:
                   new_msg->contact = new_hdr;
                   break;
         case HDR_MAXFORWARDS :
                  new_msg->maxforwards = new_hdr;
               break;
        case HDR_ROUTE :
                  new_msg->route = new_hdr;
               break;
      }

     if ( last_hdr )
      {
          last_hdr->next = new_hdr;
          last_hdr=last_hdr->next;
      }
      else
      {
           last_hdr=new_hdr;
           new_msg->headers =new_hdr;
      }
      last_hdr->next = 0;
      new_msg->last_header = last_hdr;

   }

   /* clonning data lump*/
   if (org_msg->first_line.type==SIP_REQUEST) {
      lump_chain = org_msg->add_rm;
      lump_anchor = &(new_msg->add_rm);
   }else{
      lump_chain = org_msg->repl_add_rm;
      lump_anchor = &(new_msg->repl_add_rm);
   }
   while (lump_chain)
   {
      lump_clone( (*lump_anchor) , lump_chain , p );
      /*before list*/
      lump_tmp = lump_chain->before;
      lump_anchor2 = &((*lump_anchor)->before);
      while ( lump_tmp )
      {
         lump_clone( (*lump_anchor2) , lump_tmp , p );
         lump_anchor2 = &((*lump_anchor2)->before);
         lump_tmp = lump_tmp->before;
      }
      /*after list*/
      lump_tmp = lump_chain->after;
      lump_anchor2 = &((*lump_anchor)->after);
      while ( lump_tmp )
      {
         lump_clone( (*lump_anchor2) , lump_tmp , p );
         lump_anchor2 = &((*lump_anchor2)->after);
         lump_tmp = lump_tmp->after;
      }
      lump_anchor = &((*lump_anchor)->next);
      lump_chain = lump_chain->next;
   }
   DBG("-----------> len=%d <---> written=%d\n",len,p-foo);
   return new_msg;
}



struct via_body* via_body_cloner_2( char* new_buf , char *org_buf , struct via_body *org_via, char **p)
{
    struct via_body *new_via;

    /* clones the via_body structure */
    new_via = (struct via_body*)(*p);
    memcpy( new_via , org_via , sizeof( struct via_body) );
    (*p) += ROUND4(sizeof( struct via_body ));

    /* hdr (str type) */
    new_via->hdr.s = translate_pointer( new_buf , org_buf , org_via->hdr.s );
    /* name (str type) */
    new_via->name.s = translate_pointer( new_buf , org_buf , org_via->name.s );
    /* version (str type) */
    new_via->version.s = translate_pointer( new_buf , org_buf , org_via->version.s );
    /* transport (str type) */
    new_via->transport.s = translate_pointer( new_buf , org_buf , org_via->transport.s );
    /* host (str type) */
    new_via->host.s = translate_pointer( new_buf , org_buf , org_via->host.s );
    /* port_str (str type) */
    new_via->port_str.s = translate_pointer( new_buf , org_buf , org_via->port_str.s );
    /* params (str type) */
    new_via->params.s = translate_pointer( new_buf , org_buf , org_via->params.s );
    /* comment (str type) */
    new_via->comment.s = translate_pointer( new_buf , org_buf , org_via->comment.s );


    if ( org_via->param_lst )
    {
       struct via_param *vp, *new_vp, *last_new_vp;
       for( vp=org_via->param_lst, last_new_vp=0 ; vp ; vp=vp->next )
       {
           new_vp = (struct via_param*)(*p);
           memcpy( new_vp , vp , sizeof(struct via_param));
           (*p) += ROUND4(sizeof(struct via_param));
           new_vp->name.s = translate_pointer( new_buf , org_buf , vp->name.s );
           new_vp->value.s = translate_pointer( new_buf , org_buf , vp->value.s );

           if (new_vp->type==PARAM_BRANCH)
              new_via->branch = new_vp;

           if (last_new_vp)
              last_new_vp->next = new_vp;
           else
              new_via->param_lst = new_vp;

           last_new_vp = new_vp;
           last_new_vp->next = NULL;
       }
       new_via->last_param = new_vp;
    }


   if ( org_via->next )
       new_via->next = via_body_cloner_2( new_buf , org_buf , org_via->next , p );

   return new_via;
}

