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

#include "../../core/dprint.h"
#include "../../core/route.h"
#include "../../core/fmsg.h"
#include "../../core/kemi.h"
#include "../../core/pvar.h"
#include "../../core/mem/pkg.h"
#include "../../core/mem/shm.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "msgobj_struct.h"
#include "python_exec.h"
#include "apy_kemi_export.h"
#include "apy_kemi.h"

static int *_sr_python_reload_version = NULL;
static int _sr_python_local_version = 0;
extern str _sr_python_load_file;
extern int _apy_process_rank;

/**
 * 
 */

int apy_reload_script(void)
{
	if(_sr_python_reload_version == NULL) {
		return 0;
	}
	if(*_sr_python_reload_version == _sr_python_local_version) {
		return 0;
	}
	if(apy_load_script()<0) {
		LM_ERR("failed to load script file\n");
		return -1;
	}
	if(apy_init_script(_apy_process_rank)<0) {
		LM_ERR("failed to init script\n");
		return -1;
	}
	_sr_python_local_version = *_sr_python_reload_version;
	return 0;
}
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
		ret = apy_exec(msg, "ksr_reply_route", NULL, 0);
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
		ret = apy_exec(msg, "ksr_onsend_route", NULL, 0);
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
	return PyInt_FromLong((long)rval);
}

/**
 *
 */
PyObject *sr_apy_kemi_return_str(sr_kemi_t *ket, char *sval, int slen)
{
	return PyString_FromStringAndSize(sval, slen);
}
/**
 *
 */
