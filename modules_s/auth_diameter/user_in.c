/*
 * $Id$
 *
 * Digest Authentication - Diameter support
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *  
 *  
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


/* headers defined by this module */
#include "diameter_msg.h"
#include "auth_diameter.h"
#include "defs.h"
#include "tcp_comm.h"


/* Get actual Request-URI */
static inline int get_request_uri(struct sip_msg* m, str* u)
{
     /* Use new_uri if present */
	if (m->new_uri.s)
	{
		u->s   = m->new_uri.s;
		u->len = m->new_uri.len;
		return 0;
	} 
	u->s = m->first_line.u.request.uri.s;
	u->len = m->first_line.u.request.uri.len;

	return 0;
}


/* Get To header field URI */
static inline int get_to_uri(struct sip_msg* m, str* u)
{
     // check that the header field is there and is parsed
	if (!m->to && ((parse_headers(m, HDR_TO, 0) == -1)|| (!m->to))) 
	{
		LOG(L_ERR, "get_to_uri(): Can't get To header field\n");
		return -1;
	}
	
	u->s   = ((struct to_body*)m->to->parsed)->uri.s;
	u->len = ((struct to_body*)m->to->parsed)->uri.len;
	
	return 0;
}


/* Get From header field URI */
static inline int get_from_uri(struct sip_msg* m, str* u)
{
     // check that the header field is there and is parsed
	if (parse_from_header(m) < 0) {
		LOG(L_ERR, "get_from_uri(): Error while parsing From body\n");
		return -1;
	}
	
	u->s   = ((struct to_body*)m->from->parsed)->uri.s;
	u->len = ((struct to_body*)m->from->parsed)->uri.len;

	return 0;
}

/* it checks if a user is member of a group */
int diameter_is_user_in(struct sip_msg* _m, char* _hf, char* _group)
{
	str *grp, user_name, user, domain, uri;
	dig_cred_t* cred = 0;
	int hf_type;
	struct hdr_field* h;
	struct sip_uri puri;
	AAAMessage *req;
	AAA_AVP *avp; 
	int ret;
	unsigned int tmp;

	grp = (str*)_group; /* via fixup */

	hf_type = (int)(long)_hf;

	/* extract the uri according with the _hf parameter */
	switch(hf_type) 
	{
		case 1: /* Request-URI */
			if (get_request_uri(_m, &uri) < 0) 
			{
				LOG(L_ERR, M_NAME":diameter_is_user_in(): Error while "
					"extracting Request-URI\n");
				return -1;
			}
		break;

		case 2: /* To */
			if (get_to_uri(_m, &uri) < 0) 
			{
				LOG(L_ERR, M_NAME":diameter_is_user_in(): Error while "
					"extracting To\n");
				return -2;
			}
			break;

		case 3: /* From */
			if (get_from_uri(_m, &uri) < 0) 
			{
				LOG(L_ERR, M_NAME":diameter_is_user_in(): Error while "
					"extracting From\n");
				return -3;
			}
			break;

		case 4: /* Credentials */
			get_authorized_cred(_m->authorization, &h);
			if (!h) 	
			{
				get_authorized_cred(_m->proxy_auth, &h);
				if (!h) 
				{
					LOG(L_ERR, M_NAME":diameter_is_user_in(): No authorized "
								"credentials found (error in scripts)\n");
					return -4;
				}
			}
			cred = &((auth_body_t*)(h->parsed))->digest;
			break;
	}

	if (hf_type != 4) 
	{
		if (parse_uri(uri.s, uri.len, &puri) < 0) 
		{
			LOG(L_ERR, M_NAME":diameter_is_user_in(): Error while "
				"parsing URI\n");
			return -5;
		}
		user = puri.user;
		domain = puri.host;
	} 
	else
	{
		user = cred->username.user;
		domain = cred->realm;
	}
	
	/* user@domain mode */
	if (use_domain)
	{
		user_name.len = user.len + domain.len;
		if(user_name.len>0)
		{
			user_name.len++;
			user_name.s = (char*)pkg_malloc(user_name.len);
			if (!user_name.s) 
			{
				LOG(L_ERR, M_NAME":diameter_is_user_in(): No memory left\n");
				return -6;
			}
		
			memcpy(user_name.s, user.s, user.len);
			if(user.len>0)
			{
				user_name.s[user.len] = '@';
				memcpy(user_name.s + user.len + 1, domain.s, domain.len);
			}
			else
				memcpy(user_name.s, domain.s, domain.len);
		}
	} 
	else 
		user_name = user;
	
	
	if ( (req=AAAInMessage(AA_REQUEST, AAA_APP_NASREQ))==NULL)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in():can't create new "
			"AAA message!\n");
		return -1;
	}
	
	/* Username AVP */
	if( (avp=AAACreateAVP(AVP_User_Name, 0, 0, user_name.s,
				user_name.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): avp not added \n");
		goto error1;
	}

	/* Usergroup AVP */
	if( (avp=AAACreateAVP(AVP_User_Group, 0, 0, grp->s,
				grp->len, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): avp not added \n");
		goto error1;
	}

	/* SIP_MSGID AVP */
	DBG("******* m_id=%d\n", _m->id);
	tmp = _m->id;
	if( (avp=AAACreateAVP(AVP_SIP_MSGID, 0, 0, (char*)(&tmp), 
				sizeof(tmp), AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): avp not added \n");
		goto error1;
	}

	
	/* ServiceType AVP */
	if( (avp=AAACreateAVP(AVP_Service_Type, 0, 0, SIP_GROUP_CHECK, 
				SERVICE_LEN, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR,M_NAME":diameter_is_user_in(): no more free memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): avp not added \n");
		goto error1;
	}
	

	/* Destination-Realm AVP */
	get_request_uri(_m, &uri);
	parse_uri(uri.s, uri.len, &puri);
	if( (avp=AAACreateAVP(AVP_Destination_Realm, 0, 0, puri.host.s,
						puri.host.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): no more free memory!\n");
		goto error;
	}
	
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): avp not added \n");
		goto error1;
	}
	
#ifdef DEBUG
	AAAPrintMessage(req);
#endif

	/* build a AAA message buffer */
	if(AAABuildMsgBuffer(req) != AAA_ERR_SUCCESS)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in():message buffer not created\n");
		goto error;
	}

	if(sockfd==AAA_NO_CONNECTION)
	{
		sockfd = init_mytcp(diameter_client_host, diameter_client_port);
		if(sockfd==AAA_NO_CONNECTION)
		{
			LOG(L_ERR, M_NAME":diameter_is_user_in((): failed to reconnect"
								" to Diameter client\n");
			goto error;
		}
	}

	ret =tcp_send_recv(sockfd, req->buf.s, req->buf.len, rb, _m->id);

	if(ret == AAA_CONN_CLOSED)
	{
		LOG(L_NOTICE, M_NAME":diameter_is_user_in((): connection to Diameter"
					" client closed.It will be reopened by the next request\n");
		close(sockfd);
		sockfd = AAA_NO_CONNECTION;
		goto error;
	}
	if(ret != AAA_USER_IN_GROUP)
	{
		LOG(L_ERR, M_NAME":diameter_is_user_in(): message sending to the" 
					"DIAMETER backend authorization server failed or "
					"user is not in group\n");
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

