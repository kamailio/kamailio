/*
 * $Id$
 *
 * debug print 
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */
/*!
 * \file
 * \brief SIP-router core :: 
 * \ingroup core
 * Module: \ref core
 */


 
#include "globals.h"
#include "dprint.h"
 
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#ifndef NO_SIG_DEBUG
/* signal protection: !=0 when LOG/DBG/... are printing */
volatile int dprint_crit = 0; 
#endif

static char* str_fac[]={"LOG_AUTH","LOG_CRON","LOG_DAEMON",
			"LOG_KERN","LOG_LOCAL0","LOG_LOCAL1",
			"LOG_LOCAL2","LOG_LOCAL3","LOG_LOCAL4","LOG_LOCAL5",
			"LOG_LOCAL6","LOG_LOCAL7","LOG_LPR","LOG_MAIL",
			"LOG_NEWS","LOG_USER","LOG_UUCP",
#ifndef __OS_solaris
			"LOG_AUTHPRIV","LOG_FTP","LOG_SYSLOG",
#endif
			0};
			
static int int_fac[]={LOG_AUTH ,  LOG_CRON , LOG_DAEMON ,
		      LOG_KERN , LOG_LOCAL0 , LOG_LOCAL1 ,
		      LOG_LOCAL2 , LOG_LOCAL3 , LOG_LOCAL4 , LOG_LOCAL5 ,
		      LOG_LOCAL6 , LOG_LOCAL7 , LOG_LPR , LOG_MAIL ,
		      LOG_NEWS , LOG_USER , LOG_UUCP,
#ifndef __OS_solaris
		      LOG_AUTHPRIV,LOG_FTP,LOG_SYSLOG,
#endif
		      0};

struct log_level_info log_level_info[] = {
	{"ALERT", LOG_ALERT},	  /* L_ALERT */
	{"BUG", LOG_CRIT},        /* L_BUG */
	{"CRITICAL", LOG_CRIT},   /* L_CRIT2 */
	{"",    LOG_CRIT},         /* L_CRIT */
	{"ERROR", LOG_ERR},       /* L_ERR */
	{"WARNING", LOG_WARNING}, /* L_WARN */
	{"NOTICE", LOG_NOTICE},   /* L_NOTICE */
	{"INFO", LOG_INFO},       /* L_INFO */
	{"DEBUG", LOG_DEBUG}	  /* L_DBG */
};

int str2facility(char *s)
{
	int i;

	for( i=0; str_fac[i] ; i++) {
		if (!strcasecmp(s,str_fac[i]))
			return int_fac[i];
	}
	return -1;
}

/* fixup function for log_facility cfg parameter */
int log_facility_fixup(void *handle, str *gname, str *name, void **val)
{
	int	i;

	if ((i = str2facility((char *)*val)) == -1) {
		LOG(L_ERR, "log_facility_fixup: invalid log facility: %s\n",
			(char *)*val);
		return -1;
	}
	*val = (void *)(long)i;
	return 0;
}


/**
 * per process debug log level (local)
 */

/* value for unset local log level  */
#define UNSET_LOCAL_DEBUG_LEVEL	-255

/* the local debug log level */
static int _local_debug_level = UNSET_LOCAL_DEBUG_LEVEL;

/**
 * @brief return the log level - the local one if it set,
 *   otherwise the global value
 */
int get_debug_level(void) {
	return (_local_debug_level != UNSET_LOCAL_DEBUG_LEVEL) ?
				_local_debug_level : cfg_get(core, core_cfg, debug);
}

/**
 * @brief set the local debug log level
 */
void set_local_debug_level(int level)
{
	_local_debug_level = level;
}

/**
 * @brief reset the local debug log level
 */
void reset_local_debug_level(void)
{
	_local_debug_level = UNSET_LOCAL_DEBUG_LEVEL;
}
