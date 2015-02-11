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


#include "../../action.h"
#include "../../mem/mem.h"
#include "../../sr_module.h"
#include "../../dset.h"
#include "../../parser/msg_parser.h"

#include <jni.h>

#include "global.h"
#include "utils.h"
#include "app_java_mod.h"
#include "java_iface.h"
#include "java_support.h"
#include "java_native_methods.h"
#include "java_msgobj.h"

jobject *fill_sipmsg_object(JNIEnv *env, struct sip_msg *msg)
{
    jobject *SipMsgInstance;
    jclass SipMsgClass;
    jmethodID SipMsgClassID;
    jfieldID fid;
    jstring jStrParam;

    SipMsgInstance = (jobject *)pkg_malloc(sizeof(jobject));
    if (!SipMsgInstance)
    {
	LM_ERR("%s: pkg_malloc() has failed. Not enough memory!\n", APP_NAME);
	return NULL;
    }
    memset(SipMsgInstance, 0, sizeof(jobject));

    SipMsgClass = (*env)->FindClass(env, "org/siprouter/SipMsg");
    if (!SipMsgClass || (*env)->ExceptionCheck(env))
    {
        handle_exception();
	pkg_free(SipMsgInstance);
        return NULL;
    }

    SipMsgClassID = (*env)->GetMethodID(env, SipMsgClass, "<init>", "()V");
    if (!SipMsgClassID || (*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // calling constructor
    (*SipMsgInstance) = (*env)->NewObject(env, SipMsgClass, SipMsgClassID);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->id => SipMsg.id 
    fid = (*env)->GetFieldID(env, SipMsgClass, "id", "I");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
	LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.id\n", APP_NAME);

        return NULL;
    }
    (*env)->SetIntField(env, SipMsgInstance, fid, msg->id);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->pid => SipMsg.pid
    fid = (*env)->GetFieldID(env, SipMsgClass, "pid", "I");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
	LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.pid\n", APP_NAME);
        return NULL;
    }
    (*env)->SetIntField(env, SipMsgInstance, fid, msg->pid);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->eoh => SipMsg.eoh
    fid = (*env)->GetFieldID(env, SipMsgClass, "eoh", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
	LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.eoh\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, msg->eoh);
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    // msg->unparsed => SipMsg.unparsed
    fid = (*env)->GetFieldID(env, SipMsgClass, "unparsed", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
	LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.unparsed\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, msg->unparsed);
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    // msg->buf => SipMsg.buf
    fid = (*env)->GetFieldID(env, SipMsgClass, "buf", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.buf\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, msg->buf);
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    // msg->len => SipMsg.len
    fid = (*env)->GetFieldID(env, SipMsgClass, "len", "I");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.len\n", APP_NAME);
        return NULL;
    }
    (*env)->SetIntField(env, SipMsgInstance, fid, msg->len);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->new_uri => SipMsg.new_uri
    fid = (*env)->GetFieldID(env, SipMsgClass, "new_uri", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.new_uri\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, msg->new_uri.len <= 0 ? "" : msg->new_uri.s);
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    // msg->dst_uri => SipMsg.dst_uri
    fid = (*env)->GetFieldID(env, SipMsgClass, "dst_uri", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.dst_uri\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, msg->dst_uri.len <= 0 ? "" : msg->dst_uri.s);
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    // msg->parsed_orig_ruri_ok => SipMsg.parsed_orig_ruri_ok
    fid = (*env)->GetFieldID(env, SipMsgClass, "parsed_orig_ruri_ok", "I");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.parsed_orig_ruri_ok\n", APP_NAME);
        return NULL;
    }
    (*env)->SetIntField(env, SipMsgInstance, fid, msg->parsed_orig_ruri_ok);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->add_to_branch_s => SipMsg.add_to_branch_s
    fid = (*env)->GetFieldID(env, SipMsgClass, "add_to_branch_s", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.add_to_branch_s\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, (msg->add_to_branch_len <= 0 || msg->add_to_branch_s == NULL) ? "" : strdup(msg->add_to_branch_s));
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    // msg->add_to_branch_len => SipMsg.add_to_branch_len
    fid = (*env)->GetFieldID(env, SipMsgClass, "add_to_branch_len", "I");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.add_to_branch_len\n", APP_NAME);
        return NULL;
    }
    (*env)->SetIntField(env, SipMsgInstance, fid, msg->add_to_branch_len);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->hash_index => SipMsg.hash_index
    fid = (*env)->GetFieldID(env, SipMsgClass, "hash_index", "I");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.hash_index\n", APP_NAME);
        return NULL;
    }
    (*env)->SetIntField(env, SipMsgInstance, fid, msg->hash_index);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->msg_flags => SipMsg.msg_flags
    fid = (*env)->GetFieldID(env, SipMsgClass, "msg_flags", "I");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.msg_flags\n", APP_NAME);
        return NULL;
    }
    (*env)->SetIntField(env, SipMsgInstance, fid, msg->msg_flags);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }

    // msg->set_global_address => SipMsg.set_global_address
    fid = (*env)->GetFieldID(env, SipMsgClass, "set_global_address", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.set_global_address\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, (msg->set_global_address.len <= 0 || msg->set_global_address.s == NULL) ? "" : msg->set_global_address.s);
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    // msg->set_global_port => SipMsg.set_global_port
    fid = (*env)->GetFieldID(env, SipMsgClass, "set_global_port", "Ljava/lang/String;");
    if (!fid)
    {
	(*env)->ExceptionClear(env);
        LM_ERR("%s: Can't find symbol org.siprouter.SipMsg.set_global_port\n", APP_NAME);
        return NULL;
    }
    jStrParam = (*env)->NewStringUTF(env, (msg->set_global_port.len <= 0 || msg->set_global_port.s == NULL) ? "" : msg->set_global_port.s);
    (*env)->SetObjectField(env, SipMsgInstance, fid, jStrParam);
    if ((*env)->ExceptionCheck(env))
    {
        handle_exception();
        return NULL;
    }
    (*env)->DeleteLocalRef(env, jStrParam);

    return SipMsgInstance;
}
