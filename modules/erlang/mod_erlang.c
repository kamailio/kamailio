/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "../../ver.h"
#include "../../sr_module.h"
#include "../../pt.h"
#include "../../cfg/cfg_struct.h"
#include "../../mod_fix.h"
#include "../../script_cb.h"

#ifndef USE_TCP
#error	"USE_TCP must be enabled for this module"
#endif

#include "../../pass_fd.h"

#include "mod_erlang.h"
#include "erl_helpers.h"
#include "cnode.h"
#include "handle_rpc.h"

#include "pv_xbuff.h"
#include "pv_tuple.h"
#include "pv_atom.h"
#include "pv_list.h"
#include "pv_pid.h"
#include "pv_ref.h"

MODULE_VERSION

/* module exports */
static int child_init(int rank);
static int mod_init(void);
static void mod_destroy(void);
static int postprocess_request(struct sip_msg *msg, unsigned int flags, void *_param);

/*  exported functions */
static int erl_rpc(struct sip_msg *msg, char *module, char *function, char *args, char *reply);
static int erl_reg_send_k(struct sip_msg *msg, char *_server, char *_emsg);
static int erl_send_k(struct sip_msg *msg, char *_server, char *_emsg);
static int erl_reply_k(struct sip_msg *msg, char *_emsg);

/* fix-ups */
static int fixup_rpc(void** param, int param_no);
static int fixup_send(void** param, int param_no);
static int fixup_reg(void** param, int param_no);
static int fixup_reply(void** param, int param_no);

/* initialize common vars */
str cookie = STR_NULL;
int trace_level = 0;
str cnode_alivename = STR_NULL;
str cnode_host = STR_NULL;
int no_cnodes=1;
int rpc_reply_with_struct = 0;

str erlang_nodename  = STR_NULL;
str erlang_node_sname = STR_NULL;

int *usocks[2];
int csockfd;

handler_common_t* io_handlers = NULL;

erl_api_t erl_api;

static pv_export_t pvs[] = {
		{
				{ "erl_tuple", (sizeof("erl_tuple")-1) },
				PVT_OTHER,
				pv_tuple_get,
				pv_tuple_set,
				pv_xbuff_parse_name,
				0,
				0,
				0
		},
		{
				{ "erl_atom", (sizeof("erl_atom")-1) },
				PVT_OTHER,
				pv_atom_get,
				pv_atom_set,
				pv_atom_parse_name,
				0,
				0,
				0
		},
		{
				{ "erl_list", (sizeof("erl_list")-1) },
				PVT_OTHER,
				pv_list_get,
				pv_list_set,
				pv_xbuff_parse_name,
				0,
				0,
				0
		},
		{
				{ "erl_xbuff", (sizeof("erl_xbuff")-1) },
				PVT_OTHER,
				pv_xbuff_get,
				pv_xbuff_set,
				pv_xbuff_parse_name,
				0,
				0,
				0
		},
		{
				{ "erl_pid", (sizeof("erl_pid")-1) },
				PVT_OTHER,
				pv_pid_get,
				pv_pid_set,
				pv_pid_parse_name,
				0,
				0,
				0
		},
		{
				{ "erl_ref", (sizeof("erl_ref")-1) },
				PVT_OTHER,
				pv_ref_get,
				pv_ref_set,
				pv_ref_parse_name,
				0,
				0,
				0
		},
		{{0,0}, 0, 0, 0, 0, 0, 0, 0}
};

/* exported parameters */
static param_export_t parameters[] =
{
		/* Kamailo C node parameters */
		{ "no_cnodes", PARAM_INT, &no_cnodes },
		{ "cnode_alivename", PARAM_STR, &cnode_alivename },
		{ "cnode_host", PARAM_STR, &cnode_host },

		/* Erlang node parameters */
		{ "erlang_nodename", PARAM_STR, &erlang_nodename },
		{ "erlang_node_sname", PARAM_STR, &erlang_node_sname },

		{ "cookie", PARAM_STR, &cookie },
		{ "trace_level", PARAM_INT, &trace_level }, /* tracing level on the distribution */
		{ "rpc_reply_with_struct", PARAM_INT, &rpc_reply_with_struct},
		{ 0, 0, 0 }
};

/* exported commands */

static cmd_export_t commands[] =
{
		{"erl_rpc", (cmd_function)erl_rpc, 4, fixup_rpc, 0, ANY_ROUTE},
		{"erl_send", (cmd_function)erl_send_k, 2, fixup_send, 0, ANY_ROUTE},
		{"erl_reg_send", (cmd_function)erl_reg_send_k, 2, fixup_reg, 0, ANY_ROUTE},
		{"erl_reply", (cmd_function)erl_reply_k, 1, fixup_reply, 0, EVENT_ROUTE},
		{"load_erl",(cmd_function)load_erl,0, 0,         0,         0}, /* API loader */
		{ 0, 0, 0, 0, 0, 0 }
};


struct module_exports exports = {
		"erlang",
		DEFAULT_DLFLAGS,
		commands,
		parameters,
		NULL,
		NULL,
		pvs,
		NULL,
		mod_init,
		NULL,
		mod_destroy,
		child_init
};

/**
 * \brief Initialize Erlang module
 */
