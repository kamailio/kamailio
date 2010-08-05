/* 
 * $Id$
 * 
 * Copyright (C) 2008 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @file mod_fix.c
 * @brief kamailio compatible fixups
 */
/* 
 * History:
 * --------
 *  2008-11-25  initial version (andrei)
 */

/*!
 * \file
 * \brief SIP-router core :: 
 * \ingroup core
 * Module: \ref core
 */

#include "mod_fix.h"
#include "mem/mem.h"
#include "trim.h"



#if 0
/* TODO: */
int fixup_regexpNL_null(void** param, int param_no); /* not used */
int fixup_regexpNL_none(void** param, int param_no); /* textops */
#endif



#define FREE_FIXUP_FP(suffix, minp, maxp) \
	int fixup_free_##suffix(void** param, int param_no) \
	{ \
		if ((param_no > (maxp)) || (param_no < (minp))) \
			return E_UNSPEC; \
		if (*param){ \
			fparam_free_contents((fparam_t*)*param); \
			pkg_free(*param); \
			*param=0; \
		} \
		return 0; \
	}


/** macro for declaring a fixup and the corresponding free_fixup
  * for a function which fixes to fparam_t and expects 2 different types.
  *
  * The result (in *param) will be a fparam_t.
  *
  * @param suffix - function suffix (fixup_ will be pre-pended to it 
  * @param minp - minimum parameter number acceptable
  * @param maxp - maximum parameter number
  * @param no1 -  number of parameters of type1
  * @param type1 - fix_param type for the 1st param
  * @paran type2 - fix_param type for all the other params
  */
#define FIXUP_F2FP(suffix, minp, maxp, no1, type1, type2) \
	int fixup_##suffix (void** param, int param_no) \
	{ \
		if ((param_no > (maxp)) || (param_no <(minp))) \
			return E_UNSPEC; \
		if (param_no <= (no1)){ \
			if (fix_param_types((type1), param)!=0) {\
				ERR("Cannot convert function parameter %d to" #type1 "\n", \
						param_no);\
				return E_UNSPEC; \
			} \
		}else{ \
			if (fix_param_types((type2), param)!=0) {\
				ERR("Cannot convert function parameter %d to" #type2 "\n", \
						param_no); \
				return E_UNSPEC; \
			} \
		}\
		return 0; \
	} \
	FREE_FIXUP_FP(suffix, minp, maxp)


/** macro for declaring a fixup and the corresponding free_fixup
  * for a function which fixes directly to the requested type.
  *
  * @see FIXUP_F2FP for the parameters
  * Side effect: declares also some _fp_helper functions
  */
