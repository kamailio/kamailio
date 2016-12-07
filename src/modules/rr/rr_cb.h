/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*!
 * \file
 * \brief Route & Record-Route module
 * \ingroup rr
 */

#ifndef RR_CB_H_
#define RR_CB_H_

#include "../../str.h"
#include "../../parser/msg_parser.h"


/*! \brief callback function prototype */
typedef void (rr_cb_t) (struct sip_msg* req, str *rr_param, void *param);
/*! \brief register callback function prototype */
typedef int (*register_rrcb_t)( rr_cb_t f, void *param);


/*! rr callback */
struct rr_callback {
	int id;				/*!< id of this callback - useless */
	rr_cb_t* callback;		/*!< callback function */
	void *param;			/*!< param to be passed to callback function */
	struct rr_callback* next; /*!< next callback element*/
};


/*!
 * \brief destroy global callback list, frees memory
 */
void destroy_rrcb_lists(void);


/*!
 * \brief register a RR callback, allocates new private memory for it
 * \param f callback register function
 * \param param callback parameter
 * \return 0 on success, -1 on failure (out of memory)
 */
int register_rrcb(rr_cb_t f, void *param );


/*!
 * \brief run RR transaction callbacks
 * \param req SIP request
 * \param rr_param callback list
 */
void run_rr_callbacks( struct sip_msg *req, str *rr_param);


#endif
