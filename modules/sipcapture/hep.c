/*
 * hep related functions
 *
 * Copyright (C) 2011-14 Alexandr Dubovikov <alexandr.dubovikov@gmail.com>
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../events.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../lib/srdb1/db.h"
#include "../../receive.h"

#include "hep.h"
#include "sipcapture.h"


static int show_error = 0;
static int count = 0;

struct hep_timehdr* heptime;

/* HEPv2 HEPv3 */
int hepv2_received(char *buf, unsigned int len, struct receive_info *ri);
int hepv3_received(char *buf, unsigned int len, struct receive_info *ri);
int parsing_hepv3_message(char *buf, unsigned int len);
/**
 * HEP message
 */
/* int hep_msg_received(char * buf, unsigned int len, struct receive_info * ri) */
int hep_msg_received(void *data)
{
        void **srevp;
        char *buf;
        unsigned *len;
        struct receive_info *ri;

        if(!hep_capture_on) {
        	if(show_error == 0) {
                	LOG(L_ERR, "sipcapture:hep_msg_received HEP is not enabled\n");
                	show_error = 1;
        	}
                return -1;
        }

        srevp = (void**)data;

        buf = (char *)srevp[0];
        len = (unsigned *)srevp[1];
        ri = (struct receive_info *)srevp[2];                        

	count++;
        struct hep_hdr *heph;
        /* hep_hdr */
        heph = (struct hep_hdr*) buf;

        /* Check version */
        if(heph->hp_v == 1 || heph->hp_v == 2)  {

                return hepv2_received(buf, *len, ri);
        }
        else if(!memcmp(buf, "\x48\x45\x50\x33",4)) {

                return hepv3_received(buf, *len, ri);
        }
        else {

                LOG(L_ERR, "ERROR: sipcapture:hep_msg_received: not supported version or bad length: v:[%d] l:[%d]\n",
                                                heph->hp_v, heph->hp_l);
                return -1;
        }
}

