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

#ifndef __DIAMETER_IMS_CODE_CMD_H
#define __DIAMETER_IMS_CODE_CMD_H


/*	Command Codes alocated for IMS	*/
/*		The Gq Interface 			*/
#define IMS_AAR		265		/**< Bearer-Authorization		Request	*/
#define IMS_AAA		265		/**< Bearer-Authorization		Answer	*/
#define IMS_RAR		258		/**< Re-Auth					Request */
#define IMS_RAA		258		/**< Re-Auth					Answer	*/
#define IMS_STR		275		/**< Session Termination 		Request */
#define IMS_STA		275		/**< Session Termination 		Answer	*/
#define IMS_ASR		274		/**< Abort-Session-Request		Request */
#define IMS_ASA		274		/**< Abort-Session-Request		Answer	*/
/* The Gx Interface */
#define IMS_CCR		272
#define IMS_CCA		272
/*		The Cx/Dx Interface 			*/
#define IMS_UAR		300		/**< User-Authorization			Request	*/
#define IMS_UAA		300		/**< User-Authorization			Answer	*/
#define IMS_SAR		301		/**< Server-Assignment			Request */
#define IMS_SAA		301		/**< Server-Assignment			Answer	*/
#define IMS_LIR		302		/**< Location-Info				Request */
#define IMS_LIA		302		/**< Location-Info				Answer	*/
#define IMS_MAR		303		/**< Multimedia-Auth			Request */
#define IMS_MAA		303		/**< Multimedia-Auth			Answer	*/
#define IMS_RTR		304		/**< Registration-Termination	Request */
#define IMS_RTA		304		/**< Registration-Termination	Answer	*/
#define IMS_PPR		305		/**< Push-Profile				Request */
#define IMS_PPA		305		/**< Push-Profile				Answer	*/
/**		The Sh/Ph Interface 			*/
#define IMS_UDR		306		/**< User-Data					Request */
#define IMS_UDA		306		/**< User-Data					Answer	*/
#define IMS_PUR		307		/**< Profile-Update				Request */
#define IMS_PUA		307		/**< Profile-Update				Answer	*/
#define IMS_SNR		308		/**< Subscriber-Notifications	Request */
#define IMS_SNA		308		/**< Subscriber-Notifications	Answer	*/
#define IMS_PNR		309		/**< Push-Notification			Request */
#define IMS_PNA		309		/**< Push-Notification			Answer	*/
/**	Allocated Command Codes, not used yet	*/
#define IMS_10R		310
#define IMS_10A		310
#define IMS_11R		311
#define IMS_11A		311
#define IMS_12R		312
#define IMS_12A		312
#define IMS_13R		313
#define IMS_13A		313


#endif /* __DIAMETER_IMS_CODE_CMD_H */
