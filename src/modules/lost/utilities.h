/*
 * lost module utility functions
 *
 * Copyright (C) 2021 Wolfgang Kampichler
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

#ifndef LOST_UTILITIES_H
#define LOST_UTILITIES_H

#include "response.h"

#define LAQUOT '<'
#define COLON ':'

#define LOST_GEOLOC_HEADER "Geolocation: "
#define LOST_GEOLOC_HEADER_SIZE strlen(LOST_GEOLOC_HEADER)
#define LOST_PAI_HEADER "P-Asserted-Identity: "
#define LOST_PAI_HEADER_SIZE strlen(LOST_PAI_HEADER)

#define LOST_BOUNDARY_TRUE 1
#define LOST_BOUNDARY_FALSE 0
#define LOST_XPATH_DPTH 3
#define LOST_XPATH_GP "//gp:location-info/*"

#define XPATH_NS                                         \
	"gp=urn:ietf:params:xml:ns:pidf:geopriv10"           \
	" "                                                  \
	"xmlns=urn:ietf:params:xml:ns:pidf"                  \
	" "                                                  \
	"ca=urn:ietf:params:xml:ns:pidf:geopriv10:civicAddr" \
	" "                                                  \
	"gm=http://www.opengis.net/gml"

#define LOST_PRO_GEO2D "geodetic-2d"
#define LOST_PRO_CIVIC "civic"

#define LOST_PNT "Point"
#define LOST_CIR "Circle"
#define LOST_CIV "civicAddress"

#define HELD_TYPE_ANY "any"
#define HELD_TYPE_CIV "civic"
#define HELD_TYPE_GEO "geodetic"
#define HELD_TYPE_URI "locationURI"
#define HELD_TYPE_SEP " "

#define HELD_LR "locationRequest"
#define HELD_LT "locationType"
#define HELD_RT "responseTime"
#define HELD_ED "emergencyDispatch"
#define HELD_ER "emergencyRouting"

#define HELD_EXACT_TRUE 1
#define HELD_EXACT_FALSE 0

#define BUFSIZE 128	   /* temporary buffer to hold geolocation */
#define RANDSTRSIZE 16 /* temporary id in a findService request */

typedef struct lost_loc
{
	char *identity;	 /* location idendity (findServiceRequest) */
	char *urn;		 /* service URN (findServiceRequest) */
	char *xpath;	 /* civic address (findServiceRequest) */
	char *geodetic;	 /* geodetic location (findServiceRequest) */
	char *longitude; /* geo longitude */
	char *latitude;	 /* geo latitude */
	char *profile;	 /* location profile (findServiceRequest) */
	int radius;		 /* geo radius (findServiceRequest) */
	int recursive;	 /* recursion true|false (findServiceRequest)*/
	int boundary;	 /* boundary ref|value (findServiceRequest)*/
} s_lost_loc_t, *p_lost_loc_t;

typedef struct lost_held
{
	char *identity; /* location idendity (locationRequest) */
	char *type;		/* location type (locationRequest) */
	int time;		/* response time (locationRequest) */
	int exact;		/* exact true|false (locationRequest)*/
} s_lost_held_t, *p_lost_held_t;

typedef enum lost_geotype
{
	ANY,		 /* any type */
	CID,		 /* content-indirection */
	HTTP,		 /* http uri */
	HTTPS,		 /* https uri */
	UNKNOWN = -1 /* unknown */
} lost_geotype_t;

typedef struct lost_geolist
{
	char *value;		 /* geolocation header value */
	char *param;		 /* value parameter */
	lost_geotype_t type; /* type */
	struct lost_geolist *next;
} s_lost_geolist_t, *p_lost_geolist_t;

void lost_rand_str(char *, size_t);
void lost_free_loc(p_lost_loc_t *);
void lost_free_held(p_lost_held_t *);
void lost_free_string(str *);
void lost_free_geoheader_list(p_lost_geolist_t *);
void lost_reverse_geoheader_list(p_lost_geolist_t *);

int lost_parse_location_info(xmlNodePtr, p_lost_loc_t);
int lost_xpath_location(xmlDocPtr, char *, p_lost_loc_t);
int lost_parse_geo(xmlNodePtr, p_lost_loc_t);
int lost_parse_host(const char *, str *, int *);
int lost_new_geoheader_list(p_lost_geolist_t *, str);
int lost_get_nameinfo(char *, str *, int);

char *lost_find_service_request(p_lost_loc_t, p_lost_list_t, int *);
char *lost_held_location_request(p_lost_held_t, int *);
char *lost_held_post_request(int *, long, char *);
char *lost_get_content(xmlNodePtr, const char *, int *);
char *lost_get_property(xmlNodePtr, const char *, int *);
char *lost_get_from_header(struct sip_msg *, int *);
char *lost_get_pai_header(struct sip_msg *, int *);
char *lost_get_childname(xmlNodePtr, const char *, int *);
char *lost_trim_content(char *, int *);
char *lost_copy_geoheader_value(char *, int);
char *lost_get_geoheader_value(p_lost_geolist_t, lost_geotype_t, int *);
char *lost_copy_string(str, int *);

p_lost_loc_t lost_new_loc(str);
p_lost_loc_t lost_parse_pidf(str, str);
p_lost_held_t lost_new_held(str, str, int, int);
p_lost_geolist_t lost_get_geolocation_header(struct sip_msg *, int *);

#endif
