/*
 * $Id$
 */


#include "sip_msg.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../ut.h"


#define ROUND4(s) ((s)%4)?((s)+4)/4*4:(s)

struct via_body* via_body_cloner( char* new_buf , char *org_buf , struct via_body *org_via);
struct hdr_field* header_cloner( struct sip_msg *new_msg , struct sip_msg *org_msg, struct hdr_field *hdr);

struct via_body* via_body_cloner_2( char* new_buf , char *org_buf , struct via_body *org_via, char **p);


struct sip_msg* sip_msg_cloner_1( struct sip_msg *org_msg )
{
    struct sip_msg   *new_msg=0;
    struct hdr_field  *header, *last_hdr, *new_hdr;

    /* clones the sip_msg structure */
    new_msg = (struct sip_msg*)sh_malloc( sizeof( struct sip_msg) );
    if (!new_msg) {
	DBG("DEBUG: sip_msg_cloner: sip_msg allocation failed\n");
	return NULL;
    }
    memcpy( new_msg , org_msg , sizeof( struct sip_msg) );

    /* if something bad happens during the allocation process,
       don't try to release pointers which were cloned
    */
    new_msg->new_uri.s=NULL;
    new_msg->headers=NULL;
    new_msg->add_rm=NULL;
    new_msg->repl_add_rm=NULL;
    new_msg->orig=NULL;
    new_msg->buf=NULL;

    /* the original message - orig ( char*  type) */
    new_msg->orig = (char*)sh_malloc( new_msg->len+1 );
    if (!new_msg->orig) {
	DBG("DEBUG: sip_msg_cloner: new_msg->orig allocation failed\n");
	goto error;
    }
    memcpy( new_msg->orig , org_msg->orig, new_msg->len );
    new_msg->orig[ new_msg->len ] = 0;

    /* the scratch pad - buf ( char* type) */
    new_msg->buf = (char*)sh_malloc( new_msg->len+1 );
    if (!new_msg->buf) {
	DBG("DEBUG: sip_msg_cloner:  new_msg->buf allocation failed\n");
	goto error;
    }
    memcpy( new_msg->buf , org_msg->buf, new_msg->len );
    new_msg->buf[ new_msg->len ] = 0;

    /* where the parse stopped - unparsed (char* type)*/
    new_msg->unparsed = translate_pointer( new_msg->buf , org_msg->buf , org_msg->unparsed );

    /* end of header - eoh (char* type)*/
    new_msg->eoh = translate_pointer( new_msg->buf , org_msg->buf , org_msg->eoh );

