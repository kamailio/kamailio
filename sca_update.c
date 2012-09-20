#include "sca_common.h"

#include <assert.h>

#include "sca.h"
#include "sca_appearance.h"
#include "sca_update.h"
#include "sca_util.h"


const str		SCA_METHOD_UPDATE = STR_STATIC_INIT( "UPDATE" );
const str		SCA_METHOD_INVITE = STR_STATIC_INIT( "INVITE" );


    static dlg_t *
sca_update_dlg_for_info( str *ruri, str *from_uri, str *to_uri,
	str *contact_uri, str *call_id, str *from_tag, str *to_tag )
{
    dlg_t		*dlg;
    sip_uri_t		f_uri, c_uri;
    str			from_aor = STR_NULL;
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

    /*
     * reconcile the contact and the From URI. the .s value must be
     * freed in the caller.
     */
    if ( parse_uri( from_uri->s, from_uri->len, &f_uri ) < 0 ) {
	LM_ERR( "sca_update_dlg_for_info: parse_uri %.*s failed",
		STR_FMT( from_uri ));
	goto error;
    }
    if ( parse_uri( contact_uri->s, contact_uri->len, &c_uri ) < 0 ) {
	LM_ERR( "sca_update_dlg_for_info: parse_uri %.*s failed",
		STR_FMT( contact_uri ));
	goto error;
    }

    if ( sca_aor_create_from_info( &from_aor, c_uri.type, &c_uri.user,
		&f_uri.host, &f_uri.port ) < 0 ) {
	LM_ERR( "sca_update_dlg_for_info: sca_aor_create_from_info failed" );
	goto error;
    }

    /* From */
    dlg->loc_uri = from_aor;

    /* To */
    dlg->rem_uri = *to_uri;

    /* dialog */
    dlg->id.call_id = *call_id;
    dlg->id.loc_tag = *from_tag;
    dlg->id.rem_tag = *to_tag;

    dlg->state = DLG_CONFIRMED;

    return( dlg );

error:
    if ( dlg != NULL ) {
	pkg_free( dlg );
    }

    return( NULL );
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
	str *to_tag, str *asserted_id )
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

    dlg = sca_update_dlg_for_info( request_uri, from_uri, to_uri, contact_uri,
				    call_id, from_tag, to_tag );
    if ( dlg == NULL ) {
	LM_ERR( "sca_update_endpoint failed: could not create dlg for %.*s",
		STR_FMT( request_uri ));
	return( -1 );
    }

    headers.s = hdrbuf;
    len = contact_uri->len + strlen( "Contact: " ) + CRLF_LEN;
    if ( !SCA_STR_EMPTY( asserted_id )) {
	len += asserted_id->len + strlen( "P-Asserted-Identity: " ) + CRLF_LEN;
    }
    if ( len >= sizeof( hdrbuf )) {
	LM_ERR( "sca_update_endpoint: addition headers too long" );
	goto done;
    }

    len = strlen( "Contact: " );
    memcpy( hdrbuf, "Contact: ", len );
    memcpy( hdrbuf + len, contact_uri->s, contact_uri->len );
    len += contact_uri->len;
    memcpy( hdrbuf + len, CRLF, CRLF_LEN );
    len += CRLF_LEN;

    if ( !SCA_STR_EMPTY( asserted_id )) {
	memcpy( hdrbuf + len, "P-Asserted-Identity: ",
		    strlen( "P-Asserted-Identity: " ));
	len += strlen( "P-Asserted-Identity: " );
	memcpy( hdrbuf + len, asserted_id->s, asserted_id->len );
	len += asserted_id->len;
	memcpy( hdrbuf + len, CRLF, CRLF_LEN );
	len += CRLF_LEN;
    }

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
	pkg_free( dlg->loc_uri.s );
	pkg_free( dlg );
    }

    return( rc );
}

    int
