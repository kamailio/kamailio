/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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

#include <Python.h>
#include <frameobject.h>

#include "../../core/dprint.h"
#include "../../core/route.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../core/locking.h"
#include "../../core/pvar.h"
#include "../../core/timer.h"
#include "../../core/mem/pkg.h"
#include "../../core/mem/shm.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "msgobj_struct.h"
#include "python_exec.h"
#include "apy_kemi_export.h"
#include "apy_kemi.h"

int *_sr_python_reload_version = NULL;
int _sr_python_local_version = 0;
gen_lock_t* _sr_python_reload_lock;
extern str _sr_python_load_file;
extern int _apy_process_rank;

int apy_reload_script(void);
/**
 *
 */
int sr_kemi_config_engine_python(sip_msg_t *msg, int rtype, str *rname,
		str *rparam)
{
	int ret;

	ret = -1;
	if(rtype==REQUEST_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, 0);
		} else {
			ret = apy_exec(msg, "ksr_request_route", NULL, 1);
		}
	} else if(rtype==CORE_ONREPLY_ROUTE) {
		if(kemi_reply_route_callback.len>0) {
			ret = apy_exec(msg, kemi_reply_route_callback.s, NULL, 0);
		}
	} else if(rtype==BRANCH_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==BRANCH_FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==TM_ONREPLY_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s, NULL, 0);
		}
	} else if(rtype==ONSEND_ROUTE) {
		if(kemi_onsend_route_callback.len>0) {
			ret = apy_exec(msg, kemi_onsend_route_callback.s, NULL, 0);
		}
		return 1;
	} else if(rtype==EVENT_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = apy_exec(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, 0);
		}
	} else {
		if(rname!=NULL) {
			LM_ERR("route type %d with name [%.*s] not implemented\n",
				rtype, rname->len, rname->s);
		} else {
			LM_ERR("route type %d with no name not implemented\n",
				rtype);
		}
	}

	if(rname!=NULL) {
		LM_DBG("execution of route type %d with name [%.*s] returned %d\n",
				rtype, rname->len, rname->s, ret);
	} else {
		LM_DBG("execution of route type %d with no name returned %d\n",
			rtype, ret);
	}

	return 1;
}

/**
 *
 */
PyObject *sr_kemi_apy_return_true(void)
{
	Py_INCREF(Py_True);
	return Py_True;
}

/**
 *
 */
PyObject *sr_kemi_apy_return_false(void)
{
	Py_INCREF(Py_False);
	return Py_False;
}

/**
 *
 */
PyObject *sr_apy_kemi_return_none(void)
{
	Py_INCREF(Py_None);
	return Py_None;
}

/**
 *
 */
PyObject *sr_kemi_apy_return_int(sr_kemi_t *ket, int rval)
{
	if(ket!=NULL && ket->rtype==SR_KEMIP_BOOL) {
		if(rval==SR_KEMI_TRUE) {
			return sr_kemi_apy_return_true();
		} else {
			return sr_kemi_apy_return_false();
		}
	}
	return PyLong_FromLong((long)rval);
}

/**
 *
 */
PyObject *sr_apy_kemi_return_str(sr_kemi_t *ket, char *sval, int slen)
{
	return PyUnicode_FromStringAndSize(sval, slen);
}

/**
 *
 */
PyObject *sr_kemi_apy_return_xval(sr_kemi_t *ket, sr_kemi_xval_t *rx)
{
	switch(rx->vtype) {
		case SR_KEMIP_NONE:
			return sr_apy_kemi_return_none();
		case SR_KEMIP_INT:
			return sr_kemi_apy_return_int(ket, rx->v.n);
		case SR_KEMIP_STR:
			return sr_apy_kemi_return_str(ket, rx->v.s.s, rx->v.s.len);
		case SR_KEMIP_BOOL:
			if(rx->v.n!=SR_KEMI_FALSE) {
				return sr_kemi_apy_return_true();
			} else {
				return sr_kemi_apy_return_false();
			}
		case SR_KEMIP_ARRAY:
			LM_ERR("unsupported return type: array\n");
			sr_kemi_xval_free(rx);
			return sr_apy_kemi_return_none();
		case SR_KEMIP_DICT:
			LM_ERR("unsupported return type: map\n");
			sr_kemi_xval_free(rx);
			return sr_apy_kemi_return_none();
		case SR_KEMIP_XVAL:
			/* unknown content - return false */
			return sr_kemi_apy_return_false();
		case SR_KEMIP_NULL:
			return sr_apy_kemi_return_none();
		default:
			/* unknown type - return false */
			return sr_kemi_apy_return_false();
	}
}

/**
 *
 */