    /* first_line (struct msg_start type) */
    if ( org_msg->first_line.type==SIP_REQUEST )
    {
	/* method (str type) */
	new_msg->first_line.u.request.method.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.request.method.s );
	/* uri (str type) */
	new_msg->first_line.u.request.uri.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.request.uri.s );
	/* version (str type) */
	new_msg->first_line.u.request.version.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.request.version.s );
    }
    else if ( org_msg->first_line.type==SIP_REPLY )
    {
	/* version (str type) */
	new_msg->first_line.u.reply.version.s = translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.reply.version.s );
	/* status (str type) */
	new_msg->first_line.u.reply.status.s =  translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.reply.status.s );
	/* reason (str type) */
	new_msg->first_line.u.reply.reason.s =  translate_pointer( new_msg->buf , org_msg->buf , org_msg->first_line.u.reply.reason.s );
    }

    /* new_uri  ( str type )*/
	if (org_msg->new_uri.s){
		if (!(new_msg->new_uri.s = (char*)sh_malloc( org_msg->new_uri.len )))
		{
			DBG("DEBUG: sip_msg_cloner: new_msg->new_uri.s allocation failed\n");
			goto error;
		}
		memcpy( new_msg->new_uri.s , org_msg->new_uri.s ,
				org_msg->new_uri.len );
	}

    /* add_rm ( struct lump* )  -> have to be changed!!!!!!! */
    new_msg->add_rm  = 0;
    /* repl_add_rm ( struct lump* ) -> have to be changed!!!!!!!  */
    new_msg->repl_add_rm  = 0;

    /* all the headers */
    new_msg->via1=0;
    new_msg->via2=0;
    for( header = org_msg->headers , last_hdr=0  ;  header ; header=header->next)
    {
	new_hdr = header_cloner( new_msg , org_msg , header );
	if (!new_hdr) {
		DBG("DEBUG: sip_msg_cloner: new_hdr allocation failed\n");
		goto error;
	}
	switch ( header->type )
	{
	    case HDR_VIA :
		if ( !new_msg->via1 )
		{
		    new_msg->h_via1 = new_hdr;
		    new_msg->via1 = via_body_cloner( new_msg->buf , org_msg->buf , (struct via_body*)header->parsed );
		    if (!new_msg->via1) {
			DBG("DEBUG: sip_msg_cloner:  new_msg->via1 allocation failed\n");
			goto hf_error;
		    }

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
		        new_msg->via2 = via_body_cloner( new_msg->buf , org_msg->buf , (struct via_body*)header->parsed );
			if (!new_msg->via2) {
				DBG("DEBUG: sip_msg_cloner:  new_msg->via2 allocation failed\n");
				goto hf_error;
			}
		        new_hdr->parsed  = (void*)new_msg->via2;
		     }
		}
		else if ( new_msg->via2 && new_msg->via1 )
		{
		    new_hdr->parsed  = new_msg->via1 = via_body_cloner( new_msg->buf , org_msg->buf , (struct via_body*)header->parsed );
		    if (!new_hdr->parsed) {
				DBG("DEBUG: sip_msg_cloner:  new_hdr->parsed  via1 allocation failed\n");
				goto hf_error;
		    }
		}
		break;
	    case HDR_FROM :
		new_msg->from = new_hdr;
		break;
	    case HDR_TO :
		new_msg->to = new_hdr;
		break;
	    case HDR_CSEQ :
		if (header->parsed)
		{
		  new_hdr->parsed = (void*)sh_malloc( sizeof(struct cseq_body) );
		  if (!new_hdr->parsed) {
			DBG("DEBUG: sip_msg_cloner:  new_hdr->parsed  cseq allocation failed\n");
			goto hf_error;
		  }
		  memcpy( new_hdr->parsed , header->parsed , sizeof(struct cseq_body) );
		  ((struct cseq_body*)new_hdr->parsed)->number.s = translate_pointer( new_msg->buf , org_msg->buf , ((struct cseq_body*)header->parsed)->number.s );
		  ((struct cseq_body*)new_hdr->parsed)->method.s = translate_pointer( new_msg->buf , org_msg->buf , ((struct cseq_body*)header->parsed)->method.s );
		}
		new_msg->cseq = new_hdr;
		break;
	    case HDR_CALLID :
		new_msg->callid = new_hdr;
		break;
	    case HDR_CONTACT :
		new_msg->contact = new_hdr;
		break;
	    case HDR_MAXFORWARDS :
		new_msg->maxforwards = new_hdr;
		break;
	    case HDR_ROUTE :
		new_msg->route = new_hdr;
		break;
	    default :
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

    return new_msg;

hf_error:
	sh_free( new_hdr );
error:
	sip_msg_free( new_msg );
	/* sh_free( new_msg ); */
	return 0;

}




struct via_body* via_body_cloner( char* new_buf , char *org_buf , struct via_body *org_via)
{
    struct via_body *new_via;

    /* clones the via_body structure */
    new_via = (struct via_body*)sh_malloc( sizeof( struct via_body) );
    if (!new_via) return NULL;
    memcpy( new_via , org_via , sizeof( struct via_body) );

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

    if ( org_via->next ) {
        new_via->next = via_body_cloner( new_buf , org_buf , org_via->next );
	if (!new_via->next)
		goto error;
    }

