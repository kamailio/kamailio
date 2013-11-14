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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define DEFAULTMODULE "Kamailio"
#define MAX_LIB_PATHS 10

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../lib/kmi/mi.h"
#include "../../modules/rr/api.h"
#include "../../modules/sl/sl.h"

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

/* Allow unsafe module functions - functions with fixups. This will create
 * memory leaks, the variable thus is not documented! */
int unsafemodfnc = 0;

/* Reference to the running Perl interpreter instance */
PerlInterpreter *my_perl = NULL;

/** SL API structure */
sl_api_t slb;

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

	if (perl_parse(new_perl, xs_init, argc, argv, NULL)) {
		LM_ERR("failed to load perl file \"%s\".\n", argv[argc-1]);
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
 * Called by kamailio at init time
 */
static int mod_init(void) {

	int ret = 0;
	int argc = 1;
	char *argt[] = { MOD_NAME, NULL };
	char **argv;

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
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

	argv = argt;
	PERL_SYS_INIT3(&argc, &argv, &environ);

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
 * called by kamailio at exit time
 */
static void destroy(void)
{
	if(my_perl==NULL)
		return;
	unload_perl(my_perl);
	PERL_SYS_TERM();
	my_perl = NULL;
}
