/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "log.h"
#include <stdio.h>
#include <stdarg.h>




static int use_syslog = 0;
static int log_level = LOG_WARNING;




void init_log(char *_prgname, int _use_syslog) {
	use_syslog = _use_syslog;
	if (use_syslog) {
		openlog(_prgname, LOG_PID, LOG_DAEMON);
	}
}




void set_log_level(int level) {
	log_level = level;
}




void destroy_log(void) {
	if (use_syslog) closelog();
}




void log_stdout(char * format, va_list ap)
{
	vfprintf(stdout, format, ap);
	fflush(stdout);
}




void pdb_log(int priority, char * format, ...) {
	va_list ap;

	if (priority<=log_level) {
		va_start(ap, format);
		if (use_syslog) vsyslog(priority, format, ap);
		else log_stdout(format, ap);
		va_end(ap);
	}
}