PyObject *sr_apy_kemi_exec_func_ex(sr_kemi_t *ket, PyObject *self, PyObject *args, int idx)
{
	str fname;
	int i;
	int ret;
	sr_kemi_val_t vps[SR_KEMI_PARAMS_MAX];
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;
	sr_kemi_xval_t *xret;

	env_P = sr_apy_env_get();

	if(env_P==NULL) {
		LM_ERR("invalid Python environment attributes\n");
		return sr_kemi_apy_return_false();
	}
	if(env_P->msg==NULL) {
		lmsg = faked_msg_next();
	} else {
		lmsg = env_P->msg;
	}

	if(ket->mname.len>0) {
		LM_DBG("execution of method: %.*s\n", ket->fname.len, ket->fname.s);
	} else {
		LM_DBG("execution of method: %.*s.%.*s\n",
				ket->mname.len, ket->mname.s,
				ket->fname.len, ket->fname.s);
	}
	fname = ket->fname;

	if(ket->ptypes[0]==SR_KEMIP_NONE) {
		if(ket->rtype==SR_KEMIP_XVAL) {
			xret = ((sr_kemi_xfm_f)(ket->func))(lmsg);
			return sr_kemi_apy_return_xval(ket, xret);
		} else {
			ret = ((sr_kemi_fm_f)(ket->func))(lmsg);
			return sr_kemi_apy_return_int(ket, ret);
		}
	}

	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_val_t));
	if(ket->ptypes[1]==SR_KEMIP_NONE) {
		i = 1;
		if(ket->ptypes[0]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "i:kemi-param-n", &vps[0].n)) {
				LM_ERR("unable to retrieve int param %d\n", 0);
				return sr_kemi_apy_return_false();
			}
			LM_DBG("param[%d] for: %.*s is int: %d\n", i,
				fname.len, fname.s, vps[0].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "s:kemi-param-s", &vps[0].s.s)) {
				LM_ERR("unable to retrieve str param %d\n", 0);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			LM_DBG("param[%d] for: %.*s is str: %.*s\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s);
		} else {
			LM_ERR("not implemented yet\n");
			return sr_kemi_apy_return_false();
		}
	} else if(ket->ptypes[2]==SR_KEMIP_NONE) {
		i = 2;
		if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ii:kemi-param-nn", &vps[0].n, &vps[1].n)) {
				LM_ERR("unable to retrieve int-int params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			LM_DBG("params[%d] for: %.*s are int-int: [%d] [%d]\n", i,
				fname.len, fname.s, vps[0].n, vps[1].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "is:kemi-param-ns", &vps[0].n, &vps[1].s.s)) {
				LM_ERR("unable to retrieve int-str params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are int-str: [%d] [%.*s]\n", i,
				fname.len, fname.s, vps[0].n, vps[1].s.len, vps[1].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR && ket->ptypes[1]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "si:kemi-param-sn", &vps[0].s.s, &vps[1].n)) {
				LM_ERR("unable to retrieve str-int params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			LM_DBG("params[%d] for: %.*s are str-int: [%.*s] [%d]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s, vps[1].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR && ket->ptypes[1]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "ss:kemi-param-ss", &vps[0].s.s, &vps[1].s.s)) {
				LM_ERR("unable to retrieve str-str param %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are str: [%.*s] [%.*s]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s,
				vps[1].s.len, vps[1].s.s);
		} else {
			LM_ERR("not implemented yet\n");
			return sr_kemi_apy_return_false();
		}

	} else if(ket->ptypes[3]==SR_KEMIP_NONE) {
		i = 3;
		if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "iii:kemi-param-nnn", &vps[0].n,
						&vps[1].n, &vps[2].n)) {
				LM_ERR("unable to retrieve int-int-int params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			LM_DBG("params[%d] for: %.*s are int-int-int: [%d] [%d] [%d]\n",
					i, fname.len, fname.s, vps[0].n, vps[1].n, vps[2].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "iis:kemi-param-nns", &vps[0].n,
						&vps[1].n, &vps[2].s.s)) {
				LM_ERR("unable to retrieve int-int-str params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[2].s.len = strlen(vps[2].s.s);
			LM_DBG("params[%d] for: %.*s are int-int-str: [%d] [%d] [%.*s]\n", i,
					fname.len, fname.s, vps[0].n, vps[1].n, vps[2].s.len, vps[2].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "isi:kemi-param-nsn", &vps[0].n,
						&vps[1].s.s, &vps[2].n)) {
				LM_ERR("unable to retrieve int-str-int params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are int-str-int: [%d] [%.*s] [%d]\n", i,
				fname.len, fname.s, vps[0].n, vps[1].s.len, vps[1].s.s, vps[2].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "iss:kemi-param-nss", &vps[0].n,
						&vps[1].s.s, &vps[2].s.s)) {
				LM_ERR("unable to retrieve int-str-str param %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			LM_DBG("params[%d] for: %.*s are int-str-str: [%d] [%.*s]"
					" [%.*s]\n", i, fname.len, fname.s,
					vps[0].n, vps[1].s.len, vps[1].s.s, vps[2].s.len, vps[2].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR && ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "sii:kemi-param-snn", &vps[0].s.s,
						&vps[1].n, &vps[2].n)) {
				LM_ERR("unable to retrieve str-int-int params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			LM_DBG("params[%d] for: %.*s are str-int: [%.*s] [%d] [%d]\n", i,
					fname.len, fname.s, vps[0].s.len, vps[0].s.s, vps[1].n,
					vps[2].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR && ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "sis:kemi-param-ssn", &vps[0].s.s,
						&vps[1].n, &vps[2].s.s)) {
				LM_ERR("unable to retrieve str-int-str param %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			LM_DBG("params[%d] for: %.*s are str-str-int: [%.*s] [%d] [%.*s]\n",
					i, fname.len, fname.s,
					vps[0].s.len, vps[0].s.s,
					vps[1].n, vps[2].s.len, vps[2].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR && ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ssi:kemi-param-ssn", &vps[0].s.s,
						&vps[1].s.s, &vps[2].n)) {
				LM_ERR("unable to retrieve str-str-int param %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are str-str-int: [%.*s] [%.*s]"
					" [%d]\n", i, fname.len, fname.s,
					vps[0].s.len, vps[0].s.s,
					vps[1].s.len, vps[1].s.s, vps[2].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR && ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "sss:kemi-param-sss", &vps[0].s.s,
						&vps[1].s.s, &vps[2].s.s)) {
				LM_ERR("unable to retrieve str-str-str param %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			LM_DBG("params[%d] for: %.*s are str-str-str: [%.*s] [%.*s]"
					" [%.*s]\n", i, fname.len, fname.s,
					vps[0].s.len, vps[0].s.s,
					vps[1].s.len, vps[1].s.s, vps[2].s.len, vps[2].s.s);
		} else {
			LM_ERR("not implemented yet\n");
			return sr_kemi_apy_return_false();
		}
	} else if(ket->ptypes[4]==SR_KEMIP_NONE) {
		i = 4;
		if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "ssss:kemi-param-ssss",
					&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s)) {
				LM_ERR("unable to retrieve ssss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].s.s, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "sssn:kemi-param-sssn",
					&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].n)) {
				LM_ERR("unable to retrieve sssn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].s.s, vps[3].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "ssns:kemi-param-ssns",
					&vps[0].s.s, &vps[1].s.s, &vps[2].n, &vps[3].s.s)) {
				LM_ERR("unable to retrieve ssns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].n, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ssnn:kemi-param-ssnn",
					&vps[0].s.s, &vps[1].s.s, &vps[2].n, &vps[3].n)) {
				LM_ERR("unable to retrieve ssnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].n, vps[3].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "snss:kemi-param-snss",
					&vps[0].s.s, &vps[1].n, &vps[2].s.s, &vps[3].s.s)) {
				LM_ERR("unable to retrieve snss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].s.s, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "snsn:kemi-param-snsn",
					&vps[0].s.s, &vps[1].n, &vps[2].s.s, &vps[3].n)) {
				LM_ERR("unable to retrieve snsn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].s.s, vps[3].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "snns:kemi-param-snns",
					&vps[0].s.s, &vps[1].n, &vps[2].n, &vps[3].s.s)) {
				LM_ERR("unable to retrieve snns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].n, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "snnn:kemi-param-snnn",
					&vps[0].s.s, &vps[1].n, &vps[2].n, &vps[3].n)) {
				LM_ERR("unable to retrieve snnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].n, vps[3].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nsss:kemi-param-nsss",
					&vps[0].n, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s)) {
				LM_ERR("unable to retrieve nsss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].s.s, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nssn:kemi-param-nssn",
					&vps[0].n, &vps[1].s.s, &vps[2].s.s, &vps[3].n)) {
				LM_ERR("unable to retrieve nssn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].s.s, vps[3].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nsns:kemi-param-nsns",
					&vps[0].n, &vps[1].s.s, &vps[2].n, &vps[3].s.s)) {
				LM_ERR("unable to retrieve nsns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].n, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nsnn:kemi-param-nsnn",
					&vps[0].n, &vps[1].s.s, &vps[2].n, &vps[3].n)) {
				LM_ERR("unable to retrieve nsnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].n, vps[3].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nnss:kemi-param-nnss",
					&vps[0].n, &vps[1].n, &vps[2].s.s, &vps[3].s.s)) {
				LM_ERR("unable to retrieve nnss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].s.s, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nnsn:kemi-param-nnsn",
					&vps[0].n, &vps[1].n, &vps[2].s.s, &vps[3].n)) {
				LM_ERR("unable to retrieve nnsn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].s.s, vps[3].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nnns:kemi-param-nnns",
					&vps[0].n, &vps[1].n, &vps[2].n, &vps[3].s.s)) {
				LM_ERR("unable to retrieve nnns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].n, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nnnn:kemi-param-nnnn",
					&vps[0].n, &vps[1].n, &vps[2].n, &vps[3].n)) {
				LM_ERR("unable to retrieve nnnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].n, vps[3].n);
		} else {
			LM_ERR("invalid parameters for: %.*s\n", fname.len, fname.s);
			return sr_kemi_apy_return_false();
		}
	} else if(ket->ptypes[5]==SR_KEMIP_NONE) {
		i = 5;
		if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "sssss:kemi-param-sssss",
					&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve sssss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%s] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].s.s, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ssssn:kemi-param-ssssn",
					&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve ssssn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%s] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].s.s, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "sssns:kemi-param-sssns",
					&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve sssns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%s] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].s.s, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "sssnn:kemi-param-sssnn",
					&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve sssnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%s] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].s.s, vps[3].n, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "ssnss:kemi-param-ssnss",
					&vps[0].s.s, &vps[1].s.s, &vps[2].n, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve ssnss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%d] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].n, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ssnsn:kemi-param-ssnsn",
					&vps[0].s.s, &vps[1].s.s, &vps[2].n, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve ssnsn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%d] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].n, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "ssnns:kemi-param-ssnns",
					&vps[0].s.s, &vps[1].s.s, &vps[2].n, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve ssnns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%d] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].n, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ssnnn:kemi-param-ssnnn",
					&vps[0].s.s, &vps[1].s.s, &vps[2].n, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve ssnnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%s] [%d] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].s.s, vps[2].n, vps[3].n, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "snsss:kemi-param-snsss",
					&vps[0].s.s, &vps[1].n, &vps[2].s.s, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve snsss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%s] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].s.s, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "snssn:kemi-param-snssn",
					&vps[0].s.s, &vps[1].n, &vps[2].s.s, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve snssn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%s] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].s.s, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "snsns:kemi-param-snsns",
					&vps[0].s.s, &vps[1].n, &vps[2].s.s, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve snsns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%s] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].s.s, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "snsnn:kemi-param-snsnn",
					&vps[0].s.s, &vps[1].n, &vps[2].s.s, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve snsnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%s] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].s.s, vps[3].n, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "snnss:kemi-param-snnss",
					&vps[0].s.s, &vps[1].n, &vps[2].n, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve snnss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%d] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].n, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "snnsn:kemi-param-snnsn",
					&vps[0].s.s, &vps[1].n, &vps[2].n, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve snnsn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%d] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].n, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "snnns:kemi-param-snnns",
					&vps[0].s.s, &vps[1].n, &vps[2].n, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve snnns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%d] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].n, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "snnnn:kemi-param-snnnn",
					&vps[0].s.s, &vps[1].n, &vps[2].n, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve snnnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);

			LM_DBG("params[%d] for: %.*s are: [%s] [%d] [%d] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].s.s, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nssss:kemi-param-nssss",
					&vps[0].n, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nssss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%s] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].s.s, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nsssn:kemi-param-nsssn",
					&vps[0].n, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve nsssn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%s] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].s.s, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nssns:kemi-param-nssns",
					&vps[0].n, &vps[1].s.s, &vps[2].s.s, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nssns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%s] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].s.s, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nssnn:kemi-param-nssnn",
					&vps[0].n, &vps[1].s.s, &vps[2].s.s, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve nssnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%s] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].s.s, vps[3].n, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nsnss:kemi-param-nsnss",
					&vps[0].n, &vps[1].s.s, &vps[2].n, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nsnss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%d] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].n, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nsnsn:kemi-param-nsnsn",
					&vps[0].n, &vps[1].s.s, &vps[2].n, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve nsnsn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%d] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].n, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nsnns:kemi-param-nsnns",
					&vps[0].n, &vps[1].s.s, &vps[2].n, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nsnns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%d] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].n, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nsnnn:kemi-param-nsnnn",
					&vps[0].n, &vps[1].s.s, &vps[2].n, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve nsnnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%s] [%d] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].s.s, vps[2].n, vps[3].n, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nnsss:kemi-param-nnsss",
					&vps[0].n, &vps[1].n, &vps[2].s.s, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nnsss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%s] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].s.s, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nnssn:kemi-param-nnssn",
					&vps[0].n, &vps[1].n, &vps[2].s.s, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve nnssn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%s] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].s.s, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nnsns:kemi-param-nnsns",
					&vps[0].n, &vps[1].n, &vps[2].s.s, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nnsns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[2].s.len = strlen(vps[2].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%s] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].s.s, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nnsnn:kemi-param-nnsnn",
					&vps[0].n, &vps[1].n, &vps[2].s.s, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve nnsnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[2].s.len = strlen(vps[2].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%s] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].s.s, vps[3].n, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nnnss:kemi-param-nnnss",
					&vps[0].n, &vps[1].n, &vps[2].n, &vps[3].s.s, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nnnss params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%d] [%s] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].n, vps[3].s.s, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nnnsn:kemi-param-nnnsn",
					&vps[0].n, &vps[1].n, &vps[2].n, &vps[3].s.s, &vps[4].n)) {
				LM_ERR("unable to retrieve nnnsn params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[3].s.len = strlen(vps[3].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%d] [%s] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].n, vps[3].s.s, vps[4].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "nnnns:kemi-param-nnnns",
					&vps[0].n, &vps[1].n, &vps[2].n, &vps[3].n, &vps[4].s.s)) {
				LM_ERR("unable to retrieve nnnns params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[4].s.len = strlen(vps[4].s.s);

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%d] [%d] [%s]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].n, vps[3].n, vps[4].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_INT
				&& ket->ptypes[1]==SR_KEMIP_INT
				&& ket->ptypes[2]==SR_KEMIP_INT
				&& ket->ptypes[3]==SR_KEMIP_INT
				&& ket->ptypes[4]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "nnnnn:kemi-param-nnnnn",
					&vps[0].n, &vps[1].n, &vps[2].n, &vps[3].n, &vps[4].n)) {
				LM_ERR("unable to retrieve nnnnn params %d\n", i);
				return sr_kemi_apy_return_false();
			}

			LM_DBG("params[%d] for: %.*s are: [%d] [%d] [%d] [%d] [%d]\n",
					i, fname.len, fname.s,
					vps[0].n, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
		} else {
			LM_ERR("invalid parameters for: %.*s\n", fname.len, fname.s);
			return sr_kemi_apy_return_false();
		}
	} else {
		i = 6;
		if(ket->ptypes[0]==SR_KEMIP_STR
				&& ket->ptypes[1]==SR_KEMIP_STR
				&& ket->ptypes[2]==SR_KEMIP_STR
				&& ket->ptypes[3]==SR_KEMIP_STR
				&& ket->ptypes[4]==SR_KEMIP_STR
				&& ket->ptypes[5]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "ssssss:kemi-param-ssssss",
						&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s,
						&vps[4].s.s, &vps[5].s.s)) {
				LM_ERR("unable to retrieve str-str-str-str params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);
			vps[5].s.len = strlen(vps[5].s.s);
			LM_DBG("params[%d] for: %.*s are str: [%.*s] [%.*s]"
					" [%.*s] [%.*s] [%.*s] [%.*s]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s,
				vps[1].s.len, vps[1].s.s, vps[2].s.len, vps[2].s.s,
				vps[3].s.len, vps[3].s.s, vps[4].s.len, vps[4].s.s,
				vps[5].s.len, vps[5].s.s);
		} else {
			LM_ERR("not implemented yet\n");
			return sr_kemi_apy_return_false();
		}
	}

	switch(i) {
		case 1:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfmn_f)(ket->func))(lmsg, vps[0].n);
					return sr_kemi_apy_return_xval(ket, xret);
				} else {
					ret = ((sr_kemi_fmn_f)(ket->func))(lmsg, vps[0].n);
					return sr_kemi_apy_return_int(ket, ret);
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					xret = ((sr_kemi_xfms_f)(ket->func))(lmsg, &vps[0].s);
					return sr_kemi_apy_return_xval(ket, xret);
				} else {
					ret = ((sr_kemi_fms_f)(ket->func))(lmsg, &vps[0].s);
					return sr_kemi_apy_return_int(ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		case 2:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmnn_f)(ket->func))(lmsg, vps[0].n, vps[1].n);
						return sr_kemi_apy_return_xval(ket, xret);
					} else {
						ret = ((sr_kemi_fmnn_f)(ket->func))(lmsg, vps[0].n, vps[1].n);
						return sr_kemi_apy_return_int(ket, ret);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmns_f)(ket->func))(lmsg, vps[0].n, &vps[1].s);
						return sr_kemi_apy_return_xval(ket, xret);
					} else {
						ret = ((sr_kemi_fmns_f)(ket->func))(lmsg, vps[0].n, &vps[1].s);
						return sr_kemi_apy_return_int(ket, ret);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_kemi_apy_return_false();
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmsn_f)(ket->func))(lmsg, &vps[0].s, vps[1].n);
						return sr_kemi_apy_return_xval(ket, xret);
					} else {
						ret = ((sr_kemi_fmsn_f)(ket->func))(lmsg, &vps[0].s, vps[1].n);
						return sr_kemi_apy_return_int(ket, ret);
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->rtype==SR_KEMIP_XVAL) {
						xret = ((sr_kemi_xfmss_f)(ket->func))(lmsg, &vps[0].s, &vps[1].s);
						return sr_kemi_apy_return_xval(ket, xret);
					} else {
						ret = ((sr_kemi_fmss_f)(ket->func))(lmsg, &vps[0].s, &vps[1].s);
						return sr_kemi_apy_return_int(ket, ret);
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_kemi_apy_return_false();
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		case 3:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						/* nnn */
						ret = ((sr_kemi_fmnnn_f)(ket->func))(lmsg,
								vps[0].n, vps[1].n, vps[2].n);
						return sr_kemi_apy_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						/* nns */
						ret = ((sr_kemi_fmnns_f)(ket->func))(lmsg,
								vps[0].n, vps[1].n, &vps[2].s);
						return sr_kemi_apy_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_kemi_apy_return_false();
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						/* nsn */
						ret = ((sr_kemi_fmnsn_f)(ket->func))(lmsg,
								vps[0].n, &vps[1].s, vps[2].n);
						return sr_kemi_apy_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						/* nss */
						ret = ((sr_kemi_fmnss_f)(ket->func))(lmsg,
								vps[0].n, &vps[1].s, &vps[2].s);
						return sr_kemi_apy_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_kemi_apy_return_false();
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_kemi_apy_return_false();
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						/* snn */
						ret = ((sr_kemi_fmsnn_f)(ket->func))(lmsg,
								&vps[0].s, vps[1].n, vps[2].n);
						return sr_kemi_apy_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						/* sns */
						ret = ((sr_kemi_fmsns_f)(ket->func))(lmsg,
								&vps[0].s, vps[1].n, &vps[2].s);
						return sr_kemi_apy_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_kemi_apy_return_false();
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						/* ssn */
						ret = ((sr_kemi_fmssn_f)(ket->func))(lmsg,
								&vps[0].s, &vps[1].s, vps[2].n);
						return sr_kemi_apy_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						/* sss */
						ret = ((sr_kemi_fmsss_f)(ket->func))(lmsg,
								&vps[0].s, &vps[1].s, &vps[2].s);
						return sr_kemi_apy_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_kemi_apy_return_false();
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_kemi_apy_return_false();
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		case 4:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsssn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssns_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnss_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnsn_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnns_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnn_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsss_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnssn_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsns_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnn_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnss_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnsn_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnns_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnn_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		case 5:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssssn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssns_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsssnn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssnss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnsn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssnns_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmssnnn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnsss_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnssn_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnsns_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnsnn_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnnss_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnsn_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsnnns_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmsnnnn_f)(ket->func))(lmsg,
						&vps[0].s, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnssss_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsssn_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnssns_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnssnn_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsnss_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnsn_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnsnns_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnsnnn_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnsss_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnssn_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, &vps[2].s, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnsns_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnsnn_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, &vps[2].s, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnnss_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnsn_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, vps[2].n, &vps[3].s, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmnnnns_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n, &vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_INT
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT
					&& ket->ptypes[4]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmnnnnn_f)(ket->func))(lmsg,
						vps[0].n, vps[1].n, vps[2].n, vps[3].n, vps[4].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n", fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		case 6:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR
					&& ket->ptypes[5]==SR_KEMIP_STR) {
				/* ssssss */
				ret = ((sr_kemi_fmssssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s, &vps[5].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		default:
			LM_ERR("invalid parameters for: %.*s\n",
					fname.len, fname.s);
			return sr_kemi_apy_return_false();
	}
}

/**
 *
 */
PyObject *sr_apy_kemi_exec_func(PyObject *self, PyObject *args, int idx)
{
	sr_kemi_t *ket = NULL;
	PyObject *ret = NULL;
	PyThreadState *pstate = NULL;
	PyFrameObject *pframe = NULL;
#if PY_VERSION_HEX >= 0x030B0000
	PyCodeObject *pcode = NULL;
#endif
	struct timeval tvb = {0}, tve = {0};
	struct timezone tz;
	unsigned int tdiff;

	ket = sr_apy_kemi_export_get(idx);
	if(ket==NULL) {
		return sr_kemi_apy_return_false();
	}
	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tvb, &tz);
	}

	ret = sr_apy_kemi_exec_func_ex(ket, self, args, idx);

	if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)
			&& is_printable(cfg_get(core, core_cfg, latency_log))) {
		gettimeofday(&tve, &tz);
		tdiff = (tve.tv_sec - tvb.tv_sec) * 1000000
				   + (tve.tv_usec - tvb.tv_usec);
		if(tdiff >= cfg_get(core, core_cfg, latency_limit_action)) {
			pstate = PyThreadState_GET();
			if (pstate != NULL) {
#if PY_VERSION_HEX >= 0x030B0000
				pframe = PyThreadState_GetFrame(pstate);
				if(pframe != NULL) {
					pcode = PyFrame_GetCode(pframe);
				}
#else
				pframe = pstate->frame;
#endif
			}

#if PY_VERSION_HEX >= 0x030B0000
			LOG(cfg_get(core, core_cfg, latency_log),
					"alert - action KSR.%s%s%s(...)"
					" took too long [%u ms] (file:%s func:%s line:%d)\n",
					(ket->mname.len>0)?ket->mname.s:"",
					(ket->mname.len>0)?".":"", ket->fname.s, tdiff,
					(pcode)?PyBytes_AsString(pcode->co_filename):"",
					(pcode)?PyBytes_AsString(pcode->co_name):"",
					(pframe)?PyFrame_GetLineNumber(pframe):0);
#else
			LOG(cfg_get(core, core_cfg, latency_log),
					"alert - action KSR.%s%s%s(...)"
					" took too long [%u ms] (file:%s func:%s line:%d)\n",
					(ket->mname.len>0)?ket->mname.s:"",
					(ket->mname.len>0)?".":"", ket->fname.s, tdiff,
					(pframe && pframe->f_code)?PyBytes_AsString(pframe->f_code->co_filename):"",
					(pframe && pframe->f_code)?PyBytes_AsString(pframe->f_code->co_name):"",
					(pframe && pframe->f_code)?PyCode_Addr2Line(pframe->f_code, pframe->f_lasti):0);
#endif
		}
	}

	return ret;
}

