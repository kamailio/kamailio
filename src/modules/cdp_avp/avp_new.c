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

#include "avp_new.h"

extern struct cdp_binds *cdp;

/**
 * Just create an AVP and return it
 * @param d - buffer with the encoded data
 * @param len - length of the data
 * @param avp_code - code of the avp
 * @param avp_flags - flags for the avp
 * @param avp_vendorid - vendorid for the avp
 * @param data_do - what to do with the data next 
 * @return ptr to the new avp or null on error
 */
inline AAA_AVP *cdp_avp_new(int avp_code,int avp_flags,int avp_vendorid,str data,AVPDataStatus data_do)
{
	if (avp_vendorid!=0) avp_flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
	return cdp->AAACreateAVP(avp_code,avp_flags,avp_vendorid,data.s,data.len,data_do);	
}

