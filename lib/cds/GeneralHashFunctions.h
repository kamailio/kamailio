/*
 **************************************************************************
 *                                                                        *
 *          General Purpose Hash Function Algorithms Library              *
 *                                                                        *
 * Author: Arash Partow - 2002                                            *
 * URL: http://www.partow.net                                             *
 * URL: http://www.partow.net/programming/hashfunctions/index.html        *
 *                                                                        *
 * Copyright notice:                                                      *
 * Free use of the General Purpose Hash Function Algorithms Library is    *
 * permitted under the guidelines and in accordance with the most current *
 * version of the Common Public License.                                  *
 * http://www.opensource.org/licenses/cpl.php                             *
 *                                                                        *
 **************************************************************************
*/



#ifndef INCLUDE_GENERALHASHFUNCTION_C_H
#define INCLUDE_GENERALHASHFUNCTION_C_H


#include <stdio.h>


typedef unsigned int (*hash_function)(const char*, unsigned int len);


unsigned int RSHash  (const char* str, unsigned int len);
unsigned int JSHash  (const char* str, unsigned int len);
unsigned int PJWHash (const char* str, unsigned int len);
unsigned int ELFHash (const char* str, unsigned int len);
unsigned int BKDRHash(const char* str, unsigned int len);
unsigned int SDBMHash(const char* str, unsigned int len);
unsigned int DJBHash (const char* str, unsigned int len);
unsigned int DEKHash (const char* str, unsigned int len);
unsigned int APHash  (const char* str, unsigned int len);


#endif
