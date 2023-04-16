/**
 * Copyright (C) 2018 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "../app_ruby/app_ruby_papi.h"

#include "app_ruby_api.h"

MODULE_VERSION


int app_ruby_proc_bind(app_ruby_papi_t *papi)
{
	papi->AppRubyInitChild = app_ruby_proc_init_child;
	papi->AppRubyModDestroy = app_ruby_proc_mod_destroy;
	papi->AppRubyRunEx = app_ruby_proc_run_ex;
	papi->AppRubyOptSetS = app_ruby_proc_opt_set_s;
	papi->AppRubyOptSetN = app_ruby_proc_opt_set_n;
	papi->AppRubyOptSetP = app_ruby_proc_opt_set_p;
	papi->AppRubyGetExportSize = app_ruby_proc_get_export_size;
	papi->AppRubyGetExport = app_ruby_proc_get_export;
	papi->AppRubyInitialized = app_ruby_proc_initialized;
	papi->AppRubyLocalVersion = app_ruby_proc_local_version;

	return 0;
}
