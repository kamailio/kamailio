#include "sca_common.h"

#include <sys/types.h>
#include <stdlib.h>

#include "../usrloc/usrloc.h"
#include "../../modules/tm/tm_load.h"
#include "../../timer.h"

#include "sca.h"
#include "sca_call_info.h"
#include "sca_rpc.h"
#include "sca_subscribe.h"
#include "sca_usrloc_cb.h"


MODULE_VERSION

/* MODULE OBJECT */
sca_mod			*sca;


/* EXTERNAL API */
usrloc_api_t		ul;	/* usrloc callbacks */
struct tm_binds		tmb;	/* tm functions for sending messages */
sl_api_t		slb;

/* PROTOTYPES */
static int		sca_mod_init( void );
static int		sca_bind_usrloc( usrloc_api_t *, sca_mod ** );
static int		sca_set_config( sca_mod * );

/* EXPORTED COMMANDS */
static cmd_export_t	cmds[] = {
    { "sca_handle_subscribe", sca_handle_subscribe, 0, NULL, REQUEST_ROUTE },
    { "sca_call_info_update", sca_call_info_update, 0, NULL,
	REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
    { NULL, NULL, -1, 0, 0 },
};

/* EXPORTED RPC INTERFACE */
static rpc_export_t	sca_rpc[] = {
    { "sca.all_subscriptions", sca_rpc_show_all_subscriptions,
			sca_rpc_show_all_subscriptions_doc, 0 },
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
str			domain = STR_NULL;
str			outbound_proxy = STR_NULL;
int			hash_table_size = -1;
int			call_info_max_expires = 3600;
int			line_seize_max_expires = 15;
int			purge_expired_interval = 120;

static param_export_t	params[] = {
    { "domain",			STR_PARAM,	&domain.s },
    { "outbound_proxy",		STR_PARAM,	&outbound_proxy.s },
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
    NULL,		/* destructor function */
    NULL,		/* oncancel function */
    NULL,		/* per-child initialization function */
};

/*
 * bind usrloc API and register callbacks for events we care about.
 */
    static int
sca_bind_usrloc( usrloc_api_t *ul_api, sca_mod **scam )
{
    bind_usrloc_t	bind_usrloc;

    assert( scam != NULL );

    bind_usrloc = (bind_usrloc_t)find_export( "ul_bind_usrloc", 1, 0 );
    if ( !bind_usrloc ) {
	LM_ERR( "Can't find exported usrloc functions" );
	return( -1 );
    }

    if ( bind_usrloc( ul_api ) < 0 ) {
	LM_ERR( "Can't bind exported usrloc functions" );
	return( -1 );
    }
    if ( ul_api->register_ulcb == NULL ) {
	LM_ERR( "Failed to import usrloc register callback function" );
	return( -1 );
    }

    /*
     * register callback functions for:
     *		1. contact insertion (new REGISTER for an AoR);
     *		2. contact expiration (registration expired);
     *		3. contact re-registration;
     *		4. contact deletion (REGISTER with Expires: 0)
     */
    if ( ul_api->register_ulcb( UL_CONTACT_INSERT,
			sca_contact_change_cb, *scam ) < 0 ) {
	LM_ERR( "Failed to register for usrloc contact insert callback" );
	return( -1 );
    }
    if ( ul_api->register_ulcb( UL_CONTACT_EXPIRE,
			sca_contact_change_cb, *scam ) < 0 ) {
	LM_ERR( "Failed to register for usrloc contact expire callback" );
	return( -1 );
    }
    if ( ul_api->register_ulcb( UL_CONTACT_UPDATE,
			sca_contact_change_cb, *scam ) < 0 ) {
	LM_ERR( "Failed to register for usrloc contact update callback" );
	return( -1 );
    }
    if ( ul_api->register_ulcb( UL_CONTACT_DELETE,
			sca_contact_change_cb, *scam ) < 0 ) {
	LM_ERR( "Failed to register for usrloc contact update callback" );
	return( -1 );
    }

    return( 0 );
} 

    static int
sca_set_config( sca_mod *scam )
{
    scam->cfg = (sca_config *)shm_malloc( sizeof( sca_config ));
    if ( scam->cfg == NULL ) {
	LM_ERR( "Failed to shm_malloc module configuration" );
	return( -1 );
    }

    if ( domain.s == NULL ) {
	LM_ERR( "SCA domain modparam is required in configuration script" );
	return( -1 );
    }
    domain.len = strlen( domain.s );
    scam->cfg->domain = &domain;

    if ( outbound_proxy.s ) {
	outbound_proxy.len = strlen( outbound_proxy.s );
	scam->cfg->outbound_proxy = &outbound_proxy;
    }

    if ( hash_table_size > 0 ) {
	scam->cfg->hash_table_size = 1 << hash_table_size;
    } else {
	scam->cfg->hash_table_size = 512;
    }

    scam->cfg->call_info_max_expires = call_info_max_expires;
    scam->cfg->line_seize_max_expires = line_seize_max_expires;
    scam->cfg->purge_expired_interval = purge_expired_interval;

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

    if ( rpc_register_array( sca_rpc ) != 0 ) {
	LM_ERR( "Failed to register RPC commands" );
	return( -1 );
    }

#ifdef notdef
    if ( sca_bind_usrloc( &ul, &sca ) != 0 ) {
	LM_ERR( "Failed to initialize required usrloc API" );
	return( -1 );
    }
#endif /* notdef */

    if ( load_tm_api( &tmb ) != 0 ) {
	LM_ERR( "Failed to initialize required tm API" );
	return( -1 );
    }
    sca->tm_api = &tmb;

    if ( sl_load_api( &slb ) != 0 ) {
	LM_ERR( "Failed to initialize required sl API" );
	return( -1 );
    }
    sca->sl_api = &slb;
    
    if ( sca_set_config( sca ) != 0 ) {
	LM_ERR( "Failed to set configuration" );
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

    /* start timer to clear expired subscriptions */
    register_timer( sca_subscription_purge_expired, sca,
		    sca->cfg->purge_expired_interval );

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