int hepv2_received(char *buf, unsigned int len, struct receive_info *ri){

	int hl;
        struct hep_hdr *heph;
        struct ip_addr dst_ip, src_ip;
        char *hep_payload, *end, *hep_ip;
        struct hep_iphdr *hepiph = NULL;

	struct hep_timehdr* heptime_tmp = NULL;
        memset(heptime, 0, sizeof(struct hep_timehdr));

        struct hep_ip6hdr *hepip6h = NULL;

	hep_offset = 0; 
	
	hl = hep_offset = sizeof(struct hep_hdr);
        end = buf + len;
        if (unlikely(len<hep_offset)) {
        	LOG(L_ERR, "ERROR: sipcapture:hep_msg_received len less than offset [%i] vs [%i]\n", len, hep_offset);
                return -1;
        }

	/* hep_hdr */
        heph = (struct hep_hdr*) buf;

        switch(heph->hp_f){
        	case AF_INET:
                	hl += sizeof(struct hep_iphdr);
                        break;
		case AF_INET6:
                	hl += sizeof(struct hep_ip6hdr);
                        break;
		default:
                        LOG(L_ERR, "ERROR: sipcapture:hep_msg_received:  unsupported family [%d]\n", heph->hp_f);
                        return -1;
	}

        /* PROTO */
        if(heph->hp_p == IPPROTO_UDP) ri->proto=PROTO_UDP;
        else if(heph->hp_p == IPPROTO_TCP) ri->proto=PROTO_TCP;
        else if(heph->hp_p == IPPROTO_IDP) ri->proto=PROTO_TLS; /* fake protocol */
#ifdef USE_SCTP
        else if(heph->hp_p == IPPROTO_SCTP) ri->proto=PROTO_SCTP;
#endif
        else {
        	LOG(L_ERR, "ERROR: sipcapture:hep_msg_received: unknow protocol [%d]\n",heph->hp_p);
                ri->proto = PROTO_NONE;
	}

        hep_ip = buf + sizeof(struct hep_hdr);

        if (unlikely(hep_ip>end)){
                LOG(L_ERR,"hep_ip is over buf+len\n");
                return -1;
        }

	switch(heph->hp_f){
		case AF_INET:
                	hep_offset+=sizeof(struct hep_iphdr);
                        hepiph = (struct hep_iphdr*) hep_ip;
                        break;

		case AF_INET6:
                	hep_offset+=sizeof(struct hep_ip6hdr);
                        hepip6h = (struct hep_ip6hdr*) hep_ip;
                        break;

	}

	/* VOIP payload */
        hep_payload = buf + hep_offset;

        if (unlikely(hep_payload>end)){
        	LOG(L_ERR,"hep_payload is over buf+len\n");
                return -1;
	}

	/* timming */
        if(heph->hp_v == 2) {
                hep_offset+=sizeof(struct hep_timehdr);
                heptime_tmp = (struct hep_timehdr*) hep_payload;

                heptime->tv_sec = heptime_tmp->tv_sec;
                heptime->tv_usec = heptime_tmp->tv_usec;
                heptime->captid = heptime_tmp->captid;
        }


	/* fill ip from the packet to dst_ip && to */
        switch(heph->hp_f){

		case AF_INET:
                	dst_ip.af = src_ip.af = AF_INET;
                        dst_ip.len = src_ip.len = 4 ;
                        memcpy(&dst_ip.u.addr, &hepiph->hp_dst, 4);
                        memcpy(&src_ip.u.addr, &hepiph->hp_src, 4);
                        break;

		case AF_INET6:
                	dst_ip.af = src_ip.af = AF_INET6;
                        dst_ip.len = src_ip.len = 16 ;
                        memcpy(&dst_ip.u.addr, &hepip6h->hp6_dst, 16);
                        memcpy(&src_ip.u.addr, &hepip6h->hp6_src, 16);
                        break;

	}

        ri->src_ip = src_ip;
        ri->src_port = ntohs(heph->hp_sport);

        ri->dst_ip = dst_ip;
        ri->dst_port = ntohs(heph->hp_dport);

	/* cut off the offset */
	/* 
	 *  len -= offset;
         *  p = buf + offset;
	 *  memmove(buf, p, BUF_SIZE+1); 
	*/

        hep_payload = buf + hep_offset;

        receive_msg(hep_payload,(unsigned int)(len - hep_offset), ri);
	
	return -1;
}


/**
 * HEP message
 */
int hepv3_received(char *buf, unsigned int len, struct receive_info *ri)
{
	if(!parsing_hepv3_message(buf, len)) {
		LM_ERR("couldn't parse hepv3 message\n");
        	return -2;
        }

	return -1;
}

