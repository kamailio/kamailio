/*
 * lost module functions
 *
 * Copyright (C) 2022 Wolfgang Kampichler
 * DEC112, FREQUENTIS AG
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file
 * \brief Kamailio lost :: functions
 * \ingroup lost
 * Module: \ref lost
 */
/*****************/

#include "../../modules/http_client/curl_api.h"

#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/route_struct.h"
#include "../../core/ut.h"
#include "../../core/trim.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_body.h"
#include "../../core/lvalue.h"

#include "pidf.h"
#include "utilities.h"
#include "response.h"
#include "naptr.h"

#define LOST_SUCCESS 200
#define LOST_CLIENT_ERROR 400
#define LOST_SERVER_ERROR 500

#define HELD_DEFAULT_TYPE "geodetic locationURI"
#define HELD_DEFAULT_TYPE_LEN (sizeof(HELD_DEFAULT_TYPE) - 1)

#define NAPTR_LOST_SERVICE_HTTP "LoST:http"
#define NAPTR_LOST_SERVICE_HTTPS "LoST:https"
#define NAPTR_LIS_SERVICE_HELD "LIS:HELD"

#define ACCEPT_HDR                      \
	"Accept: "                          \
	"application/pidf+xml,application/" \
	"held+xml;q=0.5"

extern httpc_api_t httpapi;

extern int lost_geoloc_type;
extern int lost_geoloc_order;
extern int lost_verbose;
extern int held_resp_time;
extern int held_exact_type;
extern int held_post_req;
extern str held_loc_type;

char mtheld[] = "application/held+xml;charset=utf-8";
char mtlost[] = "application/lost+xml;charset=utf-8";

char uri_element[] = "uri";
char name_element[] = "displayName";
char errors_element[] = "errors";

/*
 * lost_held_type(type, exact, lgth)
 * verifies module params and returns valid HELD loaction type
 * allocated in private memory
 */
char *lost_held_type(char *type, int *exact, int *lgth)
{
	char *ret = NULL;
	char *tmp = NULL;
	int len = 0;

	ret = (char *)pkg_malloc(1);
	if(ret == NULL)
		goto err;

	memset(ret, 0, 1);
	*lgth = 0;

	if(strstr(type, HELD_TYPE_ANY)) {
		len = strlen(ret) + strlen(HELD_TYPE_ANY) + 1;
		tmp = pkg_realloc(ret, len);
		if(tmp == NULL)
			goto err;
		ret = tmp;
		strcat(ret, HELD_TYPE_ANY);
		*exact = 0;
	} else {
		if(strstr(type, HELD_TYPE_CIV)) {
			len = strlen(ret) + strlen(HELD_TYPE_CIV) + 1;
			tmp = pkg_realloc(ret, len);
			if(tmp == NULL)
				goto err;
			ret = tmp;
			strcat(ret, HELD_TYPE_CIV);
		}
		if(strstr(type, HELD_TYPE_GEO)) {
			if(strlen(ret) > 1) {
				len = strlen(ret) + strlen(HELD_TYPE_SEP) + 1;
				tmp = pkg_realloc(ret, len);
				if(tmp == NULL)
					goto err;
				ret = tmp;
				strcat(ret, HELD_TYPE_SEP);
			}
			len = strlen(ret) + strlen(HELD_TYPE_GEO) + 1;
			tmp = pkg_realloc(ret, len);
			if(tmp == NULL)
				goto err;
			ret = tmp;
			strcat(ret, HELD_TYPE_GEO);
		}
		if(strstr(type, HELD_TYPE_URI)) {
			if(strlen(ret) > 1) {
				len = strlen(ret) + strlen(HELD_TYPE_SEP) + 1;
				tmp = pkg_realloc(ret, len);
				if(tmp == NULL)
					goto err;
				ret = tmp;
				strcat(ret, HELD_TYPE_SEP);
			}
			len = strlen(ret) + strlen(HELD_TYPE_URI) + 1;
			tmp = pkg_realloc(ret, len);
			if(tmp == NULL)
				goto err;
			ret = tmp;
			strcat(ret, HELD_TYPE_URI);
		}
	}

	*lgth = strlen(ret);
	return ret;

err:
	PKG_MEM_ERROR;
	/* clean up */
	if(ret != NULL) {
		pkg_free(ret);
		ret = NULL;
	}
	*lgth = 0;
	return NULL;
}

/*
 * lost_function_held(msg, con, pidf, url, err, id)
 * assembles and runs HELD locationRequest, parses results
 */
