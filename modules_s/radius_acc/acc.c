/* $Id: acc.c
 *
 * Radius accounting module. We log requests and replies by 
 * attaching to the callbacks in the transaction module. The 
 * accounting requests are constructed using the radiusclient
 * library into standard value pairs and sent to radius server.
 * All the retransmissions and server backup information is administered
 * by radiusclient either by the libray it self or the configuration
 * files. Look at the Radius AAA report for details.
 * 
 * @author Stelios Sidiroglou-Douskos <ssi@fokus.gmd.de>
 * $Id$
 */

#include <radiusclient.h>
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include <string.h>
#include "../../ut.h"
#include "../../error.h"
#include "../tm/t_hooks.h"
#include "../tm/tm_load.h"
#include "../../config.h"
#include <ctype.h> 								/*isdigit */
#include "acc_mod.h"	
#include "../../parser/digest/digest.h" 		/*digest parser*/
#include "../../parser/digest/digest_parser.h"  /*digest struct*/
#include "../../parser/parse_from.h"			/*from parser*/
#include "utils.h"								/*auth_get_username*/
#define SIP_SERVICE_TYPE 15
#define TMP_SIZE		 256


/******************************************************************************
 *  Sends a "start" or "stop" accounting message to the RADIUS server.
 *  Params:struct cell* t          callback object
 *         struct sip_msg* msg     Sip message object
 *  returns: 0 on success
 *  		 -1 on failure
 *****************************************************************************/
