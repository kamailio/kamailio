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
	/* clean up */
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
	str pidfuri = STR_NULL;

	static str rtype = STR_STATIC_INIT(HELD_DEFAULT_TYPE);
	static str sheld = STR_STATIC_INIT(NAPTR_LIS_SERVICE_HELD);

	char ustr[MAX_URI_SIZE];
	char istr[NI_MAXHOST];
	char *ipstr = NULL;
	char *lisurl = NULL;

	int len = 0;
	int curl = 0;
	int flag = 0;
	int naptr = 0;
	int presence = 0;

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
		if(fixup_get_svalue(_m, (gparam_p)_con, &con) != 0) {
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
	/* assemble locationRequest */
	held = lost_new_held(did, rtype, held_resp_time, held_exact_type);
	if(held == NULL) {
		LM_ERR("held object allocation failed\n");
		lost_free_string(&idhdr); /* clean up */
		goto err;
	}
	que.s = lost_held_location_request(held, &que.len);
	lost_free_held(held); /* clean up */
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
			if(len > 0) {
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
			}
			url.s = &(ustr[0]);
			url.len = MAX_URI_SIZE;
			if((naptr = lost_naptr_lookup(host, &sheld, &url)) == 0) {
				LM_ERR("NAPTR failed on [%.*s]\n", host.len, host.s);
				lost_free_string(&que); /* clean up */
				lost_free_string(&idhdr);
				goto err;
			}
		} else {
			LM_ERR("failed to get location service for [%.*s]\n", did.len, did.s);
			lost_free_string(&que); /* clean up */
			lost_free_string(&idhdr);
			goto err;
		}

		LM_DBG("NATPR lookup returned [%.*s]\n", url.len, url.s);

		/* curl doesn't like str */
		len = 0;
		lisurl = lost_copy_string(url, &len);
		/* send to service */
		if(len > 0) {
			curl = httpapi.http_client_query(_m, lisurl, &res, que.s, mtheld);
			pkg_free(lisurl); /*clean up */
		} else {
			goto err;	
		}
	}
	/* only HTTP 2xx responses are accepted */
	if(curl >= 300 || curl < 100) {
		if(con.s != NULL && con.len > 0) {
			LM_ERR("[%.*s] failed with error: %d\n", con.len, con.s, curl);
		} else {
			LM_ERR("[%.*s] failed with error: %d\n", url.len, url.s, curl);
		}
		lost_free_string(&res);
		goto err;
	}

	LM_DBG("[%.*s] returned: %d\n", con.len, con.s, curl);

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
				if(xmlStrcmp(cur_node->name, (const xmlChar *)"presence") == 0) {

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
			curl = httpapi.http_client_query(_m, geo.s, &pidfuri, NULL, NULL);
			/* only HTTP 2xx responses are accepted */
			if(curl >= 300 || curl < 100) {
				LM_ERR("dereferencing location failed: %d\n", curl);
				/* clean up */
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
	p_fsr_t fsrdata = NULL;

	str name = STR_NULL; /* return value displayName */
	str uri = STR_NULL;	 /* return value uri */
	str err = STR_NULL;	 /* return value error */

	str tmp = STR_NULL;
	str url = STR_NULL;
	str urn = STR_NULL;
	str req = STR_NULL;
	str con = STR_NULL;
	str ret = STR_NULL;
	str pidf = STR_NULL;
	str geohdr = STR_NULL;
	str oldurl = STR_NULL;
	str losturl = STR_NULL;

	static str shttp = STR_STATIC_INIT(NAPTR_LOST_SERVICE_HTTP);
	static str shttps = STR_STATIC_INIT(NAPTR_LOST_SERVICE_HTTPS);

	struct msg_start *fl;

	char ustr[MAX_URI_SIZE];
	char *search = NULL;
	char *geoval = NULL;

	int geotype = 0;
	int redirect = 0;
	int curl = 0;
	int naptr = 0;
	int geoitems = 0;

	if(_con == NULL || _uri == NULL || _name == NULL || _err == NULL) {
		LM_ERR("invalid parameter\n");
		goto err;
	}
	/* connection from parameter */
	if(_con) {
		if(fixup_get_svalue(_m, (gparam_p)_con, &con) != 0) {
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
		if(fixup_get_svalue(_m, (gparam_p)_urn, &urn) != 0) {
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
		if(fixup_get_svalue(_m, (gparam_p)_pidf, &pidf) != 0) {
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
	/* neither valifd pidf parameter nor loc ... check geolocation header */
	if(loc == NULL) {

		LM_DBG("looking for geolocation header ...\n");

		if(lost_get_geolocation_header(_m, &geohdr) == 0) {
			LM_ERR("geolocation header not found\n");
		}

		LM_DBG("parsing geolocation header ...\n");

		/* parse Geolocation header */
		geolist = lost_new_geoheader_list(geohdr, &geoitems);
		if(geoitems == 0) {
			LM_ERR("invalid geolocation header\n");
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
			curl = httpapi.http_client_query(_m, url.s, &ret, NULL, NULL);
			url.s = NULL;
			url.len = 0;
			/* only HTTP 2xx responses are accepted */
			if(curl >= 300 || curl < 100) {
				LM_ERR("http GET failed with error: %d\n", curl);
				/* clean up */
				lost_free_string(&ret);
				goto err;
			}
			pidf.s = ret.s;
			pidf.len = ret.len;
			if(pidf.s != NULL && pidf.len > 0) {

				LM_DBG("LbR pidf-lo: [%.*s]\n", pidf.len, pidf.s);

			} else {
				LM_WARN("dereferencing location failed\n");
			}
		}
		/* clean up */
		lost_delete_geoheader_list(geolist);
		lost_free_string(&ret);
	}
	if(pidf.s == NULL && pidf.len == 0) {
		LM_ERR("location object not found\n");
		goto err;
	}
	/* parse the pidf and get loc object */
	loc = lost_parse_pidf(pidf, urn);
	if(loc == NULL) {
		LM_ERR("parsing pidf failed\n");
		goto err;
	}
	/* assemble findService request */
	req.s = lost_find_service_request(loc, &req.len);
	lost_free_loc(loc); /* clean up */
	if(req.s == NULL && req.len == 0) {
		LM_ERR("lost request failed\n");
		goto err;
	}

	LM_DBG("findService request: [%.*s]\n", req.len, req.s);

	/* send findService request to mapping server - HTTP POST */
	if(naptr) {
		curl = httpapi.http_client_query(_m, url.s, &ret, req.s, mtlost);
	} else {
		curl = httpapi.http_connect(_m, &con, NULL, &ret, mtlost, &req);
	}
	/* only HTTP 2xx responses are accepted */
	if(curl >= 300 || curl < 100) {
		LM_ERR("[%.*s] failed with error: %d\n", con.len, con.s, curl);
		lost_free_string(&ret);
		goto err;
	}

	LM_DBG("[%.*s] returned: %d\n", con.len, con.s, curl);

	if(ret.len == 0) {
		LM_ERR("findService request failed\n");
		goto err;
	}

	LM_DBG("findService response: [%.*s]\n", ret.len, ret.s);

	/* at least parse one request */
	redirect = 1;
	while(redirect) {
		fsrdata = lost_parse_findServiceResponse(ret);
		lost_print_findServiceResponse(fsrdata);
		switch(fsrdata->category) {
			case RESPONSE:
				if(fsrdata->uri != NULL) {
					/* get the first uri element */
					if((tmp.s = fsrdata->uri->value) != NULL) {
						tmp.len = strlen(fsrdata->uri->value);
						uri.s = lost_copy_string(tmp, &uri.len);
					}
				} else {
					LM_ERR("uri not found: [%.*s]\n", ret.len, ret.s);
					goto err;
				}
				if(fsrdata->mapping != NULL) {
					/* get the displayName element */
					if((tmp.s = fsrdata->mapping->name->text) != NULL) {
						tmp.len = strlen(fsrdata->mapping->name->text);
						name.s = lost_copy_string(tmp, &name.len);
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
						err.s = lost_copy_string(tmp, &err.len);
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
						if((naptr = lost_naptr_lookup(tmp, &shttps, &url))
								== 0) {
							naptr = lost_naptr_lookup(tmp, &shttp, &url);
						}
						if(naptr == 0) {
							LM_ERR("NAPTR failed on [%.*s]\n", tmp.len, tmp.s);
							goto err;
						}
						/* clean up */
						tmp.s = NULL;
						tmp.len = 0;
						/* check loop */
						if(oldurl.s != NULL && oldurl.len > 0) {
							if(str_strcasecmp(&url, &oldurl) == 0) {
								LM_ERR("loop detected: "
									   "[%.*s]<-->[%.*s]\n",
										oldurl.len, oldurl.s, url.len, url.s);
								goto err;
							}
						}
						/* remember the redirect target */
						oldurl.s = lost_copy_string(url, &oldurl.len);
						/* clean up */
						lost_free_findServiceResponse(fsrdata);
						lost_free_string(&ret);
						/* send request */
						curl = httpapi.http_client_query(
								_m, url.s, &ret, req.s, mtlost);
						url.s = NULL;
						url.len = 0;
						/* only HTTP 2xx responses are accepted */
						if(curl >= 300 || curl < 100) {
							LM_ERR("http GET failed with error: %d\n", curl);
							goto err;
						}
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
	lost_free_findServiceResponse(fsrdata);
	lost_free_string(&ret);
	lost_free_string(&req);
	lost_free_string(&oldurl);

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
	/* clean up */
	if(fsrdata != NULL) {
		lost_free_findServiceResponse(fsrdata);
	}
	if(geolist != NULL) {
		lost_delete_geoheader_list(geolist);
	}
	if(oldurl.s != NULL) {
		lost_free_string(&oldurl);
	}
	if(loc != NULL) {
		lost_free_loc(loc);
	}
	if(ret.s != NULL && ret.len > 0) {
		lost_free_string(&ret);
	}
	if(req.s != NULL && req.len > 0) {
		lost_free_string(&req);
	}

	return LOST_CLIENT_ERROR;
}
