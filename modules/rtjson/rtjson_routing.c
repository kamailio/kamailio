/**
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../xavp.h"
#include "../../dset.h"
#include "../../mem/shm_mem.h"
#include "../../data_lump.h"
#include "../../lib/srutils/srjson.h"
#include "../../modules/tm/tm_load.h"
#include "../../modules/uac/api.h"

#include "rtjson_routing.h"

typedef struct rtjson_data {
	srjson_doc_t *jdoc;
	int idx;
} rtjson_data_t;

extern str _rtjson_xavp_name;

int rtjson_init_serial(sip_msg_t *msg, srjson_doc_t *jdoc, sr_xavp_t *iavp);
int rtjson_init_parallel(sip_msg_t *msg, srjson_doc_t *jdoc, sr_xavp_t *iavp);

/* tm */
static struct tm_binds tmb;
/* uac */
static uac_api_t uacb;

int rtjson_init(void)
{
	if (load_tm_api( &tmb ) == -1) {
		LM_NOTICE("cannot load the TM API - some features are diabled\n");
		memset(&tmb, 0, sizeof(struct tm_binds));
	}
	if (load_uac_api(&uacb) < 0) {
		LM_NOTICE("cannot bind to UAC API - some features are diabled\n");
		memset(&uacb, 0, sizeof(uac_api_t));
	}
	return 0;
}

#ifdef RTJSON_STORE_SHM
/**
 *
 */
void rtjson_data_free(void *ptr, sr_xavp_sfree_f sfree)
{
	rtjson_data_t *rdata;

	rdata = (rtjson_data_t*)ptr;

	if(rdata->jdoc) {
		rdata->jdoc->free_fn = sfree;
		srjson_DeleteDoc(rdata->jdoc);
	}
	sfree(ptr);
}

/**
 *
 */
void *rtjson_malloc(size_t sz)
{
	return shm_malloc(sz);
}

/**
 *
 */
void rtjson_free(void *ptr)
{
	shm_free(ptr);
}

/**
 *
 */
int rtjson_init_routes(sip_msg_t *msg, str *rdoc)
{
	srjson_Hooks jhooks;
	srjson_doc_t *tdoc = NULL;
	sr_data_t *xdata = NULL;
	rtjson_data_t *rdata = NULL;
	sr_xavp_t *xavp=NULL;
	str xname;
	sr_xval_t xval;


	memset(&jhooks, 0, sizeof(srjson_Hooks));
	jhooks.malloc_fn = rtjson_malloc;
	jhooks.free_fn = rtjson_free;

	tdoc = srjson_NewDoc(&jhooks);

	if(tdoc==NULL) {
		LM_ERR("no more shm\n");
		return -1;
	}
	tdoc->root = srjson_Parse(tdoc, rdoc->s);
	if(tdoc->root == NULL) {
		LM_ERR("invalid json doc [[%s]]\n", rdoc->s);
		srjson_DeleteDoc(tdoc);
		return -1;
	}
	xdata = shm_malloc(sizeof(sr_data_t));
	if(xdata==NULL) {
		LM_ERR("no more shm\n");
		srjson_DeleteDoc(tdoc);
		return -1;
	}
	memset(xdata, 0, sizeof(sr_data_t));
	rdata = shm_malloc(sizeof(rtjson_data_t));
	if(rdata==NULL) {
		LM_ERR("no more shm\n");
		srjson_DeleteDoc(tdoc);
		shm_free(xdata);
		return -1;
	}
	memset(rdata, 0, sizeof(rtjson_data_t));

	rdata->jdoc = tdoc;
	xdata->p = rdata;
	xdata->pfree = rtjson_data_free;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_STR;
	xval.v.s = *rdoc;
	xname.s = "json";
	xname.len = 4;
	if(xavp_add_value(&xname, &xval, &xavp)==NULL) {
		goto error;
	}

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_DATA;
	xval.v.data = xdata;
	xname.s = "data";
	xname.len = 4;
	if(xavp_add_value(&xname, &xval, &xavp)==NULL) {
		goto error;
	}
	/* reset pointers - they are linked inside xavp now */
	tdoc = NULL;
	xdata = NULL;
	rdata = NULL;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_XAVP;
	xval.v.xavp = xavp;
	if(xavp_add_value(&_rtjson_xavp_name, &xval, NULL)==NULL) {
		goto error;
	}

	return 0;
error:
	if(xavp) xavp_destroy_list(&xavp);
	if(rdata) shm_free(rdata);
	if(xdata) shm_free(xdata);
	if(tdoc) srjson_DeleteDoc(tdoc);
	return -1;
}
#else

