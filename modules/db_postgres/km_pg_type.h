/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2008 1&1 Internet AG
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
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */

/*
 * OID definitions, copied from postgresql/catalog/pg_types.h.
 * It would be probably more correct to use the definitions from there.
 */
#define BOOLOID			16
#define BYTEAOID		17
#define CHAROID			18
#define NAMEOID			19
#define INT8OID			20
#define INT2OID			21
#define INT2VECTOROID		22
#define INT4OID			23
#define REGPROCOID		24
#define TEXTOID			25
#define OIDOID			26
#define TIDOID			27
#define XIDOID 			28
#define CIDOID 			29
#define OIDVECTOROID		30
#define POINTOID		600
#define LSEGOID			601
#define PATHOID			602
#define BOXOID			603
#define POLYGONOID		604
#define LINEOID			628
#define FLOAT4OID 		700
#define FLOAT8OID 		701
#define ABSTIMEOID		702
#define RELTIMEOID		703
#define TINTERVALOID		704
#define UNKNOWNOID		705
#define CIRCLEOID		718
#define CASHOID 		790
#define MACADDROID 		829
#define INETOID 		869
#define CIDROID 		650
#define ACLITEMOID		1033
#define BPCHAROID		1042
#define VARCHAROID		1043
#define DATEOID			1082
#define TIMEOID			1083
#define TIMESTAMPOID		1114
#define TIMESTAMPTZOID		1184
#define INTERVALOID		1186
#define TIMETZOID		1266
#define BITOID	 		1560
#define VARBITOID	  	1562
#define NUMERICOID		1700
#define REFCURSOROID		1790
#define REGPROCEDUREOID 	2202
#define REGOPEROID		2203
#define REGOPERATOROID		2204
#define REGCLASSOID		2205
#define REGTYPEOID		2206
#define RECORDOID		2249
#define CSTRINGOID		2275
#define ANYOID			2276
#define ANYARRAYOID		2277
#define VOIDOID			2278
#define TRIGGEROID		2279
#define LANGUAGE_HANDLEROID	2280
#define INTERNALOID		2281
#define OPAQUEOID		2282
