/**
 * skulltag::BitReader class - Allows reading arbitrary bit lengths of data.
 * Version 1 - Revsion 0
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

#ifndef _BIT_READER_VERSION
#define _BIT_READER_VERSION 1

/** Prevents naming convention problems via encapsulation. */
namespace skulltag { // scope limitation

/** BitReader - Allows reading of varying amounts of bits from a char buffer. <br>
 * Very usefull for inputting variable bit length encodings such as Huffman. */

	class BitReader {
		int bufferBits;					/**< intermediary buffer of bits. */
		int bitsUsed;					/**< number of bits used in the buffer. */
		unsigned char const * currentByte;	/**< position in memory the next char will be read. */
		int bytesAvailable;				/**< amount of available space left in the input buffer in bytes. Excludes the contents of bufferBits. */
		int bitsAvailable;				/**< amount of available space left in the input buffer in bits. Includes the contents of bufferBits. */
		int maximumBytes;				/**< total amount of bytes that can be read from the input buffer. */
		static int mask[];				/**< maps a number of bits to a bit mask containing as many bits. */
		static int intSize;				/**< number of chars in an int. */
		static int intBitSize;			/**< number of bits in an ing. */
public:

		/** Creates a new BitReader. */
		BitReader();

		/** Creates a new BitReader.
		 * @param input		Source of data that bits will be read from.
		 * @param max		Maximum number of chars to read. */
		BitReader( unsigned char const * input, int const &max );

		/** Sets the input buffer that bytes will be read from.
		 * @param input		Source of data that bits will be read from.
		 * @param max		Maximum number of chars to input.
		 * @return true if successful, false if an error occurs. */
		bool inputBuffer( unsigned char const * input, int const &max );
		
		/** Fetches a specified number of bits and stores them in an int.
		 * @param bits 		destination of the retrieved bits. <br>
		 *		The bits will be stored in the least significant portion of the int.
		 * @param count		the number of bits to fetch.
		 * @return the number of bits read -- may not be equal to the amount requested. */
		int get( int &bits, int const &count );

		/** @return Amount of bits that can be read from this BitReader. */
		int availableBits();

	private:

		/** Fills the internal bit buffer.
		 * @return true if successful, false if an error occurs. */
		bool fill();

		/** Initializes this BitReader. */
		void init();

	};

}
#endif

