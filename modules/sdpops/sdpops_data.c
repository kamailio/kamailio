/*
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../dprint.h"
#include "sdpops_data.h"

#if 0

http://www.iana.org/assignments/rtp-parameters

Registry Name: RTP Payload types (PT) for standard audio and video encodings - Closed
Reference: [RFC3551]
Registration Procedures: Registry closed; see [RFC3551], Section 3

Note:
The RFC "RTP Profile for Audio and Video Conferences with Minimal
Control" [RFC3551] specifies an initial set "payload types".  This
list maintains and extends that list.

Registry:
PT        encoding name   audio/video (A/V)  clock rate (Hz)  channels (audio)  Reference
--------  --------------  -----------------  ---------------  ----------------  ---------
0         PCMU            A                  8000             1                 [RFC3551]
1         Reserved	  
2         Reserved
3         GSM             A                  8000             1                 [RFC3551]
4         G723            A                  8000             1                 [Kumar][RFC3551]
5         DVI4            A                  8000             1                 [RFC3551]
6         DVI4            A                  16000            1                 [RFC3551]
7         LPC             A                  8000             1                 [RFC3551]
8         PCMA            A                  8000             1                 [RFC3551]
9         G722            A                  8000             1                 [RFC3551]
10        L16             A                  44100            2                 [RFC3551]
11        L16             A                  44100            1                 [RFC3551]
12        QCELP           A                  8000             1                 [RFC3551]
13        CN              A                  8000             1                 [RFC3389]
14        MPA             A                  90000                              [RFC3551][RFC2250]
15        G728            A                  8000             1                 [RFC3551]
16        DVI4            A                  11025            1                 [DiPol]
17        DVI4            A                  22050            1                 [DiPol]
18        G729            A                  8000             1                 [RFC3551]
19        Reserved        A
20        Unassigned      A
21        Unassigned      A
22        Unassigned      A
23        Unassigned      A
24        Unassigned      V
25        CelB            V                  90000                              [RFC2029]
26        JPEG            V                  90000                              [RFC2435]
27        Unassigned      V
28        nv              V                  90000                              [RFC3551]
29        Unassigned      V
30        Unassigned      V
31        H261            V                  90000                              [RFC4587]
32        MPV             V                  90000                              [RFC2250]
33        MP2T            AV                 90000                              [RFC2250]
34        H263            V                  90000                              [Zhu]
35-71     Unassigned      ?
72-76     Reserved for RTCP conflict avoidance                                  [RFC3551]
77-95     Unassigned      ?
96-127    dynamic         ?                                                     [RFC3551] 


Registry Name: RTP Payload Format media types
Reference: [RFC4855]
Registration Procedures: Standards Action Process or expert approval

#endif

int sdpops_get_id_by_name(str *name, str *id)
{
	return 0;
}
