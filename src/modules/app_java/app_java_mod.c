/**
 * Copyright (C) 2013 Konstantin Mosesov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#include <libgen.h>

#include "../../str.h"
#include "../../sr_module.h"

#include <jni.h>

#include "global.h"

#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"

#include "java_native_methods.h"

MODULE_VERSION

static char* class_name = "Kamailio";
static char* child_init_mname = "child_init";
static char* java_options_str = "-Djava.compiler=NONE";

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);


/** module parameters */
static param_export_t params[] = {
    {"class_name",         PARAM_STRING, &class_name },
    {"child_init_method",  PARAM_STRING, &child_init_mname }, /* Unused parameter? */
    {"java_options",	   PARAM_STRING, &java_options_str },
    {"force_cmd_exec", INT_PARAM, &force_cmd_exec },
    {0,0,0}
};


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    { "java_method_exec",		(cmd_function)j_nst_exec_0,	2,	NULL,	0,	ANY_ROUTE },
    { "java_method_exec",		(cmd_function)j_nst_exec_1,	3,	NULL,	0,	ANY_ROUTE },
    { "java_s_method_exec",		(cmd_function)j_s_nst_exec_0,	2,	NULL,	0,	ANY_ROUTE },
    { "java_s_method_exec",		(cmd_function)j_s_nst_exec_1,	3,	NULL,	0,	ANY_ROUTE },

    { "java_staticmethod_exec",		(cmd_function)j_st_exec_0,	2,	NULL,	0,	ANY_ROUTE },
    { "java_staticmethod_exec",		(cmd_function)j_st_exec_1,	3,	NULL,	0,	ANY_ROUTE },
    { "java_s_staticmethod_exec",	(cmd_function)j_s_st_exec_0,	2,	NULL,	0,	ANY_ROUTE },
    { "java_s_staticmethod_exec",	(cmd_function)j_s_st_exec_1,	3,	NULL,	0,	ANY_ROUTE },

    { 0, 0, 0, 0, 0, 0 }
};

/** module exports */
struct module_exports exports = {
    APP_NAME,                       /* module name */
//    RTLD_NOW | RTLD_GLOBAL,         /* dlopen flags */
    DEFAULT_DLFLAGS,		    /* dlopen flags */
    cmds,                           /* exported functions */
    params,                         /* exported parameters */
    0,                              /* exported statistics */
    0,                              /* exported MI functions */
    0,                              /* exported pseudo-variables */
    0,                              /* extra processes */
    mod_init,                       /* module initialization function */
    (response_function) NULL,       /* response handling function */
    (destroy_function) mod_destroy, /* destroy function */
    child_init                      /* per-child init function */
};

static int mod_init(void)
{
    JavaVMInitArgs  vm_args;
    jint res;
    JavaVMOption *options;
    char **opts;
    int nOptions;

    if (force_cmd_exec < 0 || force_cmd_exec > 1)
    {
	LM_ERR("Parameter force_cmd_exec should be either 0 or 1\n");
	return -1;
    }

    if (force_cmd_exec)
    {
	LM_NOTICE("%s: Parameter force_cmd_exec may cause a memory leaks if used from embedded languages\n", APP_NAME);
    }

    options = (JavaVMOption *)pkg_malloc(sizeof(JavaVMOption));
    if (!options)
    {
	LM_ERR("pkg_malloc() failed: Couldn't initialize Java VM: Not enough memory\n");
	return -1;
    }
    memset(options, 0, sizeof(JavaVMOption));

    LM_INFO("Initializing Java VM with options: %s\n", java_options_str);

    opts = split(java_options_str, " ");
    for (nOptions=0; opts[nOptions] != NULL; nOptions++)
    {
	options[nOptions].optionString = opts[nOptions];
    }

    /* IMPORTANT: specify vm_args version # if you use JDK1.1.2 and beyond */
    vm_args.version = JNI_VERSION_1_2;
    vm_args.nOptions = nOptions;
    vm_args.ignoreUnrecognized = JNI_FALSE;
    vm_args.options = options;

    res = JNI_CreateJavaVM(&jvm, (void **)&env, &vm_args);
    if (res < 0)
    {
	handle_VM_init_failure(res);
	return -1;
    }

    LM_INFO("%s: Java VM initialization OK\n", APP_NAME);

    // attach to current thread
    (*jvm)->AttachCurrentThread(jvm, (void **)&env, NULL);
    if ((*env)->ExceptionCheck(env))
    {
	handle_exception();
	return -1;
    }

    KamailioClass = (*env)->FindClass(env, class_name);
    if (!KamailioClass || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    KamailioClassRef = (*env)->NewGlobalRef(env, KamailioClass);
    if (!KamailioClassRef || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    KamailioID = (*env)->GetMethodID(env, KamailioClass, "<init>", "()V");
    if (!KamailioID || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    // calling constructor
    KamailioClassInstance = (*env)->NewObject(env, KamailioClass, KamailioID);
    if (!KamailioClassInstance || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    // keep a reference to kamailio class instance
    KamailioClassInstanceRef = (*env)->NewGlobalRef(env, KamailioClassInstance);
    if (!KamailioClassInstanceRef || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    LM_INFO("%s: module initialization OK\n", APP_NAME);

    if (jvm != NULL)
        (*jvm)->DetachCurrentThread(jvm);

    return 0;
}

static int child_init(int rank)
{
    int retval;
    jmethodID child_init_id;

    // attach to current thread
    (*jvm)->AttachCurrentThread(jvm, (void **)&env, NULL);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return -1;
    }

    child_init_id = (*env)->GetMethodID(env, KamailioClass, "child_init", "(I)I");
    if ((*env)->ExceptionCheck(env))
    {
	handle_exception();
    	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    retval = (int)(*env)->CallIntMethod(env, KamailioClassInstanceRef, child_init_id, rank);
    if ((*env)->ExceptionCheck(env))
    {
	handle_exception();
    	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    (*env)->DeleteLocalRef(env, child_init_id);
    (*jvm)->DetachCurrentThread(jvm);

    msg = NULL;

    return retval;
}

static void mod_destroy(void)
{
    if (env != NULL)
    {
	(*env)->DeleteGlobalRef(env, KamailioClassInstanceRef);
	(*env)->DeleteGlobalRef(env, KamailioClassRef);
    }

    if (jvm != NULL)
    {
	(*jvm)->DetachCurrentThread(jvm);
	(*jvm)->DestroyJavaVM(jvm);
    }

    if (msg)
    {
	pkg_free(msg);
    }

}
