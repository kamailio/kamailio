/*
 * XMPP Module
 * This file is part of Kamailio, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * 
 */

/*! \file
 * \brief The XMPP api
 * \ingroup xmpp
 */


#ifndef _XMPP_API_H_
#define _XMPP_API_H_

#define XMPP_RCV_MESSAGE      (1<<0)
#define XMPP_RCV_PRESENCE     (1<<1)
#define XMPP_RCV_IQ			  (1<<2)

typedef void (xmpp_cb_f) (char *msg, int type, void *param);
typedef int (*register_xmpp_cb_t)(int types, xmpp_cb_f f, void *param);


typedef struct xmpp_callback_
{
	int types;                   /*!< types of events that trigger the callback*/
	xmpp_cb_f *cbf;              /*!< callback function */
	void *cbp;                   /*!< param to be passed to callback function */
	struct xmpp_callback_ *next;
} xmpp_callback_t;

typedef struct xmpp_cb_list_
{
	xmpp_callback_t *first;
	int types;
} xmpp_cb_list_t;


extern xmpp_cb_list_t*  _xmpp_cb_list;


#define xmpp_isset_cb_type(_types_) \
	((_xmpp_cb_list->types)|(_types_) )


int init_xmpp_cb_list(void);

void destroy_xmpp_cb_list(void);


int register_xmpp_cb( int types, xmpp_cb_f f, void *param );

/*! \brief run all transaction callbacks for an event type */
static inline void run_xmpp_callbacks( int type, char *msg)
{
	xmpp_callback_t *it;

	for (it=_xmpp_cb_list->first; it; it=it->next)  {
		if(it->types&type) {
			LM_DBG("cb: msg=%p, callback type %d/%d fired\n",
				msg, type, it->types );
			it->cbf( msg, type, it->cbp );
		}
	}
}

typedef int (*xmpp_send_xpacket_f)(str *from, str *to, str *msg, str *id);
int xmpp_send_xpacket(str *from, str *to, str *msg, str *id);

typedef int (*xmpp_send_xmessage_f)(str *from, str *to, str *msg, str *id);
int xmpp_send_xmessage(str *from, str *to, str *msg, str *id);

typedef int (*xmpp_send_xsubscribe_f)(str *from, str *to, str *msg, str *id);
int xmpp_send_xsubscribe(str *from, str *to, str *msg, str *id);

typedef int (*xmpp_send_xnotify_f)(str *from, str *to, str *msg, str *id);
int xmpp_send_xnotify(str *from, str *to, str *msg, str *id);

typedef char* (*xmpp_translate_uri_f)(char *uri);
char *decode_uri_sip_xmpp(char *uri);
char *encode_uri_sip_xmpp(char *uri);
char *decode_uri_xmpp_sip(char *jid);
char *encode_uri_xmpp_sip(char *jid);

typedef struct xmpp_api_
{
	register_xmpp_cb_t register_callback;
	xmpp_send_xpacket_f xpacket;
	xmpp_send_xmessage_f xmessage;
	xmpp_send_xsubscribe_f xsubscribe;
	xmpp_send_xnotify_f xnotify;
	xmpp_translate_uri_f decode_uri_sip_xmpp;
	xmpp_translate_uri_f encode_uri_sip_xmpp;
	xmpp_translate_uri_f decode_uri_xmpp_sip;
	xmpp_translate_uri_f encode_uri_xmpp_sip;
} xmpp_api_t;

typedef int (*bind_xmpp_t)(xmpp_api_t* api);
int bind_xmpp(xmpp_api_t* api);

#endif

