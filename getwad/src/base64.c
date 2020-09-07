#include "base64.h"
#include <string.h>

static const char encodingTable[65] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
	'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
	'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
	'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/',
	'\0'
	};

/*  <- b64_encsize */

int
b64_encsize(int sz)
{
	int nblocks;

	if (sz<=0) return -1;
	nblocks = (sz+2) / 3;
	return 4*nblocks;
}

/*  -> b64_encsize */
/*  <- b64_decsize */

int
b64_decsize(int sz)
{
	int nblocks;

	if (sz<=0) return -1;
	nblocks = (sz+3) / 4;
	return 3*nblocks;
}

/*  -> b64_decsize */
/*  <- b64_encode */

int
b64_encode(unsigned char *inbuf,int inbufsz,unsigned char *outbuf,int outbufsz)
{
	int		nblocks,remaining;
	unsigned char	tempbuf[3];

	if (!inbuf || !outbuf || inbufsz<=0) return -1;
	nblocks = (inbufsz+2) / 3;
	if (outbufsz<4*nblocks) return -2;

	for (remaining=inbufsz;   remaining>0;   remaining-=3)
	{
		if (remaining >= 3)
		{
			memcpy(tempbuf,inbuf,3);
		}
		else
		{
			memset(tempbuf,0,3);
			memcpy(tempbuf,inbuf,remaining);
		}
		outbuf[0] = encodingTable[  (tempbuf[0] & 0xFC) >> 2 ];
		outbuf[1] = encodingTable[ ((tempbuf[0] & 0x03) << 4) | ((tempbuf[1] & 0xF0) >> 4) ];
		outbuf[2] = encodingTable[ ((tempbuf[1] & 0x0F) << 2) | ((tempbuf[2] & 0xC0) >> 6) ];
		outbuf[3] = encodingTable[   tempbuf[2] & 0x3F ];
		switch (remaining)
		{
			case 1:	outbuf[2] = outbuf[3] = '=';	break;
			case 2:	outbuf[3] = '=';		break;
		}
		inbuf += 3;
		outbuf += 4;
	}
	return 4*nblocks;
}

/*  -> b64_encode */
/*  <- b64_decode */

int
b64_decode(unsigned char *inbuf,int inbufsz,unsigned char *outbuf,int outbufsz)
{
	int		nblocks,remaining,pending;
	char		*p;
	unsigned char	c,temp[4];
	unsigned char	*pout;

	if (!inbuf || !outbuf || inbufsz<=0) return -1;
	nblocks = (inbufsz+3) / 4;
	if (outbufsz<3*nblocks) return -2;

	pout = outbuf;
	pending = 0;
	for (remaining=inbufsz;   remaining>0;   remaining--)
	{
		c = *inbuf++;
		if ( (p=strchr(encodingTable,c)) != NULL )	c = (unsigned char) (p-encodingTable);
		else if (c=='=')				break;
		else						return -3;
		temp[pending++] = c;
		if (pending==4)
		{
			*pout++ = (unsigned char) (  (temp[0] << 2)         | ((temp[1] & 0x30) >> 4) );
			*pout++ = (unsigned char) ( ((temp[1] & 0x0F) << 4) | ((temp[2] & 0x3C) >> 2) );
			*pout++ = (unsigned char) ( ((temp[2] & 0x03) << 6) |  (temp[3] & 0x3F)       );
			pending = 0;
		}
	}
	if (remaining>3) return -4;
	if (pending)
	{
		if (pending==1) return -5;			/* Should not happen */
		memset(temp+pending,0,4-pending);
		*pout++ = (unsigned char) ( (temp[0] << 2) | ((temp[1] & 0x30) >> 4) );
		if (pending>=2)	*pout++ = (unsigned char) ( ((temp[1] & 0x0F) << 4) | ((temp[2] & 0x3C) >> 2) );
		if (pending>=3)	*pout++ = (unsigned char) ( ((temp[2] & 0x03) << 6) |  (temp[3] & 0x3F)       );
	}
	return (int)(pout-outbuf);
}

/*  -> b64_decode */