int lost_held_function(struct sip_msg *_m, char *_con, char *_pidf, char *_url,
		char *_err, char *_id)
{
	pv_spec_t *pspidf;
	pv_spec_t *psurl;
	pv_spec_t *pserr;

	pv_value_t pvpidf;
	pv_value_t pvurl;
	pv_value_t pverr;

	p_lost_held_t held = NULL;

	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	xmlNodePtr cur_node = NULL;

	str geo = STR_NULL; /* return value geolocation uri */
	str res = STR_NULL; /* return value pidf */
	str err = STR_NULL; /* return value error */

	str url = STR_NULL;
	str did = STR_NULL;
	str que = STR_NULL;
	str con = STR_NULL;
	str host = STR_NULL;
	str name = STR_NULL;
	str idhdr = STR_NULL;
	str pidfurl = STR_NULL;

	static str rtype = STR_STATIC_INIT(HELD_DEFAULT_TYPE);
	static str sheld = STR_STATIC_INIT(NAPTR_LIS_SERVICE_HELD);

	char ustr[MAX_URI_SIZE];
	char istr[NI_MAXHOST];
	char *ipstr = NULL;
	char *lisurl = NULL;
	char *heldreq = NULL;

	int len = 0;
	int curl = 0;
	int flag = 0;
	int naptr = 0;
	int presence = 0;
	int res_error = 0;

	if(_pidf == NULL || _url == NULL || _err == NULL) {
		LM_ERR("invalid parameter\n");
		goto err;
	}

	/* module parameter */
	if(held_loc_type.len > 0) {
		rtype.s = held_loc_type.s;
		rtype.len = held_loc_type.len;
	}
	/* connection from parameter */
	if(_con) {
		if(get_str_fparam(&con, _m, (gparam_p)_con) != 0) {
			LM_ERR("cannot get connection string\n");
			goto err;
		}
		/* check if connection exists */
		if(con.s != NULL && con.len > 0) {
			if(httpapi.http_connection_exists(&con) == 0) {
				LM_ERR("connection: [%s] does not exist\n", con.s);
				goto err;
			}
		}
	}
	/* id from parameter */
	if(_id) {
		if(get_str_fparam(&did, _m, (gparam_p)_id) != 0) {
			LM_ERR("cannot get device id\n");
			goto err;
		}
		if(did.len == 0) {
			LM_ERR("no device id found\n");
			goto err;
		}
	} else {

		LM_DBG("parsing P-A-I header\n");

		/* id from P-A-I header */
		idhdr.s = lost_get_pai_header(_m, &idhdr.len);
		if(idhdr.len == 0) {
			LM_WARN("P-A-I header not found, trying From header ...\n");

			LM_DBG("parsing From header\n");

			/* id from From header */
			idhdr.s = lost_get_from_header(_m, &idhdr.len);
			if(idhdr.len == 0) {
				LM_ERR("no device id found\n");
				goto err;
			}
		}
		did.s = idhdr.s;
		did.len = idhdr.len;
	}
	LM_INFO("### HELD id [%.*s]\n", did.len, did.s);
	/* assemble locationRequest */
	held = lost_new_held(did, rtype, held_resp_time, held_exact_type);
	if(held == NULL) {
		LM_ERR("held object allocation failed\n");
		goto err;
	}
	que.s = lost_held_location_request(held, &que.len);
	lost_free_held(&held); /* clean up */
	if(que.len == 0) {
		LM_ERR("held request document error\n");
		que.s = NULL;
		goto err;
	}

	LM_DBG("held location request: [%s]\n", que.s);

	/* send locationRequest to location server - HTTP POST */
	if(con.s != NULL && con.len > 0) {

		LM_DBG("using connection [%.*s]\n", con.len, con.s);

		/* send via connection */
		curl = httpapi.http_connect(_m, &con, NULL, &res, mtheld, &que);
	} else {
		/* we have no connection ... do a NAPTR lookup */
		if(lost_parse_host(did.s, &host, &flag) > 0) {

			LM_DBG("no conn. trying NATPR lookup [%.*s]\n", host.len, host.s);

			/* remove '[' and ']' from string (IPv6) */
			if(flag == AF_INET6) {
				host.s++;
				host.len = host.len - 2;
			}
			/* is it a name or ip ... check nameinfo (reverse lookup) */
			len = 0;
			ipstr = lost_copy_string(host, &len);
			if(ipstr != NULL) {
				name.s = &(istr[0]);
				name.len = NI_MAXHOST;
				if(lost_get_nameinfo(ipstr, &name, flag) > 0) {

					LM_DBG("ip [%s] to name [%.*s]\n", ipstr, name.len, name.s);

					/* change ip string to name */
					host.s = name.s;
					host.len = name.len;
				} else {

					/* keep string */
					LM_DBG("no nameinfo for [%s]\n", ipstr);
				}
				pkg_free(ipstr); /* clean up */
				ipstr = NULL;
			} else {
				LM_ERR("could not copy host info\n");
			}
			url.s = &(ustr[0]);
			url.len = MAX_URI_SIZE;
			if((naptr = lost_naptr_lookup(host, &sheld, &url)) == 0) {
				LM_ERR("NAPTR failed on [%.*s]\n", host.len, host.s);
				goto err;
			}
		} else {
			LM_ERR("failed to get location service for [%.*s]\n", did.len,
					did.s);
			goto err;
		}

		LM_DBG("NATPR lookup returned [%.*s]\n", url.len, url.s);

		/* curl doesn't like str */
		len = 0;
		lisurl = lost_copy_string(url, &len);
		if(lisurl == NULL) {
			LM_ERR("could not copy POST url\n");
			goto err;
		}
		/* send to service */
		curl = httpapi.http_client_query_c(
				_m, lisurl, &res, que.s, mtheld, ACCEPT_HDR);
		pkg_free(lisurl); /*clean up */
		lisurl = NULL;
	}
	/* only HTTP 2xx responses are accepted */
	if(curl >= 300 || curl < 100) {
		if(con.s != NULL && con.len > 0) {
			LM_ERR("[%.*s] failed with error: %d\n", con.len, con.s, curl);
		} else {
			LM_ERR("POST [%.*s] failed with error: %d\n", url.len, url.s, curl);
		}
		goto err;
	}
	if(con.s != NULL && con.len > 0) {

		LM_DBG("[%.*s] returned: %d\n", con.len, con.s, curl);

	} else {

		LM_DBG("[%.*s] returned: %d\n", url.len, url.s, curl);
	}
	did.s = NULL;
	did.len = 0;
	/* clean up */
	lost_free_string(&idhdr);
	lost_free_string(&que);
	/* read and parse the returned xml */
	doc = xmlReadMemory(res.s, res.len, 0, NULL,
			XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA);
	if(doc == NULL) {
		LM_WARN("invalid xml document: [%.*s]\n", res.len, res.s);
		doc = xmlRecoverMemory(res.s, res.len);
		if(doc == NULL) {
			LM_ERR("xml document recovery failed on: [%.*s]\n", res.len, res.s);
			goto err;
		}

		LM_DBG("xml document recovered\n");
	}
	root = xmlDocGetRootElement(doc);
	if(root == NULL) {
		LM_ERR("empty xml document\n");
		goto err;
	}
	/* check the root element ... shall be locationResponse, or error */
	if(xmlStrcmp(root->name, (const xmlChar *)"locationResponse") == 0) {

		LM_DBG("HELD location response [%.*s]\n", res.len, res.s);

		for(cur_node = root->children; cur_node; cur_node = cur_node->next) {
			if(cur_node->type == XML_ELEMENT_NODE) {
				if(xmlStrcmp(cur_node->name, (const xmlChar *)"locationUriSet")
						== 0) {

					LM_DBG("*** node '%s' found\n", cur_node->name);

					/* get the locationUri element */
					geo.s = lost_get_content(
							root, (char *)HELD_TYPE_URI, &geo.len);
					if(geo.len == 0) {
						LM_WARN("%s element not found\n", HELD_TYPE_URI);
						geo.s = NULL;
					} else {
						geo.s = lost_trim_content(geo.s, &geo.len);
					}
				}
				if(xmlStrcmp(cur_node->name, (const xmlChar *)"presence")
						== 0) {

					LM_DBG("*** node '%s' found\n", cur_node->name);

					/* response contains presence node */
					presence = 1;
				}
			}
		}
		/* if we do not have a presence node but a location URI */
		/* dereference pidf.lo at location server via HTTP GET */
		if((presence == 0) && (geo.s != NULL && geo.len > 0)) {
			LM_INFO("presence node not found in HELD response, trying URI "
					"...\n");
			if(held_post_req == 0) {
				curl = httpapi.http_client_query_c(
						_m, geo.s, &pidfurl, NULL, mtheld, ACCEPT_HDR);
			} else {
				len = 0;
				heldreq = lost_held_post_request(&len, 0, NULL);
				if(heldreq == NULL) {
					LM_ERR("could not create POST request\n");
					goto err;
				}

				LM_DBG("held POST request: [%.*s]\n", len, heldreq);

				curl = httpapi.http_client_query_c(
						_m, geo.s, &pidfurl, heldreq, mtheld, ACCEPT_HDR);
				pkg_free(heldreq); /* clean up */
				heldreq = NULL;
			}
			/* only HTTP 2xx responses are accepted */
			if(curl >= 300 || curl < 100) {
				LM_ERR("GET [%.*s] failed with error: %d\n", pidfurl.len,
						pidfurl.s, curl);
				goto err;
			}
			if(pidfurl.len == 0) {
				LM_WARN("HELD location request failed [%.*s]\n", geo.len,
						geo.s);
			} else {

				LM_DBG("HELD location response [%.*s]\n", pidfurl.len,
						pidfurl.s);

				res.s = pidfurl.s;
				res.len = pidfurl.len;
			}
		}
		/* error received */
	} else if(xmlStrcmp(root->name, (const xmlChar *)"error") == 0) {

		LM_DBG("HELD error response [%.*s]\n", res.len, res.s);

		/* get the error property */
		err.s = lost_get_property(root, (char *)"code", &err.len);
		if(err.len == 0) {
			LM_ERR("error - property not found: [%.*s]\n", res.len, res.s);
			goto err;
		}
		LM_WARN("locationRequest error response: [%.*s]\n", err.len, err.s);
	} else {
		LM_ERR("root element is not valid: [%.*s]\n", res.len, res.s);
		goto err;
	}

	/* clean up */
	xmlFreeDoc(doc);
	doc = NULL;

	/* set writeable pvars */
	pvpidf.rs = res;
	pvpidf.rs.s = res.s;
	pvpidf.rs.len = res.len;

	pvpidf.flags = PV_VAL_STR;
	pspidf = (pv_spec_t *)_pidf;
	pspidf->setf(_m, &pspidf->pvp, (int)EQ_T, &pvpidf);
	lost_free_string(&res); /* clean up */

	pvurl.rs = geo;
	pvurl.rs.s = geo.s;
	pvurl.rs.len = geo.len;

	pvurl.flags = PV_VAL_STR;
	psurl = (pv_spec_t *)_url;
	psurl->setf(_m, &psurl->pvp, (int)EQ_T, &pvurl);
	lost_free_string(&geo); /* clean up */

	/* return error code in case of response error */
	if(err.len > 0) {
		res_error = 1;
	}
	pverr.rs = err;
	pverr.rs.s = err.s;
	pverr.rs.len = err.len;

	pverr.flags = PV_VAL_STR;
	pserr = (pv_spec_t *)_err;
	pserr->setf(_m, &pserr->pvp, (int)EQ_T, &pverr);
	lost_free_string(&err); /* clean up */

	return (res_error > 0) ? LOST_SERVER_ERROR : LOST_SUCCESS;

err:
	/* clean up pointer */
	lost_free_string(&que);
	lost_free_string(&idhdr);
	lost_free_string(&pidfurl);
	/* clean up xml */
	if(doc != NULL) {
		xmlFreeDoc(doc);
	}
	/* clean up string */
	if(res.s != NULL && res.len > 0) {
		lost_free_string(&res);
	}
	if(geo.s != NULL && geo.len > 0) {
		lost_free_string(&geo);
	}
	if(err.s != NULL && err.len > 0) {
		lost_free_string(&err);
	}

	return LOST_CLIENT_ERROR;
}


