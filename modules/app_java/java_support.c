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

#include <libgen.h>

#include "../../str.h"
#include "../../sr_module.h"

#include <jni.h>

#include "global.h"

#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"


static char *_append_exception_trace_messages(char *msg_str, jthrowable a_exception, jmethodID a_mid_throwable_getCause, jmethodID a_mid_throwable_getStackTrace, jmethodID a_mid_throwable_toString, jmethodID a_mid_frame_toString)
{
    jobjectArray frames;
    jsize frames_length, i;
    jstring msg_obj;
    jobject frame;
    jthrowable cause;
    jclass exClass;
    jmethodID mid;
    jboolean isCopy;

    const char *tmpbuf;

    // Get the array of StackTraceElements.
    frames = (jobjectArray) (*env)->CallObjectMethod(env, a_exception, a_mid_throwable_getStackTrace);
    if (!frames)
    {
        exClass = (*env)->GetObjectClass(env, a_exception);
        mid = (*env)->GetMethodID(env, exClass, "toString", "()Ljava/lang/String;");
        msg_obj = (jstring) (*env)->CallObjectMethod(env, a_exception, mid);

        isCopy = JNI_FALSE;
	tmpbuf = (*env)->GetStringUTFChars(env, msg_obj, &isCopy);

	strcat(msg_str, tmpbuf);
	strcat(msg_str, "\n    <<No stacktrace available>>");

        (*env)->ReleaseStringUTFChars(env, msg_obj, tmpbuf);
        (*env)->DeleteLocalRef(env, msg_obj);

	return msg_str;
    }
    else
    {
	frames_length = (*env)->GetArrayLength(env, frames);
    }

    // Add Throwable.toString() before descending stack trace messages.
    if (frames != 0)
    {
        msg_obj = (jstring) (*env)->CallObjectMethod(env, a_exception, a_mid_throwable_toString);
	tmpbuf = (*env)->GetStringUTFChars(env, msg_obj, 0);
	
	strcat(msg_str, "Exception in thread \"main\" ");
	strcat(msg_str, tmpbuf);

        (*env)->ReleaseStringUTFChars(env, msg_obj, tmpbuf);
        (*env)->DeleteLocalRef(env, msg_obj);
    }


    // Append stack trace messages if there are any.
    if (frames_length > 0)
    {
        for (i=0; i<frames_length; i++)
        {
            // Get the string returned from the 'toString()'
            // method of the next frame and append it to
            // the error message.
            frame = (*env)->GetObjectArrayElement(env, frames, i);
            msg_obj = (jstring) (*env)->CallObjectMethod(env, frame, a_mid_frame_toString);

            tmpbuf = (*env)->GetStringUTFChars(env, msg_obj, 0);

	    strcat(msg_str, "\n    at ");
	    strcat(msg_str, tmpbuf);

            (*env)->ReleaseStringUTFChars(env, msg_obj, tmpbuf);
            (*env)->DeleteLocalRef(env, msg_obj);
            (*env)->DeleteLocalRef(env, frame);
        }
    }
    else
    {
	strcat(msg_str, "\n    <<No stacktrace available>>");
    }


    // If 'a_exception' has a cause then append the
    // stack trace messages from the cause.
    if (frames != 0)
    {
        cause = (jthrowable) (*env)->CallObjectMethod(env, a_exception, a_mid_throwable_getCause);
        if (cause != 0)
        {
            tmpbuf = _append_exception_trace_messages(msg_str, cause, a_mid_throwable_getCause, a_mid_throwable_getStackTrace, a_mid_throwable_toString, a_mid_frame_toString);
	    strcat(msg_str, tmpbuf);
        }
    }

    if (msg_str != NULL)
	return strdup(msg_str);
    else
	return NULL;
}

void handle_exception(void)
{
    char *error_msg = NULL;
    char msg_str[8192];

    jthrowable exception;
    jclass throwable_class, frame_class;
    jmethodID mid_throwable_getCause, mid_throwable_getStackTrace;
    jmethodID mid_throwable_toString, mid_frame_toString;

    if (!(*env)->ExceptionCheck(env))
	return;

    memset(&msg_str, 0, sizeof(msg_str));

    // Get the exception and clear as no
    // JNI calls can be made while an exception exists.
    exception = (*env)->ExceptionOccurred(env);
    if (exception)
    {
//	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);

	throwable_class = (*env)->FindClass(env, "java/lang/Throwable");

	mid_throwable_getCause = (*env)->GetMethodID(env, throwable_class, "getCause", "()Ljava/lang/Throwable;");
        mid_throwable_getStackTrace =  (*env)->GetMethodID(env, throwable_class, "getStackTrace",  "()[Ljava/lang/StackTraceElement;");
	mid_throwable_toString = (*env)->GetMethodID(env, throwable_class, "toString", "()Ljava/lang/String;");

        frame_class = (*env)->FindClass(env, "java/lang/StackTraceElement");
	mid_frame_toString = (*env)->GetMethodID(env, frame_class, "toString", "()Ljava/lang/String;");

        error_msg = _append_exception_trace_messages(msg_str, exception, mid_throwable_getCause, mid_throwable_getStackTrace, mid_throwable_toString, mid_frame_toString);    


	(*env)->DeleteLocalRef(env, exception);
    }

    LM_ERR("%s: Exception:\n%s\n", APP_NAME, error_msg == NULL ? "(no info)" : error_msg);

}

void ThrowNewException(JNIEnv *env, char *fmt, ...)
{
    va_list ap;
    char buf[1024];

    memset(buf, 0, sizeof(char));

    va_start(ap, fmt);
    vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);

    (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), buf);
}

void handle_VM_init_failure(int res)
{
    switch(res)
    {
	    case -1:
		LM_ERR("%s: Couldn't initialize Java VM: unknown error\n", APP_NAME);
		break;
	    case -2:
		LM_ERR("%s: Couldn't initialize Java VM: thread detached from the VM\n", APP_NAME);
	        break;
	    case -3:
		LM_ERR("%s: Couldn't initialize Java VM: JNI version error\n", APP_NAME);
		break;
	    case -4:
		LM_ERR("%s: Couldn't initialize Java VM: not enough memory\n", APP_NAME);
		break;
	    case -5:
		LM_ERR("%s: Couldn't initialize Java VM: VM already created\n", APP_NAME);
		break;
	    case -6:
		LM_ERR("%s: Couldn't initialize Java VM: invalid arguments\n", APP_NAME);
		break;
	    default:
		LM_ERR("%s: Couldn't initialize Java VM. Error code: %d\n", APP_NAME, res);
		break;
    }
}


