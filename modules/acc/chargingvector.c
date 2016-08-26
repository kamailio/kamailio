#include <string.h>
#include <netdb.h>

#include "../../mod_fix.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../lib/kcore/cmpapi.h"
#include "chargingvector.h"

#define SIZE_CONF_ID 16
#define P_CHARGING_VECTOR    "P-Charging-Vector"
#define LOOPBACK_IP  16777343

static char pcv_buf[100];
static str pcv = { pcv_buf, 0 };
static str pcv_host = { NULL, 0 };
static str pcv_id = { NULL, 0 };
static uint64_t     	counter = 0 ;


enum PCV_Status {
	PCV_NONE = 0,
	PCV_PARSED = 1,
	PCV_GENERATED = 2
};

static enum PCV_Status pcv_status = PCV_NONE;
static unsigned int current_msg_id = (unsigned int)-1;

static void acc_generate_charging_vector(char * pcv)
{
    char             s[PATH_MAX]        = {0};
    struct hostent*  host               = NULL;
    int              cdx                = 0 ;
    int              tdx                = 0 ;
    int              idx                = 0 ;
    int              ipx                = 0 ;
    int pid;
    uint64_t         ct                 = 0 ;
    struct in_addr*  in                 = NULL;
    static struct in_addr ip            = {0};
    unsigned char             		newConferenceIdentifier[SIZE_CONF_ID]={0};

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
			if ( ip.s_addr  == 0 )
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
    LM_DBG("CV generate");
    int i =0;
    pcv[0] = '\0';
    for ( i = 0 ; i < SIZE_CONF_ID ; i ++ )
    {
        char hex[4] = {0 };

        snprintf(hex,4,"%02X",newConferenceIdentifier[i]);
                strcat(pcv,hex);
    }
}

static unsigned int acc_param_end(const char * s, unsigned int len)
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

static int acc_parse_charging_vector(const char * pvc_value, unsigned int len)
{
    /* now point to each PCV component */
    LM_DBG("acc: parsing PCV header [%s].\n", pvc_value);

    char *s = strstr(pvc_value, "icid-value=");
    if (s != NULL)
    {
        pcv_id.s = s + strlen("icid-value=");
	pcv_id.len = acc_param_end(pcv_id.s, len);
	LM_INFO("acc: parsed P-Charging-Vector icid-value=%.*s",
		pcv_id.len, pcv_id.s );
    }
    else
    {
	pcv_id.s = NULL;
	pcv_id.len = 0;
    }
    
    s = strstr(pvc_value, "icid-generated-at=");
    if (s != NULL)
    {
        pcv_host.s = s + strlen("icid-generated-at=");
	pcv_host.len = acc_param_end(pcv_id.s, len);
	LM_DBG("acc: parsed P-Charging-Vector icid-generated-at=%.*s",
		pcv_host.len, pcv_host.s );
    }
    else
    {
	pcv_host.s = NULL;
	pcv_host.len = 0;
    }

    // Buggy charging vector where only icid-value is sent ...
    if ( pcv_host.s == NULL &&  pcv_id.s == NULL && len > 0)
    {
	pcv_id.s = (char *) pvc_value,
	pcv_id.len = acc_param_end(pcv_id.s, len);
	LM_WARN("acc: parsed BUGGY P-Charging-Vector %.*s", pcv_id.len, pcv_id.s );
    }

    return (pcv_id.s != NULL);
}

static int acc_get_charging_vector(struct sip_msg *msg, struct hdr_field ** hf_pcv )
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
	    if ( hf->name.s[0] == 'P' )
	    {
		LM_INFO("acc: checking heander=%.*s\n", hf->name.len, hf->name.s );
	    }
	
	    if ( cmp_hdrname_str(&hf->name, &hdrname) == 0)
	    {
		/* 
		 * append p charging vector valus after the header name "P-Charging-Vector" and
		 * the ": " (+2)
		 */
	        char * pcv_body = pcv_buf + strlen(P_CHARGING_VECTOR) + 2;
		
		if (hf->body.len > 0) 
		{
		    memcpy( pcv_body, hf->body.s, hf->body.len );
		    pcv.len = hf->body.len + strlen(P_CHARGING_VECTOR) + 2;
		    pcv_body[hf->body.len]= '\0';
		    if ( acc_parse_charging_vector( pcv_body, hf->body.len ) == 0)
		    {
			LM_ERR("P-Charging-Vector header found but failed to parse value [%s].\n", pcv_body);
		    }
		    else
		    {
		        pcv_status = PCV_PARSED;
		    }
	    	    return 2;
		}
		else
		{
		    pcv_id.s =0;
		    pcv_id.len = 0;
		    pcv_host.s = 0;
		    pcv_host.len = 0;
		    LM_WARN("P-Charging-Vector header found but no value.\n");
		}
		*hf_pcv = hf;
	    }
	}
	LM_INFO("No valid P-Charging-Vector header found.\n");
	return 1;
}

