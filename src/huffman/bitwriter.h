/*
 * skulltag::BitWriter class - Enables writing arbitrary bit lengths of data.
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

#ifndef _BIT_WRITER_VERSION
#define _BIT_WRITER_VERSION 1

/** Prevents naming convention problems via encapsulation. */
namespace skulltag { // scope limitation

/** BitWriter - Allows writing of varying amounts of bits to a char buffer. <br>
 * Very usefull for outputting variable bit length encodings such as Huffman. */

	class BitWriter {
		int bufferBits;					/**< intermediary buffer of bits. */
		int bitsLeft;					/**< number of bits left in the buffer. */
		unsigned char * currentByte;	/**< position in memory the next char will be stored. */
		int bytesAvailable;				/**< amount of available space left in the output buffer in bytes. Excludes the contents of bufferBits. */
		int bitsAvailable;				/**< amount of available space left in the output buffer in bits. Includes the contents of bufferBits. */
		int maximumBytes;				/**< total amount of bytes that can be written to the output buffer. */
		static int mask[];				/**< maps a number of bits to a bit mask containing as many bits. */
		static int intSize;				/**< number of chars in an int. */
public:

		/** Creates a new BitWriter. */
		BitWriter();

		/** Creates a new BitWriter.
		 * @param output	Destination that bits will be written to.
		 * @param max		Maximum number of chars to output. */
		BitWriter( unsigned char * const output, int const &max );

		/** Appends a char worth of bits to the buffer.
		 * @param bits 		the bits to append.
		 * @return true if successful, false if an error occurs. */
		bool put( unsigned char const &bits );

		/** Appends a short worth of bits to the buffer.
		 * @param bits 		the bits to append.
		 * @return true if successful, false otherwise.
		 * @return true if successful, false if an error occurs. */
		bool put( short const &bits );

		/** Appends an int worth of bits to the buffer.
		 * @param bits 		the bits to append.
		 * @return true if successful, false if an error occurs. */
		bool put( int const &bits );

		/** Appends a specified number of bits from an int to the buffer.
		 * @param bits 		the bits to append. <br>
		 *		The bits should be stored in the least significant portion of the int.
		 * @param count		the number of bits to append.
		 * @return true if successful, false if an error occurs. */
		bool put( int const &bits, int count );

		/** Appends multiple chars from a buffer to this BitWriter.
		 * @param inputBuffer 		pointer to char data
		 * @param count				number of chars to read
		 * @return true if successful, false if an error occurs. */
		bool put( unsigned char const * const inputBuffer, int count );

		/** Sets the output buffer that bytes will be written to.
		 * @param output	Destination that bits will be written to.
		 * @param max		Maximum number of chars to output.
		 * @return true if successful, false if an error occurs. */
		bool outputBuffer( unsigned char * const output, int const &max );

		/** Writes any full chars of data stored in this BitWriter to the output char buffer.
		 * @return true if successful, false if an error occurs. */
		bool flush();

		/** Flushes this BitWriter then outputs any partial chars by padding them with zeros. <br>
		 * After calling finish() all other calls to update the BitWriter will fail until a buffer is set via outputBuffer().
		 * @param bytesWritten 	out: the number of bytes written to the output buffer.
		 * @param paddingBits	out: the number of padding bits used in the final byte of output.
		 * @return true if successful, false if an error occurs. */
		bool finish( int &bytesWritten, int &paddingBits );

	private:

		/** Initializes this BitWriter. */
		void init();

	};

}
#endif

