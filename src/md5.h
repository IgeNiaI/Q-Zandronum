/*
 * This is the header file for the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#ifndef MD5_H
#define MD5_H

#include "files.h"

struct MD5Context
{
	MD5Context() { Init(); }

	void Init();
	void Update(const BYTE *buf, unsigned len);
	void Update(FileReader *file, unsigned len);
	void Final(BYTE digest[16]);

private:
	DWORD buf[4];
	DWORD bytes[2];
	DWORD in[16];

};

void MD5Transform(DWORD buf[4], DWORD const in[16]);

// [BB] Calculates the MD5 sum of a file and writes it to MD5Sum.
// Writes 33 bytes in total (32 bytes for the sum + 1 for the terminating 0).
// Returns false, if there was a problem reading the file.
bool MD5SumOfFile ( const char *Filename, char *MD5Sum );

#endif /* !MD5_H */
