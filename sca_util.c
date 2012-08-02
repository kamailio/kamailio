#include "sca_common.h"

#include <assert.h>

#include "sca_util.h"

    int
sca_get_msg_contact_uri( sip_msg_t *msg, str *contact_uri )
{
    contact_body_t	*contact_body;

    assert( msg != NULL );
    assert( contact_uri != NULL );

    if ( SCA_HEADER_EMPTY( msg->contact )) {
	LM_ERR( "Empty Contact header" );
	return( -1 );
    }
    if ( parse_contact( msg->contact ) < 0 ) {
	LM_ERR( "Failed to parse Contact header: %.*s",
		STR_FMT( &msg->contact->body ));
	return( -1 );
    }
    if (( contact_body = (contact_body_t *)msg->contact->parsed ) == NULL ) {
	LM_ERR( "Invalid Contact header: %.*s", STR_FMT( &msg->contact->body ));
	return( -1 );
    }
    if ( contact_body->star ) {
	LM_ERR( "Invalid Contact header: SCA Contact must not be \"*\"" );
	return( -1 );
    }
    if ( contact_body->contacts == NULL ) {
	LM_ERR( "Invalid Contact header: parser found no contacts" );
	return( -1 );
    }
    if ( contact_body->contacts->next ) {
	LM_ERR( "Invalid Contact header: Contact may only contain one URI" );
	return( -1 );
    }

    contact_uri->s = contact_body->contacts->uri.s;
    contact_uri->len = contact_body->contacts->uri.len;

    return( 0 );
}

    int
sca_get_msg_cseq_number( sip_msg_t *msg )
{
    int		cseq;

    assert( msg != NULL );

    if ( SCA_HEADER_EMPTY( msg->cseq )) {
	LM_ERR( "Empty Cseq header" );
	return( -1 );
    }
    if ( str2int( &(get_cseq( msg )->number), (unsigned int *)&cseq ) != 0 ) {
	LM_ERR( "Bad Cseq header: %.*s", STR_FMT( &msg->cseq->body ));
	return( -1 );
    }

    return( cseq );
}

/* assumes cseq header in msg is already parsed */
    int
sca_get_msg_cseq_method( sip_msg_t *msg )
{
    int		method;

    assert( msg != NULL );

    if ( SCA_HEADER_EMPTY( msg->cseq )) {
	LM_ERR( "Empty Cseq header" );
	return( -1 );
    }

    return( get_cseq( msg )->method_id );
}


    int
sca_get_msg_from_header( sip_msg_t *msg, struct to_body **from )
{
    struct to_body	*f;

    assert( msg != NULL );
    assert( from != NULL );

    if ( SCA_HEADER_EMPTY( msg->from )) {
	LM_ERR( "Empty From header" );
	return( -1 );
    }
    if ( parse_from_header( msg ) < 0 ) {
	LM_ERR( "Bad From header" );
	return( -1 );
    }
    f = get_from( msg );
    if ( SCA_STR_EMPTY( &f->tag_value )) {
	LM_ERR( "Bad From header: no tag parameter" );
	return( -1 );
    }

    *from = f;

    return( 0 );
}

    int
sca_get_msg_to_header( sip_msg_t *msg, struct to_body **to )
{
    struct to_body	parsed_to;
    struct to_body	*t = NULL;

    assert( msg != NULL );
    assert( to != NULL );

    if ( SCA_HEADER_EMPTY( msg->to )) {
	LM_ERR( "Empty To header" );
	return( -1 );
    }
    t = get_to( msg );
    if ( t == NULL ) {
	parse_to( msg->to->body.s,
		  msg->to->body.s + msg->to->body.len + 1, /* end of buffer */
		  &parsed_to );
	if ( parsed_to.error != PARSE_OK ) {
	    LM_ERR( "Bad To header" );
	    return( -1 );
	}
	t = &parsed_to;
    }

    *to = t;

    return( 0 );
}
