/**
 * Copyright (C) 2021 Daniel-Constantin Mierla (asipto.com)
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <libwebsockets.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_param.h"

#include "api.h"


MODULE_VERSION

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);
static int bind_lwsc(lwsc_api_t* api);

static int w_lwsc_request(sip_msg_t* msg, char* pwsurl, char* pdata);
static int w_lwsc_request_proto(sip_msg_t* msg, char* pwsurl, char* pwsproto,
		char* pdata);
static int w_lwsc_notify(sip_msg_t* msg, char* pwsurl, char* pdata);
static int w_lwsc_notify_proto(sip_msg_t* msg, char* pwsurl, char* pwsproto,
		char* pdata);

static int _lwsc_timeout_connect = 0;
static int _lwsc_timeout_send = 0;
static int _lwsc_timeout_read = 2000000;
static int _lwsc_timeout_init = 2000000;
static str _lwsc_protocol = str_init("kmsg");
static int _lwsc_verbosity = 0;

static cmd_export_t cmds[]={
	{"lwsc_request", (cmd_function)w_lwsc_request, 2,
		fixup_spve_all, 0, ANY_ROUTE},
	{"lwsc_request_proto", (cmd_function)w_lwsc_request_proto, 3,
		fixup_spve_all, 0, ANY_ROUTE},
	{"lwsc_notify", (cmd_function)w_lwsc_notify, 2,
		fixup_spve_all, 0, ANY_ROUTE},
	{"lwsc_notify_proto", (cmd_function)w_lwsc_notify_proto, 3,
		fixup_spve_all, 0, ANY_ROUTE},
	{"bind_lwsc",   (cmd_function)bind_lwsc, 0,
		0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{ "timeout_connect", PARAM_INT, &_lwsc_timeout_connect },
	{ "timeout_send",    PARAM_INT, &_lwsc_timeout_send },
	{ "timeout_read",    PARAM_INT, &_lwsc_timeout_read },
	{ "timeout_init",    PARAM_INT, &_lwsc_timeout_init },
	{ "protocol",        PARAM_STR, &_lwsc_protocol },
	{ "verbosity",       PARAM_INT, &_lwsc_verbosity },

	{ 0, 0, 0 }
};

static int lwsc_pv_get(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);
static int lwsc_pv_parse_name(pv_spec_t *sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"lwsc",  sizeof("lwsc")-1}, PVT_OTHER,  lwsc_pv_get,    0,
			lwsc_pv_parse_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

struct module_exports exports = {
	"lwsc",          /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};


/**
 * @brief Initialize crypto module function
 */
static int mod_init(void)
{
	return 0;
}

/**
 * @brief Initialize crypto module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

/**
 *
 */
typedef struct lwsc_endpoint {
	str wsurl;
	/* clone of wsurl for libwebsockets parsing with dropping in zeros */
	str wsurlparse;
	str wsurlpath;
	str wsproto;
	/* first LWS_PRE bytes must preserved for headers */
	str wbuf;
	str rbuf;
	int rdone;
	int tlson;
	struct lws_protocols protocols[2];
	struct lws_context_creation_info crtinfo;
	struct lws_client_connect_info coninfo;
	struct lws_context *wsctx;
	struct lws *wsi;

	pthread_mutex_t wslock;
	pthread_t wsthread;

	int wsready;
	int status;

	struct lwsc_endpoint *next;
} lwsc_endpoint_t;

/**
 *
 */
static lwsc_endpoint_t *_lwsc_endpoints = NULL;

/**
 *
 */
static str _lwsc_rdata_buf = STR_NULL;

/**
 *
 */
static lwsc_endpoint_t* lwsc_get_endpoint_by_wsi(struct lws *wsi)
{
	lwsc_endpoint_t *ep;

	for(ep=_lwsc_endpoints; ep!=NULL; ep=ep->next) {
		if(ep->wsi==wsi) {
			return ep;
		}
	}
	return NULL;
}

/**
 *
 */
