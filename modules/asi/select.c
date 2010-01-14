/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */

#include "select.h"
#include "ut.h"
//#include "modules/tm/t_cancel.h"

#include "tid.h"
#include "binds.h"

static int canceled_tid(str* res, select_t* s, struct sip_msg* msg)
{
	const static str EMPTY = STR_STATIC_INIT("");
	unsigned int index, label;

	if (msg->REQ_METHOD != METHOD_CANCEL) {
		WARN("invoked with non CANCEL method (%d).\n", msg->REQ_METHOD);
		*res = EMPTY;
		return 0;
	}
	if (tmb.t_get_canceled_ident(msg, &index, &label) < 0) {
		ERR("failed to lookup CANCEL's original transaction.\n");
		return -1;
	} else {
		*res = *tid2str(index, label);
	}
	return 0;
}

static int current_tid(str* res, select_t* s, struct sip_msg* msg)
{
	unsigned int index, label;

	if (tmb.t_get_trans_ident(msg, &index, &label) < 0) {
		ERR("failed to lookup transaction.\n");
		return -1;
	} else {
		*res = *tid2str(index, label);
	}
	return 0;
}

static int dummy(str* res, select_t* s, struct sip_msg* msg) {return 0;}

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("asi"), dummy, 
			SEL_PARAM_EXPECTED},
	{dummy, SEL_PARAM_STR, STR_STATIC_INIT("tid"), dummy, 
			SEL_PARAM_EXPECTED},
	{dummy, SEL_PARAM_STR, STR_STATIC_INIT("canceled"), canceled_tid, 0},
	{dummy, SEL_PARAM_STR, STR_STATIC_INIT("current"), current_tid, 0},

	/* marks end of table */
	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};