/**
 *
 */
PyObject *_sr_apy_ksr_module = NULL;
PyObject **_sr_apy_ksr_modules_list = NULL;

PyMethodDef *_sr_KSRMethods = NULL;
#define SR_APY_KSR_MODULES_SIZE	256
#define SR_APY_KSR_METHODS_SIZE	(SR_APY_KEMI_EXPORT_SIZE + SR_APY_KSR_MODULES_SIZE)

/**
 *
 */
static int sr_apy_kemi_f_ktest(sip_msg_t *msg, str *txt)
{
	if(txt!=NULL && txt->s!=NULL)
		LM_DBG("%.*s", txt->len, txt->s);
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t _sr_apy_kemi_test[] = {
	{ str_init(""), str_init("ktest"),
		SR_KEMIP_NONE, sr_apy_kemi_f_ktest,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

static struct PyModuleDef KSR_moduledef = {
        PyModuleDef_HEAD_INIT,
        "KSR",
        NULL,
        -1,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
};

/**
 *
 */
static PyMethodDef _sr_apy_kemi_x_Methods[] = {
	{"modf", (PyCFunction)msg_call_function, METH_VARARGS,
		"Invoke function exported by the other module."},
	{NULL, NULL, 0, NULL} /* sentinel */
};

static struct PyModuleDef KSR_x_moduledef = {
        PyModuleDef_HEAD_INIT,
        "KSR.x",
        NULL,
        -1,
        _sr_apy_kemi_x_Methods,
        NULL,
        NULL,
        NULL,
        NULL
};

/* forward */
static PyObject* init_KSR(void);
int sr_apy_init_ksr(void) {
	PyImport_AppendInittab("KSR", &init_KSR);
	return 0;
}
/**
 *
 */
static
PyObject* init_KSR(void)
{
	PyMethodDef *_sr_crt_KSRMethods = NULL;
	sr_kemi_module_t *emods = NULL;
	int emods_size = 0;
	int i;
	int k;
	int m;
	int n;
	char mname[128];

	/* init faked sip msg */
	if(faked_msg_init()<0)
	{
		LM_ERR("failed to init local faked sip msg\n");
		return NULL;
	}

	_sr_KSRMethods = malloc(SR_APY_KSR_METHODS_SIZE * sizeof(PyMethodDef));
	if(_sr_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return NULL;
	}
	_sr_apy_ksr_modules_list = malloc(SR_APY_KSR_MODULES_SIZE * sizeof(PyObject*));
	if(_sr_apy_ksr_modules_list==NULL) {
		LM_ERR("no more pkg memory\n");
		return NULL;
	}
	memset(_sr_KSRMethods, 0, SR_APY_KSR_METHODS_SIZE * sizeof(PyMethodDef));
	memset(_sr_apy_ksr_modules_list, 0, SR_APY_KSR_MODULES_SIZE * sizeof(PyObject*));

	emods_size = sr_kemi_modules_size_get();
	emods = sr_kemi_modules_get();

	n = 0;
	_sr_crt_KSRMethods = _sr_KSRMethods;
	if(emods_size==0 || emods[0].kexp==NULL) {
		LM_DBG("exporting KSR.%s(...)\n", _sr_apy_kemi_test[0].fname.s);
		_sr_crt_KSRMethods[0].ml_name = _sr_apy_kemi_test[0].fname.s;
		_sr_crt_KSRMethods[0].ml_meth = sr_apy_kemi_export_associate(&_sr_apy_kemi_test[0]);
		_sr_crt_KSRMethods[0].ml_flags = METH_VARARGS;
		_sr_crt_KSRMethods[0].ml_doc = NAME " exported function";
	} else {
		for(i=0; emods[0].kexp[i].func!=NULL; i++) {
			LM_DBG("exporting KSR.%s(...)\n", emods[0].kexp[i].fname.s);
			_sr_crt_KSRMethods[i].ml_name = emods[0].kexp[i].fname.s;
			_sr_crt_KSRMethods[i].ml_meth =
				sr_apy_kemi_export_associate(&emods[0].kexp[i]);
			if(_sr_crt_KSRMethods[i].ml_meth == NULL) {
				LM_ERR("failed to associate kemi function with python export\n");
				free(_sr_KSRMethods);
				_sr_KSRMethods = NULL;
				return NULL;
			}
			_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
			_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
			n++;
		}
	}

	KSR_moduledef.m_methods = _sr_crt_KSRMethods;
	_sr_apy_ksr_module = PyModule_Create(&KSR_moduledef);

	Py_INCREF(_sr_apy_ksr_module);

	m = 0;

	/* special sub-modules - x.modf() can have variable number of params */
	_sr_apy_ksr_modules_list[m] = PyModule_Create(&KSR_x_moduledef);
	PyModule_AddObject(_sr_apy_ksr_module, "x", _sr_apy_ksr_modules_list[m]);
	Py_INCREF(_sr_apy_ksr_modules_list[m]);
	m++;

	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
			n++;
			_sr_crt_KSRMethods = _sr_KSRMethods + n;
			snprintf(mname, 128, "KSR.%s", emods[k].kexp[0].mname.s);
			for(i=0; emods[k].kexp[i].func!=NULL; i++) {
				LM_DBG("exporting %s.%s(...)\n", mname,
						emods[k].kexp[i].fname.s);
				_sr_crt_KSRMethods[i].ml_name = emods[k].kexp[i].fname.s;
				_sr_crt_KSRMethods[i].ml_meth =
					sr_apy_kemi_export_associate(&emods[k].kexp[i]);
				if(_sr_crt_KSRMethods[i].ml_meth == NULL) {
					LM_ERR("failed to associate kemi function with python export\n");
					free(_sr_KSRMethods);
					_sr_KSRMethods = NULL;
					return NULL;
				}
				_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
				_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
				n++;
			}
			LM_DBG("initializing kemi sub-module: %s (%s)\n", mname,
					emods[k].kexp[0].mname.s);

			PyModuleDef *mmodule  = malloc(sizeof(PyModuleDef));
			memset(mmodule, 0, sizeof(PyModuleDef));
			mmodule->m_name = strndup(mname, 127);
			mmodule->m_methods = _sr_crt_KSRMethods;
			mmodule->m_size = -1;

			_sr_apy_ksr_modules_list[m] = PyModule_Create(mmodule);
			PyModule_AddObject(_sr_apy_ksr_module, emods[k].kexp[0].mname.s, _sr_apy_ksr_modules_list[m]);
			Py_INCREF(_sr_apy_ksr_modules_list[m]);
			m++;
		}
	}
	LM_DBG("module 'KSR' has been initialized\n");
	return _sr_apy_ksr_module;
}

/**
 *
 */
void sr_apy_destroy_ksr(void)
{
	if(_sr_apy_ksr_module!=NULL) {
		Py_XDECREF(_sr_apy_ksr_module);
		_sr_apy_ksr_module = NULL;
	}
	if(_sr_KSRMethods!=NULL) {
		free(_sr_KSRMethods);
		_sr_KSRMethods = NULL;
	}

	LM_DBG("module 'KSR' has been destroyed\n");
}

/**
 *
 */
int apy_sr_init_mod(void)
{
	if(_sr_python_reload_version == NULL) {
		_sr_python_reload_version = (int*)shm_malloc(sizeof(int));
		if(_sr_python_reload_version == NULL) {
			LM_ERR("failed to allocated reload version\n");
			return -1;
		}
		*_sr_python_reload_version = 0;
	}
	_sr_python_reload_lock = lock_alloc();
	lock_init(_sr_python_reload_lock);
	return 0;
}

static const char* app_python_rpc_reload_doc[2] = {
	"Reload python file",
	0
};


static void app_python_rpc_reload(rpc_t* rpc, void* ctx)
{
	int v;
	void *vh;

	if(_sr_python_load_file.s == NULL && _sr_python_load_file.len<=0) {
		LM_WARN("script file path not provided\n");
		rpc->fault(ctx, 500, "No script file");
		return;
	}
	if(_sr_python_reload_version == NULL) {
		LM_WARN("reload not enabled\n");
		rpc->fault(ctx, 500, "Reload not enabled");
		return;
	}

	_sr_python_local_version = v = *_sr_python_reload_version;
	*_sr_python_reload_version += 1;
	LM_INFO("marking for reload Python script file: %.*s (%d => %d)\n",
		_sr_python_load_file.len, _sr_python_load_file.s,
		v,
		*_sr_python_reload_version);

	if (rpc->add(ctx, "{", &vh) < 0) {
		rpc->fault(ctx, 500, "Server error");
		return;
	}
	rpc->struct_add(vh, "dd",
			"old", v,
			"new", *_sr_python_reload_version);

	return;
}

static const char* app_python_rpc_api_list_doc[2] = {
	"List kemi exports to javascript",
	0
};

static void app_python_rpc_api_list(rpc_t* rpc, void* ctx)
{
	int i;
	int n;
	sr_kemi_t *ket;
	void* th;
	void* sh;
	void* ih;

	if (rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}
	n = 0;
	for(i=0; i<SR_APY_KSR_METHODS_SIZE ; i++) {
		ket = sr_apy_kemi_export_get(i);
		if(ket==NULL) continue;
		n++;
	}

	if(rpc->struct_add(th, "d[",
				"msize", n,
				"methods",  &ih)<0)
	{
		rpc->fault(ctx, 500, "Internal error array structure");
		return;
	}
	for(i=0; i<SR_APY_KSR_METHODS_SIZE; i++) {
		ket = sr_apy_kemi_export_get(i);
		if(ket==NULL) continue;
		if(rpc->struct_add(ih, "{", "func", &sh)<0) {
			rpc->fault(ctx, 500, "Internal error internal structure");
			return;
		}
		if(rpc->struct_add(sh, "SSSS",
				"ret", sr_kemi_param_map_get_name(ket->rtype),
				"module", &ket->mname,
				"name", &ket->fname,
				"params", sr_kemi_param_map_get_params(ket->ptypes))<0) {
			LM_ERR("failed to add the structure with attributes (%d)\n", i);
			rpc->fault(ctx, 500, "Internal error creating dest struct");
			return;
		}
	}
}

rpc_export_t app_python_rpc_cmds[] = {
	{"app_python.reload", app_python_rpc_reload,
		app_python_rpc_reload_doc, 0},
	{"app_python.api_list", app_python_rpc_api_list,
		app_python_rpc_api_list_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int app_python_init_rpc(void)
{
	if (rpc_register_array(app_python_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
