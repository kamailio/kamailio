#include "sip_msg.h"
#include "../../dprint.h"
#include "../../mem.h"



struct sip_msg* sip_msg_cloner( struct sip_msg *org_msg )
{
    struct sip_msg   *new_msg=0;
    struct hdr_field  *header, *last_hdr, *new_hdr;

    /* clones the sip_msg structure */
    new_msg = (struct sip_msg*)sh_malloc( sizeof( struct sip_msg) );
    memcpy( new_msg , org_msg , sizeof( struct sip_msg) );

    /* the original message - orig ( char*  type) */
    new_msg->orig = (char*)sh_malloc( new_msg->len+1 );
    memcpy( new_msg->orig , org_msg->orig, new_msg->len );
    new_msg->orig[ new_msg->len ] = 0;

    /* the scratch pad - buf ( char* type) */
    new_msg->buf = (char*)sh_malloc( new_msg->len+1 );
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

    /* all the headers */
    new_msg->via1=0;
    new_msg->via2=0;
    for( header = org_msg->headers , last_hdr=0  ;  header ; header=header->next)
    {
	switch ( header->type )
	{
	    case HDR_VIA :
		new_hdr = header_cloner( new_msg , org_msg , header );
		if ( !new_msg->via1 )
		{
		    new_msg->h_via1 = new_hdr;
		    new_msg->via1 = via_body_cloner( new_msg->buf , org_msg->buf , (struct via_body*)header->parsed );
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
		        new_hdr->parsed  = (void*)new_msg->via2;
		     }
		}
		else if ( new_msg->via2 && new_msg->via1 )
		{
		    new_hdr->parsed  = new_msg->via1 = via_body_cloner( new_msg->buf , org_msg->buf , (struct via_body*)header->parsed );
		}
		break;
	    case HDR_FROM :
		new_hdr = header_cloner( new_msg , org_msg , header );
		new_msg->from = new_hdr;
		break;
	    case HDR_CSEQ :
		new_hdr = header_cloner( new_msg , org_msg , header );
		if (header->parsed)
		{
		  new_hdr->parsed = (void*)sh_malloc( sizeof(struct cseq_body) );
		  memcpy( new_hdr->parsed , header->parsed , sizeof(struct cseq_body) );
		  ((struct cseq_body*)new_hdr->parsed)->number.s = translate_pointer( new_msg->buf , org_msg->buf , ((struct cseq_body*)header->parsed)->number.s );
		  ((struct cseq_body*)new_hdr->parsed)->method.s = translate_pointer( new_msg->buf , org_msg->buf , ((struct cseq_body*)header->parsed)->method.s );
		}
		new_msg->cseq = new_hdr;
		break;
	    case HDR_CALLID :
		new_hdr = header_cloner( new_msg , org_msg , header );
		new_msg->callid = new_hdr;
		break;
	    case HDR_CONTACT :
		new_hdr = header_cloner( new_msg , org_msg , header );
		new_msg->contact = new_hdr;
		break;
	    default :
		new_hdr = header_cloner( new_msg , org_msg , header );
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
    }

    last_hdr->next = 0;
    new_msg->last_header = last_hdr;

    /* new_uri  ( str type )*/
    new_msg->new_uri.s = (char*)sh_malloc( org_msg->new_uri.len );
    memcpy( new_msg->new_uri.s , org_msg->new_uri.s , org_msg->new_uri.len );

    /* add_rm ( struct lump* )  -> have to be changed!!!!!!! */
    new_msg->add_rm  = 0;
    /* repl_add_rm ( struct lump* ) -> have to be changed!!!!!!!  */
    new_msg->repl_add_rm  = 0;

    return new_msg;

error:
	sip_msg_free( new_msg );
	sh_free( new_msg );
	return 0;

}




struct via_body* via_body_cloner( char* new_buf , char *org_buf , struct via_body *org_via)
{
    struct via_body *new_via;

    /* clones the via_body structure */
    new_via = (struct via_body*)sh_malloc( sizeof( struct via_body) );
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

    if ( new_via->param_lst )
    {
       struct via_param *vp, *new_vp, *last_new_vp;
       for( vp=org_via->param_lst, last_new_vp=0 ; vp ; vp=vp->next )
       {
          new_vp = (struct via_param*)sh_malloc(sizeof(struct via_param));
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
       }
       new_via->last_param = new_vp;
    }


    if ( org_via->next )
        new_via->next = via_body_cloner( new_buf , org_buf , org_via->next );

   return new_via;
}




struct hdr_field* header_cloner( struct sip_msg *new_msg , struct sip_msg *org_msg, struct hdr_field *org_hdr)
{
    struct hdr_field* new_hdr;

    new_hdr = (struct hdr_field*)sh_malloc( sizeof(struct hdr_field) );
    memcpy( new_hdr , org_hdr , sizeof(struct hdr_field) );

    /* name */
    new_hdr->name.s =  translate_pointer( new_msg->buf , org_msg->buf , org_hdr->name.s );
    /* body */
    new_hdr->body.s =  translate_pointer( new_msg->buf , org_msg->buf , org_hdr->body.s );

    return new_hdr;
}



char*   translate_pointer( char* new_buf , char *org_buf , char* p)
{
    if (!p)
	return 0;
    else
	return new_buf + (p-org_buf);
}




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



/*only the content*/
void sip_msg_free(struct sip_msg* msg)
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

   DBG("DEBUG: sip_msg_free : done\n");
}