PyObject *sr_apy_kemi_exec_func(PyObject *self, PyObject *args, int idx)
{
	str fname;
	int i;
	int ret;
	sr_kemi_t *ket = NULL;
	sr_kemi_val_t vps[SR_KEMI_PARAMS_MAX];
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;

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

	ket = sr_apy_kemi_export_get(idx);
	if(ket==NULL) {
		return sr_kemi_apy_return_false();
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
		ret = ((sr_kemi_fm_f)(ket->func))(lmsg);
		return sr_kemi_apy_return_int(ket, ret);
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
		} else {
			if(!PyArg_ParseTuple(args, "s:kemi-param-s", &vps[0].s.s)) {
				LM_ERR("unable to retrieve str param %d\n", 0);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			LM_DBG("param[%d] for: %.*s is str: %.*s\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s);
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
		} else {
			if(!PyArg_ParseTuple(args, "ss:kemi-param-ss", &vps[0].s.s, &vps[1].s.s)) {
				LM_ERR("unable to retrieve str-str param %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are str: [%.*s] [%.*s]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s,
				vps[1].s.len, vps[1].s.s);
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
			LM_DBG("params[%d] for: %.*s are str-str-int: [%.*s] [%.*s]"
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
				|| ket->ptypes[1]==SR_KEMIP_STR
				|| ket->ptypes[2]==SR_KEMIP_STR
				|| ket->ptypes[3]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "ssss:kemi-param-ssss",
						&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s)) {
				LM_ERR("unable to retrieve str-str-str-str params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			LM_DBG("params[%d] for: %.*s are str: [%.*s] [%.*s]"
					" [%.*s] [%.*s]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s,
				vps[1].s.len, vps[1].s.s, vps[2].s.len, vps[2].s.s,
				vps[3].s.len, vps[3].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR
				|| ket->ptypes[1]==SR_KEMIP_STR
				|| ket->ptypes[2]==SR_KEMIP_INT
				|| ket->ptypes[3]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ssii:kemi-param-ssnn",
						&vps[0].s.s, &vps[1].s.s, &vps[2].n, &vps[3].n)) {
				LM_ERR("unable to retrieve str-str-int-int params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are str: [%.*s] [%.*s]"
					" [%d] [%d]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s,
				vps[1].s.len, vps[1].s.s, vps[2].n, vps[3].n);
		} else {
			LM_ERR("not implemented yet\n");
			return sr_kemi_apy_return_false();
		}
	} else if(ket->ptypes[5]==SR_KEMIP_NONE) {
		i = 5;
		if(ket->ptypes[0]==SR_KEMIP_STR
				|| ket->ptypes[1]==SR_KEMIP_STR
				|| ket->ptypes[2]==SR_KEMIP_STR
				|| ket->ptypes[3]==SR_KEMIP_STR
				|| ket->ptypes[4]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "sssss:kemi-param-sssss",
						&vps[0].s.s, &vps[1].s.s, &vps[2].s.s, &vps[3].s.s,
						&vps[4].s.s)) {
				LM_ERR("unable to retrieve str-str-str-str params %d\n", i);
				return sr_kemi_apy_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			vps[2].s.len = strlen(vps[2].s.s);
			vps[3].s.len = strlen(vps[3].s.s);
			vps[4].s.len = strlen(vps[4].s.s);
			LM_DBG("params[%d] for: %.*s are str: [%.*s] [%.*s]"
					" [%.*s] [%.*s] [%.*s]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s,
				vps[1].s.len, vps[1].s.s, vps[2].s.len, vps[2].s.s,
				vps[3].s.len, vps[3].s.s, vps[4].s.len, vps[4].s.s);
		} else {
			LM_ERR("not implemented yet\n");
			return sr_kemi_apy_return_false();
		}
	} else {
		i = 6;
		if(ket->ptypes[0]==SR_KEMIP_STR
				|| ket->ptypes[1]==SR_KEMIP_STR
				|| ket->ptypes[2]==SR_KEMIP_STR
				|| ket->ptypes[3]==SR_KEMIP_STR
				|| ket->ptypes[4]==SR_KEMIP_STR
				|| ket->ptypes[5]==SR_KEMIP_STR) {
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
				ret = ((sr_kemi_fmn_f)(ket->func))(lmsg, vps[0].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fms_f)(ket->func))(lmsg, &vps[0].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		case 2:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmnn_f)(ket->func))(lmsg, vps[0].n, vps[1].n);
					return sr_kemi_apy_return_int(ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmns_f)(ket->func))(lmsg, vps[0].n, &vps[1].s);
					return sr_kemi_apy_return_int(ket, ret);
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_kemi_apy_return_false();
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmsn_f)(ket->func))(lmsg, &vps[0].s, vps[1].n);
					return sr_kemi_apy_return_int(ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmss_f)(ket->func))(lmsg, &vps[0].s, &vps[1].s);
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
				/* ssss */
				ret = ((sr_kemi_fmssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				/* ssnn */
				ret = ((sr_kemi_fmsssn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_INT
					&& ket->ptypes[3]==SR_KEMIP_INT) {
				/* ssnn */
				ret = ((sr_kemi_fmssnn_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, vps[2].n, vps[3].n);
				return sr_kemi_apy_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_INT
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR) {
				/* ssnn */
				ret = ((sr_kemi_fmnsss_f)(ket->func))(lmsg,
						vps[0].n, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_kemi_apy_return_false();
			}
		break;
		case 5:
			if(ket->ptypes[0]==SR_KEMIP_STR
					&& ket->ptypes[1]==SR_KEMIP_STR
					&& ket->ptypes[2]==SR_KEMIP_STR
					&& ket->ptypes[3]==SR_KEMIP_STR
					&& ket->ptypes[4]==SR_KEMIP_STR) {
				/* sssss */
				ret = ((sr_kemi_fmsssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s);
				return sr_kemi_apy_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
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
PyObject *_sr_apy_ksr_module = NULL;
PyObject *_sr_apy_ksr_module_dict = NULL;
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
static sr_kemi_t _sr_apy_kemi_test[] = {
	{ str_init(""), str_init("ktest"),
		SR_KEMIP_NONE, sr_apy_kemi_f_ktest,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
PyObject *sr_apy_kemi_return_none_mode(int rmode)
{
	if(rmode) {
		return sr_apy_kemi_return_str(NULL, "<<null>>", 8);
	} else {
		return sr_apy_kemi_return_none();
	}
}

/**
 *
 */
static PyObject *sr_apy_kemi_f_pv_get_mode(PyObject *self, PyObject *args,
		int rmode)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;

	env_P = sr_apy_env_get();

	if(env_P==NULL) {
		LM_ERR("invalid Python environment attributes\n");
		return sr_apy_kemi_return_none_mode(rmode);
	}
	if(env_P->msg==NULL) {
		lmsg = faked_msg_next();
	} else {
		lmsg = env_P->msg;
	}

	if(!PyArg_ParseTuple(args, "s:pv.get", &pvn.s)) {
		LM_ERR("unable to retrieve str param\n");
		return sr_apy_kemi_return_none_mode(rmode);
	}

	if(pvn.s==NULL || lmsg==NULL) {
		LM_ERR("invalid context attributes\n");
		return sr_apy_kemi_return_none_mode(rmode);
	}

	pvn.len = strlen(pvn.s);
	LM_DBG("pv get: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return sr_apy_kemi_return_none_mode(rmode);
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return sr_apy_kemi_return_none_mode(rmode);
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(lmsg, pvs, &val) != 0)
	{
		LM_ERR("unable to get pv value for [%s]\n", pvn.s);
		return sr_apy_kemi_return_none_mode(rmode);
	}
	if(val.flags&PV_VAL_NULL) {
		return sr_apy_kemi_return_none_mode(rmode);
	}
	if(val.flags&PV_TYPE_INT) {
		return sr_kemi_apy_return_int(NULL, val.ri);
	}
	return sr_apy_kemi_return_str(NULL, val.rs.s, val.rs.len);
}

/**
 *
 */
static PyObject *sr_apy_kemi_f_pv_get(PyObject *self, PyObject *args)
{
	return sr_apy_kemi_f_pv_get_mode(self, args, 0);
}

/**
 *
 */
static PyObject *sr_apy_kemi_f_pv_getw(PyObject *self, PyObject *args)
{
	return sr_apy_kemi_f_pv_get_mode(self, args, 1);
}

/**
 *
 */
static PyObject *sr_apy_kemi_f_pv_seti(PyObject *self, PyObject *args)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;

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

	memset(&val, 0, sizeof(pv_value_t));
	if(!PyArg_ParseTuple(args, "si:pv.seti", &pvn.s, &val.ri)) {
		LM_ERR("unable to retrieve str-int params\n");
		return sr_kemi_apy_return_false();
	}

	if(pvn.s==NULL || lmsg==NULL) {
		LM_ERR("invalid context attributes\n");
		return sr_kemi_apy_return_false();
	}
	val.flags |= PV_TYPE_INT|PV_VAL_INT;
	pvn.len = strlen(pvn.s);

	LM_DBG("pv set: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return sr_kemi_apy_return_false();
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return sr_kemi_apy_return_false();
	}
	if(pv_set_spec_value(lmsg, pvs, 0, &val)<0)
	{
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return sr_kemi_apy_return_false();
	}
	return sr_kemi_apy_return_true();
}

/**
 *
 */
static PyObject *sr_apy_kemi_f_pv_sets(PyObject *self, PyObject *args)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;

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

	memset(&val, 0, sizeof(pv_value_t));
	if(!PyArg_ParseTuple(args, "ss:pv.sets", &pvn.s, &val.rs.s)) {
		LM_ERR("unable to retrieve str-int params\n");
		return sr_kemi_apy_return_false();
	}

	if(pvn.s==NULL || val.rs.s==NULL || lmsg==NULL) {
		LM_ERR("invalid context attributes\n");
		return sr_kemi_apy_return_false();
	}

	val.rs.len = strlen(val.rs.s);
	val.flags |= PV_VAL_STR;

	pvn.len = strlen(pvn.s);
	LM_DBG("pv set: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return sr_kemi_apy_return_false();
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return sr_kemi_apy_return_false();
	}
	if(pv_set_spec_value(lmsg, pvs, 0, &val)<0) {
		LM_ERR("unable to set pv [%s]\n", pvn.s);
		return sr_kemi_apy_return_false();
	}

	return sr_kemi_apy_return_true();
}

/**
 *
 */
static PyObject *sr_apy_kemi_f_pv_unset(PyObject *self, PyObject *args)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;

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

	if(!PyArg_ParseTuple(args, "s:pv.unset", &pvn.s)) {
		LM_ERR("unable to retrieve str param\n");
		return sr_kemi_apy_return_false();
	}

	if(pvn.s==NULL || lmsg==NULL) {
		LM_ERR("invalid context attributes\n");
		return sr_kemi_apy_return_false();
	}

	pvn.len = strlen(pvn.s);
	LM_DBG("pv unset: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return sr_kemi_apy_return_false();
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return sr_kemi_apy_return_false();
	}
	memset(&val, 0, sizeof(pv_value_t));
	val.flags |= PV_VAL_NULL;
	if(pv_set_spec_value(lmsg, pvs, 0, &val)<0) {
		LM_ERR("unable to unset pv [%s]\n", pvn.s);
		return sr_kemi_apy_return_false();
	}

	return sr_kemi_apy_return_true();
}

/**
 *
 */
static PyObject *sr_apy_kemi_f_pv_is_null(PyObject *self, PyObject *args)
{
	str pvn;
	pv_spec_t *pvs;
	pv_value_t val;
	int pl;
	sr_apy_env_t *env_P;
	sip_msg_t *lmsg = NULL;

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

	if(!PyArg_ParseTuple(args, "s:pv.unset", &pvn.s)) {
		LM_ERR("unable to retrieve str param\n");
		return sr_kemi_apy_return_false();
	}

	if(pvn.s==NULL || lmsg==NULL) {
		LM_ERR("invalid context attributes\n");
		return sr_kemi_apy_return_false();
	}

	pvn.len = strlen(pvn.s);
	LM_DBG("pv is null test: %s\n", pvn.s);
	pl = pv_locate_name(&pvn);
	if(pl != pvn.len) {
		LM_ERR("invalid pv [%s] (%d/%d)\n", pvn.s, pl, pvn.len);
		return sr_kemi_apy_return_false();
	}
	pvs = pv_cache_get(&pvn);
	if(pvs==NULL) {
		LM_ERR("cannot get pv spec for [%s]\n", pvn.s);
		return sr_kemi_apy_return_true();
	}
	memset(&val, 0, sizeof(pv_value_t));
	if(pv_get_spec_value(lmsg, pvs, &val) != 0) {
		LM_NOTICE("unable to get pv value for [%s]\n", pvn.s);
		return sr_kemi_apy_return_true();
	}
	if(val.flags&PV_VAL_NULL) {
		return sr_kemi_apy_return_true();
	} else {
		return sr_kemi_apy_return_false();
	}
}

static PyMethodDef _sr_apy_kemi_pv_Methods[] = {
	{"get",		sr_apy_kemi_f_pv_get,		METH_VARARGS,
		NAME " - pv get value"},
	{"getw",	sr_apy_kemi_f_pv_getw,		METH_VARARGS,
		NAME " - pv get value or <<null>>"},
	{"seti",	sr_apy_kemi_f_pv_seti,		METH_VARARGS,
		NAME " - pv set int value"},
	{"sets",	sr_apy_kemi_f_pv_sets,		METH_VARARGS,
		NAME " - pv set str value"},
	{"unset",	sr_apy_kemi_f_pv_unset,		METH_VARARGS,
		NAME " - pv uset value (assign $null)"},
	{"is_null",	sr_apy_kemi_f_pv_is_null,	METH_VARARGS,
		NAME " - pv test if it is $null"},

	{NULL, 		NULL, 			0, 		NULL}
};

/**
 *
 */
static PyMethodDef _sr_apy_kemi_x_Methods[] = {
	{"modf", (PyCFunction)msg_call_function, METH_VARARGS,
		"Invoke function exported by the other module."},
	{NULL, NULL, 0, NULL} /* sentinel */
};

/**
 *
 */
int sr_apy_init_ksr(void)
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
		return -1;
	}

	_sr_KSRMethods = malloc(SR_APY_KSR_METHODS_SIZE * sizeof(PyMethodDef));
	if(_sr_KSRMethods==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	_sr_apy_ksr_modules_list = malloc(SR_APY_KSR_MODULES_SIZE * sizeof(PyObject*));
	if(_sr_apy_ksr_modules_list==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
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
				return -1;
			}
			_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
			_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
			n++;
		}
	}

	_sr_apy_ksr_module = Py_InitModule("KSR", _sr_crt_KSRMethods);
	_sr_apy_ksr_module_dict = PyModule_GetDict(_sr_apy_ksr_module);

	Py_INCREF(_sr_apy_ksr_module);

	m = 0;

	/* special sub-modules - pv.get() can return int or string */
	_sr_apy_ksr_modules_list[m] = Py_InitModule("KSR.pv",
			_sr_apy_kemi_pv_Methods);
	PyDict_SetItemString(_sr_apy_ksr_module_dict,
			"pv", _sr_apy_ksr_modules_list[m]);
	Py_INCREF(_sr_apy_ksr_modules_list[m]);
	m++;
	/* special sub-modules - x.modf() can have variable number of params */
	_sr_apy_ksr_modules_list[m] = Py_InitModule("KSR.x",
			_sr_apy_kemi_x_Methods);
	PyDict_SetItemString(_sr_apy_ksr_module_dict,
			"x", _sr_apy_ksr_modules_list[m]);
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
					return -1;
				}
				_sr_crt_KSRMethods[i].ml_flags = METH_VARARGS;
				_sr_crt_KSRMethods[i].ml_doc = NAME " exported function";
				n++;
			}
			LM_DBG("initializing kemi sub-module: %s (%s)\n", mname,
					emods[k].kexp[0].mname.s);
			_sr_apy_ksr_modules_list[m] = Py_InitModule(mname, _sr_crt_KSRMethods);
			PyDict_SetItemString(_sr_apy_ksr_module_dict,
					emods[k].kexp[0].mname.s, _sr_apy_ksr_modules_list[m]);
			Py_INCREF(_sr_apy_ksr_modules_list[m]);
			m++;
		}
	}
	LM_DBG("module 'KSR' has been initialized\n");
	return 0;
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
	if(_sr_apy_ksr_module_dict!=NULL) {
		Py_XDECREF(_sr_apy_ksr_module_dict);
		_sr_apy_ksr_module_dict = NULL;
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

	v = *_sr_python_reload_version;
	LM_INFO("marking for reload js script file: %.*s (%d => %d)\n",
				_sr_python_load_file.len, _sr_python_load_file.s,
				_sr_python_local_version, v);
	*_sr_python_reload_version += 1;

	if(apy_reload_script()<0) {
		rpc->fault(ctx, 500, "Reload failed");
		return;	
	}

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