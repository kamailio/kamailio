/*
 * $Id$
 *
 * Perl module for OpenSER
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define DEFAULTMODULE "OpenSER"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../mi/mi.h"
#include "../rr/api.h"
#include "../sl/sl_api.h"

/* lock_ops.h defines union semun, perl does not need to redefine it */
#ifdef USE_SYSV_SEM
# define HAS_UNION_SEMUN
#endif

#include "perlfunc.h"
#include "perl.h"

/* #include "perlxsi.h" function is in here... */

MODULE_VERSION

/* Full path to the script including executed functions */
char *filename = NULL;

/* Path to an arbitrary directory where the OpenSER Perl modules are
 * installed */
char *modpath = NULL;

/* Allow unsafe module functions - functions with fixups. This will create
 * memory leaks, the variable thus is not documented! */
int unsafemodfnc = 0;

/* Reference to the running Perl interpreter instance */
PerlInterpreter *my_perl = NULL;

/** SL binds */
struct sl_binds slb;

/*
 * Module destroy function prototype
 */
static void destroy(void);

/*
 * Module child-init function prototype
 */
static int child_init(int rank);

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
	{"filename", STR_PARAM, &filename},
	{"modpath", STR_PARAM, &modpath},
	{"unsafemodfnc", INT_PARAM, &unsafemodfnc},
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
	"perl", 
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
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	return 0;
}


EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);
EXTERN_C void boot_OpenSER(pTHX_ CV* cv);


/*
 * This is output by perl -MExtUtils::Embed -e xsinit
 * and complemented by the OpenSER bootstrapping
 */
EXTERN_C void xs_init(pTHX) {
        char *file = __FILE__;
        dXSUB_SYS;

        newXS("OpenSER::bootstrap", boot_OpenSER, file);

        newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}


/*
 * Initialize the perl interpreter.
 * This might later be used to reinit the module.
 */
PerlInterpreter *parser_init(void) {
	int argc = 0;
	char *argv[9];
	PerlInterpreter *new_perl = NULL;
	int modpathset = 0;

	new_perl = perl_alloc();

	if (!new_perl) {
		LM_ERR("could not allocate perl.\n");
		return NULL;
	}

	perl_construct(new_perl);

	argv[0] = ""; argc++; /* First param _needs_ to be empty */
	
	 /* Possible Include path extension by modparam */
	if (modpath && (strlen(modpath) > 0)) {
		modpathset = argc;
		LM_INFO("setting lib path: '%s'\n", modpath);
		argv[argc] = pkg_malloc(strlen(modpath)+20);
		sprintf(argv[argc], "-I%s", modpath);
		argc++;
	}

	argv[argc] = "-M"DEFAULTMODULE; argc++; /* Always "use" Openser.pm */

	argv[argc] = filename; /* The script itself */
	argc++;

	if (perl_parse(new_perl, xs_init, argc, argv, NULL)) {
		LM_ERR("failed to load perl file \"%s\".\n", argv[argc-1]);
		if (modpathset) pkg_free(argv[modpathset]);
		return NULL;
	} else {
		LM_INFO("successfully loaded perl file \"%s\"\n", argv[argc-1]);
	}

	if (modpathset) pkg_free(argv[modpathset]);
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
int perl_reload(struct sip_msg *m, char *a, char *b) {

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
		return 1;
	} else {
		return 0;
	}

}


/*
 * Reinit through fifo.
 * Currently does not seem to work :((
 */
struct mi_root* perl_mi_reload(struct mi_root *cmd_tree, void *param)
{
	if (perl_reload(NULL, NULL, NULL)) {
		return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	} else {
		return init_mi_tree( 500, "Perl reload failed", 18);
	}

}


/*
 * mod_init
 * Called by openser at init time
 */
static int mod_init(void) {

	int ret = 0;

	if (!filename) {
		LM_ERR("insufficient module parameters. Module not loaded.\n");
		return -1;
	}

	/**
	 * We will need sl_send_reply from stateless
	 * module for sending replies
	 */


	/* load the SL API */
	if (load_sl_api(&slb)!=0) {
		LM_ERR("can't load SL API\n");
		return -1;
	}

	PERL_SYS_INIT3(NULL, NULL, &environ);

	if ((my_perl = parser_init())) {
		ret = 0;
#ifdef PERL_EXIT_DESTRUCT_END
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#else
		PL_exit_flags |= PERL_EXIT_EXPECTED;
#endif

	} else {
		ret = -1;
	}

	return ret;
}

/*
 * destroy
 * called by openser at exit time
 */
static void destroy(void)
{
	if(my_perl==NULL)
		return;
	unload_perl(my_perl);
	PERL_SYS_TERM();
	my_perl = NULL;
}
