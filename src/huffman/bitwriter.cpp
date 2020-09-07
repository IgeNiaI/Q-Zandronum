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

#include "bitwriter.h"

/* The internal buffer of bits is an int which is initiallized to zero.
 * The Bit Buffer: [00000000000000000000000000000000] - 0 bits stored.
 *
 * Bits stored in the bit buffer occupy the most significant bits available.
 * When bits are stored their order is preserved.
 * Storing the 5 bits 01011 into the bit buffer would result in:
 * The Bit Buffer: [01011000000000000000000000000000] - 5 bits stored.
 *
 * Additionally storing 0x12f3 (13 bits) would result in:
 * The Bit Buffer: [01011100101111001100000000000000] - 18 bits stored.
 *
 * Data is stored via any of the put(...) functions.
 *
 * To retrieve bytes (chars) of data from the bit buffer and empty the bit buffer: flush();
 * Flushing the bit buffer takes groups of 8 bits (octets) and stores them in the
 * output buffer at the current byte position. Calling flush() would cause output
 * to receive 2 bytes (0x5c, 0xbc) == (01011100, 10111100) and the remaining bits would
 * be moved to the most significant bit positions.
 * The Bit Buffer: [11000000000000000000000000000000] - 2 bits stored.
 * Note: The empty bits of the bit buffer must be zero to avoid additional masking operations.
 * flush() is called automatically if the bit buffer is too full during a call to put(...)
 *
 * When the bit storage operation is complete call finish(...).  Finish will flush as many full
 * bytes of data into the output buffer as possible, the remaining bits will be padded out
 * to a full octet with zeros (on their less significant side), then written to the output
 * buffer. finish( bytesStored, padding ) passes by reference the number of bytes stored
 * and number of padding bits added (0 to 7) to the last stored byte.
 *
 * Bits are not added one at a time, they are masked, shifted and bitwise OR'ed into
 * the buffer in as large a group as possible. This allows the BitWriter to add multiple bits in a single
 * operation instead of calling a function for each 1 bit added.
 *
 * Normal usage:
 *
 * char * dataArray = new char[ max_output_size ];
 * BitWriter * buffer = new BitWriter( dataArray, max_output_size );
 * ...
 * // various calls to put(...);
 * ...
 * int numBytesOutput, paddedBits;
 * buffer->finish( numBytesOutput, paddedBits );
 * ...
 * // do something with dataArray
 * ...
 * delete buffer;
 * delete dataArray;
 *
 * */

/** Prevents naming convention problems via encapsulation. */
namespace skulltag {

	// Static variable initially set to zero as a signal for init()
	int BitWriter::intSize = 0;
	int BitWriter::mask[32];

	/** Initializes this BitWriter. */
	void BitWriter::init(){

		// initialize static variables if not initialized yet.
		if ( intSize == 0 ){
			intSize = sizeof maximumBytes;
			mask[0] = 0;

			// fill mask such that m = { 0, 1, 3, 7, 15, etc. }
			for ( int i = 1; i < 32; i++ ) mask[i] = (mask[i-1] << 1) | 1;
		}

		// initialize member variables.
		bitsAvailable =		0;
		bytesAvailable =	0;
		bufferBits =		0;
		currentByte =		0;
		maximumBytes =		0;
		bitsLeft =			intSize << 3;
	}

	/** Creates a new BitWriter. */
	BitWriter::BitWriter(){
		init();
	}

	/** Creates a new BitWriter.
	 * @param output	Destination that bits will be written to.
	 * @param max		Maximum number of chars to output. */
	BitWriter::BitWriter( unsigned char * const output, int const &max ){
		outputBuffer( output, max );
	}

	/** Sets the output buffer that bytes will be written to.
	 * @param output	Destination that bits will be written to.
	 * @param max		Maximum number of chars to output.
	 * @return true if successful, false otherwise. */
	bool BitWriter::outputBuffer( unsigned char * const output, int const &max ){
		init(); // zero the vars.
		currentByte = output;
		if ( output == 0 ) return false;
		if ( max < 1 ) return false;
		bytesAvailable = max;
		bitsAvailable = max << 3;
		maximumBytes = max;
		return true;
	}

	/** Appends a char worth of bits to the buffer.
	 * @param bits 		the bits to append.
	 * @return true if successful, false otherwise. */
	bool BitWriter::put( unsigned char const &bits ){
		return put( (int)bits, 8 );
	}