sca_update_endpoints( sip_msg_t *msg, char *p1, char *p2 )
{
    sca_appearance	*app = NULL;
    sip_uri_t		a_uri, c_uri;
    struct to_body	*from;
    struct to_body	*to;
    str			from_aor = STR_NULL;
    str			to_aor = STR_NULL;
    str			aor = STR_NULL;
    int			slot_idx = -1;
    int			rc = -1;

    if ( sca_get_msg_from_header( msg, &from ) < 0 ) {
	LM_ERR( "sca_update_endpoints: failed to get From header" );
	goto done;
    }
    if ( sca_get_msg_to_header( msg, &to ) < 0 ) {
	LM_ERR( "sca_update_endpoints: failed to get To header" );
	goto done;
    }

    if ( sca_uri_extract_aor( &from->uri, &from_aor ) < 0 ) {
	LM_ERR( "sca_update_endpoints: failed to get From AoR from %.*s",
		STR_FMT( &from->uri ));
	goto done;
    }
    if ( sca_uri_extract_aor( &to->uri, &to_aor ) < 0 ) {
	LM_ERR( "sca_update_endpoints: failed to get To AoR from %.*s",
		STR_FMT( &to->uri ));
	goto done;
    }

    if ( sca_uri_lock_if_shared_appearance( sca, &from_aor, &slot_idx )) {
	app = sca_appearance_for_tags_unsafe( sca, &from_aor,
		    &msg->callid->body, &from->tag_value, NULL, slot_idx );
	if ( app == NULL ) {
	    LM_ERR( "sca_update_endpoints: No appearance for %.*s matching "
		    "call-id <%.*s> and from-tag <%.*s>", STR_FMT( &from_aor ),
		    STR_FMT( &msg->callid->body ), STR_FMT( &from->tag_value ));
	    goto done;
	}

#ifdef notdef
	setflag( msg, (flag_t)sca->cfg->update_flag );
#endif /* notdef */

	/* reconcile the contact and the URIs. */
	if ( parse_uri( from->uri.s, from->uri.len, &a_uri ) < 0 ) {
	    LM_ERR( "sca_update_dlg_for_info: parse_uri %.*s failed",
		    STR_FMT( &from_aor ));
	    goto done;
	}
	if ( parse_uri( app->callee.s, app->callee.len, &c_uri ) < 0 ) {
	    LM_ERR( "sca_update_dlg_for_info: parse_uri %.*s failed",
		    STR_FMT( &app->callee ));
	    goto done;
	}
LM_INFO( "ADMORTEN DEBUG: a_uri.host: %.*s, a_uri.port: %.*s",
		STR_FMT( &a_uri.host ), STR_FMT( &a_uri.port ));
LM_INFO( "ADMORTEN DEBUG: c_uri.user: %.*s", STR_FMT( &c_uri.user ));
	if ( sca_aor_create_from_info( &aor, c_uri.type, &c_uri.user,
		    &a_uri.host, &a_uri.port ) < 0 ) {
	    LM_ERR( "sca_update_dlg_for_info: sca_aor_create_from_info failed");
	    goto done;
	}
LM_INFO( "ADMORTEN DEBUG: aor: %.*s", STR_FMT( &aor ));

	/* UPDATE both endpoints to use correct URIs */
	if ( sca_update_endpoint( sca, &app->owner, &to_aor, &from_aor,
		&app->callee, &app->dialog.call_id, &app->dialog.to_tag,
		&app->dialog.from_tag, NULL ) < 0 ) {
	    LM_ERR( "sca_call_info_ack_from_handler: failed to UPDATE "
                    "%.*s, Contact: %.*s, %.*s;to-tag=%.*s;from-tag=%.*s",
                    STR_FMT( &app->callee ), STR_FMT( &app->owner ),
                    STR_FMT( &app->dialog.call_id ),
                    STR_FMT( &app->dialog.from_tag ),
                    STR_FMT( &app->dialog.to_tag ));
            goto done;
	}

	if ( sca_update_endpoint( sca, &app->callee, &from_aor, &to_aor,
                &app->owner, &app->dialog.call_id, &app->dialog.from_tag,
                &app->dialog.to_tag, &aor ) < 0 ) {
            LM_ERR( "sca_call_info_ack_from_handler: failed to UPDATE "
                    "%.*s, Contact: %.*s, %.*s;to-tag=%.*s;from-tag=%.*s",
                    STR_FMT( &app->callee ), STR_FMT( &app->owner ),
                    STR_FMT( &app->dialog.call_id ),
                    STR_FMT( &app->dialog.from_tag ),
                    STR_FMT( &app->dialog.to_tag ));
            goto done;
        }
    }

    rc = 1;

done:
    if ( slot_idx >= 0 ) {
	sca_hash_table_unlock_index( sca->appearances, slot_idx );
    }
    if ( aor.s != NULL ) {
	/* pkg_malloc'd in sca_aor_create_from_info */
	pkg_free( aor.s );
    }

    return( rc );
}