/**
 *
 */
int rtjson_init_routes(sip_msg_t *msg, str *rdoc)
{
	sr_xavp_t *xavp=NULL;
	str xname;
	sr_xval_t xval;
	srjson_doc_t tdoc;

	srjson_InitDoc(&tdoc, NULL);

	tdoc.root = srjson_Parse(&tdoc, rdoc->s);
	if(tdoc.root == NULL) {
		LM_ERR("invalid json doc [[%s]]\n", rdoc->s);
		srjson_DestroyDoc(&tdoc);
		return -1;
	}

	/* basic validation */

	srjson_DestroyDoc(&tdoc);

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_INT;
	xval.v.i = 0;
	xname.s = "idx";
	xname.len = 3;
	if(xavp_add_value(&xname, &xval, &xavp)==NULL) {
		goto error;
	}

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_STR;
	xval.v.s = *rdoc;
	xname.s = "json";
	xname.len = 4;
	if(xavp_add_value(&xname, &xval, &xavp)==NULL) {
		goto error;
	}

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_XAVP;
	xval.v.xavp = xavp;
	if(xavp_add_value(&_rtjson_xavp_name, &xval, NULL)==NULL) {
		goto error;
	}

	return 0;

error:
	if(xavp) xavp_destroy_list(&xavp);
	return -1;
}
#endif

/**
 *
 */
int rtjson_push_routes(sip_msg_t *msg)
{
	sr_xavp_t *javp = NULL;
	sr_xavp_t *iavp = NULL;
	srjson_doc_t tdoc;
	srjson_t *nj = NULL;
	str val;
	str xname;
	int ret;

	xname.s = "json";
	xname.len = 4;
	javp = xavp_get_child_with_sval(&_rtjson_xavp_name, &xname);
	if(javp==NULL || javp->val.v.s.len<=0) {
		LM_WARN("no json for routing\n");
		return -1;
	}

	xname.s = "idx";
	xname.len = 3;
	iavp = xavp_get_child_with_ival(&_rtjson_xavp_name, &xname);
	if(iavp==NULL) {
		LM_WARN("no idx for routing\n");
		return -1;
	}

	srjson_InitDoc(&tdoc, NULL);

	tdoc.root = srjson_Parse(&tdoc, javp->val.v.s.s);
	if(tdoc.root == NULL) {
		LM_ERR("invalid json doc [[%s]]\n", javp->val.v.s.s);
		srjson_DestroyDoc(&tdoc);
		return -1;
	}

	nj = srjson_GetObjectItem(&tdoc, tdoc.root, "routing");
	if(nj==NULL || nj->valuestring==NULL) {
		LM_ERR("missing or invalid routing field\n");
		goto error;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);

	if(val.len==6 && strncmp(val.s, "serial", 6)==0) {
		LM_DBG("supported routing [%.*s]\n", val.len, val.s);
		ret = rtjson_init_serial(msg, &tdoc, iavp);
	} else if(val.len==8 && strncmp(val.s, "parallel", 8)==0) {
		LM_DBG("supported routing [%.*s]\n", val.len, val.s);
		ret = rtjson_init_parallel(msg, &tdoc, iavp);
	} else {
		LM_ERR("unsupported routing [%.*s]\n", val.len, val.s);
		goto error;
	}

	srjson_DestroyDoc(&tdoc);
	return ret;

error:
	srjson_DestroyDoc(&tdoc);
	return -1;
}

/**
 *
 */
