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

#include "bin_utils.h"
#include "../../locking.h"

#include "../../lib/ims/useful_defs.h"
/** 
 * Whether to print debug message while encoding/decoding 
 */
#define BIN_DEBUG 0

/** 
 * Whether to do sanity checks on the available data when decoding
 * If you are crazy about start-up performance you can disable this.
 * However, this is very useful for detecting broken snapshots
 */
#define BIN_DECODE_CHECKS 1

static inline int str_shm_dup(str *dest,str *src)
{
	dest->s = shm_malloc(src->len);
	if (!dest->s){
		LM_ERR("str_shm_dup: Error allocating %d bytes\n",src->len);
		dest->len=0;
		return 0;
	}
	dest->len = src->len;
	memcpy(dest->s,src->s,src->len);
	return 1;
}


inline int bin_alloc(bin_data *x, int max_len)
{                                
	x->s = (char*)BIN_ALLOC_METHOD(max_len);     
	if (!x->s){
		LM_ERR("Error allocating %d bytes.\n",max_len);
		x->len=0;
		x->max=0;
		return 0;
	}
    x->len=0;
    x->max=max_len;
	return 1;
}

inline int bin_realloc(bin_data *x, int delta)
{
#if BIN_DEBUG
	LOG(L_INFO,"INFO:"M_NAME":bin_realloc: realloc %p from %d to + %d\n",x->s,x->max,delta);
#endif	
	x->s=BIN_REALLOC_METHOD(x->s,x->max + delta);    
	if (x->s==NULL){                             
		LM_ERR("No more memory to expand %d with %d  \n",x->max,delta);
		return 0;
	}
	x->max += delta;
	return 1;
}

inline int bin_expand(bin_data *x, int delta)
{
	if (x->max-x->len>=delta) return 1;
#if BIN_DEBUG	
	LOG(L_INFO,"INFO:"M_NAME":bin_realloc: realloc %p from %d to + %d\n",x->s,x->max,delta);
#endif	
	x->s=BIN_REALLOC_METHOD(x->s,x->max + delta);    
	if (x->s==NULL){                             
		LM_ERR("No more memory to expand %d with %d  \n",x->max,delta);
		return 0;
	}
	x->max += delta;
	return 1;
}

inline void bin_free(bin_data *x)
{
	BIN_FREE_METHOD(x->s);
	x->s=0;x->len=0;x->max=0;
}

/**
 *	simple print function 
 */
inline void bin_print(bin_data *x)
{
	int i,j,w=16;
	char c;
	fprintf(stderr,"----------------------------------\nBinary form %d (max %d) bytes:\n",x->len,x->max);
	for(i=0;i<x->len;i+=w){
		fprintf(stderr,"%04X> ",i);
		for(j=0;j<w;j++){
			if (i+j<x->len) fprintf(stderr,"%02X ",(unsigned char)x->s[i+j]);
			else fprintf(stderr,"   ");
		}
		printf("\t");
		for(j=0;j<w;j++)if (i+j<x->len){
			if (x->s[i+j]>32) c=x->s[i+j];
			else c = '.';
			fprintf(stderr,"%c",c);
		}else fprintf(stderr," ");
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"\n---------------------------------\n");
}

/* basic data type reprezentation functions */




/**
 *	Append a char of 1 byte 
 */
inline int bin_encode_char(bin_data *x,char k) 
{ 
	if (!bin_expand(x,1)) return 0;
	x->s[x->len++]= k; 
#if BIN_DEBUG	
	LM_ERR("[%d]:[%.02x] new len %04x\n",k,x->s[x->len-1],x->len);
#endif
	return 1;   
}
/**
 *	Decode of 1 char
 */
inline int bin_decode_char(bin_data *x,char *c)
{
#if BIN_DECODE_CHECKS
	if (x->max+1 > x->len) return 0;
#endif	
	*c = x->s[x->max];
	x->max += 1;
#if BIN_DEBUG	
	LM_ERR("bin_decode_char: [%d] new pos %04x\n",*c,x->max);
#endif
	return 1;
}




/**
 *	Append an unsigned char of 1 byte 
 */
inline int bin_encode_uchar(bin_data *x,unsigned char k) 
{ 
	if (!bin_expand(x,1)) return 0;
	x->s[x->len++]= k; 
#if BIN_DEBUG	
	LM_ERR("bin_encode_uchar: [%u]:[%.02x] new len %04x\n",k,x->s[x->len-1],x->len);
#endif
	return 1;   
}
/**
 *	Decode of 1 unsigned char
 */
