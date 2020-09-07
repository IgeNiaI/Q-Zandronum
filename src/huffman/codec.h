/*
 * skulltag::Codec class interface - Base class for data encoding or decoding operations.
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

#ifndef _CODEC_VERSION
#define _CODEC_VERSION 1

/** Prevents naming convention problems via encapsulation. */
namespace skulltag {

	/** Huffman tree node -- used to represent a Huffman tree. <br>
	 * Huffman trees are use by compression / decompression codecs. */
	struct HuffmanNode {
		int bitCount;			/**< number of bits in the Huffman code. */
		int code;				/**< bit representation of a Huffman code. */
		int value;				/**< the value the Huffman code represents. */
		HuffmanNode * branch;	/**< the left and right child branches or NULL (0) if leaf. */
	};

// Codec Class Interface

	/** Base class for encoding and decoding data. */
	class Codec {

	public:

		/** Decodes data read from an input buffer and stores the result in the output buffer.
		 * @return number of bytes stored in the output buffer or -1 if an error occurs while encoding. */
		virtual int encode(
			unsigned char const * const input,	/**< in: pointer to the first byte to encode. */
			unsigned char * const output,		/**< out: pointer to an output buffer to store data. */
			int const &inLength,		/**< in: number of bytes of input buffer to encoded. */
			int const &outLength		/**< in: maximum length of data to output. */
		) const = 0;

		/** Decodes data read from an input buffer and stores the result in the output buffer.
		 * @return number of bytes stored in the output buffer or -1 if an error occurs while decoding. */
		virtual int decode(
			unsigned char const * const input,	/**< in: pointer to data that needs decoding. */
			unsigned char * const output,		/**< out: pointer to output buffer to store decoded data. */
			int const &inLength,		/**< in: number of bytes of input buffer to read. */
			int const &outLength		/**< in: maximum length of data to output. */
		) = 0;

	}; // end class Codec

}; // end namespace Codec

#endif
