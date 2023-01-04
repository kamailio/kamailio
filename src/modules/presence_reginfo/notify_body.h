/*
 * presence_dialoginfo module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2008 Klaus Darilion, IPCom
 * Copyright (C) 2022 Matteo Brancaleoni, VoiSmart
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

/*! \file
 * \brief Kamailio presence reginfo  ::
 * \ref notify_body.c
 * \ingroup presence_reginfo
 */


#ifndef _NBODY_H_
#define _NBODY_H_

str *reginfo_agg_nbody(str *pres_user, str *pres_domain, str **body_array,
		int n, int off_index);

str *reginfo_body_setversion(subs_t *subs, str *body);

void free_xml_body(char *body);

#endif
