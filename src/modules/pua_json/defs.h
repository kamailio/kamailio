/*
 * PUA_JSON module
 *
 * Copyright (C) 2010-2014 2600Hz
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 */

#ifndef _PUA_JSON_DEFS_H_
#define _PUA_JSON_DEFS_H_

#define BLF_MAX_DIALOGS 8
#define BLF_JSON_PRES		"Presentity"
#define BLF_JSON_PRES_USER	"Presentity-User"
#define BLF_JSON_PRES_REALM	"Presentity-Realm"
#define BLF_JSON_FROM      	"From"
#define BLF_JSON_FROM_USER 	"From-User"
#define BLF_JSON_FROM_REALM	"From-Realm"
#define BLF_JSON_FROM_URI	"From-URI"
#define BLF_JSON_TO        	"To"
#define BLF_JSON_TO_USER 	"To-User"
#define BLF_JSON_TO_REALM	"To-Realm"
#define BLF_JSON_TO_URI		"To-URI"
#define BLF_JSON_CALLID    	"Call-ID"
#define BLF_JSON_TOTAG     	"To-Tag"
#define BLF_JSON_FROMTAG   	"From-Tag"
#define BLF_JSON_STATE     	"State"
#define BLF_JSON_USER      	"User"
#define BLF_JSON_QUEUE     	"Queue"
#define BLF_JSON_EXPIRES	"Expires"
#define BLF_JSON_APP_NAME       "App-Name"
#define BLF_JSON_APP_VERSION    "App-Version"
#define BLF_JSON_NODE           "Node"
#define BLF_JSON_SERVERID       "Server-ID"
#define BLF_JSON_EVENT_CATEGORY "Event-Category"
#define BLF_JSON_EVENT_NAME     "Event-Name"
#define BLF_JSON_TYPE           "Type"
#define BLF_JSON_MSG_ID         "Msg-ID"
#define BLF_JSON_DIRECTION      "Direction"

#define BLF_JSON_CONTACT   	"Contact"
#define BLF_JSON_EVENT_PKG      "Event-Package"
#define MWI_JSON_WAITING        "Messages-Waiting"
#define MWI_JSON_VOICE_MESSAGE  "MWI-Voice-Message"
#define MWI_JSON_NEW            "Messages-New"
#define MWI_JSON_SAVED          "Messages-Saved"
#define MWI_JSON_URGENT         "Messages-Urgent"
#define MWI_JSON_URGENT_SAVED   "Messages-Urgent-Saved"
#define MWI_JSON_ACCOUNT        "Message-Account"
#define MWI_JSON_FROM      	"From"
#define MWI_JSON_TO        	"To"

#define TO_TAG_BUFFER_SIZE 128
#define FROM_TAG_BUFFER_SIZE 128
#define SENDER_BUFFER_SIZE 1024
#define DIALOGINFO_BODY_BUFFER_SIZE 8192
#define MWI_BODY_BUFFER_SIZE 2048
#define PRESENCE_BODY_BUFFER_SIZE 4096

#define MWI_BODY_VOICE_MESSAGE "Messages-Waiting: %.*s\r\nMessage-Account: %.*s\r\nVoice-Message: %.*s\r\n"
#define MWI_BODY_NO_VOICE_MESSAGE "Messages-Waiting: %.*s\r\nMessage-Account: %.*s\r\n"
#define MWI_BODY             "Messages-Waiting: %.*s\r\nMessage-Account: %.*s\r\nVoice-Message: %.*s/%.*s (%.*s/%.*s)\r\n"
#define PRESENCE_BODY        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" xmlns:dm=\"urn:ietf:params:xml:ns:pidf:data-model\" xmlns:rpid=\"urn:ietf:params:xml:ns:pidf:rpid\" xmlns:c=\"urn:ietf:params:xml:ns:pidf:cipid\" entity=\"%s\"> \
<tuple xmlns=\"urn:ietf:params:xml:ns:pidf\" id=\"%s\">\
<status>\
<basic>%s</basic>\
</status>\
</tuple>\
<note xmlns=\"urn:ietf:params:xml:ns:pidf\">%s</note>\
<dm:person xmlns:dm=\"urn:ietf:params:xml:ns:pidf:data-model\" xmlns:rpid=\"urn:ietf:params:xml:ns:pidf:rpid\" id=\"1\">\
<rpid:activities>%s</rpid:activities>\
<dm:note>%s</dm:note>\
</dm:person>\
</presence>"

#define DIALOGINFO_EMPTY_BODY "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\"1\" state=\"full\" entity=\"%.*s\"> \
<dialog call-id=\"76001e23e09704ea9e1257ebea85e1f3\" direction=\"initiator\">\
<state>terminated</state>\
</dialog>\
</dialog-info>"

#define LOCAL_TAG "local-tag=\"%.*s\""
#define REMOTE_TAG "remote-tag=\"%.*s\""

#define DIALOGINFO_BODY "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\"1\" state=\"full\" entity=\"%.*s\">\
<dialog id=\"%.*s\" call-id=\"%.*s\" %.*s %.*s direction=\"%.*s\">\
<state>%.*s</state>\
<local>\
<identity display=\"%.*s\">%.*s</identity>\
<target uri=\"%.*s\"/>\
</local>\
<remote>\
<identity display=\"%.*s\">%.*s</identity>\
<target uri=\"%.*s\"/>\
</remote>\
</dialog>\
</dialog-info>"

#define DIALOGINFO_BODY_2 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\"1\" state=\"full\" entity=\"%.*s\">\
<dialog id=\"%.*s\" call-id=\"%.*s\" %.*s %.*s direction=\"%.*s\">\
<state>%.*s</state>\
<local>\
<identity display=\"%.*s\">%.*s</identity>\
</local>\
<remote>\
<identity display=\"%.*s\">%.*s</identity>\
</remote>\
</dialog>\
</dialog-info>"

#endif /* _PUA_JSON_DEFS_H_ */
