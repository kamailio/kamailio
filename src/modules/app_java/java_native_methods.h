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

#ifndef __JAVA_NATIVE_METHODS_H__
#define	__JAVA_NATIVE_METHODS_H__

#include "../../str.h"
#include "../../sr_module.h"

#include <jni.h>

JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1ERR(JNIEnv *, jobject, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1WARN(JNIEnv *, jobject, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1NOTICE(JNIEnv *, jobject, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1INFO(JNIEnv *, jobject, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1DBG(JNIEnv *, jobject, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1CRIT(JNIEnv *, jobject, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1ALERT(JNIEnv *, jobject, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1GEN1(JNIEnv *, jobject, jint, jstring);
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1GEN2(JNIEnv *, jobject, jint, jint, jstring);

JNIEXPORT jint JNICALL Java_org_siprouter_NativeMethods_KamExec(JNIEnv *, jobject, jstring, jobjectArray);
int KamExec(JNIEnv *, char *, int, char **);


JNIEXPORT jobject JNICALL Java_org_siprouter_SipMsg_ParseSipMsg(JNIEnv *, jobject);

JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getMsgType(JNIEnv *, jobject);
JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getStatus(JNIEnv *, jobject);
JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getRURI(JNIEnv *, jobject);
JNIEXPORT jobject JNICALL Java_org_siprouter_SipMsg_getSrcAddress(JNIEnv *, jobject);
JNIEXPORT jobject JNICALL Java_org_siprouter_SipMsg_getDstAddress(JNIEnv *, jobject);
JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getBuffer(JNIEnv *, jobject);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_seturi(JNIEnv *, jobject, jstring);
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_rewriteuri(JNIEnv *, jobject, jstring);
jint cf_seturi(JNIEnv *, jobject, jstring, char *);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_add_1local_1rport(JNIEnv *, jobject);
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_append_1branch(JNIEnv *, jobject, jstring);
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_drop(JNIEnv *, jobject);

JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_force_1rport(JNIEnv *, jobject);
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_add_1rport(JNIEnv *, jobject);
jint cf_force_rport(JNIEnv *, jobject, char *);

// not confirmed as working
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_force_1send_1socket(JNIEnv *, jobject, jstring, jint);

// not confirmed as working
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_forward(JNIEnv *, jobject, jstring, jint);

JNIEXPORT jboolean JNICALL Java_org_siprouter_CoreMethods_isflagset(JNIEnv *, jobject, jint);
JNIEXPORT void JNICALL Java_org_siprouter_CoreMethods_setflag(JNIEnv *, jobject, jint);
JNIEXPORT void JNICALL Java_org_siprouter_CoreMethods_resetflag(JNIEnv *, jobject, jint);
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_revert_1uri(JNIEnv *, jobject);
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_route(JNIEnv *, jobject, jobject);

#endif
