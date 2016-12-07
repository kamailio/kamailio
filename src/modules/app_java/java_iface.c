/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../str.h"
#include "../../sr_module.h"

#include <string.h>
#include <jni.h>

#include "global.h"
#include "java_iface.h"
#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"
#include "java_native_methods.h"
#include "java_msgobj.h"
#include "java_sig_parser.h"

/*
    example of java prototype: public int method_name();
    example of kamailio invocation: java_method_exec("method_name", "V");
*/
int j_nst_exec_0(struct sip_msg *msgp, char *method_name, char *signature)
{
    return java_exec(msgp, 0, 0, method_name, signature, NULL);
}
/*
    example of java prototype: public int method_name(int param);
    example of kamailio invocation: java_method_exec("method_name", "I", "5");
*/
int j_nst_exec_1(struct sip_msg *msgp, char *method_name, char *signature, char *param)
{
    return java_exec(msgp, 0, 0, method_name, signature, param);
}
/*
    example of java prototype: public synchronized int method_name();
    example of kamailio invocation: java_s_method_exec("method_name", "V");
*/
int j_s_nst_exec_0(struct sip_msg *msgp, char *method_name, char *signature)
{
    return java_exec(msgp, 0, 1, method_name, signature, NULL);
}
/*
    example of java prototype: public synchronized int method_name(int param);
    example of kamailio invocation: java_s_method_exec("method_name", "I", "5");
*/
int j_s_nst_exec_1(struct sip_msg *msgp, char *method_name, char *signature, char *param)
{
    return java_exec(msgp, 0, 1, method_name, signature, param);
}


/*
    example of java prototype: public static int method_name();
    example of kamailio invocation: java_staticmethod_exec("method_name", "V");
*/
int j_st_exec_0(struct sip_msg *msgp, char *method_name, char *signature)
{
    return java_exec(msgp, 1, 0, method_name, signature, NULL);
}
/*
    example of java prototype: public static int method_name(int param);
    example of kamailio invocation: java_staticmethod_exec("method_name", "I", "5");
*/
int j_st_exec_1(struct sip_msg *msgp, char *method_name, char *signature, char *param)
{
    return java_exec(msgp, 1, 0, method_name, signature, param);
}
/*
    example of java prototype: public static synchronized int method_name();
    example of kamailio invocation: java_s_staticmethod_exec("method_name", "V");
*/
int j_s_st_exec_0(struct sip_msg *msgp, char *method_name, char *signature)
{
    return java_exec(msgp, 1, 1, method_name, signature, NULL);
}
/*
    example of java prototype: public static synchronized int method_name(int param);
    example of kamailio invocation: java_s_staticmethod_exec("method_name", "I", "5");
*/
int j_s_st_exec_1(struct sip_msg *msgp, char *method_name, char *signature, char *param)
{
    return java_exec(msgp, 1, 1, method_name, signature, param);
}


