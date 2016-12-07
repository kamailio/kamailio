/**
 * $Id$
 *
 * Copyright (C) 2006-2007 VozTelecom Sistemas S.L
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

#include "../../error.h"


/** Kamailio ERRORS ARE NEGATIVE, SEAS ERROR CODES ARE POSITIVE */

#define SE_CANCEL_MSG "500 SEAS cancel error"
#define SE_CANCEL_MSG_LEN (sizeof(SE_CANCEL_MSG)-1)
#define SE_CANCEL 1

#define SE_UAC_MSG "500 SEAS uac error"
#define SE_UAC_MSG_LEN (sizeof(SE_UAC_MSG)-1)
#define SE_UAC 2
