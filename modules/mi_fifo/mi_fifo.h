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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * History:
 * ---------
 *  2006-09-25  first version (bogdan)
 */

/*!
 * \file
 * \brief MI Fifo :: Core
 * \ingroup mi
 */

#ifndef _MI_FIFO_H_
#define _MI_FIFO_H_

#define DEFAULT_MI_REPLY_DIR "/tmp/"

#define DEFAULT_MI_REPLY_IDENT "\t"

#define MI_CMD_SEPARATOR       ':'

/* the 2-chars separator between name and value */
#define MI_ATTR_VAL_SEP1 ':'
#define MI_ATTR_VAL_SEP2 ':'

/* maximum length of a FIFO line */
#define MAX_MI_FIFO_BUFFER    1024

/* how patient is ser with FIFO clients not awaiting a reply? 
	4 x 80ms = 0.32 sec */
#define FIFO_REPLY_RETRIES  4

/* maximum size for the composed fifo reply name */
#define MAX_MI_FILENAME 128

/* size of buffer used by parser to read and build the MI tree */
#define MAX_MI_FIFO_READ 8192

#endif /* _MI_FIFO */
