/* 
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

/*!
 * \file
 * \brief Kamailio core :: kamailio compatible fixups
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
		if (*param) \
			fparam_free_restore(param); \
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
  * @param type2 - fix_param type for all the other params
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
			if ((ret=fixup_free_fpt_##suffix(&p, param_no))==0) *param=p; \
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
FIXUP_F1T(str_all, 1, 100,  FPARAM_STR)

/*
  no free fixups possible for unit_*
  (they overwrite the pointer with the converted number => the original
   value cannot be recovered)
FIXUP_F1T(uint_null, 1, 1, FPARAM_INT)
FIXUP_F1T(uint_uint, 1, 2, FPARAM_INT)
*/



int fixup_uint_uint(void** param, int param_no)
{
	str s;
	unsigned int num;
	
	s.s = *param;
	s.len = strlen(s.s);
	if (likely(str2int(&s, &num) == 0)) {
		*param = (void*)(long)num;
	} else
		/* not a number */
		return E_UNSPEC;
	return 0;
}



int fixup_uint_null(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_uint_uint(param, param_no);
	return E_UNSPEC;
}


/* fixup_regexp_null() has to be written "by hand", since
   it needs to save the original pointer (the fixup users expects
   a pointer to the regex in *param and hence the original value
   needed on free cannot be recovered directly).
FIXUP_F1T(regexp_null, 1, 1, FPARAM_REGEX)
*/

struct regex_fixup {
	regex_t regex; /* compiled regex */
	void* orig;    /* original pointer */
};

int fixup_regexp_null(void** param, int param_no)
{
	struct regex_fixup* re;
	
	if (param_no != 1)
		return E_UNSPEC;
	if ((re=pkg_malloc(sizeof(*re))) ==0) {
		ERR("No memory left\n");
		goto error;
	}
	if (regcomp(&re->regex, *param,
				REG_EXTENDED|REG_ICASE|REG_NEWLINE))
		goto error;
	re->orig = *param;
	*param = re;
	return 0;
error:
	if (re)
		pkg_free(re);
	return E_UNSPEC;
}


int fixup_free_regexp_null(void** param, int param_no)
{
	struct regex_fixup* re;
	
	if (param_no != 1)
		return E_UNSPEC;
	if (*param) {
		re = *param;
		*param = re->orig;
		regfree(&re->regex);
		pkg_free(re);
	}
	return 0;
}

/* fixup_pvar_*() has to be written "by hand", since
   it needs to save the original pointer (the fixup users expects
   a pointer to the pv_spec_t in *param and hence the original value
   needed on free cannot be recovered directly).
FIXUP_F1T(pvar_null, 1, 1, FPARAM_PVS)
FIXUP_F1T(pvar_pvar, 1, 2, FPARAM_PVS)
*/

struct pvs_fixup {
	pv_spec_t pvs; /* parsed pv spec */
	void* orig;    /* original pointer */
};

int fixup_pvar_all(void** param, int param_no)
{
	struct pvs_fixup* pvs_f;
	str name;
	
	pvs_f = 0;
	name.s = *param;
	name.len = strlen(name.s);
	trim(&name);
	if (name.len == 0 || name.s[0] != '$')
		/* not a pvs id */
		goto error;
	if ((pvs_f=pkg_malloc(sizeof(*pvs_f))) == 0) {
		ERR("No memory left\n");
		goto error;
	}
	if (pv_parse_spec2(&name, &pvs_f->pvs, 1) == 0)
		/* not a valid pvs identifier */
		goto error;
	pvs_f->orig = *param;
	*param = pvs_f;
	return 0;
error:
	if (pvs_f)
		pkg_free(pvs_f);
	return E_UNSPEC;
}



int fixup_free_pvar_all(void** param, int param_no)
{
	struct pvs_fixup* pvs_f;
	
	if (*param) {
		pvs_f = *param;
		*param = pvs_f->orig;
		/* free only the contents (don't attempt to free &pvs_f->pvs)*/
		pv_spec_destroy(&pvs_f->pvs);
		/* free the whole pvs_fixup */
		pkg_free(pvs_f);
	}
	return 0;
}



int fixup_pvar_pvar(void** param, int param_no)
{
	if (param_no > 2)
		return E_UNSPEC;
	return fixup_pvar_all(param, param_no);
}



int fixup_free_pvar_pvar(void** param, int param_no)
{
	if (param_no > 2)
		return E_UNSPEC;
	return fixup_free_pvar_all(param, param_no);
}



int fixup_pvar_null(void** param, int param_no)
{
	if (param_no != 1)
		return E_UNSPEC;
	return fixup_pvar_all(param, param_no);
}



int fixup_free_pvar_null(void** param, int param_no)
{
	if (param_no != 1)
		return E_UNSPEC;
	return fixup_free_pvar_all(param, param_no);
}

int fixup_pvar_none(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_pvar_all(param, param_no);
	return 0;
}



int fixup_free_pvar_none(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_free_pvar_all(param, param_no);
	return 0;
}


/* must be written "by hand", see above (fixup_pvar_pvar).
FIXUP_F2T(pvar_str, 1, 2, 1, FPARAM_PVS, FPARAM_STR)
FIXUP_F2T(pvar_str_str, 1, 3, 1, FPARAM_PVS, FPARAM_STR)
*/

int fixup_pvar_str(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_pvar_all(param, param_no);
	else if (param_no == 2)
		return fixup_str_str(param, param_no);
	return E_UNSPEC;
}



int fixup_free_pvar_str(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_free_pvar_all(param, param_no);
	else if (param_no == 2)
		return fixup_free_str_str(param, param_no);
	return E_UNSPEC;
}



