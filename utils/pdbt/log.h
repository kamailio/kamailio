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

#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>




void init_log(char *_prgname, int _use_syslog);

void set_log_level(int level);

void destroy_log(void);

void pdb_log(int priority, char * format, ...);

#define LEMERG(fmt, args...) pdb_log(LOG_EMERG, fmt, ## args)
#define LALERT(fmt, args...) pdb_log(LOG_ALERT, fmt, ## args)
#define LCRIT(fmt, args...) pdb_log(LOG_CRIT, fmt, ## args)
#define LERR(fmt, args...) pdb_log(LOG_ERR, fmt, ## args)
#define LWARNING(fmt, args...) pdb_log(LOG_WARNING, fmt, ## args)
#define LNOTICE(fmt, args...) pdb_log(LOG_NOTICE, fmt, ## args)
#define LINFO(fmt, args...) pdb_log(LOG_INFO, fmt, ## args)
#define LDEBUG(fmt, args...) pdb_log(LOG_DEBUG, fmt, ## args)

/* several shell exit codes for the application pdbt */

#define PDB_OK 0              /* Everything ok */
#define PDB_USE_ERROR 1       /* Wrong usage of application (unknown command, file not found, etc.) */
#define PDB_NOT_IN_PDB 2      /* A queried number is not in the pdb */
#define PDB_TIMEOUT 3         /* A timeout (server not responding) occurred */
#define PDB_OTHER 4           /* Another application error occurred */

#endif
