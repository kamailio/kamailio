/* $Id$
 *
 * Copyright (C) 2009 Sippy Software, Inc., http://www.sippysoft.com
 *
 * This file is part of SIP-Router, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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


//// native methods ////

/*
    java: native void LM_XXXXXX(Params XXXX);
    c: JNIEXPORT void JNICALL Java_Kamailio_LM_1XXXXXX(JNIEnv *jenv, jobject this, Params XXXX)

    Why (for example) Java_Kamailio_LM_1ERR but not Java_Kamailio_LM_ERR? 
    See explaination here: http://qscribble.blogspot.ca/2012/04/underscores-in-jni-method-names.html
*/

#ifdef LM_ERR
// java: native void LM_ERR(String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1ERR(JNIEnv *jenv, jobject this, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_ERR("%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif

#ifdef LM_WARN
// java: native void LM_WARN(String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1WARN(JNIEnv *jenv, jobject this, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_WARN("%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif

#ifdef LM_NOTICE
// java: native void LM_NOTICE(String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1NOTICE(JNIEnv *jenv, jobject this, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_NOTICE("%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif

#ifdef LM_INFO
// java: native void LM_INFO(String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1INFO(JNIEnv *jenv, jobject this, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_INFO("%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif

#ifdef LM_DBG
// java: native void LM_DBG(String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1DBG(JNIEnv *jenv, jobject this, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_DBG("%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif

#ifdef LM_CRIT
// java: native void LM_CRIT(String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1CRIT(JNIEnv *jenv, jobject this, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_CRIT("%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif


#ifdef LM_ALERT
// java: native void LM_ALERT(String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1ALERT(JNIEnv *jenv, jobject this, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_ALERT("%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif


#ifdef LM_GEN1
// java: native void LM_GEN1(int logLevel, String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1GEN1(JNIEnv *jenv, jobject this, jint ll, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_GEN1(ll, "%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif

#ifdef LM_GEN2
// java: native void LM_GEN2(int logLevel, int logFacility, String s);
JNIEXPORT void JNICALL Java_Kamailio_LM_1GEN2(JNIEnv *jenv, jobject this, jint ll, jint lf, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_GEN2(ll, lf, "%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}
#endif



//// native properties ////
