/***
*
*	Rocket Crowbar 1.9: mdhash.cpp, RIPEMD-160 (Bosselaers' reference rmd160.c and hashtest.c's
*	RMD()). wog.cpp's WOGCommand hashes its password argument with RMD.
*
*	The original's words are unsigned long, 32 bits on i386; uint32_t keeps the hash right on
*	64-bit builds.
*
****/

#pragma once

#include <stdint.h>

void MDinit(uint32_t* MDbuf);
void compress(uint32_t* MDbuf, uint32_t* X);
void MDfinish(uint32_t* MDbuf, unsigned char* strptr, uint32_t lswlen, uint32_t mswlen);
unsigned char* RMD(unsigned char* message); // 20-byte digest in a static buffer
