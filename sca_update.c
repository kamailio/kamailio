#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_update.h"


const str		SCA_METHOD_UPDATE = STR_STATIC_INIT( "UPDATE" );


    static dlg_t *
sca_update_dlg_for_info( str *ruri, str *from_uri, str *to_uri,
	str *call_id, str *from_tag, str *to_tag )
{
    dlg_t		*dlg;
    static int		cseq = 1;

    dlg = (dlg_t *)pkg_malloc( sizeof( dlg_t ));
    if ( dlg == NULL ) {
	LM_ERR( "sca_update_dlg_for_info: pkg_malloc dlg_t for %.*s "
		"failed: out of memory", STR_FMT( ruri ));
	return( NULL );
    }
    memset( dlg, 0, sizeof( dlg_t ));

    /* all of our UPDATEs are one-offs after the dialog's been replaced */
    dlg->loc_seq.value = cseq++;
    dlg->loc_seq.is_set = 1;

    /* RURI */
    dlg->rem_target = *ruri;

    /* From */
    dlg->loc_uri = *from_uri;

    /* To */
    dlg->rem_uri = *to_uri;

    /* dialog */
    dlg->id.call_id = *call_id;
    dlg->id.loc_tag = *from_tag;
    dlg->id.rem_tag = *to_tag;

    dlg->state = DLG_CONFIRMED;

    return( dlg );
}

    static void
sca_update_endpoint_reply_cb( struct cell *t, int cb_type,
	struct tmcb_params *cbp )
{
    sip_msg_t		*update_reply = NULL;

    if ( cbp == NULL ) {
	LM_ERR( "Empty parameters passed to UPDATE callback!" );
	return;
    }
    if (( update_reply = cbp->rpl ) == NULL ) {
	LM_ERR( "Empty reply passed to UPDATE callback!" );
	return;
    }
    if ( update_reply == FAKED_REPLY ) {
	LM_ERR( "UPDATE failed: FAKED REPLY from proxy" );
	return;
    }

    LM_INFO( "UPDATE returned %d", update_reply->REPLY_STATUS );
}

    int
sca_update_endpoint( sca_mod *scam, str *request_uri, str *from_uri,
	str *to_uri, str *contact_uri, str *call_id, str *from_tag,
	str *to_tag )
{
    uac_req_t		update_req;
    dlg_t		*dlg = NULL;
    str			headers = STR_NULL;
    char		hdrbuf[ 1024 ];
    int			len;
    int			rc = -1;

    assert( scam != NULL );
    assert( request_uri != NULL );
    assert( from_uri != NULL );
    assert( to_uri != NULL );
    assert( call_id != NULL );
    assert( from_tag != NULL );
    assert( to_tag != NULL );

    dlg = sca_update_dlg_for_info( request_uri, from_uri, to_uri, call_id,
				    from_tag, to_tag );
    if ( dlg == NULL ) {
	LM_ERR( "sca_update_endpoint failed: could not create dlg for %.*s",
		STR_FMT( request_uri ));
	return( -1 );
    }

    headers.s = hdrbuf;
    len = contact_uri->len + strlen( "Contact: " ) + CRLF_LEN;
    if ( len >= sizeof( hdrbuf )) {
	LM_ERR( "sca_update_endpoint: Contact URI <%.*s> too long",
		STR_FMT( contact_uri ));
	goto done;
    }

    len = strlen( "Contact: " );
    memcpy( hdrbuf, "Contact: ", len );
    memcpy( hdrbuf + len, contact_uri->s, contact_uri->len );
    len += contact_uri->len;
    memcpy( hdrbuf + len, CRLF, CRLF_LEN );
    len += CRLF_LEN;
    headers.len = len;

    set_uac_req( &update_req, (str *)&SCA_METHOD_UPDATE, &headers, NULL, dlg,
		TMCB_LOCAL_COMPLETED, sca_update_endpoint_reply_cb, scam );

    rc = scam->tm_api->t_request_within( &update_req );
    if ( rc < 0 ) {
	LM_ERR( "Failed to send UPDATE to %.*s", STR_FMT( request_uri ));
	/* fall through, return rc from t_request_within */
    }

done:
    if ( dlg != NULL ) {
	pkg_free( dlg );
    }

    return( rc );
}