int radius_log_reply(struct cell* t, struct sip_msg* msg)
{
	UINT4 av_type;							/* value pair integer */
  	int result;                           	/* Acct request status */
  	VALUE_PAIR *send = NULL;              	/* Radius Value pairs */
  	UINT4 client_port;                    	/* sip port */
  	struct sip_uri uri;                   	/* sip uri structure */   
  	int len, ret;							/* parse uri variables */
	char *tmp;								/* Temporary buffer */
	char *buf;								/* ibid */
	int stop = 0;                         	/* Acount status STOP flag */
 	struct to_body *to;						/* Structs containing tags */
  	struct sip_msg *rq;					  	/* Reply structure */
	struct to_body *from;
	struct cseq_body *cseq;					/* Cseq body--for Numeric */
	auth_body_t *cred;						/* Digital Credentials*/
	str	username;							/* Username string */
	
	DBG("**************radius_log_reply() CALLED \n");
  	rq =  t->uas.request;	

	/* 
   	 * Add status type, Accounting START in our case 
   	 *                   1: START
   	 *                   2: STOP
     *                   3: INTERIM-UPDATE
     *                   7: ACC ON
     *                   8: ACC OFF
     *                   15: RESERVED FOR FAILURES.
     * I need to add more logic here...
     */
  	if (rq->REQ_METHOD == METHOD_INVITE) {
    	av_type = PW_STATUS_START;
  	} else if ((rq->REQ_METHOD == METHOD_BYE) || 
						(rq->REQ_METHOD == METHOD_CANCEL)) {
    	av_type = PW_STATUS_STOP;
    	stop = 1; /* set stop flag look down for details*/
  	} else {
		DBG("WARNING: Radius accounting not started, method: %d...\n", 
							rq->REQ_METHOD);
		return(ERROR_RC);
	}
  	if (rc_avpair_add(&send, PW_ACCT_STATUS_TYPE, &av_type, 0) == NULL) {
    	DBG("radius_log_reply(): ERROR:PW_ACCT_STATUS_TYPE \n");
		return(ERROR_RC);
  	}
	
  	/* Add service type, always SIP */
  	av_type = SIP_SERVICE_TYPE; 
  	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &av_type, 0) == NULL) {
    	DBG("radius_log_reply(): ERROR:PW_SERVICE_TYPE \n");
		return(ERROR_RC);
  	}

  	/* Add sip method */  
  	if (rq->first_line.type == SIP_REQUEST) {
    	av_type = rq->REQ_METHOD;
    	if (rc_avpair_add(&send, PW_SIP_METHOD, &av_type, 0) == NULL) {
      		DBG("radius_log_reply(): ERROR:PW_SIP_METHOD \n");
			return(ERROR_RC);
    	}
  	}
	
  	/* FIXME: Add sip response code -- status code ,integer only if no method*/
  	if (rq->first_line.type == SIP_REPLY) {
		/* take a reply from message -- that's safe and we don't need to be
		   worried about TM reply status being changed concurrently */
    	av_type = msg->REPLY_STATUS;
   		if (rc_avpair_add(&send, PW_SIP_RESPONSE_CODE, &av_type, 0) == NULL) {
      		DBG("radius_log_reply(): ERROR:PW_SIP_RESPONSE_CODE \n");
			return(ERROR_RC);
    	}
  	}
		
	/* 
	 * Add calling station ID, URL in FROM string 
	 */
	if (msg->from->parsed == 0) {
		/* Do whatever is supposed to be done in shared mem */
		parse_from_header(msg->from);
	} 
	
	from = (struct to_body *)msg->from->parsed;
	
	if (from->error == PARSE_OK) {
		tmp = cleanbody(from->uri);
		
		/*If the parsing's OK then we can safely add the tags */
		if (rc_avpair_add(&send,PW_SIP_FROM_TAG,
					&from->tag_value.s[0], 0) == NULL) {
			DBG("radius_log_reply(): ERROR:PW_SIP_FROM_TAG \n");
    		return(ERROR_RC);
		}
	} else {
		DBG("radius_log_reply(): Error parsing from body \n");
		/* 
		 * Since the from body wasn't parsed correctly, copy the
		 * From body 
		 */
		tmp = &rq->from->body.s[0];
	}

	if (rc_avpair_add(&send, PW_CALLING_STATION_ID, tmp, 0) == NULL) {
    	DBG("radius_log_reply(): ERROR:PW_CALLING_STATION_ID %s \n", 
									rq->from->body.s);
		return(ERROR_RC);
	}
								
	/* 
	 * Add called station ID, URL in TO string parse_to_param
	 */
	to = (struct to_body *) msg->to->parsed;
	if (to->error == PARSE_OK) {
		tmp = cleanbody(to->uri);
		
		/*
		 * If the parsing's OK then we can safely add the tags 
		 * Actually, we still have to check if the tag was parsed correctly,
		 * make sure it's not NULL. That's why I memset the structure...
		 */
		if (rq->REQ_METHOD != METHOD_INVITE) {
			if (rc_avpair_add(&send,PW_SIP_TO_TAG,
		    					&to->tag_value.s[0], 0) == NULL) {
				DBG("radius_log_reply(): ERROR:PW_SIP_TO_TAG\n"); 
    			return(ERROR_RC);
			}
		}
	} else {
		DBG("radius_log_reply(): Error parsing to body \n");
		/* 
		 * Since the from body wasn't parsed correctly, copy the
		 * TO body 
		 */
		tmp = &rq->to->body.s[0];
	}

  	if (rc_avpair_add(&send, PW_CALLED_STATION_ID, tmp, 0) == NULL) {
		DBG("radius_log_reply(): ERROR:PW_CALLED_STATION_ID %.*s]\n", 
									rq->to->body.len, rq->to->body.s);
		return(ERROR_RC);
  	}
							
	/* Parse the cseq numeric part */
  	cseq = get_cseq(rq);
	tmp = cleanbody(cseq->number);
  	if (rc_avpair_add(&send,PW_SIP_CSEQ , tmp, 0) == NULL) {
		DBG("radius_log_reply(): ERROR:PW_SIP_CSEQ \n");
		return(ERROR_RC);
  	}

	/* Add Reply Status... The method of the original request... */
	av_type = rq->REPLY_STATUS;
   	if (rc_avpair_add(&send, PW_SIP_RESPONSE_CODE, &av_type, 0) == NULL) {
   		DBG("radius_log_ack(): ERROR:PW_SIP_RESPONSE_CODE \n");
		return(ERROR_RC);
	}
	
	/*
	 * Parse uri structure...
	 */
  	if (rq->new_uri.s) {
    	buf = rq->new_uri.s;
    	len = rq->new_uri.len;
  	} else{
    	buf = rq->first_line.u.request.uri.s;
    	len = rq->first_line.u.request.uri.len;
  	}
  	ret = parse_uri(buf, len, &uri);
  	if (ret < 0) {
    	LOG(L_ERR, "ERROR: Accounting bad_uri <%s>,"
				" dropping packet\n",tmp);
    	return(ERROR_RC);
  	}

	/*
	 * If available, take user-name from digest else extract the FROM field.
	 */

	if (!(rq->authorization)) {
		     /* No credentials parsed yet */
		if (parse_headers(rq, HDR_AUTHORIZATION, 0) == -1) {
			LOG(L_ERR, "radis_log_reply: Error while parsing auth headers\n");
			return -2;
		}
	}
	
	/* Parse only if there's something there... */
	if (rq->authorization) {
		if (parse_credentials(rq->authorization) != -1) {
			cred = (auth_body_t*)(rq->authorization->parsed);
			if (check_dig_cred(&(cred->digest)) != E_DIG_OK) {
				LOG(L_ERR, "radius_log_reply: Credentials missing\n");
				return(ERROR_RC);
			} else {
				/* Extract username from uri */
				username.s = from->uri.s;
				username.len = from->uri.len;
				if (auth_get_username(&username) < 0) {
            	    LOG(L_ERR, "radius_log_reply: "
									"Error while extracting username\n");
        	        return(ERROR_RC);
		        }
				tmp = cleanbody(username);
			}
		}
	} else {
		/* Extract username from uri */
		username.s = from->uri.s;
		username.len = from->uri.len;
		if (auth_get_username(&username) < 0) {
        	LOG(L_ERR, "radius_log_reply: "
							"Error while extracting username\n");
       	    return(ERROR_RC);
		}
		tmp = cleanbody(username);
	}
		
	if (rc_avpair_add(&send, PW_USER_NAME, tmp, 0) == NULL) {
    	DBG("radius_log_reply(): ERROR: \n");
		return(ERROR_RC);
	}
	
  	/* Add sip-translated-request-uri string*/
	if (msg->new_uri.s) 
		tmp = cleanbody(rq->new_uri);     
    else
    	tmp = cleanbody(rq->first_line.u.request.uri);

  	if (rc_avpair_add(&send, PW_SIP_TRANSLATED_REQ_URI, 
						tmp, 0) == NULL) {
  		DBG("radius_log_reply(): ERROR:PW_SIP_TRANSLATED_REQ_URI \n");
		return(ERROR_RC);
  	}
	
	/* 
	 * ADD ACCT-SESSION ID ---> CALL-ID 
     * start and stop must have the same callid...
     */
	tmp = cleanbody(rq->callid->body);
	if (rc_avpair_add(&send, PW_ACCT_SESSION_ID, tmp, 0) == NULL) {
		DBG("radius_log_reply(): ERROR:PW_ACCT_SESSION_ID \n");
		return(ERROR_RC); 
  	}

  	/*FIXME: Dorgham and Jiri said that we don't need this
   	 * Acct-session-time
     * Can only be present in Accounting-Request records
     * where Acct-status-type is set to STOP.
     * Represents the time between the arrival of 200 OK
     * in response to an invite from caller until the the
     * arrival of the corresponding BYE request by either 
     * party. 
     */
	if (stop) {
    	av_type = 0; /* Add session time here... */
    	if (rc_avpair_add(&send, PW_ACCT_SESSION_TIME,
			      &av_type, 0) == NULL) {
      		DBG("radius_log_reply(): ERROR:PW_ACCT_SESSION_TIME \n");
			return(ERROR_RC); 
    	}
  	}

	/* FIXME: Dorgham said that we don't need this...
   	 * Acct-Terminate-Cause 
   	 * Termination cause only if Acct-status type is STOP
   	 * 1   User Request (BYE)
   	 * 3   Lost Service
   	 * 4   Idle Timeout (e.g., loss of RTP or RTCP)
     * 5   Session Timeout (e.g., SIP session timer)
     * 6   Admin Reset
     * 7   Admin Reboot
     * 8   Port Error
     * 9   NAS Error
     * 10  NAS Request
     * 11  NAS Reboot
     * 15  Service Unavailable
     */
	if (stop) {
    	av_type = 0; /* Add session terminate cause here... */
    	if (rc_avpair_add(&send, PW_ACCT_TERMINATE_CAUSE,
					      &av_type, 0) == NULL) {
      		DBG("radius_log_reply(): ERROR:PW_ACCT_TERMINATE_CAUSE \n");
			return(ERROR_RC); 
    	}
  	}
  
	/* Radius authentication... This is not needed */
  	av_type = PW_RADIUS;
  	if (rc_avpair_add(&send, PW_ACCT_AUTHENTIC, &av_type, 0) == NULL) {
    	DBG("radius_log_reply(): ERROR:PW_ACCT_AUTHENTIC \n");
		return(ERROR_RC);	
  	}
    
  	/* Send Request to Radius Server */
	client_port = SIP_PORT;
  	result = rc_acct(client_port, send);

	/* I think that is done automatically but it doesn't hurt */
  	rc_avpair_free(send);
  
  	if (result != OK_RC) {
    	/* RADIUS server could be down so make this a warning */
    	DBG("radius_acct_start(): Error: Accounting Request Rejected\n");
  	} else {
    	DBG("RADIUS Accounting Request Received... \n");
  	}
  
  	return result;
}