/*
 * lost_held_dereference(msg, url, pidf, err, rtime, rtype)
 * assembles and runs HELD locationRequest (POST), returns result as pidf
 */
int lost_held_dereference(struct sip_msg *_m, char *_url, char *_pidf,
		char *_err, char *_rtime, char *_rtype)
{
	pv_spec_t *pspidf;
	pv_spec_t *pserr;

	pv_value_t pvpidf;
	pv_value_t pverr;

	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;

	str url = STR_NULL;
	str rtm = STR_NULL;
	str rtp = STR_NULL;

	str res = STR_NULL; /* return value location response */
	str err = STR_NULL; /* return value error */

	char *ptr = NULL;
	char *lisurl = NULL;
	char *heldreq = NULL;
	char *rtype = NULL;

	long ltime = 0;
	long rtime = 0;

	int len = 0;
	int curl = 0;
	int exact = 0;
	int ret = LOST_SUCCESS;

	if(_url == NULL || _rtime == NULL || _pidf == NULL || _rtype == NULL
			|| _err == NULL) {
		LM_ERR("invalid parameter\n");
		goto err;
	}

	/* dereference url from parameter */
	if(_url) {
		if(get_str_fparam(&url, _m, (gparam_p)_url) != 0) {
			LM_ERR("cannot get dereference url\n");
			goto err;
		}
		if(url.len == 0) {
			LM_ERR("no dereference url found\n");
			goto err;
		}
	}

	/* response time from parameter */
	if(_rtime) {
		if(get_str_fparam(&rtm, _m, (gparam_p)_rtime) != 0) {
			LM_ERR("cannot get response time\n");
			goto err;
		}
		if(rtm.len == 0) {
			/* default: rtime = 0 */
			LM_WARN("no response time found\n");
		} else {
			ltime = strtol(rtm.s, &ptr, 10);
			/* look for a number ... */
			if((ltime > 0) && (strlen(ptr) == 0)) {
				/* responseTime: milliseconds */
				rtime = ltime;
				/* or a string */
			} else if((ltime == 0) && (strlen(ptr) > 0)) {
				if(strncasecmp(ptr, HELD_ED, strlen(HELD_ED)) == 0) {
					/* responseTime: emergencyDispatch */
					rtime = -1;
				} else if(strncasecmp(ptr, HELD_ER, strlen(HELD_ER)) == 0) {
					/* responseTime: emergencyRouting */
					rtime = 0;
				}
			}
		}
	}

	/* response type from parameter */
	if(_rtype) {
		if(get_str_fparam(&rtp, _m, (gparam_p)_rtype) != 0) {
			LM_ERR("cannot get response type\n");
			goto err;
		}
		if(rtp.len == 0) {
			LM_WARN("no response type found\n");
			rtype = NULL;
		} else {
			len = 0;
			/* response type string sanity check */
			rtype = lost_held_type(rtp.s, &exact, &len);
			/* default value will be used if nothing was returned */
			if(rtype == NULL) {
				LM_WARN("cannot normalize [%.*s]\n", rtp.len, rtp.s);
			}
		}
	}

	/* get the HELD request body */
	heldreq = lost_held_post_request(&len, rtime, rtype);

	/* clean up */
	if(rtype != NULL) {
		pkg_free(rtype);
		rtype = NULL;
	}

	if(heldreq == NULL) {
		LM_ERR("could not create POST request\n");
		goto err;
	}

	LM_DBG("POST request: [%.*s]\n", len, heldreq);

	/* curl doesn't like str */
	len = 0;
	lisurl = lost_copy_string(url, &len);
	if(lisurl == NULL) {
		LM_ERR("could not copy POST url\n");
		pkg_free(heldreq); /* clean up */
		heldreq = NULL;
		goto err;
	}

	LM_DBG("POST url: [%.*s]\n", len, lisurl);

	curl = httpapi.http_client_query_c(
			_m, lisurl, &res, heldreq, mtheld, ACCEPT_HDR);
	pkg_free(lisurl); /* clean up */
	lisurl = NULL;
	pkg_free(heldreq);
	heldreq = NULL;

	/* only HTTP 2xx responses are accepted */
	if(curl >= 300 || curl < 100) {
		LM_ERR("POST [%.*s] failed with error: %d\n", url.len, url.s, curl);
		goto err;
	}
	if(res.s != NULL && res.len > 0) {

		LM_DBG("LbR pidf-lo: [%.*s]\n", res.len, res.s);

	} else {
		LM_ERR("dereferencing location failed\n");
		goto err;
	}

	/* read and parse the returned xml */
	doc = xmlReadMemory(res.s, res.len, 0, NULL,
			XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA);
	if(doc == NULL) {
		LM_WARN("invalid xml document: [%.*s]\n", res.len, res.s);
		doc = xmlRecoverMemory(res.s, res.len);
		if(doc == NULL) {
			LM_ERR("xml document recovery failed on: [%.*s]\n", res.len, res.s);
			goto err;
		}

		LM_DBG("xml document recovered\n");
	}
	root = xmlDocGetRootElement(doc);
	if(root == NULL) {
		LM_ERR("empty xml document\n");
		goto err;
	}

	/* check root element ... shall be presence|locationResponse, or error */
	if((!xmlStrcmp(root->name, (const xmlChar *)"presence"))
			|| (!xmlStrcmp(root->name, (const xmlChar *)"locationResponse"))) {

		LM_DBG("HELD location response [%.*s]\n", res.len, res.s);

		/* check content and set response code
		 * + 0 nothing found: return 200
		 * + 1 reference found: return 201
		 * + 2 value found: return 202
		 * + 3 value and reference found: return 203
		 */
		ret += lost_check_HeldResponse(root);
		/* error received */
	} else if(xmlStrcmp(root->name, (const xmlChar *)"error") == 0) {

		LM_DBG("HELD error response [%.*s]\n", res.len, res.s);

		/* get the error property */
		err.s = lost_get_property(root, (char *)"code", &err.len);
		if(err.len == 0) {
			LM_ERR("error - property not found: [%.*s]\n", res.len, res.s);
			goto err;
		}
		LM_WARN("locationRequest error response: [%.*s]\n", err.len, err.s);
	} else {
		LM_ERR("root element is not valid: [%.*s]\n", res.len, res.s);
		goto err;
	}

	/* clean up */
	xmlFreeDoc(doc);
	doc = NULL;

	/* set writeable pvars */
	pvpidf.rs = res;
	pvpidf.rs.s = res.s;
	pvpidf.rs.len = res.len;

	pvpidf.flags = PV_VAL_STR;
	pspidf = (pv_spec_t *)_pidf;
	pspidf->setf(_m, &pspidf->pvp, (int)EQ_T, &pvpidf);
	lost_free_string(&res); /* clean up */

	/* return error code in case of response error */
	if(err.len > 0) {
		ret = LOST_SERVER_ERROR;
	}
	pverr.rs = err;
	pverr.rs.s = err.s;
	pverr.rs.len = err.len;

	pverr.flags = PV_VAL_STR;
	pserr = (pv_spec_t *)_err;
	pserr->setf(_m, &pserr->pvp, (int)EQ_T, &pverr);
	lost_free_string(&err); /* clean up */

	return ret;

err:
	/* clean up xml */
	if(doc != NULL) {
		xmlFreeDoc(doc);
	}
	/* clean up string */
	if(res.s != NULL && res.len > 0) {
		lost_free_string(&res);
	}
	if(err.s != NULL && err.len > 0) {
		lost_free_string(&err);
	}

	return LOST_CLIENT_ERROR;
}

