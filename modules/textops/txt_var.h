/*
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
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
 */

/*!
 * \file
 * \brief Regular expressions
 * \ingroup textops
 * Module: \ref textops
 */

		       
#ifndef _TXT_VAR_H_
#define _TXT_VAR_H_

#include "../../pvar.h"

enum _tr__txt_type { TR_TXT_NONE=0, TR_TXT_RE };
enum _tr_s_subtype { 
	TR_TXT_RE_NONE=0, TR_TXT_RE_SUBST };

char* tr_txt_parse_re(str *in, trans_t *tr);

#endif