/***********************************************************
 *  Sends a "start" accounting message to the RADIUS server.
 *  Params:struct cell* t          callback object
 *         struct sip_msg* msg     Sip message object
 *  returns: 0 on success
 *  		 -1 on failure
 ***********************************************************/
int radius_log_ack(struct cell* t, struct sip_msg* msg)
{
	UINT4 av_type;							/* value pair integer */
  	int result;                           	/* Acct request status */
  	VALUE_PAIR *send = NULL;              	/* Radius Value pairs */
  	UINT4 client_port;                    	/* sip port */
  	struct sip_uri uri;                   	/* sip uri structure */   
  	int len, ret;							/* parse uri variables */
	char *tmp;								/* Temporary buffer */
	char *buf;								/* ibid */
  	int stop = 0;                         	/* Acount status STOP flag */
 	struct to_body *to;						/* Structs containing TO tags */
  	struct sip_msg *rq;					  	/* Reply structure */
	struct to_body *from;					/* struct to hold FROM tags */
 
	DBG("**************radius_log_ack() CALLED \n");

  	rq =  t->uas.request;	
 	
	/* Add session ID */
	if (rc_avpair_add(&send, PW_ACCT_SESSION_ID, msg->callid, 0) == NULL) {
    	DBG("radius_log_ack(): ERROR:PW_ACCT_SESSION_ID, \n");
		return(ERROR_RC);	
  	}	
  	    
  	/*
	 * Add status type, Accounting START in our case 
   	 *                   1: START
   	 *                   2: STOP
     *                   3: INTERIM-UPDATE
     *                   7: ACC ON
     *                   8: ACC OFF
     *                   15: RESERVED FOR FAILURES.
     * I need to add more logic here...
     */
  	if (rq->REQ_METHOD == METHOD_INVITE) {
    	av_type = PW_STATUS_START;
  	} else if (rq->REQ_METHOD == METHOD_BYE) {
    	av_type = PW_STATUS_STOP;
    	stop = 1; /* set stop flag look down for details*/
  	} else {
		DBG("WARNING: Radius accounting not started, method: %d...\n", 
							msg->REQ_METHOD);
		return(ERROR_RC);
	}
  	if (rc_avpair_add(&send, PW_ACCT_STATUS_TYPE, &av_type, 0) == NULL) {
    	DBG("radius_log_ack(): ERROR:PW_ACCT_STATUS_TYPE \n");
		return(ERROR_RC);
  	}

  	/* Add service type, always SIP */
  	av_type = SIP_SERVICE_TYPE; 
  	if (rc_avpair_add(&send, PW_SERVICE_TYPE, &av_type, 0) == NULL) {
    	DBG("radius_log_ack(): ERROR:PW_SERVICE_TYPE \n");
		return(ERROR_RC);
  	}

  	/* Add sip method */  
  	if (msg->first_line.type == SIP_REQUEST) {
    	av_type = msg->REQ_METHOD;
    	if (rc_avpair_add(&send, PW_SIP_METHOD, &av_type, 0) == NULL) {
      		DBG("radius_log_ack(): ERROR:PW_SIP_METHOD \n");
			return(ERROR_RC);
    	}
  	}

	/* Add Reply Status... The method of the original request... */
	av_type = rq->REPLY_STATUS;
   	if (rc_avpair_add(&send, PW_SIP_RESPONSE_CODE, &av_type, 0) == NULL) {
   		DBG("radius_log_ack(): ERROR:PW_SIP_RESPONSE_CODE \n");
		return(ERROR_RC);
	}
  	
	/* 
	 * Add calling station ID, URL in FROM string 
	 */
	if (msg->from->parsed == 0) {
		/* Do whatever is supposed to be done in shared mem */
		parse_from_header(msg->from);
	} 
	
	from = (struct to_body *)msg->from->parsed;

	if (from->error == PARSE_OK) {
		tmp = cleanbody(from->uri);
		
		/* Add sip-from-tag string */
  		if (rc_avpair_add(&send,PW_SIP_FROM_TAG, 
									&from->tag_value.s[0], 0) == NULL) {
    		DBG("radius_log_ack(): ERROR: PW_SIP_FROM_TAG \n");
			return(ERROR_RC);
    	} 

	} else {
		DBG("radius_log_ack(): Error parsing from body \n");
		/* 
		 * Since the from body wasn't parsed correctly, copy the
		 * From body 
		 */
		tmp = &msg->from->body.s[0];
	}


	if (rc_avpair_add(&send, PW_CALLING_STATION_ID, tmp, 0) == NULL) {
    	DBG("radius_log_ack(): ERROR:PW_CALLING_STATION_ID %s \n", 
									msg->from->body.s);
		return(ERROR_RC);
	}
	
									
	/* Add calling station ID, URL in TO string */
  	to = (struct to_body *)msg->to->parsed;
	
	if (to->error == PARSE_OK) {
		tmp = cleanbody(to->uri);
		
		/*Add sip-to-tag string */
		if (rc_avpair_add(&send,PW_SIP_TO_TAG  ,
		    	&to->tag_value.s[0], 0) == NULL) {
    		DBG("radius_log_ack(): ERROR: PW_SIP_TO_TAG \n");
			return(ERROR_RC);
    	} 
	} else {
		DBG("radius_log_ack(): Error parsing to body \n");
		/* 
		 * Since the from body wasn't parsed correctly, copy the
		 * From body 
		 */
		tmp = &msg->to->body.s[0];
	}

	if (rc_avpair_add(&send, PW_CALLED_STATION_ID, tmp, 0) == NULL) {
		DBG("radius_log_ack(): ERROR:PW_CALLED_STATION_ID %.*s]\n", 
									msg->to->body.len, msg->to->body.s);
		return(ERROR_RC);
  	}
   		
  	/* Add sip-cseq string*/
	tmp = cleanbody(msg->cseq->body);
  	if (rc_avpair_add(&send, PW_SIP_CSEQ, tmp, 0) == NULL) {
		DBG("radius_log_ack(): ERROR:PW_SIP_CSEQ \n");
		return(ERROR_RC);
  	}  
    

	/*
	 * Parse the uri structure, if there's a new changed uri parse that one.
	 */
  	if (msg->new_uri.s) {
    	buf = msg->new_uri.s;
    	len = msg->new_uri.len;
  	} else{
    	buf = msg->first_line.u.request.uri.s;
    	len = msg->first_line.u.request.uri.len;
  	}

  	ret = parse_uri(buf, len, &uri);
  	if (ret < 0) {
    	LOG(L_ERR, "ERROR: Accounting bad_uri <%s>,"
			" dropping packet\n",tmp);
    	return(ERROR_RC);
  	}

  	/* Add user name */
  	if (rc_avpair_add(&send, PW_USER_NAME, uri.user.s, 0) == NULL) {
    	DBG("radius_log_ack(): ERROR: \n");
		return(ERROR_RC);
  	}
  	
	/* Add sip-translated-request-uri string*/
	if (msg->new_uri.s) {
		tmp = cleanbody(msg->new_uri);     
	}
    else
    	tmp = cleanbody(msg->first_line.u.request.uri);

	tmp = cleanbody(msg->new_uri);
  	if (rc_avpair_add(&send, PW_SIP_TRANSLATED_REQ_URI, 
							tmp, 0) == NULL) {
  		DBG("radius_log_ack(): ERROR:PW_SIP_TRANSLATED_REQ_URI \n");
		return(ERROR_RC);
  	}

	/* 
	 * ADD ACCT-SESSION ID ---> CALL-ID 
     * start and stop must have the same callid...
     */
	/*
	tmp = cleanbody(msg->callid->body);
	if (rc_avpair_add(&send, PW_ACCT_SESSION_ID, 
					&msg->callid->body.s[0], 0) == NULL) {
		DBG("radius_log_ack(): ERROR:PW_ACCT_SESSION_ID \n");
		return(ERROR_RC); 
  	}*/	 

  	/*FIXME: Dorgham and Jiri said that we don't need this
   	 * Acct-session-time
     * Can only be present in Accounting-Request records
     * where Acct-status-type is set to STOP.
     * Represents the time between the arrival of 200 OK
     * in response to an invite from caller until the the
     * arrival of the corresponding BYE request by either 
     * party. 
     */
	if (stop) {
    	av_type = 0; /* Add session time here... */
    	if (rc_avpair_add(&send, PW_ACCT_SESSION_TIME,
			      &av_type, 0) == NULL) {
      		DBG("radius_log_ack(): ERROR:PW_ACCT_SESSION_TIME \n");
			return(ERROR_RC); 
    	}
  	}

	/* FIXME: Dorgham said that we don't need this...
   	 * Acct-Terminate-Cause 
   	 * Termination cause only if Acct-status type is STOP
   	 * 1   User Request (BYE)
   	 * 3   Lost Service
   	 * 4   Idle Timeout (e.g., loss of RTP or RTCP)
     * 5   Session Timeout (e.g., SIP session timer)
     * 6   Admin Reset
     * 7   Admin Reboot
     * 8   Port Error
     * 9   NAS Error
     * 10  NAS Request
     * 11  NAS Reboot
     * 15  Service Unavailable
     */
	if (stop) {
    	av_type = 0; /* Add session terminate cause here... */
    	if (rc_avpair_add(&send, PW_ACCT_TERMINATE_CAUSE,
					      &av_type, 0) == NULL) {
      		DBG("radius_log_ack(): ERROR:PW_ACCT_TERMINATE_CAUSE \n");
			return(ERROR_RC); 
    	}
  	}
  
	/* Radius authentication... This is not needed */
  	av_type = PW_RADIUS;
  	if (rc_avpair_add(&send, PW_ACCT_AUTHENTIC, &av_type, 0) == NULL) {
    	DBG("radius_log_ack(): ERROR:PW_ACCT_AUTHENTIC \n");
		return(ERROR_RC);	
  	}

    /* Send Request to Radius Server */
  	client_port = SIP_PORT;
  	result = rc_acct(client_port, send);

	/* I think that is done automatically but it doesn't hurt */
  	rc_avpair_free(send);
  
  	if (result != OK_RC) {
    	/* RADIUS server could be down so make this a warning */
    	DBG("radius_acct_start(): Error, Accounting Request failed\n");
  	} else {
    	DBG("RADIUS Accounting Request Accepted... \n");
  	}
  
  	return result;
}

  	

