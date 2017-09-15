/**
 * dispatcher module
 *
 * Copyright (C) 2004-2006 FhG Fokus
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

/*! \file
 * \ingroup dispatcher
 * \brief Dispatcher :: Dispatch
 */

#ifndef _DISPATCH_H_
#define _DISPATCH_H_

#include <stdio.h>
#include "../../core/pvar.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/rand/kam_rand.h"
#include "../../modules/tm/tm_load.h"


/* clang-format off */
#define DS_HASH_USER_ONLY	1  /*!< use only the uri user part for hashing */
#define DS_FAILOVER_ON		2  /*!< store the other dest in avps */

#define DS_INACTIVE_DST		1  /*!< inactive destination */
#define DS_TRYING_DST		2  /*!< temporary trying destination */
#define DS_DISABLED_DST		4  /*!< admin disabled destination */
#define DS_PROBING_DST		8  /*!< checking destination */
#define DS_STATES_ALL		15  /*!< all bits for the states of destination */

#define ds_skip_dst(flags)	((flags) & (DS_INACTIVE_DST|DS_DISABLED_DST))

#define DS_PROBE_NONE		0
#define DS_PROBE_ALL		1
#define DS_PROBE_INACTIVE	2
#define DS_PROBE_ONLYFLAGGED	3

#define DS_MATCH_ALL		0
#define DS_MATCH_NOPORT		1
#define DS_MATCH_NOPROTO	2
/* clang-format on */

extern str ds_db_url;
extern str ds_table_name;
extern str ds_set_id_col;
extern str ds_dest_uri_col;
extern str ds_dest_flags_col;
extern str ds_dest_priority_col;
extern str ds_dest_attrs_col;

extern int ds_flags;
extern int ds_use_default;

extern int_str dst_avp_name;
extern unsigned short dst_avp_type;
extern int_str grp_avp_name;
extern unsigned short grp_avp_type;
extern int_str cnt_avp_name;
extern unsigned short cnt_avp_type;
extern int_str dstid_avp_name;
extern unsigned short dstid_avp_type;
extern int_str attrs_avp_name;
extern unsigned short attrs_avp_type;
extern int_str sock_avp_name;
extern unsigned short sock_avp_type;

extern pv_elem_t *hash_param_model;

extern str ds_setid_pvname;
extern pv_spec_t ds_setid_pv;
extern str ds_attrs_pvname;
extern pv_spec_t ds_attrs_pv;

/* Structure containing pointers to TM-functions */
extern struct tm_binds tmb;
extern str ds_ping_method;
extern str ds_ping_from;
extern int probing_threshold;  /*!< number of failed requests,
								before a destination is taken into probing */
extern int inactive_threshold; /*!< number of successful requests,
								before a destination is taken into active */
extern int ds_probing_mode;
extern str ds_outbound_proxy;
extern str ds_default_socket;
extern struct socket_info *ds_default_sockinfo;

int init_data(void);
int init_ds_db(void);
int ds_load_list(char *lfile);
int ds_connect_db(void);
void ds_disconnect_db(void);
int ds_load_db(void);
int ds_reload_db(void);
int ds_destroy_list(void);
int ds_select_dst_limit(
		struct sip_msg *msg, int set, int alg, unsigned int limit, int mode);
int ds_select_dst(struct sip_msg *msg, int set, int alg, int mode);
int ds_next_dst(struct sip_msg *msg, int mode);
int ds_update_state(sip_msg_t *msg, int group, str *address, int state);
int ds_reinit_state(int group, str *address, int state);
int ds_mark_dst(struct sip_msg *msg, int mode);
int ds_print_list(FILE *fout);
int ds_log_sets(void);
int ds_list_exist(int set);


int ds_load_unset(struct sip_msg *msg);
int ds_load_update(struct sip_msg *msg);

int ds_hash_load_init(unsigned int htsize, int expire, int initexpire);
int ds_hash_load_destroy(void);

int ds_is_from_list(struct sip_msg *_m, int group);
int ds_is_addr_from_list(sip_msg_t *_m, int group, str *uri, int mode);

/*! \brief
 * Timer for checking inactive destinations
 */
void ds_check_timer(unsigned int ticks, void *param);


/*! \brief
 * Timer for checking active calls load
 */
void ds_ht_timer(unsigned int ticks, void *param);

/*! \brief
 * Check if the reply-code is valid:
 */
int ds_ping_check_rplcode(int);

/* clang-format off */
typedef struct _ds_attrs {
	str body;
	str duid;
	str socket;
	int maxload;
	int weight;
	int rweight;
} ds_attrs_t;

typedef struct _ds_latency_stats {
	struct timeval start;
	int min;
	int max;
	float average;  // weigthed average, estimate of the last few weeks
	float stdev;    // last standard deviation
	float estimate; // short term estimate, EWMA exponential weighted moving average
	double m2;      // sum of squares, used for recursive variance calculation
	int32_t count;
	uint32_t timeout;
} ds_latency_stats_t;

typedef struct _ds_dest {
	str uri;
	int flags;
	int priority;
	int dload;
	ds_attrs_t attrs;
	ds_latency_stats_t latency_stats;
	struct socket_info * sock;
	struct ip_addr ip_address; 	/*!< IP-Address of the entry */
	unsigned short int port; 	/*!< Port of the URI */
	unsigned short int proto; 	/*!< Protocol of the URI */
	int message_count;
	struct _ds_dest *next;
} ds_dest_t;

typedef struct _ds_set {
	int id;				/*!< id of dst set */
	int nr;				/*!< number of items in dst set */
	int last;			/*!< last used item in dst set (round robin) */
	int wlast;			/*!< last used item in dst set (by weight) */
	int rwlast;			/*!< last used item in dst set (by relative weight) */
	ds_dest_t *dlist;
	unsigned int wlist[100];
	unsigned int rwlist[100];
	struct _ds_set *next[2];
	int longer;
} ds_set_t;
/* clang-format on */

#define AVL_LEFT 0
#define AVL_RIGHT 1
#define AVL_NEITHER -1
#define AVL_BALANCED(n) (n->longer < 0)

ds_set_t *ds_get_list(void);
int ds_get_list_nr(void);

int ds_ping_active_init(void);
int ds_ping_active_get(void);
int ds_ping_active_set(int v);

/* Create if not exist and return ds_set_t by id */
ds_set_t *ds_avl_insert(ds_set_t **root, int id, int *setn);
ds_set_t *ds_avl_find(ds_set_t *node, int id);
void ds_avl_destroy(ds_set_t **node);

#endif