static int mod_init(void)
{
	/* check required parameters */

	if (!cookie.s || cookie.len == 0)
	{
		LM_CRIT("Erlang cookie parameter is required\n");
		return -1;
	}
	cookie.s[cookie.len]=0;

	if ((!erlang_nodename.s || erlang_nodename.len == 0)
			&& (!erlang_node_sname.s || erlang_node_sname.len == 0)) {
		LM_CRIT("Erlang node name is required\n");
		return -1;
	}
	if (erlang_nodename.s) {
		erlang_nodename.s[erlang_nodename.len]=0;
	}
	if (erlang_node_sname.s) {
		erlang_node_sname.s[erlang_node_sname.len]=0;
	}

	if (!cnode_alivename.s || !cnode_alivename.len) {
		LM_CRIT("Kamailio C node alive name is required\n");
		return -1;
	}
	cnode_alivename.s[cnode_alivename.len]=0;

	if (!cnode_host.s || !cnode_host.len) {
		LM_WARN("Kamailio host name is not set, trying with gethostname...\n");
		return -1;
	}

	if (erl_load_api(&erl_api)) {
		LM_CRIT("failed to load erl API\n");
		return -1;
	}

	if (compile_xbuff_re()) {
		return -1;
	}

	if (register_script_cb(postprocess_request, POST_SCRIPT_CB | REQUEST_CB, 0)
			!= 0)
	{
		LOG(L_CRIT, "could not register request post processing call back.\n");
		return -1;
	}

	/* init RPC handler for Erlang calls */
	init_rpc_handlers();

	/* add space for extra processes */
	register_procs(no_cnodes);

	/* add child to update local config framework structures */
	cfg_register_child(no_cnodes);

	return 0;
}

#define MAX_CNODE_DESC_LEN (MAXNODELEN + sizeof("Erlang C node "))

/**
 * \brief Initialize Erlang module children
 */
static int child_init(int rank)
{
	char _cnode_desc[MAX_CNODE_DESC_LEN];
	int pair[2], data[2];
	int i,j,pid;

	if (rank == PROC_INIT) {

#ifdef SHM_MEM
		usocks[KSOCKET]=(int*)shm_malloc(sizeof(int)*no_cnodes);
		if (!usocks[KSOCKET]) {
			LM_ERR("Not enough memory\n");
			return -1;
		}

		usocks[CSOCKET]=(int*)shm_malloc(sizeof(int)*no_cnodes);
		if (!usocks[CSOCKET]) {
			LM_ERR("Not enough memory\n");
			return -1;
		}
#else
		usocks[KSOCKET]=(int*)pkg_malloc(sizeof(int)*no_cnodes);
		if (!usocks[KSOCKET]) {
			LM_ERR("Not enough memory\n");
			return -1;
		}

		usocks[CSOCKET]=(int*)pkg_malloc(sizeof(int)*no_cnodes);
		if (!usocks[CSOCKET]) {
			LM_ERR("Not enough memory\n");
			return -1;
		}
#endif

		for(i=0;i<no_cnodes;i++) {
			if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair)) {
				LOG(L_CRIT,"failed create socket pair: %s. -- don't start -- \n", strerror(errno));
				return -1;
			}
			usocks[KSOCKET][i]=pair[KSOCKET];
			usocks[CSOCKET][i]=pair[CSOCKET];
		}

		return 0;
	}

	if (rank == PROC_MAIN) {
		for (j=0; j<no_cnodes; j++) {

			snprintf(_cnode_desc, MAX_CNODE_DESC_LEN, "%s%.*s%d@%.*s", "Erlang C Node ", STR_FMT(&cnode_alivename), j+1, STR_FMT(&cnode_host));

			if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair)) {
				LOG(L_CRIT,"failed create socket pair: %s. -- don't start -- \n", strerror(errno));
				return -1;
			}

			if ((pid = fork_process(PROC_NOCHLDINIT, _cnode_desc, 0)) == -1) {
				return -1; /* error -- don't start -- */
			} else if (pid == 0) {
				/* child */
				if(cfg_child_init()) {
					LM_CRIT("failed cfg_child_init\n");
					return -1;
				}

				for (i=0;i<no_cnodes;i++)
				{
					close(usocks[KSOCKET][i]);
					if (i!=j) close(usocks[CSOCKET][i]);
				}

				csockfd = usocks[CSOCKET][j];

				/* enter Erlang C Node main loop (cnode process) */
				cnode_main_loop(j);

				LM_CRIT("failed to start Erlang C node main loop!\n");

				return -1;
			}

			/* parent */
		}

		return 0;
	}

	for (i=0;i<no_cnodes;i++) {
		close(usocks[CSOCKET][i]);
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair)) {
		LOG(L_CRIT,"failed create socket pair: %s. -- don't start -- \n", strerror(errno));
		return -1;
	}

	data[0] = pair[CSOCKET];
	data[1] = my_pid();

	if (send_fd(usocks[KSOCKET][process_no % no_cnodes],(void*)&data,sizeof(data),pair[CSOCKET])==-1) {
		LM_CRIT("failed to send socket %d over socket %d to cnode\n",pair[CSOCKET],usocks[KSOCKET][process_no % no_cnodes]);
		return -1;
	}

	csockfd = pair[KSOCKET];

	erl_set_nonblock(csockfd);

	erl_init_common();

	for (i=0;i<no_cnodes;i++) {
		close(usocks[KSOCKET][i]);
	}

	return 0;
}

