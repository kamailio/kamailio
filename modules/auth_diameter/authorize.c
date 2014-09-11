/*
 * $Id$
 *
 * Digest Authentication - Diameter support
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *`
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 * 
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-09-11: updated to new build_lump_rpl() interface (bogdan)
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags  (bogdan)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>

/* memory management */
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

/* printing messages, dealing with strings and other utils */
#include "../../dprint.h"
#include "../../str.h"
#include "../../ut.h"

/* digest parser headers */
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"

/* necessary when dealing with lumps */
#include "../../data_lump_rpl.h"

/* headers defined by this module */
#include "diameter_msg.h"
#include "auth_diameter.h"
#include "defs.h"
#include "authorize.h"
#include "tcp_comm.h"

static str dia_401_err = str_init(MESSAGE_401);
static str dia_403_err = str_init("Forbidden");
static str dia_407_err = str_init(MESSAGE_407);
static str dia_400_err = str_init(MESSAGE_400);
static str dia_500_err = str_init(MESSAGE_500);


/* Extract URI depending on the request from To or From header */
int get_uri(struct sip_msg* m, str** uri)
{
	if ((REQ_LINE(m).method.len == 8) && 
					(memcmp(REQ_LINE(m).method.s, "REGISTER", 8) == 0)) 
	{	
		/* REGISTER */
		if (!m->to && ((parse_headers(m, HDR_TO_F, 0) == -1)|| (!m->to))) 
		{
			LM_ERR("the To header field was not found or malformed\n");
			
			/* it was a REGISTER and an error appeared when parsing TO header*/
			return -1;
		}
		*uri = &(get_to(m)->uri);
	} 
	else 
	{
		if (parse_from_header(m)<0)
		{
			LM_ERR("failed to parse FROM header\n");

			/* an error appeared when parsing FROM header */
			return -1;
		}
		*uri = &(get_from(m)->uri);
	}

    /* success */
	return 0;
}


/* Return parsed To or From host part of the parsed uri (that is realm) */
int get_realm(struct sip_msg* m, int hftype, struct sip_uri* u)
{
	str uri;

	/* extracting the uri */
	if ((REQ_LINE(m).method.len==8)
					&& !memcmp(REQ_LINE(m).method.s, "REGISTER", 8) 
					&& (hftype == HDR_AUTHORIZATION_T) ) 
	{ 
		/* REGISTER */
		if (!m->to && ((parse_headers(m, HDR_TO_F, 0) == -1) || (!m->to))) 
		{
			LM_ERR("failed to parse TO header\n");
			/* signal the error */
			return -1;
		}
		
	    /* Body of To header field is parsed automatically */
		uri = get_to(m)->uri; 
	} 
	else 
	{
		if (parse_from_header(m)<0) 
		{
			LM_ERR("failed to parse FROM header\n");
			/* signal the error */
			return -1;
		}

		uri = get_from(m)->uri;
	}
	
	/* parsing the uri */
	if (parse_uri(uri.s, uri.len, u) < 0) 
	{
		LM_ERR("failed to parse URI\n");
		return -1;
	}
	
	/* everything was OK */
	return 0;
}


auth_diam_result_t diam_pre_auth(struct sip_msg* _m, str* _realm, int _hftype, 
													struct hdr_field** _h)
{
	int ret;
	struct sip_uri uri;
	str realm;

	if ((_m->REQ_METHOD == METHOD_ACK) ||  (_m->REQ_METHOD == METHOD_CANCEL))
		return AUTHORIZED;

	/* if no realm supplied, find out now */
	if (_realm==0 || _realm->len == 0) 
	{
		if (get_realm(_m, _hftype, &uri) < 0) 
		{
			LM_ERR("failed to extract realm\n");
			if (send_resp(_m, 400, &dia_400_err, 0, 0) == -1) 
			{
				LM_ERR("failed to send 400 reply\n");
			}
			return ERROR;
		}
		realm = uri.host;
	} else {
		realm = *_realm;
	}

	ret = find_credentials(_m, &realm, _hftype, _h);
	if (ret < 0) 
	{
		LM_ERR("credentials not found\n");
		if (send_resp(_m, (ret == -2) ? 500 : 400, 
			      (ret == -2) ? &dia_500_err : &dia_400_err, 0, 0) == -1) 
		{
			LM_ERR("failed to send 400 reply\n");
		}
		return ERROR;
	} 
	else 
		if (ret > 0) 
		{
			LM_ERR("credentials with given realm not found\n");
			return NO_CREDENTIALS;
		}
	

	return DO_AUTHORIZATION;
}


