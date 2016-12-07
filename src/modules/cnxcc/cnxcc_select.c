/*
 * $Id$
 *
 * Copyright (C) 2012 Carlos Ruiz Díaz (caruizdiaz.com),
 *                    ConexionGroup (www.conexiongroup.com)
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
 */

#include "../../select.h"
#include "../../select_buf.h"

#include "cnxcc_mod.h"

extern data_t _data;

int sel_root(str* res, select_t* s, struct sip_msg* msg)  /* dummy */ {
	return 0;
}

int sel_channels(str* res, select_t* s, struct sip_msg* msg) {
	LM_DBG("sel_channels\n");

	return 0;
}

int sel_channels_count(str* res, select_t* s, struct sip_msg* msg) {
	LM_DBG("sel_channels_count for [%.*s]\n",  s->params[2].v.s.len, s->params[2].v.s.s);

	credit_data_t *credit_data	= NULL;
	int value					= 0;

	if (s->params[2].v.s.len <= 0)
	{
		LM_ERR("Client must be specified\n");
		return -1;
	}

	if (try_get_credit_data_entry(&s->params[2].v.s, &credit_data) >= 0)
		value = credit_data->number_of_calls;
	else
		LM_DBG("Client [%.*s] not found\n", s->params[2].v.s.len, s->params[2].v.s.s);

	res->s 	= int2str(value, &res->len);

	return 0;
}


