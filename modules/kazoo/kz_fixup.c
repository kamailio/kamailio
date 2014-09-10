/*
 * kz_fixup.c
 *
 *  Created on: Aug 2, 2014
 *      Author: root
 */


#include "../../mod_fix.h"
#include "../../lvalue.h"

#include "kz_fixup.h"

int fixup_kz_json(void** param, int param_no)
{
  if (param_no == 1 || param_no == 2) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 3) {
		if (fixup_pvar_null(param, 1) != 0) {
		    LM_ERR("failed to fixup result pvar\n");
		    return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
		    LM_ERR("result pvar is not writeble\n");
		    return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

int fixup_kz_json_free(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}


int fixup_kz_amqp_encode(void** param, int param_no)
{
  if (param_no == 1 ) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 2) {
		if (fixup_pvar_null(param, 1) != 0) {
		    LM_ERR("failed to fixup result pvar\n");
		    return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
		    LM_ERR("result pvar is not writeble\n");
		    return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

int fixup_kz_amqp_encode_free(void** param, int param_no)
{
	if (param_no == 1 ) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 2) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}



int fixup_kz_amqp(void** param, int param_no)
{
  if (param_no == 1 || param_no == 2 || param_no == 3) {
		return fixup_spve_null(param, 1);
	}

	if (param_no == 4) {
		if (fixup_pvar_null(param, 1) != 0) {
		    LM_ERR("failed to fixup result pvar\n");
		    return -1;
		}
		if (((pv_spec_t *)(*param))->setf == NULL) {
		    LM_ERR("result pvar is not writeble\n");
		    return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

int fixup_kz_amqp_free(void** param, int param_no)
{
	if (param_no == 1 || param_no == 2 || param_no == 3) {
		return fixup_free_spve_null(param, 1);
	}

	if (param_no == 4) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}


int fixup_kz_amqp4(void** param, int param_no)
{
	return fixup_spve_str(param, 1);
}

int fixup_kz_amqp4_free(void** param, int param_no)
{
	return fixup_free_spve_str(param, 1);
}



