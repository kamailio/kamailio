/*
 * defs.h
 *
 *  Created on: Jul 15, 2014
 *      Author: root
 */

#ifndef DBK_DEFS_H_
#define DBK_DEFS_H_

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
#define MWI_JSON_NEW            "Messages-New"
#define MWI_JSON_SAVED          "Messages-Saved"
#define MWI_JSON_URGENT         "Messages-Urgent"
#define MWI_JSON_URGENT_SAVED   "Messages-Urgent-Saved"
#define MWI_JSON_ACCOUNT        "Message-Account"
#define MWI_JSON_FROM      	"From"
#define MWI_JSON_TO        	"To"

#define DIALOGINFO_BODY_BUFFER_SIZE 8192
#define MWI_BODY_BUFFER_SIZE 2048
#define PRESENCE_BODY_BUFFER_SIZE 4096

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

#define json_extract_field(json_name, field)  do {                      \
    struct json_object* obj =  kz_json_get_object(json_obj, json_name); \
    field.s = (char*)json_object_get_string(obj);                       \
    if (field.s == NULL) {                                              \
      LM_DBG("Json-c error - failed to extract field [%s]\n", json_name); \
      field.s = "";                                                     \
    } else {                                                            \
      field.len = strlen(field.s);                                      \
    }                                                                   \
    LM_DBG("%s: [%s]\n", json_name, field.s?field.s:"Empty");           \
  } while (0);

#endif /* DBK_DEFS_H_ */
