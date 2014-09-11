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
#ifndef DIAMETER_EPC_CODE_CMD_H_
#define DIAMETER_EPC_CODE_CMD_H_
/*	Command Codes used in the EPC 	*/

/*		The Rx Interface 			*/
#define Diameter_AAR		265		/**< Bearer-Authorization		Request	*/
#define Diameter_AAA		265		/**< Bearer-Authorization		Answer	*/
#define Diameter_RAR		258		/**< Re-Auth					Request */
#define Diameter_RAA		258		/**< Re-Auth					Answer	*/
#define Diameter_STR		275		/**< Session Termination 		Request */
#define Diameter_STA		275		/**< Session Termination 		Answer	*/
#define Diameter_ASR		274		/**< Abort-Session-Request		Request */
#define Diameter_ASA		274		/**< Abort-Session-Request		Answer	*/
/*		The Rf Interface			*/
#define Diameter_ACR		271		/**< Accounting Request */
#define Diameter_ACA		271		/**< Accounting Answer  */
/* The Gx and Gxx Interface */
#define Diameter_CCR		272
#define Diameter_CCA		272

/* The Sh/Sp interface */
#define Diameter_UDR 		306
#define Diameter_UDA 		306
#define Diameter_PUR		307
#define Diameter_PUA		307
#define Diameter_SNR		308
#define Diameter_SNA		308
#define Diameter_PNR		309
#define Diameter_PNA		309


/* The S6a/S6d Interfaces */
#define Diameter_ULR		316
#define Diameter_ULA		316
#define Diameter_CLR		317
#define Diameter_CLA		317
#define Diameter_AIR		318
#define Diameter_AIA		318
#define Diameter_IDR		319
#define Diameter_IDA		319
#define Diameter_DSR		320
#define Diameter_DSA		320
#define Diameter_PurgeUER	321
#define Diameter_PurgeUEA	321
#define Diameter_RSR		322
#define Diameter_RSA		322
#define Diameter_NOR		323
#define Diameter_NOA		323

/* The 3GPP EPS AAA Interfaces */
/* SWa - non-3GPP untrusted AN <-> AAA Server/Proxy */
/* SWm - non-3GPP untrusted ePDG <-> AAA Server/Proxy */
/* STa - non-3GPP trusted AN <-> AAA Server/Proxy */
/* S6b/H2 - PGW/HA <-> AAA Server/Proxy */
/* SWd - AAA Server <-> AAA Proxy */
#define Diameter_DER		268
#define Diameter_DEA		268
/* SWx - HSS <-> AAA Server/Proxy */
#define Diameter_SAR		301
#define Diameter_SAA		301
#define Diameter_MAR		303
#define Diameter_MAA		303
#define Diameter_RTR		304
#define Diameter_RTA		304
#define Diameter_PPR		305
#define Diameter_PPA		305


/* The S13 Interface */
#define Diameter_ECR		324
#define Diameter_ECA		324

/*not standard interfaces*/
#define Diameter_MC_AF_ROUTE_UP		400
#define Diameter_MC_AF_ROUTE_DEL	401

#endif /*DIAMETER_EPC_CODE_CMD_H_*/