inline int bin_decode_uchar(bin_data *x,unsigned char *c)
{
#if BIN_DECODE_CHECKS
	if (x->max+1 > x->len) return 0;
#endif	
	*c = x->s[x->max];
	x->max += 1;
#if BIN_DEBUG	
	LM_ERR("bin_decode_uchar: [%u] new pos %04x\n",*c,x->max);
#endif
	return 1;
}







/**
 *	Append the a short  
 */
inline int bin_encode_short(bin_data *x,short k) 
{ 
	if (!bin_expand(x,2)) return 0;
	x->s[x->len++]=k & 0x00FF;    
	x->s[x->len++]=(k & 0xFF00) >> 8;   
#if BIN_DEBUG	
	LM_ERR("bin_encode_short: [%d]:[%.02x %.02x] new len %04x\n",k,x->s[x->len-2],x->s[x->len-1],x->len);
#endif
	return 1;   
}
/**
 *	Decode of a short
 */
inline int bin_decode_short(bin_data *x,short *v)
{
#if BIN_DECODE_CHECKS
	if (x->max+2 > x->len) return 0;
#endif
	*v =	(unsigned char)x->s[x->max  ]    |
	 		(unsigned char)x->s[x->max+1]<<8;
	x->max += 2;
#if BIN_DEBUG	
	LM_ERR("bin_decode_short: [%d] new pos %04x\n",*v,x->max);
#endif
	return 1;
}


/**
 *	Append the an unsigned short  
 */
inline int bin_encode_ushort(bin_data *x,unsigned short k) 
{ 
	if (!bin_expand(x,2)) return 0;
	x->s[x->len++]=k & 0x00FF;    
	x->s[x->len++]=(k & 0xFF00) >> 8;   
#if BIN_DEBUG	
	LM_ERR("bin_encode_ushort: [%u]:[%.02x %.02x] new len %04x\n",k,x->s[x->len-2],x->s[x->len-1],x->len);
#endif
	return 1;   
}
/**
 *	Decode of a short
 */
inline int bin_decode_ushort(bin_data *x,unsigned short *v)
{
#if BIN_DECODE_CHECKS
	if (x->max+2 > x->len) return 0;
#endif
	*v =	(unsigned char)x->s[x->max  ]    |
	 		(unsigned char)x->s[x->max+1]<<8;
	x->max += 2;
#if BIN_DEBUG	
	LM_ERR("bin_decode_ushort: [%u] new pos %04x\n",*v,x->max);
#endif
	return 1;
}


/**
 *	Append an integer
 */
inline int bin_encode_int(bin_data *x,int k) 
{ 
	int len = sizeof(int),i;
	if (!bin_expand(x,len)) return 0;
	for(i=0;i<len;i++){
		x->s[x->len++]= k & 0xFF;
		k = k>>8;          
	}
#if BIN_DEBUG		    
	switch(len){
		case 4:
			LM_ERR(":bin_encode_int: [%d]:[%.02x %.02x %.02x %.02x] new len %04x\n",k,
				x->s[x->len-4],x->s[x->len-3],x->s[x->len-2],x->s[x->len-1],x->len);
			break;
		case 8:
			LM_ERR("bin_encode_int: [%d]:[%.02x %.02x %.02x %.02x%.02x %.02x %.02x %.02x] new len %04x\n",k,
				x->s[x->len-8],x->s[x->len-7],x->s[x->len-6],x->s[x->len-5],
				x->s[x->len-4],x->s[x->len-3],x->s[x->len-2],x->s[x->len-1],
				x->len);
			break;
	}
#endif		
	return 1;   
}
/**
 *	Decode an integer
 */
inline int bin_decode_int(bin_data *x,int *v)
{
	int len = sizeof(int),i;
#if BIN_DECODE_CHECKS
	if (x->max+len > x->len) return 0;
#endif
	*v = 0;
	for(i=0;i<len;i++)
		*v =  *v | ((unsigned char)x->s[x->max++] <<(8*i));
#if BIN_DEBUG	
	LM_ERR("bin_decode_int: [%d] new pos %04x\n",*v,x->max);
#endif
	return 1;
}



/**
 *	Append an unsigned integer
 */
