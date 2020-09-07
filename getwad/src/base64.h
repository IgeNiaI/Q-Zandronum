/*
 * Utility routines to deal with base64 encoding/decoding.
 * Written by Hippocrates Sendoukas, June 2002.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the number of bytes required to store the encoded form
 * of "sz" bytes. If sz<=0, then it returns -1. It does not
 */
int	b64_encsize(int sz);

/*
 * Returns the number of bytes required to store the decoded form
 * of "sz" (assumed to be encoded) bytes. If sz<=0, then it returns -1.
 */
int	b64_decsize(int sz);

/*
 * Encodes the specified input and returns the number of bytes placed
 * in outbuf. If it returns -1, then the parameters are bad. If it
 * returns -2, then there is not enough room in outbuf for the output
 * string.
 */
int	b64_encode(unsigned char *inbuf,int inbufsz,
		unsigned char *outbuf,int outbufsz);

/*
 * Decodes the specified input and returns the number of bytes placed
 * in outbuf. If it returns -1, then the parameters are bad. If it
 * returns -2, then there is not enough room in outbuf for the output
 * string. If it returns -3, then the input contains an invalid character.
 * If it returns -4, it found a premature '=' (or there may be junk after
 * the end of the encoded data). If it returns -5, the input data are
 * corrupted.
 */
int	b64_decode(unsigned char *inbuf,int inbufsz,
		unsigned char *outbuf,int outbufsz);

#ifdef __cplusplus
}
#endif