int java_exec(struct sip_msg *msgp, int is_static, int is_synchronized, char *method_name, char *signature, char *param)
{
    char *retval_sig;
    char *cs;
    size_t cslen;
    jint retval;
    int locked;
    jfieldID fid;
    jclass cls;
    jmethodID invk_method, invk_method_ref;
    jvalue *jparam;

    if (signature == NULL || !strcmp(signature, ""))
    {
	LM_ERR("%s: java_method_exec(): signature is empty or invalid.\n", APP_NAME);
	return -1;
    }

    if (param == NULL && strcmp(signature, "V"))
    {
	LM_ERR("%s: java_method_exec(): no parameter (parameter is NULL) but signature '%s' is not equals to 'V'.\n", APP_NAME, signature);
	return -1;
    }

    if (is_sig_allowed(signature) == 0)
    {
	LM_ERR("%s: java_method_exec(): error: signature '%s' isn't supported yet.\n", APP_NAME, signature);
	return -1;
    }

    if (!strcmp(signature, "V"))
    {
	signature = "";
    }

    retval_sig = "I";

    cslen = strlen(signature) + 2 + 1 + 1;	// '(' + 'signature' + ')' + 'return signature' + null terminator
    cs = (char *)pkg_malloc(cslen * sizeof(char));
    if (!cs)
    {
	LM_ERR("%s: pkg_malloc() has failed. Can't allocate %lu bytes. Not enough memory!\n", APP_NAME, (unsigned long)cslen);
	return -1;
    }
    snprintf(cs, cslen, "(%s)%s", signature, retval_sig);
    cs[cslen] = '\0';

    // attach to current thread
    (*jvm)->AttachCurrentThread(jvm, (void **)&env, NULL);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return -1;
    }

    cls = (*env)->GetObjectClass(env, KamailioClassInstance);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
	(*jvm)->DetachCurrentThread(jvm);
        return -1;
    }
    fid = (*env)->GetFieldID(env, cls, "mop", "I");
    if (!fid)
    {
        handle_exception();
	(*jvm)->DetachCurrentThread(jvm);
        return -1;
    }

    msg = msgp;

    // find a method by signature
    invk_method = is_static ? 
		    (*env)->GetStaticMethodID(env, KamailioClassRef, method_name, cs) : 
		    (*env)->GetMethodID(env, KamailioClassRef, method_name, cs);
    if (!invk_method || (*env)->ExceptionCheck(env))
    {
	handle_exception();
    	(*jvm)->DetachCurrentThread(jvm);
	return -1;
    }

    pkg_free(cs);

    // keep local reference to method
    invk_method_ref = (*env)->NewLocalRef(env, invk_method);
    if (!invk_method_ref || (*env)->ExceptionCheck(env))
    {
        handle_exception();
	(*env)->DeleteLocalRef(env, invk_method_ref);
	(*jvm)->DetachCurrentThread(jvm);
        return -1;
    }

    retval = -1;

    if (is_synchronized)
    {
	if ((*env)->MonitorEnter(env, invk_method_ref) != JNI_OK)
        {
	    locked = 0;
	    LM_ERR("%s: MonitorEnter() has failed! Can't synchronize!\n", APP_NAME);
	}
	else
	{
	    locked = 1;
	}
    }

    if (param == NULL)
    {
	retval = is_static ?
		    (int)(*env)->CallStaticIntMethod(env, KamailioClassRef, invk_method_ref) :
		    (int)(*env)->CallIntMethod(env, KamailioClassInstanceRef, invk_method_ref);
    }
    else
    {
	jparam = get_value_by_sig_type(signature, param);
	if (jparam == NULL)
	{
	    (*env)->DeleteLocalRef(env, invk_method_ref);
	    (*env)->DeleteLocalRef(env, invk_method);
    	    (*jvm)->DetachCurrentThread(jvm);
	    return -1;
	}

	retval = is_static ?
		    (int)(*env)->CallStaticIntMethod(env, KamailioClassRef, invk_method_ref, *jparam) :
		    (int)(*env)->CallIntMethod(env, KamailioClassInstanceRef, invk_method_ref, *jparam);
    }

    if ((*env)->ExceptionCheck(env))
    {
        LM_ERR("%s: %s(): %s() has failed. See exception below.\n", APP_NAME,
		(is_static ? 
			(is_synchronized ? "java_s_staticmethod_exec" : "java_staticmethod_exec") :
			(is_synchronized ? "java_s_method_exec" : "java_method_exec")
		),
		is_static ? "CallStaticIntMethod" : "CallIntMethod"
	);

        handle_exception();

	(*env)->DeleteLocalRef(env, invk_method_ref);
	(*env)->DeleteLocalRef(env, invk_method);
        (*jvm)->DetachCurrentThread(jvm);

        return -1;
    }

    if (is_synchronized && locked)
    {
	if ((*env)->MonitorExit(env, invk_method_ref) != JNI_OK)
	{
	    LM_ERR("%s: MonitorExit() has failed! Can't synchronize!\n", APP_NAME);
	}
    }

    (*env)->DeleteLocalRef(env, invk_method_ref);
    (*env)->DeleteLocalRef(env, invk_method);
    (*jvm)->DetachCurrentThread(jvm);

    return retval;
}











