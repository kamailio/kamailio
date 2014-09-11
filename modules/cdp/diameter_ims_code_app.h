/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
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
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
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

#ifndef __DIAMETER_IMS_CODE_APP_H
#define __DIAMETER_IMS_CODE_APP_H

/* Application Identifiers	*/
#define IMS_Gq	16777222	/**< Gq interface between P-CSCF and PDF 		*/
#define IMS_Cx	16777216	/**< Cx interface between I/S-CSCF and HSS 		*/
#define IMS_Dx	16777216	/**< Cx interface between I/S-CSCF and SLF 		*/
#define IMS_Sh	16777217	/**< Sh interface between AS and HSS	 		*/
#define IMS_e2	16777231	/**< e2 interface between CLF and AF	 		*/
#define IMS_Ph	16777217	/**< Sh interface between PresenceServer and HSS*/
#define IMS_Rx  16777236	/**< Rx interface between P-CSCF and PCRF 		*/
#define IMS_Gx	16777224	/**< Gx interface between PCRF and PCEF 		*/
#define IMS_Rf  16777223    /**< Rf interface between P/I/S-CSCF and CDF, 
								according to TS32.299 R7    */ 

#define IMS_Ro  4               /**< Ro interface between P/I/S-CSCF and OCS, according to TS32.299 R7    */

#endif /* __DIAMETER_IMS_CODE_APP_H */
