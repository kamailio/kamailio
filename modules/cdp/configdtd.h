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

#define DP_CONFIG_DTD "\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<!ELEMENT DiameterPeer (Peer*, Acceptor*, Auth*, Acct*, SupportedVendor*, Realm*, DefaultRoute*)>\
<!ATTLIST DiameterPeer \
	FQDN		CDATA				#REQUIRED\
	Realm		CDATA				#REQUIRED\
	Vendor_Id	CDATA				#REQUIRED\
	Product_Name CDATA				#REQUIRED\
	AcceptUnknownPeers CDATA		#REQUIRED\
	DropUnknownOnDisconnect CDATA	#REQUIRED\
	Tc			CDATA				#REQUIRED\
	Workers		CDATA				#REQUIRED\
	QueueLength	CDATA				#REQUIRED\
	ConnectTimeout	   CDATA		#IMPLIED\
	TransactionTimeout CDATA		#IMPLIED\
	SessionsHashSize CDATA			#IMPLIED\
	DefaultAuthSessionTimeout CDATA	#IMPLIED\
	MaxAuthSessionTimeout CDATA		#IMPLIED\
>\
<!ELEMENT Peer (#PCDATA)>\
<!ATTLIST Peer\
	FQDN		CDATA				#REQUIRED\
	realm		CDATA				#REQUIRED\
	port		CDATA				#REQUIRED\
>\
<!ELEMENT Acceptor (#PCDATA)>\
<!ATTLIST Acceptor\
	port		CDATA				#REQUIRED\
	bind		CDATA				#IMPLIED\
>\
<!ELEMENT Auth (#PCDATA)>\
<!ATTLIST Auth\
	id			CDATA				#REQUIRED\
	vendor		CDATA				#REQUIRED\
>\
<!ELEMENT Acct (#PCDATA)>\
<!ATTLIST Acct\
	id			CDATA				#REQUIRED\
	vendor		CDATA				#REQUIRED\
>\
<!ELEMENT SupportedVendor (#PCDATA)>\
<!ATTLIST SupportedVendor\
	vendor		CDATA				#REQUIRED\
>\
<!ELEMENT Realm (Route*)>\
<!ATTLIST Realm\
	name		CDATA				#REQUIRED\
>\
<!ELEMENT Route (#PCDATA)>\
<!ATTLIST Route\
	FQDN		CDATA				#REQUIRED\
	metric		CDATA				#REQUIRED\
>\
<!ELEMENT DefaultRoute (#PCDATA)>\
<!ATTLIST DefaultRoute\
	FQDN		CDATA				#REQUIRED\
	metric		CDATA				#REQUIRED\
>\
";