#define FIXUP_F2T(suffix, minp, maxp, no1, type1, type2) \
	FIXUP_F2FP(fp_##suffix, minp, maxp, no1, type1, type2) \
	int fixup_##suffix (void** param, int param_no) \
	{ \
		int ret; \
		if ((ret=fixup_fp_##suffix (param, param_no))!=0) \
			return ret; \
		*param=((fparam_t*)*param)->fixed; \
		return 0; \
	} \
	int fixup_free_##suffix (void** param, int param_no) \
	{ \
		void* p; \
		int ret; \
		if (param && *param){ \
			p=*param - (long)&((fparam_t*)0)->v; \
			if ((ret=fixup_free_fp_##suffix(&p, param_no))==0) *param=p; \
			return ret; \
		} \
		return 0; \
	}


/** macro for declaring a fixup and the corresponding free_fixup
  * for a function expecting first no1 params as fparamt_t and the
  * rest as direct type.
  *
  * @see FIXUP_F2FP for the parameters with the exception
  * that only the first no1 parameters are converted to 
  * fparamt_t and the rest directly to the correponding type
  *
  * Side effect: declares also some _fpt_helper functions
  */
#define FIXUP_F2FP_T(suffix, minp, maxp, no1, type1, type2) \
	FIXUP_F2FP(fpt_##suffix, minp, maxp, no1, type1, type2) \
	int fixup_##suffix (void** param, int param_no) \
	{ \
		int ret; \
		if ((ret=fixup_fpt_##suffix(param, param_no))!=0) \
			return ret; \
		if (param_no>(no1)) *param=&((fparam_t*)*param)->v; \
		return 0; \
	} \
	int fixup_free_##suffix (void** param, int param_no) \
	{ \
		void* p; \
		int ret; \
		if (param && *param){ \
			p=(param_no>(no1))? *param - (long)&((fparam_t*)0)->v : *param;\
			if ((ret=fixup_free_fpt_##suffix(&p, param_no))==0) *param=0; \
			return ret; \
		} \
		return 0; \
	}


/** macro for declaring a fixup which fixes all the paremeters to the same
  * type.
  *
  * @see FIXUP_F2T.
  */
#define FIXUP_F1T(suffix, minp, maxp, type) \
	FIXUP_F2T(suffix, minp, maxp, maxp, type, 0)



FIXUP_F1T(str_null, 1, 1, FPARAM_STR)
FIXUP_F1T(str_str, 1, 2,  FPARAM_STR)

/* TODO: int can be converted in place, no need for pkg_malloc'ed fparam_t*/
FIXUP_F1T(uint_null, 1, 1, FPARAM_INT)
FIXUP_F1T(uint_uint, 1, 2, FPARAM_INT)

FIXUP_F1T(regexp_null, 1, 1, FPARAM_REGEX)

FIXUP_F1T(pvar_null, 1, 1, FPARAM_PVS)
FIXUP_F1T(pvar_pvar, 1, 2, FPARAM_PVS)

FIXUP_F2T(pvar_str, 1, 2, 1, FPARAM_PVS, FPARAM_STR)
FIXUP_F2T(pvar_str_str, 1, 3, 1, FPARAM_PVS, FPARAM_STR)

FIXUP_F2FP(igp_null, 1, 1, 1, FPARAM_INT|FPARAM_PVS, 0)
FIXUP_F2FP(igp_igp, 1, 2, 2,  FPARAM_INT|FPARAM_PVS, 0)
FIXUP_F2FP(igp_pvar, 1, 2, 1,  FPARAM_INT|FPARAM_PVS, FPARAM_PVS)

FIXUP_F2FP_T(igp_pvar_pvar, 1, 3, 1, FPARAM_INT|FPARAM_PVS, FPARAM_PVS)

/** macro for declaring a spve fixup and the corresponding free_fixup
  * for a function expecting first no1 params as fparam converted spve 
  * and the * rest as direct type.
  *
  * @see FIXUP_F2FP for the parameters with the exception
  * that the first no1 parameters are converted to fparam_t from spve
  * and the rest directly to the corresponding type
  *
  * Side effect: declares also some _spvet_helper functions
  */
#define FIXUP_F_SPVE_T(suffix, minp, maxp, no1, type2) \
	FIXUP_F1T(spvet_##suffix, minp, maxp, type2) \
	int fixup_##suffix (void** param, int param_no) \
	{ \
		int ret; \
		char * bkp; \
		fparam_t* fp; \
		if (param_no<=(no1)){ \
			if ((ret=fix_param_types(FPARAM_PVE, param))<0){ \
				ERR("Cannot convert function parameter %d to" #type2 "\n", \
						param_no);\
				return E_UNSPEC; \
			} else{ \
				fp=(fparam_t*)*param; \
				if ((ret==0) && (fp->v.pve->spec.getf==0)){ \
					bkp=fp->orig; \
					fp->orig=0; /* make sure orig string is not freed */ \
					fparam_free_contents(fp); \
					pkg_free(fp); \
					*param=bkp; \
					return fix_param_types(FPARAM_STR, param); \
				} else if (ret==1) \
					return fix_param_types(FPARAM_STR, param); \
				return ret; \
			} \
		} else return fixup_spvet_##suffix(param, param_no); \
		return 0; \
	} \
	int fixup_free_##suffix (void** param, int param_no) \
	{ \
		if (param && *param){ \
			if (param_no<=(no1)){ \
				fparam_free_contents((fparam_t*)*param); \
				pkg_free(*param); \
				*param=0; \
			} else \
				return fixup_free_spvet_##suffix(param, param_no); \
		} \
		return 0; \
	}


/* format: name, minp, maxp, no_of_spve_params, type_for_rest_params */
FIXUP_F_SPVE_T(spve_spve, 1, 2, 2, 0)
FIXUP_F_SPVE_T(spve_uint, 1, 2, 1, FPARAM_INT)
FIXUP_F_SPVE_T(spve_str, 1, 2, 1, FPARAM_STR)
FIXUP_F_SPVE_T(spve_null, 1, 1, 1, 0)
