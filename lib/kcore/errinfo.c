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
 */

/*!
 * \file errinfo.c
 * \brief Kamailio Error info functions
 */


#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "errinfo.h"

/*! global error info */
err_info_t _oser_err_info;

/*! \brief Get global error state
 */
err_info_t* get_err_info(void) { return &_oser_err_info; }

/*! \brief Initialize global error state
 */
void init_err_info(void)
{
	memset(&_oser_err_info, 0, sizeof(err_info_t));
}

/*! \brief Set suggested error info message
 */
void set_err_info(int ec, int el, char *info)
{
	LM_DBG("ec: %d, el: %d, ei: '%s'\n", ec, el,
			(info)?info:"");
	_oser_err_info.eclass = ec;
	_oser_err_info.level = el;
	if(info)
	{
		_oser_err_info.info.s   = info;
		_oser_err_info.info.len = strlen(info);
	}
}

/*! \brief Set suggested error reply
 */
void set_err_reply(int rc, char *rr)
{
	_oser_err_info.rcode = rc;
	if(rr)
	{
		_oser_err_info.rreason.s   = rr;
		_oser_err_info.rreason.len = strlen(rr);
	}
}

