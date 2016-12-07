/*
 * Copyright (C) 2009 1&1 Internet AG
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
 */

/*!
 * \file
 * \brief Kamailio utils :: 
 * \ingroup utils
 * Module: \ref utils
 */

#include "conf.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../proxy.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFSIZE 1000

/*! \brief special filter indices */
enum {
	sfidx_request = 0,
	sfidx_reply,
	sfilter_cnt
};

/*! special filter masks */
static int sfilter_mask[sfilter_cnt] = { 1, 2 };

/*! special filter names */
static char *sfilter_str[sfilter_cnt] = {
	"REQUEST",
	"REPLY"
};


struct fwd_setting {
	int active;
	int sfilter;
	char *filter_methods;
	struct proxy_l* proxy;
};


static struct fwd_setting *fwd_settings = NULL;
static int fwd_max_id = 0;


/*!
 * \brief Removes white spaces and new lines from s
 * \todo check if we can use the functions from ut.h
 * Removes white spaces and new lines from s, the contents of s are modified.
 * \param s the string.
 */
static void remove_spaces(char *s)
{
	char *p, *dst;
	for (p = s, dst = s; *p != '\0'; ++p) {
		if (!isspace(*p)) *dst++ = *p;
	}
	*dst = '\0';

}


/*!
 * \brief Converts a string to integer.
 * \todo check if we can use the functions from ut.h
 *  params:
 *    s: The string to be converted to int.
 *  returns:
 *    >=0 on success
 *     -1 otherwise
 */
static int conf_str2int(char *s)
{
	if (s == NULL) return -1;

	errno = 0;
	char *end = NULL;
	long int i = strtol(s, &end, 10);
	if ((errno != 0) || (i == LONG_MIN) || (i == LONG_MAX) || (end == s)) {
		LM_ERR("invalid string '%s'.\n", s);
		return -1;
	}

	return i;
}


/*!
 * \brief Converts string to integer and checks for validity.
 * \todo check if we can use the functions from ut.h
 *  params:
 *    id_str: ID as string to be converted to int.
 *  returns:
 *    >=0 on success
 *     -1 otherwise
 */
int conf_str2id(char *id_str)
{
	int id = conf_str2int(id_str);

	if ((id<0) || (id > fwd_max_id)) {
	LM_ERR("id %d is out of range.\n", id);
	return -1;
	}

	return id;
}


/*!
 * \brief Updates switch configuration
 * \param id Update the configuration with this ID.
 * \param param_str can be either "off" or "on".
 * \return 0 on success, -1 otherwise
 */
static int update_switch(int id, char* param_str)
{
	if (param_str == NULL) {
		LM_ERR("param_str is NULL.\n");
		return -1;
	}

	if (strcmp(param_str, "on") == 0) {
		fwd_settings[id].active = 1;
		return 0;
	} else if (strcmp(param_str, "off") == 0) {
		fwd_settings[id].active = 0;
		return 0;
	}

	LM_ERR("invalid switch '%s'.\n", param_str);
	return -1;
}


/*!
 * \brief Updates filter configuration.
 * Updates filter configuration.
 * If filter_methods is not NULL, memory is freed.
 * If filter methods are found, memory for the string is allocated,
 * otherwise filter_methods is set to NULL.
 * \param id update the configuration with this ID.
 * \param flist a list of filter names.
 * \return 0 on success, -1 otherwise
 */
