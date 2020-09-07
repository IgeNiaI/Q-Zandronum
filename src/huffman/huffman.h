/*
 * Replacement for older Skulltag Launcher Protocol's huffman.cpp
 * 
 * Copyright 2009 Timothy Landers
 * email: code.vortexcortex@gmail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//Macro name kept for backwards compatibility.
#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#include "huffcodec.h"

/** Creates and intitializes a HuffmanCodec Object. <br>
 * Also arranges for HUFFMAN_Destruct() to be called upon termination. */
void HUFFMAN_Construct();

/** Releases resources allocated by the HuffmanCodec. */
void HUFFMAN_Destruct();

/** Applies Huffman encoding to a block of data. */
void HUFFMAN_Encode(
	unsigned char const * const inputBuffer,	/**< in: Pointer to start of data that is to be encoded. */
	unsigned char * const outputBuffer,			/**< out: Pointer to destination buffer where encoded data will be stored. */
	int const &inputBufferSize,					/**< in: Number of chars to read from inputBuffer. */
	int *outputBufferSize						/**< in+out: Max chars to write into outputBuffer. Upon return holds the number of chars stored or 0 if an error occurs. */
);

/** Decodes a block of data that is Huffman encoded. */
void HUFFMAN_Decode(
	unsigned char const * const inputBuffer,	/**< in: Pointer to start of data that is to be decoded. */
	unsigned char * const outputBuffer,			/**< out: Pointer to destination buffer where decoded data will be stored. */
	int const &inputBufferSize,					/**< in: Number of chars to read from inputBuffer. */
	int *outputBufferSize						/**< in+out: Max chars to write into outputBuffer. Upon return holds the number of chars stored or 0 if an error occurs. */
);

#endif // __HUFFMAN_H__
