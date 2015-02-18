/*
 * Copyright (C) 2012 Andrew Mortensen
 *
 * This file is part of the sca module for Kamailio, a free SIP server.
 *
 * The sca module is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * The sca module is distributed in the hope that it will be useful,
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
#include "sca_common.h"

#include <sys/types.h>
#include <stdlib.h>

#include "../../timer.h"
#include "../../timer_proc.h"

#include "sca.h"
#include "sca_appearance.h"
#include "sca_db.h"
#include "sca_call_info.h"
#include "sca_rpc.h"
#include "sca_subscribe.h"


MODULE_VERSION

/* MODULE OBJECT */
sca_mod			*sca = NULL;


/* EXTERNAL API */
db_func_t		dbf;	/* db api */
struct tm_binds		tmb;	/* tm functions for sending messages */
sl_api_t		slb;	/* sl callback, function for getting to-tag */

/* PROTOTYPES */
static int		sca_mod_init( void );
static int		sca_child_init( int );
static void		sca_mod_destroy( void );
static int		sca_set_config( sca_mod * );

/* EXPORTED COMMANDS */
static cmd_export_t	cmds[] = {
    { "sca_handle_subscribe", sca_handle_subscribe, 0, NULL, REQUEST_ROUTE },
    { "sca_call_info_update", sca_call_info_update, 0, NULL,
	REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
    { "sca_call_info_update", sca_call_info_update, 1, fixup_var_int_1,
	REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
    { NULL, NULL, -1, 0, 0 },
};

/* EXPORTED RPC INTERFACE */
static rpc_export_t	sca_rpc[] = {
    { "sca.all_subscriptions", sca_rpc_show_all_subscriptions,
			sca_rpc_show_all_subscriptions_doc, 0 },
    { "sca.subscription_count", sca_rpc_subscription_count,
			sca_rpc_subscription_count_doc, 0 },
    { "sca.show_subscription", sca_rpc_show_subscription,
			sca_rpc_show_subscription_doc, 0 },
    { "sca.subscribers", sca_rpc_show_subscribers,
			sca_rpc_show_subscribers_doc, 0 },
    { "sca.deactivate_all_subscriptions", sca_rpc_deactivate_all_subscriptions,
			sca_rpc_deactivate_all_subscriptions_doc, 0 },
    { "sca.deactivate_subscription", sca_rpc_deactivate_subscription,
			sca_rpc_deactivate_subscription_doc, 0 },
    { "sca.all_appearances", sca_rpc_show_all_appearances,
			sca_rpc_show_all_appearances_doc, 0 },
    { "sca.show_appearance", sca_rpc_show_appearance,
			sca_rpc_show_appearance_doc, 0 },
    { "sca.seize_appearance", sca_rpc_seize_appearance,
			sca_rpc_seize_appearance_doc, 0 },
    { "sca.update_appearance", sca_rpc_update_appearance,
			sca_rpc_update_appearance_doc, 0 },
    { "sca.release_appearance", sca_rpc_release_appearance,
			sca_rpc_release_appearance_doc, 0 },
    { NULL, NULL, NULL, 0 },
};

/* EXPORTED PARAMETERS */
str			outbound_proxy = STR_NULL;
str			db_url = STR_STATIC_INIT( DEFAULT_DB_URL );
str			db_subs_table = STR_STATIC_INIT( "sca_subscriptions" );
str			db_state_table = STR_STATIC_INIT( "sca_state" );
int			db_update_interval = 300;
int			hash_table_size = -1;
int			call_info_max_expires = 3600;
int			line_seize_max_expires = 15;
int			purge_expired_interval = 120;

static param_export_t	params[] = {
    { "outbound_proxy",		PARAM_STR,	&outbound_proxy },
    { "db_url",			PARAM_STR,	&db_url },
    { "subs_table",		PARAM_STR,	&db_subs_table },
    { "state_table",		PARAM_STR,	&db_state_table },
    { "db_update_interval",	INT_PARAM,	&db_update_interval },
    { "hash_table_size",	INT_PARAM,	&hash_table_size },
    { "call_info_max_expires",	INT_PARAM,	&call_info_max_expires },
    { "line_seize_max_expires", INT_PARAM,	&line_seize_max_expires },
    { "purge_expired_interval",	INT_PARAM,	&purge_expired_interval },
    { NULL,			0,		NULL },
};

/* MODULE EXPORTS */
struct module_exports	exports = {
    "sca",		/* module name */
    cmds,		/* exported functions */
    NULL,		/* RPC methods */
    params,		/* exported parameters */
    sca_mod_init,	/* module initialization function */
    NULL,		/* response handling function */
    sca_mod_destroy,	/* destructor function */
    NULL,		/* oncancel function */
    sca_child_init,	/* per-child initialization function */
};


    static int
sca_bind_sl( sca_mod *scam, sl_api_t *sl_api )
{
    sl_cbelem_t		sl_cbe;

    assert( scam != NULL );
    assert( sl_api != NULL );

    if ( sl_load_api( sl_api ) != 0 ) {
	LM_ERR( "Failed to initialize required sl API" );
	return( -1 );
    }
    scam->sl_api = sl_api;

    sl_cbe.type = SLCB_REPLY_READY;
    sl_cbe.cbf = (sl_cbf_f)sca_call_info_sl_reply_cb;

    if ( scam->sl_api->register_cb( &sl_cbe ) < 0 ) {
	LM_ERR( "Failed to register sl reply callback" );
	return( -1 );
    }

    return( 0 );
}

    static int
sca_bind_srdb1( sca_mod *scam, db_func_t *db_api )
{
    db1_con_t	*db_con = NULL;
    int		rc = -1;

    if ( db_bind_mod( scam->cfg->db_url, db_api ) != 0 ) {
	LM_ERR( "Failed to initialize required DB API" );
	goto done;
    }
    scam->db_api = db_api;

    if ( !DB_CAPABILITY( (*db_api), DB_CAP_ALL )) {
	LM_ERR( "Selected database %.*s lacks required capabilities",
		STR_FMT( scam->cfg->db_url ));
	goto done;
    }

    /* ensure database exists and table schemas are correct */
    db_con = db_api->init( scam->cfg->db_url );
    if ( db_con == NULL ) {
	LM_ERR( "sca_bind_srdb1: failed to connect to DB %.*s",
		STR_FMT( scam->cfg->db_url ));
	goto done;
    }

    if ( db_check_table_version( db_api, db_con,
	    scam->cfg->subs_table, SCA_DB_SUBSCRIPTIONS_TABLE_VERSION ) < 0 ) {
	LM_ERR( "Version check of %.*s table in DB %.*s failed",
		STR_FMT( scam->cfg->subs_table ), STR_FMT( scam->cfg->db_url ));
	LM_ERR( "%.*s table version %d required",
		STR_FMT( scam->cfg->subs_table ),
		SCA_DB_SUBSCRIPTIONS_TABLE_VERSION );
	goto done;
    }

    /* DB and tables are OK, close DB handle. reopen in each child. */
    rc = 0;

done:
    if ( db_con != NULL ) {
	db_api->close( db_con );
	db_con = NULL;
    }

    return( rc );
}

    static int
sca_set_config( sca_mod *scam )
{
    scam->cfg = (sca_config *)shm_malloc( sizeof( sca_config ));
    if ( scam->cfg == NULL ) {
	LM_ERR( "Failed to shm_malloc module configuration" );
	return( -1 );
    }
    memset(scam->cfg, 0, sizeof( sca_config ));

    if ( outbound_proxy.s ) {
	scam->cfg->outbound_proxy = &outbound_proxy;
    }

    if ( !db_url.s || db_url.len<=0 ) {
	LM_ERR( "sca_set_config: db_url must be set!" );
	return( -1 );
    }
    scam->cfg->db_url = &db_url;

    if ( !db_subs_table.s || db_subs_table.len<=0 ) {
	LM_ERR( "sca_set_config: subs_table must be set!" );
	return( -1 );
    }
    scam->cfg->subs_table = &db_subs_table;

    if ( !db_state_table.s || db_state_table.len<=0 ) {
	LM_ERR( "sca_set_config: state_table must be set!" );
	return( -1 );
    }
    scam->cfg->state_table = &db_state_table;

    if ( hash_table_size > 0 ) {
	scam->cfg->hash_table_size = 1 << hash_table_size;
    } else {
	scam->cfg->hash_table_size = 512;
    }

    scam->cfg->db_update_interval = db_update_interval;
    scam->cfg->call_info_max_expires = call_info_max_expires;
    scam->cfg->line_seize_max_expires = line_seize_max_expires;
    scam->cfg->purge_expired_interval = purge_expired_interval;

    return( 0 );
}

    static int
sca_child_init( int rank )
{
    if ( rank == PROC_INIT || rank == PROC_TCP_MAIN ) {
	return( 0 );
    }

    if ( rank == PROC_MAIN ) {
	if ( fork_dummy_timer( PROC_TIMER, "SCA DB SYNC PROCESS",
			    0, /* we don't need sockets, just writing to DB */
			    sca_subscription_db_update_timer, /* timer cb */
			    NULL, /* parameter passed to callback */
			    sca->cfg->db_update_interval ) < 0 ) {
	    LM_ERR( "sca_child_init: failed to register subscription DB "
		    "sync timer process" );
	    return( -1 );
	}

	return( 0 );
    }

    if ( sca->db_api == NULL || sca->db_api->init == NULL ) {
	LM_CRIT( "sca_child_init: DB API not loaded!" );
	return( -1 );
    }

    return( 0 );
}

    static int
sca_mod_init( void )
{
    sca = (sca_mod *)shm_malloc( sizeof( sca_mod ));
    if ( sca == NULL ) {
	LM_ERR( "Failed to shm_malloc module object" );
	return( -1 );
    }
    memset( sca, 0, sizeof( sca_mod ));

    if ( sca_set_config( sca ) != 0 ) {
	LM_ERR( "Failed to set configuration" );
	goto error;
    }

    if ( rpc_register_array( sca_rpc ) != 0 ) {
	LM_ERR( "Failed to register RPC commands" );
	goto error;
    }

    if ( sca_bind_srdb1( sca, &dbf ) != 0 ) {
	LM_ERR( "Failed to initialize required DB API" );
	goto error;
    }

    if ( load_tm_api( &tmb ) != 0 ) {
	LM_ERR( "Failed to initialize required tm API" );
	goto error;
    }
    sca->tm_api = &tmb;

    if ( sca_bind_sl( sca, &slb ) != 0 ) {
	LM_ERR( "Failed to initialize required sl API" );
	goto error;
    }
    
    if ( sca_hash_table_create( &sca->subscriptions,
			sca->cfg->hash_table_size ) != 0 ) {
	LM_ERR( "Failed to create subscriptions hash table" );
	goto error;
    }
    if ( sca_hash_table_create( &sca->appearances,
			sca->cfg->hash_table_size ) != 0 ) {
	LM_ERR( "Failed to create appearances hash table" );
	goto error;
    }

    sca_subscriptions_restore_from_db( sca );

    register_timer( sca_subscription_purge_expired, sca,
		    sca->cfg->purge_expired_interval );
    register_timer( sca_appearance_purge_stale, sca,
		    sca->cfg->purge_expired_interval );

    /*
     * register separate timer process to write subscriptions to DB.
     * move to 3.3+ timer API (register_basic_timer) at some point.
     *
     * timer process forks in sca_child_init, above.
     */
    register_dummy_timers( 1 );

    LM_INFO( "initialized" );

    return( 0 );

error:
    if ( sca != NULL ) {
	if ( sca->cfg != NULL ) {
	    shm_free( sca->cfg );
	}
	if ( sca->subscriptions != NULL ) {
	    sca_hash_table_free( sca->subscriptions );
	}
	if ( sca->appearances != NULL ) {
	    sca_hash_table_free( sca->appearances );
	}
	shm_free( sca );
	sca = NULL;
    }

    return( -1 );
}

    void
sca_mod_destroy( void )
{
	if(sca==0)
		return;

    /* write back to the DB to retain most current subscription info */
    if ( sca_subscription_db_update() != 0 ) {
		if(sca && sca->cfg && sca->cfg->db_url) {
			LM_ERR( "sca_mod_destroy: failed to save current subscriptions "
				"in DB %.*s", STR_FMT( sca->cfg->db_url ));
		}
    }

    sca_db_disconnect();
}
