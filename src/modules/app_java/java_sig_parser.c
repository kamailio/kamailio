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

#include <string.h>
#include <errno.h>
#include <float.h>

#include "../../str.h"
#include "../../sr_module.h"

#include <jni.h>

#include "global.h"
#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"
#include "java_native_methods.h"
#include "java_sig_parser.h"

int is_sig_allowed(char *s)
{
    if (s == NULL || strlen(s) < 1)
	return 0;

    if (!strcmp(s, " ") || !strcmp(s, "\n") || !strcmp(s, "\r") || !strcmp(s, "\t"))
    {
	LM_ERR("%s: signature error: '%s' contains whitespaces or any unparsable chars.\n", APP_NAME, s);
	return 0;
    }

//    LM_ERR("s='%s', strlen(s)=%d\n", s, strlen(s));

    if (strlen(s) == 1)			// signature is single modifier (primitive)
    {
	if (!strcmp(s, "["))		// invalid signature modifier definition
	{
	    LM_ERR("%s: signature error: '%s': no type of array specified.\n", APP_NAME, s);
	    return 0;
	}

	if (!strcmp(s, "L"))		// invalid signature modifier definition
	{
	    LM_ERR("%s: signature error '%s': no object specified.\n", APP_NAME, s);
	    return 0;
	}

#ifndef JAVA_INV_SUPP_TYPE_VOID
	if (!strcmp(s, "V"))
	{
	    LM_ERR("%s: signature error '%s': no object specified.\n", APP_NAME, s);
	    return 0;
	}
#endif

    }
    else				// a complex signature (object)
    {
#ifndef JAVA_INV_SUPP_TYPE_ARRAYS
	if (strcmp(s, "[") > 0)
	{
	    LM_ERR("%s: signature error: '%s' denotes array which isn't supported yet.\n", APP_NAME, s);
	    return 0;
	}
#endif


	if (strrchr(&s[0], 'L') > 0)
	{
#ifndef JAVA_INV_SUPP_TYPE_OBJECTS
	    LM_ERR("%s: signature error: '%s' denotes object which isn't supported yet.\n", APP_NAME, s);
	    return 0;
#else
	    int f = 0;
#ifdef	JAVA_INV_SUPP_TYPE_BOOLEAN
	    if (!strcmp(s, "Ljava/lang/Boolean;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_BYTE
	    if (!strcmp(s, "Ljava/lang/Byte;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_CHARACTER
	    if (!strcmp(s, "Ljava/lang/Character;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_DOUBLE
	    if (!strcmp(s, "Ljava/lang/Double;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_FLOAT
	    if (!strcmp(s, "Ljava/lang/Float;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_INTEGER
	    if (!strcmp(s, "Ljava/lang/Integer;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_LONG
	    if (!strcmp(s, "Ljava/lang/Long;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_SHORT
	    if (!strcmp(s, "Ljava/lang/Short;"))
		f = 1;
#endif
#ifdef	JAVA_INV_SUPP_TYPE_STRING
	    if (!strcmp(s, "Ljava/lang/String;"))
		f = 1;
#endif
	    if (f == 0)
	    {
		LM_ERR("%s: signature '%s' isn't supported yet.\n", APP_NAME, s);
		return 0;
	    }
#endif
	}

    }

    return 1;
}



static char *get_conv_err_str(int en)
{
    switch(en)
    {
	case EINVAL:	return "The value of base constant is not supported or no conversion could be performed";
	case ERANGE:	return "The given string was out of range; the value converted has been clamped.";
	default:	return "General parse error";
    }
}


/* explaination of jvalue fields:
typedef union jvalue {
    jboolean z;
    jbyte    b;
    jchar    c;
    jshort   s;
    jint     i;
    jlong    j;
    jfloat   f;
    jdouble  d;
    jobject  l;
} jvalue;
*/

jvalue *get_value_by_sig_type(char *sig, char *pval)
{
    char *endptr;
    char scptr;
    int siptr;
    long slptr;
    short ssptr;
    double sdptr;
    float sfptr;
    jstring sjptr;
    jvalue *ret;

    ret = (jvalue *)pkg_malloc(sizeof(jvalue));
    if (!ret)
    {
	LM_ERR("%s: pkg_malloc() has failed. Not enouph memory!\n", APP_NAME);
	return NULL;
    }

    if (sig == NULL || strlen(sig) <= 0)
    {
	LM_ERR("%s: Can't process empty or NULL signature.\n", APP_NAME);
	pkg_free(ret);
	return NULL;
    }
    if (pval == NULL || strlen(pval) <= 0)
    {
	LM_ERR("%s: Can't process empty or NULL parameter value.\n", APP_NAME);
	pkg_free(ret);
	return NULL;
    }

    // boolean
    if (!strncmp(sig, "Z", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_BOOLEAN)
	|| !strcmp(sig, "Ljava/lang/Boolean;")
#endif
	) {
	    if (!strncasecmp(pval, "true", 4))
		(*ret).z = (jboolean)JNI_TRUE;
/* comment this block to avoid conversation '1' to 'true' */
	    else if (!strncmp(pval, "1", 1))
		(*ret).z = (jboolean)JNI_TRUE;
	    else if (!strncasecmp(pval, "false", 5))
		(*ret).z = (jboolean)JNI_FALSE;
/* comment this block to avoid conversation '0' to 'false' */
	    else if (!strncmp(pval, "0", 1))
		(*ret).z = (jboolean)JNI_FALSE;
	    else
	    {
    		LM_ERR("%s: Can't cast '%s' to type '%s'.\n", APP_NAME, pval, sig);
		pkg_free(ret);
		return NULL;
	    }

	    return ret;
    }
    else
    // byte
    if (!strncmp(sig, "B", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_BYTE)
	|| !strcmp(sig, "Ljava/lang/Byte;")
#endif
	) {
//	    skptr = (signed char)char2jbyte(pval);
	    sscanf(pval, "%x", &siptr);
	    if (siptr == 0 && errno != 0)
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Error: %s.\n", APP_NAME, pval, sig, get_conv_err_str(errno));
		pkg_free(ret);
                return NULL;
	    }
            if (siptr < SCHAR_MAX || siptr > SCHAR_MAX)
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Reason: overflow.", APP_NAME, pval, sig);
		pkg_free(ret);
                return NULL;
	    }

            (*ret).b = (jbyte)siptr;
	    return ret;
    }
    else
    // char
    if (!strncmp(sig, "C", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_CHARACTER)
	|| !strcmp(sig, "Ljava/lang/Character;")
#endif
	) {
	    sscanf(pval, "%c", &scptr);
	    if (scptr == 0 && errno != 0)
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Error: %s.\n", APP_NAME, pval, sig, get_conv_err_str(errno));
		pkg_free(ret);
                return NULL;
	    }
            if (scptr < CHAR_MIN || scptr > CHAR_MAX)	// overflow
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Reason: overflow.", APP_NAME, pval, sig);
		pkg_free(ret);
                return NULL;
	    }

            (*ret).c = (jchar)scptr;
	    return ret;
    }
    else
    // double
    if (!strncmp(sig, "D", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_DOUBLE)
	|| !strcmp(sig, "Ljava/lang/Double;")
#endif
	) {
	    sdptr = (double)strtod(pval, &endptr);
	    if ((sdptr == 0 && errno != 0) || (pval == endptr))
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Error: %s.\n", APP_NAME, pval, sig, get_conv_err_str(errno));
		pkg_free(ret);
                return NULL;
	    }
            if (sdptr < LLONG_MIN || sdptr > LLONG_MAX)	// overflow
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Reason: overflow.", APP_NAME, pval, sig);
		pkg_free(ret);
                return NULL;
	    }

	    (*ret).d = (jdouble)sdptr;
	    return ret;
    }
    else
    // float
    if (!strncmp(sig, "F", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_FLOAT)
	|| !strcmp(sig, "Ljava/lang/Float;")
#endif
	) {
	    sfptr = (float)strtof(pval, &endptr);
	    if ((sfptr == 0 && errno != 0) || (pval == endptr))
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Error: %s.\n", APP_NAME, pval, sig, get_conv_err_str(errno));
		pkg_free(ret);
                return NULL;
	    }
            if (sfptr < FLT_MIN || sfptr > FLT_MAX)	// overflow
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Reason: overflow.", APP_NAME, pval, sig);
		pkg_free(ret);
                return NULL;
	    }

	    (*ret).f = (jfloat)sfptr;
	    return ret;
    }
    else
    // integer
    if (!strncmp(sig, "I", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_INTEGER)
	|| !strcmp(sig, "Ljava/lang/Integer;")
#endif
	) {
	    slptr = strtol(pval, &endptr, 10);
	    if ((slptr == 0 && errno != 0) || (pval == endptr))
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Error: %s.\n", APP_NAME, pval, sig, get_conv_err_str(errno));
		pkg_free(ret);
                return NULL;
	    }
	    if (slptr < INT_MIN || slptr > INT_MAX)	// overflow
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Reason: overflow.", APP_NAME, pval, sig);
		pkg_free(ret);
                return NULL;
	    }

	    (*ret).i = (jint)slptr;
	    return ret;
    }
    else
    // long
    if (!strncmp(sig, "J", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_LONG)
	|| !strcmp(sig, "Ljava/lang/Long;")
#endif
	) {
	    slptr = (long)strtol(pval, &endptr, 10);
	    if ((slptr == 0 && errno != 0) || (pval == endptr))
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Error: %s.\n", APP_NAME, pval, sig, get_conv_err_str(errno));
		pkg_free(ret);
                return NULL;
	    }
	    if (slptr < LONG_MIN || slptr > LONG_MAX)	// overflow
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Reason: overflow.", APP_NAME, pval, sig);
		pkg_free(ret);
                return NULL;
	    }

	    (*ret).j = (jlong)slptr;
	    return ret;
    }
    else
    // short
    if (!strncmp(sig, "S", 1)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_SHORT)
	|| !strcmp(sig, "Ljava/lang/Short;")
#endif
	) {
	    ssptr = (short)strtod(pval, &endptr);
	    if ((ssptr == 0 && errno != 0) || (pval == endptr))
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Error: %s.\n", APP_NAME, pval, sig, get_conv_err_str(errno));
		pkg_free(ret);
                return NULL;
	    }
	    if (ssptr < SHRT_MIN || ssptr > SHRT_MAX)	// overflow
	    {
		LM_ERR("%s: Can't cast '%s' to type '%s'. Reason: overflow.", APP_NAME, pval, sig);
		pkg_free(ret);
                return NULL;
	    }

	    (*ret).s = (jshort)ssptr;
	    return ret;
    }
    // String (object)
#if defined(JAVA_INV_SUPP_TYPE_OBJECTS) && defined(JAVA_INV_SUPP_TYPE_STRING)
    else
    if (!strcmp(sig, "Ljava/lang/String;"))
    {
	    sjptr = (*env)->NewStringUTF(env, pval);
	    if ((*env)->ExceptionCheck(env))
	    {
		pkg_free(ret);
		handle_exception();
		return NULL;
	    }
/*
	    if (pval != NULL && sjptr == NULL)
	    {
		pkg_free(ret);
                return NULL;
	    }
*/
	    (*ret).l = (jstring)sjptr;
	    return ret;
    }
#endif
#ifdef JAVA_INV_SUPP_TYPE_VOID
    else
    if (!strncmp(sig, "V", 1))
    {
	pkg_free(ret);
	return NULL;
    }
#endif
    else
    {
	// unknown sig
	LM_ERR("%s: Can't cast '%s' to signature '%s'\n", APP_NAME, pval, sig);
	pkg_free(ret);
	return NULL;
    }

    return NULL;
}

