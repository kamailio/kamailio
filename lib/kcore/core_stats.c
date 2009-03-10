/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2006-01-23  first version (bogdan)
 *  2006-11-28  Added statistics for the number of bad URI's, methods, and 
 *              proxy requests (Jeffrey Magder - SOMA Networks)
 */

/*!
 * \file
 * \brief Kamailio Core statistics
 */


#include <string.h>

#include "statistics.h"


#ifdef STATISTICS

stat_var* rcv_reqs;				/*!< received requests        */
stat_var* rcv_rpls;				/*!< received replies         */
stat_var* fwd_reqs;				/*!< forwarded requests       */
stat_var* fwd_rpls;				/*!< forwarded replies        */
stat_var* drp_reqs;				/*!< dropped requests         */
stat_var* drp_rpls;				/*!< dropped replies          */
stat_var* err_reqs;				/*!< error requests           */
stat_var* err_rpls;				/*!< error replies            */
stat_var* bad_URIs;				/*!< number of bad URIs       */
stat_var* unsupported_methods;	/*!< unsupported methods      */
stat_var* bad_msg_hdr;			/*!< messages with bad header */


/*! exported core statistics */
stat_export_t core_stats[] = {
	{"rcv_requests" ,         0,  &rcv_reqs              },
	{"rcv_replies" ,          0,  &rcv_rpls              },
	{"fwd_requests" ,         0,  &fwd_reqs              },
	{"fwd_replies" ,          0,  &fwd_rpls              },
	{"drop_requests" ,        0,  &drp_reqs              },
	{"drop_replies" ,         0,  &drp_rpls              },
	{"err_requests" ,         0,  &err_reqs              },
	{"err_replies" ,          0,  &err_rpls              },
	{"bad_URIs_rcvd",         0,  &bad_URIs              },
	{"unsupported_methods",   0,  &unsupported_methods   },
	{"bad_msg_hdr",           0,  &bad_msg_hdr           },
	{0,0,0}
};

#endif