static void lwsc_print_log(int llevel, const char* lmsg)
{
	if(llevel&LLL_ERR) {
		LM_ERR("libwebsockets: %s\n", lmsg);
	} else if(llevel&LLL_WARN) {
		LM_INFO("libwebsockets: %s\n", lmsg);
	} else if(llevel&LLL_NOTICE) {
		LM_INFO("libwebsockets: %s\n", lmsg);
	} else {
		LM_INFO("libwebsockets(%d): %s\n", llevel, lmsg);
	}
}

/**
 *
 */
static void lwsc_set_logging(void)
{
	if(_lwsc_endpoints==NULL) {
		lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, lwsc_print_log);
	}
}

/**
 *
 */
static int ksr_lwsc_callback(struct lws *wsi, enum lws_callback_reasons reason,
		void *user, void *in, size_t len)
{
	int m = 0;
#if LWS_LIBRARY_VERSION_MAJOR >= 3
	size_t remain = 0;
	int first = 0;
	int final = 0;
#endif
	lwsc_endpoint_t *ep = NULL;
	str rbuf = STR_NULL;
	str wbuf = STR_NULL;
	int blen = 0;

	if(_lwsc_verbosity>1) {
		LM_DBG("callback called with reason %d\n", reason);
	}

	switch (reason) {

		case LWS_CALLBACK_PROTOCOL_INIT:
			if(_lwsc_verbosity>0) {
				LM_DBG("LWS_CALLBACK_PROTOCOL_INIT: %d\n", reason);
			}
			break;
		case LWS_CALLBACK_PROTOCOL_DESTROY:
			if(_lwsc_verbosity>0) {
				LM_DBG("LWS_CALLBACK_PROTOCOL_DESTROY: %d\n", reason);
			}
			break;
#if LWS_LIBRARY_VERSION_MAJOR >= 3
		case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
			if(_lwsc_verbosity>0) {
				LM_DBG("LWS_CALLBACK_EVENT_WAIT_CANCELLED: %d\n", reason);
			}
			break;
#endif

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			LM_ERR("CLIENT_CONNECTION_ERROR: %s\n", in ? (char *)in : "(null)");
			ep = lwsc_get_endpoint_by_wsi(wsi);
			if(ep==NULL) {
				LM_ERR("no endpoint for wsi %p\n", wsi);
				goto done;
			}
			ep->wsready = 0;
			ep->wsi = NULL;
			break;

		case LWS_CALLBACK_GET_THREAD_ID:
			if(_lwsc_verbosity>0) {
				LM_DBG("LWS_CALLBACK_GET_THREAD_ID: %d\n", reason);
			}
			return (long)pthread_self();

		case LWS_CALLBACK_CLOSED:
			LM_DBG("LWS_CALLBACK_CLOSED - wsi: %p\n", wsi);
			ep = lwsc_get_endpoint_by_wsi(wsi);
			if(ep==NULL) {
				LM_ERR("no endpoint for wsi %p\n", wsi);
				goto done;
			}
			ep->wsready = 0;
			ep->wsi = NULL;
			break;

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			LM_DBG("LWS_CALLBACK_CLIENT_ESTABLISHED - wsi: %p\n", wsi);
			ep = lwsc_get_endpoint_by_wsi(wsi);
			if(ep==NULL) {
				LM_ERR("no endpoint for wsi %p\n", wsi);
				goto done;
			}
			ep->wsready = 1;
			lws_callback_on_writable(wsi);
			break;

#if LWS_LIBRARY_VERSION_MAJOR >= 3
		case LWS_CALLBACK_CLIENT_CLOSED:
			LM_DBG("LWS_CALLBACK_CLIENT_CLOSED - wsi: %p\n", wsi);
			ep = lwsc_get_endpoint_by_wsi(wsi);
			if(ep==NULL) {
				LM_ERR("no endpoint for wsi %p\n", wsi);
				goto done;
			}
			ep->wsready = 0;
			ep->wsi = NULL;
			break;
#endif

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			ep = lwsc_get_endpoint_by_wsi(wsi);
			if(ep==NULL) {
				LM_ERR("no endpoint for wsi %p\n", wsi);
				goto done;
			}
			pthread_mutex_lock(&ep->wslock);
			if(ep->wbuf.s!=NULL && ep->wbuf.len>LWS_PRE) {
				wbuf = ep->wbuf;
				ep->wbuf.s = NULL;
				ep->wbuf.len = 0;
			}
			pthread_mutex_unlock(&ep->wslock);
			if(wbuf.s!=NULL) {
				m = lws_write(wsi, (unsigned char*)wbuf.s + LWS_PRE,
						wbuf.len - LWS_PRE, LWS_WRITE_TEXT);
				if (m < wbuf.len - LWS_PRE) {
					LM_ERR("sending message failed: %d < %d\n", m,
							wbuf.len - LWS_PRE);
				}
				pkg_free(wbuf.s);
			}
			break;

#if LWS_LIBRARY_VERSION_MAJOR >= 3
		case LWS_CALLBACK_TIMER:
			if(_lwsc_verbosity>0) {
				LM_DBG("LWS_CALLBACK_TIMER: %d - wsi: %p\n", reason, wsi);
			}
			// lws_callback_on_writable(wsi);
			break;
#endif

		case LWS_CALLBACK_CLIENT_RECEIVE:
#if LWS_LIBRARY_VERSION_MAJOR >= 3
			first = lws_is_first_fragment(wsi);
			final = lws_is_final_fragment(wsi);
			remain = lws_remaining_packet_payload(wsi);
			LM_DBG("LWS_CALLBACK_RECEIVE - wsi: %p len: %lu, first: %d, "
					"final = %d, remains = %lu\n", wsi,
					(unsigned long)len, first, final,
					(unsigned long)remain);
#else
			LM_DBG("LWS_CALLBACK_RECEIVE - wsi: %p len: %lu\n", wsi,
					(unsigned long)len);
#endif
			ep = lwsc_get_endpoint_by_wsi(wsi);
			if(ep==NULL) {
				LM_ERR("no endpoint for wsi %p\n", wsi);
				goto done;
			}
			pthread_mutex_lock(&ep->wslock);
			if(len>0) {
				blen = ep->rbuf.len + len;
				rbuf.s = (char*)pkg_malloc(blen + 1);
				if(rbuf.s==NULL) {
					PKG_MEM_ERROR;
				} else {
					if(ep->rbuf.s!=NULL) {
						LM_DBG("append to read buffer of %d bytes more %d bytes\n",
								ep->rbuf.len, (int)len);
						memcpy(rbuf.s, ep->rbuf.s, ep->rbuf.len);
						pkg_free(ep->rbuf.s);
						ep->rbuf.s = NULL;
					}
					memcpy(rbuf.s + ep->rbuf.len, in, len);
					rbuf.len = blen;
					rbuf.s[rbuf.len] = '\0';
					ep->rbuf = rbuf;
				}
			}
			if (lws_is_final_fragment(wsi)) {
				ep->rdone = 1;
			}
			pthread_mutex_unlock(&ep->wslock);
			break;

		case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
			LM_INFO("server initiated connection close - wsi: %p len: %lu, "
					"in: %s\n", wsi, (unsigned long)len, (in)?(char*)in:"");
			ep = lwsc_get_endpoint_by_wsi(wsi);
			if(ep==NULL) {
				LM_ERR("no endpoint for wsi %p\n", wsi);
				goto done;
			}
			ep->wsready = 0;
			ep->wsi = NULL;
			break;
		default:
			if(_lwsc_verbosity>1) {
				LM_DBG("unhandled reason %d\n", reason);
			}
			break;
	}

done:
	return 0;
}


