/*
 * Copyright (C) 2007 SOMA Networks, Inc.
 * Written by Ovidiu Sas (osas)
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

#ifndef _QOS_QOS_LOAD_H_
#define _QOS_QOS_LOAD_H_

#include "qos_cb.h"

struct qos_binds {
	register_qoscb_f  register_qoscb;
};


typedef int(*load_qos_f)( struct qos_binds *qosb );
int load_qos( struct qos_binds *qosb);


static inline int load_qos_api( struct qos_binds *qosb )
{
	load_qos_f load_qos;

	/* import the QOS auto-loading function */
	if ( !(load_qos=(load_qos_f)find_export("load_qos", 0, 0))) {
		LM_ERR("can't import load_qos\n");
		return -1;
	}
	/* let the auto-loading function load all QOS stuff */
	if (load_qos( qosb )==-1)
		return -1;

	return 0;
}


#endif