/* Authorize digest credentials */
int authorize(struct sip_msg* msg, pv_elem_t* realm, int hftype)
{
	auth_diam_result_t ret;
	struct hdr_field* h;
	auth_body_t* cred = NULL;
	str* uri;
	struct sip_uri puri;
	str  domain;

	if (realm) {
		if (pv_printf_s(msg, realm, &domain)!=0) {
			LM_ERR("pv_printf_s failed\n");
			return AUTH_ERROR;
		}
	} else {
		domain.len = 0;
		domain.s = 0;
	}

	/* see what is to do after a first look at the message */
	ret = diam_pre_auth(msg, &domain, hftype, &h);

	switch(ret) 
	{
		case NO_CREDENTIALS:   cred = NULL;
							   break;

		case DO_AUTHORIZATION: cred = (auth_body_t*)h->parsed;
							   break;
		default:               return ret;
	}

	if (get_uri(msg, &uri) < 0) 
	{
		LM_ERR("From/To URI not found\n");
		return AUTH_ERROR;
	}
	
	if (parse_uri(uri->s, uri->len, &puri) < 0) 
	{
		LM_ERR("failed to parse From/To URI\n");
		return AUTH_ERROR;
	}
//	user.s = (char *)pkg_malloc(puri.user.len);
//	un_escape(&(puri.user), &user);
	
	/* parse the ruri, if not yet */
	if(msg->parsed_uri_ok==0 && parse_sip_msg_uri(msg)<0)
	{
		LM_ERR("failed to parse the Request-URI\n");
		return AUTH_ERROR;
	}
	
	/* preliminary check */
	if(cred)
	{
		if (puri.host.len != cred->digest.realm.len) 
		{
			LM_DBG("credentials realm and URI host do not match\n");  
			return AUTH_ERROR;
		}
	
		if (strncasecmp(puri.host.s, cred->digest.realm.s, puri.host.len) != 0) 
		{
			LM_DBG("credentials realm and URI host do not match\n");
			return AUTH_ERROR;
		}
	}
	
	if( diameter_authorize(cred?h:NULL, &msg->first_line.u.request.method,
					puri, msg->parsed_uri, msg->id, rb) != 1)
	{
		send_resp(msg, 500, &dia_500_err, NULL, 0);
		return AUTH_ERROR;
	}
	
	if( srv_response(msg, rb, hftype) != 1 )
		return AUTH_ERROR;

	mark_authorized_cred(msg, h);

	return AUTHORIZED;
}



/*
 * This function creates and submits diameter authentication request as per
 * draft-srinivas-aaa-basic-digest-00.txt. 
 * Service type of the request is Authenticate-Only.
 * Returns:
 * 		 1 - success
 * 		-1 - error
 * 			
 */
int diameter_authorize(struct hdr_field* hdr, str* p_method, struct sip_uri uri,
						struct sip_uri ruri, unsigned int m_id, rd_buf_t* rb)
{
	str user_name;
	AAAMessage *req;
	AAA_AVP *avp, *position; 
	int name_flag, port_flag;
	dig_cred_t* cred;
	unsigned int tmp;

	if ( !p_method )
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if ( (req=AAAInMessage(AA_REQUEST, AAA_APP_NASREQ))==NULL)
		return -1;

	if(hdr && hdr->parsed)
		cred = &(((auth_body_t*)hdr->parsed)->digest);
	else
		cred = NULL;
			
