/*
 * $Id$
 *
 * Kazoo module interface
 *
 * Copyright (C) 2010-2014 2600Hz
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2014-08  first version (2600hz)
 */

#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"

#include "kz_fixup.h"

int fixup_kz_json(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2) {
		return fixup_spve_null(param, 1);
	}

	if(param_no == 3) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

int fixup_kz_json_free(void **param, int param_no)
{
	if(param_no == 1 || param_no == 2) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 3) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}


int fixup_kz_amqp_encode(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_spve_null(param, 1);
	}

	if(param_no == 2) {
		if(fixup_pvar_null(param, 1) != 0) {
			LM_ERR("failed to fixup result pvar\n");
			return -1;
		}
		if(((pv_spec_t *)(*param))->setf == NULL) {
			LM_ERR("result pvar is not writeble\n");
			return -1;
		}
		return 0;
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}

int fixup_kz_amqp_encode_free(void **param, int param_no)
{
	if(param_no == 1) {
		return fixup_free_spve_null(param, 1);
	}

	if(param_no == 2) {
		return fixup_free_pvar_null(param, 1);
	}

	LM_ERR("invalid parameter number <%d>\n", param_no);
	return -1;
}


int fixup_kz_amqp(void **param, int param_no)
{
	return fixup_spve_null(param, 1);
}

int fixup_kz_amqp_free(void **param, int param_no)
{
	return fixup_free_spve_null(param, 1);
}


int fixup_kz_amqp4(void **param, int param_no)
{
	return fixup_spve_str(param, 1);
}

int fixup_kz_amqp4_free(void **param, int param_no)
{
	return fixup_free_spve_str(param, 1);
}


int fixup_kz_async_amqp(void **param, int param_no)
{
	return fixup_spve_null(param, 1);
}

int fixup_kz_async_amqp_free(void **param, int param_no)
{
	return fixup_free_spve_null(param, 1);
}