inline int bin_encode_uint(bin_data *x,unsigned int k) 
{ 
	int len = sizeof(unsigned int),i;
	if (!bin_expand(x,len)) return 0;
	for(i=0;i<len;i++){
		x->s[x->len++]= k & 0xFF;
		k = k>>8;          
	}
#if BIN_DEBUG		    
	switch(len){
		case 4:
			LM_ERR("bin_encode_uint: [%u]:[%.02x %.02x %.02x %.02x] new len %04x\n",k,
				x->s[x->len-4],x->s[x->len-3],x->s[x->len-2],x->s[x->len-1],x->len);
			break;
		case 8:
			LM_ERR("bin_encode_uint: [%u]:[%.02x %.02x %.02x %.02x%.02x %.02x %.02x %.02x] new len %04x\n",k,
				x->s[x->len-8],x->s[x->len-7],x->s[x->len-6],x->s[x->len-5],
				x->s[x->len-4],x->s[x->len-3],x->s[x->len-2],x->s[x->len-1],
				x->len);
			break;
	}
#endif		
	return 1;   
}
/**
 *	Decode an unsigned integer
 */
inline int bin_decode_uint(bin_data *x,unsigned int *v)
{
	int len = sizeof(unsigned int),i;
#if BIN_DECODE_CHECKS
	if (x->max+len > x->len) return 0;
#endif
	*v = 0;
	for(i=0;i<len;i++)
		*v =  *v | ((unsigned char)x->s[x->max++] <<(8*i));
#if BIN_DEBUG	
	LM_ERR("in_decode_uint: [%u] new pos %04x\n",*v,x->max);
#endif
	return 1;
}

/**
 *	Append a time_t structure
 */
inline int bin_encode_time_t(bin_data *x,time_t k) 
{ 
	int len = sizeof(time_t),i;
	if (!bin_expand(x,len)) return 0;
	for(i=0;i<len;i++){
		x->s[x->len++]= k & 0xFF;
		k = k>>8;          
	}
#if BIN_DEBUG		    
	switch(len){
		case 4:
			LM_ERR("bin_encode_time_t: [%u]:[%.02x %.02x %.02x %.02x] new len %04x\n",(unsigned int)k,
				x->s[x->len-4],x->s[x->len-3],x->s[x->len-2],x->s[x->len-1],x->len);
			break;
		case 8:
			LM_ERR("bin_encode_time_t: [%u]:[%.02x %.02x %.02x %.02x%.02x %.02x %.02x %.02x] new len %04x\n",(unsigned int)k,
				x->s[x->len-8],x->s[x->len-7],x->s[x->len-6],x->s[x->len-5],
				x->s[x->len-4],x->s[x->len-3],x->s[x->len-2],x->s[x->len-1],
				x->len);
			break;
	}
#endif		
	return 1;   
}
/**
 *	Decode an unsigned integer
 */
inline int bin_decode_time_t(bin_data *x,time_t *v)
{
	int len = sizeof(time_t),i;
#if BIN_DECODE_CHECKS
	if (x->max+len > x->len) return 0;
#endif
	*v = 0;
	for(i=0;i<len;i++)
		*v =  *v | ((unsigned char)x->s[x->max++] <<(8*i));
#if BIN_DEBUG	
	LM_ERR("bin_decode_time_t: [%u] new pos %04x\n",(unsigned int) *v,x->max);
#endif
	return 1;
}


/**
 *	Append a string 
 */
inline int bin_encode_str(bin_data *x,str *s) 
{ 
	if (!bin_expand(x,2+s->len)) return 0;
	if (s->len>65535) 
		LM_ERR("bin_encode_str: Possible loss of characters in encoding (string > 65535bytes) %d bytes \n",s->len);
	x->s[x->len++]=s->len & 0x000000FF;
	x->s[x->len++]=(s->len & 0x0000FF00)>>8;
	memcpy(x->s+x->len,s->s,s->len);
	x->len+=s->len;
#if BIN_DEBUG		
	LM_ERR(":bin_encode_str : [%d]:[%.02x %.02x]:[%.*s] new len %04x\n",s->len,
		x->s[x->len-s->len-2],x->s[x->len-s->len-1],s->len,s->s,x->len);
#endif		
	return 1;   
}
/**
 *	Decode of a str string
 */
