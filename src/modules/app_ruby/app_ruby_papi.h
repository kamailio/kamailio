/**
 * Copyright (C) 2022 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifndef _APP_RUBY_PAPI_H_
#define _APP_RUBY_PAPI_H_

#include "../../core/kemi.h"

typedef struct app_ruby_papi
{
	int (*AppRubyInitChild)(void);
	void (*AppRubyModDestroy)(void);
	int (*AppRubyRunEx)(sip_msg_t *msg, char *func, char *p1, char *p2,
			char *p3, int emode);
	int (*AppRubyOptSetS)(char *optName, str *optVal);
	int (*AppRubyOptSetN)(char *optName, int optVal);
	int (*AppRubyOptSetP)(char *optName, void *optVal);
	int (*AppRubyGetExportSize)(void);
	sr_kemi_t *(*AppRubyGetExport)(int idx);
	int (*AppRubyInitialized)(void);
	int (*AppRubyLocalVersion)(void);
} app_ruby_papi_t;

typedef int (*app_ruby_proc_bind_f)(app_ruby_papi_t *papi);

#endif /* _APP_RUBY_PAPI_H_ */
