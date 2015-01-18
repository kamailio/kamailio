/*
 * Fast 32-bit Header Field Name Parser -- keys
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/** Parser :: Fast 32-bit Header Field Name Parser -- keys .
 * @file
 * @ingroup parser
 */

#ifndef KEYS_H
#define KEYS_H

/*! \name Parser definitions
\verbatim
 * a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z  :  ' ' -
 * 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f 70 71 72 73 74 75 76 77 78 79 7a 3a 20  2d
\endverbatim
 *
 * A quick way to generate the hex string is:
 *  perl -e '$_=lc shift; print "0x".unpack("H*",reverse)."\n";' 4_char_str
 * or to generate the define list:
    perl -e '$s=lc shift; $s.=" "x(4-(length $s)%4);
             print map("#define _". reverse ."_ 0x". (unpack "H*")."\n",
                       reverse unpack("(a[4])*", reverse $s));'  string
 * Note that some editing is still needed (replace spaces and account for ':').
 * Test manually/visually if dword is intended string using:
 * awk '/^#define/ {printf("%s \"%c%c%c%c\" ... %s \n",  $3,strtonum("0x" substr($3,9,2)),strtonum("0x" substr($3,7,2)),strtonum("0x" substr($3,5,2)),strtonum("0x" substr($3,3,2)),$5)}' keys.h
 *
 */

/*!{ */

#define _acce_ 0x65636361   /* "acce" */
#define _allo_ 0x6f6c6c61   /* "allo" */
#define _auth_ 0x68747561   /* "auth" */
#define _oriz_ 0x7a69726f   /* "oriz" */
#define _atio_ 0x6f697461   /* "atio" */
#define _call_ 0x6c6c6163   /* "call" */
#define __id2_ 0x2064692d   /* "-id " */
#define __id1_ 0x3a64692d   /* "-id:" */
#define _cont_ 0x746e6f63   /* "cont" */
#define _act2_ 0x20746361   /* "act " */
#define _act1_ 0x3a746361   /* "act:" */
#define _ent__ 0x2d746e65   /* "ent-" */
#define _leng_ 0x676e656c   /* "leng" */
#define _th12_ 0x203a6874   /* "th: " */
#define _type_ 0x65707974   /* "type" */
#define _cseq_ 0x71657363   /* "cseq" */
#define _expi_ 0x69707865   /* "expi" */
#define _res2_ 0x20736572   /* "res " */
#define _res1_ 0x3a736572   /* "res:" */
#define _from_ 0x6d6f7266   /* "from" */
#define _max__ 0x2d78616d   /* "max-" */
#define _forw_ 0x77726f66   /* "forw" */
#define _ards_ 0x73647261   /* "ards" */
#define _prox_ 0x786f7270   /* "prox" */
#define _y_au_ 0x75612d79   /* "y-au" */
#define _thor_ 0x726f6874   /* "thor" */
#define _izat_ 0x74617a69   /* "izat" */
#define _ion2_ 0x206e6f69   /* "ion " */
#define _ion1_ 0x3a6e6f69   /* "ion:" */
#define _then_ 0x6e656874   /* "then" */
#define _tica_ 0x61636974   /* "tica" */
#define _te1_  0x003a6574   /* "te:" */
#define _te2_  0x00206574   /* "te " */
#define _y_re_ 0x65722d79   /* "y-re" */
#define _quir_ 0x72697571   /* "quir" */
#define _reco_ 0x6f636572   /* "reco" */
#define _rd_r_ 0x722d6472   /* "rd-r" */
#define _oute_ 0x6574756f   /* "oute" */
#define _requ_ 0x75716572   /* "requ" */
#define _ire2_ 0x20657269   /* "ire " */
#define _ire1_ 0x3a657269   /* "ire:" */
#define _rout_ 0x74756f72   /* "rout" */
#define _supp_ 0x70707573   /* "supp" */
#define _orte_ 0x6574726f   /* "orte" */
#define _to12_ 0x203a6f74   /* "to: " */
#define _unsu_ 0x75736e75   /* "unsu" */
#define _ppor_ 0x726f7070   /* "ppor" */
#define _ted2_ 0x20646574   /* "ted " */
#define _ted1_ 0x3a646574   /* "ted:" */
#define _via2_ 0x20616976   /* "via " */
#define _via1_ 0x3a616976   /* "via:" */
#define _www__ 0x2d777777   /* "www-" */
#define _enti_ 0x69746e65   /* "enti" */
#define _cate_ 0x65746163   /* "cate" */
#define _even_ 0x6e657665   /* "even" */
#define _sip_  0x2d706973   /* "sip-" */
#define _ifm_  0x6d2d6669   /* "if-m" */
#define _atch_ 0x68637461   /* "atch" */
#define _date_ 0x65746164   /* "date" */
#define _iden_ 0x6e656469   /* "iden" */
#define _tity_ 0x79746974   /* "tity" */
#define _info_ 0x6f666e69   /* "info" */
#define _path_ 0x68746170   /* "path" */
#define _100r_ 0x72303031   /* "100r" */
#define _time_ 0x656d6974   /* "time" */

