/* $Id: Radius Authorization
 * Radius digest Authorize method.
 * @author Stelios Sidiroglou-Douskos <ssi@fokus.gmd.de>
 * $Id$
 */


#include <radiusclient.h>
#include "digest.h"
#include "../../str.h"
#include "../../parser/digest/digest_parser.h"
#include <string.h>
#include "../../dprint.h"
#include "utils.h"
/*
 * Sends the the digest information to the radius server so that
 * the radius server which holds the only unknown, the password
 * can reconstruct the response and see if it matches the message.
 *
 * params: cred_t*	cred 	Pointer to the credential structure which
 * 							holds neccessary digest information.
 * 		   str*		method 	String representation of the SIP method.
 *
 * returns -1 on failure, 
 * 			0 on success
 */
int radius_authorize(dig_cred_t * cred, str* _method) 
{
	int             result;
    char            msg[4096];
    VALUE_PAIR      *send, *received;
    UINT4           service;
    str 			method; 
	send = NULL;

	method.s = _method->s;
	method.len = _method->len;
	/*
	 * Add all the user digest parameters according to the qop defined.
	 * Most devices tested only offer support for the simplest digest.
	 */
	if (rc_avpair_add(&send, PW_USER_NAME, 
						cleanbody(cred->username), 0) == NULL)
    	return(ERROR_RC);

	if (rc_avpair_add(&send, PW_SIP_USER_ID, 
						cleanbody(cred->username), 0) == NULL)
    	return (ERROR_RC);

	if (rc_avpair_add(&send, PW_SIP_USER_REALM, 
						cleanbody(cred->realm), 0) == NULL)
        return (ERROR_RC);
 
	if (rc_avpair_add(&send, PW_SIP_NONCE, 
						cleanbody(cred->nonce), 0) == NULL)
        return (ERROR_RC);
 
	if (rc_avpair_add(&send, PW_SIP_NONCE_COUNT, 
						cleanbody(cred->nc), 0) == NULL)
        return (ERROR_RC);

	if (rc_avpair_add(&send, PW_SIP_USER_DIGEST_URI, 
						cleanbody(cred->uri), 0) == NULL)
        return (ERROR_RC);
	
	if (rc_avpair_add(&send, PW_SIP_USER_METHOD, 
						cleanbody(method), 0) == NULL)
        return (ERROR_RC);

	/*
	if (cred->qop == QOP_AUTH) {
		if (rc_avpair_add(&send, PW_SIP_QOP, "auth", 0) == NULL)
        	return (ERROR_RC);
	} else if (cred->qop == QOP_AUTH_INT) {
		if (rc_avpair_add(&send, PW_SIP_QOP, "auth-int", 0) == NULL)
        	return (ERROR_RC);
	} else  {
		if (rc_avpair_add(&send, PW_SIP_QOP, "", 0) == NULL)
        	return (ERROR_RC);
	}
	*/

	if (rc_avpair_add(&send, PW_SIP_USER_RESPONSE, cred->response.s, 0) == NULL)
        return (ERROR_RC);

	
	/* Indicate the service type, Authenticate only in our case */
       service = PW_AUTHENTICATE_ONLY;
	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0) == NULL) {
		DBG("radius_authorize() Error adding service type \n");
	 	return (ERROR_RC);  	
	}
       
    result = rc_auth(0, send, &received, msg);
       
    if (result == OK_RC) {
    	DBG("RADIUS AUTHENTICATION SUCCESS \n");
	}
    else {
		DBG("RADIUS AUTHENTICATION FAILURE \n");
	}
    return result;
}


/*
 * This is an alternative version that works with the implementation
 * provided by freeradius. The difference here is that all the parameters
 * are placed into one Attribute (DIGEST_ATTRIBUTES) so that to economize
 * on name-mapping on the radius servers. I have kept the code structure
 * similar to the previous example and have adjusted to DIGEST_ATTRIBUTES
 * prior to sending the msg for code simplicity.
 */