	/** Appends a short worth of bits to the buffer.
	 * @param bits 		the bits to append.
	 * @return true if successful, false otherwise. */
	bool BitWriter::put( short const &bits ){
		static int shortBitSize = (sizeof bits) << 3;
		return put( (int)bits, shortBitSize );
	}
	/** Appends an int worth of bits to the buffer.
	 * @param bits 		the bits to append.
	 * @return true if successful, false otherwise. */
	bool BitWriter::put( int const &bits ){
		static int intBitSize = intSize << 3;
		return put( bits, intBitSize);
	}

	/** Appends multiple chars from a buffer to this BitWriter.
	 * @param inputBuffer 		pointer to char data
	 * @param count				number of chars to read
	 * @return true if successful, false otherwise. */
	bool BitWriter::put( unsigned char const * const inputBuffer, int count ){
		int i = 0;

		// Read in 4 bytes at a time and send all at once to the bit buffer.
		while ( (i + 3) < count ){
			if ( !put(
				((int)inputBuffer[ i ]	<<	24) |
				((int)inputBuffer[i+1]	<<	16) |
				((int)inputBuffer[i+2]	<<	 8) |
				 (int)inputBuffer[i+3],		32
			) ) return false;
			i+=4;
		}

		// If any bytes remain, output them one at a time.
		while ( i < count ){
			if ( !put( (int)inputBuffer[ i ], 8 ) ) return false;
			i++;
		}

		return true;
	}

	/** Appends a specified number of bits from an int to the buffer.
	 * @param bits 		the bits to append. <br>
	 *		The bits should be stored in the least significant portion of the int.
	 * @param count		the number of bits to append.
	 * @return true if successful, false otherwise. */
	bool BitWriter::put( int const &bits, int count ){
		if ( count > bitsAvailable ) return false;
		if ( (bitsLeft < 1) && (!flush()) ) return false;
		if ( count > bitsLeft ){
			// not enough space in buffer, fill buffer with top end of input bits then flush.
			bufferBits |= mask[bitsLeft] & (bits >> (count - bitsLeft));
			count -= bitsLeft;
			bitsAvailable -= bitsLeft;
			bitsLeft = 0;

			// Buffer's full, needs flushing.
			if (!flush()) return false;
		}

		// if there are still bits of input...
		if ( count > 0 ){

			// shift the input bits up to the end of the bit buffer.
			bufferBits |= (mask[count] & bits) << (bitsLeft - count);
			bitsAvailable -= count;
			bitsLeft -= count;
		}
		return true;
	}

	/** Writes any full chars of data stored in this BitWriter to the output char buffer.
	 * @return true if successful, false if an error occurs. */
	bool BitWriter::flush(){
		// static var to hold how many bits are in an int.
		static int intBitSize = intSize << 3;
		if ( currentByte == 0 ) return false;
		int numBits = intBitSize - bitsLeft;

		// while there's at least one octet of data in the buffer.
		while ( numBits > 7 ){

			// fail if no bytes can be written.
			if ( bytesAvailable <= 0 ) return false;

			// get a byte off the top end of the buffer.
			*currentByte = (bufferBits >> (intBitSize - 8)) & mask[8];

			// Set variables to reflect the change.
			currentByte++;
			bytesAvailable--;
			bufferBits = bufferBits << 8;
			bitsLeft += 8;
			numBits -= 8;
		}
		return true;
	}

	/** Flushes this BitWriter then outputs any partial chars by padding them with zeros. <br>
	 * After calling finish() all other calls to update the BitWriter will fail until a buffer is set via outputBuffer().
	 * @param bytesWritten 	out: the number of bytes written to the output buffer.
	 * @param paddingBits	out: the number of padding bits used in the final byte of output.
	 * @return true if successful, false if an error occurs. */
	bool BitWriter::finish( int &bytesWritten, int &paddingBits ){
		static int intBitSize = intSize << 3;
		// set meaningful return values even if flush() fails.
		bytesWritten = maximumBytes - bytesAvailable;
		paddingBits = 0;
		if ( flush() ){
			// use a temp var to avoid setting paddingBits to invalid value on failure.
			int pad = (8 - (intBitSize - bitsLeft)) & 7;
			if ( pad > 0 ){
				// all empty bits should be zero. Artificially extend by the number of bits needed.
				bitsLeft -= pad;
				if ( !flush() ){
					// Prevent futher use even on failure.
					init();
					return false;
				}
				// return the temp bit padding value.
				paddingBits = pad;
			}
			bytesWritten = maximumBytes - bytesAvailable;
			init(); // set initial state -- no further writing can occur.
			return true;
		}
		// Prevents futher use even on failure.
		init();
		return false;
	}

}
