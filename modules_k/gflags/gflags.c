/*
 * $Id$
 *
 * Copyright (C) 2004 FhG
 * Copyright (C) 2005-2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2004-09-09  initial module created (jiri)
 *  2006-05-31  flag range checked ; proper cleanup at module destroy ;
 *              got rid of memory allocation in fixup function ;
 *              optimized fixup function -> compute directly the bitmap ;
 *              allowed functions from BRANCH_ROUTE (bogdan)
 *
 * TODO:
 * -----
 * - named flags (takes a protected name list)
 *
 *
 * gflags module: global flags; it keeps a bitmap of flags
 * in shared memory and may be used to change behaviour 
 * of server based on value of the flags. E.g.,
 *    if (is_gflag("1")) { t_relay_to_udp("10.0.0.1","5060"); }
 *    else { t_relay_to_udp("10.0.0.2","5060"); }
 * The benefit of this module is the value of the switch flags
 * can be manipulated by external applications such as web interface
 * or command line tools.
 *
 *
 */


/* flag buffer size for FIFO protocool */
#define MAX_FLAG_LEN 12
/* FIFO action protocol names */
#define FIFO_SET_GFLAG "set_gflag"
#define FIFO_IS_GFLAG "is_gflag"
#define FIFO_RESET_GFLAG "reset_gflag"

#include "../../sr_module.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../fifo_server.h"
#include <stdio.h>

MODULE_VERSION

static int set_gflag(struct sip_msg*, char *, char *);
static int reset_gflag(struct sip_msg*, char *, char *);
static int is_gflag(struct sip_msg*, char *, char *);

static int fixup_str2int( void** param, int param_no);

static int  mod_init(void);
static void mod_destroy(void);

static int initial=0;
static unsigned int *gflags=0;

static cmd_export_t cmds[]={
	{"set_gflag",    set_gflag,   1,   fixup_str2int,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"reset_gflag",  reset_gflag, 1,   fixup_str2int,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{"is_gflag",     is_gflag,    1,   fixup_str2int,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE|BRANCH_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={ 
	{"initial", INT_PARAM, &initial},
	{0,0,0} 
};

struct module_exports exports = {
	"gflags",
	cmds,        /* exported functions */
	params,      /* exported parameters */
	0,           /* exported statistics */
	mod_init,    /* module initialization function */
	0,           /* response function*/
	mod_destroy, /* destroy function */
	0            /* per-child init function */
};


/**************************** fixup functions ******************************/
static int fixup_str2int( void** param, int param_no)
{
	unsigned int myint;
	str param_str;

	/* we only fix the parameter #1 */
	if (param_no!=1)
		return 0;

	param_str.s=(char*) *param;
	param_str.len=strlen(param_str.s);

	if (str2int(&param_str, &myint )<0) {
		LOG(L_ERR, "ERROR:gflags:fixup_str2int: bad number <%s>\n",
			(char *)(*param));
		return E_CFG;
	}
	if ( myint >= 8*sizeof(*gflags) ) {
		LOG(L_ERR, "ERROR:gflags:fixup_str2int: flag <%d> out of "
			"range [0..%lu]\n", myint, 8*sizeof(*gflags) );
		return E_CFG;
	}
	/* convert from flag index to flag bitmap */
	myint = 1 << myint;
	/* success -- change to int */
	pkg_free(*param);
	*param=(void *)(long)myint;
	return 0;
}


static unsigned int read_flag(FILE *pipe, char *response_file)
{
	char flag_str[MAX_FLAG_LEN];
	int flag_len;
	unsigned int flag;
	str fs;

	if (!read_line(flag_str, MAX_FLAG_LEN, pipe, &flag_len) 
			|| flag_len == 0) {
		fifo_reply(response_file, "400: gflags: invalid flag number\n");
		LOG(L_ERR, "ERROR:gflags:read_flag: invalid flag number\n");
		return 0;
	}

	fs.s=flag_str;fs.len=flag_len;
	if (str2int(&fs, &flag) < 0) {
		fifo_reply(response_file, "400: gflags: invalid flag format\n");
		LOG(L_ERR, "ERROR:gflags:read_flag: invalid flag format\n");
		return 0;
	}

	if ( flag >= 8*sizeof(*gflags) ) {
		fifo_reply(response_file, "400: gflags: flag out of range\n");
		LOG(L_ERR, "ERROR:gflags:read_flag: flag out of range\n");
		return 0;
	}
	/* convert from flag index to flag bitmap */
	flag = 1 << flag;
	return flag;
}


static int fifo_set_gflag( FILE* pipe, char* response_file )
{
	unsigned int flag;

	flag=read_flag(pipe, response_file);
	if (!flag) {
		LOG(L_ERR, "ERROR:gflags:fifo_set_gflag: failed in read_flag\n");
		return 1;
	}

	(*gflags) |= flag;
	fifo_reply (response_file, "200 OK\n");
	return 1;
}


static int fifo_reset_gflag( FILE* pipe, char* response_file )
{
	unsigned int flag;

	flag=read_flag(pipe, response_file);
	if (!flag) {
		LOG(L_ERR, "ERROR:gflags:fifo_reset_gflag: failed in read_flag\n");
		return 1;
	}

	(*gflags) &= ~ flag;
	fifo_reply (response_file, "200 OK\n");
	return 1;
}


static int fifo_is_gflag( FILE* pipe, char* response_file )
{
	unsigned int flag;

	flag=read_flag(pipe, response_file);
	if (!flag) {
		LOG(L_ERR, "ERROR:gflags:fifo_reset_gflag: failed in read_flag\n");
		return 1;
	}

	fifo_reply (response_file, "200 OK\n%s\n", 
			((*gflags) & flag) ? "TRUE" : "FALSE" );
	return 1;
}


static int set_gflag(struct sip_msg *bar, char *flag, char *foo) 
{
	(*gflags) |= (unsigned int)(long)flag;
	return 1;
}


static int reset_gflag(struct sip_msg *bar, char *flag, char *foo)
{
	(*gflags) &= ~ ((unsigned int)(long)flag);
	return 1;
}


static int is_gflag(struct sip_msg *bar, char *flag, char *foo)
{
	return ( (*gflags) & ((unsigned int)(long)flag)) ? 1 : -1;
}


static int mod_init(void)
{
	gflags=(unsigned int *) shm_malloc(sizeof(unsigned int));
	if (!gflags) {
		LOG(L_ERR, "Error: gflags/mod_init: no shmem\n");
		return -1;
	}
	*gflags=initial;
	if (register_fifo_cmd(fifo_set_gflag, FIFO_SET_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register FIFO_SET_GFLAG\n");
		return -1;
	}
	if (register_fifo_cmd(fifo_reset_gflag, FIFO_RESET_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register FIFO_RESET_GFLAG\n");
		return -1;
	}
	if (register_fifo_cmd(fifo_is_gflag, FIFO_IS_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register FIFO_SET_GFLAG\n");
		return -1;
	}

	return 0;
}


static void mod_destroy(void)
{
	if (gflags)
		shm_free(gflags);
}
