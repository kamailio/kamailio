/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 *
 * Copyright (C) 2003 August.Net Services, LLC
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */
/*
** ________________________________________________________________________
**
**
**                      $RCSfile$
**                     $Revision$
**
**             Last change $Date$
**           Last change $Author$
**                        $State$
**                       $Locker$
**
**               Original author: Andrew Fullford
**
**           Copyright (C) August Associates  1995
**
** ________________________________________________________________________
*/

#include "aug_std.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static char *aug_module_name = 0;

/*
**  May eventually add ``on exit'' function calls.
*/
augExport void aug_exit(int exit_code)
{
	DABNAME("aug_exit");

	DABTRACE("Exiting with code %d", exit_code);
	exit(exit_code);
}

augExport void aug_abort_va(char *file, int line, char *fmt, va_list ap)
{
	DABNAME("aug_abort");

	DAB("ABORT from +%d %s", line, file);

	fflush(stdout);
	fprintf(stderr, "\r\n\n");
	if(aug_module_name)
		fprintf(stderr, "%s: ", aug_module_name);
	fprintf(stderr, "ABORT: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, " -- from +%d %s\r\n\n", line, file);

	if(DABLEVEL(DAB_TRACE))
	{
		fprintf(stderr, "Program terminating via abort()\r\n\n");
		abort();
	}

	aug_exit(augEXIT_ABORT);
}

augExport void aug_abort(char *file, int line, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	aug_abort_va(file, line, fmt, ap);
	va_end(ap);
}

/*
**  WARNING -- don't use DABs below here, aug_debug.c calls this code.
*/
augExport void aug_setmodule(char *name)
{
	char *prog;
	int len;

	if(!name)
		return;

	if((prog = strrchr(name, '/')) ||
	   (prog = strrchr(name, '\\')))
		prog++;
	else
		prog = name;

	aug_module_name = malloc(strlen(prog) + 1);
	strcpy(aug_module_name, prog);
}

augExport char *aug_module(void)
{
	return (aug_module_name ? aug_module_name : "");
}