/**
 *
 */
static int ksr_shoud_reconnect(unsigned int *last, unsigned int secs)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	if ((unsigned long)tv.tv_sec-(unsigned long)(*last)<(unsigned long)secs) {
		return 0;
	}

	*last = (unsigned int)tv.tv_sec;

	return 1;
}

/**
 *
 */
static void* ksr_lwsc_thread(void *arg)
{
	lwsc_endpoint_t *ep;
	int rcount = 0;
	unsigned int ltime = 0;

	ep = (lwsc_endpoint_t*)arg;

	/* wait 2secs for initial connect */
	while(ep->wsi==NULL && rcount<200) {
		usleep(10000);
		rcount++;
	}
	/* try to connect every 2secs */
	while(ep->wsi==NULL) {
		usleep(2000000);
		if(ep->wsi==NULL) {
#if LWS_LIBRARY_VERSION_MAJOR >= 3
			ep->coninfo.pwsi = &ep->wsi;
			lws_client_connect_via_info(&ep->coninfo);
#else
			ep->wsi = lws_client_connect_via_info(&ep->coninfo);;
#endif
		}
	}

	while(ep->status==0) {
		if((ep->wsi==NULL) && ksr_shoud_reconnect(&ltime, 2)) {
			LM_DBG("attempting to reconnect: %u\n", ltime);
#if LWS_LIBRARY_VERSION_MAJOR >= 3
			ep->coninfo.pwsi = &ep->wsi;
			lws_client_connect_via_info(&ep->coninfo);
#else
			ep->wsi = lws_client_connect_via_info(&ep->coninfo);;
#endif
		}
		lws_service(ep->wsctx, 100);
	}

	return NULL;
}