inline int bin_decode_str(bin_data *x,str *s)
{
#if BIN_DECODE_CHECKS
	if (x->max+2 > x->len) return 0;
#endif
	s->len = (unsigned char)x->s[x->max  ]    |
	 		(unsigned char)x->s[x->max+1]<<8;
	x->max +=2;
	if (x->max+s->len>x->len) return 0;
	s->s = x->s + x->max;
	x->max += s->len;
#if BIN_DEBUG	
	LM_ERR("bin_decode_str : [%d]:[%.*s] new pos %04x\n",s->len,s->len,s->s,x->max);
#endif
	return 1;
}




/**
 *	Encode and append a Public Indentity
 * @param x - binary data to append to
 * @param pi - the public identity to encode
 * @returns 1 on succcess or 0 on error
 */
static int bin_encode_public_identity(bin_data *x,ims_public_identity *pi)
{
	if (!bin_encode_char(x,pi->barring)) goto error;
	if (!bin_encode_str(x,&(pi->public_identity))) goto error;	
	return 1;
error:
	LM_ERR("bin_encode_public_identity: Error while encoding.\n");
	return 0;		
}

/**
 *	Decode a binary string from a binary data structure
 * @param x - binary data to decode from
 * @param pi - the public identity to decode into
 * @returns 1 on succcess or 0 on error
 */
static int bin_decode_public_identity(bin_data *x,ims_public_identity *pi)
{
	str s;
	if (!bin_decode_char(x,	&(pi->barring))) goto error;
	if (!bin_decode_str(x,&s)||!str_shm_dup(&(pi->public_identity),&s))	goto error;
	
	return 1;
error:
	LM_ERR("bin_decode_public_identity: Error while decoding (at %d (%04x)).\n",x->max,x->max);
	if (pi) {
		if (pi->public_identity.s) shm_free(pi->public_identity.s);
	}
	return 0;
}

/**
 *	Encode and append a SPT
 * @param x - binary data to append to
 * @param spt - the service point trigger to encode
 * @returns 1 on succcess or 0 on error
 */
static int bin_encode_spt(bin_data *x, ims_spt *spt)
{
	unsigned char c = spt->condition_negated<<7 | spt->registration_type<<4 | spt->type;
	// cond negated, reg type, spt type
	if (!bin_encode_uchar(x,c)) goto error;

	//group
	if (!bin_encode_int(x,spt->group)) goto error;

	//spt
	switch(spt->type){
		case 1:
			if (!bin_encode_str(x,&(spt->request_uri))) goto error; 
			break;
		case 2:
			if (!bin_encode_str(x,&(spt->method))) goto error; 
			break;
		case 3:
			if (!bin_encode_short(x,spt->sip_header.type)) goto error;
			if (!bin_encode_str(x,&(spt->sip_header.header))) goto error; 
			if (!bin_encode_str(x,&(spt->sip_header.content))) goto error; 
			break;
		case 4:
			if (!bin_encode_char(x,spt->session_case)) goto error;
			break;
		case 5:
			if (!bin_encode_str(x,&(spt->session_desc.line))) goto error; 
			if (!bin_encode_str(x,&(spt->session_desc.content))) goto error; 
			break;
	}
	return 1;
error:
	LM_ERR("bin_encode_spt: Error while encoding.\n");
	return 0;		
}


/**
 *	Decode an SPT
 * @param x - binary data to decode from
 * @param spt - the service point trigger to decode into
 * @returns 1 on succcess or 0 on error
 */
