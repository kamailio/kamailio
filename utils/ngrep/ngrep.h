/*
 * $Id$
 *
 * Copyright (c) 2001  Jordan Ritter <jpr5@darkridge.com>
 *
 * Please refer to the COPYRIGHT file for more information. 
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#define VERSION "1.40.1"


#define ETHHDR_SIZE 14 
#define TOKENRING_SIZE 22
#define PPPHDR_SIZE 4 
#define SLIPHDR_SIZE 16
#define RAWHDR_SIZE 0
#define LOOPHDR_SIZE 4 
#define FDDIHDR_SIZE 21
#define ISDNHDR_SIZE 16

#ifndef IP_OFFMASK
#define IP_OFFMASK 0x1fff
#endif

#define WORD_REGEX "((^%s\\W)|(\\W%s$)|(\\W%s\\W))"
#define IP_ONLY "ip and ( %s)"


char *get_filter(char **);
void process(u_char *, struct pcap_pkthdr*, u_char *);
void _dump(char *, int); 
void dump_line_by_line(char *, int); 
void clean_exit(int);
void usage(int);
void version(void);

int re_match_func(char *, int); 
int bin_match_func(char *, int);
int blank_match_func(char *, int); 

int strishex(char *);

void print_time_absolute(struct pcap_pkthdr *);
void print_time_diff_init(struct pcap_pkthdr *);
void print_time_diff(struct pcap_pkthdr *);

void dump_delay_proc_init(struct pcap_pkthdr *);
void dump_delay_proc(struct pcap_pkthdr *);

void update_windowsize(int);
