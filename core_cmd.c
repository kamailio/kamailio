/*
 * $Id$
 *
 * Copyright (C) 2005 iptelorg GmbH
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

#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include "mem/mem.h"
#include "sr_module.h"
#include "dprint.h"
#include "core_cmd.h"
#include "globals.h"
#include "pt.h"
#include "ut.h"
#include "core_cmd.h"


#define MAX_CTIME_LEN 128

/* up time */
static time_t up_since;
static char up_since_ctime[MAX_CTIME_LEN];


static const char* system_listMethods_doc[] = {
	"Lists all RPC methods supported by the server.",  /* Documentation string */
	0                                                  /* Method signature(s) */
};

static void system_listMethods(rpc_t* rpc, void* c)
{
	struct sr_module* t;
	rpc_export_t* ptr;
	
	for(ptr = core_rpc_methods; ptr && ptr->name; ptr++) {
		if (rpc->add(c, "s", ptr->name) < 0) return;
	}

	for(t = modules; t; t = t->next) {
		for(ptr = t->exports->rpc_methods; ptr && ptr->name; ptr++) {
			if (rpc->add(c, "s", ptr->name) < 0) return;
		}
	}
}

static const char* system_methodSignature_doc[] = {
	"Returns signature of given method.",  /* Documentation string */
	0                                      /* Method signature(s) */
};

static void system_methodSignature(rpc_t* rpc, void* c)
{
	rpc->fault(c, 500, "Not Implemented Yet");
}


static const char* system_methodHelp_doc[] = {
	"Print the help string for given method.",  /* Documentation string */
	0                                           /* Method signature(s) */
};

static void system_methodHelp(rpc_t* rpc, void* c)
{	
	struct sr_module* t;
	rpc_export_t* ptr;
	char* name;

	if (rpc->scan(c, "s", &name) < 0) return;

	for(t = modules; t; t = t->next) {
		for(ptr = t->exports->rpc_methods; ptr && ptr->name; ptr++) {
			if (strcmp(name, ptr->name) == 0) {
				if (ptr->doc_str && ptr->doc_str[0]) {
					rpc->add(c, "s", ptr->doc_str[0]);
				} else {
					rpc->add(c, "s", "");
				}
				return;
			}
		}
	}
	rpc->fault(c, 500, "Method Not Implemented");
}


static const char* core_prints_doc[] = {
	"Returns the string given as parameter.",   /* Documentation string */
	0                                           /* Method signature(s) */
};

static void core_prints(rpc_t* rpc, void* c)
{
	char* string;

	if (rpc->scan(c, "s", &string) < 0) return;
	rpc->add(c, "s", string);
}


static const char* core_version_doc[] = {
	"Returns the version string of the server.", /* Documentation string */
	0                                           /* Method signature(s) */
};

static void core_version(rpc_t* rpc, void* c)
{
	rpc->add(c, "s", SERVER_HDR);
}



static const char* core_uptime_doc[] = {
	"Returns uptime of SER server.",  /* Documentation string */
	0                                 /* Method signature(s) */
};


static void core_uptime(rpc_t* rpc, void* c)
{
	time_t now;

	time(&now);
	rpc->printf(c, "now: %s", ctime(&now));
	rpc->printf(c, "up_since: %s", up_since_ctime);
	rpc->printf(c, "uptime: %f", difftime(now, up_since));
}


static const char* core_ps_doc[] = {
	"Returns the description of running SER processes.",  /* Documentation string */
	0                                                     /* Method signature(s) */
};


static void core_ps(rpc_t* rpc, void* c)
{
	int p;

	for (p=0; p<process_count;p++) {
		rpc->printf(c, "pid: %d", pt[p].pid);
		rpc->printf(c, "desc: %s", pt[p].desc);
	}
}


static const char* core_pwd_doc[] = {
	"Returns the working directory of SER server.",    /* Documentation string */
	0                                                  /* Method signature(s) */
};


static void core_pwd(rpc_t* rpc, void* c)
{
        char *cwd_buf;
        int max_len;

        max_len = pathmax();
        cwd_buf = pkg_malloc(max_len);
        if (!cwd_buf) {
                ERR("core_pwd: No memory left\n");
                rpc->fault(c, 500, "Server Ran Out of Memory");
		return;
        }

        if (getcwd(cwd_buf, max_len)) {
		rpc->add(c, "s", cwd_buf);
        } else {
		rpc->fault(c, 500, "getcwd Failed");
        }
        pkg_free(cwd_buf);
}


static const char* core_arg_doc[] = {
	"Returns the list of command line arguments used on SER startup.",  /* Documentation string */
	0                                                                   /* Method signature(s) */
};


static void core_arg(rpc_t* rpc, void* c)
{
        int p;

        for (p = 0; p < my_argc; p++) {
		if (rpc->add(c, "s", my_argv[p]) < 0) return;
        }
}


static const char* core_kill_doc[] = {
	"Sends the given signal to SER.",  /* Documentation string */
	0                                  /* Method signature(s) */
};


static void core_kill(rpc_t* rpc, void* c)
{
	int sig_no;
	if (rpc->scan(c, "d", &sig_no) < 0) return;
	rpc->send(c);
	kill(0, sig_no);
}


/* 
 * RPC Methods exported by this module 
 */
rpc_export_t core_rpc_methods[] = {
	{"system.listMethods",     system_listMethods,     system_listMethods_doc,     RET_ARRAY},
	{"system.methodSignature", system_methodSignature, system_methodSignature_doc, RET_VALUE},
	{"system.methodHelp",      system_methodHelp,      system_methodHelp_doc,      RET_VALUE},
	{"core.prints",            core_prints,            core_prints_doc,            RET_VALUE},
	{"core.version",           core_version,           core_version_doc,           RET_VALUE},
	{"core.uptime",            core_uptime,            core_uptime_doc,            RET_ARRAY},
	{"core.ps",                core_ps,                core_ps_doc,                RET_ARRAY},
	{"core.pwd",               core_pwd,               core_pwd_doc,               RET_VALUE},
	{"core.arg",               core_arg,               core_arg_doc,               RET_ARRAY},
	{"core.kill",              core_kill,              core_kill_doc,              RET_VALUE},
	{0, 0, 0, 0}
};

int rpc_init_time(void)
{
	char *t;
	time(&up_since);
	t=ctime(&up_since);
	if (strlen(t)+1>=MAX_CTIME_LEN) {
		ERR("Too long data %d\n", (int)strlen(t));
		return -1;
	}
	memcpy(up_since_ctime,t,strlen(t)+1);
	return 0;
}