int rtjson_init_serial(sip_msg_t *msg, srjson_doc_t *jdoc, sr_xavp_t *iavp)
{
	srjson_t *tj = NULL;
	srjson_t *nj = NULL;
	srjson_t *rj = NULL;
	str val;
	unsigned int bflags = 0;
	unsigned int old_bflags = 0;

	tj = srjson_GetObjectItem(jdoc, jdoc->root, "routes");
	if(tj==NULL || tj->type!=srjson_Array || tj->child==NULL) {
		LM_ERR("missing or invalid routes field\n");
		goto error;
	}
	nj = tj->child;

	clear_branches();

	rj = srjson_GetObjectItem(jdoc, nj, "uri");
	if(rj!=NULL && rj->type==srjson_String && rj->valuestring!=NULL) {
		val.s = rj->valuestring;
		val.len = strlen(val.s);
		LM_DBG("rewrite r-uri to: [%.*s]\n", val.len, val.s);
		if (rewrite_uri(msg, &val) < 0) {
			LM_ERR("unable to rewrite Request-URI\n");
			goto error;
		}
	}

	reset_dst_uri(msg);
	reset_path_vector(msg);
	reset_instance(msg);
	reset_ruid(msg);
	reset_ua(msg);
	reset_force_socket(msg);
	msg->reg_id = 0;
	set_ruri_q(0);

	rj = srjson_GetObjectItem(jdoc, nj, "dst_uri");
	if(rj!=NULL && rj->type==srjson_String && rj->valuestring!=NULL) {
		val.s = rj->valuestring;
		val.len = strlen(val.s);
		LM_DBG("rewrite dst-uri to: [%.*s]\n", val.len, val.s);
		if (set_dst_uri(msg, &val) < 0) {
			LM_ERR("unable to set destination uri\n");
			goto error;
		}
	}

	rj = srjson_GetObjectItem(jdoc, nj, "path");
	if(rj!=NULL && rj->type==srjson_String && rj->valuestring!=NULL) {
		val.s = rj->valuestring;
		val.len = strlen(val.s);
		LM_DBG("rewrite path to: [%.*s]\n", val.len, val.s);
		if (set_path_vector(msg, &val) < 0) {
			LM_ERR("unable to set path\n");
			goto error;
		}
	}

	rj = srjson_GetObjectItem(jdoc, nj, "branch_flags");
	if(rj!=NULL && rj->type==srjson_Number && rj->valueint!=0) {
		bflags = rj->valueint;

		old_bflags = 0;
		getbflagsval(0, &old_bflags);
		setbflagsval(0, old_bflags|bflags);
	}

	iavp->val.v.i++;

	return 0;

error:
	return -1;
}

/**
 *
 */
int rtjson_prepare_branch(sip_msg_t *msg, srjson_doc_t *jdoc, srjson_t *nj)
{
	srjson_t *rj = NULL;
	srjson_t *tj = NULL;
	srjson_t *vj = NULL;
	str xdsp = {0};
	str xuri = {0};
	str xhdr = {0};
	unsigned int fr = 0;
	unsigned int fr_inv = 0;
	struct lump *anchor = NULL;
	char *s;


	if(tmb.set_fr!=NULL) {
		rj = srjson_GetObjectItem(jdoc, nj, "fr_timer");
		if(rj!=NULL && rj->type==srjson_Number && rj->valueint!=0) {
			fr = rj->valueint;
		}
		rj = srjson_GetObjectItem(jdoc, nj, "fr_inv_timer");
		if(rj!=NULL && rj->type==srjson_Number && rj->valueint!=0) {
			fr_inv = rj->valueint;
		}
		if(fr || fr_inv) tmb.set_fr(msg, fr_inv, fr);
	}
	rj = srjson_GetObjectItem(jdoc, nj, "headers");
	if(rj==NULL || rj->type!=srjson_Object || rj->child==NULL) {
		LM_DBG("no header operations - done\n");
		return 0;
	}

	tj = srjson_GetObjectItem(jdoc, rj, "extra");
	if(tj!=NULL && tj->type==srjson_String && tj->valuestring!=0) {
		xhdr.s =  tj->valuestring;
		xhdr.len = strlen(xhdr.s);
	}

	if(xhdr.len>4) {
		LM_DBG("appending extra headers: [%.*s]\n", xhdr.len, xhdr.s);
		anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
		if(anchor == 0) {
			LM_ERR("can't get anchor\n");
			return -1;
		}
		s = pkg_malloc(xhdr.len+1);
		if(s==NULL) {
			LM_ERR("no more pkg\n");
			return -1;
		}
		strncpy(s, xhdr.s, xhdr.len);
		s[xhdr.len] = '\0';
		if (insert_new_lump_before(anchor, s, xhdr.len, 0) == 0) {
			LM_ERR("can't insert lump\n");
			pkg_free(s);
			return -1;
		}
	}

	if(uacb.replace_from!=NULL) {
		tj = srjson_GetObjectItem(jdoc, rj, "from");
		if(tj!=NULL && tj->type==srjson_Object && rj->child!=NULL) {
			vj = srjson_GetObjectItem(jdoc, tj, "display");
			if(vj!=NULL && vj->type==srjson_String && vj->valuestring!=0) {
				xdsp.s =  vj->valuestring;
				xdsp.len = strlen(xdsp.s);
			}
			vj = srjson_GetObjectItem(jdoc, tj, "uri");
			if(vj!=NULL && vj->type==srjson_String && vj->valuestring!=0) {
				xuri.s =  vj->valuestring;
				xuri.len = strlen(xuri.s);
			}
			if(xdsp.len>0 || xuri.len>0) {
				uacb.replace_from(msg, &xdsp, &xuri);
			}
		}
	}

	if(uacb.replace_to!=NULL) {
		tj = srjson_GetObjectItem(jdoc, rj, "to");
		if(tj!=NULL && tj->type==srjson_Object && rj->child!=NULL) {
			vj = srjson_GetObjectItem(jdoc, tj, "display");
			if(vj!=NULL && vj->type==srjson_String && vj->valuestring!=0) {
				xdsp.s =  vj->valuestring;
				xdsp.len = strlen(xdsp.s);
			}
			vj = srjson_GetObjectItem(jdoc, tj, "uri");
			if(vj!=NULL && vj->type==srjson_String && vj->valuestring!=0) {
				xuri.s =  vj->valuestring;
				xuri.len = strlen(xuri.s);
			}
			if(xdsp.len>0 || xuri.len>0) {
				uacb.replace_from(msg, &xdsp, &xuri);
			}
		}
	}

	return 0;
}