int fixup_pvar_str_str(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_pvar_all(param, param_no);
	else if (param_no == 2 || param_no == 3)
		return fixup_str_all(param, param_no);
	return E_UNSPEC;
}



int fixup_free_pvar_str_str(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_free_pvar_all(param, param_no);
	else if (param_no == 2 || param_no == 3)
		return fixup_free_str_all(param, param_no);
	return E_UNSPEC;
}


int fixup_pvar_uint(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_pvar_all(param, param_no);
	else if (param_no == 2)
		return fixup_uint_uint(param, param_no);
	return E_UNSPEC;
}


int fixup_free_pvar_uint(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_free_pvar_all(param, param_no);
	return E_UNSPEC;
}


FIXUP_F2FP(igp_null, 1, 1, 1, FPARAM_INT|FPARAM_PVS, 0)
FIXUP_F2FP(igp_igp, 1, 2, 2,  FPARAM_INT|FPARAM_PVS, 0)

/* must be declared by hand, because of the pvar special handling
   (see above)
FIXUP_F2FP(igp_pvar, 1, 2, 1,  FPARAM_INT|FPARAM_PVS, FPARAM_PVS)
FIXUP_F2FP_T(igp_pvar_pvar, 1, 3, 1, FPARAM_INT|FPARAM_PVS, FPARAM_PVS)
*/

int fixup_igp_pvar(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_igp_null(param, param_no);
	else if (param_no == 2)
		return fixup_pvar_all(param, param_no);
	return E_UNSPEC;
}



int fixup_free_igp_pvar(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_free_igp_null(param, param_no);
	else if (param_no == 2)
		return fixup_free_pvar_all(param, param_no);
	return E_UNSPEC;
}



int fixup_igp_pvar_pvar(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_igp_null(param, param_no);
	else if (param_no == 2 || param_no == 3)
		return fixup_pvar_all(param, param_no);
	return E_UNSPEC;
}



int fixup_free_igp_pvar_pvar(void** param, int param_no)
{
	if (param_no == 1)
		return fixup_free_igp_null(param, param_no);
	else if (param_no == 2 || param_no == 3)
		return fixup_free_pvar_all(param, param_no);
	return E_UNSPEC;
}



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
		fparam_t* fp; \
		if (param_no<=(no1)){ \
			if ((ret=fix_param_types(FPARAM_PVE, param))<0){ \
				ERR("Cannot convert function parameter %d to spve \n", \
						param_no);\
				return E_UNSPEC; \
			} else{ \
				fp=(fparam_t*)*param; \
				if ((ret==0) && (fp->v.pve->spec==0 \
							|| fp->v.pve->spec->getf==0)){ \
					fparam_free_restore(param); \
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
			if (param_no<=(no1)) \
				fparam_free_restore(param); \
			else \
				return fixup_free_spvet_##suffix(param, param_no); \
		} \
		return 0; \
	}


/* format: name, minp, maxp, no_of_spve_params, type_for_rest_params */
FIXUP_F_SPVE_T(spve_spve, 1, 2, 2, 0)
FIXUP_F_SPVE_T(spve_uint, 1, 2, 1, FPARAM_INT)
FIXUP_F_SPVE_T(spve_str, 1, 2, 1, FPARAM_STR)
FIXUP_F_SPVE_T(spve_null, 1, 1, 1, 0)

/** get the corresp. fixup_free* function.
 * @param f -fixup function pointer.
 * @return  - pointer to free_fixup function if known, 0 otherwise.
 */
free_fixup_function mod_fix_get_fixup_free(fixup_function f)
{
	if (f == fixup_str_null) return fixup_free_str_null;
	if (f == fixup_str_str) return fixup_free_str_str;
	/* no free fixup for fixup_uint_* (they overwrite the pointer
	   value with a number and the original value cannot be recovered) */
	if (f == fixup_uint_null) return 0;
	if (f == fixup_uint_uint) return 0;
	if (f == fixup_regexp_null) return fixup_free_regexp_null;
	if (f == fixup_pvar_null) return fixup_free_pvar_null;
	if (f == fixup_pvar_pvar) return fixup_free_pvar_pvar;
	if (f == fixup_pvar_str) return fixup_free_pvar_str;
	if (f == fixup_pvar_str_str) return fixup_free_pvar_str_str;
	if (f == fixup_igp_igp) return fixup_free_igp_igp;
	if (f == fixup_igp_null) return fixup_free_igp_null;
	if (f == fixup_igp_pvar) return fixup_free_igp_pvar;
	if (f == fixup_igp_pvar_pvar) return fixup_free_igp_pvar_pvar;
	if (f == fixup_spve_spve) return fixup_free_spve_spve;
	if (f == fixup_spve_null) return fixup_free_spve_null;
	/* no free fixup, because of the uint part (the uint cannot be freed,
	   see above fixup_uint_null) */
	if (f == fixup_spve_uint) return 0;
	if (f == fixup_spve_str) return fixup_free_spve_str;
	return 0;
}

/**
 *
 */
int fixup_spve_all(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
}

/**
 *
 */
int fixup_igp_all(void** param, int param_no)
{
	return fixup_igp_null(param, 1);
}

/**
 *
 */
int fixup_spve_igp(void** param, int param_no)
{
	if(param_no==1)
		return fixup_spve_null(param, 1);
	if(param_no==2)
		return fixup_igp_null(param, 1);
	return E_UNSPEC;
}

/**
 *
 */
int fixup_free_spve_igp(void** param, int param_no)
{
	if(param_no==1)
		return fixup_free_spve_null(param, 1);
	if(param_no==2)
		return fixup_free_igp_null(param, 1);
	return E_UNSPEC;
}