int radius_authorize_freeradius(dig_cred_t * cred, str* _method) 
{
	int             result;
    char            msg[4096];
    VALUE_PAIR      *send, *received;
    UINT4           service;
    VALUE_PAIR 		*vp;    
	str				method;
	send = NULL;

	method.s = _method->s;
	method.len = _method->len;

	/*
	 * Add all the user digest parameters according to the qop defined.
	 * Most devices tested only offer support for the simplest digest.
	 */
	if (rc_avpair_add(&send, PW_USER_NAME, 
						cleanbody(cred->username), 0) == NULL)
    	return(ERROR_RC);

	if (rc_avpair_add(&send, PW_DIGEST_USER_NAME, 
						cleanbody(cred->username), 0) == NULL)
    	return (ERROR_RC);

	if (rc_avpair_add(&send, PW_DIGEST_REALM, 
						cleanbody(cred->realm), 0) == NULL)
        return (ERROR_RC);
 
	if (rc_avpair_add(&send, PW_DIGEST_NONCE, 
						cleanbody(cred->nonce), 0) == NULL)
        return (ERROR_RC);
 
	if (rc_avpair_add(&send, PW_DIGEST_URI, 
						cleanbody(cred->uri), 0) == NULL)
        return (ERROR_RC);
	
	if (rc_avpair_add(&send, PW_DIGEST_METHOD, 
						cleanbody(method), 0) == NULL)
        return (ERROR_RC);
	
	/* 
	 * Add the additional authentication fields according to the QOP.
	 */
	if (cred->qop.qop_parsed == QOP_AUTH) {
		if (rc_avpair_add(&send, PW_DIGEST_QOP, "auth", 0) == NULL) {
        	return (ERROR_RC);
		}
		
		if (rc_avpair_add(&send, PW_DIGEST_NONCE, 
							cleanbody(cred->nc), 0) == NULL)
        return (ERROR_RC);
		
		if (rc_avpair_add(&send, PW_DIGEST_CNONCE, 
							cleanbody(cred->cnonce), 0) == NULL) {
        	return (ERROR_RC);
		}
		
	} else if (cred->qop.qop_parsed == QOP_AUTHINT) {
		if (rc_avpair_add(&send, PW_DIGEST_QOP, "auth-int", 0) == NULL)
        	return (ERROR_RC);

		if (rc_avpair_add(&send, PW_DIGEST_NONCE_COUNT, 
							cleanbody(cred->nc), 0) == NULL) {
			return (ERROR_RC);
		}
		
		if (rc_avpair_add(&send, PW_DIGEST_CNONCE, 
							cleanbody(cred->cnonce), 0) == NULL) {
        	return (ERROR_RC);
		}

		if (rc_avpair_add(&send, PW_DIGEST_BODY_DIGEST, 
							cleanbody(cred->opaque), 0) == NULL) {
        	return (ERROR_RC);
		}
		
	} else  {
		/* send nothing for qop == "" */
	}
	
	

	/*
	 * Now put everything place all the previous attributes into the
	 * PW_DIGEST_ATTRIBUTES
	 */
	
	/*
	 *  Fix up Digest-Attributes issues see draft-sterman-aaa-sip-00
	 */
	for (vp = send; vp != NULL; vp = vp->next) {
		switch (vp->attribute) {
	  		default:
	    	break;

			/* Fall thru the know values */
			case PW_DIGEST_REALM:
			case PW_DIGEST_NONCE:
			case PW_DIGEST_METHOD:
			case PW_DIGEST_URI:
			case PW_DIGEST_QOP:
			case PW_DIGEST_ALGORITHM:
			case PW_DIGEST_BODY_DIGEST:
			case PW_DIGEST_CNONCE:
			case PW_DIGEST_NONCE_COUNT:
			case PW_DIGEST_USER_NAME:
	
			/* overlapping! */
			memmove(&vp->strvalue[2], &vp->strvalue[0], vp->lvalue);
			vp->strvalue[0] = vp->attribute - PW_DIGEST_REALM + 1;
			vp->lvalue += 2;
			vp->strvalue[1] = vp->lvalue;
			vp->attribute = PW_DIGEST_ATTRIBUTES;
			break;
		}
	}

	/* Add the response... What to calculate against... */
	if (rc_avpair_add(&send, PW_DIGEST_RESPONSE, 
						cleanbody(cred->response), 0) == NULL)
        return (ERROR_RC);

	/* Indicate the service type, Authenticate only in our case */
       service = PW_AUTHENTICATE_ONLY;
	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &service, 0) == NULL) {
		DBG("radius_authorize() Error adding service type \n");
	 	return (ERROR_RC);  	
	}
       
    result = rc_auth(0, send, &received, msg);
       
    if (result == OK_RC) {
    	DBG("RADIUS AUTHENTICATION SUCCESS \n");
		/*TODO:vp_printlist*/
		if (msg != NULL) 
			DBG("You belong to group: %s \n", msg);
	}
    else {
		DBG("RADIUS AUTHENTICATION FAILURE \n");
	}
    return result;
}


