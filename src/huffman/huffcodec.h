/*
 * skulltag::HuffmanCodec class - Huffman encoder and decoder.
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

#ifndef _HUFFMAN_CODEC_VERSION
#define _HUFFMAN_CODEC_VERSION 1
#define _HUFFMAN_CODEC_REV 0

#include "codec.h"
#include "bitwriter.h"
#include "bitreader.h"

/** Prevents naming convention problems via encapsulation. */
namespace skulltag {

	/** HuffmanCodec class - Encodes and Decodes data using a Huffman tree. */
	class HuffmanCodec : public Codec {

		/** top level node of the Huffman tree used for decoding. */
		HuffmanNode * root;

		/** table of Huffman codes and bit lengths used for encoding. */
		HuffmanNode ** codeTable;

		/** intermediary destination of huffman codes. */
		BitWriter * writer;
		
		/** When true this HuffmanCodec reverses its bytes after encoding and before decoding to
		 * provide compatibility with the backwards bit ordering of the original ST Huffman Encoding.
		 * Default value is "false" (do not reverse bits). */
		bool reverseBits;

		/** When false this HuffmanCodec return -1 instead of expanding data during encoding.
		 * Default value is "true" (allow data expansion). */
		bool expandable;
		
		/** Determines if this HuffmanCodec owns its Huffman tree nodes. */
		bool huffResourceOwner;

		/** Reverses the order of bits in a byte.
		 *	EG: The statement <code>reverseMap[0xAF] == 0xF5</code> is <code>true</code>. <br>
		 *	The index <code>10101111</code> stores the reverse value: <code>11110101</code>. <br>
		 *  Note: One array lookup is much faster than Eight bit manipulating loop iterations. */
		static unsigned char const reverseMap[];
	
		/** Number of bits the shortest huffman code in the tree has. */
		int shortestCode;	

	public:	

		/** Creates a new HuffmanCodec from the Huffman tree data.
		 * @param treeData 		pointer to a buffer containing the Huffman tree structure definition.
		 * @param dataLength 	length in bytes of the Huffman tree structure data. */
		HuffmanCodec( unsigned char const * const treeData, int dataLength );
		
		/** Creates a new HuffmanCodec that uses the specified Huffman resources.
		* @param treeRootNode	The root node of a valid huffman tree.
		* @param leafCodeTable	A code lookup table where references to HuffmanNodes are stored with their array index equal to their value.
		* Note: The tree nodes will not be released upon destruction of this HuffmanCodec. */
		HuffmanCodec(
			HuffmanNode * treeRootNode,
			HuffmanNode ** leafCodeTable
		);

		/** Frees resources used internally by this HuffmanCodec. */
		virtual ~HuffmanCodec();

		/** Decodes data read from an input buffer and stores the result in the output buffer.
		 * @return number of bytes stored in the output buffer or -1 if an error occurs while encoding. */
		virtual int encode(
			unsigned char const * const input,	/**< in: pointer to the first byte to encode. */
			unsigned char * const output,		/**< out: pointer to an output buffer to store data. */
			int const &inLength,				/**< in: number of bytes of input buffer to encoded. */
			int const &outLength				/**< in: maximum length of data to output. */
		) const;

		/** Decodes data read from an input buffer and stores the result in the output buffer.
		 * @return number of bytes stored in the output buffer or -1 if an error occurs while decoding. */
		virtual int decode(
			unsigned char const * const input,	/**< in: pointer to data that needs decoding. */
			unsigned char * const output,		/**< out: pointer to output buffer to store decoded data. */
			int const &inLength,				/**< in: number of bytes of input buffer to read. */
			int const &outLength				/**< in: maximum length of data to output. */
		);

		/** Enables or Disables backwards bit ordering of bytes.
		 * @param backwards  "true" enables reversed bit order bytes, "false" uses standard byte bit ordering. */
		void reversedBytes( bool backwards );

		/** Check the state of backwards bit ordering for bytes.
		 * @return  true: bits within bytes are reversed. false: bits within bytes are normal. */
		bool reversedBytes();

		/** Enable or Disable data expansion during encoding.
		 * @param expandingAllowed	"true" allows encoding to expand data. "false" causes failure upon expansion. */
		void allowExpansion( bool expandable );

		/** Check the state of data expandability.
		 * @return	 true: data expansion is allowed.  false: data is not allowed to expand. */
		bool allowExpansion();

		/** Sets the ownership of this HuffmanCodec's resources.
		* @param ownsResources	When false the tree will not be released upon destruction of this HuffmanCodec.
		* 						When true deleting this HuffmanCodec will cause the Huffman tree to be released. */
		void huffmanResourceOwner( bool ownsResources );
		
		/** Checks the ownership state of this HuffmanCodec's resources.
		* @return ownsResources	When false the tree will not be released upon destruction of this HuffmanCodec.
		* 						When true deleting this HuffmanCodec will cause the Huffman tree to be released. */
		bool huffmanResourceOwner();

		/** Deletes all sub nodes of a HuffmanNode by traversing and deleting its child nodes.
		 * @param treeNode pointer to a HuffmanNode whos children will be deleted. */
		static void deleteTree( HuffmanNode * treeNode );

		/** Recursively builds a Huffman Tree. <br>
		 * The initial root node should have the following field values: <br>
		 * <pre>
		 * bitCount : 0
		 * code     : 0
		 * value    : -1
		 * branch   : 0 (NULL)
		 * </pre>
		 * @param node		in/out: branch node of the Huffman Tree.
		 * @param treeData	in: char array containing the Huffman Tree's byte representation.
		 * @param index		in: Current array element to read the next tree node from.
		 * @param dataLength in: Length of treeData
		 * @param codeTable in/out: array of pointers to HuffmanNode structs.
		 * @param tableLength in: maximum index allowed in the codeTable.
		 * @return the next index to read from or -1 if an error occurs.
		 * */
		int buildTree(
			HuffmanNode * node,
			unsigned char const * const treeData,
			int index,
			int dataLength,
			HuffmanNode ** const &codeTable,
			int tableLength
		);

		/** Decreases a codeLength to the shortest Huffman code bit length found in the node or any of its children. <br>
		 * Set to Zero before calling to determine minimum code bit length.
		 * @param node			in: The node to begin searching at.
		 * @param codeLength	out: Variable to hold the longest code bit length found. */
		static void minCodeLength( HuffmanNode const * const node, int &codeLength );
		
		/** Increases a codeLength up to the longest Huffman code bit length found in the node or any of its children. <br>
		 * Set to Zero before calling to determine maximum code bit length.
		 * @param node			in: The node to begin searching at.
		 * @param codeLength	out: Variable to hold the longest code bit length found. */
		static void maxCodeLength( HuffmanNode const * const node, int &codeLength );

	private:
		
		/** Perform initialization procedures common to all constructors. */
		void init();

	}; // end class Huffman Codec.
} // end namespace skulltag

#endif