	if(!cred)
	{
		/* Username AVP */
		user_name.s = 0;
		user_name.len = uri.user.len + uri.host.len;
		if(user_name.len>0)
		{
			user_name.len += 2;
			user_name.s = (char*)ad_malloc(user_name.len*sizeof(char));
			memset(user_name.s, 0, user_name.len);

			memcpy(user_name.s, uri.user.s, uri.user.len);
			if(uri.user.len>0)
			{
				memcpy(user_name.s+uri.user.len, "@", 1);
				memcpy(user_name.s+uri.user.len+1, uri.host.s, uri.host.len);
			}
			else
				memcpy(user_name.s, uri.host.s, uri.host.len);
		}

		if( (avp=AAACreateAVP(AVP_User_Name, 0, 0, user_name.s, 
							user_name.len, AVP_FREE_DATA)) == 0)
		{
			LM_ERR("no more pkg memory left!\n");
			if(user_name.len>0)
				pkg_free(user_name.s);
			goto error;
		}
		if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
		{
			LM_ERR("avp not added \n");
			goto error1;
		}
	}
	else /* it is a SIP message with credentials */
	{
		/* Add Username AVP */
		if (cred->username.domain.len>0) 
		{
			if( (avp=AAACreateAVP(AVP_User_Name, 0, 0, cred->username.whole.s,
							cred->username.whole.len, AVP_DUPLICATE_DATA)) == 0)
			{
				LM_ERR("no more pkg memory left!\n");
				goto error;
			}

			if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
			{
				LM_ERR("avp not added \n");
				goto error1;
			}
		}
		else 
		{
			user_name.s = 0;
			user_name.len = cred->username.user.len + cred->realm.len;
			if(user_name.len>0)
			{
				user_name.s = ad_malloc(user_name.len);
				if (!user_name.s) 
				{
					LM_ERR(" no more pkg memory left\n");
					goto error;
				}
				memcpy(user_name.s, cred->username.whole.s, 
									cred->username.whole.len);
				if(cred->username.whole.len>0)
				{
					user_name.s[cred->username.whole.len] = '@';
					memcpy(user_name.s + cred->username.whole.len + 1, 
							cred->realm.s, cred->realm.len);
				}
				else
					memcpy(user_name.s,	cred->realm.s, cred->realm.len);
			}

			if( (avp=AAACreateAVP(AVP_User_Name, 0, 0, user_name.s,	
							user_name.len, AVP_FREE_DATA)) == 0)
			{
				LM_ERR(" no more pkg memory left!\n");
				if(user_name.len>0)
					pkg_free(user_name.s);
				goto error;
			}

			if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
			{
				LM_ERR(" avp not added \n");
				goto error1;
			}
		}
	}