/*
 *
 *
 */
int rad_acc_request( struct sip_msg *rq, char * comment, char  *foo)
{
	int comment_len;

	comment_len = strlen(comment);

	printf("rad_acc_request(): ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ CALLED!!!\n");

	/* we can parse here even if we do not knwo whether the
	   request is an incmoing pkg_mem message or stoed  shmem_msg;
	   thats because acc_missed_report operating over shmem never
	   enters acc_missed if the header fields are not parsed
	   and entering the function from script gives pkg_mem,
	   which can be parsed
	*/
	   
	if ( parse_headers(rq, HDR_FROM | HDR_CALLID, 0)==-1 
				|| !(rq->from && rq->callid) ) {
		LOG(L_ERR, "ERROR: acc_missed: From not dounf\n");
		return -1;
	}
	LOG( log_level,
		"ACC: call missed: "
		"i-uri=%.*s, o-uri=%.*s, call_id=%.*s, "
		"from=%.*s, reason=%.*s\n",
        rq->first_line.u.request.uri.len,
        rq->first_line.u.request.uri.s,
		rq->new_uri.len, rq->new_uri.s,
		rq->callid->body.len, rq->callid->body.s,
		rq->from->body.len, rq->from->body.s,
        comment_len, comment);
	return 1;
}

/*
 *
 *
 */
void rad_acc_missed_report( struct cell* t, struct sip_msg *reply,
	unsigned int code )
{
	struct sip_msg *rq;
	str acc_text;

	rq =  t->uas.request;
	/* it is coming from TM -- it must be already parsed ! */
	if (! rq->from ) {
		LOG(L_ERR, "ERROR: TM request for accounting not parsed\n");
		return;
	}

	get_reply_status(&acc_text, reply, code);
	if (acc_text.s==0) {
		LOG(L_ERR, "ERROR: acc_missed_report: get_reply_status failed\n" );
		return;
	}

	rad_acc_request(rq, acc_text.s , 0 /* foo */);
	pkg_free(acc_text.s);

	/* zdravime vsechny cechy -- jestli jste se dostali se ctenim 
	   kodu az sem a rozumite mu, jste na tom lip, nez autor!
	   prijmete uprimne blahoprani
	*/
}