/**
 *
 */
static lwsc_endpoint_t* lwsc_get_endpoint(str *wsurl, str *wsproto)
{
	lwsc_endpoint_t *ep;
	int ssize = 0;
	const char *urlproto = NULL;
	const char *urlpath = NULL;
	int s = 0;
	str lwsproto = STR_NULL;

	if(wsproto!=NULL && wsproto->s!=NULL && wsproto->len>0) {
		lwsproto = *wsproto;
	} else {
		lwsproto = _lwsc_protocol;
	}

	for(ep=_lwsc_endpoints; ep!=NULL; ep=ep->next) {
		if(ep->wsurl.len==wsurl->len && ep->wsproto.len==lwsproto.len
				&& strncmp(ep->wsurl.s, wsurl->s, wsurl->len)==0
				&& strncmp(ep->wsproto.s, lwsproto.s, lwsproto.len)==0) {
			return ep;
		}
	}
	ssize = sizeof(lwsc_endpoint_t) + 3*(wsurl->len + 1) + (lwsproto.len + 1);
	ep = (lwsc_endpoint_t*)pkg_malloc(ssize);
	if(ep==NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}
	memset(ep, 0, ssize);
	ep->wsurl.s = (char*)ep + sizeof(lwsc_endpoint_t);
	memcpy(ep->wsurl.s, wsurl->s, wsurl->len);
	ep->wsurl.len = wsurl->len;
	ep->wsurlparse.s = ep->wsurl.s + wsurl->len + 1;
	memcpy(ep->wsurlparse.s, wsurl->s, wsurl->len);
	ep->wsurlparse.len = wsurl->len;
	ep->wsurlpath.s = ep->wsurlparse.s + wsurl->len + 1;
	ep->wsurlpath.s[0] = '/';
	ep->wsurlpath.len = 1;
	ep->wsproto.s = ep->wsurlpath.s + wsurl->len + 1;
	memcpy(ep->wsproto.s, lwsproto.s, lwsproto.len);
	ep->wsproto.len = lwsproto.len;


	if (lws_parse_uri(ep->wsurlparse.s, &urlproto, &ep->coninfo.address,
				&ep->coninfo.port, &urlpath)) {
		LM_ERR("cannot parse ws url [%.*s]\n", wsurl->len, wsurl->s);
		goto error;
	}
	if(urlpath!=NULL && strlen(urlpath) > 0) {
		strcpy(ep->wsurlpath.s + 1, urlpath);
		ep->wsurlpath.len = strlen(ep->wsurlpath.s);
	}
	ep->coninfo.path = (const char*)ep->wsurlpath.s;

	if (strcmp(urlproto, "wss")==0 || strcmp(urlproto, "https")==0) {
		ep->tlson = 1;
	}

	ep->crtinfo.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	if(ep->tlson==1) {
		ep->crtinfo.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	}
	ep->protocols[0].name = ep->wsproto.s;
	ep->protocols[0].callback = ksr_lwsc_callback;
	ep->crtinfo.protocols = ep->protocols;
	ep->crtinfo.gid = -1;
	ep->crtinfo.uid = -1;
#if LWS_LIBRARY_VERSION_MAJOR == 3
	ep->crtinfo.ws_ping_pong_interval = 5; /*secs*/
#endif
	/* 1 internal and 1 (+ 1 http2 nwsi) */
	ep->crtinfo.fd_limit_per_thread = 1 + 1 + 1;

	ep->wsctx = lws_create_context(&ep->crtinfo);
	if (!ep->wsctx) {
		LM_ERR("failed to intialize context for ws url [%.*s]\n",
				wsurl->len, wsurl->s);
		goto error;
	}

	ep->coninfo.context = ep->wsctx;
	ep->coninfo.ssl_connection = 0;
	ep->coninfo.host = ep->coninfo.address;
	ep->coninfo.origin = ep->coninfo.address;
	ep->coninfo.ietf_version_or_minus_one = -1;
	ep->coninfo.protocol = ep->protocols[0].name;
	if(ep->tlson==1) {
#if LWS_LIBRARY_VERSION_MAJOR >= 3
		ep->coninfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED
			| LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
#else
		ep->coninfo.ssl_connection = 2;
#endif
	}
	pthread_mutex_init(&ep->wslock, NULL);

	LM_DBG("connecting to [%.*s]\n", wsurl->len, wsurl->s);

	ep->next = _lwsc_endpoints;
	_lwsc_endpoints = ep;

#if LWS_LIBRARY_VERSION_MAJOR >= 3
	ep->coninfo.pwsi = &ep->wsi;
	lws_client_connect_via_info(&ep->coninfo);
#else
	ep->wsi = lws_client_connect_via_info(&ep->coninfo);;
#endif
	if(ep->wsi==NULL) {
		LM_ERR("failed to creating the ws client instance [%.*s]\n",
				wsurl->len, wsurl->s);
		/* unlink - first item */
		_lwsc_endpoints = ep->next;
		goto error;
	}
	LM_DBG("ws connection instance [%p]\n", ep->wsi);

	s = pthread_create(&ep->wsthread, NULL, &ksr_lwsc_thread, (void*)ep);
	if (s != 0) {
		LM_ERR("failed to create the event loop thread\n");
		goto error;
	}
	pthread_detach(ep->wsthread);

	return ep;

error:
	pkg_free(ep);
	return NULL;
}