/**
 *
 */
int rtjson_append_branch(sip_msg_t *msg, srjson_doc_t *jdoc, srjson_t *nj)
{
	srjson_t *rj = NULL;
	str uri = {0};
	str duri = {0};
	str path = {0};
	struct socket_info* fsocket = NULL;
	unsigned int bflags = 0;

	rj = srjson_GetObjectItem(jdoc, nj, "uri");
	if(rj==NULL || rj->type!=srjson_String || rj->valuestring==NULL) {
		return -1;
	}

	uri.s = rj->valuestring;
	uri.len = strlen(uri.s);

	rj = srjson_GetObjectItem(jdoc, nj, "dst_uri");
	if(rj!=NULL && rj->type==srjson_String && rj->valuestring!=NULL) {
		duri.s = rj->valuestring;
		duri.len = strlen(duri.s);
	}
	rj = srjson_GetObjectItem(jdoc, nj, "path");
	if(rj!=NULL && rj->type==srjson_String && rj->valuestring!=NULL) {
		path.s = rj->valuestring;
		path.len = strlen(path.s);
	}
	
	if (append_branch(msg, &uri, &duri, &path, 0, bflags,
					  fsocket, 0 /*instance*/, 0,
					  0, 0) <0) {
		LM_ERR("failed to append branch\n");
		goto error;
	}

	return 0;

error:
	return -1;
}

/**
 *
 */
int rtjson_init_parallel(sip_msg_t *msg, srjson_doc_t *jdoc, sr_xavp_t *iavp)
{
	srjson_t *tj = NULL;
	srjson_t *nj = NULL;
	int ret;

	tj = srjson_GetObjectItem(jdoc, jdoc->root, "routes");
	if(tj==NULL || tj->type!=srjson_Array || tj->child==NULL) {
		LM_ERR("missing or invalid routes field\n");
		goto error;
	}
	nj = tj->child;

	ret = rtjson_init_serial(msg, jdoc, iavp);
	if(ret<0)
		return ret;

	/* skip first - used for r-uri */
	nj = nj->next;

	while(nj) {
		rtjson_append_branch(msg, jdoc, nj);

		iavp->val.v.i++;
		nj = nj->next;
	}

	return 0;

error:
	return -1;
}

/**
 *
 */
