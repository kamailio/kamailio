/*
 * lost module functions
 *
 * Copyright (C) 2019 Wolfgang Kampichler
 * DEC112, FREQUENTIS AG
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
 * \brief Kamailio lost :: functions
 * \ingroup lost
 * Module: \ref lost
 */

#ifndef LOST_FUNCTIONS_H
#define LOST_FUNCTIONS_H

char *lost_held_type(char *, int *, int *);

int lost_held_function(
		struct sip_msg *, char *, char *, char *, char *, char *);

int lost_function(
		struct sip_msg *, char *, char *, char *, char *, char *, char *);

#endif
