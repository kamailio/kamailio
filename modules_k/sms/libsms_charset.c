/*
SMS Server Tools
Copyright (C) 2000 Stefan Frings

This program is free software unless you got it under another license directly
from the author. You can redistribute it and/or modify it under the terms of 
the GNU General Public License as published by the Free Software Foundation.
Either version 2 of the License, or (at your option) any later version.

http://www.isis.de/members/~s.frings
mailto:s.frings@mail.isis.de
 */


#include "libsms_charset.h"

#define noc 183 
// non existent character

// iso 8859-1
unsigned char charset[128] ={
		     '@' , 163 , '$' , 165 , 232 , 233 , 249 , 236 ,
		     242 , 199 ,  10 , 216 , 248 ,  13 , 197 , 229 ,
		     noc , '_' , noc , noc , noc , noc , noc , noc ,
		     noc , noc , noc , noc , 198 , 230 , 223 , 201 ,
		     ' ' , '!' ,  34 , '#' , '$' , '%' , '&' ,  39 ,
		     '(' , ')' , '*' , '+' , ',' , '-' , '.' , '/' ,
		     '0' , '1' , '2' , '3' , '4' , '5' , '6' , '7' ,
		     '8' , '9' , ':' , ';' , '<' , '=' , '>' , '?' ,
		     161 , 'A' , 'B' , 'C' , 'D' , 'E' , 'F' , 'G' ,
		     'H' , 'I' , 'J' , 'K' , 'L' , 'M' , 'N' , 'O' ,
		     'P' , 'Q' , 'R' , 'S' , 'T' , 'U' , 'V' , 'W' ,
		     'X' , 'Y' , 'Z' , 196 , 214 , 209 , 220 , 167 ,
		     191 , 'a' , 'b' , 'c' , 'd' , 'e' , 'f' , 'g' ,
		     'h' , 'i' , 'j' , 'k' , 'l' , 'm' , 'n' , 'o' ,
		     'p' , 'q' , 'r' , 's' , 't' , 'u' , 'v' , 'w' ,
		     'x' , 'y' , 'z' , 228 , 246 , 241 , 252 , 224 };
		      
char ascii2sms(const char c)
{
	char found='*'; // replacement for nonexistent characters
	int i;

	for (i=0; i<128 ; i++)
		if (c==charset[i])
		{
			found=i;
			break;
		}

	return found;
}




char sms2ascii(const char c)
{
	return charset[(int)c];
}
