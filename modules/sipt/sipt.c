/*
 *
 * Copyright (C) 2013 Voxbone SA
 *
 * This file is part of SIP-Router, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 
 */


#include "../../sr_module.h"
#include "../../parser/parse_param.h"
#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "../../mod_fix.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_body.h"
#include "../../parser/parser_f.h"
#include "../../trim.h"
#include "ss7.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


MODULE_VERSION

static int sipt_destination(struct sip_msg *msg, char *_destination, char *_hops, char * _nai);
static int sipt_set_calling(struct sip_msg *msg, char *_origin, char *_nai, char *_pres, char * _screen);
static int sipt_get_hop_counter(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_event_info(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_cpc(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_calling_party_nai(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_presentation(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_screening(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_called_party_nai(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

static int mod_init(void);
static void mod_destroy(void);



static int fixup_str_str_str(void** param, int param_no)
{
	if(param_no == 1 || param_no == 2 || param_no == 3 || param_no == 4)
	{
		return fixup_str_null(param, 1);
	}
	return E_CFG;
}

static int fixup_free_str_str_str(void** param, int param_no)
{
	if(param_no == 1 || param_no == 2 || param_no == 3 || param_no == 4)
	{
		return fixup_free_str_null(param, 1);
	}
	return E_CFG;
}


static cmd_export_t cmds[]={
	{"sipt_destination", /* action name as in scripts */
		(cmd_function)sipt_destination,  /* C function name */
		3,          /* number of parameters */
		fixup_str_str_str, fixup_free_str_str_str,         /* */
		/* can be applied to original requests */
		REQUEST_ROUTE|BRANCH_ROUTE}, 
	{"sipt_set_calling", /* action name as in scripts */
		(cmd_function)sipt_set_calling,  /* C function name */
		4,          /* number of parameters */
		fixup_str_str_str, fixup_free_str_str_str,         /* */
		/* can be applied to original requests */
		REQUEST_ROUTE|BRANCH_ROUTE}, 
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={ 
	{0,0,0} 
};

static mi_export_t mi_cmds[] = {
	{ 0, 0, 0, 0, 0}
};

static pv_export_t mod_items[] = {
        { {"sipt_presentation",  sizeof("sipt_presentation")-1}, PVT_OTHER,  sipt_get_presentation,    0,
                0, 0, 0, 0 },
        { {"sipt_screening",  sizeof("sipt_screening")-1}, PVT_OTHER,  sipt_get_screening,    0,
                0, 0, 0, 0 },
        { {"sipt_hop_counter",  sizeof("sipt_hop_counter")-1}, PVT_OTHER,  sipt_get_hop_counter,    0,
                0, 0, 0, 0 },
        { {"sipt_event_info",  sizeof("sipt_event_info")-1}, PVT_OTHER,  sipt_get_event_info,    0,
                0, 0, 0, 0 },
        { {"sipt_cpc",  sizeof("sipt_cpc")-1}, PVT_OTHER,  sipt_get_cpc,    0,
                0, 0, 0, 0 },
        { {"sipt_calling_party_nai",  sizeof("sipt_calling_party_nai")-1}, PVT_OTHER,  sipt_get_calling_party_nai,    0,
                0, 0, 0, 0 },
        { {"sipt_called_party_nai",  sizeof("sipt_called_party_nai")-1}, PVT_OTHER,  sipt_get_called_party_nai,    0,
                0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"sipt",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,        /* exported functions */
	params,      /* exported parameters */
	0,           /* exported statistics */
	mi_cmds,     /* exported MI functions */
	mod_items,   /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0            /* per-child init function */
};

static int sipt_get_hop_counter(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	pv_get_sintval(msg, param, res, isup_get_hop_counter((unsigned char*)body.s, body.len));
	return 0;
}

static int sipt_get_event_info(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_CPG)
	{
		LM_DBG("message not an CPG\n");
		return -1;
	}
	
	pv_get_sintval(msg, param, res, isup_get_event_info((unsigned char*)body.s, body.len));
	return 0;
}

static int sipt_get_cpc(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	pv_get_sintval(msg, param, res, isup_get_cpc((unsigned char*)body.s, body.len));
	return 0;
}

static int sipt_get_calling_party_nai(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	pv_get_sintval(msg, param, res, isup_get_calling_party_nai((unsigned char*)body.s, body.len));
	return 0;
}

static int sipt_get_presentation(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	pv_get_sintval(msg, param, res, isup_get_presentation((unsigned char*)body.s, body.len));
	return 0;
}

static int sipt_get_screening(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	LM_DBG("about to get screening\n");
	
	pv_get_sintval(msg, param, res, isup_get_screening((unsigned char*)body.s, body.len));
	return 0;
}

static int sipt_get_called_party_nai(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	pv_get_sintval(msg, param, res, isup_get_called_party_nai((unsigned char*)body.s, body.len));
	return 0;
}

static int sipt_destination(struct sip_msg *msg, char *_destination, char *_hops, char * _nai)
{
	str * str_hops = (str*)_hops;
	unsigned int hops = 0;
	str2int(str_hops, &hops);
	str * nai = (str*)_nai;
	unsigned int int_nai = 0;
	str2int(nai, &int_nai);
	str * destination = (str*)_destination;
	struct sdp_mangler mangle;

	// update forwarded iam
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}
	str sdp;
	sdp.s = get_body_part(msg, TYPE_APPLICATION, SUBTYPE_SDP, &sdp.len);
	
	unsigned char newbuf[1024];
	memset(newbuf, 0, 1024);
	if (body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}

	mangle.msg = msg;
	mangle.body_offset = (int)(body.s - msg->buf);


	char * digits = calloc(1,destination->len+2);
	memcpy(digits, destination->s, destination->len);
	digits[destination->len] = '#';

	int res = isup_update_destination(&mangle, digits, hops, int_nai, (unsigned char*)body.s, body.len);
	free(digits);
	if(res < 0)
	{
		LM_DBG("error updating IAM\n");
		return -1;
	}

	return 1;
}

static int sipt_set_calling(struct sip_msg *msg, char *_origin, char *_nai, char * _pres, char *_screen)
{
	unsigned int pres = 0;
	str * str_pres = (str*)_pres;
	str2int(str_pres, &pres);
	unsigned int screen = 0;
	str * str_screen = (str*)_screen;
	str2int(str_screen, &screen);
	str * nai = (str*)_nai;
	unsigned int int_nai = 0;
	str2int(nai, &int_nai);
	str * origin = (str*)_origin;
	struct sdp_mangler mangle;

	// update forwarded iam
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_INFO("No ISUP Message Found");
		return -1;
	}

	if (body.s==0) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len -(int)(body.s-msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}


	mangle.msg = msg;
	mangle.body_offset = (int)(body.s - msg->buf);

	char * digits = calloc(1,origin->len+1);
	memcpy(digits, origin->s, origin->len);

	int res = isup_update_calling(&mangle, digits, int_nai, pres, screen, (unsigned char*)body.s, body.len);
	free(digits);
	if(res < 0)
	{
		LM_DBG("error updating IAM\n");
		return -1;
	}

	return 1;
}


static int mod_init(void)
{
	return 0;
}


static void mod_destroy(void)
{
}