static int bin_decode_spt(bin_data *x, ims_spt *spt)
{
	unsigned char k;
	str s;
	
	if (!bin_decode_uchar(x,&k)) goto error;
	
	spt->type = k & 0x0F;
	spt->condition_negated = ((k & 0x80)!=0);
	spt->registration_type = ((k & 0x70)>>4);
	
	if (!bin_decode_int(x,&(spt->group))) goto error;

	switch (spt->type){
		case 1:
			if (!bin_decode_str(x,&s)||!str_shm_dup(&(spt->request_uri),&s)) goto error;
			break;
		case 2:
			if (!bin_decode_str(x,&s)||!str_shm_dup(&(spt->method),&s)) goto error;
			break;
		case 3:
			if (!bin_decode_short(x,&(spt->sip_header.type))) goto error;
			if (!bin_decode_str(x,&s)||!str_shm_dup(&(spt->sip_header.header),&s)) goto error;
			if (!bin_decode_str(x,&s)||!str_shm_dup(&(spt->sip_header.content),&s)) goto error;
			break;
		case 4:
			if (!bin_decode_char(x,&(spt->session_case))) goto error;
			break;
		case 5:
			if (!bin_decode_str(x,&s)||!str_shm_dup(&(spt->session_desc.line),&s)) goto error;
			if (!bin_decode_str(x,&s)||!str_shm_dup(&(spt->session_desc.content),&s)) goto error;
			break;

	}
	return 1;
	
error:
	LM_ERR("bin_decode_spt: Error while decoding (at %d (%04x)).\n",x->max,x->max);
	if (spt){
		switch (spt->type){
			case 1:
				if (spt->request_uri.s) shm_free(spt->request_uri.s);
				break;
			case 2:
				if (spt->method.s) shm_free(spt->method.s);
				break;
			case 3:
				if (spt->sip_header.header.s) shm_free(spt->sip_header.header.s);
				if (spt->sip_header.header.s) shm_free(spt->sip_header.content.s);
				break;
			case 4:
				break;
			case 5:
				if (spt->sip_header.header.s) shm_free(spt->session_desc.line.s);
				if (spt->sip_header.header.s) shm_free(spt->session_desc.content.s);
				break;
		}
	}
	return 0;
}	



/**
 *	Encode and Append a Filter Criteria
 * @param x - binary data to append to
 * @param spt - the service point trigger to encode
 * @returns 1 on succcess or 0 on error
 */
static int bin_encode_filter_criteria(bin_data *x, ims_filter_criteria *fc)
{
	int i;
	char ppindicator;

	//priority
	if (!bin_encode_int(x,fc->priority)) goto error;
	
	//profile part indicator
	if (fc->profile_part_indicator) ppindicator = (*fc->profile_part_indicator)+1;
	else ppindicator = 0;
	if (!bin_encode_char(x,ppindicator)) goto error;
			
	// trigger point 
	if (fc->trigger_point) {
		if (!bin_encode_char(x,fc->trigger_point->condition_type_cnf)) goto error;
		
		if (!bin_encode_ushort(x,fc->trigger_point->spt_cnt)) goto error;
		
		for(i=0;i<fc->trigger_point->spt_cnt;i++)
			if (!bin_encode_spt(x,fc->trigger_point->spt+i)) goto error;
	} else {
		if (!bin_encode_char(x,100)) goto error;
	}
	
	//app server
	if (!bin_encode_str(x,&(fc->application_server.server_name))) goto error;
	if (!bin_encode_char(x,fc->application_server.default_handling)) goto error;
	if (!bin_encode_str(x,&(fc->application_server.service_info))) goto error;
	
	return 1;
error:
	LM_ERR("bin_encode_filter_criteria: Error while encoding.\n");
	return 0;		
}


/**
 *	Decode a Filter Criteria
 * @param x - binary data to decode from
 * @param fc - filter criteria to decode into
 * @returns 1 on succcess or 0 on error
 */
