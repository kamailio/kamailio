/*
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

#ifndef str_h
#define str_h

/** @defgroup str_string Counted-Length Strings 
 * @{
 * 
 * Implementation of counted-length strings. In SER and its modules, strings
 * are often stored in the ::str structure. In addition to the pointer
 * pointing to the first character of the string, the structure also contains
 * the length of the string.
 * 
 * @section motivation Motivation
 * Storing the length of the string together with the pointer to the string
 * has two advantages. First, it makes many string operations faster because
 * it is not necessary to count the number of characters at runtime. Second,
 * the pointer can point to arbitrary substrings within a SIP message (which
 * itself is stored as one long string spanning the whole message) without the
 * need to make a zero-terminated copy of it. 
 *
 * @section drawbacks Drawbacks 
 * Note well that the fact that string stored
 * using this data structure are not zero terminated makes them a little
 * incovenient to use with many standard libc string functions, because these
 * usually expect the input to be zero-terminated. In this case you have to
 * either make a zero-terminated copy or inject the terminating zero behind
 * the actuall string (if possible). Note that injecting a zero terminating
 * characters is considered to be dangerous.
 */

/** @file 
 * This header field defines the ::str data structure that is used across
 * SER sources to store counted-length strings. The file also defines several
 * convenience macros.
 */

/** Data structure used across SER sources to store counted-length strings.
 * This is the data structure that is used to store counted-length
 * strings in SER core and modules.
 */
struct _str{
	char* s; /**< Pointer to the first character of the string */
	int len; /**< Length of the string */
};


/** Data structure used across SER sources to store counted-length strings.
 * @see _str
 */
typedef struct _str str;

/** Initializes static ::str string with string literal.
 * This is a convenience macro that can be used to initialize
 * static ::str strings with string literals like this:
 * \code static str var = STR_STATIC_INIT("some_string"); \endcode
 * @param v is a string literal
 * @sa STR_NULL
 */
#define STR_STATIC_INIT(v) {(v), sizeof(v) - 1}

/* kamailio compatibility macro (same thing as above) */
#define str_init(v) STR_STATIC_INIT(v)

/** Initializes ::str string with NULL pointer and zero length.
 * This is a convenience macro that can be used to initialize
 * ::str string variable to NULL string with zero length:
 * \code str var = STR_NULL; \endcode
 * @sa STR_STATIC_INIT
 */
#define STR_NULL {0, 0}

/** Formats ::str string for use in printf-like functions.
 * This is a macro that prepares a ::str string for use in functions which 
 * use printf-like formatting strings. This macro is necessary  because 
 * ::str strings do not have to be zero-terminated and thus it is necessary 
 * to provide printf-like functions with the number of characters in the 
 * string manually. Here is an example how to use the macro: 
 * \code printf("%.*s\n", STR_FMT(var));\endcode Note well that the correct 
 * sequence in the formatting string is %.*, see the man page of printf for 
 * more details.
 */
#define STR_FMT(_pstr_)	\
  ((_pstr_ != (str *)0) ? (_pstr_)->len : 0), \
  ((_pstr_ != (str *)0) ? (_pstr_)->s : "")


/** Compares two ::str strings.
 * This macro implements comparison of two strings represented using ::str 
 * structures. First it compares the lengths of both string and if and only 
 * if they are same then both strings are compared using memcmp.
 * @param x is first string to be compared
 * @param y is second string to be compared
 * @return 1 if strings are same, 0 otherwise
 */
#define STR_EQ(x,y) (((x).len == (y).len) && \
					 (memcmp((x).s, (y).s, (x).len) == 0))

/** @} */

/** Appends a sufffix
 * @param orig is the original string
 * @param suffix is the suffix string
 * @param dest is the result ::str of appending suffix to orig
 * @return 0 if ok -1 if error
 * remember to free the dest->s private memory
 */
int str_append(str *orig, str *suffix, str *dest);

#endif
