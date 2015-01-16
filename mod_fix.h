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

/**
 * @file mod_fix.h
 * @brief Kamailio core :: Generic fixup functions for module function parameter.
 * (kamailio compatibility)
 * @ingroup core
 * Module: \ref core
 */

#ifndef _mod_fix_h_
#define _mod_fix_h_

#include "sr_module.h"
#include "pvar.h"

#define GPARAM_TYPE_INT		FPARAM_INT
#define GPARAM_TYPE_STR		FPARAM_STR
#define GPARAM_TYPE_PVS		FPARAM_PVS
#define GPARAM_TYPE_PVE		FPARAM_PVE

/**
 * generic parameter that holds a string, an int, a pseudo-variable
 * or a ser select, avp, or subst.
 * 
 * Note: used only for compatibility with existing kamailio code,
 *  please use fparam_t directly in the future.
 *
 * @see fparam_t
 */

/* reuse ser fparam_t */
#define gparam_t fparam_t

typedef gparam_t *gparam_p;

int fixup_get_svalue(struct sip_msg* msg, gparam_p gp, str *val);

/** get a string value out of a fparam_t.
  *
  * Note: this macro/function is  for kamailio compatibility
  * (please use get_str_fparam() directly in the future)
  *
  * @param msg  - pointer to the sip message
  * @param fp   - pointer to the fparam_t
  * @param sval - pointer to str, used to store the result
  * @return  0 on success, -1 on error
  */
#define fixup_get_svalue(msg, fp, sval) get_str_fparam(sval, msg, fp)

/** get an int value out of a fparam_t.
  *
  * Note: this macro/function is  for kamailio compatibility
  * (please use get_int_fparam() directly in the future)
  *
  * @param msg  - pointer to the sip message
  * @param fp   - pointer to the fparam_t
  * @param ival - pointer to str, used to store the result
  * @return  0 on success, -1 on error
  */
#define fixup_get_ivalue(msg, fp, ival) get_int_fparam(ival, msg, fp)

int fixup_str_null(void** param, int param_no);
int fixup_str_str(void** param, int param_no);

int fixup_free_str_null(void** param, int param_no);
int fixup_free_str_str(void** param, int param_no);

int fixup_uint_null(void** param, int param_no);
int fixup_uint_uint(void** param, int param_no);


int fixup_regexp_null(void** param, int param_no);
int fixup_free_regexp_null(void** param, int param_no);
#if 0
int fixup_regexp_none(void** param, int param_no);
int fixup_free_regexp_none(void** param, int param_no);
/* not implemened yet */
int fixup_regexpNL_null(void** param, int param_no);
int fixup_regexpNL_none(void** param, int param_no);
#endif

int fixup_pvar_null(void **param, int param_no);
int fixup_free_pvar_null(void** param, int param_no);

int fixup_pvar_none(void** param, int param_no);
int fixup_free_pvar_none(void** param, int param_no);

int fixup_pvar_pvar(void **param, int param_no);
int fixup_free_pvar_pvar(void** param, int param_no);

int fixup_pvar_str(void** param, int param_no);
int fixup_free_pvar_str(void** param, int param_no);

int fixup_pvar_str_str(void** param, int param_no);
int fixup_free_pvar_str_str(void** param, int param_no);

int fixup_pvar_uint(void** param, int param_no);
int fixup_free_pvar_uint(void** param, int param_no);

int fixup_igp_igp(void** param, int param_no);
int fixup_free_igp_igp(void** param, int param_no);
int fixup_igp_null(void** param, int param_no);
int fixup_free_igp_null(void** param, int param_no);
int fixup_get_ivalue(struct sip_msg* msg, gparam_p gp, int *val);

int fixup_igp_pvar(void** param, int param_no);
int fixup_free_igp_pvar(void** param, int param_no);

int fixup_igp_pvar_pvar(void** param, int param_no);
int fixup_free_igp_pvar_pvar(void** param, int param_no);

int fixup_spve_spve(void** param, int param_no);
int fixup_free_spve_spve(void** param, int param_no);
int fixup_spve_null(void** param, int param_no);
int fixup_free_spve_null(void** param, int param_no);
int fixup_spve_uint(void** param, int param_no);
int fixup_spve_str(void** param, int param_no);
int fixup_free_spve_str(void** param, int param_no);

int fixup_spve_all(void** param, int param_no);
int fixup_igp_all(void** param, int param_no);

int fixup_spve_igp(void** param, int param_no);
int fixup_free_spve_igp(void** param, int param_no);

/** get the corresp. free fixup function.*/
free_fixup_function mod_fix_get_fixup_free(fixup_function f);

#endif
