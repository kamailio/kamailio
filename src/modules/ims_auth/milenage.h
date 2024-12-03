/*-------------------------------------------------------------------
 *          Example algorithms f1, f1*, f2, f3, f4, f5, f5*
 *-------------------------------------------------------------------
 *
 *  A sample implementation of the example 3GPP authentication and
 *  key agreement functions f1, f1*, f2, f3, f4, f5 and f5*.  This is
 *  a byte-oriented implementation of the functions, and of the block
 *  cipher kernel function Rijndael.
 *
 *  This has been coded for clarity, not necessarily for efficiency.
 *
 *  The functions f2, f3, f4 and f5 share the same inputs and have
 *  been coded together as a single function.  f1, f1* and f5* are
 *  all coded separately.
 *
 *-----------------------------------------------------------------*/

#ifndef MILENAGE_H
#define MILENAGE_H

#include <stdint.h>

void ComputeOPc(uint8_t op_c[16], uint8_t k[16], uint8_t op[16]);

void f0(uint8_t rand[16]);

void f1(uint8_t mac_a[8], uint8_t k[16], uint8_t rand[16], uint8_t sqn[6],
		uint8_t amf[2], uint8_t op_c[16]);
void f2345(uint8_t res[8], uint8_t ck[16], uint8_t ik[16], uint8_t ak[6],
		uint8_t k[16], uint8_t op_c[16], uint8_t rand[16]);

void f1star(uint8_t mac_s[8], uint8_t k[16], uint8_t op_c[16], uint8_t rand[16],
		uint8_t sqn[6], uint8_t amf[2]);
void f5star(uint8_t ak[6], uint8_t k[16], uint8_t op_c[16], uint8_t rand[16]);


#endif