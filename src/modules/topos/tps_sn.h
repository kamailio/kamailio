/**
 * Copyright (C) 2026 Stefan-Cristian Mititelu (net2phone.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
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
 * \brief Kamailio topos ::
 * \ingroup topos
 * Module: \ref topos
 */

#ifndef _TOPOS_SN_H_
#define _TOPOS_SN_H_

#include "tps_storage.h"
#include "../../core/parser/parse_rr.h"

void tps_refresh_scontacts_from_sn(
		tps_data_t *mtsd, tps_data_t *stsd, rr_t *srr);
void tps_refresh_srr_from_sn(tps_data_t *mtsd, rr_t *srr);

#endif
