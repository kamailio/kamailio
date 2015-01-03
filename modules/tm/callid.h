/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ----------
 * 2003-04-09 Created by janakj
 */

/*!
 * \file 
 * \brief TM :: Fast Call-ID generator
 * \ingroup tm
 */


#ifndef CALLID_H
#define CALLID_H

#include "../../str.h"


/**
 * \brief Initialize the Call-ID generator, generates random prefix
 * \return 0 on success, -1 on error
 */
int init_callid(void);


/**
 * \brief Child initialization, generates suffix
 * \param rank not used
 * \return 0 on success, -1 on error
 */
int child_init_callid(int rank);


/**
 * \brief TM API export
 */
typedef void (*generate_callid_f)(str*);


/**
 * \brief Get a unique Call-ID
 * \param callid returned Call-ID
 */
void generate_callid(str* callid);


#endif /* CALLID_H */
