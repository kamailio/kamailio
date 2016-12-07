/*
 * $Id$
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Focus. Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include <time.h>

#include "../../sr_module.h"
MODULE_VERSION

#include "mod_export.h"

#include "../cdp/cdp_load.h"

struct cdp_binds *cdp;
	
int 	cdp_avp_init();
int 	cdp_avp_child_init(int rank);
void 	cdp_avp_destroy();
cdp_avp_bind_t*	cdp_avp_get_bind();
	
static cmd_export_t cdp_avp_cmds[] = {
	{"cdp_avp_get_bind",			(cmd_function)cdp_avp_get_bind, 	NO_SCRIPT, 0, 0},
	
	{ 0, 0, 0, 0, 0 }
};
	
/**
 * Exported SER module interface
 */
struct module_exports exports = {
	"cdp_avp",
	cdp_avp_cmds,                   /**< Exported functions */
	0,
	0,                     			/**< Exported parameters */
	cdp_avp_init,                   /**< Module initialization function */
	(response_function) 0,
	(destroy_function) cdp_avp_destroy,
	0,
	(child_init_function) cdp_avp_child_init /**< per-child init function */
};


/** Sample binding */
cdp_avp_bind_t cdp_avp_bind={
		0,	/* cdp 		*/
		
		{	/* basic 	*/
				cdp_avp_new,
				
				cdp_avp_add_new_to_list,
				cdp_avp_add_new_to_msg,
				cdp_avp_add_to_list,
				cdp_avp_add_to_msg,
				
				cdp_avp_get_next_from_list,
				cdp_avp_get_next_from_msg,
				cdp_avp_get_from_list,
				cdp_avp_get_from_msg,
		},
		
		{	/* base_data	*/
				cdp_avp_new_OctetString,
				cdp_avp_new_Integer32,
				cdp_avp_new_Integer64,
				cdp_avp_new_Unsigned32,
				cdp_avp_new_Unsigned64,
				cdp_avp_new_Float32,
				cdp_avp_new_Float64,
				cdp_avp_new_Grouped,
				
				cdp_avp_new_Address,
				cdp_avp_new_Time,
				cdp_avp_new_UTF8String,
				cdp_avp_new_DiameterIdentity,
				cdp_avp_new_DiameterURI,
				cdp_avp_new_Enumerated,
				cdp_avp_new_IPFilterRule,
				cdp_avp_new_QoSFilterRule,
				
				
				cdp_avp_get_OctetString,
				cdp_avp_get_Integer32,
				cdp_avp_get_Integer64,
				cdp_avp_get_Unsigned32,
				cdp_avp_get_Unsigned64,
				cdp_avp_get_Float32,
				cdp_avp_get_Float64,
				cdp_avp_get_Grouped,
				cdp_avp_free_Grouped,

				cdp_avp_get_Address,
				cdp_avp_get_Time,
				cdp_avp_get_UTF8String,
				cdp_avp_get_DiameterIdentity,
				cdp_avp_get_DiameterURI,
				cdp_avp_get_Enumerated,
				cdp_avp_get_IPFilterRule,
				cdp_avp_get_QoSFilterRule,
		},
		
		{	/*	base 	*/
				
				#define CDP_AVP_INIT					
					#include "base.h"				
				#undef	CDP_AVP_INIT												
		},
		
		{	/*	ccapp 	*/
				#define CDP_AVP_INIT			
					#include "ccapp.h"		
				#undef	CDP_AVP_INIT
		},

		{	/*  nasapp  */
				#define CDP_AVP_INIT					
					#include "nasapp.h"				
				#undef	CDP_AVP_INIT
		},
		
		{	/*  imsapp  */
				#define CDP_AVP_INIT					
					#include "imsapp.h"				
				#undef	CDP_AVP_INIT
		},	
		
		{	/*  epcapp  */				
				#define CDP_AVP_INIT					
					#include "epcapp.h"				
				#undef	CDP_AVP_INIT				
		}
};


/**
 * Module initialization function - called once at startup.
 * \note Other modules might not be loaded at this moment.
 * If this returns failure, wharf will exit
 * 
 * @param config - abstract configuration string
 * @return 1 on success or 0 on failure
 */
int cdp_avp_init()
{
	LOG(L_DBG," Initializing module cdp_avp\n");
	load_cdp_f load_cdp;
	/* bind to the cdp module */
	if (!(load_cdp = (load_cdp_f)find_export("load_cdp",NO_SCRIPT,0))) {
		LOG(L_ERR, "ERR"M_NAME":mod_init: Can not import load_cdp. This module requires cdp module\n");
		goto error;
	}
	cdp = pkg_malloc(sizeof(struct cdp_binds));
	if (!cdp) return 0;
	/* Load CDP module bindings*/
	if (load_cdp(cdp) == -1)
		goto error;
	
	cdp_avp_bind.cdp = cdp;
	
	return 0;
error:
	return -1;
}

/**
 * Module initialization function - called once for every process.
 * \note All modules have by now executed the mod_init.
 * If this returns failure, wharf will exit
 * 
 * @param rank - rank of the process calling this
 * @return 1 on success or 0 on failure
 */
int cdp_avp_child_init(int rank)
{
	LOG(L_DBG,"Initializing child in module cdp_avp for rank [%d]\n",
			rank);
	return 1;
}



/**
 * Module destroy function. 
 * Spould clean-up and do nice shut-down.
 * \note Will be called multiple times, once from each process, although crashed processes might not.
 */
void cdp_avp_destroy(void)
{
	LOG(L_DBG,"Destroying module cdp_avp\n");
	pkg_free(cdp);
}


/**
 * Returns the module's binding. This will give the structure containing the 
 * functions and data to be used from other processes.
 * @return the pointer to the binding.
 */
cdp_avp_bind_t* cdp_avp_get_bind()
{
	return &cdp_avp_bind;
}