/**
 *
 */
static int lwsc_api_request(str* wsurl, str *wsproto, str* sdata,
		str *rdata, int rtimeout)
{
	lwsc_endpoint_t *ep = NULL;
	str wbuf = STR_NULL;
	int rcount = 0;
	int icount = 0;

	if(wsurl==NULL || wsurl->s==NULL || wsurl->len<=0
			|| sdata==NULL || sdata->s==NULL || sdata->len<=0
			|| rdata==NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	lwsc_set_logging();
	ep = lwsc_get_endpoint(wsurl, wsproto);
	if(ep==NULL) {
		LM_ERR("endpoint not available\n");
		return -1;
	}

	while(ep->wsready==0) {
		usleep(10000);
		icount += 10000;
		if(icount>=_lwsc_timeout_init) {
			LM_ERR("connection not ready after init timeout\n");
			return -1;
		}
	}

	wbuf.s = (char*)pkg_malloc(LWS_PRE + sdata->len + 1);
	if(wbuf.s==NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(wbuf.s, 0, LWS_PRE + sdata->len + 1);
	memcpy(wbuf.s + LWS_PRE, sdata->s, sdata->len);
	wbuf.len = LWS_PRE + sdata->len;

	rdata->s = NULL;
	rdata->len = 0;

	pthread_mutex_lock(&ep->wslock);
	if(ep->rbuf.s!=NULL) {
		/* clear ws receive buffer */
		LM_ERR("losing read buffer content of %d bytes\n", ep->rbuf.len);
		pkg_free(ep->rbuf.s);
		ep->rbuf.s = NULL;
		ep->rbuf.len = 0;
	}
	ep->rdone = 0;
	if(ep->wbuf.s!=NULL) {
		LM_ERR("losing write buffer content of %d bytes\n", ep->wbuf.len);
		pkg_free(ep->wbuf.s);
	}
	ep->wbuf = wbuf;
	pthread_mutex_unlock(&ep->wslock);

	/* notify main loop another message should be sent */
    lws_callback_on_writable(ep->wsi);
    lws_cancel_service(ep->wsctx);

	do {
		pthread_mutex_lock(&ep->wslock);
		if(ep->rdone==1) {
			if(ep->rbuf.s!=NULL) {
				*rdata = ep->rbuf;
				ep->rbuf.s = NULL;
				ep->rbuf.len = 0;
			}
			ep->rdone = 0;
		}
		pthread_mutex_unlock(&ep->wslock);
		if(rdata->s==NULL) {
			usleep(10000);
		}
		rcount += 10000;
	} while(rcount<rtimeout && rdata->s==NULL);

	if(rdata->s==NULL) {
		LM_DBG("no response data received before timeout\n");
		return -2;
	}

	return 1;
}

/**
 *
 */
static int ki_lwsc_request(sip_msg_t* msg, str* wsurl, str* data)
{
	/* clear global per-process receive buffer */
	if(_lwsc_rdata_buf.s!=NULL) {
		pkg_free(_lwsc_rdata_buf.s);
		_lwsc_rdata_buf.s = NULL;
		_lwsc_rdata_buf.len = 0;
	}

	return lwsc_api_request(wsurl, NULL, data, &_lwsc_rdata_buf,
			_lwsc_timeout_read);
}

/**
 *
 */
static int w_lwsc_request(sip_msg_t* msg, char* pwsurl, char* pdata)
{
	str swsurl = STR_NULL;
	str sdata = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)pwsurl, &swsurl) != 0) {
		LM_ERR("cannot get ws url\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pdata, &sdata) != 0) {
		LM_ERR("cannot get data value\n");
		return -1;
	}

	return ki_lwsc_request(msg, &swsurl, &sdata);
}