#define _pt_l_ 0x6c2d7470   /* "pt-l" */
#define _angu_ 0x75676e61   /* "angu" */
#define _age2_ 0x20656761   /* "age " */
#define _age1_ 0x3a656761   /* "age:" */
#define _orga_ 0x6167726f   /* "orga" */
#define _niza_ 0x617a696e   /* "niza" */
#define _tion_ 0x6e6f6974   /* "tion" */
#define _prio_ 0x6f697270   /* "prio" */
#define _rity_ 0x79746972   /* "rity" */
#define _subj_ 0x6a627573   /* "subj" */
#define _subs_ 0x73627573   /* "subs" */
#define _crip_ 0x70697263   /* "crip" */
#define __sta_ 0x6174732d   /* "-sta" */
#define _ect2_ 0x20746365   /* "ect " */
#define _ect1_ 0x3a746365   /* "ect:" */
#define _user_ 0x72657375   /* "user" */
#define __age_ 0x6567612d   /* "-age" */
#define _disp_ 0x70736964   /* "disp" */
#define _osit_ 0x7469736f   /* "osit" */
#define _ion2_ 0x206e6f69   /* "ion " */
#define _ion1_ 0x3a6e6f69   /* "ion:" */

#define _pt_d_ 0x642d7470   /* "pt-d" */
#define _ispo_ 0x6f707369   /* "ispo" */
#define _siti_ 0x69746973   /* "siti" */

#define _serv_ 0x76726573   /* "serv" */

#define _dive_ 0x65766964   /* "dive" */
#define _rsio_ 0x6f697372   /* "rsio" */

#define _remo_ 0x6f6d6572   /* "remo" */
#define _te_p_ 0x702d6574   /* "te-p" */
#define _arty_ 0x79747261   /* "arty" */
#define __id2_ 0x2064692d   /* "-id " */
#define __id1_ 0x3a64692d   /* "-id:" */

#define _refe_ 0x65666572   /* "refe" */
#define _r_to_ 0x6f742d72   /* "r-to" */

#define _pt_c_ 0x632d7470   /* "pt-c" */
#define _onta_ 0x61746e6f   /* "onta" */

#define _sess_ 0x73736573   /* "sess" */
#define _ion__ 0x2d6e6f69   /* "ion-" */
#define _res2_ 0x20736572   /* "res " */
#define _res1_ 0x3a736572   /* "res:" */

#define _w_ev_ 0x76652d77   /* "w-ev" */
#define _ents_ 0x73746e65   /* "ents" */

#define _enco_ 0x6f636e65   /* "enco" */
#define _ding_ 0x676e6964   /* "ding" */

#define _rred_ 0x64657272   /* "rred" */
#define __by2_ 0x2079622d   /* "-by " */
#define __by1_ 0x3a79622d   /* "-by:" */

#define _reje_ 0x656a6572   /* "reje" */
#define _ct_c_ 0x632d7463   /* "ct-c" */

#define _est__ 0x2d747365   /* "est-" */

#define _min__ 0x2d6e696d   /* "min-" */


#define _retr_ 0x72746572   /* "retr" */
#define _y_af_ 0x66612d79   /* "y-af" */
#define _ter1_ 0x3a726574   /* "ter:" */
#define _ter2_ 0x20726574   /* "ter " */

#define _priv_ 0x76697270   /* "priv" */
#define _acy2_ 0x20796361   /* "acy " */
#define _acy1_ 0x3a796361   /* "acy:" */

#define _reas_ 0x73616572  /* "reas" */
#define _on1_ 0x203a6e6f  /* "on: " */
#define _on2_ 0x3a206e6f  /* "on :" */
#define _on3_ 0x20206e6f  /* "on  " */

#define _p_as_ 0x73612d70 /* "p-as" */
#define _sert_ 0x74726573 /* "sert" */
#define _ed_i_ 0x692d6465 /* "ed-i" */
#define _dent_ 0x746e6564 /* "dent" */
#define _ity1_ 0x3a797469 /* "ity:" */
#define _ity2_ 0x20797469 /* "ity " */

#define _p_pr_ 0x72702d70 /* "p-pr" */
#define _efer_ 0x72656665 /* "efer" */
#define _red__ 0x2d646572 /* "red-" */
/* _iden_ is already defined */
/* _tity_ is already defined */

/*!} */

#endif /* KEYS_H */
