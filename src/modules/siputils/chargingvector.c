/*
 * Copyright (C) 2016 kamailio.org
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
 */

#include <string.h>
#include <netdb.h>

#include "../../core/mod_fix.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/mem/mem.h"
#include "../../core/str.h"
#include "../../core/strutils.h"
#include "chargingvector.h"

#define SIZE_CONF_ID 16
#define P_CHARGING_VECTOR "P-Charging-Vector"
#define LOOPBACK_IP 16777343

#define PCV_BUF_SIZE 256
static char pcv_buf[PCV_BUF_SIZE];
static str pcv = { pcv_buf, 0 };
static str pcv_id = { NULL, 0 };
static str pcv_host = { NULL, 0 };
static str pcv_orig = { NULL, 0 };
static str pcv_term = { NULL, 0 };
static uint64_t counter = 0;


enum PCV_Status {
	PCV_NONE = 0,
	PCV_PARSED = 1,
	PCV_GENERATED = 2
};

static enum PCV_Status pcv_status = PCV_NONE;
static unsigned int current_msg_id = (unsigned int)-1;

static void sip_generate_charging_vector(char * pcv)
{
	char             s[PATH_MAX]        = {0};
	struct hostent*  host               = NULL;
	int              cdx                = 0;
	int              tdx                = 0;
	int              idx                = 0;
	int              ipx                = 0;
	int pid;
	uint64_t         ct                 = 0;
	struct in_addr*  in                 = NULL;
	static struct in_addr ip            = {0};
	unsigned char newConferenceIdentifier[SIZE_CONF_ID]={0};

	memset(pcv,0,SIZE_CONF_ID);
	pid = getpid();

	if ( ip.s_addr  == 0  )
	{
		if (!gethostname(s, PATH_MAX))
		{
			if ((host = gethostbyname(s)) != NULL)
			{
				int idx = 0 ;
				for (idx = 0 ; host->h_addr_list[idx]!=NULL ;  idx++)
				{
					in = (struct in_addr*)host->h_addr_list[idx];
					if (in->s_addr == LOOPBACK_IP )
					{
						if ( ip.s_addr == 0 )
						{
							ip=*in;
						}
					}
					else
					{
						ip=*in;
					}
				}
			}
		}
	}

	ct=counter++;
	if ( counter > 0xFFFFFFFF ) counter=0;

	memset(newConferenceIdentifier,0,SIZE_CONF_ID);
	newConferenceIdentifier[0]='I';
	newConferenceIdentifier[1]='V';
	newConferenceIdentifier[2]='S';
	idx=3;
	while ( idx < SIZE_CONF_ID )
	{
		if ( idx < 7 )
		{
			// 3-6 =IP
			newConferenceIdentifier[idx]=((ip.s_addr>>(ipx*8))&0xff);
			ipx++;
		}
		else if (idx < 11 )
		{
			// 7-11 = PID
			newConferenceIdentifier[idx]=((pid>>(cdx*8))&0xff);
			cdx++;
		}
		else if (idx == 11 )
		{
			time_t ts = time(NULL);
			newConferenceIdentifier[idx]=(ts&0xff);
		}
		else
		{
			// 12-16 = COUNTER
			newConferenceIdentifier[idx]=((ct>>(tdx*8))&0xff);
			tdx++;
		}
		idx++;
	}
	LM_DBG("PCV generate\n");
	int i = 0;
	pcv[0] = '\0';
	for ( i = 0 ; i < SIZE_CONF_ID ; i ++ )
	{
		char hex[4] = {0};

		snprintf(hex,4,"%02X",newConferenceIdentifier[i]);
		strcat(pcv,hex);
	}
}

static unsigned int sip_param_end(const char * s, unsigned int len)
{
	unsigned int i;

	for (i=0; i<len; i++)
	{
		if (s[i] == '\0' || s[i] == ' ' || s[i] == ';' || s[i] == ',' ||
				s[i] == '\r' || s[i] == '\n' )
		{
			return i;
		}
	}
	return len;
}