/**
 * @brief Destroy module allocated resources
 */
static void mod_destroy(void)
{
#ifdef SHM_MEM
		shm_free(usocks[0]);
		shm_free(usocks[1]);
#else
		pkg_free(usocks[0]);
		pkg_free(usocks[1]);
#endif
		free_tuple_fmt_buff();
		free_atom_fmt_buff();
		free_list_fmt_buff();
		free_xbuff_fmt_buff();
		free_pid_fmt_buff();
		free_ref_fmt_buff();
}

static int postprocess_request(struct sip_msg *msg, unsigned int flags, void *_param)
{
	free_tuple_fmt_buff();
	free_atom_fmt_buff();
	free_list_fmt_buff();
	free_xbuff_fmt_buff();
	free_pid_fmt_buff();
	free_ref_fmt_buff();
	return 0;
}

/**
 * Erlang RPC.
 */
static int erl_rpc(struct sip_msg *msg, char *_m, char *_f, char *_a, char *_r)
{
	erl_param_t *m=(erl_param_t*)_m;
	erl_param_t *f=(erl_param_t*)_f;
	erl_param_t *a=(erl_param_t*)_a;
	erl_param_t *r=(erl_param_t*)_r;

	str module;
	str function;
	str vname;
	sr_xavp_t *xreq=NULL;
	sr_xavp_t *xrepl=NULL;
	pv_spec_t sp;
	pv_spec_t *nsp = NULL;
	pv_param_t  pvp;
	pv_name_t *pvn;
	pv_index_t *pvi;
	int idx;
	int idxf;
	int attr;
	ei_x_buff ei_req;
	ei_x_buff ei_rep;

	switch (m->type) {
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&module,msg,&m->value.fp)) {
			LM_ERR("can't get module name\n");
		}
		break;
	default:
		LM_ERR("unexpected type for module name parameter\n");
		return -1;
	}

	switch (f->type) {
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&function,msg,&f->value.fp)) {
			LM_ERR("can't get function name\n");
		}
		break;
	default:
		LM_ERR("unexpected type for function name parameter\n");
		return -1;
	}

	switch(a->type){
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&vname,msg,&a->value.fp)){
			LM_ERR("can't get name of arguments parameter\n");
			return -1;
		}
		xreq = pv_list_get_list(&vname);
		if (!xreq){
			xreq = pv_xbuff_get_xbuff(&vname);
		}
		if (!xreq) {
			LM_ERR("can't find variable $list(%.*s) nor $xbuff(%.*s)",STR_FMT(&vname),STR_FMT(&vname));
			return -1;
		}
		break;
	case ERL_PARAM_XBUFF_SPEC:
		sp = a->value.sp;
		pvp = sp.pvp; /* work on copy */

		if (pvp.pvn.type != PV_NAME_INTSTR || !(pvp.pvn.u.isname.type & AVP_NAME_STR)) {
			LM_ERR("unsupported name of list\n");
			return -1;
		}

		if( pvp.pvn.type == PV_NAME_PVAR) {
			nsp = pvp.pvn.u.dname;
		}

		if (nsp) {
			pvi = &nsp->pvp.pvi;
			pvn = &nsp->pvp.pvn;
		} else {
			pvi = &pvp.pvi;
			pvn = &pvp.pvn;
		}

		if (sp.setf == pv_list_set ) {
			xreq = pv_list_get_list(&pvn->u.isname.name.s);
		} else if (sp.setf == pv_xbuff_set) {
			xreq = pv_xbuff_get_xbuff(&pvn->u.isname.name.s);
		}  else if (sp.setf == pv_tuple_set) {
			xreq = pv_tuple_get_tuple(&pvn->u.isname.name.s);
		}

		/* fix index */
		attr = xbuff_get_attr_flags(pvi->type);
		pvi->type = xbuff_fix_index(pvi->type);

		/* get the index */
		if(pv_get_spec_index(msg, &pvp, &idx, &idxf))
		{
			LM_ERR("invalid index\n");
			return -1;
		}

		if (xbuff_is_attr_set(attr)) {
			LM_WARN("attribute is not expected here!\n");
		}

		if (!xreq) {
			LM_ERR("undefined variable '%.*s'\n",STR_FMT(&pvn->u.isname.name.s));
			return -1;
		}

		xreq = xreq->val.v.xavp;

		if ((idxf != PV_IDX_ALL) && !xbuff_is_no_index(attr) ) {
			xreq = xavp_get_nth(&xreq->val.v.xavp,idx,NULL);
		}

		if (!xreq) {
			LM_ERR("undefined value in '%.*s' at index %d\n",STR_FMT(&pvn->u.isname.name.s),idx);
			return -1;
		}

		if (xreq->val.type != SR_XTYPE_XAVP || xreq->name.s[0] != 'l') {
			LM_ERR("given value in parameter args is not list\n");
			return -1;
		}
		break;
	default:
		LM_ERR("unexpected type for arguments parameter\n");
		return -1;
	}

	switch(r->type){
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&vname,msg,&r->value.fp)){
			LM_ERR("can't get name of arguments parameter\n");
			return -1;
		}
		xrepl = pv_xbuff_get_xbuff(&vname);
		break;
	case ERL_PARAM_XBUFF_SPEC:
		sp = r->value.sp;
		pvp = sp.pvp; /* work on copy */

		if (pvp.pvn.type != PV_NAME_INTSTR || !(pvp.pvn.u.isname.type & AVP_NAME_STR)) {
			LM_ERR("unsupported name of xbuff\n");
			return -1;
		}

		if( pvp.pvn.type == PV_NAME_PVAR) {
			nsp = pvp.pvn.u.dname;
		}

		if (nsp) {
			pvi = &nsp->pvp.pvi;
			pvn = &nsp->pvp.pvn;
		} else {
			pvi = &pvp.pvi;
			pvn = &pvp.pvn;
		}

		if (sp.setf == pv_xbuff_set ) {
			xrepl = pv_xbuff_get_xbuff(&pvn->u.isname.name.s);
		} else {
			LM_ERR("unsupported variable type, xbuff only\n");
			return -1;
		}

		/* fix index */
		attr = xbuff_get_attr_flags(pvi->type);
		pvi->type = xbuff_fix_index(pvi->type);

		/* get the index */
		if(pv_get_spec_index(msg, &pvp, &idx, &idxf))
		{
			LM_ERR("invalid index\n");
			return -1;
		}

		if (xbuff_is_attr_set(attr)) {
			LM_WARN("attribute is not expected here!\n");
		}

		if ((idxf != PV_IDX_ALL) && !xbuff_is_no_index(attr) ) {
			LM_ERR("index is not expected here!\n");
			return -1;
		}

		break;
	default:
		LM_ERR("unexpected type for arguments parameter\n");
		return -1;
	}

	/* note: new without version byte */
	ei_x_new(&ei_req);

	/* ei_x_buff <- XAVP */
	if (erl_api.xavp2xbuff(&ei_req,xreq)) {
		LM_ERR("failed to encode\n");
		ei_x_free(&ei_req);
		return -1;
	}

	memset((void*)&ei_rep,0,sizeof(ei_x_buff));

	erl_api.rpc(&ei_rep,&module,&function,&ei_req);

	if (xrepl) {
		xavp_destroy_list(&xrepl->val.v.xavp);
	} else {
		xrepl = xbuff_new(&pvn->u.isname.name.s);
	}

	/* must be XAVP */
	xrepl->val.type = SR_XTYPE_XAVP;

	/* XAVP <- ei_x_buff */
	if (erl_api.xbuff2xavp(&xrepl->val.v.xavp,&ei_rep)){
		LM_ERR("failed to decode\n");
		ei_x_free(&ei_req);
		ei_x_free(&ei_rep);
		return -1;
	}

	ei_x_free(&ei_req);
	ei_x_free(&ei_rep);

	return 1;
}