static int update_filter(int id, char *flist)
{
	if (flist == NULL) {
		LM_ERR("flist is NULL.\n");
		return -1;
	}

	/* reset special filter mask and filter methods*/
	fwd_settings[id].sfilter = 0;
	if (fwd_settings[id].filter_methods != NULL) {
		shm_free(fwd_settings[id].filter_methods);
		fwd_settings[id].filter_methods = NULL;
	}

	int i;
	for (i=0; i<sfilter_cnt; i++) {
		if (strstr(flist, sfilter_str[i]) != NULL) {
			/* special filter name is found in flist -> add to special filter mask */
			fwd_settings[id].sfilter |= sfilter_mask[i];
		}
	}

	char buf[BUFSIZE+1], tmp[BUFSIZE+1];
	buf[0] = '\0';
	char *set_p = flist;
	char *token = NULL;
	while ((token = strsep(&set_p, ":"))) {  /* iterate through list of filters */
		int found  = 0;
		/* is it a special filter? */
		for (i=0; i<sfilter_cnt; i++) {
			if (strcmp(token, sfilter_str[i]) == 0) {
				found = 1;
				break;
			}
		}

		if (found == 0) {
		/* no special filter! */
			if (buf[0]) {
				strcpy(tmp, buf);
				snprintf(buf, BUFSIZE, "%s:%s", tmp, token);
				buf[BUFSIZE]='\0';
			} else {
				snprintf(buf, BUFSIZE, "%s", token);
				buf[BUFSIZE]='\0';
			}
		}
	}

	int len = strlen(buf);
	if (len > 0) {
		char *flc = shm_malloc(len+1);
		if (flc == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		memcpy(flc, buf, len+1);
		fwd_settings[id].filter_methods = flc;
	}
	return 0;
}


/*!
 * Updates proxy configuration
 * \param id update the configuration with this ID.
 * \param host_str the destination host.
 * \param port_str the port number as string.
 * \return 0 on success, -1 otherwise
 */
static int update_proxy(int id, char *host_str, char *port_str)
{
	if (host_str == NULL) {
		LM_ERR("host_str is NULL.\n");
		return -1;
	}
	if (port_str == NULL) {
		LM_ERR("port_str is NULL.\n");
		return -1;
	}

	int port = conf_str2int(port_str);
	if (port < 0) {
		LM_ERR("invalid port '%s'.\n", port_str);
		return -1;
	}

	/* make copy of host string since mk_proxy does not */
	str host;
	host.len = strlen(host_str);
	host.s = shm_malloc(host.len+1);
	if (host.s == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	strcpy(host.s, host_str);

	/* make proxy in shared memory */
	struct proxy_l* proxy;
	proxy = mk_shm_proxy(&host, port, PROTO_UDP);
	if (proxy == NULL) {
		LM_ERR("cannot make proxy (host='%s', port=%d).\n", host_str, port);
		shm_free(host.s);
		return -1;
	}

	if (fwd_settings[id].proxy) {
		/* cleaning up old proxy */
		if (fwd_settings[id].proxy->name.s) {
			shm_free(fwd_settings[id].proxy->name.s);
		}
		free_shm_proxy(fwd_settings[id].proxy);
		shm_free(fwd_settings[id].proxy);
	}
	fwd_settings[id].proxy = proxy;  /* new proxy is now acitvated */

	return 0;
}


/*!
 * \brief Parses configuration string for the switch
 * Parses a configuration string for switch settings and updates
 * the configuration structure.
 * \param settings the configuration string in the following form:
\verbatim
 *              <id>=<switch>[,<id>=<switch>]...
\endverbatim
 * \return 1 on success, -1 otherwise
 */
int conf_parse_switch(char *settings)
{
	/* make a copy since we are modifying it */
	int len = strlen(settings);
	if (len==0) return 1;
	char *strc = (char *)pkg_malloc(len+1);
	if (strc == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(strc, settings, len+1);
	remove_spaces(strc);

	char *set_p = strc;
	char *token = NULL;
	while ((token = strsep(&set_p, ","))) {  /* iterate through list of settings */
		char *id_str = strsep(&token, "=");
		int id = conf_str2id(id_str);
		if (id < 0) {
			LM_ERR("cannot parse id '%s'.\n", id_str);
			pkg_free(strc);
			return -1;
		}

		/* got all data for one setting -> update configuration now */
		if (update_switch(id, token) < 0) {
			LM_ERR("cannot update switch.\n");
			pkg_free(strc);
			return -1;
		}
	}

	pkg_free(strc);
	return 1;
}


/*!
 * \brief Output configuration in FIFO format
 * \param rpl_tree FIFO root
 * \return 0 on success, -1 on failure
 */
int conf_show(struct mi_root* rpl_tree)
{
	int id, sfilter;
	struct mi_node * node = NULL;

	node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "id switch %30s proxy\n", "filter");
	if(node == NULL)
		goto error;

	for (id=0; id<=fwd_max_id; id++) {
		char buf[BUFSIZE+1];
		char tmp[BUFSIZE+1];
		buf[0]='\0';
		for (sfilter=0; sfilter<sfilter_cnt; sfilter++) {
			if (fwd_settings[id].sfilter&sfilter_mask[sfilter]) {
				if (buf[0]) {
					strcpy(tmp, buf);
					snprintf(buf, BUFSIZE, "%s:%s", tmp, sfilter_str[sfilter]);
					buf[BUFSIZE]='\0';
				} else {
					snprintf(buf, BUFSIZE, "%s", sfilter_str[sfilter]);
					buf[BUFSIZE]='\0';
				}
			}
		}
		if (fwd_settings[id].filter_methods) {
			if (buf[0]) {
				strcpy(tmp, buf);
				snprintf(buf, BUFSIZE, "%s:%s", tmp, fwd_settings[id].filter_methods);
				buf[BUFSIZE]='\0';
			} else {
				snprintf(buf, BUFSIZE, "%s", fwd_settings[id].filter_methods);
				buf[BUFSIZE]='\0';
			}
		}
		node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "%2d %s %33s %s:%d\n", id,
			fwd_settings[id].active ? "on " : "off", buf,
			fwd_settings[id].proxy ? fwd_settings[id].proxy->name.s : "",
			fwd_settings[id].proxy ? fwd_settings[id].proxy->port : 0);
		if(node == NULL)
			goto error;

	}
	return 0;

error:
	return -1;
}


/*!
 * \brief Parses a configuration string for the filter
 * Parses a configuration string for switch settings and
 * updates the configuration structure.
 * \param settings The configuration string in the following form:
\verbatim
 *              <id>=<filter>[:<filter>]...[,<id>=<filter>[:<filter>]...]...
\endverbatim
 * \return 1 on success, -1 otherwise
 */
int conf_parse_filter(char *settings)
{
	/* make a copy since we are modifying it */
	int len = strlen(settings);
	if (len==0) return 1;
	char *strc = (char *)pkg_malloc(len+1);
	if (strc == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(strc, settings, len+1);
	remove_spaces(strc);

	char *set_p = strc;
	char *token = NULL;
	while ((token = strsep(&set_p, ","))) {  /* iterate through list of settings */
		char *id_str = strsep(&token, "=");
		int id = conf_str2id(id_str);
		if (id<0) {
			LM_ERR("cannot parse id '%s'.\n", id_str);
			pkg_free(strc);
			return -1;
		}
		if (update_filter(id, token) < 0) {
			LM_ERR("cannot extract filters.\n");
			pkg_free(strc);
			return -1;
		}
	}

	pkg_free(strc);
	return 1;
}


/*!
 * \brief Parses a configuration string for proxy settings
 * Parses a configuration string for proxy settings and
 * updates the configuration structure.
 * \param settings: The configuration string in the following form:
\verbatim
 *              <id>=<host>:<port>[,<id>=<host>:<port>]...
\endverbatim
 * \return: 1 on success, -1 otherwise
 */
int conf_parse_proxy(char *settings)
{
	/* make a copy since we are modifying it */
	int len = strlen(settings);
	if (len==0) return 1;
	char *strc = (char *)pkg_malloc(len+1);
	if (strc == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(strc, settings, len+1);
	remove_spaces(strc);

	char *set_p = strc;
	char *token = NULL;
	while ((token = strsep(&set_p, ","))) {  /* iterate through list of settings */
		char *id_str = strsep(&token, "=");
		int id = conf_str2id(id_str);
		if (id<0) {
			LM_ERR("cannot parse id '%s'.\n", id_str);
			pkg_free(strc);
			return -1;
		}
		char *host = strsep(&token, ":");

		/* got all data for one setting -> update configuration now */
		if (update_proxy(id, host, token) < 0) {
			LM_ERR("cannot update proxy.\n");
			pkg_free(strc);
			return -1;
		}
	}

	pkg_free(strc);
	return 1;
}


/*!
 * \brief Checks if method string is in filter_methods
 * \param id use configuration with this ID when checking
 * \param method method string to be searched for
 * \param method_len length of method string
 * \return 1 if method is found in filter_methods, 0 otherwise
 */
static int filter_methods_contains_request(int id, char *method, int method_len)
{
	char *p = fwd_settings[id].filter_methods;

	while (p != NULL) {
		if (strncmp(p, method, method_len) == 0) {
			return 1;
		}
		p = strchr(p, ':');
		if (p != NULL) p++;
	}

	return 0;
}


/*!
 * \brief Checks forwarding is needed
 * \param msg the SIP message to be forwarded
 * \param id use configuration with this ID when checking
 * \return pointer to proxy structure of destination if forwarding is needed, NULL otherwise
 */
struct proxy_l *conf_needs_forward(struct sip_msg *msg, int id)
{
	if ((msg == NULL) || (fwd_settings[id].active == 0)) {
		return NULL;
	}

	if (msg->first_line.type == SIP_REPLY) {
		if (fwd_settings[id].sfilter&sfilter_mask[sfidx_reply]) {
			return fwd_settings[id].proxy;
		}
	}

	if (msg->first_line.type == SIP_REQUEST) {
		if (fwd_settings[id].sfilter&sfilter_mask[sfidx_request]) {
			return fwd_settings[id].proxy;
		}

		if (filter_methods_contains_request(id, msg->first_line.u.request.method.s, msg->first_line.u.request.method.len) > 0) {
			return fwd_settings[id].proxy;
		}
	}

	return NULL;
}


/*!
 * \brief Initialize configuration
 * \param max_id number of configuration statements
 * \return 0 on success, -1 on failure
 */
int conf_init(int max_id)
{
	/* allocate and initialize memory for configuration */
	fwd_settings = shm_malloc(sizeof(struct fwd_setting)*(max_id+1));
	if (fwd_settings == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(fwd_settings, 0, sizeof(struct fwd_setting)*(max_id+1));
	fwd_max_id = max_id;
	return 0;
}


/*!
 * \brief Destroy configuration
 */
void conf_destroy(void)
{
	int id;

	if (fwd_settings) {
		for (id=0; id<=fwd_max_id; id++) {
			fwd_settings[id].active = 0;
			if (fwd_settings[id].proxy) {
				if (fwd_settings[id].proxy->name.s) {
					shm_free(fwd_settings[id].proxy->name.s);
				}
				free_shm_proxy(fwd_settings[id].proxy);
				shm_free(fwd_settings[id].proxy);
			}
		}
		shm_free(fwd_settings);
	}
}