static int sip_parse_charging_vector(const char * pcv_value, unsigned int len)
{
	/* now point to each PCV component */
	LM_DBG("parsing PCV header [%s]\n", pcv_value);

	char *s = NULL;

	s = strstr(pcv_value, "icid-value=");
	if (s != NULL)
	{
		pcv_id.s = s + strlen("icid-value=");
		pcv_id.len = sip_param_end(pcv_id.s, len);
		LM_DBG("parsed P-Charging-Vector icid-value=%.*s\n",
				pcv_id.len, pcv_id.s );
	}
	else
	{
		LM_WARN("mandatory icid-value not found\n");
		pcv_id.s = NULL;
		pcv_id.len = 0;
	}

	s = strstr(pcv_value, "icid-generated-at=");
	if (s != NULL)
	{
		pcv_host.s = s + strlen("icid-generated-at=");
		pcv_host.len = sip_param_end(pcv_host.s, len);
		LM_DBG("parsed P-Charging-Vector icid-generated-at=%.*s\n",
				pcv_host.len, pcv_host.s );
	}
	else
	{
		LM_DBG("icid-generated-at not found\n");
		pcv_host.s = NULL;
		pcv_host.len = 0;
	}

	s = strstr(pcv_value, "orig-ioi=");
	if (s != NULL)
	{
		pcv_orig.s = s + strlen("orig-ioi=");
		pcv_orig.len = sip_param_end(pcv_orig.s, len);
		LM_INFO("parsed P-Charging-Vector orig-ioi=%.*s\n",
				pcv_orig.len, pcv_orig.s );
	}
	else
	{
		pcv_orig.s = NULL;
		pcv_orig.len = 0;
	}

	s = strstr(pcv_value, "term-ioi=");
	if (s != NULL)
	{
		pcv_term.s = s + strlen("term-ioi=");
		pcv_term.len = sip_param_end(pcv_term.s, len);
		LM_INFO("parsed P-Charging-Vector term-ioi=%.*s\n",
				pcv_term.len, pcv_term.s );
	}
	else
	{
		pcv_term.s = NULL;
		pcv_term.len = 0;
	}

	// only icid-value is mandatory, log anyway when missing icid-generated-at
	if ( pcv_host.s == NULL && pcv_id.s != NULL && len > 0)
	{
		LM_WARN("icid-generated-at is missing %.*s\n", len, pcv_value);
	}

	return (pcv_id.s != NULL);
}

static int sip_get_charging_vector(struct sip_msg *msg, struct hdr_field ** hf_pcv )
{
	struct hdr_field *hf;
	char * hdrname_cstr = P_CHARGING_VECTOR;
	str hdrname = { hdrname_cstr , strlen( hdrname_cstr) };

	/* we need to be sure we have parsed all headers */
	if (parse_headers(msg, HDR_EOH_F, 0)<0)
	{
		LM_ERR("error parsing headers\n");
		return -1;
	}

	for (hf=msg->headers; hf; hf=hf->next)
	{
		if ( hf->name.s[0] != 'P' )
		{
			continue;
		}

		if ( cmp_hdrname_str(&hf->name, &hdrname) == 0)
		{
			/*
			 * append p charging vector values after the header name "P-Charging-Vector" and
			 * the ": " (+2)
			 */
			char * pcv_body = pcv_buf + strlen(P_CHARGING_VECTOR) + 2;

			if (hf->body.len > 0)
			{
				memcpy( pcv_body, hf->body.s, hf->body.len );
				pcv.len = hf->body.len + strlen(P_CHARGING_VECTOR) + 2;
				pcv_body[hf->body.len]= '\0';
				if ( sip_parse_charging_vector( pcv_body, hf->body.len ) == 0)
				{
					LM_ERR("P-Charging-Vector header found but failed to parse value [%s].\n", pcv_body);
					pcv_status = PCV_NONE;
					pcv.s = NULL;
					pcv.len = 0;
				}
				else
				{
					pcv_status = PCV_PARSED;
					pcv.s = hf->body.s;
					pcv.len = hf->body.len;
				}
				return 2;
			}
			else
			{
				pcv_id.s = 0;
				pcv_id.len = 0;
				pcv_host.s = 0;
				pcv_host.len = 0;
				LM_WARN("P-Charging-Vector header found but no value.\n");
			}
			*hf_pcv = hf;
		}
	}
	LM_DBG("No valid P-Charging-Vector header found.\n");
	return 1;
}

// Remove PCV if it is in the inbound request (if it was found by sip_get_charging_vector)
static int  sip_remove_charging_vector(struct sip_msg *msg, struct hdr_field *hf)
{
	struct lump* l;

	if ( hf != NULL )
	{
		l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
		if (l==0)
		{
			LM_ERR("no memory\n");
			return -1;
		}
		return 2;
	}
	else
	{
		return 1;
	}
}

static int sip_add_charging_vector(struct sip_msg *msg)
{
	struct lump* anchor;
	char * s;

	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if(anchor == 0)
	{
		LM_ERR("can't get anchor\n");
		return -1;
	}

	s = (char*)pkg_malloc(pcv.len);
	if (!s) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(s, pcv.s, pcv.len );

	if (insert_new_lump_before(anchor, s, pcv.len, 0) == 0)
	{
		LM_ERR("can't insert lump\n");
		pkg_free(s);
		return -1;
	}
	return 1;
}