static int fixup_rpc(void** param, int param_no)
{
	erl_param_t *erl_param;

	str s;

	erl_param=(erl_param_t*)pkg_malloc(sizeof(erl_param_t));
	if(!erl_param) {
		LM_ERR("no more memory\n");
		return -1;
	}
	memset(erl_param,0,sizeof(erl_param_t));

	if(param_no==1 || param_no==2) {
		if (fix_param_types(FPARAM_STR|FPARAM_STRING|FPARAM_AVP|FPARAM_PVS|FPARAM_PVE,param)){
			LM_ERR("wrong parameter #%d\n",param_no);
			return -1;
		}
		erl_param->type = ERL_PARAM_FPARAM;
		erl_param->value.fp = *(fparam_t*)*param;
	}

	if (param_no==3 || param_no==4) {

		s.s = (char*)*param; s.len = strlen(s.s);

		if (pv_parse_avp_name(&erl_param->value.sp,&s)) {
			LM_ERR("failed to parse parameter #%d\n",param_no);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		}

		if (erl_param->value.sp.pvp.pvn.type == PV_NAME_INTSTR) {
			if (fix_param_types(FPARAM_STR|FPARAM_STRING,param)) {
				LM_ERR("wrong parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}
			erl_param->type = ERL_PARAM_FPARAM;
			erl_param->value.fp = *(fparam_t*)*param;
		} else if(pv_parse_spec( &s, &erl_param->value.sp)==NULL || erl_param->value.sp.type!=PVT_OTHER) {

			/* only XBUFF is accepted for args and reply */
			LM_ERR("wrong parameter #%d: accepted types are list of xbuff\n",param_no);
			pv_spec_free(&erl_param->value.sp);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		} else {
			/* lets check what is acceptable */
			if (erl_param->value.sp.setf != pv_list_set && erl_param->value.sp.setf != pv_xbuff_set) {
				LM_ERR("wrong parameter #%d: accepted types are list of xbuff\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}
			erl_param->type = ERL_PARAM_XBUFF_SPEC;
		}
	}

	*param = (void*)erl_param;

	return 0;
}

static int erl_reg_send_k(struct sip_msg *msg, char *_server, char *_emsg)
{
	erl_param_t *param_server=(erl_param_t*)_server;
	erl_param_t *param_emsg=(erl_param_t*)_emsg;

	str server;
	str str_msg;
	sr_xavp_t *xmsg=NULL;
	pv_spec_t sp;
	pv_spec_t *nsp = NULL;
	pv_param_t  pvp;
	pv_name_t *pvn;
	pv_index_t *pvi;
	int idx;
	int idxf;
	int attr;
	ei_x_buff ei_msg;

	switch (param_server->type) {
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&server,msg,&param_server->value.fp)) {
			LM_ERR("can't get server process name\n");
		}
		break;
	default:
		LM_ERR("unexpected type for server name parameter\n");
		return -1;
	}

	ei_x_new_with_version(&ei_msg);

	switch(param_emsg->type){
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&str_msg,msg,&param_emsg->value.fp)){
			LM_ERR("can't get emsg parameter\n");
			goto err;
		}

		ei_x_encode_string_len(&ei_msg,str_msg.s,str_msg.len);

		break;
	case ERL_PARAM_XBUFF_SPEC:
		sp = param_emsg->value.sp;
		pvp = sp.pvp; /* work on copy */

		if (pvp.pvn.type != PV_NAME_INTSTR || !(pvp.pvn.u.isname.type & AVP_NAME_STR)) {
			LM_ERR("unsupported name of list\n");
			return -1;
		}

		if( pvp.pvn.type == PV_NAME_PVAR) {
			nsp = pvp.pvn.u.dname;
		}

		if (nsp) {
			pvi = &nsp->pvp.pvi;
			pvn = &nsp->pvp.pvn;
		} else {
			pvi = &pvp.pvi;
			pvn = &pvp.pvn;
		}

		if (sp.setf == pv_list_set ) {
			xmsg = pv_list_get_list(&pvn->u.isname.name.s);
		} else if (sp.setf == pv_xbuff_set) {
			xmsg = pv_xbuff_get_xbuff(&pvn->u.isname.name.s);
		}  else if (sp.setf == pv_tuple_set) {
			xmsg = pv_tuple_get_tuple(&pvn->u.isname.name.s);
		}

		/* fix index */
		attr = xbuff_get_attr_flags(pvi->type);
		pvi->type = xbuff_fix_index(pvi->type);

		/* get the index */
		if(pv_get_spec_index(msg, &pvp, &idx, &idxf))
		{
			LM_ERR("invalid index\n");
			return -1;
		}

		if (xbuff_is_attr_set(attr)) {
			LM_WARN("attribute is not expected here!\n");
		}

		if (!xmsg) {
			LM_ERR("undefined variable '%.*s'\n",STR_FMT(&pvn->u.isname.name.s));
			return -1;
		}

		xmsg = xmsg->val.v.xavp;

		if ((idxf != PV_IDX_ALL) && !xbuff_is_no_index(attr) ) {
			xmsg = xavp_get_nth(&xmsg->val.v.xavp,idx,NULL);
		}

		if (!xmsg) {
			LM_ERR("undefined value in '%.*s' at index %d\n",STR_FMT(&pvn->u.isname.name.s),idx);
			goto err;
		}

		/* ei_x_buff <- XAVP */
		if (erl_api.xavp2xbuff(&ei_msg,xmsg)) {
			LM_ERR("failed to encode %.*s\n",STR_FMT(&pvn->u.isname.name.s));
			goto err;
		}

		break;
	default:
		LM_ERR("unexpected type for emsg parameter\n");
		return -1;
	}

	if (erl_api.reg_send(&server,&ei_msg)) {
		goto err;
	}

	ei_x_free(&ei_msg);

	return 1;

err:
	ei_x_free(&ei_msg);

	return -1;
}

