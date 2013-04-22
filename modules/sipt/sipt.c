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
static int sipt_get_hop_counter(struct sip_msg *msg, char *x, char *y);
static int sipt_get_cpc(struct sip_msg *msg, char *x, char *y);
static int sipt_get_calling_party_nai(struct sip_msg *msg, char *x, char *y);
static int sipt_get_called_party_nai(struct sip_msg *msg, char *x, char *y);

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
	{"sipt_get_hop_counter", /* action name as in scripts */
		(cmd_function)sipt_get_hop_counter,  /* C function name */
		0,          /* number of parameters */
		0, 0,         /* */
		/* can be applied to original requests */
		REQUEST_ROUTE|BRANCH_ROUTE}, 
	{"sipt_get_cpc", /* action name as in scripts */
		(cmd_function)sipt_get_cpc,  /* C function name */
		0,          /* number of parameters */
		0, 0,         /* */
		/* can be applied to original requests */
		REQUEST_ROUTE|BRANCH_ROUTE}, 
	{"sipt_get_calling_party_nai", /* action name as in scripts */
		(cmd_function)sipt_get_calling_party_nai,  /* C function name */
		0,          /* number of parameters */
		0, 0,         /* */
		/* can be applied to original requests */
		REQUEST_ROUTE|BRANCH_ROUTE}, 
	{"sipt_get_called_party_nai", /* action name as in scripts */
		(cmd_function)sipt_get_called_party_nai,  /* C function name */
		0,          /* number of parameters */
		0, 0,         /* */
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

/*! \brief returns the value of boundary parameter from the Contect-Type HF */
static inline int get_boundary_param(struct sip_msg *msg, str *boundary)
{
        str     s;
        char    *c;
        param_t *p, *list;

#define is_boundary(c) \
        (((c)[0] == 'b' || (c)[0] == 'B') && \
        ((c)[1] == 'o' || (c)[1] == 'O') && \
        ((c)[2] == 'u' || (c)[2] == 'U') && \
        ((c)[3] == 'n' || (c)[3] == 'N') && \
        ((c)[4] == 'd' || (c)[4] == 'D') && \
        ((c)[5] == 'a' || (c)[5] == 'A') && \
        ((c)[6] == 'r' || (c)[6] == 'R') && \
        ((c)[7] == 'y' || (c)[7] == 'Y'))

#define boundary_param_len (sizeof("boundary")-1)

        /* get the pointer to the beginning of the parameter list */
        s.s = msg->content_type->body.s;
        s.len = msg->content_type->body.len;
        c = find_not_quoted(&s, ';');
        if (!c)
                return -1;
        c++;
        s.len = s.len - (c - s.s);
        s.s = c;
        trim_leading(&s);

        if (s.len <= 0)
                return -1;

        /* parse the parameter list, and search for boundary */
        if (parse_params(&s, CLASS_ANY, NULL, &list)<0)
                return -1;

        boundary->s = NULL;
        for (p = list; p; p = p->next)
                if ((p->name.len == boundary_param_len) &&
                        is_boundary(p->name.s)
                ) {
                        boundary->s = p->body.s;
                        boundary->len = p->body.len;
                        break;
                }
        free_params(list);
        if (!boundary->s || !boundary->len)
                return -1;

        DBG("boundary is \"%.*s\"\n",
                boundary->len, boundary->s);
        return 0;
}

static char * SDP_HEADER = "Content-Type: application/sdp\r\n\r\n";
static char * ISUP_HEADER = "Content-Type: application/ISUP; version=ITU-93\r\nContent-Disposition: signal; handling=optional\r\n\r\n";

static int replace_body(struct sip_msg *msg, str * nb)
{
	str body;
        body.s = get_body(msg);
	struct lump *anchor;
	char * buf;
	free_lump_list(msg->body_lumps);
	msg->body_lumps = NULL;


        if (msg->content_length)
        {
                body.len = get_content_length( msg );
                if(body.len > 0)
                {
                        if(body.s+body.len>msg->buf+msg->len)
                        {
                                LM_ERR("invalid content length: %d\n", body.len);
                                return -1;
                        }
                        if(del_lump(msg, body.s - msg->buf, body.len, 0) == 0)
                        {
                                LM_ERR("cannot delete existing body");
                                return -1;
                        }
                }
        }

        anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);

        if (anchor == 0)
        {
                LM_ERR("failed to get anchor\n");
                return -1;
        }

        if (msg->content_length==0)
        {
                /* need to add Content-Length */
                int len = nb->len;
		int value_len;
                char* value_s=int2str(len, &value_len);
                LM_DBG("content-length: %d (%s)\n", value_len, value_s);

                len=CONTENT_LENGTH_LEN+value_len+CRLF_LEN;
                buf=pkg_malloc(sizeof(char)*(len));

                if (buf==0)
                {
                        LM_ERR("out of pkg memory\n");
                        return -1;
                }

                memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
                memcpy(buf+CONTENT_LENGTH_LEN, value_s, value_len);
                memcpy(buf+CONTENT_LENGTH_LEN+value_len, CRLF, CRLF_LEN);
                if (insert_new_lump_after(anchor, buf, len, 0) == 0)
                {
                        LM_ERR("failed to insert content-length lump\n");
                        pkg_free(buf);
                        return -1;
                }
        }


        anchor = anchor_lump(msg, body.s - msg->buf, 0, 0);

        if (anchor == 0)
        {
                LM_ERR("failed to get body anchor\n");
                return -1;
        }

        buf=pkg_malloc(sizeof(char)*(nb->len));
        if (buf==0)
        {
                LM_ERR("out of pkg memory\n");
                return -1;
        }
        memcpy(buf, nb->s, nb->len);
        if (insert_new_lump_after(anchor, buf, nb->len, 0) == 0)
        {
                LM_ERR("failed to insert body lump\n");
                pkg_free(buf);
                return -1;
        }
        LM_DBG("new body: [%.*s]", nb->len, nb->s);

	return 0;
}

