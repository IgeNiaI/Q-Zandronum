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

// required for atexit()
#include <stdlib.h>

#include "huffman.h"
#include "huffcodec.h"
#include "i_system.h"

using namespace skulltag;
// Global Variables

/** Reference to the HuffmanCodec Object that will perform the encoding and decoding. */
static HuffmanCodec * __codec = NULL;

// Function Implementation

/** Creates and intitializes a HuffmanCodec Object. <br>
 * Also arranges for HUFFMAN_Destruct() to be called upon termination. */
void HUFFMAN_Construct(){
	
	// The exact structure description of a Huffman tree
	static unsigned char const compatible_huffman_tree[] = {
		  0,  0,  0,  1,128,  0,  0,  0,  3, 38, 34,  2,  1, 80,  3,110,
		144, 67,  0,  2,  1, 74,  3,243,142, 37,  2,  3,124, 58,182,  0,
		  0,  1, 36,  0,  3,221,131,  3,245,163,  1, 35,  3,113, 85,  0,
		  1, 41,  1, 77,  3,199,130,  0,  1,206,  3,185,153,  3, 70,118,
		  0,  3,  3,  5,  0,  0,  1, 24,  0,  2,  3,198,190, 63,  2,  3,
		139,186, 75,  0,  1, 44,  2,  3,240,218, 56,  3, 40, 39,  0,  0,
		  2,  2,  3,244,247, 81, 65,  0,  3,  9,125,  3, 68, 60,  0,  0,
		  1, 25,  3,191,138,  3, 86, 17,  0,  1, 23,  3,220,178,  2,  3,
		165,194, 14,  1,  0,  2,  2,  0,  0,  2,  1,208,  3,150,157,181,
		  1,222,  2,  3,216,230,211,  0,  2,  2,  3,252,141, 10, 42,  0,
		  2,  3,134,135,104,  1,103,  3,187,225, 95, 32,  0,  0,  0,  0,
		  0,  0,  1, 57,  1, 61,  3,183,237,  0,  0,  3,233,234,  3,246,
		203,  2,  3,250,147, 79,  1,129,  0,  1,  7,  3,143,136,  1, 20,
		  3,179,148,  0,  0,  0,  3, 28,106,  3,101, 87,  1, 66,  0,  3,
		180,219,  3,227,241,  0,  1, 26,  1,251,  3,229,214,  3, 54, 69,
		  0,  0,  0,  0,  0,  3,231,212,  3,156,176,  3, 93, 83,  0,  3,
		 96,253,  3, 30, 13,  0,  0,  2,  3,175,254, 94,  3,159, 27,  2,
		  1,  8,  3,204,226, 78,  0,  0,  0,  3,107, 88,  1, 31,  3,137,
		169,  2,  2,  3,215,145,  6,  4,  1,127,  0,  1, 99,  3,209,217,
		  0,  3,213,238,  3,177,170,  1,132,  0,  0,  0,  2,  3, 22, 12,
		114,  2,  2,  3,158,197, 97, 45,  0,  1, 46,  1,112,  3,174,249,
		  0,  3,224,102,  2,  3,171,151,193,  0,  0,  0,  3, 15, 16,  3,
		  2,168,  1, 49,  3, 91,146,  0,  1, 48,  3,173, 29,  0,  3, 19,
		126,  3, 92,242,  0,  0,  0,  0,  0,  0,  3,205,192,  2,  3,235,
		149,255,  2,  3,223,184,248,  0,  0,  3,108,236,  3,111, 90,  2,
		  3,117,115, 71,  0,  0,  3, 11, 50,  0,  3,188,119,  1,122,  3,
		167,162,  1,160,  1,133,  3,123, 21,  0,  0,  2,  1, 59,  2,  3,
		155,154, 98, 43,  0,  3, 76, 51,  2,  3,201,116, 72,  2,  0,  2,
		  3,109,100,121,  2,  3,195,232, 18,  1,  0,  2,  0,  1,164,  2,
		  3,120,189, 73,  0,  1,196,  3,239,210,  3, 64, 62, 89,  0,  0,
		  1, 33,  2,  3,228,161, 55,  2,  3, 84,152, 47,  0,  0,  2,  3,
		207,172,140,  3, 82,166,  0,  3, 53,105,  1, 52,  3,202,200
	};

	// create a HuffmanCodec that is compatible with the previous implementation.
	__codec = new HuffmanCodec( compatible_huffman_tree, sizeof compatible_huffman_tree );
	
	// set up the HuffmanCodec to perform in a backwards compatible fashion.
	__codec->reversedBytes( true );
	__codec->allowExpansion( false );
	
	// request that the destruct function be called upon exit.
	atterm( HUFFMAN_Destruct );
}

/** Releases resources allocated by the HuffmanCodec. */
void HUFFMAN_Destruct(){
	delete __codec;
	__codec = NULL;
}

/** Applies Huffman encoding to a block of data. */
void HUFFMAN_Encode(
	/** in: Pointer to start of data that is to be encoded. */
	unsigned char const * const inputBuffer,
	/** out: Pointer to destination buffer where encoded data will be stored. */
	unsigned char * const outputBuffer,
	/** in: Number of chars to read from inputBuffer. */
	int const &inputBufferSize,
	/**< in+out: Max chars to write into outputBuffer. <br>
	 * 		Upon return holds the number of chars stored or 0 if an error occurs. */
	int * outputBufferSize
){
	int bytesWritten = __codec->encode( inputBuffer, outputBuffer, inputBufferSize, *outputBufferSize );
	
	// expansion occured -- provide backwards compatibility
	if ( bytesWritten < 0 ){
		// check buffer sizes
		if ( *outputBufferSize < (inputBufferSize + 1) ){
			// outputBuffer too small, return "no bytes written"
			*outputBufferSize = 0;
			return;
		}
		
		// perform the unencoded copy
		for ( int i = 0; i < inputBufferSize; i++ ) outputBuffer[i+1] = inputBuffer[i];
		// supply the "unencoded" signal and bytesWritten
		outputBuffer[0] = 0xff;
		*outputBufferSize = inputBufferSize + 1;
	} else {
		// assign the bytesWritten return value
		*outputBufferSize = bytesWritten;
	}
} // end function HUFFMAN_Encode

/** Decodes a block of data that is Huffman encoded. */
void HUFFMAN_Decode(
	unsigned char const * const inputBuffer,	/**< in: Pointer to start of data that is to be decoded. */
	unsigned char * const outputBuffer,			/**< out: Pointer to destination buffer where decoded data will be stored. */
	int const &inputBufferSize,					/**< in: Number of chars to read from inputBuffer. */
	int *outputBufferSize						/**< in+out: Max chars to write into outputBuffer. Upon return holds the number of chars stored or 0 if an error occurs. */
){
	// check for "unencoded" signal & provide backwards compatibility
	if ((inputBufferSize > 0) && ((inputBuffer[0]&0xff) == 0xff)){
		// check buffer sizes
		if ( *outputBufferSize < (inputBufferSize - 1) ){
			// outputBuffer too small, return "no bytes written"
			*outputBufferSize = 0;
			return;			
		}
		
		// perform the unencoded copy
		for ( int i = 1; i < inputBufferSize; i++ ) outputBuffer[i-1] = inputBuffer[i];
		
		// supply the bytesWritten
		*outputBufferSize = inputBufferSize - 1;
	} else {
		// decode the data
		*outputBufferSize = __codec->decode( inputBuffer, outputBuffer, inputBufferSize, *outputBufferSize );
	}
} // end function HUFFMAN_Decode