	/* SIP_MSGID AVP */
	LM_DBG("******* m_id=%d\n", m_id);
	tmp = m_id;
	if( (avp=AAACreateAVP(AVP_SIP_MSGID, 0, 0, (char*)(&tmp), 
				sizeof(m_id), AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR(" no more pkg memory left!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR(" avp not added \n");
		goto error1;
	}

	
	
	/* SIP Service AVP */
	if( (avp=AAACreateAVP(AVP_Service_Type, 0, 0, SIP_AUTHENTICATION, 
				SERVICE_LEN, AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR(" no more pkg memory left!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR(" avp not added \n");
		goto error1;
	}
		
	/* Destination-Realm AVP */
	if( (avp=AAACreateAVP(AVP_Destination_Realm, 0, 0, uri.host.s,
						uri.host.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR(" no more pkg memory left!\n");
		goto error;
	}

#ifdef DEBUG	
	LM_DBG("Destination Realm: %.*s\n", uri.host.len, uri.host.s);	
#endif

	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR(" avp not added \n");
		goto error1;
	}
	
	/* Resource AVP */
	user_name.len = ruri.user.len + ruri.host.len + ruri.port.len + 2;
	user_name.s = (char*)ad_malloc(user_name.len*sizeof(char));
	memset(user_name.s, 0, user_name.len);
	memcpy(user_name.s, ruri.user.s, ruri.user.len);

	name_flag= 0;
	if(ruri.user.s)
	{		
		name_flag = 1;
		memcpy(user_name.s+ruri.user.len, "@", 1);
	}	

	memcpy(user_name.s+ruri.user.len+name_flag, ruri.host.s, ruri.host.len);

	port_flag=0;
	if(ruri.port.s)
	{
		port_flag = 1;	
		memcpy(user_name.s+ruri.user.len+ruri.host.len+1, ":", 1);
		memcpy(user_name.s+ruri.user.len+ruri.host.len+name_flag+port_flag, 
					ruri.port.s, ruri.port.len);
	}
#ifdef DEBUG
	LM_DBG(": AVP_Resource=%.*s\n", user_name.len, user_name.s);
#endif

	if( (avp=AAACreateAVP(AVP_Resource, 0, 0, user_name.s,
						user_name.len, AVP_FREE_DATA)) == 0)
	{
		LM_ERR(" no more pkg memory left!\n");
		if(user_name.s)
			pkg_free(user_name.s);
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR(" avp not added \n");
		goto error1;
	}

	if(cred) /* it is a SIP message with credentials */
	{
		/* Response AVP */
		if( (avp=AAACreateAVP(AVP_Response, 0, 0, hdr->body.s,
						hdr->body.len, AVP_DUPLICATE_DATA)) == 0)
		{
			LM_ERR(" no more pkg memory left!\n");
			goto error;
		}
		
		position = AAAGetLastAVP(&(req->avpList));
		if( AAAAddAVPToMessage(req, avp, position)!= AAA_ERR_SUCCESS)
				
		{
			LM_ERR(" avp not added \n");
			goto error1;
		}

		/* Method AVP */
		if( (avp=AAACreateAVP(AVP_Method, 0, 0, p_method->s,
						p_method->len, AVP_DUPLICATE_DATA)) == 0)
		{
			LM_ERR(" no more pkg memory left!\n");
			goto error;
		}
		
		position = AAAGetLastAVP(&(req->avpList));
		if( AAAAddAVPToMessage(req, avp, position)!= AAA_ERR_SUCCESS)
				
		{
			LM_ERR(" avp not added \n");
			goto error1;
		}

	
	}			
#ifdef DEBUG
	AAAPrintMessage(req);
#endif

	/* build a AAA message buffer */
	if(AAABuildMsgBuffer(req) != AAA_ERR_SUCCESS)
	{
		LM_ERR(" message buffer not created\n");
		goto error;
	}
	
	if(sockfd==AAA_NO_CONNECTION)
	{
		sockfd = init_mytcp(diameter_client_host, diameter_client_port);
		if(sockfd==AAA_NO_CONNECTION)
		{
			LM_ERR(" failed to reconnect to Diameter client\n");
			goto error;
		}
	}

	/* send the message to the DIAMETER CLIENT */
	switch( tcp_send_recv(sockfd, req->buf.s, req->buf.len, rb, m_id) )
	{
		case AAA_ERROR: /* a transmission error occurred */
			LM_ERR(" message sending to the" 
					" DIAMETER backend authorization server failed\n");
			goto error;
	
		case AAA_CONN_CLOSED:
			LM_NOTICE("connection to Diameter"
					" client closed.It will be reopened by the next request\n");
			close(sockfd);
			sockfd = AAA_NO_CONNECTION;
			goto error;

		case AAA_TIMEOUT:
			LM_NOTICE("no response received\n");
			close(sockfd);
			sockfd = AAA_NO_CONNECTION;
			goto error;
	}

	AAAFreeMessage(&req);
	return 1;

error1:
	AAAFreeAVP(&avp);
error:
	AAAFreeMessage(&req);
	return -1;
}

/* give the appropriate response to the SER client */
int srv_response(struct sip_msg* msg, rd_buf_t * rb, int hftype)
{
	int auth_hf_len=0, ret=0;
	char* auth_hf;

	switch(rb->ret_code)
	{
		case AAA_AUTHORIZED:
			return 1;
			
		case AAA_NOT_AUTHORIZED:
			send_resp(msg, 403, &dia_403_err, NULL, 0);
			return -1;

		case AAA_SRVERR:
			send_resp(msg, 500, &dia_500_err, NULL, 0);
			return -1;
				
		case AAA_CHALENGE:
		 	if(hftype==HDR_AUTHORIZATION_T) /* SIP server */
			{
				auth_hf_len = WWW_AUTH_CHALLENGE_LEN+rb->chall_len;
				auth_hf = (char*)ad_malloc(auth_hf_len*(sizeof(char)));
				memset(auth_hf, 0, auth_hf_len);
				memcpy(auth_hf,WWW_AUTH_CHALLENGE, WWW_AUTH_CHALLENGE_LEN);
				memcpy(auth_hf+WWW_AUTH_CHALLENGE_LEN, rb->chall,
					rb->chall_len);
		
				ret = send_resp(msg, 401, &dia_401_err, auth_hf, auth_hf_len);

			}
			else	/* Proxy Server */
			{
				auth_hf_len = PROXY_AUTH_CHALLENGE_LEN+rb->chall_len;
				auth_hf = (char*)ad_malloc(auth_hf_len*(sizeof(char)));
				memset(auth_hf, 0, auth_hf_len);
				memcpy(auth_hf, PROXY_AUTH_CHALLENGE, PROXY_AUTH_CHALLENGE_LEN);
				memcpy(auth_hf + PROXY_AUTH_CHALLENGE_LEN, rb->chall, 
						rb->chall_len);
				ret = send_resp(msg, 407, &dia_407_err, auth_hf, auth_hf_len);
			}

			if (auth_hf) pkg_free(auth_hf);
	
			if (ret == -1) 
			{
				LM_ERR("failed to send challenge to the client of SER\n");
				return -1;
			}
			return -1;
	}
	
	// never reach this 
	return -1;		
}


/*
 * Create a response with given code and reason phrase
 * Optionally add new headers specified in _hdr
 */
int send_resp(struct sip_msg* m, int code, str* reason,
					char* hdr, int hdr_len)
{
	/* Add new headers if there are any */
	if ((hdr) && (hdr_len)) {
		if (add_lump_rpl( m, hdr, hdr_len, LUMP_RPL_HDR)==0) {
			LM_ERR("unable to append hdr\n");
			return -1;
		}
	}

	return slb.freply(m, code, reason);
}

