/*
 * lost_function(msg, con, pidf, uri, name, err, pidf, urn)
 * assembles and runs LOST findService request, parses results
 */
int lost_function(struct sip_msg *_m, char *_con, char *_uri, char *_name,
		char *_err, char *_pidf, char *_urn)
{
	pv_spec_t *psname;
	pv_spec_t *psuri;
	pv_spec_t *pserr;

	pv_value_t pvname;
	pv_value_t pvuri;
	pv_value_t pverr;

	p_lost_loc_t loc = NULL;
	p_lost_geolist_t geolist = NULL;
	p_lost_fsr_t fsrdata = NULL;

	str name = STR_NULL; /* return value displayName */
	str uri = STR_NULL;	 /* return value uri */
	str err = STR_NULL;	 /* return value error */

	str tmp = STR_NULL;
	str url = STR_NULL;
	str urn = STR_NULL;
	str req = STR_NULL;
	str con = STR_NULL;
	str ret = STR_NULL;
	str src = STR_NULL;
	str pidf = STR_NULL;
	str rereq = STR_NULL;
	str oldurl = STR_NULL;
	str losturl = STR_NULL;

	static str shttp = STR_STATIC_INIT(NAPTR_LOST_SERVICE_HTTP);
	static str shttps = STR_STATIC_INIT(NAPTR_LOST_SERVICE_HTTPS);

	struct msg_start *fl;

	char ustr[MAX_URI_SIZE];
	char *search = NULL;
	char *geoval = NULL;
	char *urlrep = NULL;
	char *heldreq = NULL;

	int geotype = 0;
	int redirect = 0;
	int curl = 0;
	int len = 0;
	int naptr = 0;
	int geoitems = 0;
	int res_error = 0;

	if(_con == NULL || _uri == NULL || _name == NULL || _err == NULL) {
		LM_ERR("invalid parameter\n");
		goto err;
	}
	/* connection from parameter */
	if(_con) {
		if(get_str_fparam(&con, _m, (gparam_p)_con) != 0) {
			LM_ERR("cannot get connection string\n");
			goto err;
		}
		/* check if connection exists */
		if(con.s != NULL && con.len > 0) {
			if(httpapi.http_connection_exists(&con) == 0) {
				LM_WARN("connection: [%.*s] does not exist\n", con.len, con.s);
				/* check if NAPTR lookup works with connection parameter */
				losturl.s = &(ustr[0]);
				losturl.len = MAX_URI_SIZE;
				if((naptr = lost_naptr_lookup(con, &shttps, &losturl)) == 0) {
					naptr = lost_naptr_lookup(con, &shttp, &losturl);
				}
				if(naptr == 0) {
					LM_ERR("NAPTR failed on [%.*s]\n", con.len, con.s);
					goto err;
				}
			}
		}
	}
	/* urn from parameter */
	if(_urn) {
		if(get_str_fparam(&urn, _m, (gparam_p)_urn) != 0) {
			LM_ERR("cannot get service urn parameter\n");
			goto err;
		}
	}
	/* urn from request line */
	if(urn.len == 0) {

		LM_DBG("no service urn parameter, trying request line ...\n");

		fl = &(_m->first_line);
		urn.len = fl->u.request.uri.len;
		urn.s = fl->u.request.uri.s;
	}
	/* check urn scheme */
	search = urn.s;
	if(is_urn(search) > 0) {
		LM_INFO("### LOST urn\t[%.*s]\n", urn.len, urn.s);
	} else {
		LM_ERR("service urn not found\n");
		goto err;
	}
	/* pidf from parameter */
	if(_pidf) {
		if(get_str_fparam(&pidf, _m, (gparam_p)_pidf) != 0) {
			LM_ERR("cannot get pidf parameter\n");
		} else {

			LM_DBG("parsing pidf parameter ...\n");

			if(pidf.s != NULL && pidf.len > 0) {

				LM_DBG("pidf: [%.*s]\n", pidf.len, pidf.s);

				/* parse the pidf and get loc object */
				loc = lost_parse_pidf(pidf, urn);
			}
		}
	}
	/* neither valid pidf parameter nor loc ... check geolocation header */
	if(loc == NULL) {

		/* parse Geolocation header */

		LM_DBG("parsing geolocation header ...\n");

		geolist = lost_get_geolocation_header(_m, &geoitems);

		if(geoitems == 0) {
			LM_ERR("geolocation header not found\n");
			goto err;
		}

		LM_DBG("number of location URIs: %d\n", geoitems);

		if(lost_geoloc_order == 0) {

			LM_DBG("reversing location URI sequence\n");

			lost_reverse_geoheader_list(&geolist);
		}
		switch(lost_geoloc_type) {
			case ANY: /* type: 0 */
				geoval = lost_get_geoheader_value(geolist, ANY, &geotype);

				LM_DBG("geolocation header field (any): %s\n", geoval);

				break;
			case CID: /* type: 1 */
				geoval = lost_get_geoheader_value(geolist, CID, &geotype);

				LM_DBG("geolocation header field (LbV): %s\n", geoval);

				break;
			case HTTP: /* type: 2 */
				geoval = lost_get_geoheader_value(geolist, HTTP, &geotype);
				/* fallback to https */
				if(geoval == NULL) {
					LM_WARN("no valid http URL ... trying https\n");
					geoval = lost_get_geoheader_value(geolist, HTTPS, &geotype);
				}

				LM_DBG("geolocation header field (LbR): %s\n", geoval);

				break;
			case HTTPS: /* type: 3 */
				/* prefer https */
				geoval = lost_get_geoheader_value(geolist, HTTPS, &geotype);
				/* fallback to http */
				if(geoval == NULL) {
					LM_WARN("no valid https URL ... trying http\n");
					geoval = lost_get_geoheader_value(geolist, HTTP, &geotype);
				}

				LM_DBG("geolocation header field (LbR): %s\n", geoval);

				break;
			default:
				LM_WARN("unknown module parameter value\n");
				geoval = lost_get_geoheader_value(geolist, UNKNOWN, &geotype);

				LM_DBG("geolocation header field (any): %s\n", geoval);

				break;
		}
		if(geoval == NULL) {
			LM_ERR("invalid geolocation header\n");
			goto err;
		}
		LM_INFO("### LOST loc\t[%s]\n", geoval);
		/* clean up */
		pidf.s = NULL;
		pidf.len = 0;
		/* use location by value */
		if(geotype == CID) {
			/* get body part - filter=>content-indirection */
			pidf.s = get_body_part_by_filter(_m, 0, 0, geoval, NULL, &pidf.len);
			if(pidf.s != NULL && pidf.len > 0) {

				LM_DBG("LbV pidf-lo: [%.*s]\n", pidf.len, pidf.s);

			} else {
				LM_WARN("no multipart body found\n");
			}
		}
		/* use location by reference */
		if((geotype == HTTPS) || (geotype == HTTP)) {
			url.s = geoval;
			url.len = strlen(geoval);
			/* ! dereference pidf.lo at location server - HTTP GET */
			/* ! requires hack in http_client module */
			/* ! functions.c => http_client_query => query_params.oneline = 0; */
			if(held_post_req == 0) {
				curl = httpapi.http_client_query_c(
						_m, url.s, &ret, NULL, mtheld, ACCEPT_HDR);
			} else {
				len = 0;
				heldreq = lost_held_post_request(&len, 0, NULL);
				if(heldreq == NULL) {
					LM_ERR("could not create POST request\n");
					goto err;
				}

				LM_DBG("POST request: [%.*s]\n", len, heldreq);

				curl = httpapi.http_client_query_c(
						_m, url.s, &ret, heldreq, mtheld, ACCEPT_HDR);
				pkg_free(heldreq); /* clean up */
				heldreq = NULL;
			}
			/* only HTTP 2xx responses are accepted */
			if(curl >= 300 || curl < 100) {
				if(held_post_req == 0) {
					LM_ERR("GET [%.*s] failed with error: %d\n", url.len, url.s,
							curl);
				} else {
					LM_ERR("POST [%.*s] failed with error: %d\n", url.len,
							url.s, curl);
				}
				/* clean up */
				lost_free_string(&ret);
				goto err;
			}
			url.s = NULL;
			url.len = 0;
			pidf.s = ret.s;
			pidf.len = ret.len;
			if(pidf.s != NULL && pidf.len > 0) {

				LM_DBG("LbR pidf-lo: [%.*s]\n", pidf.len, pidf.s);

			} else {
				LM_WARN("dereferencing location failed\n");
			}
		}
		/* clean up */
		lost_free_geoheader_list(&geolist);
		lost_free_string(&ret);

		if(pidf.s == NULL && pidf.len == 0) {
			LM_ERR("location object not found\n");
			goto err;
		}
		/* parse the pidf and get loc object */
		loc = lost_parse_pidf(pidf, urn);
	}
	/* pidf parsing failed ... return */
	if(loc == NULL) {
		LM_ERR("parsing pidf failed\n");
		goto err;
	}
	/* assemble findService request */
	req.s = lost_find_service_request(loc, NULL, &req.len);

	if(req.s == NULL && req.len == 0) {
		LM_ERR("lost request failed\n");
		goto err;
	}

	LM_DBG("findService request: [%.*s]\n", req.len, req.s);

	/* send findService request to mapping server - HTTP POST */
	if(naptr) {
		/* copy url */
		len = 0;
		urlrep = lost_copy_string(url, &len);
		if(urlrep == NULL) {
			LM_ERR("could not copy POST url\n");
			goto err;
		}
		/* send request */
		curl = httpapi.http_client_query(_m, urlrep, &ret, req.s, mtlost);
		pkg_free(urlrep); /* clean up */
		urlrep = NULL;
	} else {
		curl = httpapi.http_connect(_m, &con, NULL, &ret, mtlost, &req);
	}
	/* only HTTP 2xx responses are accepted */
	if(curl >= 300 || curl < 100) {
		if(naptr) {
			LM_ERR("POST [%.*s] failed with error: %d\n", url.len, url.s, curl);
		} else {
			LM_ERR("[%.*s] failed with error: %d\n", con.len, con.s, curl);
		}
		lost_free_string(&ret);
		goto err;
	}

	if(naptr) {
		LM_DBG("[%.*s] returned: %d\n", url.len, url.s, curl);
	} else {
		LM_DBG("[%.*s] returned: %d\n", con.len, con.s, curl);
	}

	if(ret.len == 0) {
		LM_ERR("findService request failed\n");
		goto err;
	}

	LM_DBG("findService response: [%.*s]\n", ret.len, ret.s);

	/* at least parse one response */
	redirect = 1;
	while(redirect) {
		fsrdata = lost_parse_findServiceResponse(ret);
		if(lost_verbose == 1) {
			lost_print_findServiceResponse(fsrdata);
		}
		switch(fsrdata->category) {
			case RESPONSE:
				if(fsrdata->uri != NULL) {
					/* get the first sips uri element ... */
					if(lost_search_response_list(&fsrdata->uri, &tmp.s, SIPS_S)
							> 0) {
						tmp.len = strlen(tmp.s);
						/* or the first sip uri element ... */
					} else if(lost_search_response_list(
									  &fsrdata->uri, &tmp.s, SIP_S)
							  > 0) {
						tmp.len = strlen(tmp.s);
						/* or return error if nothing found */
					} else {
						LM_ERR("sip/sips uri not found: [%.*s]\n", ret.len,
								ret.s);
						goto err;
					}
					/* copy uri string */
					if(pkg_str_dup(&uri, &tmp) < 0) {
						LM_ERR("could not copy: [%.*s]\n", tmp.len, tmp.s);
						goto err;
					}
				} else {
					LM_ERR("uri element not found: [%.*s]\n", ret.len, ret.s);
					goto err;
				}
				if(fsrdata->mapping != NULL) {
					/* get the displayName element */
					if((tmp.s = fsrdata->mapping->name->text) != NULL) {
						tmp.len = strlen(fsrdata->mapping->name->text);
						if(pkg_str_dup(&name, &tmp) < 0) {
							LM_ERR("could not copy: [%.*s]\n", tmp.len, tmp.s);
							goto err;
						}
					}
				} else {
					LM_ERR("name not found: [%.*s]\n", ret.len, ret.s);
					goto err;
				}
				/* we are done */
				redirect = 0;
				break;
			case ERROR:
				/* get the errors element */
				if(fsrdata->errors != NULL) {
					if((tmp.s = fsrdata->errors->issue->type) != NULL) {
						tmp.len = strlen(fsrdata->errors->issue->type);
						if(pkg_str_dup(&err, &tmp) < 0) {
							LM_ERR("could not copy: [%.*s]\n", tmp.len, tmp.s);
							goto err;
						}
					}
					/* clean up */
					tmp.s = NULL;
					tmp.len = 0;
				} else {
					LM_ERR("errors not found: [%.*s]\n", ret.len, ret.s);
					goto err;
				}
				/* we are done */
				redirect = 0;
				break;
			case REDIRECT:
				/* get the target element */
				if(fsrdata->redirect != NULL) {
					if((tmp.s = fsrdata->redirect->target) != NULL) {
						tmp.len = strlen(fsrdata->redirect->target);
						url.s = &(ustr[0]);
						url.len = MAX_URI_SIZE;
						/* check loop ... current response */
						if(oldurl.s != NULL && oldurl.len > 0) {
							if(str_strcasecmp(&tmp, &oldurl) == 0) {
								LM_ERR("loop detected: "
									   "[%.*s]<-->[%.*s]\n",
										oldurl.len, oldurl.s, tmp.len, tmp.s);
								goto err;
							}
						}
						/* add redirecting source to path list */
						if((src.s = fsrdata->redirect->source) != NULL) {
							src.len = strlen(fsrdata->redirect->source);
							if(lost_append_response_list(&fsrdata->path, src)
									== 0) {
								LM_ERR("could not append server to path "
									   "elememt\n");
								goto err;
							}
						}
						/* clean up */
						src.s = NULL;
						src.len = 0;
						/* check loop ... path elements */
						char *via = NULL;
						if(lost_search_response_list(
								   &fsrdata->path, &via, tmp.s)
								> 0) {
							LM_ERR("loop detected: "
								   "[%s]<-->[%.*s]\n",
									via, tmp.len, tmp.s);
							goto err;
						}
						/* remember the redirect target */
						if(pkg_str_dup(&oldurl, &tmp) < 0) {
							LM_ERR("could not copy: [%.*s]\n", tmp.len, tmp.s);
							goto err;
						}
						/* get url string via NAPTR */
						naptr = lost_naptr_lookup(tmp, &shttps, &url);
						if(naptr == 0) {
							/* fallback to http */
							naptr = lost_naptr_lookup(tmp, &shttp, &url);
						}
						/* nothing found ... return */
						if(naptr == 0) {
							LM_ERR("NAPTR failed on [%.*s]\n", tmp.len, tmp.s);
							goto err;
						}
						/* clean up */
						tmp.s = NULL;
						tmp.len = 0;

						/* assemble new findService request including path element */
						rereq.s = lost_find_service_request(
								loc, fsrdata->path, &rereq.len);
						/* clean up */
						lost_free_findServiceResponse(&fsrdata);
						lost_free_string(&ret);

						LM_DBG("findService request: [%.*s]\n", rereq.len,
								rereq.s);

						/* copy url */
						len = 0;
						urlrep = lost_copy_string(url, &len);
						if(urlrep == NULL) {
							LM_ERR("could not copy POST url\n");
							goto err;
						}
						/* send request */
						curl = httpapi.http_client_query(
								_m, urlrep, &ret, rereq.s, mtlost);
						/*clean up */
						pkg_free(urlrep);
						urlrep = NULL;
						lost_free_string(&rereq);

						/* only HTTP 2xx responses are accepted */
						if(curl >= 300 || curl < 100) {
							LM_ERR("POST [%.*s] failed with error: %d\n",
									url.len, url.s, curl);
							goto err;
						}
						/* reset url string */
						url.s = NULL;
						url.len = 0;
						/* once more ... we got a redirect */
						redirect = 1;
					}
				} else {
					LM_ERR("redirect element not found: [%.*s]\n", ret.len,
							ret.s);
					goto err;
				}
				break;
			case OTHER:
			default:
				LM_ERR("pidf is not valid: [%.*s]\n", ret.len, ret.s);
				goto err;
				break;
		}
	}

	/* clean up */
	lost_free_findServiceResponse(&fsrdata);
	lost_free_string(&ret);
	lost_free_string(&req);
	lost_free_string(&oldurl);
	lost_free_loc(&loc);

	/* set writable pvars */
	pvname.rs = name;
	pvname.rs.s = name.s;
	pvname.rs.len = name.len;

	pvname.flags = PV_VAL_STR;
	psname = (pv_spec_t *)_name;
	psname->setf(_m, &psname->pvp, (int)EQ_T, &pvname);
	lost_free_string(&name); /* clean up */

	pvuri.rs = uri;
	pvuri.rs.s = uri.s;
	pvuri.rs.len = uri.len;

	pvuri.flags = PV_VAL_STR;
	psuri = (pv_spec_t *)_uri;
	psuri->setf(_m, &psuri->pvp, (int)EQ_T, &pvuri);
	lost_free_string(&uri); /* clean up */

	/* return error code in case of response error */
	if(err.len > 0) {
		res_error = 1;
	}
	pverr.rs = err;
	pverr.rs.s = err.s;
	pverr.rs.len = err.len;

	pverr.flags = PV_VAL_STR;
	pserr = (pv_spec_t *)_err;
	pserr->setf(_m, &pserr->pvp, (int)EQ_T, &pverr);
	lost_free_string(&err); /* clean up */

	return (res_error > 0) ? LOST_SERVER_ERROR : LOST_SUCCESS;

err:
	/* clean up */
	lost_free_findServiceResponse(&fsrdata);
	lost_free_geoheader_list(&geolist);
	lost_free_loc(&loc);
	/* clean up string */
	if(oldurl.s != NULL && oldurl.len > 0) {
		lost_free_string(&oldurl);
	}
	if(ret.s != NULL && ret.len > 0) {
		lost_free_string(&ret);
	}
	if(req.s != NULL && req.len > 0) {
		lost_free_string(&req);
	}
	if(rereq.s != NULL && rereq.len > 0) {
		lost_free_string(&rereq);
	}
	if(name.s != NULL && name.len > 0) {
		lost_free_string(&name);
	}
	if(uri.s != NULL && uri.len > 0) {
		lost_free_string(&uri);
	}
	if(err.s != NULL && err.len > 0) {
		lost_free_string(&err);
	}

	return LOST_CLIENT_ERROR;
}
