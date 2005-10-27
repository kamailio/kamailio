/*-------------------------------------------------------------------------
 *
 * pg_type.h
 *	  definition of the system "type" relation (pg_type)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2005, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_type.h,v 1.158 2004/12/31 22:03:26 pgsql Exp $
 *
 * NOTES
 *	  the genbki.sh script reads this file and generates .bki
 *	  information from the DATA() statements.
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PG_TYPE_H
#define _PG_TYPE_H

/* OIDS 1 - 99 */

#define BOOLOID	         16  /* boolean, 'true'/'false' */
#define BYTEAOID         17  /* variable-length string, binary values escaped" */
#define CHAROID	         18  /* single character */
#define NAMEOID	         19  /* 63-character type for storing system identifiers */
#define INT8OID	         20  /* ~18 digit integer, 8-byte storage */
#define INT2OID	         21  /* -32 thousand to 32 thousand, 2-byte storage */
#define INT2VECTOROID    22  /* array of INDEX_MAX_KEYS int2 integers, used in system tables */
#define INT4OID	         23  /* -2 billion to 2 billion integer, 4-byte storage */
#define REGPROCOID       24  /* registered procedure */
#define TEXTOID	         25  /* variable-length string, no limit specified */
#define OIDOID	         26  /* object identifier(oid), maximum 4 billion */
#define TIDOID	         27  /* (Block, offset), physical location of tuple */
#define XIDOID           28  /* transaction id */
#define CIDOID           29  /* command identifier type, sequence in transaction id */
#define OIDVECTOROID     30  /* array of INDEX_MAX_KEYS oids, used in system tables */

/* OIDS 700 - 799 */

#define FLOAT4OID       700  /* single-precision floating point number, 4-byte storage */
#define FLOAT8OID       701  /* double-precision floating point number, 8-byte storage */
#define ABSTIMEOID      702  /* absolute, limited-range date and time (Unix system time) */
#define RELTIMEOID      703  /* relative, limited-range time interval (Unix delta time) */
#define TINTERVALOID    704  /* (abstime,abstime), time interval */
#define UNKNOWNOID      705
#define CIRCLEOID       718  /* geometric circle '(center,radius)' */
#define CASHOID         790  /* monetary amounts, $d,ddd.cc */

/* OIDS 800 - 899 */

#define MACADDROID      829  /* XX:XX:XX:XX:XX:XX, MAC address */
#define INETOID         869  /* IP address/netmask, host address, netmask optional */
#define CIDROID         650  /* network IP address/netmask, network address */

/* OIDS 1000 - 1099 */

#define BPCHAROID      1042  /* char(length), blank-padded string, fixed storage length */
#define VARCHAROID     1043  /* varchar(length), non-blank-padded string, variable storage length */
#define DATEOID	       1082  /* ANSI SQL date */
#define TIMEOID	       1083  /* hh:mm:ss, ANSI SQL time */

/* OIDS 1100 - 1199 */

#define TIMESTAMPOID   1114  /* date and time */
#define TIMESTAMPTZOID 1184  /* date and time with time zone */
#define INTERVALOID    1186  /* @ <number> <units>, time interval */

/* OIDS 1200 - 1299 */

#define TIMETZOID      1266  /* hh:mm:ss, ANSI SQL time */

/* OIDS 1500 - 1599 */

#define BITOID	       1560  /* fixed-length bit string */
#define VARBITOID      1562  /* variable-length bit string */

/* OIDS 1700 - 1799 */

#define NUMERICOID     1700  /* numeric(precision, decimal), arbitrary precision number */

#endif /* _PG_TYPE_H */
