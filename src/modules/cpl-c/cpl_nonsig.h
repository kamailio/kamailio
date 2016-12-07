/*
 * $Id$
 *
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
 * -------
 * 2003-06-27: file created (bogdan)
 */

#ifndef _CPL_NONSIG_H_
#define _CPL_NONSIG_H_

#include <unistd.h>

#include "../../str.h"
#include "cpl_env.h"

struct cpl_cmd {
	unsigned int code;
	str s1;
	str s2;
	str s3;
};


#define CPL_LOG_CMD    1
#define CPL_MAIL_CMD   2

#define MAX_LOG_DIR_SIZE    256


void cpl_aux_process( int cmd_out, char *log_dir);


static inline void write_cpl_cmd(unsigned int code, str *s1, str *s2, str *s3)
{
	static struct cpl_cmd cmd;

	cmd.code = code;
	cmd.s1 = *s1;
	cmd.s2 = *s2;
	cmd.s3 = *s3;

	if (write( cpl_env.cmd_pipe[1], &cmd, sizeof(struct cpl_cmd) )==-1)
		LM_ERR("write ret: %s\n",strerror(errno));
}



#endif
