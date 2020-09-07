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

#include "huffcodec.h"

/** Prevents naming convention problems via encapsulation. */
namespace skulltag {

// HuffmanCodec Implementation

	/** Reverses the order of bits in a byte.
	 *	EG: The statement <code>reverseMap[0xAF] == 0xF5</code> is <code>true</code>. <br>
	 *	The index <code>10101111</code> stores the reverse value: <code>11110101</code>. <br>
	 *  Note: One array lookup is much faster than Eight bit manipulating loop iterations. */
	unsigned char const HuffmanCodec::reverseMap[] = {
		  0,128, 64,192, 32,160, 96,224, 16,144, 80,208, 48,176,112,240,
		  8,136, 72,200, 40,168,104,232, 24,152, 88,216, 56,184,120,248,
		  4,132, 68,196, 36,164,100,228, 20,148, 84,212, 52,180,116,244,
		 12,140, 76,204, 44,172,108,236, 28,156, 92,220, 60,188,124,252,
		  2,130, 66,194, 34,162, 98,226, 18,146, 82,210, 50,178,114,242,
		 10,138, 74,202, 42,170,106,234, 26,154, 90,218, 58,186,122,250,
		  6,134, 70,198, 38,166,102,230, 22,150, 86,214, 54,182,118,246,
		 14,142, 78,206, 46,174,110,238, 30,158, 94,222, 62,190,126,254,
		  1,129, 65,193, 33,161, 97,225, 17,145, 81,209, 49,177,113,241,
		  9,137, 73,201, 41,169,105,233, 25,153, 89,217, 57,185,121,249,
		  5,133, 69,197, 37,165,101,229, 21,149, 85,213, 53,181,117,245,
		 13,141, 77,205, 45,173,109,237, 29,157, 93,221, 61,189,125,253,
		  3,131, 67,195, 35,163, 99,227, 19,147, 83,211, 51,179,115,243,
		 11,139, 75,203, 43,171,107,235, 27,155, 91,219, 59,187,123,251,
		  7,135, 71,199, 39,167,103,231, 23,151, 87,215, 55,183,119,247,
		 15,143, 79,207, 47,175,111,239, 31,159, 95,223, 63,191,127,255
	};

	/** Creates a new HuffmanCodec
	 * @param treeData char array containing the tree data to use.
	 * @param dataLength number of chars in treeData. */
	HuffmanCodec::HuffmanCodec(
		unsigned char const * const treeData,
		int dataLength
	) : Codec() {
		init();
		// init code table (256 pointers to Huffman Leaf Nodes.)
		codeTable = new HuffmanNode*[256];
		for (int i = 0; i < 256; i++) codeTable[i] = 0;
		// build root node
		root = new HuffmanNode;
		root->bitCount = 0;
		root->code = 0;
		root->value = -1;
		// recursive Huffman tree builder.
		buildTree( root, treeData, 0, dataLength, codeTable, 256 );
		huffResourceOwner = true;
	}
	

	/** Creates a new HuffmanCodec that uses the specified Huffman resources.
	* @param treeRootNode	The root node of a valid huffman tree.
	* @param leafCodeTable	A code lookup table where references to HuffmanNodes are stored with their array index equal to their value.
	* Note: The tree nodes will not be released upon destruction of this HuffmanCodec. */
	HuffmanCodec::HuffmanCodec(
		HuffmanNode * treeRootNode,
		HuffmanNode ** leafCodeTable
	){
		init();
		// assign values -- no table building or allocations.
		root = treeRootNode;
		codeTable = leafCodeTable;
		huffResourceOwner = false;
	}
	
	/** Checks the ownership state of this HuffmanCodec's resources.
	* @return true if the tree & code table will be released upon destruction of this HuffmanCodec. <br>
	* 		A false return value means this HuffmanCodec is not responsible for deleting its resources. */
	bool HuffmanCodec::huffmanResourceOwner(){
		return huffResourceOwner;
	}

	/** Perform initialization procedures common to all constructors. */
	void HuffmanCodec::init(){
		writer = new BitWriter();
		reverseBits = false;
		expandable = true;
		huffResourceOwner = false;
	}
	
