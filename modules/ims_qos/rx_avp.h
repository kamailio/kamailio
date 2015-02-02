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
 *
 *
 * History:
 * --------
 *  2011-02-02  initial version (jason.penton)
 */


#ifndef __PCC_AVP_H
#define __PCC_AVP_H


#define PCC_MAX_Char 64
#define PCC_MAX_Char4 256
/* Maximum Number of characters to represent some AVP datas*/
/*and ipv6 addresses in character*/
#define PCC_Media_Sub_Components 10


struct AAA_AVP_List;
struct AAAMessage;
enum dialog_direction;

int audio_default_bandwidth;
int video_default_bandwidth;


/*helper*/
int rx_add_framed_ip_avp(AAA_AVP_LIST * list, str ip, uint16_t version);

int rx_add_avp(AAAMessage *m, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func);

int rx_add_vendor_specific_application_id_group(AAAMessage *msg, unsigned int vendorid, unsigned int auth_app_id);
int rx_add_destination_realm_avp(AAAMessage *msg, str data);
inline int rx_add_subscription_id_avp(AAAMessage *msg, str identifier, int identifier_type);
inline int rx_add_auth_application_id_avp(AAAMessage *msg, unsigned int data);

inline int rx_add_media_component_description_avp(AAAMessage *msg, int number, str *media_description, str *ipA, str *portA, str *ipB, str *portB, str *transport, 
        str *raw_payload, str *rpl_raw_payload, enum dialog_direction dlg_direction);

inline int rx_add_media_component_description_avp_register(AAAMessage *msg);

AAA_AVP *rx_create_media_subcomponent_avp(int number, char *proto, str *ipA, str *portA, str *ipB, str *portB);

AAA_AVP *rx_create_media_subcomponent_avp_register();

AAA_AVP* rx_create_codec_data_avp(str *raw_sdp_stream, int number, int direction);

inline int rx_get_result_code(AAAMessage *msg, unsigned int *data);
unsigned int rx_get_abort_cause(AAAMessage *msg);


#endif /*__PCC_AVP_H*/
