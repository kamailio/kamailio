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
#define FIFO_GET_GFLAGS "get_gflags"

#include <stdio.h>
#include "../../sr_module.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../fifo_server.h"
#include "../../mi/mi.h"

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
			"range [0..%lu]\n", myint, (unsigned long)8*sizeof(*gflags) );
		return E_CFG;
	}
	/* convert from flag index to flag bitmap */
	myint = 1 << myint;
	/* success -- change to int */
	pkg_free(*param);
	*param=(void *)(long)myint;
	return 0;
}



/**************************** module functions ******************************/

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



/**************************** FIFO functions ******************************/
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


static int fifo_get_gflags( FILE* pipe, char* response_file )
{
	fifo_reply (response_file, "200 OK\n0x%X\n%u\n", (*gflags),(*gflags));
	return 1;
}



/************************* MI functions *******************************/
#define MI_BAD_PARM_S    "Bad parameter"
#define MI_BAD_PARM_LEN  (sizeof(MI_BAD_PARM_S)-1)

static inline int mi_get_mask( str *val, unsigned int *mask )
{
	/* hexa or decimal*/
	if (val->len>2 && val->s[0]=='0' && val->s[1]=='x') {
		return hexstr2int( val->s+2, val->len-2, mask);
	} else {
		return str2int( val, mask);
	}
}



static struct mi_node* mi_set_gflag(struct mi_node* cmd, void* param )
{
	unsigned int flag;
	struct mi_node* node;

	node = cmd->kids;
	if(node == NULL)
		goto error;

	if( mi_get_mask( &node->value, &flag) <0)
		goto error;
	if (!flag) {
		LOG(L_ERR, "ERROR:gflags:mi_set_gflag: incorrect flag\n");
		goto error;
	}

	(*gflags) |= flag;

	return init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
error:
	return init_mi_tree(MI_BAD_PARM_S,MI_BAD_PARM_LEN);
}



struct mi_node*  mi_reset_gflag(struct mi_node* cmd, void* param )
{
	unsigned int flag;
	struct mi_node* node = NULL;

	node = cmd->kids;
	if(node == NULL)
		goto error;

	if( mi_get_mask( &node->value, &flag) <0)
		goto error;
	if (!flag) {
		LOG(L_ERR, "ERROR:gflags:mi_set_gflag: incorrect flag\n");
		goto error;
	}

	(*gflags) &= ~ flag;

	return init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
error:
	return init_mi_tree(MI_BAD_PARM_S,MI_BAD_PARM_LEN);
}



struct mi_node* mi_is_gflag(struct mi_node* cmd, void* param )
{
	unsigned int flag;
	struct mi_node* rpl= NULL;
	struct mi_node* node = NULL;

	node = cmd->kids;
	if(node == NULL)
		goto error_param;

	if( mi_get_mask( &node->value, &flag) <0)
		goto error_param;
	if (!flag) {
		LOG(L_ERR, "ERROR:gflags:mi_set_gflag: incorrect flag\n");
		goto error_param;
	}

	rpl= init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
	if(rpl ==0)
		return 0;

	if((*gflags) & flag)
		node = add_mi_node_child(rpl, 0, 0, 0, "TRUE", 4);
	else
		node = add_mi_node_child(rpl, 0, 0, 0, "FALSE", 5);

	if(node == NULL)
	{
		LOG(L_ERR, "gflags:mi_set_gflag:ERROR while adding node\n");
		free_mi_tree(rpl);
		return 0;
	}

	return rpl;
error_param:
	return init_mi_tree(MI_BAD_PARM_S,MI_BAD_PARM_LEN);
}


struct mi_node*  mi_get_gflags(struct mi_node* cmd, void* param )
{
	struct mi_node* rpl= NULL;
	struct mi_node* node= NULL;

	rpl= init_mi_tree( MI_200_OK_S, MI_200_OK_LEN );
	if(rpl == NULL)
		return 0;

	node = addf_mi_node_child(rpl,0, 0, 0, "0x%X",(*gflags));
	if(node == NULL)
		goto error;

	node = addf_mi_node_child(rpl,0, 0, 0, "%u",(*gflags));
	if(node == NULL)
		goto error;

	return rpl;
error:
	free_mi_tree(rpl);
	return 0;
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
	if (register_fifo_cmd(fifo_get_gflags, FIFO_GET_GFLAGS, 0) < 0) {
		LOG(L_CRIT, "Cannot register FIFO_SET_GFLAG\n");
		return -1;
	}

	if (register_mi_cmd(mi_set_gflag, FIFO_SET_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register MI %s\n",FIFO_SET_GFLAG);
		return -1;
	}
	if (register_mi_cmd(mi_reset_gflag, FIFO_RESET_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register MI %s\n",FIFO_RESET_GFLAG);
		return -1;
	}
	if (register_mi_cmd(mi_is_gflag, FIFO_IS_GFLAG, 0) < 0) {
		LOG(L_CRIT, "Cannot register MI %s\n",FIFO_SET_GFLAG);
		return -1;
	}
	if (register_mi_cmd(mi_get_gflags, FIFO_GET_GFLAGS, 0) < 0) {
		LOG(L_CRIT, "Cannot register MI %s\n",FIFO_SET_GFLAG);
		return -1;
	}

	return 0;
}


static void mod_destroy(void)
{
	if (gflags)
		shm_free(gflags);
}
