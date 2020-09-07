/*
** m_oldrandom.cpp
** Contains Doom's original random number generator.
**
**---------------------------------------------------------------------------
** Copyright 1993-1996 by id Software, Inc.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

static const char rcsid[] = "$Id: m_random.c,v 1.1 1997/02/03 22:45:11 b1 Exp $";


//
// M_Random
// Returns a 0-255 number
//
unsigned char rndtable[256] = {
	0,	 8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,	66 ,
	74,  21, 211,  47,	80, 242, 154,  27, 205, 128, 161,  89,	77,  36 ,
	95, 110,  85,  48, 212, 140, 211, 249,	22,  79, 200,  50,	28, 188 ,
	52, 140, 202, 120,	68, 145,  62,  70, 184, 190,  91, 197, 152, 224 ,
	149, 104,  25, 178, 252, 182, 202, 182, 141, 197,	4,	81, 181, 242 ,
	145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0 ,
	175, 143,  70, 239,  46, 246, 163,	53, 163, 109, 168, 135,   2, 235 ,
	25,  92,  20, 145, 138,  77,  69, 166,	78, 176, 173, 212, 166, 113 ,
	94, 161,  41,  50, 239,  49, 111, 164,	70,  60,   2,  37, 171,  75 ,
	136, 156,  11,	56,  42, 146, 138, 229,  73, 146,  77,	61,  98, 196 ,
	135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113 ,
	80, 250, 108,	7, 255, 237, 129, 226,	79, 107, 112, 166, 103, 241 ,
	24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224 ,
	145, 224,  81, 206, 163,  45,  63,	90, 168, 114,  59,	33, 159,  95 ,
	28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226 ,
	71,  17, 161,  93, 186,  87, 244, 138,	20,  52, 123, 251,	26,  36 ,
	17,  46,  52, 231, 232,  76,  31, 221,	84,  37, 216, 165, 212, 106 ,
	197, 242,  98,	43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136 ,
	120, 163, 236, 249
};

int 	prndindex = 0;

// Which one is deterministic?
int P_Random (void)
{
	prndindex = (prndindex+1)&0xff;
	return rndtable[prndindex];
}

void M_ClearRandom (void)
{
	prndindex = 0;
}
