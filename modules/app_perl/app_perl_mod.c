/*
 * $Id$
 *
 * Perl module for Kamailio
 *
 * Copyright (C) 2006 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define DEFAULTMODULE "Kamailio"
#define MAX_LIB_PATHS 10

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/time.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/kmi/mi.h"
#include "../../modules/rr/api.h"
#include "../../modules/sl/sl.h"

#include "../../rpc.h"
#include "../../rpc_lookup.h"

/* lock_ops.h defines union semun, perl does not need to redefine it */
#ifdef USE_SYSV_SEM
# define HAS_UNION_SEMUN
#endif

#include "perlfunc.h"
#include "app_perl_mod.h"

/* #include "perlxsi.h" function is in here... */

MODULE_VERSION

/* Full path to the script including executed functions */
char *filename = NULL;

/* Path to an arbitrary directory where the Kamailio Perl modules are
 * installed */
char *modpath = NULL;

/* Function to be called before perl interpreter instance is destroyed
 * when attempting reinit */
static char *perl_destroy_func = NULL;

/* Allow unsafe module functions - functions with fixups. This will create
 * memory leaks, the variable thus is not documented! */
int unsafemodfnc = 0;

/* number of execution cycles after which perl interpreter is reset */
int _ap_reset_cycles_init = 0;
int _ap_exec_cycles = 0;
int *_ap_reset_cycles = 0;

/* Reference to the running Perl interpreter instance */
PerlInterpreter *my_perl = NULL;

/** SL API structure */
sl_api_t slb;

static int ap_init_rpc(void);

/*
 * Module destroy function prototype
 */
static void destroy(void);

/* environment pointer needed to init perl interpreter */
extern char **environ;

/*
 * Module initialization function prototype
 */
static int mod_init(void);


/*
 * Reload perl interpreter - reload perl script. Forward declaration.
 */
struct mi_root* perl_mi_reload(struct mi_root *cmd_tree, void *param);



/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{ "perl_exec_simple", (cmd_function)perl_exec_simple1, 1,  NULL, 0,
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
	{ "perl_exec_simple", (cmd_function)perl_exec_simple2, 2,  NULL, 0,
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
	{ "perl_exec", (cmd_function)perl_exec1, 1,  NULL, 0, 
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
	{ "perl_exec", (cmd_function)perl_exec2, 2, NULL, 0,
							     REQUEST_ROUTE | FAILURE_ROUTE
							   | ONREPLY_ROUTE | BRANCH_ROUTE },
	{ 0, 0, 0, 0, 0, 0 }
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"filename", PARAM_STRING, &filename},
	{"modpath", PARAM_STRING, &modpath},
	{"unsafemodfnc", INT_PARAM, &unsafemodfnc},
	{"reset_cycles", INT_PARAM, &_ap_reset_cycles_init},
	{"perl_destroy_func",  PARAM_STRING, &perl_destroy_func},
	{ 0, 0, 0 }
};


/*
 * Exported MI functions
 */
static mi_export_t mi_cmds[] = {
	/* FIXME This does not yet work... 
	{ "perl_reload",  perl_mi_reload, MI_NO_INPUT_FLAG,  0,  0  },*/
	{ 0, 0, 0, 0, 0}

};




/*
 * Module info
 */

#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif

#ifndef RTLD_GLOBAL
/* Unsupported! */
#define RTLD_GLOBAL 0
#endif

/*
 * Module interface
 */
struct module_exports exports = {
	"app_perl", 
	RTLD_NOW | RTLD_GLOBAL,
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,          /* response function */
	destroy,    /* destroy function */
	0           /* child initialization function */
};



EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);
EXTERN_C void boot_Kamailio(pTHX_ CV* cv);


/*
 * This is output by perl -MExtUtils::Embed -e xsinit
 * and complemented by the Kamailio bootstrapping
 */
