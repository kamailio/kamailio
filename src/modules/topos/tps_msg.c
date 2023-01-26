/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief SIP-router topoh ::
 * \ingroup topoh
 * Module: \ref topoh
 */

#include <string.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/data_lump.h"
#include "../../core/forward.h"
#include "../../core/trim.h"
#include "../../core/dset.h"
#include "../../core/msg_translator.h"
#include "../../core/parser/parse_rr.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_via.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_refer_to.h"


#include "tps_msg.h"
#include "tps_storage.h"

extern int _tps_param_mask_callid;
extern int _tps_contact_mode;
extern str _tps_cparam_name;
extern int _tps_rr_update;
extern int _tps_header_mode;

extern str _tps_context_param;
extern str _tps_context_value;

str _sr_hname_xbranch = str_init("P-SR-XBranch");
str _sr_hname_xuuid = str_init("P-SR-XUID");

unsigned int _tps_methods_nocontact = METHOD_CANCEL|METHOD_BYE|METHOD_PRACK;
unsigned int _tps_methods_noinitial = 0;

/**
 *
 */
int tps_skip_rw(char *s, int len)
{
	while(len>0)
	{
		if(s[len-1]==' ' || s[len-1]=='\t' || s[len-1]=='\n' || s[len-1]=='\r'
				|| s[len-1]==',')
			len--;
		else return len;
	}
	return 0;
}

/**
 *
 */
struct via_param *tps_get_via_param(struct via_body *via, str *name)
{
	struct via_param *p;
	for(p=via->param_lst; p; p=p->next)
	{
		if(p->name.len==name->len
				&& strncasecmp(p->name.s, name->s, name->len)==0)
			return p;
	}
	return NULL;
}

/**
 *
 */
int tps_get_param_value(str *in, str *name, str *value)
{
	param_t* params = NULL;
	param_t* p = NULL;
	param_hooks_t phooks;
	if (parse_params(in, CLASS_ANY, &phooks, &params)<0)
		return -1;
	for (p = params; p; p=p->next)
	{
		if (p->name.len==name->len
				&& strncasecmp(p->name.s, name->s, name->len)==0)
		{
			*value = p->body;
			free_params(params);
			return 0;
		}
	}

	if(params) free_params(params);
	return 1;

}

/**
 *
 */
int tps_remove_headers(sip_msg_t *msg, uint32_t hdr)
{
	struct hdr_field *hf;
	struct lump* l;

	if(parse_headers(msg, HDR_EOH_F, 0)<0) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}
	for (hf=msg->headers; hf; hf=hf->next) {
		if (hdr!=hf->type)
			continue;
		l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
		if (l==0) {
			LM_ERR("failed to remove the header\n");
			return -1;
		}
	}
	return 0;
}

/**
 *
 */