    new_via->param_lst = NULL;
    if ( org_via->param_lst )
    {
       struct via_param *vp, *new_vp, *last_new_vp, *delete_i, *dummy;
       for( vp=org_via->param_lst, last_new_vp=0 ; vp ; vp=vp->next )
       {
          new_vp = (struct via_param*)sh_malloc(sizeof(struct via_param));
	  if (!new_vp) {
		for (delete_i=new_via->param_lst; delete_i;  ) {
			dummy=delete_i->next;
			sh_free( delete_i );
			delete_i = dummy;
		}
		goto error;
	  }
          memcpy( new_vp , vp , sizeof(struct via_param));
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



   return new_via;

error:
	sh_free(new_via);
	return NULL;
}




struct hdr_field* header_cloner( struct sip_msg *new_msg , struct sip_msg *org_msg, struct hdr_field *org_hdr)
{
    struct hdr_field* new_hdr;

    new_hdr = (struct hdr_field*)sh_malloc( sizeof(struct hdr_field) );
    if (!new_hdr) return NULL;
    memcpy( new_hdr , org_hdr , sizeof(struct hdr_field) );

    /* name */
    new_hdr->name.s =  translate_pointer( new_msg->buf , org_msg->buf , org_hdr->name.s );
    /* body */
    new_hdr->body.s =  translate_pointer( new_msg->buf , org_msg->buf , org_hdr->body.s );

    return new_hdr;
}


 /*
char*   translate_pointer( char* new_buf , char *org_buf , char* p)
{
    if (!p)
	return 0;
    else
	return new_buf + (p-org_buf);
}
*/



/* Frees the memory occupied by a SIP message
  */
void sh_free_lump(struct lump* lmp)
{
	if (lmp && (lmp->op==LUMP_ADD)){
		if (lmp->u.value) sh_free(lmp->u.value);
		lmp->u.value=0;
		lmp->len=0;
	}
}



void sh_free_lump_list(struct lump* l)
{
	struct lump* t, *r, *foo,*crt;
	t=l;
	while(t){
		crt=t;
		t=t->next;
	/*
		 dangerous recursive clean
		if (crt->before) free_lump_list(crt->before);
		if (crt->after)  free_lump_list(crt->after);
	*/
		/* no more recursion, clean after and before and that's it */
		r=crt->before;
		while(r){
			foo=r; r=r->before;
			sh_free_lump(foo);
			sh_free(foo);
		}
		r=crt->after;
		while(r){
			foo=r; r=r->after;
			sh_free_lump(foo);
			sh_free(foo);
		}

		/*clean current elem*/
		sh_free_lump(crt);
		sh_free(crt);
	}
}



void sh_free_uri(struct sip_uri* u)
{
   if (u)
   {
     if (u->user.s)
         sh_free(u->user.s);
     if (u->passwd.s)
         sh_free(u->passwd.s);
     if (u->host.s)
         sh_free(u->host.s);
     if (u->port.s)
         sh_free(u->port.s);
     if (u->params.s)
         sh_free(u->params.s);
     if (u->headers.s)
         sh_free(u->headers.s);
   }
}



void sh_free_via_param_list(struct via_param* vp)
{
   struct via_param* foo;
   while(vp)
    {
       foo=vp;
       vp=vp->next;
       sh_free(foo);
    }
}



void sh_free_via_list(struct via_body* vb)
{
   struct via_body* foo;
   while(vb)
    {
      foo=vb;
      vb=vb->next;
     if (foo->param_lst)
        sh_free_via_param_list(foo->param_lst);
      sh_free(foo);
    }
}


/* frees a hdr_field structure,
 * WARNING: it frees only parsed (and not name.s, body.s)*/
void sh_clean_hdr_field(struct hdr_field* hf)
{
   if (hf->parsed)
   {
      switch(hf->type)
      {
         case HDR_VIA:
               sh_free_via_list(hf->parsed);
             break;
         case HDR_CSEQ:
                sh_free(hf->parsed);
             break;
         default:
      }
   }
}



/* frees a hdr_field list,
 * WARNING: frees only ->parsed and ->next*/
void sh_free_hdr_field_lst(struct hdr_field* hf)
{
   struct hdr_field* foo;

   while(hf)
    {
       foo=hf;
       hf=hf->next;
       sh_clean_hdr_field(foo);
       sh_free(foo);
    }
}



void sip_msg_free_1(struct sip_msg* msg)
{
   if (!msg) return;

   DBG("DEBUG: sip_msg_free : start\n");

   if (msg->new_uri.s)
   {
      sh_free(msg->new_uri.s);
      msg->new_uri.len=0;
   }
   if (msg->headers)
      sh_free_hdr_field_lst(msg->headers);
   if (msg->add_rm)
      sh_free_lump_list(msg->add_rm);
   if (msg->repl_add_rm)
      sh_free_lump_list(msg->repl_add_rm);
   if (msg->orig) sh_free( msg->orig );
   if (msg->buf) sh_free( msg->buf );

   sh_free(msg);
   DBG("DEBUG: sip_msg_free : done\n");
}


/******************************* New fuctions ************************************/

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

struct sip_msg*  sip_msg_cloner_2( struct sip_msg *org_msg )
{
   unsigned int         len;
   struct hdr_field    *hdr,*new_hdr,*last_hdr;
   struct via_body    *via;
   struct via_param *prm;
   struct sip_msg     *new_msg;
   struct lump          *lump_chain, *lump_tmp, **lump_anchor, **lump_anchor2;
   char                       *p;


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
   /* length of the data lump structures
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
   }  */

   p=(char *)sh_malloc(len);
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
         case HDR_CALLID:
                   new_msg->callid = new_hdr;
                   break;
         case HDR_TO:
                   new_msg->to = new_hdr;
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

   /* clonning data lump
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
      /*before list/
      lump_tmp = lump_chain->before;
      lump_anchor2 = &((*lump_anchor)->before);
      while ( lump_tmp )
      {
         lump_clone( (*lump_anchor2) , lump_tmp , p );
         lump_anchor2 = &((*lump_anchor2)->before);
         lump_tmp = lump_tmp->before;
      }
      /*after list/
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
   }           */

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

