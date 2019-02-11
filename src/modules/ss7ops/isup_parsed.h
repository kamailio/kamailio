/*
 * ss7 module - ISUP parsed
 *
 * Copyright (C) 2016 Holger Hans Peter Freyther (help@moiji-mobile.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#pragma once

#include <stdint.h>
#include <sys/types.h>

#include "../../lib/srutils/srjson.h"

/**
 * State structure
 */
struct isup_state {
	srjson_doc_t*	json;
};

enum {
	ISUP_JSON_STRING,
	ISUP_FIELD_METHOD,
	ISUP_FIELD_OPC,
	ISUP_FIELD_DPC,
	ISUP_FIELD_CIC,
	ISUP_FIELD_CALLED_INN,
	ISUP_FIELD_CALLED_TON,
	ISUP_FIELD_CALLED_NPI,
	ISUP_FIELD_CALLED_NUM,
	ISUP_FIELD_CALLING_NI,
	ISUP_FIELD_CALLING_RESTRICT,
	ISUP_FIELD_CALLING_SCREENED,
	ISUP_FIELD_CALLING_TON,
	ISUP_FIELD_CALLING_NPI,
	ISUP_FIELD_CALLING_NUM,
	ISUP_FIELD_CALLING_CAT,
	ISUP_FIELD_CAUSE_STD,
	ISUP_FIELD_CAUSE_LOC,
	ISUP_FIELD_CAUSE_ITU_CLASS,
	ISUP_FIELD_CAUSE_ITU_NUM,
	ISUP_FIELD_EVENT_NUM,
	ISUP_FIELD_HOP_COUNTER,
	ISUP_FIELD_NOC_SAT,
	ISUP_FIELD_NOC_CHECK,
	ISUP_FIELD_NOC_ECHO,
	ISUP_FIELD_FWD_INTE,
	ISUP_FIELD_FWD_INTW,
	ISUP_FIELD_FWD_EE_METH,
	ISUP_FIELD_FWD_EE_INF,
	ISUP_FIELD_FWD_ISUP_NUM,
	ISUP_FIELD_FWD_ISUP_PREF,
	ISUP_FIELD_FWD_SCCP_METHOD,
	ISUP_FIELD_FWD_ISDN,
	ISUP_FIELD_MEDIUM,
	ISUP_FILED_UI_CODING,
	ISUP_FIELD_UI_TRANS_CAP,
	ISUP_FIELD_UI_TRANS_MODE,
	ISUP_FIELD_UI_TRANS_RATE,
	ISUP_FIELD_UI_LAYER1_IDENT,
	ISUP_FIELD_UI_LAYER1_PROTOCOL,
};

int isup_parse(const uint8_t *data, size_t len, struct isup_state *ptrs);
