/*
 * lost module functions
 *
 * Copyright (C) 2020 Wolfgang Kampichler
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

#define LOST_SUCCESS 200
#define LOST_CLIENT_ERROR 400
#define LOST_SERVER_ERROR 500

#define HELD_DEFAULT_TYPE "geodetic locationURI"
#define HELD_DEFAULT_TYPE_LEN (sizeof(HELD_DEFAULT_TYPE) - 1)

extern httpc_api_t httpapi;

extern int lost_geoloc_type;
extern int lost_geoloc_order;
extern int held_resp_time;
extern int held_exact_type;
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
	LM_ERR("no more private memory\n");
	if(ret != NULL) {
		pkg_free(ret);
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

	p_held_t held = NULL;

	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	xmlNodePtr cur_node = NULL;

	str did = {NULL, 0};
	str que = {NULL, 0};
	str con = {NULL, 0};
	str geo = {NULL, 0};
	str err = {NULL, 0};
	str res = {NULL, 0};
	str idhdr = {NULL, 0};
	str pidfuri = {NULL, 0};
	str rtype = {HELD_DEFAULT_TYPE, HELD_DEFAULT_TYPE_LEN};

	int curlres = 0;
	int presence = 0;

	if(_con == NULL || _pidf == NULL || _url == NULL || _err == NULL) {
		LM_ERR("invalid parameter\n");
		goto err;
	}
	/* module parameter */
	if(held_loc_type.len > 0) {
		rtype.s = held_loc_type.s;
		rtype.len = held_loc_type.len;
	}
	/* connection from parameter */
	if(fixup_get_svalue(_m, (gparam_p)_con, &con) != 0) {
		LM_ERR("cannot get connection string\n");
		goto err;
	}
	/* id from parameter */
	if(_id) {
		if(fixup_get_svalue(_m, (gparam_p)_id, &did) != 0) {
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

	/* check if connection exists */
	if(httpapi.http_connection_exists(&con) == 0) {
		LM_ERR("connection: [%s] does not exist\n", con.s);
		lost_free_string(&idhdr);
		goto err;
	}

	/* assemble locationRequest */
	held = lost_new_held(did, rtype, held_resp_time, held_exact_type);

	if(held == NULL) {
		LM_ERR("held object allocation failed\n");
		lost_free_string(&idhdr);
		goto err;
	}
	que.s = lost_held_location_request(held, &que.len);

	/* free memory */
	did.s = NULL;
	did.len = 0;
	lost_free_held(held);
	lost_free_string(&idhdr);

	if(que.len == 0) {
		LM_ERR("held request document error\n");
		que.s = NULL;
		goto err;
	}

	LM_DBG("held location request: [%s]\n", que.s);

	/* send locationRequest to location server - HTTP POST */
	curlres = httpapi.http_connect(_m, &con, NULL, &res, mtheld, &que);
	/* only HTTP 2xx responses are accepted */
	if(curlres >= 300 || curlres < 100) {
		LM_ERR("[%.*s] failed with error: %d\n", con.len, con.s, curlres);
		lost_free_string(&res);
		goto err;
	}

	LM_DBG("[%.*s] returned: %d\n", con.len, con.s, curlres);

	/* free memory */
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
	/* check the root element ... shall be locationResponse, or errors */
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
		if((presence == 0) && (geo.len > 0)) {

			LM_INFO("presence node not found in HELD response, trying URI "
					"...\n");

			curlres =
					httpapi.http_client_query(_m, geo.s, &pidfuri, NULL, NULL);
			/* only HTTP 2xx responses are accepted */
			if(curlres >= 300 || curlres < 100) {
				LM_ERR("dereferencing location failed: %d\n", curlres);
				/* free memory */
				lost_free_string(&pidfuri);
				goto err;
			}

			if(pidfuri.len == 0) {

				LM_WARN("HELD location request failed [%.*s]\n", geo.len,
						geo.s);

			} else {

				LM_DBG("HELD location response [%.*s]\n", pidfuri.len,
						pidfuri.s);

				res.s = pidfuri.s;
				res.len = pidfuri.len;
			}
		}
	} else if(xmlStrcmp(root->name, (const xmlChar *)"error") == 0) {

		LM_DBG("HELD error response [%.*s]\n", res.len, res.s);

		/* get the error patterm */
		err.s = lost_get_property(root, (char *)"code", &err.len);
		if(err.len == 0) {
			LM_ERR("error - code property not found: [%.*s]\n", res.len, res.s);
			goto err;
		}
		LM_WARN("locationRequest error response: [%.*s]\n", err.len, err.s);
	} else {
		LM_ERR("root element is not valid: [%.*s]\n", res.len, res.s);
		goto err;
	}
	xmlFreeDoc(doc);
	doc = NULL;

	/* set writeable pvars */
	pvpidf.rs = res;
	pvpidf.rs.s = res.s;
	pvpidf.rs.len = res.len;

	pvpidf.flags = PV_VAL_STR;
	pspidf = (pv_spec_t *)_pidf;
	pspidf->setf(_m, &pspidf->pvp, (int)EQ_T, &pvpidf);

	pvurl.rs = geo;
	pvurl.rs.s = geo.s;
	pvurl.rs.len = geo.len;

	pvurl.flags = PV_VAL_STR;
	psurl = (pv_spec_t *)_url;
	psurl->setf(_m, &psurl->pvp, (int)EQ_T, &pvurl);

	pverr.rs = err;
	pverr.rs.s = err.s;
	pverr.rs.len = err.len;

	pverr.flags = PV_VAL_STR;
	pserr = (pv_spec_t *)_err;
	pserr->setf(_m, &pserr->pvp, (int)EQ_T, &pverr);

	return (err.len > 0) ? LOST_SERVER_ERROR : LOST_SUCCESS;

err:
	if(doc != NULL) {
		xmlFreeDoc(doc);
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

	p_loc_t loc = NULL;
	p_geolist_t geolist = NULL;
	int geotype;

	str url = {NULL, 0};
	str uri = {NULL, 0};
	str urn = {NULL, 0};
	str err = {NULL, 0};
	str req = {NULL, 0};
	str con = {NULL, 0};
	str ret = {NULL, 0};
	str name = {NULL, 0};
	str pidf = {NULL, 0};
	str geohdr = {NULL, 0};
	str pidfhdr = {NULL, 0};

	struct msg_start *fl;
	char *search = NULL;
	char *geoval = NULL;
	int curlres = 0;
	int geoitems = 0;

	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;

	if(_con == NULL || _uri == NULL || _name == NULL || _err == NULL) {
		LM_ERR("invalid parameter\n");
		goto err;
	}
	if(fixup_get_svalue(_m, (gparam_p)_con, &con) != 0) {
		LM_ERR("cannot get connection string\n");
		goto err;
	}
	/* urn from parameter */
	if(_urn) {
		if(fixup_get_svalue(_m, (gparam_p)_urn, &urn) != 0) {
			LM_ERR("cannot get service urn\n");
			goto err;
		}
	}
	/* urn from request line */
	if(urn.len == 0) {
		LM_WARN("no sevice urn parameter, trying request line ...\n");
		fl = &(_m->first_line);
		urn.len = fl->u.request.uri.len;
		urn.s = fl->u.request.uri.s;
	}
	/* check urn scheme */
	if(urn.len > 3) {
		search = urn.s;
		if(((*(search + 0) == 'u') || (*(search + 0) == 'U'))
				&& ((*(search + 1) == 'r') || (*(search + 1) == 'R'))
				&& ((*(search + 2) == 'n') || (*(search + 2) == 'N'))
				&& (*(search + 3) == ':')) {
			LM_INFO("### LOST urn\t[%.*s]\n", urn.len, urn.s);
		} else {
			LM_ERR("service urn not found\n");
			goto err;
		}
	} else {
		LM_ERR("service urn not found\n");
		goto err;
	}
	/* pidf from parameter */
	if(_pidf) {
		if(fixup_get_svalue(_m, (gparam_p)_pidf, &pidf) != 0) {
			LM_WARN("cannot get pidf-lo parameter\n");
		} else {

			LM_DBG("parsing pidf-lo from paramenter\n");

			if(pidf.len > 0) {

				LM_DBG("pidf-lo: [%.*s]\n", pidf.len, pidf.s);

				/* parse the pidf-lo */
				loc = lost_parse_pidf(pidf, urn);
				/* free memory */
				pidf.s = NULL;
				pidf.len = 0;
			} else {
				LM_WARN("no valid pidf parameter ...\n");
			}
		}
	}

	/* no pidf-lo so far ... check geolocation header */
	if(loc == NULL) {

		LM_DBG("looking for geolocation header ...\n");

		geohdr.s = lost_get_geolocation_header(_m, &geohdr.len);
		if(geohdr.len == 0) {
			LM_ERR("geolocation header not found\n");
			goto err;
		}

		LM_DBG("geolocation header found\n");

		/* parse Geolocation header */
		geolist = lost_new_geoheader_list(geohdr, &geoitems);
		if(geoitems == 0) {
			LM_ERR("invalid geolocation header\n");
			lost_free_string(&geohdr);
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
			/* free memory */
			lost_delete_geoheader_list(geolist);
			lost_free_string(&geohdr);
			goto err;
		}

		LM_INFO("### LOST loc\t[%s]\n", geoval);

		/* use location by value */
		if(geotype == CID) {

			/* get body part - filter=>content-indirection */
			pidf.s = get_body_part_by_filter(_m, 0, 0, geoval, NULL, &pidf.len);
			if(pidf.len > 0) {

				LM_DBG("LbV pidf-lo: [%.*s]\n", pidf.len, pidf.s);

				/* parse the pidf-lo */
				loc = lost_parse_pidf(pidf, urn);
				/* free memory */
				pidf.s = NULL;
				pidf.len = 0;
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
			curlres =
					httpapi.http_client_query(_m, url.s, &pidfhdr, NULL, NULL);
			/* free memory */
			url.s = NULL;
			url.len = 0;
			/* only HTTP 2xx responses are accepted */
			if(curlres >= 300 || curlres < 100) {
				LM_ERR("http GET failed with error: %d\n", curlres);
				/* free memory */
				lost_delete_geoheader_list(geolist);
				lost_free_string(&pidfhdr);
				lost_free_string(&geohdr);
				goto err;
			}

			pidf.s = pidfhdr.s;
			pidf.len = pidfhdr.len;

			if(pidf.len > 0) {

				LM_DBG("LbR pidf-lo: [%.*s]\n", pidf.len, pidf.s);

				/* parse the pidf-lo */
				loc = lost_parse_pidf(pidf, urn);
				/* free memory */
				pidf.s = NULL;
				pidf.len = 0;
			} else {
				LM_WARN("dereferencing location failed\n");
			}
		}
		/* free memory */
		lost_delete_geoheader_list(geolist);
		lost_free_string(&geohdr);
		lost_free_string(&pidfhdr);
	}

	if(loc == NULL) {
		LM_ERR("location object not found\n");
		goto err;
	}

	/* check if connection exits */
	if(httpapi.http_connection_exists(&con) == 0) {
		LM_ERR("connection: [%.*s] does not exist\n", con.len, con.s);
		goto err;
	}
	/* assemble findService request */
	req.s = lost_find_service_request(loc, &req.len);
	/* free memory */
	lost_free_loc(loc);
	loc = NULL;

	if(req.len == 0) {
		LM_ERR("lost request failed\n");
		goto err;
	}

	LM_DBG("findService request: [%.*s]\n", req.len, req.s);

	/* send findService request to mapping server - HTTP POST */
	curlres = httpapi.http_connect(_m, &con, NULL, &ret, mtlost, &req);
	/* only HTTP 2xx responses are accepted */
	if(curlres >= 300 || curlres < 100) {
		LM_ERR("[%.*s] failed with error: %d\n", con.len, con.s, curlres);
		lost_free_string(&ret);
		goto err;
	}

	LM_DBG("[%.*s] returned: %d\n", con.len, con.s, curlres);

	/* free memory */
	lost_free_string(&req);

	if(ret.len == 0) {
		LM_ERR("findService request failed\n");
		goto err;
	}

	LM_DBG("findService response: [%.*s]\n", ret.len, ret.s);

	/* read and parse the returned xml */
	doc = xmlReadMemory(ret.s, ret.len, 0, 0,
			XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA);

	if(doc == NULL) {
		LM_ERR("invalid xml document: [%.*s]\n", ret.len, ret.s);
		doc = xmlRecoverMemory(ret.s, ret.len);
		if(doc == NULL) {
			LM_ERR("xml document recovery failed on: [%.*s]\n", ret.len, ret.s);
			goto err;
		}

		LM_DBG("xml document recovered\n");
	}
	root = xmlDocGetRootElement(doc);
	if(root == NULL) {
		LM_ERR("empty xml document: [%.*s]\n", ret.len, ret.s);
		/* free memory */
		lost_free_string(&ret);
		goto err;
	}
	/* check the root element, shall be findServiceResponse, or errors */
	if((!xmlStrcmp(root->name, (const xmlChar *)"findServiceResponse"))) {
		/* get the uri element */
		uri.s = lost_get_content(root, uri_element, &uri.len);
		if(uri.len == 0) {
			LM_ERR("uri element not found: [%.*s]\n", ret.len, ret.s);
			/* free memory */
			lost_free_string(&ret);
			goto err;
		}
		LM_INFO("### LOST uri\t[%.*s]\n", uri.len, uri.s);
		/* get the displayName element */
		name.s = lost_get_content(root, name_element, &name.len);
		if(name.len == 0) {
			LM_ERR("displayName element not found: [%.*s]\n", ret.len, ret.s);
			/* free memory */
			lost_free_string(&ret);
			goto err;
		}
		LM_INFO("### LOST din\t[%.*s]\n", name.len, name.s);
	} else if((!xmlStrcmp(root->name, (const xmlChar *)"errors"))) {

		LM_DBG("findService error response received\n");

		/* get the error patterm */
		err.s = lost_get_childname(root, errors_element, &err.len);
		LM_DBG("findService error response: [%.*s]\n", err.len, err.s);
		if(err.len == 0) {
			LM_ERR("error pattern element not found: [%.*s]\n", ret.len, ret.s);
			/* free memory */
			lost_free_string(&ret);
			goto err;
		}
		LM_WARN("findService error response: [%.*s]\n", err.len, err.s);
	} else {
		LM_ERR("root element is not valid: [%.*s]\n", ret.len, ret.s);
		/* free memory */
		lost_free_string(&ret);
		goto err;
	}

	/* free memory */
	lost_free_string(&ret);

	/* set writable pvars */
	pvname.rs = name;
	pvname.rs.s = name.s;
	pvname.rs.len = name.len;

	pvname.flags = PV_VAL_STR;
	psname = (pv_spec_t *)_name;
	psname->setf(_m, &psname->pvp, (int)EQ_T, &pvname);

	pvuri.rs = uri;
	pvuri.rs.s = uri.s;
	pvuri.rs.len = uri.len;

	pvuri.flags = PV_VAL_STR;
	psuri = (pv_spec_t *)_uri;
	psuri->setf(_m, &psuri->pvp, (int)EQ_T, &pvuri);

	pverr.rs = err;
	pverr.rs.s = err.s;
	pverr.rs.len = err.len;

	pverr.flags = PV_VAL_STR;
	pserr = (pv_spec_t *)_err;
	pserr->setf(_m, &pserr->pvp, (int)EQ_T, &pverr);

	return (err.len > 0) ? LOST_SERVER_ERROR : LOST_SUCCESS;

err:
	/* free memory */
	if(doc != NULL) {
		xmlFreeDoc(doc);
		doc = NULL;
	}
	if(loc != NULL) {
		lost_free_loc(loc);
	}

	return LOST_CLIENT_ERROR;
}
