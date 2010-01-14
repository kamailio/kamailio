/*
 * Copyright (c) 2010 IPTEGO GmbH.
 * All rights reserved.
 *
 * This file is part of sip-router, a free SIP server.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY IPTEGO GmbH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL IPTEGO GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Bogdan Pintea.
 */


#include "sr_module.h"
#include "dprint.h"
#include "mem/mem.h"

#include "binds.h"


xl_parse_format_f *xl_parse;
xl_print_log_f *xl_print;

struct tm_binds tmb;

#define PARSE_BIND_NAME	"xparse"
#define PRINT_BIND_NAME	"xprint"

#define LOADTM_BIND_NAME	"load_tm"

#define NO_SCRIPT	-1 /* this is dumb: every module using xlog defines it */


int xll_bind(void)
{
	xl_parse = (xl_parse_format_f *)find_export(PARSE_BIND_NAME, NO_SCRIPT, 0);
	xl_print = (xl_print_log_f *)find_export(PRINT_BIND_NAME, NO_SCRIPT, 0);

	if (xl_parse && xl_print)
		return 0;

	ERR("failed to load 'xl_lib' routines `%s', `%s'; is module XLOG "
			"loaded?\n", PARSE_BIND_NAME, PRINT_BIND_NAME);
	return -1;
}

void xl_elogs_free(xl_elog_t *root)
{
	xl_elog_t *next;
	while (root) {
		next = root->next;
		pkg_free(root);
		root = next;
	}
}

int tm_bind(void)
{
	load_tm_f tm_load;
	if (! (tm_load = (load_tm_f)find_export(LOADTM_BIND_NAME, NO_SCRIPT, 0))) {
		ERR("failed to find '%s' export; is module TM loaded?\n", 
				LOADTM_BIND_NAME);
		return -1;
	}

	if (tm_load(&tmb) < 0) {
		BUG("TM module loaded but failed to load its exported symbols.\n");
		return -1;
	}
	
	return 0;
}
