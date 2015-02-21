/**
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \brief Kamailio xmlops :: Core
 * \ingroup xmlops
 */

/*! \defgroup xmlops Xmlops :: This module implements a range of XML-based
 * operations
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "pv_xml.h"

MODULE_VERSION

extern int pv_xml_buf_size;

static pv_export_t mod_pvs[] = {
	{ {"xml", sizeof("xml")-1}, PVT_OTHER, pv_get_xml, pv_set_xml,
		pv_parse_xml_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[]={
	{ "buf_size",  INT_PARAM, &pv_xml_buf_size },
	{ "xml_ns",    PARAM_STRING|USE_FUNC_PARAM, (void*)pv_xml_ns_param },
	{ 0, 0, 0}
};

/** module exports */
struct module_exports exports= {
	"xmlops",		/* module name */
	 DEFAULT_DLFLAGS,	/* dlopen flags */
	 0,  			/* exported functions */
	 params,		/* exported parameters */
	 0,				/* exported statistics */
	 0,				/* exported MI functions */
	 mod_pvs,		/* exported pseudo-variables */
	 0,				/* extra processes */
	 0,				/* module initialization function */
	 0,				/* response handling function */
 	 0,				/* destroy function */
	 0				/* per-child init function */
};