EXTERN_C void xs_init(pTHX) {
        char *file = __FILE__;
        dXSUB_SYS;

        newXS("Kamailio::bootstrap", boot_Kamailio, file);

        newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


/*
 * Initialize the perl interpreter.
 * This might later be used to reinit the module.
 */
PerlInterpreter *parser_init(void) {
	int argc = 0;
	char *argv[MAX_LIB_PATHS + 3];
	PerlInterpreter *new_perl = NULL;
	char *entry, *stop, *end;
	int modpathset_start = 0;
	int modpathset_end = 0;
	int i;
	int pr;

	new_perl = perl_alloc();

	if (!new_perl) {
		LM_ERR("could not allocate perl.\n");
		return NULL;
	}

	perl_construct(new_perl);

	argv[0] = ""; argc++; /* First param _needs_ to be empty */
	
	 /* Possible Include path extension by modparam */
	if (modpath && (strlen(modpath) > 0)) {
		modpathset_start = argc;

		entry = modpath;
		stop = modpath + strlen(modpath);
		for (end = modpath; end <= stop; end++) {
			if ( (end[0] == ':') || (end[0] == '\0') ) {
				end[0] = '\0';
				if (argc > MAX_LIB_PATHS) {
					LM_ERR("too many lib paths, skipping lib path: '%s'\n", entry);
				} else {
					LM_INFO("setting lib path: '%s'\n", entry);
					argv[argc] = pkg_malloc(strlen(entry)+20);
					if (!argv[argc]) {
						LM_ERR("not enough pkg mem\n");
						return NULL;
					}
					sprintf(argv[argc], "-I%s", entry);
					modpathset_end = argc;
					argc++;
				}
				entry = end + 1;
			}
		}
	}

	argv[argc] = "-M"DEFAULTMODULE; argc++; /* Always "use" Kamailio.pm */

	argv[argc] = filename; /* The script itself */
	argc++;

	pr=perl_parse(new_perl, xs_init, argc, argv, NULL);
	if (pr) {
		LM_ERR("failed to load perl file \"%s\" with code %d.\n", argv[argc-1], pr);
		if (modpathset_start) {
			for (i = modpathset_start; i <= modpathset_end; i++) {
				pkg_free(argv[i]);
			}
		}
		return NULL;
	} else {
		LM_INFO("successfully loaded perl file \"%s\"\n", argv[argc-1]);
	}

	if (modpathset_start) {
		for (i = modpathset_start; i <= modpathset_end; i++) {
			pkg_free(argv[i]);
		}
	}
	perl_run(new_perl);

	return new_perl;

}

/*
 *
 */
int unload_perl(PerlInterpreter *p) {
	perl_destruct(p);
	perl_free(p);

	return 0;
}


/*
 * reload function.
 * Reinitializes the interpreter. Works, but execution for _all_
 * children is difficult.
 */
int perl_reload(void)
{

	PerlInterpreter *new_perl;

	new_perl = parser_init();

	if (new_perl) {
		unload_perl(my_perl);
		my_perl = new_perl;
#ifdef PERL_EXIT_DESTRUCT_END
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#else
#warning Perl 5.8.x should be used. Please upgrade.
#warning This binary will be unsupported.
		PL_exit_flags |= PERL_EXIT_EXPECTED;
#endif
		return 0;
	} else {
		return -1;
	}

}


/*
 * Reinit through fifo.
 * Currently does not seem to work :((
 */
struct mi_root* perl_mi_reload(struct mi_root *cmd_tree, void *param)
{
	if (perl_reload()<0) {
		return init_mi_tree( 500, "Perl reload failed", 18);
	} else {
		return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	}

}


/*
 * mod_init
 * Called by kamailio at init time
 */
static int mod_init(void) {

	int argc = 1;
	char *argt[] = { MOD_NAME, NULL };
	char **argv;
	struct timeval t1;
	struct timeval t2;

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	if(ap_init_rpc()<0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if (!filename) {
		LM_ERR("insufficient module parameters. Module not loaded.\n");
		return -1;
	}

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	_ap_reset_cycles = shm_malloc(sizeof(int));
	if(_ap_reset_cycles == NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	*_ap_reset_cycles = _ap_reset_cycles_init;

	argv = argt;
	PERL_SYS_INIT3(&argc, &argv, &environ);

	gettimeofday(&t1, NULL);
	my_perl = parser_init();
	gettimeofday(&t2, NULL);

	if (my_perl==NULL)
		goto error;

	LM_INFO("perl interpreter has been initialized (%d.%06d => %d.%06d)\n",
				(int)t1.tv_sec, (int)t1.tv_usec,
				(int)t2.tv_sec, (int)t2.tv_usec);

#ifdef PERL_EXIT_DESTRUCT_END
	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#else
	PL_exit_flags |= PERL_EXIT_EXPECTED;
#endif
	return 0;

error:
	if(_ap_reset_cycles!=NULL)
		shm_free(_ap_reset_cycles);
	_ap_reset_cycles = NULL;
	return -1;
}

/*
 * destroy
 * called by kamailio at exit time
 */
static void destroy(void)
{
	if(_ap_reset_cycles!=NULL)
		shm_free(_ap_reset_cycles);
	_ap_reset_cycles = NULL;

	if(my_perl==NULL)
		return;
	unload_perl(my_perl);
	PERL_SYS_TERM();
	my_perl = NULL;
}


/**
 * count executions and rest interpreter
 *
 */
int app_perl_reset_interpreter(void)
{
	struct timeval t1;
	struct timeval t2;
	char *args[] = { NULL };

	if(*_ap_reset_cycles==0)
		return 0;

	_ap_exec_cycles++;
	LM_DBG("perl interpreter exec cycle [%d/%d]\n",
				_ap_exec_cycles, *_ap_reset_cycles);

	if(_ap_exec_cycles<=*_ap_reset_cycles)
		return 0;

	if(perl_destroy_func)
		call_argv(perl_destroy_func, G_DISCARD | G_NOARGS, args);

	gettimeofday(&t1, NULL);
	if (perl_reload()<0) {
		LM_ERR("perl interpreter cannot be reset [%d/%d]\n",
				_ap_exec_cycles, *_ap_reset_cycles);
		return -1;
	}
	gettimeofday(&t2, NULL);

	LM_INFO("perl interpreter has been reset [%d/%d] (%d.%06d => %d.%06d)\n",
				_ap_exec_cycles, *_ap_reset_cycles,
				(int)t1.tv_sec, (int)t1.tv_usec,
				(int)t2.tv_sec, (int)t2.tv_usec);
	_ap_exec_cycles = 0;

	return 0;
}

/*** RPC implementation ***/

static const char* app_perl_rpc_set_reset_cycles_doc[3] = {
	"Set the value for reset_cycles",
	"Has one parmeter with int value",
	0
};


/*
 * RPC command to set the value for reset_cycles
 */
static void app_perl_rpc_set_reset_cycles(rpc_t* rpc, void* ctx)
{
	int rsv;

	if(rpc->scan(ctx, "d", &rsv)<1)
	{
		rpc->fault(ctx, 500, "Invalid Parameters");
		return;
	}
	if(rsv<=0)
		rsv = 0;

	LM_DBG("new reset cycle value is %d\n", rsv);

	*_ap_reset_cycles = rsv;

	return;
}

static const char* app_perl_rpc_get_reset_cycles_doc[2] = {
	"Get the value for reset_cycles",
	0
};


/*
 * RPC command to set the value for reset_cycles
 */
static void app_perl_rpc_get_reset_cycles(rpc_t* rpc, void* ctx)
{
	int rsv;
	void* th;

	rsv = *_ap_reset_cycles;

	/* add entry node */
	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error root reply");
		return;
	}

	if(rpc->struct_add(th, "d", "reset_cycles", rsv)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding reset cycles");
		return;
	}
	LM_DBG("reset cycle value is %d\n", rsv);

	return;
}


rpc_export_t app_perl_rpc_cmds[] = {
	{"app_perl.set_reset_cycles", app_perl_rpc_set_reset_cycles,
		app_perl_rpc_set_reset_cycles_doc,   0},
	{"app_perl.get_reset_cycles", app_perl_rpc_get_reset_cycles,
		app_perl_rpc_get_reset_cycles_doc,   0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
static int ap_init_rpc(void)
{
	if (rpc_register_array(app_perl_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
