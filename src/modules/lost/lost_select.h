/*
 * lost module - select interface
 *
 * Copyright (C) 2021 Eugene Sukhanov
 * NGA911
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

/*!
 * \file
 * \brief Kamailio lost :: Select interface.
 * \ingroup lost
 * Module: \ref lost
 */

#ifndef _LOST_SELECT_H
#define _LOST_SELECT_H

#include "../../core/select.h"

extern select_row_t lost_sel[];

#endif /* _LOST_SELECT_H */
