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
#include "../../ip_addr.h"
#include "../../flags.h"

#include <jni.h>

#include "global.h"
#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"
#include "java_msgobj.h"
#include "java_native_methods.h"
#include "java_sig_parser.h"


//// native methods ////

/*
    java: native void LM_XXXXXX(Params XXXX);
    c: JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1XXXXXX(JNIEnv *jenv, jobject this, Params XXXX)

    Why (for example) Java_Kamailio_LM_1ERR but not Java_Kamailio_LM_ERR? 
    See explaination here: http://qscribble.blogspot.ca/2012/04/underscores-in-jni-method-names.html

    Also, from here: http://192.9.162.55/docs/books/jni/html/design.html
    The JNI adopts a simple name-encoding scheme to ensure that all Unicode characters 
    translate into valid C function names. The underscore ("_") character separates the 
    components of fully qualified class names. Because a name or type descriptor never 
    begins with a number, we can use _0, ..., _9 for escape sequences, as illustrated below:
    +-------------------+------------------------------------+
    |  Escape Sequence  |            Denotes                 |
    +-------------------+------------------------------------+
    |	_0XXXX          |  a Unicode character XXXX          |
    |	_1              |  the character "_"                 |
    |	_2              |  the character ";" in descriptors  |
    |	_3              |  the character "[" in descriptors  |
    +-------------------+------------------------------------+

*/

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_ERR(Ljava/lang/String;)V
    Prototype: public static native void LM_ERR(String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1ERR(JNIEnv *jenv, jobject this, jstring js)
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

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_WARN(Ljava/lang/String;)V
    Prototype: public static native void LM_WARN(String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1WARN(JNIEnv *jenv, jobject this, jstring js)
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

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_NOTICE(Ljava/lang/String;)V
    Prototype: public static native void LM_NOTICE(String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1NOTICE(JNIEnv *jenv, jobject this, jstring js)
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

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_INFO(Ljava/lang/String;)V
    Prototype: public static native void LM_INFO(String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1INFO(JNIEnv *jenv, jobject this, jstring js)
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

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_DBG(Ljava/lang/String;)V
    Prototype: public static native void LM_DBG(String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1DBG(JNIEnv *jenv, jobject this, jstring js)
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

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_CRIT(Ljava/lang/String;)V
    Prototype: public static native void LM_CRIT(String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1CRIT(JNIEnv *jenv, jobject this, jstring js)
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


#ifdef LM_ALERT
/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_ALERT(Ljava/lang/String;)V
    Prototype: public static native void LM_ALERT(String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1ALERT(JNIEnv *jenv, jobject this, jstring js)
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


/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_GEN2(ILjava/lang/String;)V
    Prototype: public static native void LM_GEN1(int logLevel, String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1GEN1(JNIEnv *jenv, jobject this, jint ll, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_GEN1((int)ll, "%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: LM_GEN2(IILjava/lang/String;)V
    Prototype: public static native void LM_GEN2(int logLevel, int logFacility, String s);
*/
JNIEXPORT void JNICALL Java_org_siprouter_NativeMethods_LM_1GEN2(JNIEnv *jenv, jobject this, jint ll, jint lf, jstring js)
{
    const char *s;
    jboolean iscopy;

    s = (*jenv)->GetStringUTFChars(jenv, js, &iscopy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return;
    }

    LM_GEN2((int)ll, (int)lf, "%s", s == NULL ? "null\n" : s);

    (*jenv)->ReleaseStringUTFChars(jenv, js, s);
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: NativeMethods
    Method: KamExec(Ljava/lang/String;[Ljava/lang/String;)I
    Prototype: public static native int KamExec(String fname, String... params);
*/
JNIEXPORT jint JNICALL Java_org_siprouter_NativeMethods_KamExec(JNIEnv *jenv, jobject this, jstring jfname, jobjectArray strArrParams)
{
    int retval;
    char *fname;
    int argc;
    jsize pc;
    int i;
    char *argv[MAX_ACTIONS];
    jboolean is_copy;
    jstring strp;
    char *strc;

    if (jfname == NULL)
    {
	LM_ERR("%s: KamExec() required at least 1 argument (function name)\n", APP_NAME);
	return -1;
    }

    fname = (char *)(*jenv)->GetStringUTFChars(jenv, jfname, &is_copy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return -1;
    }

    memset(argv, 0, MAX_ACTIONS * sizeof(char *));
    argc = 0;

    pc = (*jenv)->GetArrayLength(jenv, strArrParams);
    if (pc >= 6)
    {
	pc = 6;
    }

    for (i=0; i<pc; i++)
    {
	strp = (jstring)(*jenv)->GetObjectArrayElement(jenv, strArrParams, i);
	if ((*jenv)->ExceptionCheck(jenv))
	{
    	    handle_exception();
    	    return -1;
	}

	strc = (char *)(*jenv)->GetStringUTFChars(jenv, strp, &is_copy);
	if ((*jenv)->ExceptionCheck(jenv))
	{
    	    handle_exception();
    	    return -1;
	}

	if (strc)
	{
	    argv[argc++] = strc;
	}
    }
    
    retval = KamExec(jenv, fname, argc, argv);

    (*jenv)->ReleaseStringUTFChars(jenv, jfname, fname);

    return (jint)retval;
}

int KamExec(JNIEnv *jenv, char *fname, int argc, char **argv)
{
    sr31_cmd_export_t *fexport;
    unsigned mod_ver;
    int rval;
    int mod_type;
    struct action *act;
    struct run_act_ctx ra_ctx;
    int i;

    if (!msg)
	return -1;

    fexport = find_export_record(fname, argc, 0, &mod_ver);
    if (!fexport)
    {
	LM_ERR("%s: KamExec(): '%s' - no such function\n", APP_NAME, fname);
        return -1;
    }

    /* check fixups */
    if (force_cmd_exec == 0 && fexport->fixup != NULL && fexport->free_fixup == NULL)
    {
        LM_ERR("%s: KamExec(): function '%s' has fixup - cannot be used\n", APP_NAME, fname);
	return -1;
    }

    switch(fexport->param_no)
    {
    	case 0:			mod_type = MODULE0_T;	break;
	case 1:			mod_type = MODULE1_T;	break;
	case 2:			mod_type = MODULE2_T;	break;
	case 3:			mod_type = MODULE3_T;	break;
	case 4:			mod_type = MODULE4_T;	break;
	case 5:			mod_type = MODULE5_T;	break;
	case 6:			mod_type = MODULE6_T;	break;
	case VAR_PARAM_NO:	mod_type = MODULEX_T;	break;
	default:
		LM_ERR("%s: KamExec(): unknown/bad definition for function '%s' (%d params)\n", APP_NAME, fname, fexport->param_no);
		return -1;
    }


    act = mk_action(mod_type, (argc+2),			/* number of (type, value) pairs */
                	MODEXP_ST, fexport,		/* function */
                	NUMBER_ST, argc,		/* parameter number */
			STRING_ST, argv[0],		/* param. 1 */
			STRING_ST, argv[1],		/* param. 2 */
			STRING_ST, argv[2],		/* param. 3 */
			STRING_ST, argv[3],		/* param. 4 */
			STRING_ST, argv[4],		/* param. 5 */
			STRING_ST, argv[5]		/* param. 6 */
                   );

    if (!act)
    {
	LM_ERR("%s: KamExec(): action structure couldn't be created\n", APP_NAME);
	return -1;
    }


    /* handle fixups */
    if (fexport->fixup)
    {
        if (argc == 0)
	{
            rval = fexport->fixup(0, 0);
            if (rval < 0)
	    {
		LM_ERR("%s: KamExec(): (no params) Error in fixup (0) for '%s'\n", APP_NAME, fname);
                return -1;
            }
        }
	else
	{
	    for (i=0; i<=argc; i++)
	    {
		if (act->val[i+2].u.data != 0x0)
		{
        	    rval = fexport->fixup(&(act->val[i+2].u.data), i+1);
        	    if (rval < 0)
		    {
			LM_ERR("%s: KamExec(): (params: %d) Error in fixup (%d) for '%s'\n", APP_NAME, argc, i+1, fname);
            		return -1;
        	    }
        	    act->val[i+2].type = MODFIXUP_ST;
		}
	    }
        }
    }

    init_run_actions_ctx(&ra_ctx);
    rval = do_action(&ra_ctx, act, msg);

    /* free fixups */
    if (fexport->free_fixup)
    {
	for (i=0; i<=argc; i++)
	{
	    if ((act->val[i+2].type == MODFIXUP_ST) && (act->val[i+2].u.data))
	    {
		fexport->free_fixup(&(act->val[i+2].u.data), i+1);
	    }
	}
    }

    pkg_free(act);

    return rval;
}


/*
    *** Java API ***
    Package: org.siprouter
    Class: SipMsg
    Method: ParseSipMsg()Lorg/siprouter/SipMsg;
    Prototype: public static native org.siprouter.SipMsg ParseSipMsg();
*/
JNIEXPORT jobject JNICALL Java_org_siprouter_SipMsg_ParseSipMsg(JNIEnv *jenv, jobject this)
{
    if (!msg)
	return NULL;

    return fill_sipmsg_object(jenv, msg);
}


/*
    *** Java API ***
    Package: org.siprouter
    Class: SipMsg
    Method: getMsgType()Ljava/org/String;
    Prototype: public static native String getMsgType();
*/
JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getMsgType(JNIEnv *jenv, jobject this)
{
    char *cs;
    jstring js;

    if (!msg)
	return NULL;

    switch ((msg->first_line).type)
    {
        case SIP_REQUEST:
            cs = "SIP_REQUEST";
            break;

        case SIP_REPLY:
            cs = "SIP_REPLY";
            break;

        default:
	    cs = "SIP_INVALID";
	    break;
    }

    js = (*jenv)->NewStringUTF(jenv, cs);
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    return js;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: SipMsg
    Method: getStatus()Ljava/org/String;
    Prototype: public static native String getStatus();
*/
JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getStatus(JNIEnv *jenv, jobject this)
{
    str *cs;
    jstring js;

    if (!msg)
	return NULL;

    if ((msg->first_line).type != SIP_REQUEST)
    {
	LM_ERR("%s: getStatus(): Unable to fetch status. Error: Not a request message - no method available.\n", APP_NAME);
        return NULL;
    }

    cs = &((msg->first_line).u.request.method);

    js = (*jenv)->NewStringUTF(jenv, (cs && cs->s && cs->len > 0) ? cs->s : "");
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    return js;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: SipMsg
    Method: getRURI()Ljava/org/String;
    Prototype: public static native String getRURI();
*/
JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getRURI(JNIEnv *jenv, jobject this)
{
    str *cs;
    jstring js;

    if (!msg)
	return NULL;

    if ((msg->first_line).type != SIP_REQUEST)
    {
	LM_ERR("%s: getRURI(): Unable to fetch ruri. Error: Not a request message - no method available.\n", APP_NAME);
        return NULL;
    }

    cs = &((msg->first_line).u.request.uri);

    js = (*jenv)->NewStringUTF(jenv, (cs && cs->s && cs->len > 0) ? cs->s : "");
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    return js;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: SipMsg
    Method: getSrcAddress()Lorg/siprouter/IPPair;
    Prototype: public static native org.siprouter.IPPair getSrcAddress();
*/
JNIEXPORT jobject JNICALL Java_org_siprouter_SipMsg_getSrcAddress(JNIEnv *jenv, jobject this)
{
    jclass ippair_cls;
    jmethodID ippair_cls_id;
    jobject ippair_cls_instance;

    char *ip;
    jstring jip;
    int port;

    if (!msg)
	return NULL;

    ippair_cls = (*jenv)->FindClass(jenv, "org/siprouter/IPPair");
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    ippair_cls_id = (*jenv)->GetMethodID(jenv, ippair_cls, "<init>", "(Ljava/lang/String;I)V");
    if (!ippair_cls_id || (*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    ip = ip_addr2a(&msg->rcv.src_ip);
    if (!ip)
    {
	LM_ERR("%s: getSrcAddress(): Unable to fetch src ip address.\n", APP_NAME);
	return NULL;
    }
    jip = (*jenv)->NewStringUTF(jenv, ip);
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    port = msg->rcv.src_port;
    if (port == 0x0)
    {
	LM_ERR("%s: getSrcAddress(): Unable to fetch src port.\n", APP_NAME);
	return NULL;
    }

    // calling constructor
    ippair_cls_instance = (*jenv)->NewObject(jenv, ippair_cls, ippair_cls_id, (jstring)jip, (jint)port);
    if (!ippair_cls_instance || (*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    return ippair_cls_instance;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: SipMsg
    Method: getDstAddress()Lorg/siprouter/IPPair;
    Prototype: public static native org.siprouter.IPPair getDstAddress();
*/
JNIEXPORT jobject JNICALL Java_org_siprouter_SipMsg_getDstAddress(JNIEnv *jenv, jobject this)
{
    jclass ippair_cls;
    jmethodID ippair_cls_id;
    jobject ippair_cls_instance;

    char *ip;
    jstring jip;
    int port;

    if (!msg)
	return NULL;

    ippair_cls = (*jenv)->FindClass(jenv, "org/siprouter/IPPair");
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    ippair_cls_id = (*jenv)->GetMethodID(jenv, ippair_cls, "<init>", "(Ljava/lang/String;I)V");
    if (!ippair_cls_id || (*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    ip = ip_addr2a(&msg->rcv.dst_ip);
    if (!ip)
    {
	LM_ERR("%s: getDstAddress(): Unable to fetch src ip address.\n", APP_NAME);
	return NULL;
    }
    jip = (*jenv)->NewStringUTF(jenv, ip);
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    port = msg->rcv.dst_port;
    if (port == 0x0)
    {
	LM_ERR("%s: getDstAddress(): Unable to fetch src port.\n", APP_NAME);
	return NULL;
    }

    // calling constructor
    ippair_cls_instance = (*jenv)->NewObject(jenv, ippair_cls, ippair_cls_id, (jstring)jip, (jint)port);
    if (!ippair_cls_instance || (*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    return ippair_cls_instance;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: SipMsg
    Method: getBuffer()Ljava/org/String;
    Prototype: public static native String getBuffer();
*/
JNIEXPORT jstring JNICALL Java_org_siprouter_SipMsg_getBuffer(JNIEnv *jenv, jobject this)
{
    jstring js;

    if (!msg)
	return NULL;

    if ((msg->first_line).type != SIP_REQUEST)
    {
	LM_ERR("%s: getRURI(): Unable to fetch ruri. Error: Not a request message - no method available.\n", APP_NAME);
        return NULL;
    }

    js = (*jenv)->NewStringUTF(jenv, msg->buf ? msg->buf : "");
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return NULL;
    }

    return js;
}






///// Core Functions /////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: seturi(Ljava/org/String;)I
    Prototype: public static native int seturi(String uri);
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_seturi(JNIEnv *jenv, jobject this, jstring juri)
{
    return cf_seturi(jenv, this, juri, "seturi");
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: rewriteuri(Ljava/org/String;)I
    Prototype: public static native int rewriteuri(String uri);
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_rewriteuri(JNIEnv *jenv, jobject this, jstring juri)
{
    return cf_seturi(jenv, this, juri, "rewriteuri");
}

/* wrapped function */
jint cf_seturi(JNIEnv *jenv, jobject this, jstring juri, char *fname)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;
    jboolean is_copy;
    char *curi;

    if (!msg)
    {
	LM_ERR("%s: %s: Can't process, msg=NULL\n", APP_NAME, fname);
	return -1;
    }

    curi = (char *)(*jenv)->GetStringUTFChars(jenv, juri, &is_copy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
    	return -1;
    }

    memset(&act, 0, sizeof(act));
    act.type = SET_URI_T;
    act.val[0].type = STRING_ST;
    act.val[0].u.str.s = curi;
    act.val[0].u.str.len = strlen(curi);
    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);
    (*jenv)->ReleaseStringUTFChars(jenv, juri, curi);
    return (jint)retval;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: add_local_rport()I
    Prototype: public static native int add_local_rport();
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_add_1local_1rport(JNIEnv *jenv, jobject this)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;

    if (!msg)
    {
	LM_ERR("%s: add_local_rport: Can't process, msg=NULL\n", APP_NAME);
	return -1;
    }

    memset(&act, 0, sizeof(act));
    act.type = ADD_LOCAL_RPORT_T;
    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);
    return (jint)retval;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: append_branch()I
    Method(o): append_branch(Ljava/lang/String)I
    Prototype: public static native int append_branch();
    Prototype(o): public static native int append_branch(String branch);
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_append_1branch(JNIEnv *jenv, jobject this, jstring jbranch)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;
    jboolean is_copy;
    char *cbranch;

    if (!msg)
    {
	LM_ERR("%s: append_branch: Can't process, msg=NULL\n", APP_NAME);
	return -1;
    }

    memset(&act, 0, sizeof(act));
    act.type = APPEND_BRANCH_T;

    cbranch = NULL;

    if (jbranch)
    {
	cbranch = (char *)(*jenv)->GetStringUTFChars(jenv, jbranch, &is_copy);
	if ((*jenv)->ExceptionCheck(jenv))
	{
	    handle_exception();
	    return -1;
	}

	act.val[0].type = STR_ST;
	act.val[0].u.str.s = cbranch;
	act.val[0].u.str.len = strlen(cbranch);
    }

    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);

    if (cbranch)
    {
	(*jenv)->ReleaseStringUTFChars(jenv, jbranch, cbranch);
    }

    return (jint)retval;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: drop()I
    Prototype: public static native int drop();
    Returns:
	0 if action -> end of list(e.g DROP)
        > 0 to continue processing next actions
	< 0 on error
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_drop(JNIEnv *jenv, jobject this)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;

    if (!msg)
    {
	LM_ERR("%s: drop: Can't process, msg=NULL\n", APP_NAME);
	return -1;
    }

    memset(&act, 0, sizeof(act));
    act.type = DROP_T;
    act.val[0].type = NUMBER_ST;
    act.val[0].u.number = 0;
    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);
    return (jint)retval;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: force_rport()I
    Prototype: public static native int force_rport();
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_force_1rport(JNIEnv *jenv, jobject this)
{
    return cf_force_rport(jenv, this, "force_rport");
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: add_rport()I
    Prototype: public static native int add_rport();
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_add_1rport(JNIEnv *jenv, jobject this)
{
    return cf_force_rport(jenv, this, "add_rport");
}

/* wrapped function */
jint cf_force_rport(JNIEnv *jenv, jobject this, char *fname)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;

    if (!msg)
    {
	LM_ERR("%s: %s: Can't process, msg=NULL\n", APP_NAME, fname);
	return -1;
    }

    memset(&act, 0, sizeof(act));
    act.type = FORCE_RPORT_T;
    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);
    return (jint)retval;
}


/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: force_send_socket(Ljava/lang/String;I)I
    Prototype: public static native int force_send_socket(String srchost, int srcport);
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_force_1send_1socket(JNIEnv *jenv, jobject this, jstring jsrchost, jint jsrcport)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;
    jboolean is_copy;
    struct socket_id *si;
    struct name_lst *nl;

    if (!msg)
    {
	LM_ERR("%s: force_send_socket: Can't process, msg=NULL\n", APP_NAME);
	return -1;
    }

    nl = (struct name_lst *)pkg_malloc(sizeof(struct name_lst));
    if (!nl)
    {
	LM_ERR("%s: force_send_socket: pkg_malloc() has failed. Not enough memory!\n", APP_NAME);
	return -1;
    }
    
    si = (struct socket_id *)pkg_malloc(sizeof(struct socket_id));
    if (!si)
    {
	LM_ERR("%s: force_send_socket: pkg_malloc() has failed. Not enough memory!\n", APP_NAME);
	return -1;
    }
    

    memset(&act, 0, sizeof(act));
    act.type = FORCE_SEND_SOCKET_T;

    nl->name = (char *)(*jenv)->GetStringUTFChars(jenv, jsrchost, &is_copy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
        handle_exception();
        return -1;
    }
    nl->next = NULL;
    nl->flags = 0;

    si->addr_lst = nl;
    si->flags = 0;
    si->proto = PROTO_NONE;
    si->port = (int)jsrcport;
    
    act.val[0].type = SOCKETINFO_ST;
    act.val[0].u.data = si;

    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);

    (*jenv)->ReleaseStringUTFChars(jenv, jsrchost, nl->name);
    pkg_free(nl);
    pkg_free(si);
    return (jint)retval;
}


/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: forward()I
    Method(o): forward(Ljava/lang/String;I)I
    Prototype: public static native int forward();
    Prototype(o): public static native int forward(String ruri, int i);
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_forward(JNIEnv *jenv, jobject this, jstring jrurihost, jint juriport)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;
    jboolean is_copy;
    char *crurihost;

    if (!msg)
    {
	LM_ERR("%s: forward: Can't process, msg=NULL\n", APP_NAME);
	return -1;
    }

    memset(&act, 0, sizeof(act));
    act.type = FORWARD_T;

    crurihost = NULL;

    if (jrurihost)
    {
	crurihost = (char *)(*jenv)->GetStringUTFChars(jenv, jrurihost, &is_copy);
	if ((*jenv)->ExceptionCheck(jenv))
	{
    	    handle_exception();
    	    return -1;
	}

	act.val[0].type = URIHOST_ST;
	act.val[0].u.str.s = crurihost;
	act.val[0].u.str.len = strlen(crurihost);

	act.val[1].type = NUMBER_ST;
	act.val[1].u.number = (int)juriport;
    }

    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);

    if (crurihost)
    {
	(*jenv)->ReleaseStringUTFChars(jenv, jrurihost, crurihost);
    }

    return (jint)retval;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: isflagset(I)Z
    Prototype: public static native boolean isflagset(int flag);
*/
JNIEXPORT jboolean JNICALL Java_org_siprouter_CoreMethods_isflagset(JNIEnv *jenv, jobject this, jint jflag)
{
    if (!msg)
    {
	LM_ERR("%s: isflagset: Can't process, msg=NULL\n", APP_NAME);
	return -1;
    }

    return isflagset(msg, (int)jflag) == 1 ? JNI_TRUE : JNI_FALSE;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: setflag(I)V
    Prototype: public static native void setflag(int flag);
*/
JNIEXPORT void JNICALL Java_org_siprouter_CoreMethods_setflag(JNIEnv *jenv, jobject this, jint jflag)
{
    if (!msg)
    {
	LM_ERR("%s: setflag: Can't process, msg=NULL\n", APP_NAME);
	return;
    }

    setflag(msg, (int)jflag);
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: resetflag(I)V
    Prototype: public static native void resetflag(int flag);
*/
JNIEXPORT void JNICALL Java_org_siprouter_CoreMethods_resetflag(JNIEnv *jenv, jobject this, jint jflag)
{
    if (!msg)
    {
	LM_ERR("%s: resetflag: Can't process, msg=NULL\n", APP_NAME);
	return;
    }

    resetflag(msg, (int)jflag);
}


/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: revert_uri()I
    Prototype: public static native int revert_uri();
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_revert_1uri(JNIEnv *jenv, jobject this)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;

    if (!msg)
    {
	LM_ERR("%s: revert_uri: Can't process, msg=NULL\n", APP_NAME);
	return -1;
    }

    memset(&act, 0, sizeof(act));
    act.type = REVERT_URI_T;
    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);
    return (jint)retval;
}

/*
    *** Java API ***
    Package: org.siprouter
    Class: CoreMethods
    Method: route(Ljava/lang/String)I
    Prototype: public static native int route(String target);
*/
JNIEXPORT jint JNICALL Java_org_siprouter_CoreMethods_route(JNIEnv *jenv, jobject this, jstring jtarget)
{
    struct action act;
    struct run_act_ctx ra_ctx;
    int retval;
    jboolean is_copy;
    char *ctarget;

    ctarget = (char *)(*jenv)->GetStringUTFChars(jenv, jtarget, &is_copy);
    if ((*jenv)->ExceptionCheck(jenv))
    {
	handle_exception();
	return -1;
    }

    retval = route_lookup(&main_rt, ctarget);

    if (retval == -1)	// route index lookup failed.
    {
	LM_ERR("%s: route: failed to find route name '%s'\n", APP_NAME, ctarget);
	(*jenv)->ReleaseStringUTFChars(jenv, jtarget, ctarget);
	return -1;
    }

    act.type = ROUTE_T;
    act.val[0].type = NUMBER_ST;
    act.val[0].u.number = retval;

    init_run_actions_ctx(&ra_ctx);
    retval = do_action(&ra_ctx, &act, msg);

    (*jenv)->ReleaseStringUTFChars(jenv, jtarget, ctarget);

    return retval;
}

