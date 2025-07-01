/**
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

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/kemi.h"
#include "../../core/mod_fix.h"
#include "../../core/rpc_lookup.h"

#include "geoip2_pv.h"

MODULE_VERSION

#define MAX_GEO_STR_SIZE 512
#define EARTH_RADIUS (6371.0072 * 0.6214)
#define TORADS(degrees) (degrees * (M_PI / 180))

static char *geoip2_path = NULL;

static int mod_init(void);
static void mod_destroy(void);
static int geoip2_rpc_init(void);

static int w_geoip2_match(struct sip_msg *msg, char *str1, char *str2);
static int geoip2_resid_param(modparam_t type, void *val);

static int w_geoip2_distance(
		struct sip_msg *msg, char *str1, char *str2, char *str3);

/* clang-format off */
static pv_export_t mod_pvs[] = {
	{{"gip2", sizeof("gip2") - 1}, PVT_OTHER, pv_get_geoip2, 0,
			pv_parse_geoip2_name, 0, 0, 0},

	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static cmd_export_t cmds[] = {
	{"geoip2_match", (cmd_function)w_geoip2_match, 2,
			fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"geoip2_distance", (cmd_function)w_geoip2_distance, 3,
			fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"path", PARAM_STRING, &geoip2_path},
	{"resid", PARAM_STR | PARAM_USE_FUNC, &geoip2_resid_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"geoip2",		 /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,			 /* exported functions */
	params,			 /* exported parameters */
	0,				 /* RPC method exports */
	mod_pvs,		 /* exported pseudo-variables */
	0,				 /* response handling function */
	mod_init,		 /* module initialization function */
	0,				 /* per-child init function */
	mod_destroy		 /* module destroy function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	LM_INFO("using GeoIP database path %s, library version %s\n", geoip2_path,
			MMDB_lib_version());

	if(geoip2_path == NULL || strlen(geoip2_path) == 0) {
		LM_ERR("path to GeoIP database file not set\n");
		return -1;
	}

	if(geoip2_init_pv(geoip2_path) != 0) {
		LM_ERR("cannot init for database file at: %s\n", geoip2_path);
		return -1;
	}

	if(geoip2_rpc_init() < 0) {
		LM_ERR("error during RPC initialization\n");
		return -1;
	}

	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	geoip2_destroy_pv();
}

static int geoip2_resid_param(modparam_t type, void *val)
{
	str rname;

	rname = *((str *)val);
	if(sr_geoip2_add_resid(&rname) < 0) {
		LM_ERR("failed to register result container with id: %.*s\n", rname.len,
				rname.s);
		return -1;
	}
	return 0;
}

static int ki_geoip2_match(sip_msg_t *msg, str *tomatch, str *pvclass)
{
	geoip2_pv_reset(pvclass);
	geoip2_reload_pv(geoip2_path);

	return geoip2_update_pv(tomatch, pvclass);
}

static int w_geoip2_match(sip_msg_t *msg, char *target, char *pvname)
{
	str tomatch = STR_NULL;
	str pvclass = STR_NULL;

	if(msg == NULL) {
		LM_ERR("received null msg\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)target, &tomatch) < 0) {
		LM_ERR("cannot get the address\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pvname, &pvclass) < 0) {
		LM_ERR("cannot get the pv class\n");
		return -1;
	}

	return ki_geoip2_match(msg, &tomatch, &pvclass);
}

static int geoip2_distance(
		sip_msg_t *msg, str *_ip_addr, double lat, double lon)
{
	char ip_addr[MAX_GEO_STR_SIZE] = {0};
	double lat1, lon1, lat2, lon2, orig_lat2, orig_lon2;
	double d_lat, d_lon, a, c;
	int dist = 0;
	int gai_error, mmdb_error;
	MMDB_lookup_result_s record;
	MMDB_entry_data_s entry_data;
	gen_lock_t *lock = get_gen_lock();

	if(lock == NULL) {
		LM_ERR("error GeoIP mutex is not initialized\n");
		return -1;
	}

	LM_DBG("ip_addr=%.*s lat=%f lon=%f\n", _ip_addr->len, _ip_addr->s, lat,
			lon);

	strncpy(ip_addr, _ip_addr->s, _ip_addr->len);

	MMDB_s *geoip_handle = get_geoip_handle();
	if(geoip_handle == NULL) {
		LM_ERR("error GeoIP handle is not initialized\n");
		return -1;
	}

	lock_get(lock);
	record = MMDB_lookup_string(
			geoip_handle, (const char *)ip_addr, &gai_error, &mmdb_error);

	LM_DBG("attempt to match: %s\n", ip_addr);
	if(gai_error || MMDB_SUCCESS != mmdb_error || !record.found_entry) {
		LM_DBG("no match for: %s\n", ip_addr);
		lock_release(lock);
		return -2;
	}

	if(MMDB_get_value(&record.entry, &entry_data, "location", "latitude", NULL)
			!= MMDB_SUCCESS) {
		LM_ERR("no location/latitude for: %s\n", ip_addr);
		lock_release(lock);
		return -2;
	}
	if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_DOUBLE) {
		orig_lat2 = entry_data.double_value;
	} else {
		LM_ERR("wrong format for location/latitude\n");
		lock_release(lock);
		return -3;
	}

	if(MMDB_get_value(&record.entry, &entry_data, "location", "longitude", NULL)
			!= MMDB_SUCCESS) {
		LM_ERR("no location/longitude for: %s\n", ip_addr);
		lock_release(lock);
		return -2;
	}
	lock_release(lock);
	if(entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_DOUBLE) {
		orig_lon2 = entry_data.double_value;
	} else {
		LM_ERR("wrong format for location/latitude\n");
		return -3;
	}

	LM_INFO("for ip_addr=%s the following coordinates: lat=%f lon=%f\n",
			ip_addr, orig_lat2, orig_lon2);

	lat1 = TORADS(lat);
	lon1 = TORADS(lon);
	lat2 = TORADS(orig_lat2);
	lon2 = TORADS(orig_lon2);

	d_lat = lat2 - lat1;
	d_lon = lon2 - lon1;

	a = sin(d_lat / 2) * sin(d_lat / 2)
		+ cos(lat1) * cos(lat2) * sin(d_lon / 2) * sin(d_lon / 2);
	c = 2 * atan2(sqrt(a), sqrt(1 - a));

	dist = (int)(EARTH_RADIUS * c);

	LM_DBG("lat1=%f lon1=%f lat2=%f lon2=%f distance = %d\n", lat, lon,
			orig_lat2, orig_lon2, dist);

	return dist;
}

static int ki_geoip2_distance(
		sip_msg_t *msg, str *_ipaddr, str *_lat, str *_lon)
{
	double lat = 0;
	double lon = 0;
	char buf[MAX_GEO_STR_SIZE] = {0};

	strncpy(buf, _lat->s, _lat->len);
	lat = atof(buf);
	if(!lat && errno == ERANGE) {
		LM_ERR("cannot convert string to double: %.*s\n", _lat->len, _lat->s);
		return -1;
	}

	memset(buf, 0, MAX_GEO_STR_SIZE);
	strncpy(buf, _lon->s, _lon->len);
	lon = atof(buf);
	if(!lon && errno == ERANGE) {
		LM_ERR("cannot convert string to double: %.*s\n", _lon->len, _lon->s);
		return -1;
	}

	geoip2_reload_pv(geoip2_path);

	return geoip2_distance(msg, _ipaddr, lat, lon);
}

static int w_geoip2_distance(
		sip_msg_t *msg, char *ip_addr_param, char *lat_param, char *lon_param)
{
	str ip_addr_str = STR_NULL;
	str lat_str = STR_NULL;
	str lon_str = STR_NULL;
	if(fixup_get_svalue(msg, (gparam_t *)ip_addr_param, &ip_addr_str) < 0) {
		LM_ERR("cannot get the IP address\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)lat_param, &lat_str) < 0) {
		LM_ERR("cannot get latitude string\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)lon_param, &lon_str) < 0) {
		LM_ERR("cannot get longitude string\n");
		return -1;
	}

	return ki_geoip2_distance(msg, &ip_addr_str, &lat_str, &lon_str);
}

static void geoip2_rpc_reload(rpc_t *rpc, void *ctx)
{
	if(geoip2_reload_set() < 0) {
		rpc->fault(ctx, 500, "Reload failed");
		return;
	}
}

/* clang-format off */
static const char *geoip2_rpc_reload_doc[2] = {
	"Reload GeoIP2 database file.",
	0
};

rpc_export_t ubl_rpc[] = {
	{"geoip2.reload", geoip2_rpc_reload, geoip2_rpc_reload_doc, 0},
	{0, 0, 0, 0}
};
/* clang-format on */

static int geoip2_rpc_init(void)
{
	if(rpc_register_array(ubl_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_geoip2_exports[] = {
    { str_init("geoip2"), str_init("match"),
        SR_KEMIP_INT, ki_geoip2_match,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },
    { str_init("geoip2"), str_init("distance"),
        SR_KEMIP_INT, ki_geoip2_distance,
        { SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
            SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
    },

    { {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_geoip2_exports);
	return 0;
}