	/** Increases a codeLength up to the longest Huffman code bit length found in the node or any of its children. <br>
	 * Set to Zero before calling to determine maximum code bit length.
	 * @param node			in: The node to begin searching at.
	 * @param codeLength	out: Variable to hold the longest code bit length found. */
	void HuffmanCodec::maxCodeLength( HuffmanNode const * const node, int &codeLength ){
		// [TL] We must walk each tree node since the codeTable may not contain the set of all leaf nodes.
		// bail on NULL node (tree is corrupt).
		if ( node == 0) return;
		// Recurse across children if they exist.
		if ( node->branch != 0 ){
			maxCodeLength( &(node->branch[0]), codeLength );
			maxCodeLength( &(node->branch[1]), codeLength );
		} else if ( codeLength < node->bitCount ){
			// set codeLength if it's smaller than current node's bitCount.
			codeLength = node->bitCount;
		}
	}

	/** Decreases a codeLength to the shortest Huffman code bit length found in the node or any of its children. <br>
	 * Set to Zero before calling to determine minimum code bit length.
	 * @param node			in: The node to begin searching at.
	 * @param codeLength	out: Variable to hold the longest code bit length found. */
	void HuffmanCodec::minCodeLength( HuffmanNode const * const node, int &codeLength ){
		/* [TL] Do not optimize under the assumption child nodes will have longer code Lengths!
		 * Future subclasses may have trees that diverge from Huffman specs. */
		// bail on NULL node (tree is corrupt).
		if ( node == 0 ) return;
		// Recurse across children if they exist.
		if ( node->branch != 0 ){			
			minCodeLength( &(node->branch[0]), codeLength );
			minCodeLength( &(node->branch[1]), codeLength );
		} else if ( (codeLength > node->bitCount) || (codeLength == 0) ) {
			// set codeLength if it's Zero or larger than current node's bitCount.
			codeLength = node->bitCount;
		}
	}

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
	int HuffmanCodec::buildTree(
		HuffmanNode * node,
		unsigned char const * const treeData,
		int index,
		int dataLength,
		HuffmanNode ** const &codeTable,
		int tableLength
	){
		if ( index >= dataLength ) return -1;
		// Read the branch description bit field
		int desc = treeData[index];
		index++;

		// Create the array that will hold L/R child nodes of this branch.
		node->branch = new HuffmanNode[2];

		// Read the child Nodes for this branch.
		for ( int i = 0; i < 2; i++ ){
			// Increase bit count, and update huffman code to match the node's tree position.
			node->branch[i].bitCount = node->bitCount + 1;
			node->branch[i].code = (node->code << 1) | i; // appends a 0 or 1 depending on L/R branch.
			node->branch[i].value = -1; // default value.

			// Test a bit from the branch description (least significant bit == left)
			if ( (desc & (1 << i)) == 0 ){
				// Child node is a branch; Recurse.
				if ( (index = buildTree( &(node->branch[i]), treeData, index, dataLength, codeTable, tableLength )) < 0 ) return -1;
				// This means the entire left sub tree will be read before the right sub tree gets read.
			} else {
				// Read leaf value and map its value/index in the nodes array.
				if ( index >= dataLength ) return -1;
				// set the nodes huffman code values.
				node->branch[i].code = (node->code << 1) | i;
				node->branch[i].bitCount = node->bitCount+1;
				node->branch[i].value = treeData[index] & 0xff;
				// NULL the child node's branch to mark it as a leaf.
				node->branch[i].branch = 0;
				// buffer overflow check.
				if ( (node->branch[i].value >= 0) && (node->branch[i].value <= tableLength ) )
					// store a pointer to the leaf node into the code table at the location of its byte value.
					codeTable[ node->branch[i].value ] = &node->branch[i];
				index++;
			}
		}

		return index;
	}

	/** Decodes data read from an input buffer and stores the result in the output buffer.
	 * @return number of bytes stored in the output buffer or -1 if an error occurs while encoding. */
	int HuffmanCodec::encode(
		unsigned char const * const input,	/**< in: pointer to the first byte to encode. */
		unsigned char * const output,		/**< out: pointer to an output buffer to store data. */
		int const &inLength,				/**< in: number of bytes of input buffer to encoded. */
		int const &outLength				/**< in: maximum length of data to output. */
	) const {
		// setup the bit buffer to output. if not expandable Limit output to input length.
		if ( expandable ) writer->outputBuffer( output, outLength );
		else writer->outputBuffer( output, ((inLength + 1) < outLength) ? inLength + 1 : outLength );

		writer->put( (unsigned char)0 ); // reserve place for padding signal.

		HuffmanNode * node; // temp ptr cache;
		for ( int i = 0; i < inLength; i++ ){
			node = codeTable[ 0xff & input[i] ]; //lookup node
			// Put the huffman code into the bit buffer and bail if error occurs.
			if ( !writer->put( node->code, node->bitCount ) ) return -1;
		}
		int bytesWritten, padding;
		if ( writer->finish( bytesWritten, padding ) ){
			// write padding signal byte to begining of stream.
			output[0] = (unsigned char)padding;
		} else return -1;

		// Reverse the bit order of each byte (Old Huffman Compatibility Mode)
		if ( reverseBits ) for ( int i = 1; i < bytesWritten; i++ ){
			output[i] = reverseMap[ 0xff & output[i] ];
		}

		return bytesWritten;
	} // end function encode

