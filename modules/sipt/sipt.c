/*
 *
 * Copyright (C) 2015 Voxbone SA
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

static int sipt_set_bci_1(struct sip_msg *msg, char *_charge_indicator, char *_called_status, char * _called_category, char * _e2e_indicator);
static int sipt_destination(struct sip_msg *msg, char *_destination, char *_hops, char * _nai);
static int sipt_set_calling(struct sip_msg *msg, char *_origin, char *_nai, char *_pres, char * _screen);
static int sipt_get_hop_counter(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_event_info(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_cpc(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_calling_party_nai(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_presentation(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_screening(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int sipt_get_called_party_nai(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

/* New API */
int sipt_parse_pv_name(pv_spec_p sp, str *in);
static int sipt_get_pv(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

typedef struct _sipt_pv {
        int type;
        int sub_type;
} sipt_pv_t;

typedef struct sipt_header_map
{
	char * name;
	unsigned int type;
	struct sipt_subtype_map
	{
		char * name;
		unsigned int type;
	} subtypes[5];
} sipt_header_map_t;

static sipt_header_map_t sipt_header_mapping[] =
{
	{"CALLING_PARTY_CATEGORY", ISUP_PARM_CALLING_PARTY_CAT, 
		{{NULL, 0}} },
	{"CPC", ISUP_PARM_CALLING_PARTY_CAT, 
		{{NULL, 0}} },
	{"CALLING_PARTY_NUMBER", ISUP_PARM_CALLING_PARTY_NUM, 
		{{"NATURE_OF_ADDRESS", 1}, 
			{"NAI", 1},
			{"SCREENING", 2},
			{"PRESENTATION", 3},
			{NULL, 0}
		}},
	{"CALLED_PARTY_NUMBER", ISUP_PARM_CALLED_PARTY_NUM,
		{{"NATURE_OF_ADDRESS", 1}, 
			{"NAI", 1},
			{NULL, 0}
		}},
	{"HOP_COUNTER", ISUP_PARM_HOP_COUNTER, 
		{{NULL, 0}} },
	{"EVENT_INFO", ISUP_PARM_EVENT_INFO, 
		{{NULL, 0}} },
	{ NULL, 0, {}}
};



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
	{"sipt_set_bci_1", /* action name as in scripts */
		(cmd_function)sipt_set_bci_1,  /* C function name */
		4,          /* number of parameters */
		fixup_str_str_str, fixup_free_str_str_str,         /* */
		/* can be applied to original requests */
		ONREPLY_ROUTE}, 
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
        { {"sipt_cpc",  sizeof("sipt_cpc")-1}, PVT_OTHER,  sipt_get_cpc,    0,
                0, 0, 0, 0 },
        { {"sipt_calling_party_nai",  sizeof("sipt_calling_party_nai")-1}, PVT_OTHER,  sipt_get_calling_party_nai,    0,
                0, 0, 0, 0 },
        { {"sipt_called_party_nai",  sizeof("sipt_called_party_nai")-1}, PVT_OTHER,  sipt_get_called_party_nai,    0,
                0, 0, 0, 0 },
        { {"sipt",  sizeof("sipt")-1}, PVT_OTHER,  sipt_get_pv,    0,
                sipt_parse_pv_name, 0, 0, 0 },
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

int sipt_parse_pv_name(pv_spec_p sp, str *in)
{
        sipt_pv_t *spv=NULL;
        char *p;
        str pvtype;
        str pvsubtype;
        if(sp==NULL || in==NULL || in->len<=0)
                return -1;

        spv = (sipt_pv_t*)pkg_malloc(sizeof(sipt_pv_t));
        if(spv==NULL)
                return -1;

        memset(spv, 0, sizeof(sipt_pv_t));

	p = in->s;

        while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
                p++;
        if(p>in->s+in->len || *p=='\0')
                goto error;

	pvtype.s = p;

        while(p < in->s + in->len)
        {
                if(*p=='.' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')
                        break;
                p++;
        }
        pvtype.len = p - pvtype.s;
        if(p>in->s+in->len || *p=='\0')
	{
		// only one parameter stop parsing
		pvsubtype.len = 0;
		pvsubtype.s = NULL;
                goto parse_parameters;
	}

        if(*p!='.')
        {
                while(p<in->s+in->len && (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r'))
                        p++;
                if(p>in->s+in->len || *p=='\0' || *p!='.')
		{
			// only one parameter w trailing whitespace
			pvsubtype.len = 0;
			pvsubtype.s = NULL;
			goto parse_parameters;
		}
        }
        p++;

        pvsubtype.len = in->len - (int)(p - in->s);
        pvsubtype.s = p;

 
parse_parameters:
        LM_DBG("sipt type[%.*s] - subtype[%.*s]\n", pvtype.len, pvtype.s,
                        pvsubtype.len, pvsubtype.s);
	int i = 0, j=0;

	for(i=0;sipt_header_mapping[i].name != NULL; i++)
	{
		if(strncasecmp(pvtype.s, sipt_header_mapping[i].name, pvtype.len) == 0)
		{
			spv->type = sipt_header_mapping[i].type;

			if(pvsubtype.len == 0)
				break;

			for(j=0;sipt_header_mapping[i].subtypes[j].name != NULL;j++)
			{
				if(strncasecmp(pvsubtype.s, sipt_header_mapping[i].subtypes[j].name, pvsubtype.len) == 0)
				spv->sub_type = sipt_header_mapping[i].subtypes[j].type;
			}
			if(spv->sub_type == 0)
			{
				LM_ERR("Unknown SIPT subtype [%.*s]\n", pvsubtype.len, pvsubtype.s);
				return -1;
			}
			break;
		}
	}

	LM_DBG("Type=%d subtype=%d\n",spv->type, spv->sub_type);
	if(spv->type == 0)
	{
		LM_ERR("Unknown SIPT type [%.*s]\n",pvtype.len, pvtype.s);
		return -1;
	}

	sp->pvp.pvn.u.dname = (void*)spv;
	sp->pvp.pvn.type = PV_NAME_OTHER;

	return 0;
error:
        LM_ERR("error at PV sipt name: %.*s\n", in->len, in->s);
        return -1;

}

static int sipt_get_pv(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
        sipt_pv_t *spv;

        if(msg==NULL || param==NULL)
                return -1;

        spv = (sipt_pv_t*)param->pvn.u.dname;
        if(spv==NULL)
                return -1;

        switch(spv->type)
        {
		case ISUP_PARM_CALLING_PARTY_CAT:
			return sipt_get_cpc(msg, param, res);
		case ISUP_PARM_CALLING_PARTY_NUM:
			switch(spv->sub_type)
			{
				case 1: /* NAI */
					return sipt_get_calling_party_nai(msg, param, res);
				case 2: /* SCREENIG */
					return sipt_get_screening(msg, param, res);
				case 3: /* PRESENTATION */
					return sipt_get_presentation(msg, param, res);
			}
			break;
		case ISUP_PARM_CALLED_PARTY_NUM:
			switch(spv->sub_type)
			{
				case 1: /* NAI */
					return sipt_get_called_party_nai(msg, param, res);
			}
			break;
		case ISUP_PARM_HOP_COUNTER:
			return sipt_get_hop_counter(msg, param, res);
		case ISUP_PARM_EVENT_INFO:
			return sipt_get_event_info(msg, param, res);
	}

	return -1;
}

static int sipt_set_bci_1(struct sip_msg *msg, char *_charge_indicator, char *_called_status, char * _called_category, char * _e2e_indicator)
{
	str * str_charge_indicator = (str*)_charge_indicator;
	unsigned int charge_indicator = 0;
	str2int(str_charge_indicator, &charge_indicator);
	str * str_called_status = (str*)_called_status;
	unsigned int called_status = 0;
	str2int(str_called_status, &called_status);
	str * str_called_category = (str*)_called_category;
	unsigned int called_category = 0;
	str2int(str_called_category, &called_category);
	str * str_e2e_indicator = (str*)_e2e_indicator;
	unsigned int e2e_indicator = 0;
	str2int(str_e2e_indicator, &e2e_indicator);
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

	if(body.s[0] != ISUP_ACM && body.s[0] != ISUP_COT)
	{
		LM_DBG("message not an ACM or COT\n");
		return -1;
	}

	mangle.msg = msg;
	mangle.body_offset = (int)(body.s - msg->buf);



	int res = isup_update_bci_1(&mangle, charge_indicator, called_status, called_category, e2e_indicator, (unsigned char*)body.s, body.len);
	if(res < 0)
	{
		LM_DBG("error updating ACM\n");
		return -1;
	}

	return 1;
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
