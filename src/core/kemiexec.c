/**
 * Copyright (C) 2022 Daniel-Constantin Mierla (asipto.com)
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

#include "dprint.h"
#include "parser/msg_parser.h"
#include "kemi.h"


/**
 *
 */
static sr_kemi_xval_t _sr_kemi_core_exec_xval;


/**
 *
 */
static inline sr_kemi_xval_t* sr_kemi_return_int(sr_kemi_t *ket, int ret)
{
	_sr_kemi_core_exec_xval.vtype = SR_KEMIP_INT;
	_sr_kemi_core_exec_xval.v.n = ret;
	return &_sr_kemi_core_exec_xval;
}


/**
 *
 */
static inline sr_kemi_xval_t* sr_kemi_return_false(sr_kemi_t *ket)
{
	_sr_kemi_core_exec_xval.vtype = SR_KEMIP_BOOL;
	_sr_kemi_core_exec_xval.v.n = SR_KEMI_FALSE;
	return &_sr_kemi_core_exec_xval;
}

/**
 *
 */
sr_kemi_xval_t* sr_kemi_exec_func(sr_kemi_t *ket, sip_msg_t *msg, int pno,
		sr_kemi_xval_t *vps)
{
	int ret;

	switch(pno) {
		case 1:
			if(ket->ptypes[0] & SR_KEMIP_INT) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmn_f)(ket->func))(msg, vps[0].v.n);
				} else {
					ret = ((sr_kemi_fmn_f)(ket->func))(msg, vps[0].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if(ket->ptypes[0] & SR_KEMIP_STR) {
				if(ket->rtype==SR_KEMIP_XVAL) {
					return ((sr_kemi_xfms_f)(ket->func))(msg, &vps[0].v.s);
				} else {
					ret = ((sr_kemi_fms_f)(ket->func))(msg, &vps[0].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", ket->fname.len, ket->fname.s);
				return sr_kemi_return_false(ket);
			}
		break;
		case 2:
			if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s);
				} else {
					ret = ((sr_kemi_fmss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n);
				} else {
					ret = ((sr_kemi_fmsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_LONG)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsv_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1]);
				} else {
					ret = ((sr_kemi_fmsv_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1]);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s);
				} else {
					ret = ((sr_kemi_fmns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n);
				} else {
					ret = ((sr_kemi_fmnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", ket->fname.len, ket->fname.s);
				return sr_kemi_return_false(ket);
			}
		break;
		case 3:
			if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s);
				} else {
					ret = ((sr_kemi_fmsss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n);
				} else {
					ret = ((sr_kemi_fmssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s);
				} else {
					ret = ((sr_kemi_fmsns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n);
				} else {
					ret = ((sr_kemi_fmsnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s);
				} else {
					ret = ((sr_kemi_fmnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n);
				} else {
					ret = ((sr_kemi_fmnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s);
				} else {
					ret = ((sr_kemi_fmnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n);
				} else {
					ret = ((sr_kemi_fmnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", ket->fname.len, ket->fname.s);
				return sr_kemi_return_false(ket);
			}
		break;
		case 4:
			if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmssss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmsssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmssns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmssnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmsnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmsnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmsnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmsnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmnsss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmnssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmnsns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmnsnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmnnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmnnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s);
				} else {
					ret = ((sr_kemi_fmnnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n);
				} else {
					ret = ((sr_kemi_fmnnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", ket->fname.len, ket->fname.s);
				return sr_kemi_return_false(ket);
			}
		break;
		case 5:
			if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmsssss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmssssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmsssns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmsssnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmssnss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnsn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmssnsn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmssnns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmssnnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmsnsss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnssn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmsnssn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmsnsns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmsnsnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmsnnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmsnnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmsnnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmsnnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnssss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnsssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnssns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnssnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnsnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnsnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnsnns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnsnnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnnsss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnssn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnnssn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnnsns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnnsnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnnnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnnnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
				} else {
					ret = ((sr_kemi_fmnnnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n);
				} else {
					ret = ((sr_kemi_fmnnnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", ket->fname.len, ket->fname.s);
				return sr_kemi_return_false(ket);
			}
		break;
		case 6:
			if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssssss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmssssss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsssssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssssns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmssssns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssssnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmssssnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssnss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsssnss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssnsn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsssnsn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssnns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsssnns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsssnnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsssnnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnsss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmssnsss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmssnssn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnsns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmssnsns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnsnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmssnsnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnnss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmssnnss_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnnsn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmssnnsn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnnns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmssnnns_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmssnnnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmssnnnn_f)(ket->func))(msg,
						&vps[0].v.s, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnssss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnssss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsssn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnsssn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnssns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnssns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnssnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnssnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnsnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnsnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnsnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnsnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnsnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnsss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnnsss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnssn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnnssn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnsns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnnsns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnsnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnnsnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnnnss_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnnnsn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmsnnnns_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_STR)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmsnnnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmsnnnnn_f)(ket->func))(msg,
						&vps[0].v.s, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsssss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnsssss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnssssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsssns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnsssns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsssnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnsssnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnssnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnssnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssnns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnssnns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnssnnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnssnnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnsss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnsnsss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnsnssn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnsns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnsnsns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnsnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnsnsnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnsnnss_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnsnnsn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnnns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnsnnns_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_STR)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnsnnnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnsnnnn_f)(ket->func))(msg,
						vps[0].v.n, &vps[1].v.s, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnssss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnssss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsssn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnsssn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnssns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnssns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnssnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnssnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnsnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnsnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnsnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_STR)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnsnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnsnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, &vps[2].v.s, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnsss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnnsss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnssn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnnssn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnsns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnnsns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_STR)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnsnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnnsnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, &vps[3].v.s, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnnnss_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_STR)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnnnsn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, &vps[4].v.s, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_STR)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
				} else {
					ret = ((sr_kemi_fmnnnnns_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, &vps[5].v.s);
					return sr_kemi_return_int(ket, ret);
				}
			} else if((ket->ptypes[0] & SR_KEMIP_INT)
					&& (ket->ptypes[1] & SR_KEMIP_INT)
					&& (ket->ptypes[2] & SR_KEMIP_INT)
					&& (ket->ptypes[3] & SR_KEMIP_INT)
					&& (ket->ptypes[4] & SR_KEMIP_INT)
					&& (ket->ptypes[5] & SR_KEMIP_INT)) {
				if(ket->rtype & SR_KEMIP_XVAL) {
					return ((sr_kemi_xfmnnnnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
				} else {
					ret = ((sr_kemi_fmnnnnnn_f)(ket->func))(msg,
						vps[0].v.n, vps[1].v.n, vps[2].v.n, vps[3].v.n, vps[4].v.n, vps[5].v.n);
					return sr_kemi_return_int(ket, ret);
				}
			} else {
				LM_ERR("invalid parameters for: %.*s\n", ket->fname.len, ket->fname.s);
				return sr_kemi_return_false(ket);
			}
		break;
		default:
			LM_ERR("invalid parameters for: %.*s\n", ket->fname.len, ket->fname.s);
			return sr_kemi_return_false(ket);
	}
}
