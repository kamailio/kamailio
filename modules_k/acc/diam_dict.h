/*
 * $Id$
 *
 * Copyright (C) 2002-2003 FhG Fokus
 *
 * This file is part of disc, a free diameter server/client.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 */

#ifdef DIAM_ACC

#ifndef DIAM_ACC_H
#define DIAM_ACC_H

#define SERVICE_LEN  1
#define SIP_ACCOUNTING	"9"

#define vendorID	0

/* Accounting AVPs */
enum{
	/*Accounting*/
	AVP_SIP_CALLID				= 550,	/* string */
	AVP_SIP_METHOD				= 551,	/* string */
	AVP_SIP_STATUS				= 552,	/* string */
	AVP_SIP_FROM_TAG			= 553,	/* string */
	AVP_SIP_TO_TAG				= 554,	/* string */
	AVP_SIP_CODE				= 564   /* string */
};

#endif


#endif

