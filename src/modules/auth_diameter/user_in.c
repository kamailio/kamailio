/*
 * $Id$
 *
 * Digest Authentication - Diameter support
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
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


/* Get To header field URI */
static inline int get_to_uri(struct sip_msg* m, str* u)
{
     // check that the header field is there and is parsed
	if (!m->to && ((parse_headers(m, HDR_TO_F, 0) == -1)|| (!m->to))) 
	{
		LM_ERR("can't get To header field\n");
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
		LM_ERR("failed to parse From body\n");
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
	char *p = NULL;

	grp = (str*)_group; /* via fixup */

	hf_type = (int)(long)_hf;

	uri.s = 0;
	uri.len = 0;

	/* extract the uri according with the _hf parameter */
	switch(hf_type) 
	{
		case 1: /* Request-URI */
			uri = *(GET_RURI(_m));
		break;

		case 2: /* To */
			if (get_to_uri(_m, &uri) < 0) 
			{
				LM_ERR("failed to extract To\n");
				return -2;
			}
			break;

		case 3: /* From */
			if (get_from_uri(_m, &uri) < 0) 
			{
				LM_ERR("failed to extract From URI\n");
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
					LM_ERR("no authorized credentials found "
							"(error in scripts)\n");
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
			LM_ERR("failed to parse URI\n");
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
		user_name.s = 0;
		user_name.len = user.len + domain.len;
		if(user_name.len>0)
		{
			user_name.len++;
			p = (char*)pkg_malloc(user_name.len);
			if (!p)
			{
				LM_ERR("no pkg memory left\n");
				return -6;
			}
			user_name.s = p;
		
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
		LM_ERR("can't create new AAA message!\n");
		if(p) pkg_free(p);
		return -1;
	}
	
	/* Username AVP */
	if( (avp=AAACreateAVP(AVP_User_Name, 0, 0, user_name.s,
				user_name.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR("no more pkg memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR("avp not added \n");
		goto error1;
	}

	/* Usergroup AVP */
	if( (avp=AAACreateAVP(AVP_User_Group, 0, 0, grp->s,
				grp->len, AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR("no more pkg memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR("avp not added \n");
		goto error1;
	}

	/* SIP_MSGID AVP */
	LM_DBG("******* m_id=%d\n", _m->id);
	tmp = _m->id;
	if( (avp=AAACreateAVP(AVP_SIP_MSGID, 0, 0, (char*)(&tmp), 
				sizeof(tmp), AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR("no more pkg memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR("avp not added \n");
		goto error1;
	}

	
	/* ServiceType AVP */
	if( (avp=AAACreateAVP(AVP_Service_Type, 0, 0, SIP_GROUP_CHECK, 
				SERVICE_LEN, AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR("no more pkg memory!\n");
		goto error;
	}
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR("avp not added \n");
		goto error1;
	}
	

	/* Destination-Realm AVP */
	uri = *(GET_RURI(_m));
	parse_uri(uri.s, uri.len, &puri);
	if( (avp=AAACreateAVP(AVP_Destination_Realm, 0, 0, puri.host.s,
						puri.host.len, AVP_DUPLICATE_DATA)) == 0)
	{
		LM_ERR("no more pkg memory!\n");
		goto error;
	}
	
	if( AAAAddAVPToMessage(req, avp, 0)!= AAA_ERR_SUCCESS)
	{
		LM_ERR("avp not added \n");
		goto error1;
	}
	
#ifdef DEBUG
	AAAPrintMessage(req);
#endif

	/* build a AAA message buffer */
	if(AAABuildMsgBuffer(req) != AAA_ERR_SUCCESS)
	{
		LM_ERR("message buffer not created\n");
		goto error;
	}

	if(sockfd==AAA_NO_CONNECTION)
	{
		sockfd = init_mytcp(diameter_client_host, diameter_client_port);
		if(sockfd==AAA_NO_CONNECTION)
		{
			LM_ERR("failed to reconnect to Diameter client\n");
			goto error;
		}
	}

	ret =tcp_send_recv(sockfd, req->buf.s, req->buf.len, rb, _m->id);

	if(ret == AAA_CONN_CLOSED)
	{
		LM_NOTICE("connection to Diameter client closed."
				"It will be reopened by the next request\n");
		close(sockfd);
		sockfd = AAA_NO_CONNECTION;
		goto error;
	}
	if(ret != AAA_USER_IN_GROUP)
	{
		LM_ERR("message sending to the DIAMETER backend authorization server"
				"failed or user is not in group\n");
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