static int bin_decode_filter_criteria(bin_data *x, ims_filter_criteria *fc)
{
	int i,len;
	str s;
	char ppindicator,cnf;
	
	//priority
	if (!bin_decode_int(x,&(fc->priority))) goto error;
	
	// profile part indicator
	if (!bin_decode_char(x,&ppindicator)) goto error;
	if (!ppindicator){
		fc->profile_part_indicator = 0;
	}
	else {
		fc->profile_part_indicator = (char*)shm_malloc(sizeof(char));
		if (!fc->profile_part_indicator) {
			LM_ERR("bin_decode_filter_criteria: Error allocating %lx bytes.\n",sizeof(int));
			goto error;
		}
		*(fc->profile_part_indicator) = ppindicator-1;
	}
	
	//cnf 
	if (!bin_decode_char(x,&cnf)) goto error;

	if (cnf==100)
		fc->trigger_point=NULL;
	else {
		ims_trigger_point *tp=0;
		//trigger point
		len = sizeof(ims_trigger_point);
		tp = (ims_trigger_point*)shm_malloc(len);
		fc->trigger_point = tp;
		if (!tp) {
			LM_ERR("bin_decode_filter_criteria: Error allocating %d bytes.\n",len);
			goto error;
		}
		memset(tp,0,len);
		tp->condition_type_cnf=cnf;
		
		if (!bin_decode_ushort(x,&tp->spt_cnt)) goto error;
		len = sizeof(ims_spt)*tp->spt_cnt;
		tp->spt = (ims_spt*)shm_malloc(len);
		if (!tp->spt) {
			LM_ERR("bin_decode_filter_criteria: Error allocating %d bytes.\n",len);
			goto error;
		}
		memset(tp->spt,0,len);
		for(i=0;i<tp->spt_cnt;i++)
			if (!bin_decode_spt(x,tp->spt+i)) goto error;
	}
	//app server uri
	if (!bin_decode_str(x,&s)||!str_shm_dup(&(fc->application_server.server_name),&s)) goto error;
	// app server default handling
	if (!bin_decode_char(x,&(fc->application_server.default_handling)))goto error;
	// app server service info
	if (!bin_decode_str(x,&s)||!str_shm_dup(&(fc->application_server.service_info),&s)) goto error;

	return 1;
error:
	LM_ERR("bin_decode_filter_criteria: Error while decoding (at %d (%04x)).\n",x->max,x->max);
	if (fc){
		if (fc->trigger_point){
			if (fc->trigger_point){
				if (fc->trigger_point->spt) shm_free(fc->trigger_point->spt);
			}
			shm_free(fc->trigger_point);
		}
		if (fc->application_server.server_name.s) shm_free(fc->application_server.server_name.s);
		if (fc->application_server.service_info.s) shm_free(fc->application_server.service_info.s);
	}
	return 0;		
}



/**
 *	Encode and append a Service Profile
 * @param x - binary data to append to
 * @param sp - the service profile to encode
 * @returns 1 on succcess or 0 on error
 */
static int bin_encode_service_profile(bin_data *x,ims_service_profile *sp)
{
	int i;
		
	//public identity
	if (!bin_encode_ushort(x,sp->public_identities_cnt)) return 0;
	for(i=0;i<sp->public_identities_cnt;i++)
		if (!bin_encode_public_identity(x,sp->public_identities+i)) goto error;
	
	//filter criteria
	if (!bin_encode_ushort(x,sp->filter_criteria_cnt)) return 0;
	for(i=0;i<sp->filter_criteria_cnt;i++)
		if (!bin_encode_filter_criteria(x,sp->filter_criteria+i)) goto error;
		
	//cn service_auth
	if (sp->cn_service_auth)
		i = sp->cn_service_auth->subscribed_media_profile_id;
	else i = 0xFFFFFFFF;
	if (!bin_encode_int(x,i)) goto error;

	//shared_ifc
	if (!bin_encode_ushort(x,sp->shared_ifc_set_cnt)) return 0;
	for(i=0;i<sp->shared_ifc_set_cnt;i++)
		if (!bin_encode_int(x,sp->shared_ifc_set[i])) goto error;

	return 1;
error:
	LM_ERR("bin_encode_service_profile: Error while encoding.\n");
	return 0;		
}


/**
 *	Decode a service profile
 * @param x - binary data to decode from
 * @param sp - service profile to decode into
 * @returns 1 on succcess or 0 on error
 */
