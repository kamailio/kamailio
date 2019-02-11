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

#include "../../core/str.h"
#include "../../core/sr_module.h"

#include <jni.h>

#include "global.h"

#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"

#include "java_native_methods.h"

MODULE_VERSION

static char *class_name = "Kamailio";
static char *child_init_mname = "child_init";
static char *java_options_str = "-Djava.compiler=NONE";

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);


int force_cmd_exec = 0;
JavaVM *_aj_jvm = NULL;
JNIEnv *_aj_env = NULL;
jclass KamailioClass;
jclass KamailioClassRef;
jclass KamailioClassInstanceRef;
jobject KamailioClassInstance;
jmethodID KamailioID;
sip_msg_t *_aj_msg = NULL;

/** module parameters */
static param_export_t params[] = {
	{"class_name", PARAM_STRING, &class_name},
	{"child_init_method", PARAM_STRING, &child_init_mname}, /* unused? */
	{"java_options", PARAM_STRING, &java_options_str},
	{"force_cmd_exec", INT_PARAM, &force_cmd_exec},
	{0, 0, 0}
};


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"java_method_exec", (cmd_function)j_nst_exec_0, 2, NULL, 0, ANY_ROUTE},
	{"java_method_exec", (cmd_function)j_nst_exec_1, 3, NULL, 0, ANY_ROUTE},
	{"java_s_method_exec", (cmd_function)j_s_nst_exec_0, 2, NULL, 0,
				ANY_ROUTE},
	{"java_s_method_exec", (cmd_function)j_s_nst_exec_1, 3, NULL, 0,
				ANY_ROUTE},
	{"java_staticmethod_exec", (cmd_function)j_st_exec_0, 2, NULL, 0,
				ANY_ROUTE},
	{"java_staticmethod_exec", (cmd_function)j_st_exec_1, 3, NULL, 0,
				ANY_ROUTE},
	{"java_s_staticmethod_exec", (cmd_function)j_s_st_exec_0, 2, NULL, 0,
				ANY_ROUTE},
	{"java_s_staticmethod_exec", (cmd_function)j_s_st_exec_1, 3, NULL, 0,
				ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

/** module exports */
struct module_exports exports = {
	APP_NAME, /* module name */
	DEFAULT_DLFLAGS,   /* dlopen flags */
	cmds,		   /* exported functions */
	params,		   /* exported parameters */
	0,		   /* exported RPC methods */
	0,		   /* exported pseudo-variables */
	0,	   	   /* response handling function */
	mod_init,	   /* module initialization function */
	child_init,	   /* per-child init function */
	mod_destroy 	   /* destroy function */
};

static int mod_init(void)
{
	JavaVMInitArgs vm_args;
	jint res;
	JavaVMOption *options;
	char **opts;
	int nOptions;

	if(force_cmd_exec < 0 || force_cmd_exec > 1) {
		LM_ERR("Parameter force_cmd_exec should be either 0 or 1\n");
		return -1;
	}

	if(force_cmd_exec) {
		LM_NOTICE("%s: Parameter force_cmd_exec may cause a memory leaks if "
				  "used from embedded languages\n",
				APP_NAME);
	}

	options = (JavaVMOption *)pkg_malloc(sizeof(JavaVMOption));
	if(!options) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(options, 0, sizeof(JavaVMOption));

	LM_INFO("Initializing Java VM with options: %s\n", java_options_str);

	opts = split(java_options_str, " ");
	for(nOptions = 0; opts[nOptions] != NULL; nOptions++) {
		options[nOptions].optionString = opts[nOptions];
	}

	/* IMPORTANT: specify vm_args version # if you use JDK1.1.2 and beyond */
	vm_args.version = JNI_VERSION_1_2;
	vm_args.nOptions = nOptions;
	vm_args.ignoreUnrecognized = JNI_FALSE;
	vm_args.options = options;

	res = JNI_CreateJavaVM(&_aj_jvm, (void **)&_aj_env, &vm_args);
	if(res < 0) {
		handle_VM_init_failure(res);
		return -1;
	}

	LM_INFO("%s: Java VM initialization OK\n", APP_NAME);

	// attach to current thread
	(*_aj_jvm)->AttachCurrentThread(_aj_jvm, (void **)&_aj_env, NULL);
	if((*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		return -1;
	}

	KamailioClass = (*_aj_env)->FindClass(_aj_env, class_name);
	if(!KamailioClass || (*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		return -1;
	}

	KamailioClassRef = (*_aj_env)->NewGlobalRef(_aj_env, KamailioClass);
	if(!KamailioClassRef || (*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		return -1;
	}

	KamailioID =
			(*_aj_env)->GetMethodID(_aj_env, KamailioClass, "<init>", "()V");
	if(!KamailioID || (*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		return -1;
	}

	// calling constructor
	KamailioClassInstance =
			(*_aj_env)->NewObject(_aj_env, KamailioClass, KamailioID);
	if(!KamailioClassInstance || (*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		return -1;
	}

	// keep a reference to kamailio class instance
	KamailioClassInstanceRef =
			(*_aj_env)->NewGlobalRef(_aj_env, KamailioClassInstance);
	if(!KamailioClassInstanceRef || (*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		return -1;
	}

	LM_INFO("%s: module initialization OK\n", APP_NAME);

	if(_aj_jvm != NULL)
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);

	return 0;
}

static int child_init(int rank)
{
	int retval;
	jmethodID child_init_id;

	// attach to current thread
	(*_aj_jvm)->AttachCurrentThread(_aj_jvm, (void **)&_aj_env, NULL);
	if((*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		return -1;
	}

	child_init_id = (*_aj_env)->GetMethodID(
			_aj_env, KamailioClass, "child_init", "(I)I");
	if((*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		return -1;
	}

	retval = (int)(*_aj_env)->CallIntMethod(
			_aj_env, KamailioClassInstanceRef, child_init_id, rank);
	if((*_aj_env)->ExceptionCheck(_aj_env)) {
		handle_exception();
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		return -1;
	}

	(*_aj_env)->DeleteLocalRef(_aj_env, child_init_id);
	(*_aj_jvm)->DetachCurrentThread(_aj_jvm);

	_aj_msg = NULL;

	return retval;
}

static void mod_destroy(void)
{
	if(_aj_env != NULL) {
		(*_aj_env)->DeleteGlobalRef(_aj_env, KamailioClassInstanceRef);
		(*_aj_env)->DeleteGlobalRef(_aj_env, KamailioClassRef);
	}

	if(_aj_jvm != NULL) {
		(*_aj_jvm)->DetachCurrentThread(_aj_jvm);
		(*_aj_jvm)->DestroyJavaVM(_aj_jvm);
	}

	if(_aj_msg) {
		pkg_free(_aj_msg);
	}
}
