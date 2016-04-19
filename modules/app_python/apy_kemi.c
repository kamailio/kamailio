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

#include "../../dprint.h"
#include "../../route.h"
#include "../../fmsg.h"
#include "../../kemi.h"
#include "../../mem/pkg.h"

#include "python_exec.h"
#include "apy_kemi_export.h"
#include "apy_kemi.h"

/**
 *
 */
int sr_kemi_config_engine_python(sip_msg_t *msg, int rtype, str *rname)
{
	int ret;

	ret = -1;
	if(rtype==REQUEST_ROUTE) {
		ret = apy_exec(msg, "ksr_request_route", NULL, 1);
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
			ret = apy_exec(msg, rname->s, NULL, 0);
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
PyObject *sr_apy_kemi_return_true(void)
{
	Py_INCREF(Py_True);
	return Py_True;
}

/**
 *
 */
PyObject *sr_apy_kemi_return_false(void)
{
	Py_INCREF(Py_False);
	return Py_False;
}


/**
 *
 */
PyObject *sr_apy_kemi_return_int(sr_kemi_t *ket, int rval)
{
	return PyInt_FromLong((long)rval);
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
		return sr_apy_kemi_return_false();
	}
	if(env_P->msg==NULL) {
		lmsg = faked_msg_next();
	} else {
		lmsg = env_P->msg;
	}

	ket = sr_apy_kemi_export_get(idx);
	if(ket==NULL) {
		return sr_apy_kemi_return_false();
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
		return sr_apy_kemi_return_int(ket, ret);
	}

	memset(vps, 0, SR_KEMI_PARAMS_MAX*sizeof(sr_kemi_val_t));
	if(ket->ptypes[1]==SR_KEMIP_NONE) {
		i = 1;
		if(ket->ptypes[0]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "i:kemi-param-n", &vps[0].n)) {
				LM_ERR("unable to retrieve int param %d\n", 0);
				return sr_apy_kemi_return_false();
			}
			LM_DBG("param[%d] for: %.*s is int: %d\n", i,
				fname.len, fname.s, vps[0].n);
		} else {
			if(!PyArg_ParseTuple(args, "s:kemi-param-s", &vps[0].s.s)) {
				LM_ERR("unable to retrieve str param %d\n", 0);
				return sr_apy_kemi_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			LM_DBG("param[%d] for: %.*s is str: %.*s\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s);
		}
	} else if(ket->ptypes[2]==SR_KEMIP_NONE) {
		i = 2;
		if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "ii:kemi-param-n", &vps[0].n, &vps[1].n)) {
				LM_ERR("unable to retrieve int-int params %d\n", i);
				return sr_apy_kemi_return_false();
			}
			LM_DBG("params[%d] for: %.*s are int-int: [%d] [%d]\n", i,
				fname.len, fname.s, vps[0].n, vps[1].n);
		} else if(ket->ptypes[0]==SR_KEMIP_INT && ket->ptypes[1]==SR_KEMIP_STR) {
			if(!PyArg_ParseTuple(args, "is:kemi-param-n", &vps[0].n, &vps[1].s.s)) {
				LM_ERR("unable to retrieve int-str params %d\n", i);
				return sr_apy_kemi_return_false();
			}
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are int-str: [%d] [%.*s]\n", i,
				fname.len, fname.s, vps[0].n, vps[1].s.len, vps[1].s.s);
		} else if(ket->ptypes[0]==SR_KEMIP_STR && ket->ptypes[1]==SR_KEMIP_INT) {
			if(!PyArg_ParseTuple(args, "si:kemi-param-n", &vps[0].s.s, &vps[1].n)) {
				LM_ERR("unable to retrieve str-int params %d\n", i);
				return sr_apy_kemi_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			LM_DBG("params[%d] for: %.*s are str-int: [%.*s] [%d]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s, vps[1].n);
		} else {
			if(!PyArg_ParseTuple(args, "ss:kemi-param-ss", &vps[0].s.s, &vps[1].s.s)) {
				LM_ERR("unable to retrieve str-str param %d\n", i);
				return sr_apy_kemi_return_false();
			}
			vps[0].s.len = strlen(vps[0].s.s);
			vps[1].s.len = strlen(vps[1].s.s);
			LM_DBG("params[%d] for: %.*s are str: [%.*s] [%.*s]\n", i,
				fname.len, fname.s, vps[0].s.len, vps[0].s.s,
				vps[1].s.len, vps[1].s.s);
		}

	} else if(ket->ptypes[3]==SR_KEMIP_NONE) {
		i = 3;
		LM_ERR("not implemented yet\n");
		return sr_apy_kemi_return_false();
	} else {
		LM_ERR("not implemented yet\n");
		return sr_apy_kemi_return_false();
	}

	switch(i) {
		case 1:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				ret = ((sr_kemi_fmn_f)(ket->func))(lmsg, vps[0].n);
				return sr_apy_kemi_return_int(ket, ret);
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fms_f)(ket->func))(lmsg, &vps[0].s);
				return sr_apy_kemi_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_apy_kemi_return_false();
			}
		break;
		case 2:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmnn_f)(ket->func))(lmsg, vps[0].n, vps[1].n);
					return sr_apy_kemi_return_int(ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmns_f)(ket->func))(lmsg, vps[0].n, &vps[1].s);
					return sr_apy_kemi_return_int(ket, ret);
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_apy_kemi_return_false();
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					ret = ((sr_kemi_fmsn_f)(ket->func))(lmsg, &vps[0].s, vps[1].n);
					return sr_apy_kemi_return_int(ket, ret);
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					ret = ((sr_kemi_fmss_f)(ket->func))(lmsg, &vps[0].s, &vps[1].s);
					return sr_apy_kemi_return_int(ket, ret);
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_apy_kemi_return_false();
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_apy_kemi_return_false();
			}
		break;
		case 3:
			if(ket->ptypes[0]==SR_KEMIP_INT) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmnnn_f)(ket->func))(lmsg,
								vps[0].n, vps[1].n, vps[2].n);
						return sr_apy_kemi_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmnns_f)(ket->func))(lmsg,
								vps[0].n, vps[1].n, &vps[2].s);
						return sr_apy_kemi_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_apy_kemi_return_false();
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmnsn_f)(ket->func))(lmsg,
								vps[0].n, &vps[1].s, vps[2].n);
						return sr_apy_kemi_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmnss_f)(ket->func))(lmsg,
								vps[0].n, &vps[1].s, &vps[2].s);
						return sr_apy_kemi_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_apy_kemi_return_false();
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_apy_kemi_return_false();
				}
			} else if(ket->ptypes[0]==SR_KEMIP_STR) {
				if(ket->ptypes[1]==SR_KEMIP_INT) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmsnn_f)(ket->func))(lmsg,
								&vps[0].s, vps[1].n, vps[2].n);
						return sr_apy_kemi_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmsns_f)(ket->func))(lmsg,
								&vps[0].s, vps[1].n, &vps[2].s);
						return sr_apy_kemi_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_apy_kemi_return_false();
					}
				} else if(ket->ptypes[1]==SR_KEMIP_STR) {
					if(ket->ptypes[2]==SR_KEMIP_INT) {
						ret = ((sr_kemi_fmssn_f)(ket->func))(lmsg,
								&vps[0].s, &vps[1].s, vps[2].n);
						return sr_apy_kemi_return_int(ket, ret);
					} else if(ket->ptypes[2]==SR_KEMIP_STR) {
						ret = ((sr_kemi_fmsss_f)(ket->func))(lmsg,
								&vps[0].s, &vps[1].s, &vps[2].s);
						return sr_apy_kemi_return_int(ket, ret);
					} else {
						LM_ERR("invalid parameters for: %.*s\n",
								fname.len, fname.s);
						return sr_apy_kemi_return_false();
					}
				} else {
					LM_ERR("invalid parameters for: %.*s\n",
							fname.len, fname.s);
					return sr_apy_kemi_return_false();
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_apy_kemi_return_false();
			}
		break;
		case 4:
			if(ket->ptypes[0]==SR_KEMIP_STR
					|| ket->ptypes[1]==SR_KEMIP_STR
					|| ket->ptypes[2]==SR_KEMIP_STR
					|| ket->ptypes[3]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s);
				return sr_apy_kemi_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_apy_kemi_return_false();
			}
		break;
		case 5:
			if(ket->ptypes[0]==SR_KEMIP_STR
					|| ket->ptypes[1]==SR_KEMIP_STR
					|| ket->ptypes[2]==SR_KEMIP_STR
					|| ket->ptypes[3]==SR_KEMIP_STR
					|| ket->ptypes[4]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmsssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s);
				return sr_apy_kemi_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_apy_kemi_return_false();
			}
		break;
		case 6:
			if(ket->ptypes[0]==SR_KEMIP_STR
					|| ket->ptypes[1]==SR_KEMIP_STR
					|| ket->ptypes[2]==SR_KEMIP_STR
					|| ket->ptypes[3]==SR_KEMIP_STR
					|| ket->ptypes[4]==SR_KEMIP_STR
					|| ket->ptypes[5]==SR_KEMIP_STR) {
				ret = ((sr_kemi_fmssssss_f)(ket->func))(lmsg,
						&vps[0].s, &vps[1].s, &vps[2].s, &vps[3].s,
						&vps[4].s, &vps[5].s);
				return sr_apy_kemi_return_int(ket, ret);
			} else {
				LM_ERR("invalid parameters for: %.*s\n",
						fname.len, fname.s);
				return sr_apy_kemi_return_false();
			}
		break;
		default:
			LM_ERR("invalid parameters for: %.*s\n",
					fname.len, fname.s);
			return sr_apy_kemi_return_false();
	}

	return sr_apy_kemi_return_false();
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
static int sr_apy_kemi_f_dbg(sip_msg_t *msg, str *txt)
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
		SR_KEMIP_NONE, sr_apy_kemi_f_dbg,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
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
	if(emods_size>1) {
		for(k=1; k<emods_size; k++) {
			n++;
			_sr_crt_KSRMethods += n;
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
