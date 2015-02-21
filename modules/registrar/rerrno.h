/*
 * Registrar errno
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
 */

/*!
 * \file
 * \brief SIP registrar module - registrar errno
 * \ingroup registrar   
 */  


#ifndef RERRNO_H
#define RERRNO_H


typedef enum rerr {
	R_FINE = 0,   /*!< Everything went OK */
	R_UL_DEL_R,   /*!< Usrloc record delete failed */
	R_UL_GET_R,   /*!< Usrloc record get failed */
	R_UL_NEW_R,   /*!< Usrloc new record failed */
	R_INV_CSEQ,   /*!< Invalid CSeq value */
	R_UL_INS_C,   /*!< Usrloc insert contact failed */
	R_UL_INS_R,   /*!< Usrloc insert record failed */
	R_UL_DEL_C,   /*!< Usrloc contact delete failed */
	R_UL_UPD_C,   /*!< Usrloc contact update failed */
	R_TO_USER,    /*!< No username part in To URI */
	R_AOR_LEN,    /*!< Address Of Record too long */
	R_AOR_PARSE,  /*!< Error while parsing Address Of Record */
	R_INV_EXP,    /*!< Invalid expires parameter in contact */
	R_INV_Q,      /*!< Invalid q parameter in contact */
	R_PARSE,      /*!< Error while parsing message */
	R_TO_MISS,    /*!< Missing To header field */
	R_CID_MISS,   /*!< Missing Call-ID header field */
	R_CS_MISS,    /*!< Missing CSeq header field */
	R_PARSE_EXP,  /*!< Error while parsing Expires */
	R_PARSE_CONT, /*!< Error while parsing Contact */
	R_STAR_EXP,   /*!< star and expires != 0 */
	R_STAR_CONT,  /*!< star and more contacts */
	R_OOO,        /*!< Out-Of-Order request */
	R_RETRANS,    /*!< Request is retransmission */
	R_UNESCAPE,   /*!< Error while unescaping username */
	R_TOO_MANY,   /*!< Too many contacts */
	R_CONTACT_LEN,/*!< Contact URI or RECEIVED too long */
	R_CALLID_LEN, /*!< Callid too long */
	R_PARSE_PATH, /*!< Error while parsing Path */
	R_PATH_UNSUP, /*!< Path not supported by UAC */
	R_OB_UNSUP,   /*!< Outbound not supported by UAC */
	R_OB_REQD,    /*!< Outbound required by UAC but not supported on server */
	R_OB_UNSUP_EDGE, /*!< Outbound needed for this registration but not supported on edge proxy */

} rerr_t;


extern rerr_t rerrno;


#endif /* RERRNO_H */