int rtjson_next_route(sip_msg_t *msg)
{
	sr_xavp_t *javp = NULL;
	sr_xavp_t *iavp = NULL;
	srjson_doc_t tdoc;
	srjson_t *tj = NULL;
	srjson_t *nj = NULL;
	str val;
	str xname;
	int i;

	xname.s = "json";
	xname.len = 4;
	javp = xavp_get_child_with_sval(&_rtjson_xavp_name, &xname);
	if(javp==NULL || javp->val.v.s.len<=0) {
		LM_WARN("no json for routing\n");
		return -1;
	}

	xname.s = "idx";
	xname.len = 3;
	iavp = xavp_get_child_with_ival(&_rtjson_xavp_name, &xname);
	if(iavp==NULL) {
		LM_WARN("no idx for routing\n");
		return -1;
	}

	srjson_InitDoc(&tdoc, NULL);

	tdoc.root = srjson_Parse(&tdoc, javp->val.v.s.s);
	if(tdoc.root == NULL) {
		LM_ERR("invalid json doc [[%s]]\n", javp->val.v.s.s);
		srjson_DestroyDoc(&tdoc);
		return -1;
	}

	nj = srjson_GetObjectItem(&tdoc, tdoc.root, "routing");
	if(nj==NULL || nj->valuestring==NULL) {
		LM_ERR("missing or invalid routing field\n");
		goto error;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);

	if(val.len!=6 || strncmp(val.s, "serial", 6)!=0) {
		LM_DBG("not serial routing [%.*s]\n", val.len, val.s);
		goto error;
	}

	tj = srjson_GetObjectItem(&tdoc, tdoc.root, "routes");
	if(tj==NULL || tj->type!=srjson_Array || tj->child==NULL) {
		LM_ERR("missing or invalid routes field\n");
		goto error;
	}
	nj = tj->child;

	i = 0;
	while(nj && i<iavp->val.v.i) {
		nj = nj->next;
		i++;
	}
	if(nj==NULL)
		goto error;

	iavp->val.v.i++;
	if(rtjson_append_branch(msg, &tdoc, nj)<0)
		goto error;

	srjson_DestroyDoc(&tdoc);
	return 0;

error:
	srjson_DestroyDoc(&tdoc);
	return -1;
}

/**
 *
 */
int rtjson_update_branch(sip_msg_t *msg)
{
	sr_xavp_t *javp = NULL;
	sr_xavp_t *iavp = NULL;
	srjson_doc_t tdoc;
	srjson_t *tj = NULL;
	srjson_t *nj = NULL;
	str val;
	str xname;
	int i;

	xname.s = "json";
	xname.len = 4;
	javp = xavp_get_child_with_sval(&_rtjson_xavp_name, &xname);
	if(javp==NULL || javp->val.v.s.len<=0) {
		LM_WARN("no json for routing\n");
		return -1;
	}

	xname.s = "idx";
	xname.len = 3;
	iavp = xavp_get_child_with_ival(&_rtjson_xavp_name, &xname);
	if(iavp==NULL) {
		LM_WARN("no idx for routing\n");
		return -1;
	}
	if(iavp->val.v.i<=0) {
		LM_WARN("invalid branch idx for routing\n");
		return -1;
	}

	srjson_InitDoc(&tdoc, NULL);

	tdoc.root = srjson_Parse(&tdoc, javp->val.v.s.s);
	if(tdoc.root == NULL) {
		LM_ERR("invalid json doc [[%s]]\n", javp->val.v.s.s);
		srjson_DestroyDoc(&tdoc);
		return -1;
	}

	nj = srjson_GetObjectItem(&tdoc, tdoc.root, "routing");
	if(nj==NULL || nj->valuestring==NULL) {
		LM_ERR("missing or invalid routing field\n");
		goto error;
	}
	val.s = nj->valuestring;
	val.len = strlen(val.s);

	if(val.len!=6 || strncmp(val.s, "serial", 6)!=0) {
		LM_DBG("not serial routing [%.*s]\n", val.len, val.s);
		goto error;
	}

	tj = srjson_GetObjectItem(&tdoc, tdoc.root, "routes");
	if(tj==NULL || tj->type!=srjson_Array || tj->child==NULL) {
		LM_ERR("missing or invalid routes field\n");
		goto error;
	}
	nj = tj->child;

	i = 0;
	/* stop at number of branches - 1 */
	while(nj && i<iavp->val.v.i-1) {
		nj = nj->next;
		i++;
	}
	if(nj==NULL)
		goto error;

	if(rtjson_prepare_branch(msg, &tdoc, nj)<0)
		goto error;

	srjson_DestroyDoc(&tdoc);
	return 0;

error:
	srjson_DestroyDoc(&tdoc);
	return -1;

}