int parsing_hepv3_message(char *buf, unsigned int len) {

	union sockaddr_union from;
	union sockaddr_union to;
        struct receive_info ri;
	char *tmp;
	struct ip_addr dst_ip, src_ip;
	struct socket_info* si = 0;
	int tmp_len, i;
	char *payload = NULL;
	unsigned int payload_len = 0;
        struct hep_chunk *chunk;	
        struct hep_generic_recv *hg;
        int totelem = 0;
        int chunk_vendor=0, chunk_type=0, chunk_length=0;
        int total_length = 0;


	hg = (struct hep_generic_recv*)pkg_malloc(sizeof(struct hep_generic_recv));
	if(hg==NULL) {
	        LM_ERR("no more pkg memory left for hg\n");
	        return -1;
        }
	                                                 		
	memset(hg, 0, sizeof(struct hep_generic_recv));

	
        memset(heptime, 0, sizeof(struct hep_timehdr));	
	        

	/* HEADER */
	hg->header  = (hep_ctrl_t *) (buf);

	/*Packet size */
	total_length = ntohs(hg->header->length);

	ri.src_port = 0;
	ri.dst_port = 0;
	dst_ip.af = 0;
        src_ip.af = 0;
                	        
	payload = NULL;

	i = sizeof(hep_ctrl_t);	        
	        
	while(i < total_length) {
                
	        /*OUR TMP DATA */                                  
                tmp = buf+i;

                chunk = (struct hep_chunk*) tmp;
                             
                chunk_vendor = ntohs(chunk->vendor_id);                             
                chunk_type = ntohs(chunk->type_id);
                chunk_length = ntohs(chunk->length);
                       


                /* if chunk_length */
                if(chunk_length == 0) {
                        /* BAD LEN we drop this packet */
                        goto error;
                }

                /* SKIP not general Chunks */
                if(chunk_vendor != 0) {
                        i+=chunk_length;
                }
                else {                                                                                                                               
                        switch(chunk_type) {
                                     
                                case 0:
                                        goto error;
                                        break;
                                     
                                case 1:                                                                          
                                        hg->ip_family  = (hep_chunk_uint8_t *) (tmp);
                                        i+=chunk_length;
                                        totelem++;
                                        break;
                                case 2:
                                        hg->ip_proto  = (hep_chunk_uint8_t *) (tmp);
                                        i+=chunk_length;
                                        totelem++;
                                        break;                                                     
                                case 3:
                                        hg->hep_src_ip4  = (hep_chunk_ip4_t *) (tmp);
                                        i+=chunk_length;
                                        src_ip.af=AF_INET;
				        src_ip.len=4;
				        src_ip.u.addr32[0] = hg->hep_src_ip4->data.s_addr;
				        totelem++;
				        break;
                                case 4:
                                        hg->hep_dst_ip4  = (hep_chunk_ip4_t *) (tmp);
                                        i+=chunk_length;                                                     
					dst_ip.af=AF_INET;
				        dst_ip.len=4;
				        dst_ip.u.addr32[0] = hg->hep_dst_ip4->data.s_addr;
                                        totelem++;

                                        break;
                                case 5:
                                        hg->hep_src_ip6  = (hep_chunk_ip6_t *) (tmp);
                                        i+=chunk_length;
                                        src_ip.af=AF_INET6;
				        src_ip.len=16;
				        memcpy(src_ip.u.addr, &hg->hep_src_ip6->data, 16);
				        totelem++;
                                        break;
                                case 6:
                                        hg->hep_dst_ip6  = (hep_chunk_ip6_t *) (tmp);
                                        i+=chunk_length;                                                     
                                        dst_ip.af=AF_INET6;
				        dst_ip.len=16;
				        memcpy(dst_ip.u.addr, &hg->hep_dst_ip6->data, 16);
				        totelem++;
                                        break;
        
                                case 7:
                                        hg->src_port  = (hep_chunk_uint16_t *) (tmp);
                                        ri.src_port = ntohs(hg->src_port->data);
                                        i+=chunk_length;                      
                                        totelem++;
                                        break;

                                case 8:
                                        hg->dst_port  = (hep_chunk_uint16_t *) (tmp);
                                        ri.dst_port = ntohs(hg->dst_port->data);
                                        i+=chunk_length;
                                        totelem++;
                                        break;
                                case 9:
                                        hg->time_sec  = (hep_chunk_uint32_t *) (tmp);
                                        hg->time_sec->data = ntohl(hg->time_sec->data);
                                        heptime->tv_sec = hg->time_sec->data;
                                        i+=chunk_length;
                                        totelem++;
                                        break;                                                     
                                                     
                                case 10:
                                        hg->time_usec  = (hep_chunk_uint32_t *) (tmp);
                                        hg->time_usec->data = ntohl(hg->time_usec->data);
                                        heptime->tv_usec = hg->time_usec->data;
                                        i+=chunk_length;
                                        totelem++;
                                        break;      

                                case 11:
                                        hg->proto_t  = (hep_chunk_uint8_t *) (tmp);
                                        i+=chunk_length;
                                        totelem++;
                                        break;                                                                                                                                                         

                                case 12:
                                        hg->capt_id  = (hep_chunk_uint32_t *) (tmp);
                                        i+=chunk_length;
                                        heptime->captid = ntohs(hg->capt_id->data);
                                        totelem++;
                                        break;

                                case 13:
                                        hg->keep_tm  = (hep_chunk_uint16_t *) (tmp);
                                        i+=chunk_length;
                                        break;                                                     

                                case 14:
                                        authkey = (char *) tmp + sizeof(hep_chunk_t);
                                        i+=chunk_length;                                                                             
                                        break;
                                                     
                                case 15:
                                        hg->payload_chunk  = (hep_chunk_t *) (tmp);
                                        payload = (char *) tmp+sizeof(hep_chunk_t);
                                        payload_len = chunk_length - sizeof(hep_chunk_t);
                                        i+=chunk_length;
                                        totelem++;
                                        break;
                                case 17:
                                
                                        correlation_id = (char *) tmp + sizeof(hep_chunk_t);
                                        i+=chunk_length;                                                                            
					break;

                                                     
                                default:
                                        i+=chunk_length;
                                        break;
                        }                                        
                }
        }	                                                                                                          
                        
        /* CHECK how much elements */
        if(totelem < 9) {                        
                LM_ERR("Not all elements [%d]\n", totelem);                        
                goto done;
        }                 

        if ( dst_ip.af == 0 || src_ip.af == 0)  {
                LM_ERR("NO IP's set\n");
                goto done;
        }

                        
        ip_addr2su(&to, &dst_ip, ri.dst_port);
        ip_addr2su(&from, &src_ip, ri.src_port);
                        
        ri.src_su=from;
        su2ip_addr(&ri.src_ip, &from);
        su2ip_addr(&ri.dst_ip, &to);

	if(hg->ip_proto->data == IPPROTO_TCP) ri.proto=PROTO_TCP;
	else if(hg->ip_proto->data == IPPROTO_UDP) ri.proto=PROTO_UDP;

	/* a little bit memory */
        si=(struct socket_info*) pkg_malloc(sizeof(struct socket_info));
        if (si==0) {
                LOG(L_ERR, "ERROR: new_sock_info: memory allocation error\n");
                goto error;
        }

	memset(si, 0, sizeof(struct socket_info));
        si->address = ri.dst_ip;
        si->socket=-1;

        /* set port & proto */
        si->port_no = ri.dst_port;

        if(hg->ip_proto->data == IPPROTO_TCP) si->proto=PROTO_TCP;
        else if(hg->ip_proto->data == IPPROTO_UDP) si->proto=PROTO_UDP;

        si->flags=0;
        si->addr_info_lst=0;

        si->address_str.s = ip_addr2a(&si->address);;
        si->address_str.len = strlen(si->address_str.s);                                                

        si->port_no_str.s = int2str(si->port_no, &tmp_len);
        si->port_no_str.len = tmp_len;
	si->address_str.len = strlen(si->address_str.s);

        si->name.len = si->address_str.len;
        si->name.s = si->address_str.s;
        ri.bind_address=si;


	/*TIME*/ 
        heptime->tv_sec = hg->time_sec->data;
        heptime->tv_usec = hg->time_usec->data;
        heptime->captid = ntohs(hg->capt_id->data);
          

        if(payload != NULL ) {
                /* and now recieve message */                
                if (hg->proto_t->data == 5) receive_logging_json_msg(payload, payload_len, hg, "rtcp_capture");
                else if (hg->proto_t->data == 100) receive_logging_json_msg(payload, payload_len, hg, "logs_capture");                
                else receive_msg(payload, payload_len, &ri);
        }

done:
        if(si) pkg_free(si);
        if(hg) pkg_free(hg);                     

        return 1;
        
error:

        if(si) pkg_free(si);
        if(hg) pkg_free(hg);
                
        return -1;           
        
}





