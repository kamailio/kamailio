/*
 * $Id$
 *
 * Copyright (C) 2008 
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * pvar compatibility wrapper for kamailio
 * for now it doesn't do anything (feel free to rm & replace the file)
 *
 * History:
 * --------
 *  2008-11-17  initial version compatible with kamailio pvar.h (andrei)
 */

#ifndef _pvar_h_
#define _pvar_h_

#include "str.h"
#include "usr_avp.h"
#include "parser/msg_parser.h"




enum _pv_type { 
	PVT_NONE=0,           PVT_EMPTY,             PVT_NULL, 
	PVT_MARKER,           PVT_AVP,               PVT_HDR,
	PVT_PID,              PVT_RETURN_CODE,       PVT_TIMES,
	PVT_TIMEF,            PVT_MSGID,             PVT_METHOD,
	PVT_STATUS,           PVT_REASON,            PVT_RURI,
	PVT_RURI_USERNAME,    PVT_RURI_DOMAIN,       PVT_RURI_PORT,
	PVT_FROM,             PVT_FROM_USERNAME,     PVT_FROM_DOMAIN,
	PVT_FROM_TAG,         PVT_TO,                PVT_TO_USERNAME,
	PVT_TO_DOMAIN,        PVT_TO_TAG,            PVT_CSEQ,
	PVT_CONTACT,          PVT_CALLID,            PVT_USERAGENT,
	PVT_MSG_BUF,          PVT_MSG_LEN,           PVT_FLAGS,
	PVT_HEXFLAGS,         PVT_SRCIP,             PVT_SRCPORT,
	PVT_RCVIP,            PVT_RCVPORT,           PVT_REFER_TO,
	PVT_DSET,             PVT_DSTURI,            PVT_COLOR,
	PVT_BRANCH,           PVT_BRANCHES,          PVT_CONTENT_TYPE,
	PVT_CONTENT_LENGTH,   PVT_MSG_BODY,          PVT_AUTH_USERNAME,
	PVT_AUTH_REALM,       PVT_RURI_PROTOCOL,     PVT_DSTURI_DOMAIN,
	PVT_DSTURI_PORT,      PVT_DSTURI_PROTOCOL,   PVT_FROM_DISPLAYNAME,
	PVT_TO_DISPLAYNAME,   PVT_OURI,              PVT_OURI_USERNAME,
	PVT_OURI_DOMAIN,      PVT_OURI_PORT,         PVT_OURI_PROTOCOL,
	PVT_FORCE_SOCK,       PVT_RPID_URI,          PVT_DIVERSION_URI,
	PVT_ACC_USERNAME,     PVT_PPI,               PVT_PPI_DISPLAYNAME,
	PVT_PPI_DOMAIN,       PVT_PPI_USERNAME,      PVT_PAI_URI,
	PVT_BFLAGS,           PVT_HEXBFLAGS,         PVT_SFLAGS,
	PVT_HEXSFLAGS,        PVT_ERR_CLASS,         PVT_ERR_LEVEL,
	PVT_ERR_INFO,         PVT_ERR_RCODE,         PVT_ERR_RREASON,
	PVT_SCRIPTVAR,        PVT_PROTO,             PVT_AUTH_USERNAME_WHOLE,
	PVT_AUTH_DURI,        PVT_DIV_REASON,        PVT_DIV_PRIVACY,
	PVT_AUTH_DOMAIN,      PVT_EXTRA /* keep it last */
};

typedef enum _pv_type pv_type_t;
typedef int pv_flags_t;

typedef struct _pv_value
{
	str rs;    /*!< string value */
	int ri;    /*!< integer value */
	int flags; /*!< flags about the type of value */
} pv_value_t, *pv_value_p;

typedef struct _pv_name
{
	int type;             /*!< type of name */
	union {
		struct {
			int type;     /*!< type of int_str name - compatibility with AVPs */
			int_str name; /*!< the value of the name */
		} isname;
		void *dname;      /*!< PV value - dynamic name */
	} u;
} pv_name_t, *pv_name_p;

typedef struct _pv_index
{
	int type; /*!< type of PV index */
	union {
		int ival;   /*!< integer value */
		void *dval; /*!< PV value - dynamic index */
	} u;
} pv_index_t, *pv_index_p;

typedef struct _pv_param
{
	pv_name_t    pvn; /*!< PV name */
	pv_index_t   pvi; /*!< PV index */
} pv_param_t, *pv_param_p;

typedef int (*pv_getf_t) (struct sip_msg*,  pv_param_t*, pv_value_t*);
typedef int (*pv_setf_t) (struct sip_msg*,  pv_param_t*, int, pv_value_t*);

typedef struct _pv_spec {
	pv_type_t    type;   /*!< type of PV */
	pv_getf_t    getf;   /*!< get PV value function */
	pv_setf_t    setf;   /*!< set PV value function */
	pv_param_t   pvp;    /*!< parameter to be given to get/set functions */
	void         *trans; /*!< transformations */
} pv_spec_t, *pv_spec_p;



typedef int (*pv_parse_name_f)(pv_spec_p sp, str *in);
typedef int (*pv_parse_index_f)(pv_spec_p sp, str *in);
typedef int (*pv_init_param_f)(pv_spec_p sp, int param);

/*! \brief
 * PV spec format:
 * - $class_name
 * - $class_name(inner_name)
 * - $(class_name[index])
 * - $(class_name(inner_name)[index])
 * - $(class_name{transformation})
 * - $(class_name(inner_name){transformation})
 * - $(class_name[index]{transformation})
 * - $(class_name(inner_name)[index]{transformation})
 */
typedef struct _pv_export {
	str name;                      /*!< class name of PV */
	pv_type_t type;                /*!< type of PV */
	pv_getf_t  getf;               /*!< function to get the value */
	pv_setf_t  setf;               /*!< function to set the value */
	pv_parse_name_f parse_name;    /*!< function to parse the inner name */
	pv_parse_index_f parse_index;  /*!< function to parse the index of PV */
	pv_init_param_f init_param;    /*!< function to init the PV spec */
	int iparam;                    /*!< parameter for the init function */
} pv_export_t;

#endif /* _pvar_h_ */