int sip_handle_pcv(struct sip_msg *msg, char *flags, char *str2)
{
	int generate_pcv = 0;
	int remove_pcv = 0;
	int replace_pcv = 0;
	int i;
	str flag_str;
	struct hdr_field * hf_pcv = NULL;

	pcv.len = 0;
	pcv_status = PCV_NONE;

	if(fixup_get_svalue(msg, (gparam_p)flags, &flag_str)<0) {
		LM_ERR("failed to retrieve parameter value\n");
		return -1;
	}

	// Process command flags
	for (i = 0; i < flag_str.len; i++)
	{
		switch (flag_str.s[i])
		{
			case 'r':
			case 'R':
				remove_pcv = 1;
				break;

			case 'g':
			case 'G':
				generate_pcv = 1;
				break;

			case 'f':
			case 'F':
				replace_pcv = 1;
				generate_pcv = 1;
				break;

			default:
				break;
		}
	}

	sip_get_charging_vector(msg, &hf_pcv);

	/*
	 * We need to remove the original PCV if it was present and either
	 * we were asked to remove it or we were asked to replace it
	 */
	if ( pcv_status == PCV_PARSED && (replace_pcv || remove_pcv)  )
	{
		i = sip_remove_charging_vector(msg, hf_pcv);
		if (i <= 0) return (i==0)?-1:i;
	}

	/* Generate PCV if
	 * - we were asked to generate it and it could not be obtained from the inbound packet
	 * - or if we were asked to replace it alltogether regardless its former value
	 */
	if ( replace_pcv || (generate_pcv && pcv_status != PCV_GENERATED && pcv_status != PCV_PARSED ) )
	{
		strcpy(pcv_buf, P_CHARGING_VECTOR);
		strcat(pcv_buf, ": ");

		char * pcv_body = pcv_buf  + 19;
		char pcv_value[40];

		/* We use the IP address of the interface that received the message as generated-at */
		if(msg->rcv.bind_address==NULL || msg->rcv.bind_address->address_str.s==NULL)
		{
			LM_ERR("No IP address for message. Failed to generate charging vector.\n");
			return -2;
		}

		sip_generate_charging_vector(pcv_value);

		pcv.len = snprintf( pcv_body, PCV_BUF_SIZE - 19, "icid-value=%.*s; icid-generated-at=%.*s\r\n", 32, pcv_value,
				msg->rcv.bind_address->address_str.len,
				msg->rcv.bind_address->address_str.s );
		pcv.len += 19;

		pcv_status = PCV_GENERATED;

		/* if generated, reparse it */
		sip_parse_charging_vector( pcv_body, pcv.len-19 );
		/* if it was generated, we need to send it out as a header */
		LM_INFO("Generated PCV header %.*s\n", pcv.len-2, pcv_buf );
		i = sip_add_charging_vector(msg);
		if (i <= 0)
		{
			LM_ERR("Failed to add P-Charging-Vector header\n");
			return (i==0)?-1:i;
		}
	}

	current_msg_id = msg->id;
	return 1;
}


int pv_get_charging_vector(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str pcv_pv;

	if ( current_msg_id != msg->id || pcv_status == PCV_NONE )
	{
		struct hdr_field * hf_pcv = NULL;
		if ( sip_get_charging_vector(msg, &hf_pcv) > 0 )
		{
			current_msg_id = msg->id;
		}
		LM_DBG("Parsed charging vector for pseudo-var\n");
	}
	else
	{
		LM_DBG("Charging vector is in state %d for pseudo-var\n", pcv_status);
	}

	switch(pcv_status)
	{
		case PCV_GENERATED:
			LM_DBG("pcv_status==PCV_GENERATED\n");
		case PCV_PARSED:
			LM_DBG("pcv_status==PCV_PARSED\n");
			switch( param->pvn.u.isname.name.n )
			{
				case 5:
					pcv_pv = pcv_term;
					break;
				case 4:
					pcv_pv = pcv_orig;
					break;
				case 2:
					pcv_pv = pcv_host;
					break;

				case 3:
					pcv_pv = pcv_id;
					break;

				case 1:
				default:
					pcv_pv = pcv;
					break;
			}

			if ( pcv_pv.len > 0 )
				return pv_get_strval(msg, param, res, &pcv_pv );
			else
				LM_WARN("No value for pseudo-var $pcv but status was %d.\n", pcv_status);

			break;

		case PCV_NONE:
		default:
			break;
	}

	return pv_get_null(msg, param, res);
}

int pv_parse_charging_vector_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3:
			if(strncmp(in->s, "all", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		case 4:
			if(strncmp(in->s, "orig", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else if(strncmp(in->s, "term", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else goto error;
		break;
		case 5:
			if(strncmp(in->s, "value", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else goto error;
		break;
		case 7:
			if(strncmp(in->s, "genaddr", 7)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown pcv name %.*s\n", in->len, in->s);
	return -1;
}
