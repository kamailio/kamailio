/**
 * Callback functions for RTR/PPR from the HSS
 *
 * Copyright (c) 2012 Carsten Bock, ng-voice GmbH
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
 */

#include "cxdx_callbacks.h"
#include "../../str.h"
#include "../../dprint.h"

/*int PPR_RTR_Event(void *parsed_message, int type, void *param) {
	if (type & CXDX_PPR_RECEIVED) {
		LM_ERR("Received a PPR-Request\n");
		return 1;
	}
	if (type & CXDX_RTR_RECEIVED) {
		LM_ERR("Received a RTR-Request\n");
		return 1;
	}
	return 0;
}*/
