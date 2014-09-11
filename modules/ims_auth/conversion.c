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
 
/**
 * \file
 * 
 * Serving-CSCF - Conversion between base16, base64 and base256
 * 
 *  \author Dragos Vingarzan vingarzan -at- fokus dot fraunhofer dot de
 * 
 */
 
/** base16 char constants */ 
char *hexchars="0123456789abcdef";
/**
 * Converts a binary encoded value to its base16 representation.
 * @param from - buffer containing the input data
 * @param len - the size of from
 * @param to - the output buffer  !!! must have at least len*2 allocated memory 
 * @returns the written length 
 */
int bin_to_base16(char *from,int len, char *to)
{
	int i,j;
	for(i=0,j=0;i<len;i++,j+=2){
		to[j] = hexchars[(((unsigned char)from[i]) >>4 )&0x0F];
		to[j+1] = hexchars[(((unsigned char)from[i]))&0x0F];
	}	
	return 2*len;
}

/** from base16 char to int */
#define HEX_DIGIT(x) \
	((x>='0'&&x<='9')?x-'0':((x>='a'&&x<='f')?x-'a'+10:((x>='A'&&x<='F')?x-'A'+10:0)))
/**
 * Converts a hex encoded value to its binary value
 * @param from - buffer containing the input data
 * @param len - the size of from
 * @param to - the output buffer  !!! must have at least len/2 allocated memory 
 * @returns the written length 
 */
int base16_to_bin(char *from,int len, char *to)
{
	int i,j;
	for(i=0,j=0;j<len;i++,j+=2){
		to[i] = (unsigned char) ( HEX_DIGIT(from[j])<<4 | HEX_DIGIT(from[j+1]));
	}	
	return i;
}

/**
 * Convert on character from base64 encoding to integer.
 *"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 * @param x - characted to convert
 * @returns the int value or -1 if terminal character ('=')
 */ 
static inline int base64_val(char x)\
{
	switch(x){
		case '=': return -1;
		case 'A': return 0;
		case 'B': return 1;
		case 'C': return 2;
		case 'D': return 3;
		case 'E': return 4;
		case 'F': return 5;
		case 'G': return 6;
		case 'H': return 7;
		case 'I': return 8;
		case 'J': return 9;
		case 'K': return 10;
		case 'L': return 11;
		case 'M': return 12;
		case 'N': return 13;
		case 'O': return 14;
		case 'P': return 15;
		case 'Q': return 16;
		case 'R': return 17;
		case 'S': return 18;
		case 'T': return 19;
		case 'U': return 20;
		case 'V': return 21;
		case 'W': return 22;
		case 'X': return 23;
		case 'Y': return 24;
		case 'Z': return 25;
		case 'a': return 26;
		case 'b': return 27;
		case 'c': return 28;
		case 'd': return 29;
		case 'e': return 30;
		case 'f': return 31;
		case 'g': return 32;
		case 'h': return 33;
		case 'i': return 34;
		case 'j': return 35;
		case 'k': return 36;
		case 'l': return 37;
		case 'm': return 38;
		case 'n': return 39;
		case 'o': return 40;
		case 'p': return 41;
		case 'q': return 42;
		case 'r': return 43;
		case 's': return 44;
		case 't': return 45;
		case 'u': return 46;
		case 'v': return 47;
		case 'w': return 48;
		case 'x': return 49;
		case 'y': return 50;
		case 'z': return 51;
		case '0': return 52;
		case '1': return 53;
		case '2': return 54;
		case '3': return 55;
		case '4': return 56;
		case '5': return 57;
		case '6': return 58;
		case '7': return 59;
		case '8': return 60;
		case '9': return 61;
		case '+': return 62;
		case '/': return 63;
	}
	return 0;
}

/**
 * Convert a string encoded in base64 to binary value.
 * @param from - buffer containing the input data
 * @param from_len - the size of from
 * @param to - the output buffer  !!! must have at least len*2 allocated memory 
 * @returns the written length 
 */
int base64_to_bin(char *from,int from_len, char *to)
{
	int i,j,x1,x2,x3,x4;

	for(i=0,j=0;i<from_len;i+=4){
		x1=base64_val(from[i]);
		x2=base64_val(from[i+1]);
		x3=base64_val(from[i+2]);
		x4=base64_val(from[i+3]);
		to[j++]=(x1<<2) | ((x2 & 0x30)>>4);
		if (x3==-1) break;
		to[j++]=((x2 & 0x0F)<<4) | ((x3 & 0x3C)>>2);
		if (x4==-1) break;
		to[j++]=((x3 & 0x03)<<6) | (x4 & 0x3F);
	}
	return j;
}

/** base64 characters constant */
char base64[64]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/**
 * Convert a binary string to base64 encoding.
 * @param src - the source buffer
 * @param src_len - length of the source buffer
 * @param ptr - the destination buffer - must be allocated to at least src_len/3*4+4 
 * @returns the length of the resulted buffer 
 */
int bin_to_base64(char *src,int src_len,char* ptr)
{
	int i,k;
	int triplets,rest;
	char *s;
	s=ptr;

	triplets = src_len/3;
	rest = src_len%3;
	for(i=0;i<triplets*3;i+=3){
		k = (((unsigned char) src[i])&0xFC)>>2;
		*ptr=base64[k];ptr++;

		k = (((unsigned char) src[i])&0x03)<<4;
		k |=(((unsigned char) src[i+1])&0xF0)>>4;
		*ptr=base64[k];ptr++;

		k = (((unsigned char) src[i+1])&0x0F)<<2;
		k |=(((unsigned char) src[i+2])&0xC0)>>6;
		*ptr=base64[k];ptr++;

		k = (((unsigned char) src[i+2])&0x3F);
		*ptr=base64[k];ptr++;
	}
	i=triplets*3;
	switch(rest){
		case 0:
			break;
		case 1:
			k = (((unsigned char) src[i])&0xFC)>>2;
			*ptr=base64[k];ptr++;

			k = (((unsigned char) src[i])&0x03)<<4;
			*ptr=base64[k];ptr++;

			*ptr='=';ptr++;

			*ptr='=';ptr++;
			break;
		case 2:
			k = (((unsigned char) src[i])&0xFC)>>2;
			*ptr=base64[k];ptr++;

			k = (((unsigned char) src[i])&0x03)<<4;
			k |=(((unsigned char) src[i+1])&0xF0)>>4;
			*ptr=base64[k];ptr++;

			k = (((unsigned char) src[i+1])&0x0F)<<2;
			*ptr=base64[k];ptr++;

			*ptr='=';ptr++;
			break;
	}

	return ptr-s;
}

