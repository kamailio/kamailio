#include "sip_msg.h"
#include "../../dprint.h"

char*   translate_pointer( char* new_buf , char *org_buf , char* p);
struct via_body* via_body_cloner( char* new_buf , char *org_buf , struct via_body *org_via);

struct hdr_field* header_cloner( struct sip_msg *new_msg , struct sip_msg *org_msg, struct hdr_field *hdr);




struct sip_msg* sip_msg_cloner( struct sip_msg *org_msg )
{
    struct sip_msg   *new_msg;
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

    /* via1 (via_body* type) */
    if (org_msg->via1)
	 new_msg->via1 = via_body_cloner( new_msg->buf , org_msg->buf , org_msg->via1 );

    /* via2 (via_body* type) */
    if (org_msg->via2)
	new_msg->via2 = via_body_cloner( new_msg->buf , org_msg->buf , org_msg->via2 );

    /* all the headers */
    new_msg->h_via1=0;
    new_msg->h_via2=0;
    for( header = org_msg->headers , last_hdr=0  ;  header;header=header->next)
    {
	switch ( header->type )
	{
	    case HDR_VIA1 :
		new_hdr = header_cloner( new_msg , org_msg , header );
		new_hdr->parsed  = (void*)new_msg->via1;
		if (new_msg->h_via1==0)
		    new_msg->h_via1 = new_hdr;
		else if(new_msg->h_via2==0)
		    new_msg->h_via2 = new_hdr;
		break;
	    case HDR_CALLID :
		new_hdr = header_cloner( new_msg , org_msg , header );
		new_msg->callid = new_hdr;
		break;
	    case HDR_TO :
		new_hdr = header_cloner( new_msg , org_msg , header );
		new_msg->to = new_hdr;
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
		}
		new_msg->cseq = new_hdr;
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

    if ( new_via->next )
	new_via->next = via_body_cloner( new_buf , org_buf , org_via->next );
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