static int fixup_reg(void** param, int param_no)
{
	erl_param_t *erl_param;

	str s;

	erl_param=(erl_param_t*)pkg_malloc(sizeof(erl_param_t));

	if(!erl_param) {
		LM_ERR("no more memory\n");
		return -1;
	}

	memset(erl_param,0,sizeof(erl_param_t));

	if(param_no==1) {
		if (fix_param_types(FPARAM_STR|FPARAM_STRING|FPARAM_AVP|FPARAM_PVS|FPARAM_PVE|FPARAM_INT,param)){
			LM_ERR("wrong parameter #%d\n",param_no);
			return -1;
		}
		erl_param->type = ERL_PARAM_FPARAM;
		erl_param->value.fp = *(fparam_t*)*param;
	}

	if (param_no==2) {

		s.s = (char*)*param; s.len = strlen(s.s);

		if (pv_parse_avp_name(&erl_param->value.sp,&s)) {
			LM_ERR("failed to parse parameter #%d\n",param_no);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		}

		if (erl_param->value.sp.pvp.pvn.type == PV_NAME_INTSTR) {
			if (fix_param_types(FPARAM_STR|FPARAM_STRING,param)) {
				LM_ERR("wrong parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}
			erl_param->type = ERL_PARAM_FPARAM;
			erl_param->value.fp = *(fparam_t*)*param;
		} else if(pv_parse_spec( &s, &erl_param->value.sp)==NULL) {

			/* only XBUFF is accepted for emsg and reply */
			LM_ERR("wrong parameter #%d\n",param_no);
			pv_spec_free(&erl_param->value.sp);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		} else {
			if (erl_param->value.sp.type ==PVT_XAVP) {
				LM_ERR("XAVP not acceptable for parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}

			if (erl_param->value.sp.setf == pv_list_set
					|| erl_param->value.sp.setf == pv_xbuff_set
					|| erl_param->value.sp.setf == pv_tuple_set
					|| erl_param->value.sp.setf == pv_atom_set) {

				erl_param->type = ERL_PARAM_XBUFF_SPEC;
			} else {
				erl_param->type = ERL_PARAM_FPARAM;
				erl_param->value.fp = *(fparam_t*)*param;
			}
		}
	}

	*param = (void*)erl_param;

	return 0;
}

static int erl_reply_k(struct sip_msg *msg, char *_emsg)
{
	erl_param_t *param_emsg=(erl_param_t*)_emsg;

	str str_msg;
	sr_xavp_t *xmsg=NULL;
	pv_spec_t sp;
	pv_spec_t *nsp = NULL;
	pv_param_t  pvp;
	pv_name_t *pvn;
	pv_index_t *pvi;
	int idx;
	int idxf;
	int attr;
	ei_x_buff ei_msg;

	ei_x_new_with_version(&ei_msg);

	switch(param_emsg->type){
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&str_msg,msg,&param_emsg->value.fp)){
			LM_ERR("can't get emsg parameter\n");
			goto err;
		}

		ei_x_encode_string_len(&ei_msg,str_msg.s,str_msg.len);

		break;
	case ERL_PARAM_XBUFF_SPEC:
		sp = param_emsg->value.sp;
		pvp = sp.pvp; /* work on copy */

		if (pvp.pvn.type != PV_NAME_INTSTR || !(pvp.pvn.u.isname.type & AVP_NAME_STR)) {
			LM_ERR("unsupported name of list\n");
			return -1;
		}

		if( pvp.pvn.type == PV_NAME_PVAR) {
			nsp = pvp.pvn.u.dname;
		}

		if (nsp) {
			pvi = &nsp->pvp.pvi;
			pvn = &nsp->pvp.pvn;
		} else {
			pvi = &pvp.pvi;
			pvn = &pvp.pvn;
		}

		if (sp.setf == pv_list_set ) {
			xmsg = pv_list_get_list(&pvn->u.isname.name.s);
		} else if (sp.setf == pv_xbuff_set) {
			xmsg = pv_xbuff_get_xbuff(&pvn->u.isname.name.s);
		}  else if (sp.setf == pv_tuple_set) {
			xmsg = pv_tuple_get_tuple(&pvn->u.isname.name.s);
		}

		/* fix index */
		attr = xbuff_get_attr_flags(pvi->type);
		pvi->type = xbuff_fix_index(pvi->type);

		/* get the index */
		if(pv_get_spec_index(msg, &pvp, &idx, &idxf))
		{
			LM_ERR("invalid index\n");
			return -1;
		}

		if (xbuff_is_attr_set(attr)) {
			LM_WARN("attribute is not expected here!\n");
		}

		if (!xmsg) {
			LM_ERR("undefined variable '%.*s'\n",STR_FMT(&pvn->u.isname.name.s));
			return -1;
		}

		xmsg = xmsg->val.v.xavp;

		if ((idxf != PV_IDX_ALL) && !xbuff_is_no_index(attr) ) {
			xmsg = xavp_get_nth(&xmsg->val.v.xavp,idx,NULL);
		}

		if (!xmsg) {
			LM_ERR("undefined value in '%.*s' at index %d\n",STR_FMT(&pvn->u.isname.name.s),idx);
			goto err;
		}

		/* ei_x_buff <- XAVP */
		if (erl_api.xavp2xbuff(&ei_msg,xmsg)) {
			LM_ERR("failed to encode %.*s\n",STR_FMT(&pvn->u.isname.name.s));
			goto err;
		}

		break;
	default:
		LM_ERR("unexpected type for emsg parameter\n");
		return -1;
	}

	if (erl_api.reply(&ei_msg)) {
		goto err;
	}

	ei_x_free(&ei_msg);

	return 1;

err:
	ei_x_free(&ei_msg);

	return -1;
}

static int fixup_reply(void** param, int param_no)
{
	erl_param_t *erl_param;

	str s;

	erl_param=(erl_param_t*)pkg_malloc(sizeof(erl_param_t));

	if(!erl_param) {
		LM_ERR("no more memory\n");
		return -1;
	}

	memset(erl_param,0,sizeof(erl_param_t));

	if (param_no==1) {

		s.s = (char*)*param; s.len = strlen(s.s);

		if (pv_parse_avp_name(&erl_param->value.sp,&s)) {
			LM_ERR("failed to parse parameter #%d\n",param_no);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		}

		if (erl_param->value.sp.pvp.pvn.type == PV_NAME_INTSTR) {
			if (fix_param_types(FPARAM_STR|FPARAM_STRING,param)) {
				LM_ERR("wrong parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}
			erl_param->type = ERL_PARAM_FPARAM;
			erl_param->value.fp = *(fparam_t*)*param;
		} else if(pv_parse_spec( &s, &erl_param->value.sp)==NULL) {

			/* only XBUFF is accepted for emsg and reply */
			LM_ERR("wrong parameter #%d\n",param_no);
			pv_spec_free(&erl_param->value.sp);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		} else {
			if (erl_param->value.sp.type ==PVT_XAVP) {
				LM_ERR("XAVP not acceptable for parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}

			if (erl_param->value.sp.setf == pv_list_set
					|| erl_param->value.sp.setf == pv_xbuff_set
					|| erl_param->value.sp.setf == pv_tuple_set
					|| erl_param->value.sp.setf == pv_atom_set) {

				erl_param->type = ERL_PARAM_XBUFF_SPEC;
			} else {
				erl_param->type = ERL_PARAM_FPARAM;
				erl_param->value.fp = *(fparam_t*)*param;
			}
		}
	}

	*param = (void*)erl_param;

	return 0;
}

static int erl_send_k(struct sip_msg *msg, char *_pid, char *_emsg)
{
	erl_param_t *param_pid=(erl_param_t*)_pid;
	erl_param_t *param_emsg=(erl_param_t*)_emsg;

	str str_msg;
	sr_xavp_t *xmsg=NULL;
	pv_spec_t sp;
	pv_spec_t *nsp = NULL;
	pv_param_t  pvp;
	pv_name_t *pvn;
	pv_index_t *pvi;
	int idx;
	int idxf;
	int attr;
	ei_x_buff ei_msg;
	erlang_pid *pid;

	switch (param_pid->type) {
	case ERL_PARAM_XBUFF_SPEC:
		sp = param_pid->value.sp;
		pvp = sp.pvp; /* work on copy */

		if (pvp.pvn.type != PV_NAME_INTSTR || !(pvp.pvn.u.isname.type & AVP_NAME_STR)) {
			LM_ERR("unsupported name of pid\n");
			return -1;
		}

		if( pvp.pvn.type == PV_NAME_PVAR) {
			nsp = pvp.pvn.u.dname;
		}

		if (nsp) {
			pvi = &nsp->pvp.pvi;
			pvn = &nsp->pvp.pvn;
		} else {
			pvi = &pvp.pvi;
			pvn = &pvp.pvn;
		}

		if (sp.getf == pv_pid_get ) {
			xmsg = pv_pid_get_pid(&pvn->u.isname.name.s);
		} else if (sp.getf == pv_xbuff_get) {
			xmsg = pv_xbuff_get_xbuff(&pvn->u.isname.name.s);
		} else {
			LM_ERR("BUG: unexpected type for pid parameter\n");
			return -1;
		}

		/* fix index */
		attr = xbuff_get_attr_flags(pvi->type);
		pvi->type = xbuff_fix_index(pvi->type);

		/* get the index */
		if(pv_get_spec_index(msg, &pvp, &idx, &idxf))
		{
			LM_ERR("invalid index\n");
			return -1;
		}

		if (xbuff_is_attr_set(attr)) {
			LM_WARN("attribute is not expected here!\n");
		}

		if (!xmsg) {
			LM_ERR("undefined variable '%.*s'\n",STR_FMT(&pvn->u.isname.name.s));
			return -1;
		}

		xmsg = xmsg->val.v.xavp;

		if ((idxf != PV_IDX_ALL) && !xbuff_is_no_index(attr) ) {
			xmsg = xavp_get_nth(&xmsg->val.v.xavp,idx,NULL);
		}

		if (!xmsg) {
			LM_ERR("undefined value in '%.*s' at index %d\n",STR_FMT(&pvn->u.isname.name.s),idx);
			goto err;
		}

		/* erlang_pid <- XAVP */
		if (xmsg->name.s[0] == 'p' && xmsg->val.type == SR_XTYPE_DATA && xmsg->val.v.data) {
			pid = xmsg->val.v.data->p;
		} else {
			LM_ERR("invalid value for pid parameter\n");
			return -1;
		}
		break;
	default:
		LM_ERR("unexpected type for pid parameter\n");
		return -1;
	}

	ei_x_new_with_version(&ei_msg);

	switch(param_emsg->type){
	case ERL_PARAM_FPARAM:
		if(get_str_fparam(&str_msg,msg,&param_emsg->value.fp)){
			LM_ERR("can't get emsg parameter\n");
			goto err;
		}

		ei_x_encode_string_len(&ei_msg,str_msg.s,str_msg.len);

		break;
	case ERL_PARAM_XBUFF_SPEC:
		sp = param_emsg->value.sp;
		pvp = sp.pvp; /* work on copy */

		if (pvp.pvn.type != PV_NAME_INTSTR || !(pvp.pvn.u.isname.type & AVP_NAME_STR)) {
			LM_ERR("unsupported name of list\n");
			return -1;
		}

		if( pvp.pvn.type == PV_NAME_PVAR) {
			nsp = pvp.pvn.u.dname;
		}

		if (nsp) {
			pvi = &nsp->pvp.pvi;
			pvn = &nsp->pvp.pvn;
		} else {
			pvi = &pvp.pvi;
			pvn = &pvp.pvn;
		}

		if (sp.getf == pv_list_get ) {
			xmsg = pv_list_get_list(&pvn->u.isname.name.s);
		} else if (sp.getf == pv_xbuff_get) {
			xmsg = pv_xbuff_get_xbuff(&pvn->u.isname.name.s);
		} else if (sp.getf == pv_tuple_get) {
			xmsg = pv_tuple_get_tuple(&pvn->u.isname.name.s);
		} else if (sp.getf == pv_atom_get) {
			xmsg = pv_atom_get_atom(&pvn->u.isname.name.s);
		} else if (sp.getf == pv_pid_get) {
			xmsg = pv_pid_get_pid(&pvn->u.isname.name.s);
		}

		/* fix index */
		attr = xbuff_get_attr_flags(pvi->type);
		pvi->type = xbuff_fix_index(pvi->type);

		/* get the index */
		if(pv_get_spec_index(msg, &pvp, &idx, &idxf))
		{
			LM_ERR("invalid index\n");
			return -1;
		}

		if (xbuff_is_attr_set(attr)) {
			LM_WARN("attribute is not expected here!\n");
		}

		if (!xmsg) {
			LM_ERR("undefined variable '%.*s'\n",STR_FMT(&pvn->u.isname.name.s));
			return -1;
		}

		xmsg = xmsg->val.v.xavp;

		if ((idxf != PV_IDX_ALL) && !xbuff_is_no_index(attr) ) {
			xmsg = xavp_get_nth(&xmsg->val.v.xavp,idx,NULL);
		}

		if (!xmsg) {
			LM_ERR("undefined value in '%.*s' at index %d\n",STR_FMT(&pvn->u.isname.name.s),idx);
			goto err;
		}

		/* ei_x_buff <- XAVP */
		if (erl_api.xavp2xbuff(&ei_msg,xmsg)) {
			LM_ERR("failed to encode %.*s\n",STR_FMT(&pvn->u.isname.name.s));
			goto err;
		}

		break;
	default:
		LM_ERR("unexpected type for emsg parameter\n");
		return -1;
	}

	if (erl_api.send(pid,&ei_msg)) {
		goto err;
	}

	ei_x_free(&ei_msg);

	return 1;

err:
	ei_x_free(&ei_msg);

	return -1;
}

static int fixup_send(void** param, int param_no)
{
	erl_param_t *erl_param;

	str s;

	erl_param=(erl_param_t*)pkg_malloc(sizeof(erl_param_t));

	if(!erl_param) {
		LM_ERR("no more memory\n");
		return -1;
	}

	memset(erl_param,0,sizeof(erl_param_t));

	if (param_no==1) {

		s.s = (char*)*param; s.len = strlen(s.s);

		if (pv_parse_avp_name(&erl_param->value.sp,&s)) {
			LM_ERR("failed to parse parameter #%d\n",param_no);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		}

		if (erl_param->value.sp.pvp.pvn.type == PV_NAME_INTSTR) {
			if (fix_param_types(FPARAM_STR|FPARAM_STRING,param)) {
				LM_ERR("wrong parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}
			erl_param->type = ERL_PARAM_FPARAM;
			erl_param->value.fp = *(fparam_t*)*param;
		} else if(pv_parse_spec( &s, &erl_param->value.sp)==NULL) {

			/* only XBUFF is accepted for emsg */
			LM_ERR("wrong parameter #%d\n",param_no);
			pv_spec_free(&erl_param->value.sp);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		} else {
			if (erl_param->value.sp.type ==PVT_XAVP) {
				LM_ERR("XAVP not acceptable for parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}

			if (erl_param->value.sp.getf == pv_pid_get
					|| erl_param->value.sp.getf == pv_xbuff_get) {
				erl_param->type = ERL_PARAM_XBUFF_SPEC;
			} else {
				erl_param->type = ERL_PARAM_FPARAM;
				erl_param->value.fp = *(fparam_t*)*param;
			}
		}
	}

	if (param_no==2) {

		s.s = (char*)*param; s.len = strlen(s.s);

		if (pv_parse_avp_name(&erl_param->value.sp,&s)) {
			LM_ERR("failed to parse parameter #%d\n",param_no);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		}

		if (erl_param->value.sp.pvp.pvn.type == PV_NAME_INTSTR) {
			if (fix_param_types(FPARAM_STR|FPARAM_STRING,param)) {
				LM_ERR("wrong parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}
			erl_param->type = ERL_PARAM_FPARAM;
			erl_param->value.fp = *(fparam_t*)*param;
		} else if(pv_parse_spec( &s, &erl_param->value.sp)==NULL) {

			/* only XBUFF is accepted for emsg */
			LM_ERR("wrong parameter #%d\n",param_no);
			pv_spec_free(&erl_param->value.sp);
			pkg_free((void*)erl_param);
			return E_UNSPEC;
		} else {
			if (erl_param->value.sp.type ==PVT_XAVP) {
				LM_ERR("XAVP not acceptable for parameter #%d\n",param_no);
				pkg_free((void*)erl_param);
				return E_UNSPEC;
			}

			if (erl_param->value.sp.getf == pv_list_get
					|| erl_param->value.sp.getf == pv_xbuff_get
					|| erl_param->value.sp.getf == pv_tuple_get
					|| erl_param->value.sp.getf == pv_atom_get
					|| erl_param->value.sp.getf == pv_pid_get) {

				erl_param->type = ERL_PARAM_XBUFF_SPEC;
			} else {
				erl_param->type = ERL_PARAM_FPARAM;
				erl_param->value.fp = *(fparam_t*)*param;
			}
		}
	}

	*param = (void*)erl_param;

	return 0;
}