/**
 *
 */
static int ki_lwsc_request_proto(sip_msg_t* msg, str* wsurl, str* wsproto,
		str* data)
{
	/* clear global per-process receive buffer */
	if(_lwsc_rdata_buf.s!=NULL) {
		pkg_free(_lwsc_rdata_buf.s);
		_lwsc_rdata_buf.s = NULL;
		_lwsc_rdata_buf.len = 0;
	}

	return lwsc_api_request(wsurl, wsproto, data, &_lwsc_rdata_buf,
			_lwsc_timeout_read);
}

/**
 *
 */
static int w_lwsc_request_proto(sip_msg_t* msg, char* pwsurl, char* pwsproto,
		char* pdata)
{
	str swsurl = STR_NULL;
	str swsproto = STR_NULL;
	str sdata = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)pwsurl, &swsurl) != 0) {
		LM_ERR("cannot get ws url\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pwsproto, &swsproto) != 0) {
		LM_ERR("cannot get ws proto\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pdata, &sdata) != 0) {
		LM_ERR("cannot get data value\n");
		return -1;
	}

	return ki_lwsc_request_proto(msg, &swsurl, &swsproto, &sdata);
}

/**
 *
 */
static int lwsc_api_notify(str* wsurl, str* wsproto, str* data)
{
	lwsc_endpoint_t *ep = NULL;
	str wbuf = STR_NULL;
	int icount = 0;

	if(wsurl==NULL || wsurl->s==NULL || wsurl->len<=0
			|| data==NULL || data->s==NULL || data->len<=0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	lwsc_set_logging();

	ep = lwsc_get_endpoint(wsurl, NULL);
	if(ep==NULL) {
		LM_ERR("endpoint not available\n");
		return -1;
	}

	while(ep->wsready==0) {
		usleep(10000);
		icount += 10000;
		if(icount>=_lwsc_timeout_init) {
			LM_ERR("connection not ready after init timeout\n");
			return -1;
		}
	}

	wbuf.s = (char*)pkg_malloc(LWS_PRE + data->len + 1);
	if(wbuf.s==NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(wbuf.s, 0, LWS_PRE + data->len + 1);
	memcpy(wbuf.s + LWS_PRE, data->s, data->len);
	wbuf.len = LWS_PRE + data->len;

	pthread_mutex_lock(&ep->wslock);
	if(ep->wbuf.s!=NULL) {
		LM_ERR("losing write buffer content of %d bytes\n", ep->wbuf.len);
		pkg_free(ep->wbuf.s);
	}
	ep->wbuf = wbuf;
	pthread_mutex_unlock(&ep->wslock);

	/* notify main loop another message should be sent */
    lws_callback_on_writable(ep->wsi);
    lws_cancel_service(ep->wsctx);

	LM_DBG("notification prepared for delivery\n");

	return 1;
}

/**
 *
 */
static int ki_lwsc_notify(sip_msg_t* msg, str* wsurl, str* data)
{
	return lwsc_api_notify(wsurl, NULL, data);
}

/**
 *
 */
static int w_lwsc_notify(sip_msg_t* msg, char* pwsurl, char* pdata)
{
	str swsurl = STR_NULL;
	str sdata = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)pwsurl, &swsurl) != 0) {
		LM_ERR("cannot get ws url\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pdata, &sdata) != 0) {
		LM_ERR("cannot get data value\n");
		return -1;
	}

	return ki_lwsc_notify(msg, &swsurl, &sdata);
}

/**
 *
 */
static int ki_lwsc_notify_proto(sip_msg_t* msg, str* wsurl, str* wsproto,
		str* data)
{
	return lwsc_api_notify(wsurl, wsproto, data);
}

/**
 *
 */
static int w_lwsc_notify_proto(sip_msg_t* msg, char* pwsurl, char* pwsproto,
		char* pdata)
{
	str swsurl = STR_NULL;
	str swsproto = STR_NULL;
	str sdata = STR_NULL;

	if (fixup_get_svalue(msg, (gparam_t*)pwsurl, &swsurl) != 0) {
		LM_ERR("cannot get ws url\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pwsproto, &swsproto) != 0) {
		LM_ERR("cannot get ws proto\n");
		return -1;
	}
	if (fixup_get_svalue(msg, (gparam_t*)pdata, &sdata) != 0) {
		LM_ERR("cannot get data value\n");
		return -1;
	}

	return ki_lwsc_notify_proto(msg, &swsurl, &swsproto, &sdata);
}

/**
 *
 */
static int lwsc_pv_get(sip_msg_t *msg, pv_param_t *param, pv_value_t *res)
{
	switch(param->pvn.u.isname.name.n)
	{
		case 0:
			if(_lwsc_rdata_buf.s==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &_lwsc_rdata_buf);
		case 1:
			return pv_get_uintval(msg, param, res, 0);
		default:
			return pv_get_null(msg, param, res);
	}
}

/**
 *
 */
static int lwsc_pv_parse_name(pv_spec_t *sp, str *in)
{
	if(in->len==5 && strncmp(in->s, "rdata", 5)==0) {
		sp->pvp.pvn.u.isname.name.n = 0;
	} else if(in->len==6 && strncmp(in->s, "status", 6)==0) {
		sp->pvp.pvn.u.isname.name.n = 1;
	} else {
		LM_ERR("unknown inner name [%.*s]\n", in->len, in->s);
		return -1;
	}
	return 0;
}

/**
 * @brief bind functions to LWSC API structure
 */
static int bind_lwsc(lwsc_api_t* api)
{
	if (!api) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	api->request = lwsc_api_request;

	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_lwsc_exports[] = {
	{ str_init("lwsc"), str_init("lwsc_request"),
		SR_KEMIP_INT, ki_lwsc_request,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("lwsc"), str_init("lwsc_request_proto"),
		SR_KEMIP_INT, ki_lwsc_request_proto,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("lwsc"), str_init("lwsc_notify"),
		SR_KEMIP_INT, ki_lwsc_notify,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("lwsc"), str_init("lwsc_notify_proto"),
		SR_KEMIP_INT, ki_lwsc_notify_proto,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_lwsc_exports);
	return 0;
}
