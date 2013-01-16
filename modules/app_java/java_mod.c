/**
 * $Id$
 *
 * Copyright (C) 2013 Konstantin Mosesov
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <libgen.h>

#include "../../str.h"
#include "../../sr_module.h"

#include <jni.h>

#include "global.h"

#include "utils.h"
#include "java_mod.h"
#include "java_iface.h"
#include "java_support.h"

#include "java_native_methods.h"

MODULE_VERSION

static str script_name = {.s = "/usr/local/etc/sip-router/handler.java", .len = 0};
static str class_name = {.s = "Kamailio", .len = 10};
static str child_init_mname = { .s = "child_init", .len = 0};
static str java_pkg_tree_path_str = { .s = "pkg_tree_path", .len = 0};
static str java_options_str = { .s = "java_options", .len = 0};

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

char *dname = NULL, *bname = NULL;

/** module parameters */
static param_export_t params[]={
    {"script_name",        STR_PARAM, &script_name },
    {"class_name",         STR_PARAM, &class_name },
    {"pkg_tree_path",	   STR_PARAM, &java_pkg_tree_path_str },
    {"child_init_method",  STR_PARAM, &child_init_mname },
    {"java_options",	   STR_PARAM, &java_options_str },
    {0,0,0}
};


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    { "java_exec", (cmd_function)java_exec, 1,  NULL, 0,	REQUEST_ROUTE | FAILURE_ROUTE  | ONREPLY_ROUTE | BRANCH_ROUTE },
    { 0, 0, 0, 0, 0, 0 }
};

/** module exports */
struct module_exports exports = {
    "app_java",                   /* module name */
    RTLD_NOW | RTLD_GLOBAL,         /* dlopen flags */
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
    char *class_object_name;
    size_t class_object_name_len;

    options = (JavaVMOption *)pkg_realloc(NULL, sizeof(JavaVMOption));
    if (!options)
    {
	LM_ERR("Couldn't initialize Java VM: not enough memory\n");
	return -1;
    }
    memset(options, 0, sizeof(JavaVMOption));

    LM_INFO("Initializing Java VM with options: %s\n", java_options_str.s);

    opts = split(java_options_str.s, " ");
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

    LM_INFO("app_java: Java VM initialization OK\n");

    // attach to current thread
    (*jvm)->AttachCurrentThread(jvm, (void **)&env, NULL);
    if ((*env)->ExceptionCheck(env))
    {
	handle_exception();
	return -1;
    }

    // Loading main class
    if (java_pkg_tree_path_str.len <= 0)
    {
	class_object_name_len =  strlen(JAVA_MODULE_PKG_PATH) + class_name.len + 1;	// default package name  plus  class name  plus package trailing slash
    }
    else
    {
	class_object_name_len =  java_pkg_tree_path_str.len + class_name.len + 1;	// package name  plus  class name  plus package trailing slash
    }

    class_object_name = (char *)pkg_realloc(NULL, (class_object_name_len + 1) * sizeof(char));
    if (!class_object_name)
    {
	LM_ERR("pkg_realloc has failed. Not enough memory!\n");
	return -1;
    }
    memset(class_object_name, 0, (class_object_name_len + 1) * sizeof(char));
    if (java_pkg_tree_path_str.len <= 0)
    {
	snprintf(class_object_name, class_object_name_len, "%s%s", java_pkg_tree_path_str.s, class_name.s);
    }
    else
    {
	snprintf(class_object_name, class_object_name_len, "%s/%s", JAVA_MODULE_PKG_PATH, class_name.s);
    }


//    KamailioClass = (*env)->FindClass(env, class_name.s);
    KamailioClass = (*env)->FindClass(env, class_object_name);
    pkg_free(class_object_name);
    if (!KamailioClass || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	if (jvm != NULL)
	    (*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    KamailioID = (*env)->GetMethodID(env, KamailioClass, "<init>", "()V");
    if (!KamailioID || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	if (jvm != NULL)
	    (*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    // calling constructor
    KamailioClassInstance = (*env)->NewObject(env, KamailioClass, KamailioID);
    if (!KamailioClassInstance || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	if (jvm != NULL)
	    (*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    // keep a reference to kamailio class instance
    KamailioClassInstanceRef = (*env)->NewGlobalRef(env, KamailioClassInstance);
    if (!KamailioClassInstanceRef || (*env)->ExceptionCheck(env))
    {
	handle_exception();
	if (jvm != NULL)
	    (*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    LM_INFO("app_java: initialization OK\n");

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
	if (jvm != NULL)
    	    (*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    retval = (int)(*env)->CallIntMethod(env, KamailioClassInstanceRef, child_init_id, rank);
    if ((*env)->ExceptionCheck(env))
    {
	handle_exception();
	if (jvm != NULL)
    	    (*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    (*env)->DeleteLocalRef(env, child_init_id);


    if (jvm != NULL)
        (*jvm)->DetachCurrentThread(jvm);

    return retval;
}

static void mod_destroy(void)
{
    if (env != NULL)
    {
	if (KamailioClassInstanceRef != NULL)
	    (*env)->DeleteGlobalRef(env, KamailioClassInstanceRef);
    }

    if (jvm != NULL)
    {

	(*jvm)->DetachCurrentThread(jvm);
	(*jvm)->DestroyJavaVM(jvm);
    }
}
