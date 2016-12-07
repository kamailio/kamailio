/*
 * base64 encoding / decoding
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

void base64decode(char* src_buf, int src_len, char* tgt_buf, int* tgt_len) {
	int pos, i, n;
	unsigned char c[4];
	for (pos=0, i=0, *tgt_len=0; pos < src_len; pos++) {
		if (src_buf[pos] >= 'A' && src_buf[pos] <= 'Z')
			c[i] = src_buf[pos] - 65;   /* <65..90>  --> <0..25> */
		else if (src_buf[pos] >= 'a' && src_buf[pos] <= 'z')
			c[i] = src_buf[pos] - 71;   /* <97..122>  --> <26..51> */
		else if (src_buf[pos] >= '0' && src_buf[pos] <= '9')
			c[i] = src_buf[pos] + 4;    /* <48..56>  --> <52..61> */
		else if (src_buf[pos] == '+')
			c[i] = 62;
		else if (src_buf[pos] == '/')
			c[i] = 63;
		else  /* '=' */
			c[i] = 64;
		i++;
		if (pos == src_len-1) {
			while (i < 4) {
				c[i] = 64;
				i++;
			}
		}
		if (i==4) {
			if (c[0] == 64)
				n = 0;
			else if (c[2] == 64)
				n = 1;
			else if (c[3] == 64)
				n = 2;
			else
				n = 3;
			switch (n) {
				case 3:
					tgt_buf[*tgt_len+2] = (char) (((c[2] & 0x03) << 6) | c[3]);
					/* no break */
				case 2:
					tgt_buf[*tgt_len+1] = (char) (((c[1] & 0x0F) << 4) | (c[2] >> 2));
					/* no break */
				case 1:
					tgt_buf[*tgt_len+0] = (char) ((c[0] << 2) | (c[1] >> 4));
					break;
			}
			i=0;
			*tgt_len+= n;
		}
	}
}

void base64encode(char* src_buf, int src_len, char* tgt_buf, int* tgt_len, int quoted) {
	static char code64[64+1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int pos;
	for (pos=0, *tgt_len=0; pos < src_len; pos+=3,*tgt_len+=4) {
		tgt_buf[*tgt_len+0] = code64[(unsigned char)src_buf[pos+0] >> 2];
		tgt_buf[*tgt_len+1] = code64[(((unsigned char)src_buf[pos+0] & 0x03) << 4) | ((pos+1 < src_len)?((unsigned char)src_buf[pos+1] >> 4):0)];
		if (pos+1 < src_len)
			tgt_buf[*tgt_len+2] = code64[(((unsigned char)src_buf[pos+1] & 0x0F) << 2) | ((pos+2 < src_len)?((unsigned char)src_buf[pos+2] >> 6):0)];
		else {
			if(!quoted)
				(*tgt_len)--;
			else
				/* this data is going to be quoted */
				tgt_buf[*tgt_len+2] = '='; 
		}
		if (pos+2 < src_len)
			tgt_buf[*tgt_len+3] = code64[(unsigned char)src_buf[pos+2] & 0x3F];
		else {
			if(!quoted)
				(*tgt_len)--;
			else
				tgt_buf[*tgt_len+3] = '=';
		}
	}
}