static int bin_decode_service_profile(bin_data *x, ims_service_profile *sp)
{
	int i,len;

	//public identities
	if (!bin_decode_ushort(x,&(sp->public_identities_cnt))) goto error;
	len = sizeof(ims_public_identity)*sp->public_identities_cnt;
	sp->public_identities = (ims_public_identity*)shm_malloc(len);
	if (!sp->public_identities) {
		LM_ERR("bin_decode_service_profile: Error allocating %d bytes.\n",len);
		goto error;
	}
	memset(sp->public_identities,0,len);
	for(i=0;i<sp->public_identities_cnt;i++)
		if (!bin_decode_public_identity(x,sp->public_identities+i)) goto error;
	
	// filter criteria
	if (!bin_decode_ushort(x,&(sp->filter_criteria_cnt))) goto error;	
	len = sizeof(ims_filter_criteria)*sp->filter_criteria_cnt;
	sp->filter_criteria = (ims_filter_criteria*)shm_malloc(len);
	if (!sp->filter_criteria) {
		LM_ERR("bin_decode_service_profile: Error allocating %d bytes.\n",len);
		goto error;
	}
	memset(sp->filter_criteria,0,len);
	for(i=0;i<sp->filter_criteria_cnt;i++)
		if (!bin_decode_filter_criteria(x,sp->filter_criteria+i)) goto error;

	// cn service auth
	if (!bin_decode_int(x,&i)) goto error;
	if (i==0xFFFFFFFF)
		sp->cn_service_auth = 0;
	else {
		len = sizeof(ims_cn_service_auth);
		sp->cn_service_auth = (ims_cn_service_auth*)shm_malloc(len);
		if (!sp->cn_service_auth) {
			LM_ERR("bin_decode_service_profile: Error allocating %d bytes.\n",len);
			goto error;
		}
		sp->cn_service_auth->subscribed_media_profile_id=i;
	}
	
	//shared ifc
	if (!bin_decode_ushort(x,&(sp->shared_ifc_set_cnt))) goto error;	
	len = sizeof(int)*sp->shared_ifc_set_cnt;
	sp->shared_ifc_set = (int*)shm_malloc(len);
	if (!sp->shared_ifc_set) {
		LM_ERR("bin_decode_service_profile: Error allocating %d bytes.\n",len);
		goto error;
	}
	memset(sp->shared_ifc_set,0,len);
	for(i=0;i<sp->shared_ifc_set_cnt;i++)
		if (!bin_decode_int(x,sp->shared_ifc_set+i)) goto error;

	return 1;
error:
	LM_ERR("bin_decode_service_profile: Error while decoding (at %d (%04x)).\n",x->max,x->max);
	if (sp) {
		if (sp->public_identities) shm_free(sp->public_identities);
		if (sp->filter_criteria) shm_free(sp->filter_criteria);
		if (sp->cn_service_auth) shm_free(sp->cn_service_auth);
		if (sp->shared_ifc_set) shm_free(sp->shared_ifc_set);
	}
	return 0;
}

/**
 *	Encode the entire user profile and append it to the binary data
 * @param x - binary data to append to
 * @param s - the ims subscription to encode
 * @returns 1 on succcess or 0 on error
 */
int bin_encode_ims_subscription(bin_data *x, ims_subscription *s)
{
	int i;
	if (!bin_encode_str(x,&(s->private_identity))) goto error;
	if (!bin_encode_ushort(x,s->service_profiles_cnt)) goto error;

	for(i=0;i<s->service_profiles_cnt;i++)
		if (!bin_encode_service_profile(x,s->service_profiles+i)) goto error;
	
	return 1;
error:
	LM_ERR("bin_encode_ims_subscription: Error while encoding.\n");
	return 0;	
}


/**
 *	Decode a binary string from a binary data structure
 * @param x - binary data to decode from
 * @returns the ims_subscription* where the data has been decoded
 */
ims_subscription *bin_decode_ims_subscription(bin_data *x)
{
	ims_subscription *imss=0;
	int i,len;
	str s;
	
	imss = (ims_subscription*) shm_malloc(sizeof(ims_subscription));
	if (!imss) {
		LM_ERR("bin_decode_ims_subscription: Error allocating %lx bytes.\n",sizeof(ims_subscription));
		goto error;
	}
	memset(imss,0,sizeof(ims_subscription));
	
	if (!bin_decode_str(x,&s)||!str_shm_dup(&(imss->private_identity),&s)) goto error;
	if (!bin_decode_ushort(x,	&(imss->service_profiles_cnt))) goto error;
	
	len = sizeof(ims_service_profile)*imss->service_profiles_cnt;
	imss->service_profiles = (ims_service_profile*)shm_malloc(len);
	if (!imss->service_profiles) {
		LM_ERR("bin_decode_ims_subscription: Error allocating %d bytes.\n",len);
		goto error;
	}
	memset(imss->service_profiles,0,len);

	for(i=0;i<imss->service_profiles_cnt;i++)
		if (!bin_decode_service_profile(x,imss->service_profiles+i)) goto error;

	imss->lock = lock_alloc();
	if (imss->lock==0){
		goto error;
	}
	if (lock_init(imss->lock)==0){
		lock_dealloc(imss->lock);
		imss->lock=0;
		goto error;
	}
	imss->ref_count = 1;

	return imss;
error:
	LM_ERR("bin_decode_ims_subscription: Error while decoding (at %d (%04x)).\n",x->max,x->max);
	if (imss) {
		if (imss->private_identity.s) shm_free(imss->private_identity.s);
		if (imss->service_profiles) shm_free(imss->service_profiles);
		shm_free(imss);
	}
	return 0;
}