// Remove PVC if it is in the inbound request (if it was found by acc_get_charging_vector)
static int  acc_remove_charging_vector(struct sip_msg *msg, struct hdr_field *hf)
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
	
static int acc_add_charging_vector(struct sip_msg *msg)
{
    struct lump* anchor;
    char * s;
    
    anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
    if(anchor == 0) 
    {
	LM_ERR("can't get anchor\n");
	return -1;
    }

    s  = (char*)pkg_malloc(pcv.len);
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

int acc_handle_pcv(struct sip_msg *msg, char *flags, char *str2)
{
    int generate_pcv = 0;
    int remove_pcv = 0;
    int replace_pcv = 0;
    int i;    
    str flag_str;
    struct hdr_field * hf_pcv = NULL;

    pcv.len = 0;
    pcv_status = PCV_NONE;
    strcpy(pcv_buf, P_CHARGING_VECTOR);
    strcat(pcv_buf, ": ");
    
    fixup_get_svalue(msg, (gparam_p)flags, &flag_str);
    
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
    
    acc_get_charging_vector(msg, &hf_pcv);
    
    /*
     * We need to remove the original PCV if it was present and ether
     * we were asked to remove it or we were asked to replace it
     */
    if ( pcv_status == PCV_PARSED && (replace_pcv || remove_pcv)  )
    {
	i = acc_remove_charging_vector(msg, hf_pcv);
	if (i <= 0) return i;
    }
    
    /* Generate PCV if 
     * - we were asked to generate it and it could not be obtained from the inbound packet
     * - or if we were asked to replace it alltogether regardless its former value
     */
    if ( replace_pcv || (generate_pcv && pcv_status != PCV_GENERATED && pcv_status != PCV_PARSED ) )
    {
        char * pcv_body = pcv_buf  + strlen(P_CHARGING_VECTOR) + 2;
	char pcv_value[20];

	/* We use the IP adress of the interface that received the message as generated-at */
	if(msg->rcv.bind_address==NULL || msg->rcv.bind_address->address_str.s==NULL)
	{
	    LM_ERR("No IP address for message. Failed to generate charging vector.\n");
	    return -2;
	}
	
        acc_generate_charging_vector(pcv_value);

	sprintf( pcv_body, "icid-value=%s; icid-generated-at=%.*s\r\n", pcv_value, 
	         msg->rcv.bind_address->address_str.len, 
		 msg->rcv.bind_address->address_str.s );

	pcv.len = strlen(pcv_buf);
	pcv_status = PCV_GENERATED;

	/* if generated, reparse it */
	acc_parse_charging_vector( pcv_body, strlen(pcv_body) );
	/* if it was generated, we need to send it out as a header */
	LM_INFO("Generated PCV header %s.\n", pcv_buf );
	i = acc_add_charging_vector(msg);
	if (i <= 0)
	{
	    LM_ERR("Failed to add P-Charging-Vector header\n");
	    return i;
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
	     if ( acc_get_charging_vector(msg, &hf_pcv) > 0 )
	     {
	         current_msg_id = msg->id;
	     }
	     LM_DBG("Parsed charging vector for pseudo-var \n");
	}
    	else
	{
	     LM_DBG("Charging vector is in state %d for pseudo-var\n", pcv_status);
	}
    
    switch(pcv_status)
    {
	case PCV_GENERATED:
	case PCV_PARSED:
	    switch( param->pvn.u.isname.name.n )
	    {
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
