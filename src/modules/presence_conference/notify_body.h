/*
 *
 * presence_conference module - notify_body header file mariusbucur
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2008 Klaus Darilion, IPCom
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
 * History:
 * --------
 * 2010-07-12  initial version (mariusbucur)
 */
/*! \file
 * \brief Kamailio Presence_Conference :: Notify body handling
 * \ingroup presence_conference
 */

#ifndef _CONF_NBODY_H_
#define _CONF_NBODY_H_

str* conf_agg_nbody(str* pres_user, str* pres_domain, str** body_array,
		int n, int off_index);
str* conf_body_setversion(subs_t *subs, str* body);
void free_xml_body(char* body);

#endif