	/** Decodes data read from an input buffer and stores the result in the output buffer.
	 * @return number of bytes stored in the output buffer or -1 if an error occurs while decoding. */
	int HuffmanCodec::decode(
		unsigned char const * const input,	/**< in: pointer to data that needs decoding. */
		unsigned char * const output,		/**< out: pointer to output buffer to store decoded data. */
		int const &inLength,				/**< in: number of bytes of input buffer to read. */
		int const &outLength				/**< in: maximum length of data to output. */
	){
		if ( inLength < 1 ) return 0;
		int bitsAvailable = ((inLength-1) << 3) - (0xff & input[0]);
		int rIndex = 1;		// read index of input buffer.
		int wIndex = 0;		// write index of output buffer.
		char byte = 0;		// bits of the current byte.
		int bitsLeft = 0;	// bits left in byte;

		HuffmanNode * node = root;

		// Traverse the tree, output values.
		while ( (bitsAvailable > 0) && (node != 0) ){

			// Get the next byte if we've run out.
			if ( bitsLeft <= 0 ){
				byte = input[rIndex++];
				if ( reverseBits ) byte = reverseMap[ 0xff & byte ];
				bitsLeft = 8;
			}

			// Traverse the tree according to the most significant bit.
			node = &(node->branch[ ((byte >> 7) & 0x01) ]);

			// Is the node Non NULL, and a leaf?
			if ( (node != 0) && (node->branch == 0) ){
				// buffer overflow prevention
				if ( wIndex >= outLength ) return wIndex;
				// Output leaf node's value and restart traversal at root node.
				output[ wIndex++ ] = (unsigned char)(node->value & 0xff);
				node = root;
			}

			byte <<= 1;			// cue up the next bit
			bitsLeft--;			// use up one bit of byte
			bitsAvailable--;	// decrement total bits left
		}

		return wIndex;
	} // end function decode

	/** Deletes all sub nodes of a HuffmanNode by traversing and deleting its child nodes.
	 * @param treeNode pointer to a HuffmanNode whos children will be deleted. */
	void HuffmanCodec::deleteTree( HuffmanNode * treeNode ){
		if ( treeNode == 0 ) return;
		if ( treeNode->branch != 0 ){
			deleteTree( &(treeNode->branch[0]) );
			deleteTree( &(treeNode->branch[1]) );
			delete[] treeNode->branch;
		}
	}

	/** Destructor - frees resources. */
	HuffmanCodec::~HuffmanCodec() {
		delete writer;
		//check for resource ownership before deletion
		if ( huffmanResourceOwner() ){
			delete[] codeTable;
			deleteTree( root );
			delete root;
		}
	}

	/** Enables or Disables backwards bit ordering of bytes.
	 * @param backwards  "true" enables reversed bit order bytes, "false" uses standard byte bit ordering. */
	void HuffmanCodec::reversedBytes( bool backwards ){ reverseBits = backwards; }

	/** Check the state of backwards bit ordering for bytes.
	 * @return  true: bits within bytes are reversed. false: bits within bytes are normal. */
	bool HuffmanCodec::reversedBytes(){ return reverseBits; }

	/** Enable or Disable data expansion during encoding.
	 * @param expandingAllowed	"true" allows encoding to expand data. "false" causes failure upon expansion. */
	void HuffmanCodec::allowExpansion( bool expandingAllowed ){ expandable = expandingAllowed; }

	/** Check the state of data expandability.
	 * @return	 true: data expansion is allowed.  false: data is not allowed to expand. */
	bool HuffmanCodec::allowExpansion(){ return expandable; }


}; // end namespace skulltag