int tps_add_headers(sip_msg_t *msg, str *hname, str *hbody, int hpos)
{
	struct lump* anchor;
	str hs;

	if(hname==NULL || hname->len<=0 || hbody==NULL || hbody->len<=0)
		return 0;

	if(parse_headers(msg, HDR_EOH_F, 0)<0) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if(hpos == 0) { /* append */
		/* after last header */
		anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	} else { /* insert */
		/* before first header */
		anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);
	}

	if(anchor == 0) {
		LM_ERR("can't get anchor\n");
		return -1;
	}

	hs.len = hname->len + 2 + hbody->len;
	hs.s  = (char*)pkg_malloc(hs.len + 3);
	if (hs.s==NULL) {
		PKG_MEM_ERROR_FMT("(%.*s - %d)\n", hname->len, hname->s, hs.len);
		return -1;
	}
	memcpy(hs.s, hname->s, hname->len);
	hs.s[hname->len] = ':';
	hs.s[hname->len+1] = ' ';
	memcpy(hs.s + hname->len + 2, hbody->s, hbody->len);

	/* add end of header if not present */
	if(hs.s[hname->len + 2 + hbody->len - 1]!='\n') {
		hs.s[hname->len + 2 + hbody->len] = '\r';
		hs.s[hname->len + 2 + hbody->len+1] = '\n';
		hs.len += 2;
	}

	LM_DBG("adding to headers(%d) - [%.*s]\n", hpos, hs.len, hs.s);

	if (insert_new_lump_before(anchor, hs.s, hs.len, 0) == 0) {
		LM_ERR("can't insert lump\n");
		pkg_free(hs.s);
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_get_uri_param_value(str *uri, str *name, str *value)
{
	struct sip_uri puri;

	memset(value, 0, sizeof(str));
	if(parse_uri(uri->s, uri->len, &puri)<0)
		return -1;
	return tps_get_param_value(&puri.params, name, value);
}

/**
 *
 */
int tps_get_uri_type(str *uri, int *mode, str *value)
{
	struct sip_uri puri;
	int ret;
	str r2 = {"r2", 2};

	memset(value, 0, sizeof(str));
	*mode = 0;
	if(parse_uri(uri->s, uri->len, &puri)<0)
		return -1;

	LM_DBG("PARAMS [%.*s]\n", puri.params.len, puri.params.s);

	if(check_self(&puri.host, puri.port_no, 0)==1)
	{
		/* myself -- matched on all protos */
		ret = tps_get_param_value(&puri.params, &r2, value);
		if(ret<0)
			return -1;
		if(ret==1) /* not found */
			return 0; /* skip */
		LM_DBG("VALUE [%.*s]\n",
				value->len, value->s);
		if(value->len==2 && strncasecmp(value->s, "on", 2)==0)
			*mode = 1;
		memset(value, 0, sizeof(str));
		return 0; /* skip */
	}
	/* not myself & not mask ip */
	return 1; /* encode */
}

/**
 *
 */
char* tps_msg_update(sip_msg_t *msg, unsigned int *olen)
{
	struct dest_info dst;

	init_dest_info(&dst);
	dst.proto = PROTO_UDP;
	return build_req_buf_from_sip_req(msg,
			olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
}

/**
 *
 */
int tps_skip_msg(sip_msg_t *msg)
{
	if (msg->cseq==NULL || get_cseq(msg)==NULL) {
		LM_WARN("Invalid/Unparsed CSeq in message. Skipping.");
		return 1;
	}

	if((get_cseq(msg)->method_id)&(METHOD_REGISTER|METHOD_PUBLISH))
		return 1;

	if(_tps_methods_noinitial!=0 && msg->first_line.type==SIP_REQUEST
			&& get_to(msg)->tag_value.len<=0) {
		if((get_cseq(msg)->method_id) & _tps_methods_noinitial) {
			return 1;
		}
	}

	return 0;
}

/**
 *
 */
int tps_dlg_detect_direction(sip_msg_t *msg, tps_data_t *ptsd,
		uint32_t *direction)
{
	str ftag = {0, 0};
	/* detect direction - get from-tag */
	if(parse_from_header(msg)<0 || msg->from==NULL) {
		LM_ERR("failed getting 'from' header!\n");
		goto error;
	}
	ftag = get_from(msg)->tag_value;

	if(ptsd->a_tag.len!=ftag.len) {
		*direction = TPS_DIR_UPSTREAM;
	} else {
		if(memcmp(ptsd->a_tag.s, ftag.s, ftag.len)==0) {
			*direction = TPS_DIR_DOWNSTREAM;
		} else {
			*direction = TPS_DIR_UPSTREAM;
		}
	}
	return 0;

error:
	return -1;
}

/**
 *
 */
int tps_dlg_message_update(sip_msg_t *msg, tps_data_t *ptsd, int ctmode)
{
	str tuuid = STR_NULL;
	int ret;

#define TPS_TUUID_MIN_LEN 10

	if(parse_sip_msg_uri(msg)<0) {
		LM_ERR("failed to parse r-uri\n");
		return -1;
	}

	if (ctmode == 1 || ctmode == 2) {
		if(msg->parsed_uri.sip_params.len<TPS_TUUID_MIN_LEN) {
			LM_DBG("not an expected param format\n");
			return 1;
		}
		/* find the r-uri parameter */
		ret = tps_get_param_value(&msg->parsed_uri.params,
			&_tps_cparam_name, &tuuid);
		if (ret < 0) {
			LM_ERR("failed to parse param\n");
			return -1;
		}
		if (ret == 1) {
			LM_DBG("prefix para not found\n");
			return 1;
		}
	} else {
		if(msg->parsed_uri.user.len<TPS_TUUID_MIN_LEN) {
			LM_DBG("not an expected user format\n");
			return 1;
		}
		tuuid = msg->parsed_uri.user;
	}

	if(memcmp(tuuid.s, "atpsh-", 6)==0) {
		ptsd->a_uuid = tuuid;
		return 0;
	}
	if(memcmp(tuuid.s, "btpsh-", 6)==0) {
		ptsd->a_uuid = tuuid;
		ptsd->b_uuid = tuuid;
		return 0;
	}

	LM_DBG("not an expected prefix\n");

	return 1;
}

/**
 *
 */
int tps_pack_message(sip_msg_t *msg, tps_data_t *ptsd)
{
	hdr_field_t *hdr;
	via_body_t *via;
	rr_t *rr;
	int i;
	int vlen;
	int r2;
	int isreq;
	int rr_update = _tps_rr_update;

	if(ptsd->cp==NULL) {
		ptsd->cp = ptsd->cbuf;
	}

	if(_tps_rr_update) {
		if((msg->first_line.type==SIP_REQUEST) && !(get_to(msg)->tag_value.len>0)) {
			rr_update = 0;
		}
	}

	i = 0;
	for(hdr=msg->h_via1; hdr; hdr=next_sibling_hdr(hdr)) {
		for(via=(struct via_body*)hdr->parsed; via; via=via->next) {
			i++;
			vlen = tps_skip_rw(via->name.s, via->bsize);
			if(ptsd->cp + vlen + 2 >= ptsd->cbuf + TPS_DATA_SIZE) {
				LM_ERR("no more space to pack via headers\n");
				return -1;
			}
			if(i>1) {
				*ptsd->cp = ',';
				ptsd->cp++;
				if(i>2) {
					ptsd->x_via2.len++;
				}
			}
			memcpy(ptsd->cp, via->name.s, vlen);
			if(i==1) {
				ptsd->x_via1.s = ptsd->cp;
				ptsd->x_via1.len = vlen;
				if(via->branch!=NULL) {
					ptsd->x_vbranch1.s = ptsd->x_via1.s + (via->branch->value.s - via->name.s);
					ptsd->x_vbranch1.len = via->branch->value.len;
				}
			} else {
				if(i==2) {
					ptsd->x_via2.s = ptsd->cp;
				}
				ptsd->x_via2.len += vlen;
			}
			ptsd->cp += vlen;
		}
	}
	LM_DBG("compacted headers - x_via1: [%.*s](%d) - x_via2: [%.*s](%d)"
			" - x_vbranch1: [%.*s](%d)\n",
			ptsd->x_via1.len, ZSW(ptsd->x_via1.s), ptsd->x_via1.len,
			ptsd->x_via2.len, ZSW(ptsd->x_via2.s), ptsd->x_via2.len,
			ptsd->x_vbranch1.len, ZSW(ptsd->x_vbranch1.s), ptsd->x_vbranch1.len);

	ptsd->a_rr.len = 0;
	ptsd->s_rr.len = 0;
	i = 0;
	r2 = 0;
	isreq = (msg->first_line.type==SIP_REQUEST)?1:0;
	for(hdr=msg->record_route; hdr; hdr=next_sibling_hdr(hdr)) {
		if (parse_rr(hdr) < 0) {
			LM_ERR("failed to parse RR\n");
			return -1;
		}

		for(rr =(rr_t*)hdr->parsed; rr; rr=rr->next) {
			i++;
			if(ptsd->cp + rr->nameaddr.uri.len + 4 >= ptsd->cbuf + TPS_DATA_SIZE) {
				LM_ERR("no more space to pack rr headers\n");
				return -1;
			}
			if(isreq==1 || rr_update) {
				/* sip request - get a+s-side record route */
				if(i>1) {
					if(i==2 &&r2==0) {
						ptsd->s_rr.len = ptsd->a_rr.len;
					}
					if(i==3 &&r2==1) {
						ptsd->s_rr.len = ptsd->a_rr.len;
					}
					*ptsd->cp = ',';
					ptsd->cp++;
					ptsd->a_rr.len++;
					if(rr_update) {
						ptsd->b_rr.len++;
					}
				}
				if(i==1 && rr_update) {
					ptsd->b_rr.s = ptsd->cp;
				}
				*ptsd->cp = '<';
				if(i==1) {
					ptsd->a_rr.s = ptsd->cp;
					ptsd->s_rr.s = ptsd->cp;
				}
				if(i==2 && r2==0) {
					ptsd->a_rr.s = ptsd->cp;
					ptsd->a_rr.len = 0;
				}
				if(i==3 && r2==1) {
					ptsd->a_rr.s = ptsd->cp;
					ptsd->a_rr.len = 0;
				}

				ptsd->cp++;
				ptsd->a_rr.len++;
				if(rr_update) {
					ptsd->b_rr.len++;
				}

				memcpy(ptsd->cp, rr->nameaddr.uri.s, rr->nameaddr.uri.len);
				if(i==1) {
					ptsd->bs_contact.s = ptsd->cp;
					ptsd->bs_contact.len = rr->nameaddr.uri.len;
					if(_strnstr(ptsd->bs_contact.s, ";r2=on",
								ptsd->bs_contact.len)==0) {
						LM_DBG("single record routing by proxy\n");
						ptsd->as_contact.s = ptsd->cp;
						ptsd->as_contact.len = rr->nameaddr.uri.len;
					} else {
						r2 = 1;
					}
				} else {
					if(i==2 && ptsd->as_contact.len==0) {
						LM_DBG("double record routing by proxy\n");
						ptsd->as_contact.s = ptsd->cp;
						ptsd->as_contact.len = rr->nameaddr.uri.len;
					}
				}
				ptsd->a_rr.len += rr->nameaddr.uri.len;
				ptsd->cp += rr->nameaddr.uri.len;
				*ptsd->cp = '>';
				ptsd->cp++;
				ptsd->a_rr.len++;
				if(rr_update) {
					ptsd->b_rr.len += rr->nameaddr.uri.len;
					ptsd->b_rr.len++;
				}
			} else {
				/* sip response - get b-side record route */
				if(i==1) {
					ptsd->b_rr.s = ptsd->cp;
				}
				if(i>1) {
					*ptsd->cp = ',';
					ptsd->cp++;
					ptsd->b_rr.len++;
				}
				*ptsd->cp = '<';
				ptsd->cp++;
				ptsd->b_rr.len++;
				memcpy(ptsd->cp, rr->nameaddr.uri.s, rr->nameaddr.uri.len);
				ptsd->cp += rr->nameaddr.uri.len;
				ptsd->b_rr.len += rr->nameaddr.uri.len;
				*ptsd->cp = '>';
				ptsd->cp++;
				ptsd->b_rr.len++;
			}
		}
	}
	if(isreq==1) {
		if(i==1) {
			ptsd->s_rr.len = ptsd->a_rr.len;
			ptsd->a_rr.len = 0;
		}
		if(i==2 && r2==1) {
			ptsd->s_rr.len = ptsd->a_rr.len;
			ptsd->a_rr.len = 0;
		}
	}
	LM_DBG("compacted headers - a_rr: [%.*s](%d) - b_rr: [%.*s](%d)"
			" - s_rr: [%.*s](%d)\n",
			ptsd->a_rr.len, ZSW(ptsd->a_rr.s), ptsd->a_rr.len,
			ptsd->b_rr.len, ZSW(ptsd->b_rr.s), ptsd->b_rr.len,
			ptsd->s_rr.len, ZSW(ptsd->s_rr.s), ptsd->s_rr.len);
	LM_DBG("compacted headers - as_contact: [%.*s](%d) - bs_contact: [%.*s](%d)\n",
			ptsd->as_contact.len, ZSW(ptsd->as_contact.s), ptsd->as_contact.len,
			ptsd->bs_contact.len, ZSW(ptsd->bs_contact.s), ptsd->bs_contact.len);
	ptsd->x_rr = ptsd->a_rr;
	if(isreq==0) {
		if(msg->first_line.u.reply.statuscode >= 180
				&& msg->first_line.u.reply.statuscode < 199) {
			/* provisional replies that create early dialogs
			 * - skip 199 Early Dialog Terminated */
			ptsd->y_rr = ptsd->b_rr;
		}
	}
	ptsd->s_method_id = get_cseq(msg)->method_id;
	if(_tps_context_value.len>0) {
		ptsd->x_context = _tps_context_value;
	} else if(_tps_context_param.len>0) {
		ptsd->x_context = _tps_context_param;
	}
	return 0;
}


/**
 *
 */
int tps_reinsert_via(sip_msg_t *msg, tps_data_t *ptsd, str *hbody)
{
	str hname = str_init("Via");

	if(tps_add_headers(msg, &hname, hbody, 1)<0) {
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_reinsert_contact(sip_msg_t *msg, tps_data_t *ptsd, str *hbody)
{
	str hname = str_init("Contact");

	if (get_cseq(msg)->method_id & _tps_methods_nocontact) {
		return 0;
	}

	if(tps_add_headers(msg, &hname, hbody, 0)<0) {
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_remove_name_headers(sip_msg_t *msg, str *hname)
{
	hdr_field_t *hf;
	struct lump* l;
	for (hf=msg->headers; hf; hf=hf->next)
	{
		if (hf->name.len==hname->len
				&& strncasecmp(hf->name.s, hname->s,
					hname->len)==0)
		{
			l=del_lump(msg, hf->name.s-msg->buf, hf->len, 0);
			if (l==0) {
				LM_ERR("unable to delete header [%.*s]\n",
						hname->len, hname->s);
				return -1;
			}
			return 0;
		}
	}
	return 0;
}

/**
 *
 */
int tps_reappend_separate_header_values(sip_msg_t *msg, tps_data_t *ptsd, str *hbody, str *hname)
{

        int i;
        str sb;
        char *p = NULL;

        if(hbody==NULL || hbody->s==NULL || hbody->len<=0 || hbody->s[0]=='\0')
            return 0;

        sb.len = 1;
        p = hbody->s;
        for(i=0; i<hbody->len-1; i++) {
            if(hbody->s[i]==',') {
                if(sb.len>0) {
                    sb.s = p;
                    if(sb.s[sb.len-1]==',') sb.len--;
                    if(tps_add_headers(msg, hname, &sb, 0)<0) {
                        return -1;
                    }
                }
                sb.len = 0;
                p = hbody->s + i + 1;
            }
            sb.len++;
        }


        if(sb.len>0) {
                sb.s = p;
                if(sb.s[sb.len-1]==',') sb.len--;
                if(tps_add_headers(msg, hname, &sb, 0)<0) {
                    return -1;
                }
        }


        return 0;
}

int tps_reappend_via(sip_msg_t *msg, tps_data_t *ptsd, str *hbody)
{
	str hname = str_init("Via");

	if (TPS_SPLIT_VIA & _tps_header_mode)
		return tps_reappend_separate_header_values(msg, ptsd, hbody,&hname);

	if(tps_add_headers(msg, &hname, hbody, 0)<0) {
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_append_xbranch(sip_msg_t *msg, str *hbody)
{
	if(tps_add_headers(msg, &_sr_hname_xbranch, hbody, 0)<0) {
		LM_ERR("failed to add xbranch header [%.*s]/%d\n",
				hbody->len, hbody->s, hbody->len);
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_remove_xbranch(sip_msg_t *msg)
{
	return tps_remove_name_headers(msg, &_sr_hname_xbranch);
}

/**
 *
 */
int tps_get_xbranch(sip_msg_t *msg, str *hbody)
{
	hdr_field_t *hf;
	if(parse_headers(msg, HDR_EOH_F, 0)<0) {
		return -1;
	}

	for (hf=msg->headers; hf; hf=hf->next)
	{
		if(_sr_hname_xbranch.len==hf->name.len
				&& strncasecmp(_sr_hname_xbranch.s, hf->name.s,
					hf->name.len)==0) {
			break;
		}
	}
	if(hf!=NULL) {
		*hbody = hf->body;
		return 0;
	}
	return -1;
}


/**
 *
 */
int tps_append_xuuid(sip_msg_t *msg, str *hbody)
{
	if(tps_add_headers(msg, &_sr_hname_xuuid, hbody, 0)<0) {
		LM_ERR("failed to add xuuid header [%.*s]/%d\n",
				hbody->len, hbody->s, hbody->len);
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_remove_xuuid(sip_msg_t *msg)
{
	return tps_remove_name_headers(msg, &_sr_hname_xuuid);
}

/**
 *
 */
int tps_get_xuuid(sip_msg_t *msg, str *hbody)
{
	hdr_field_t *hf;
	if(parse_headers(msg, HDR_EOH_F, 0)<0) {
		return -1;
	}

	for (hf=msg->headers; hf; hf=hf->next)
	{
		if(_sr_hname_xuuid.len==hf->name.len
				&& strncasecmp(_sr_hname_xuuid.s, hf->name.s,
					hf->name.len)==0) {
			break;
		}
	}
	if(hf!=NULL) {
		*hbody = hf->body;
		return 0;
	}
	return -1;
}

/**
 *
 */
int tps_reappend_rr(sip_msg_t *msg, tps_data_t *ptsd, str *hbody)
{
	str hname = str_init("Record-Route");

	if (TPS_SPLIT_RECORD_ROUTE & _tps_header_mode)
		return tps_reappend_separate_header_values(msg, ptsd, hbody,&hname);

	if(tps_add_headers(msg, &hname, hbody, 0)<0) {
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_reappend_route(sip_msg_t *msg, tps_data_t *ptsd, str *hbody, int rev)
{
	str hname = str_init("Route");
	int i;
	int c;
	str sb;

	if(hbody==NULL || hbody->s==NULL || hbody->len<=0 || hbody->s[0]=='\0')
		return 0;

	if(rev==1) {
		c = 0;
		sb.len = 1;
		for(i=hbody->len-2; i>=0; i--) {
			if(hbody->s[i]==',') {
				c = 1;
				if(sb.len>0) {
					sb.s = hbody->s + i + 1;
					if(sb.s[sb.len-1]==',') sb.len--;
					if(tps_add_headers(msg, &hname, &sb, 0)<0) {
						return -1;
					}
				}
				sb.len = 0;
			}
			sb.len++;
		}
		if(c==1) {
			if(sb.len>0) {
				sb.s = hbody->s;
				if(sb.s[sb.len-1]==',') sb.len--;
				if(tps_add_headers(msg, &hname, &sb, 0)<0) {
					return -1;
				}
			}
			return 0;
		}
	}

	sb = *hbody;
	if(sb.len>0 && sb.s[sb.len-1]==',') sb.len--;
	trim_zeros_lr(&sb);
	trim(&sb);
	if(sb.len>0 && sb.s[sb.len-1]==',') sb.len--;
	if (TPS_SPLIT_ROUTE & _tps_header_mode)
		return tps_reappend_separate_header_values(msg, ptsd, &sb,&hname);
	if(tps_add_headers(msg, &hname, &sb, 0)<0) {
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_request_received(sip_msg_t *msg, int dialog)
{
	tps_data_t mtsd;
	tps_data_t stsd;
	str lkey;
	str nuri;
	uint32_t direction = TPS_DIR_DOWNSTREAM;
	int ret;
	int use_branch = 0;
	unsigned int metid = 0;

	LM_DBG("handling incoming request\n");

	if(dialog==0) {
		/* nothing to do for initial request */
		return 0;
	}

	memset(&mtsd, 0, sizeof(tps_data_t));
	memset(&stsd, 0, sizeof(tps_data_t));

	if(tps_pack_message(msg, &mtsd)<0) {
		LM_ERR("failed to extract and pack the headers\n");
		return -1;
	}
	
	tps_unmask_callid(msg);
	LM_DBG("Request message after CALLID Unmask-> [%.*s] \n",msg->len,msg->buf);
	
	ret = tps_dlg_message_update(msg, &mtsd, _tps_contact_mode);
	if(ret<0) {
		LM_ERR("failed to update on dlg message\n");
		return -1;
	}

	lkey = msg->callid->body;
	LM_DBG("callid [%.*s] - a_uuid [%.*s] - b_uuid [%.*s]\n", lkey.len, lkey.s,
			mtsd.a_uuid.len, ((mtsd.a_uuid.len>0)?mtsd.a_uuid.s:""),
			mtsd.b_uuid.len, ((mtsd.b_uuid.len>0)?mtsd.b_uuid.s:""));

	tps_storage_lock_get(&lkey);

	if(tps_storage_load_dialog(msg, &mtsd, &stsd) < 0) {
		goto error;
	}
	metid = get_cseq(msg)->method_id;
	if((metid & (METHOD_BYE|METHOD_INFO|METHOD_PRACK|METHOD_UPDATE))
			&& stsd.b_contact.len <= 0) {
		/* no B-side contact, look for INVITE transaction record */
		if(metid & (METHOD_BYE|METHOD_UPDATE)) {
			/* detect direction - via from-tag */
			if(tps_dlg_detect_direction(msg, &stsd, &direction) < 0) {
				goto error;
			}
		}
		if(tps_storage_link_msg(msg, &mtsd, direction) < 0) {
			goto error;
		}
		mtsd.direction = direction;
		memset(&stsd, 0, sizeof(tps_data_t));
		if(tps_storage_load_branch(msg, &mtsd, &stsd, 1) < 0) {
			goto error;
		}
		use_branch = 1;
	} else {
		/* detect direction - via from-tag */
		if(tps_dlg_detect_direction(msg, &stsd, &direction) < 0) {
			goto error;
		}
		mtsd.direction = direction;
	}

	tps_storage_lock_release(&lkey);

	if(use_branch && direction == TPS_DIR_DOWNSTREAM) {
		nuri = stsd.b_contact;
	} else {
		if(direction == TPS_DIR_UPSTREAM) {
			nuri = stsd.a_contact;
		} else {
			nuri = stsd.b_contact;
		}
	}
	if(nuri.len>0) {
		if(rewrite_uri(msg, &nuri)<0) {
			LM_ERR("failed to update r-uri\n");
			return -1;
		} else {
			LM_DBG("r-uri updated to: [%.*s]\n", nuri.len, nuri.s);
		}
	}

	if(use_branch) {
		LM_DBG("use branch for routing information, request from direction %d\n", direction);
		if(tps_reappend_route(msg, &stsd, &stsd.s_rr, 1) < 0) {
			LM_ERR("failed to reappend s-route\n");
			return -1;
		}
		if (direction == TPS_DIR_UPSTREAM) {
			if(tps_reappend_route(msg, &stsd, &stsd.x_rr, 0) < 0) {
				LM_ERR("failed to reappend a-route\n");
				return -1;
			}
		} else {
			if(tps_reappend_route(msg, &stsd, &stsd.y_rr, 1) < 0) {
				LM_ERR("failed to reappend b-route\n");
				return -1;
			}
		}
	} else {
		if(tps_reappend_route(msg, &stsd, &stsd.s_rr,
					(direction == TPS_DIR_UPSTREAM) ? 0 : 1) < 0) {
			LM_ERR("failed to reappend s-route\n");
			return -1;
		}
		if(direction == TPS_DIR_UPSTREAM) {
			if(tps_reappend_route(msg, &stsd, &stsd.a_rr, 0) < 0) {
				LM_ERR("failed to reappend a-route\n");
				return -1;
			}
		} else {
			if(tps_reappend_route(msg, &stsd, &stsd.b_rr, 1) < 0) {
				LM_ERR("failed to reappend b-route\n");
				return -1;
			}
		}
	}
	if(dialog!=0) {
		tps_append_xuuid(msg, &stsd.a_uuid);
		if(_tps_rr_update) {
			if(tps_storage_update_dialog(msg, &mtsd, &stsd, TPS_DBU_RPLATTRS|TPS_DBU_BRR)<0) {
				goto error;
			}
		}
		if(metid & METHOD_SUBSCRIBE) {
			if(tps_storage_update_dialog(msg, &mtsd, &stsd, TPS_DBU_CONTACT|TPS_DBU_TIME)<0) {
				goto error;
			}
		}
	}
	return 0;

error:
	tps_storage_lock_release(&lkey);
	return -1;
}

/**
 *
 */
int tps_response_received(sip_msg_t *msg)
{
	tps_data_t mtsd;
	tps_data_t stsd;
	tps_data_t btsd;
	str lkey;
	uint32_t direction = TPS_DIR_DOWNSTREAM;

	LM_DBG("handling incoming response\n");

	memset(&mtsd, 0, sizeof(tps_data_t));
	memset(&stsd, 0, sizeof(tps_data_t));
	memset(&btsd, 0, sizeof(tps_data_t));
	
	if(tps_pack_message(msg, &mtsd)<0) {
		LM_ERR("failed to extract and pack the headers\n");
		return -1;
	}
	tps_unmask_callid(msg);
	LM_DBG("Response message after CALLID Unmask-> [%.*s] \n",msg->len,msg->buf);
	
	lkey = msg->callid->body;
	
	tps_storage_lock_get(&lkey);
	if(tps_storage_load_branch(msg, &mtsd, &btsd, 0)<0) {
		goto error;
	}
	LM_DBG("loaded dialog a_uuid [%.*s]\n",
			btsd.a_uuid.len, ZSW(btsd.a_uuid.s));
	if(tps_storage_load_dialog(msg, &btsd, &stsd)<0) {
		goto error;
	}

	/* detect direction - via from-tag */
	if(tps_dlg_detect_direction(msg, &stsd, &direction)<0) {
		goto error;
	}
	mtsd.direction = direction;
	if(tps_storage_update_branch(msg, &mtsd, &btsd,
				TPS_DBU_CONTACT|TPS_DBU_RPLATTRS)<0) {
		goto error;
	}
	if(tps_storage_update_dialog(msg, &mtsd, &stsd,
			(_tps_rr_update)?(TPS_DBU_RPLATTRS|TPS_DBU_BRR|TPS_DBU_ARR)
					:TPS_DBU_RPLATTRS)<0) {
		goto error;
	}
	tps_storage_lock_release(&lkey);

	tps_reappend_via(msg, &btsd, &btsd.x_via);
	tps_reappend_rr(msg, &btsd, &btsd.s_rr);
	tps_reappend_rr(msg, &btsd, &btsd.x_rr);
	tps_append_xbranch(msg, &mtsd.x_vbranch1);

	return 0;

error:
	tps_storage_lock_release(&lkey);
	return -1;
}

/**
 *
 */
int tps_request_sent(sip_msg_t *msg, int dialog, int local)
{
	tps_data_t mtsd;
	tps_data_t btsd;
	tps_data_t stsd;
	tps_data_t *ptsd;
	str lkey;
	str xuuid;
	uint32_t direction = TPS_DIR_DOWNSTREAM;

	LM_DBG("handling outgoing request (%d, %d)\n", dialog, local);

	memset(&mtsd, 0, sizeof(tps_data_t));
	memset(&btsd, 0, sizeof(tps_data_t));
	memset(&stsd, 0, sizeof(tps_data_t));
	ptsd = NULL;

	if(tps_pack_message(msg, &mtsd)<0) {
		LM_ERR("failed to extract and pack the headers\n");
		return -1;
	}

	if(dialog!=0) {
		if(tps_get_xuuid(msg, &xuuid)<0) {
			LM_DBG("no x-uuid header -Local message only Call-ID Mask if downstream \n");
			/* ACK and CANCEL go downstream so Call-ID mask required*/
			if(get_cseq(msg)->method_id==METHOD_ACK
					|| get_cseq(msg)->method_id==METHOD_CANCEL) {
				tps_mask_callid(msg);
			}
			return 0;
		}
		mtsd.a_uuid = xuuid;
		tps_remove_xuuid(msg);
	}

	lkey = msg->callid->body;

	tps_storage_lock_get(&lkey);

	if(dialog!=0) {
		if(tps_storage_load_dialog(msg, &mtsd, &stsd)==0) {
			ptsd = &stsd;
		}
		/* detect direction - via from-tag */
		if(tps_dlg_detect_direction(msg, &stsd, &direction)<0) {
			goto error;
		}
		mtsd.direction = direction;
	}

	if(tps_storage_load_branch(msg, &mtsd, &btsd, 0)!=0) {
		if(tps_storage_record(msg, &mtsd, dialog, direction)<0) {
			goto error;
		}
	} else {
		if(ptsd==NULL) ptsd = &btsd;
	}

	if(ptsd==NULL) ptsd = &mtsd;

	/* local generated requests */
	if(local) {
		/* ACK and CANCEL go downstream */
		if(get_cseq(msg)->method_id==METHOD_ACK
				|| get_cseq(msg)->method_id==METHOD_CANCEL
				|| local==2) {
			// ts_mask_callid(&msg);
			goto done;
		} else {
			/* should be for upstream */
			goto done;
		}
	}

	tps_remove_headers(msg, HDR_RECORDROUTE_T);
	tps_remove_headers(msg, HDR_CONTACT_T);
	tps_remove_headers(msg, HDR_VIA_T);

	tps_reinsert_via(msg, &mtsd, &mtsd.x_via1);
	if(direction==TPS_DIR_UPSTREAM) {
		tps_reinsert_contact(msg, ptsd, &ptsd->as_contact);
	} else {
		tps_reinsert_contact(msg, ptsd, &ptsd->bs_contact);
	}

	if(dialog!=0) {
		tps_storage_end_dialog(msg, &mtsd, ptsd);
		if(tps_storage_update_dialog(msg, &mtsd, &stsd,
					(_tps_rr_update)?(TPS_DBU_CONTACT|TPS_DBU_ARR)
						:TPS_DBU_CONTACT)<0) {
			goto error;
		}
	}

done:
	tps_storage_lock_release(&lkey);
	/*DownStream Request sent MASK CALLID */
	if(direction == TPS_DIR_DOWNSTREAM) {
	    /*  mask CallID */
	    tps_mask_callid(msg);
	    LM_DBG("SENT message after CALLID CHG->[%.*s] \n",msg->len,msg->buf);
	}
	
	return 0;

error:
	tps_storage_lock_release(&lkey);
	return -1;
}

/**
 *
 */
int tps_response_sent(sip_msg_t *msg)
{
	tps_data_t mtsd;
	tps_data_t stsd;
	tps_data_t btsd;
	str lkey;
	uint32_t direction = TPS_DIR_UPSTREAM;
	str xvbranch = {0, 0};
	int contact_keep = 0;

	LM_DBG("handling outgoing response\n");

	memset(&mtsd, 0, sizeof(tps_data_t));
	memset(&stsd, 0, sizeof(tps_data_t));
	memset(&btsd, 0, sizeof(tps_data_t));

	if(tps_get_xbranch(msg, &xvbranch)<0) {
		LM_DBG("no x-branch header - nothing to do\n");
		return 0;
	}

	if(tps_pack_message(msg, &mtsd)<0) {
		LM_ERR("failed to extract and pack the headers\n");
		return -1;
	}
	mtsd.x_vbranch1 = xvbranch;
	tps_remove_xbranch(msg);

	if(get_cseq(msg)->method_id==METHOD_MESSAGE) {
		tps_remove_headers(msg, HDR_RECORDROUTE_T);
		tps_remove_headers(msg, HDR_CONTACT_T);
		return 0;
	}

	lkey = msg->callid->body;

	tps_storage_lock_get(&lkey);
	if(tps_storage_load_branch(msg, &mtsd, &btsd, 0)<0) {
		goto error;
	}
	LM_DBG("loaded branch a_uuid [%.*s]\n",
			btsd.a_uuid.len, ZSW(btsd.a_uuid.s));
	if(tps_storage_load_dialog(msg, &btsd, &stsd)<0) {
		goto error;
	}
	tps_storage_lock_release(&lkey);

	/* detect direction - via from-tag */
	if(tps_dlg_detect_direction(msg, &stsd, &direction)<0) {
		goto error1;
	}
	mtsd.direction = direction;

	tps_remove_headers(msg, HDR_RECORDROUTE_T);

	/* keep contact without updates for redirect responses sent out */
	if(msg->first_line.u.reply.statuscode>=300
			&& msg->first_line.u.reply.statuscode<400) {
		contact_keep = 1;
	}
	if(contact_keep==0 && msg->first_line.u.reply.statuscode>100
				&& msg->first_line.u.reply.statuscode<200
				&& msg->contact==NULL) {
		contact_keep = 1;
	}
	if(contact_keep==0 && msg->first_line.u.reply.statuscode>=400
				&& msg->first_line.u.reply.statuscode<500
				&& msg->contact==NULL) {
		contact_keep = 1;
	}
	if(contact_keep==0) {
		tps_remove_headers(msg, HDR_CONTACT_T);
		if(direction==TPS_DIR_DOWNSTREAM) {
			tps_reinsert_contact(msg, &stsd, &stsd.as_contact);
		} else {
			tps_reinsert_contact(msg, &stsd, &stsd.bs_contact);
		}
	}

	tps_reappend_rr(msg, &btsd, &btsd.x_rr);
	if(tps_storage_update_branch(msg, &mtsd, &btsd, TPS_DBU_CONTACT)<0) {
		goto error;
	}
	if(tps_storage_update_dialog(msg, &mtsd, &stsd, TPS_DBU_CONTACT)<0) {
		goto error1;
	}
	
	/*DownStream Response sent MASK CALLID */
	if(direction == TPS_DIR_UPSTREAM) {
	    /*  mask CallID */
	    tps_mask_callid(msg);
	    LM_DBG("SENT Response message after CALLID CHG->[%.*s] \n",msg->len,msg->buf);
	}
	return 0;

error:
	tps_storage_lock_release(&lkey);
error1:
	return -1;
}