static int sipt_get_hop_counter(struct sip_msg *msg, char *x, char *y)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_ERR("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	return isup_get_hop_counter((unsigned char*)body.s, body.len);
}

static int sipt_get_cpc(struct sip_msg *msg, char *x, char *y)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_ERR("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	return isup_get_cpc((unsigned char*)body.s, body.len);
}

static int sipt_get_calling_party_nai(struct sip_msg *msg, char *x, char *y)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_ERR("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	return isup_get_calling_party_nai((unsigned char*)body.s, body.len);
}

static int sipt_get_called_party_nai(struct sip_msg *msg, char *x, char *y)
{
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_ERR("No ISUP Message Found");
		return -1;
	}

	if(body.s[0] != ISUP_IAM)
	{
		LM_DBG("message not an IAM\n");
		return -1;
	}
	
	return isup_get_called_party_nai((unsigned char*)body.s, body.len);
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

	// update forwarded iam
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_ERR("No ISUP Message Found");
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



	unsigned int offset = 0;

	if(sdp.s != NULL)
	{
		// we need to be clean, handle 2 cases
		// one with sdp, one without sdp
		str boundary = {0,0}; 
		get_boundary_param(msg, &boundary);
		memcpy(newbuf+offset, "--", 2);
		offset+=2;

		memcpy(newbuf+offset,boundary.s, boundary.len);
		offset += boundary.len;

		memcpy(newbuf+offset, "\r\n", 2);
		offset+=2;

		memcpy(newbuf+offset,SDP_HEADER, strlen(SDP_HEADER));
		offset += strlen(SDP_HEADER);


		memcpy(newbuf+offset,sdp.s, sdp.len);
		offset += sdp.len;

		memcpy(newbuf+offset, "\r\n", 2);
		offset+=2;
		memcpy(newbuf+offset, "\r\n--", 4);
		offset+=4;

		memcpy(newbuf+offset,boundary.s, boundary.len);
		offset += boundary.len;

		memcpy(newbuf+offset, "\r\n", 2);
		offset+=2;

		memcpy(newbuf+offset,ISUP_HEADER, strlen(ISUP_HEADER));
		offset += strlen(ISUP_HEADER);

		char * digits = calloc(1,destination->len+2);
		memcpy(digits, destination->s, destination->len);
		digits[destination->len] = '#';

		int res = isup_update_destination(digits, hops, int_nai, (unsigned char*)body.s, body.len, newbuf+offset, 512);

		if(res == -1)
		{
			LM_DBG("error updating IAM\n");
			return -1;
		}

		free(digits);
		offset += res;

		memcpy(newbuf+offset, "\r\n--", 4);
		offset+=4;

		memcpy(newbuf+offset,boundary.s, boundary.len);
		offset += boundary.len;

		memcpy(newbuf+offset, "--\r\n", 4);
		offset+=4;
	}
	else
	{
		// isup only body
		char * digits = calloc(1,destination->len+2);
		memcpy(digits, destination->s, destination->len);
		digits[destination->len] = '#';

		int res = isup_update_destination(digits, hops, int_nai, (unsigned char*)body.s, body.len, newbuf, 512);
		free(digits);
		offset = res;
		if(res == -1)
		{
			LM_DBG("error updating IAM\n");
			return -1;
		}
	}



	str nb = {(char*)newbuf, offset};
	replace_body(msg, &nb);

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

	// update forwarded iam
	str body;
	body.s = get_body_part(msg, TYPE_APPLICATION,SUBTYPE_ISUP,&body.len);

	if(body.s == NULL)
	{
		LM_ERR("No ISUP Message Found");
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



	unsigned int offset = 0;

	if(sdp.s != NULL)
	{
		// we need to be clean, handle 2 cases
		// one with sdp, one without sdp
		str boundary = {0,0}; 
		get_boundary_param(msg, &boundary);
		memcpy(newbuf+offset, "--", 2);
		offset+=2;

		memcpy(newbuf+offset,boundary.s, boundary.len);
		offset += boundary.len;

		memcpy(newbuf+offset, "\r\n", 2);
		offset+=2;

		memcpy(newbuf+offset,SDP_HEADER, strlen(SDP_HEADER));
		offset += strlen(SDP_HEADER);


		memcpy(newbuf+offset,sdp.s, sdp.len);
		offset += sdp.len;

		memcpy(newbuf+offset, "\r\n", 2);
		offset+=2;
		memcpy(newbuf+offset, "\r\n--", 4);
		offset+=4;

		memcpy(newbuf+offset,boundary.s, boundary.len);
		offset += boundary.len;

		memcpy(newbuf+offset, "\r\n", 2);
		offset+=2;

		memcpy(newbuf+offset,ISUP_HEADER, strlen(ISUP_HEADER));
		offset += strlen(ISUP_HEADER);

		char * digits = calloc(1,origin->len+1);
		memcpy(digits, origin->s, origin->len);

		int res = isup_update_calling(digits, int_nai, pres, screen, (unsigned char*)body.s, body.len, newbuf+offset, 512);

		if(res == -1)
		{
			LM_DBG("error updating IAM\n");
			return -1;
		}

		free(digits);
		offset += res;

		memcpy(newbuf+offset, "\r\n--", 4);
		offset+=4;

		memcpy(newbuf+offset,boundary.s, boundary.len);
		offset += boundary.len;

		memcpy(newbuf+offset, "--\r\n", 4);
		offset+=4;
	}
	else
	{
		// isup only body
		char * digits = calloc(1,origin->len+1);
		memcpy(digits, origin->s, origin->len);

		int res = isup_update_calling(digits, int_nai, pres, screen, (unsigned char*)body.s, body.len, newbuf, 512);
		free(digits);
		offset = res;
		if(res == -1)
		{
			LM_DBG("error updating IAM\n");
			return -1;
		}
	}



	str nb = {(char*)newbuf, offset};
	replace_body(msg, &nb);

	return 1;
}


static int mod_init(void)
{
	return 0;
}


static void mod_destroy(void)
{
}
