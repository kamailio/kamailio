/*
 * $Id$
 *
 * Digest Authentication - Diameter support
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 * 
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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


/* Extract URI depending on the request from To or From header */
int get_uri(struct sip_msg* m, str** uri)
{
	if ((REQ_LINE(m).method.len == 8) && 
					(memcmp(REQ_LINE(m).method.s, "REGISTER", 8) == 0)) 
	{	
		/* REGISTER */
		if (!m->to && ((parse_headers(m, HDR_TO, 0) == -1)|| (!m->to))) 
		{
			LOG(L_ERR, M_NAME":get_uri(): To header field not found or "
				"malformed\n");
			
			/* it was a REGISTER and an error appeared when parsing TO header*/
			return -1;
		}
		*uri = &(get_to(m)->uri);
	} 
	else 
	{
		if (parse_from_header(m) == -1) 
		{
			LOG(L_ERR, M_NAME":get_uri(): Error while parsing FROM header\n");

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
					&& (hftype == HDR_AUTHORIZATION) ) 
	{ 
		/* REGISTER */
		if (!m->to && ((parse_headers(m, HDR_TO, 0) == -1) || (!m->to))) 
		{
			LOG(L_ERR, M_NAME":get_realm(): Error while parsing TO header\n");
			/* signal the errror */
			return -1;
		}
		
	    /* Body of To header field is parsed automatically */
		uri = get_to(m)->uri; 
	} 
	else 
	{
		if (parse_from_header(m)<0) 
		{
			LOG(L_ERR, M_NAME":get_realm(): Error while parsing FROM header\n");
			/* signal the error */
			return -1;
		}

		uri = get_from(m)->uri;
	}
	
	/* parsing the uri */
	if (parse_uri(uri.s, uri.len, u) < 0) 
	{
		LOG(L_ERR, M_NAME":get_realm(): Error while parsing URI\n");
		return -1;
	}
	
	/* everything was OK */
	return 0;
}

/* Find credentials with given realm in a SIP message header */
int find_credentials(struct sip_msg* _m, str* _realm, int _hftype, 
				struct hdr_field** _h)
{
	struct hdr_field** hook, *ptr, *prev;
	int res;
	str* r;
      
	switch(_hftype) 
	{
		case HDR_AUTHORIZATION: hook = &(_m->authorization); break;
		case HDR_PROXYAUTH:     hook = &(_m->proxy_auth);    break;
		default:                hook = &(_m->authorization); break;
	}

	     
     /* If the credentials haven't been parsed yet, do it now */
	if (*hook == 0) 
		if (parse_headers(_m, _hftype, 0) == -1) 
		{
			LOG(L_ERR, M_NAME":find_credentials(): Error while parsing headers\n");
			return -1;
		}
	
	ptr = *hook;

	/* Iterate through the credentials of the message to find 
		credentials with given realm 
	*/
	while(ptr) 
	{
		res = parse_credentials(ptr);
		if (res < 0) 
		{
			LOG(L_ERR, M_NAME":find_credentials(): Error while parsing "
				"credentials\n");
			return (res == -1) ? -2 : -3;
		}
		else 
			if (res == 0) 
			{
				r = &(((auth_body_t*)(ptr->parsed))->digest.realm);
	
				if (r->len == _realm->len) 
				{
					if (!strncasecmp(_realm->s, r->s, r->len)) 
					{
						*_h = ptr;
						return 0;
					}
				}
			}
			
			prev = ptr;
			if (parse_headers(_m, _hftype, 1) == -1) 
			{
				LOG(L_ERR, M_NAME":find_credentials(): Error while parsing"
					" headers\n");
				return -4;
			}
			else 
			{	
				if (prev != _m->last_header) 
				{
					if (_m->last_header->type == _hftype) ptr = _m->last_header;
					else break;
				} 
				else break;
			}
	}
	     
     /* Credentials with given realm not found */
	return 1;
}


auth_result_t pre_auth(struct sip_msg* _m, str* _realm, int _hftype, struct hdr_field** _h)
{
	int ret;
	struct sip_uri uri;

	if ((_m->REQ_METHOD == METHOD_ACK) ||  (_m->REQ_METHOD == METHOD_CANCEL))
		return AUTHORIZED;

	/* if no realm supplied, find out now */
	if (_realm==0 || _realm->len == 0) 
	{
		if (get_realm(_m, _hftype, &uri) < 0) 
		{
			LOG(L_ERR, M_NAME":pre_auth(): Error while extracting realm\n");
			if (send_resp(_m, 400, MESSAGE_400, 0, 0) == -1) 
			{
				LOG(L_ERR, M_NAME":pre_auth(): Error while sending 400 reply\n");
			}
			return ERROR;
		}
		
		*_realm = uri.host;
	}

	ret = find_credentials(_m, _realm, _hftype, _h);
	if (ret < 0) 
	{
		LOG(L_ERR, M_NAME":pre_auth(): Error while looking for credentials\n");
		if (send_resp(_m, (ret == -2) ? 500 : 400, 
			      (ret == -2) ? MESSAGE_500 : MESSAGE_400, 0, 0) == -1) 
		{
			LOG(L_ERR, M_NAME":pre_auth(): Error while sending 400 reply\n");
		}
		return ERROR;
	} 
	else 
		if (ret > 0) 
		{
			LOG(L_ERR, M_NAME":pre_auth(): Credentials with given realm not "
				"found\n");
			return NO_CREDENTIALS;
		}
	

	return DO_AUTHORIZATION;
}


/* Authorize digest credentials */
int authorize(struct sip_msg* msg, str* realm, int hftype)
{
	auth_result_t ret;
	struct hdr_field* h;
	auth_body_t* cred = NULL;
	str* uri;
	struct sip_uri puri;
	str  domain;

	domain = *realm;

	/* see what is to do after a first look at the message */
	ret = pre_auth(msg, &domain, hftype, &h);
	
	switch(ret) 
	{
		case ERROR:            return 0;
			   
		case AUTHORIZED:       return 1;

		case NO_CREDENTIALS:   cred = NULL;
							   break;

		case DO_AUTHORIZATION: cred = (auth_body_t*)h->parsed;
							   break;
	}

	if (get_uri(msg, &uri) < 0) 
	{
		LOG(L_ERR, M_NAME":authorize(): From/To URI not found\n");
		return -1;
	}
	
	if (parse_uri(uri->s, uri->len, &puri) < 0) 
	{
		LOG(L_ERR, M_NAME":authorize(): Error while parsing From/To URI\n");
		return -1;
	}
//	user.s = (char *)pkg_malloc(puri.user.len);
//	un_escape(&(puri.user), &user);
	
	/* parse the ruri, if not yet */
	if(msg->parsed_uri_ok==0 && parse_sip_msg_uri(msg)<0)
	{
		LOG(L_ERR,M_NAME":authorize(): ERROR while parsing the Request-URI\n");
		return -1;
	}
	
	/* preliminary check */
	if(cred)
	{
		if (puri.host.len != cred->digest.realm.len) 
		{
			DBG(M_NAME":authorize(): Credentials realm and URI host do not "
				"match\n");  
			return -1;
		}
	
		if (strncasecmp(puri.host.s, cred->digest.realm.s, puri.host.len) != 0) 
		{
			DBG(M_NAME":authorize(): Credentials realm and URI host do not "
				"match\n");
			return -1;
		}
	}
	
	if( diameter_authorize(cred?h:NULL, &msg->first_line.u.request.method,
					puri, msg->parsed_uri, msg->id, rb) != 1)
	{
		send_resp(msg, 500, "Internal Server Error", NULL, 0);	
		return -1;
	}
	
	if( srv_response(msg, rb, hftype) != 1 )
		return -1;

	mark_authorized_cred(msg, h);

	return 1;
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
	str method, user_name;
	AAAMessage *req;
	AAA_AVP *avp, *position; 
	int name_flag, port_flag;
	dig_cred_t* cred;
	unsigned int tmp;

	if ( !p_method )
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): Invalid parameter value\n");
		return -1;
	}

	if ( (req=AAAInMessage(AA_REQUEST, AAA_APP_NASREQ))==NULL)
		return -1;

	if(hdr && hdr->parsed)
		cred = &(((auth_body_t*)hdr->parsed)->digest);
	else
		cred = NULL;
			
	method = *p_method;

	if(!cred)
	{
		/* Username AVP */
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
			LOG(L_ERR,M_NAME":diameter_authorize(): no more free memory!\n");
			if(user_name.len>0)
				pkg_free(user_name.s);
			goto error;
		}
		if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
		{
			LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
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
				LOG(L_ERR, M_NAME":diameter_authorize(): no more free "
					"memory!\n");
				goto error;
			}

			if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
			{
				LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
				goto error1;
			}
		}
		else 
		{
			user_name.len = cred->username.user.len + cred->realm.len;
			if(user_name.len>0)
			{
				user_name.s = ad_malloc(user_name.len);
				if (!user_name.s) 
				{
					LOG(L_ERR, M_NAME":diameter_authorize(): no more free "
						"memory\n");
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
				LOG(L_ERR, M_NAME":diameter_authorize(): no more free "
					"memory!\n");
				if(user_name.len>0)
					pkg_free(user_name.s);
				goto error;
			}

			if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
			{
				LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
				goto error1;
			}
		}
	}

	/* SIP_MSGID AVP */
	DBG("******* m_id=%d\n", m_id);
	tmp = m_id;
	if( (avp=AAACreateAVP(AVP_SIP_MSGID, 0, 0, (char*)(&tmp), 
				sizeof(m_id), AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
		goto error1;
	}

	
	
	/* SIP Service AVP */
	if( (avp=AAACreateAVP(AVP_Service_Type, 0, 0, SIP_AUTHENTICATION, 
				SERVICE_LEN, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
		goto error1;
	}
		
	/* Destination-Realm AVP */
	if( (avp=AAACreateAVP(AVP_Destination_Realm, 0, 0, uri.host.s,
						uri.host.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): no more free memory!\n");
		goto error;
	}

#ifdef DEBUG	
	DBG("Destination Realm: %.*s\n", uri.host.len, uri.host.s);	
#endif

	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
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
	}	
	memcpy(user_name.s+ruri.user.len+ruri.host.len+name_flag+port_flag, 
					ruri.port.s, ruri.port.len);
#ifdef DEBUG
	DBG(M_NAME": AVP_Resource=%.*s\n", user_name.len, user_name.s);
#endif

	if( (avp=AAACreateAVP(AVP_Resource, 0, 0, user_name.s,
						user_name.len, AVP_FREE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): no more free memory!\n");
		if(user_name.s)
			pkg_free(user_name.s);
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
		goto error1;
	}

	if(cred) /* it is a SIP message with credentials */
	{
		/* Response AVP */
		if( (avp=AAACreateAVP(AVP_Response, 0, 0, hdr->body.s,
						hdr->body.len, AVP_DUPLICATE_DATA)) == 0)
		{
			LOG(L_ERR, M_NAME":diameter_authorize(): no more free memory!\n");
			goto error;
		}
		
		position = AAAGetLastAVP(&(req->avpList));
		if( AAAAddAVPToMessage(req, avp, position)!= AAA_ERR_SUCCESS)
				
		{
			LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
			goto error1;
		}

		/* Method AVP */
		if( (avp=AAACreateAVP(AVP_Method, 0, 0, p_method->s,
						p_method->len, AVP_DUPLICATE_DATA)) == 0)
		{
			LOG(L_ERR, M_NAME":diameter_authorize(): no more free memory!\n");
			goto error;
		}
		
		position = AAAGetLastAVP(&(req->avpList));
		if( AAAAddAVPToMessage(req, avp, position)!= AAA_ERR_SUCCESS)
				
		{
			LOG(L_ERR, M_NAME":diameter_authorize(): avp not added \n");
			goto error1;
		}

	
	}			
#ifdef DEBUG
	AAAPrintMessage(req);
#endif

	/* build a AAA message buffer */
	if(AAABuildMsgBuffer(req) != AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_authorize(): message buffer not created\n");
		goto error;
	}
	
	if(sockfd==AAA_NO_CONNECTION)
	{
		sockfd = init_mytcp(diameter_client_host, diameter_client_port);
		if(sockfd==AAA_NO_CONNECTION)
		{
			LOG(L_ERR, M_NAME":diameter_authorize(): failed to reconnect"
								" to Diameter client\n");
			goto error;
		}
	}

	/* send the message to the DIAMETER CLIENT */
	switch( tcp_send_recv(sockfd, req->buf.s, req->buf.len, rb, m_id) )
	{
		case AAA_ERROR: /* a transmission error occured */
			LOG(L_ERR, M_NAME":diameter_authorize(): message sending to the" 
						" DIAMETER backend authorization server failed\n");
			goto error;
	
		case AAA_CONN_CLOSED:
			LOG(L_NOTICE, M_NAME":diameter_authorize(): connection to Diameter"
					" client closed.It will be reopened by the next request\n");
			close(sockfd);
			sockfd = AAA_NO_CONNECTION;
			goto error;

		case AAA_TIMEOUT:
			LOG(L_NOTICE,M_NAME":diameter_authorize(): no response received\n");
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
			send_resp(msg, 403, "Forbidden", NULL, 0);	
			return -1;

		case AAA_SRVERR:
			send_resp(msg, 500, "Internal Server Error", NULL, 0);	
			return -1;
				
		case AAA_CHALENGE:
		 	if(hftype==HDR_AUTHORIZATION) /* SIP server */
			{
				auth_hf_len = WWW_AUTH_CHALLENGE_LEN+rb->chall_len;
				auth_hf = (char*)ad_malloc(auth_hf_len*(sizeof(char)));
				memset(auth_hf, 0, auth_hf_len);
				memcpy(auth_hf,WWW_AUTH_CHALLENGE, WWW_AUTH_CHALLENGE_LEN);
				memcpy(auth_hf+WWW_AUTH_CHALLENGE_LEN, rb->chall,
					rb->chall_len);
		
				ret = send_resp(msg, 401, MESSAGE_401, auth_hf, auth_hf_len);

			}
			else	/* Proxy Server */
			{
				auth_hf_len = PROXY_AUTH_CHALLENGE_LEN+rb->chall_len;
				auth_hf = (char*)ad_malloc(auth_hf_len*(sizeof(char)));
				memset(auth_hf, 0, auth_hf_len);
				memcpy(auth_hf, PROXY_AUTH_CHALLENGE, PROXY_AUTH_CHALLENGE_LEN);
				memcpy(auth_hf + PROXY_AUTH_CHALLENGE_LEN, rb->chall, 
						rb->chall_len);
				ret = send_resp(msg, 407, MESSAGE_407, auth_hf, auth_hf_len);
			}

			if (auth_hf) pkg_free(auth_hf);
	
			if (ret == -1) 
			{
				LOG(L_ERR, M_NAME":srv_response():Error while sending chalenge "
					"to the client of SER\n");
				return -1;
			}
			return -1;
	}
	
	// never reach this 
	return -1;		
}


/*
 * Create a response with given code and reason phrase
 * Optionaly add new headers specified in _hdr
 */
int send_resp(struct sip_msg* m, int code, char* reason,
					char* hdr, int hdr_len)
{
	/* Add new headers if there are any */
	if ((hdr) && (hdr_len)) {
		if (add_lump_rpl( m, hdr, hdr_len, LUMP_RPL_HDR)==0) {
			LOG(L_ERR,"ERROR:auth_diamter:send_resp: unable to append hdr\n");
			return -1;
		}
	}

	return sl_reply(m, (char*)(long)code, reason);
}

















