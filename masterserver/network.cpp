//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: network.cpp
//
// Description: Contains network definitions and functions not specifically
// related to the server or client.
//
//-----------------------------------------------------------------------------

#include "../src/networkheaders.h"
#include "../src/networkshared.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ctype.h>
#include <math.h>

#include "../src/huffman/huffman.h"
#include "network.h"

//*****************************************************************************
//	VARIABLES

// Buffer that holds the data from the most recently received packet.
static	NETBUFFER_s		g_NetworkMessage;

// Network address that the most recently received packet came from.
static	NETADDRESS_s	g_AddressFrom;

// Our network socket.
static	SOCKET			g_NetworkSocket;

// Our local port.
static	USHORT			g_usLocalPort;

// Buffer for the Huffman encoding.
static	UCHAR			g_ucHuffmanBuffer[131072];

//*****************************************************************************
//	PROTOTYPES

static	void			network_Error( const char *pszError );
static	SOCKET			network_AllocateSocket( void );
static	bool			network_BindSocketToPort( SOCKET Socket, ULONG ulInAddr, USHORT usPort, bool bReUse );

//*****************************************************************************
//	FUNCTIONS

void NETWORK_Construct( USHORT usPort, const char *pszIPAddress )
{
	char			szString[128];
	ULONG			ulArg;
	USHORT			usNewPort;
	NETADDRESS_s	LocalAddress;
	bool			bSuccess;

	// Initialize the Huffman buffer.
	HUFFMAN_Construct( );

#ifdef __WIN32__
	// [BB] Linux doesn't know WSADATA, so this may not be moved outside the ifdef.
	WSADATA			WSAData;
	if ( WSAStartup( 0x0101, &WSAData ))
		network_Error( "Winsock initialization failed!\n" );

	printf( "Winsock initialization succeeded!\n" );
#endif

	ULONG ulInAddr = INADDR_ANY;
	// [BB] An IP was specfied. Check if it's valid and if it is, try to bind our socket to it.
	if ( pszIPAddress )
	{
		ULONG requestedIP = inet_addr( pszIPAddress );
		if ( requestedIP == INADDR_NONE )
		{
			sprintf( szString, "NETWORK_Construct: %s is not a valid IP address\n", pszIPAddress );
			network_Error( szString );
		}
		else
			ulInAddr = requestedIP;
	}

	g_usLocalPort = usPort;

	// Allocate a socket, and attempt to bind it to the given port.
	g_NetworkSocket = network_AllocateSocket( );
	if ( network_BindSocketToPort( g_NetworkSocket, ulInAddr, g_usLocalPort, false ) == false )
	{
		bSuccess = true;
		bool bSuccessIP = true;
		usNewPort = g_usLocalPort;
		while ( network_BindSocketToPort( g_NetworkSocket, ulInAddr, ++usNewPort, false ) == false )
		{
			// Didn't find an available port. Oh well...
			if ( usNewPort == g_usLocalPort )
			{
				// [BB] We couldn't use the specified IP, so just try any.
				if ( ulInAddr != INADDR_ANY )
				{
					ulInAddr = INADDR_ANY;
					bSuccessIP = false;
					continue;
				}
				bSuccess = false;
				break;
			}
		}

		if ( bSuccess == false )
		{
			sprintf( szString, "NETWORK_Construct: Couldn't bind socket to port: %d\n", g_usLocalPort );
			network_Error( szString );
		}
		else if ( bSuccessIP == false )
		{
			sprintf( szString, "NETWORK_Construct: Couldn't bind socket to IP %s, using the default IP instead:\n", pszIPAddress );
			network_Error( szString );
		}
		else
		{
			printf( "NETWORK_Construct: Couldn't bind to %d. Binding to %d instead...\n", g_usLocalPort, usNewPort );
			g_usLocalPort = usNewPort;
		}
	}

	ulArg = true;
	if ( ioctlsocket( g_NetworkSocket, FIONBIO, &ulArg ) == -1 )
		printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));

	// Init our read buffer.
	// [BB] Vortex Cortex pointed us to the fact that the smallest huffman code is only 3 bits
	// and it turns into 8 bits when it's decompressed. Thus we need to allocate a buffer that
	// can hold the biggest possible size we may get after decompressing (aka Huffman decoding)
	// the incoming UDP packet.
	g_NetworkMessage.Init( ((MAX_UDP_PACKET * 8) / 3 + 1), BUFFERTYPE_READ );
	g_NetworkMessage.Clear();

	// [BB] Get and save our local IP.
	if ( ( ulInAddr == INADDR_ANY ) || ( pszIPAddress == NULL ) )
		LocalAddress = NETWORK_GetLocalAddress( );
	// [BB] We are using a specified IP, so we don't need to figure out what IP we have, but just use the specified one.
	else
	{
		LocalAddress.LoadFromString ( pszIPAddress );
		LocalAddress.usPort = htons ( NETWORK_GetLocalPort() );
	}

	// Print out our local IP address.
	printf( "IP address %s\n", LocalAddress.ToString() );

	printf( "UDP Initialized.\n" );
}

//*****************************************************************************
//
int NETWORK_GetPackets( void )
{
	LONG				lNumBytes;
	INT					iDecodedNumBytes = sizeof(g_ucHuffmanBuffer);
	struct sockaddr_in	SocketFrom;
	INT					iSocketFromLength;

    iSocketFromLength = sizeof( SocketFrom );

#ifdef	WIN32
	lNumBytes = recvfrom( g_NetworkSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, &iSocketFromLength );
#else
	lNumBytes = recvfrom( g_NetworkSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, (socklen_t *)&iSocketFromLength );
#endif

	// If the number of bytes returned is -1, an error has occured.
    if ( lNumBytes == -1 ) 
    { 
#ifdef __WIN32__
        errno = WSAGetLastError( );

        if ( errno == WSAEWOULDBLOCK )
            return ( false );

		// Connection reset by peer. Doesn't mean anything to the server.
		if ( errno == WSAECONNRESET )
			return ( false );

        if ( errno == WSAEMSGSIZE )
		{
             printf( "NETWORK_GetPackets:  WARNING! Oversize packet from %s\n", g_AddressFrom.ToString() );
             return ( false );
        }

        printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
		return ( false );
#else
        if ( errno == EWOULDBLOCK )
            return ( false );

        if ( errno == ECONNREFUSED )
            return ( false );

        printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
        return ( false );
#endif
    }

	// No packets or an error, so don't process anything.
	if ( lNumBytes <= 0 )
		return ( 0 );

	// If the number of bytes we're receiving exceeds our buffer size, ignore the packet.
	if ( lNumBytes >= g_NetworkMessage.ulMaxSize )
		return ( 0 );

	// Decode the huffman-encoded message we received.
	HUFFMAN_Decode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, lNumBytes, &iDecodedNumBytes );
	g_NetworkMessage.ulCurrentSize = iDecodedNumBytes;
	g_NetworkMessage.ByteStream.pbStream = g_NetworkMessage.pbData;
	g_NetworkMessage.ByteStream.pbStreamEnd = g_NetworkMessage.ByteStream.pbStream + g_NetworkMessage.ulCurrentSize;

	// Store the IP address of the sender.
	g_AddressFrom.LoadFromSocketAddress( SocketFrom );

	return ( g_NetworkMessage.ulCurrentSize );
}

//*****************************************************************************
//
NETADDRESS_s NETWORK_GetFromAddress( void )
{
	return ( g_AddressFrom );
}

//*****************************************************************************
//
void NETWORK_LaunchPacket( NETBUFFER_s *pBuffer, NETADDRESS_s Address )
{
	LONG				lNumBytes;
	INT					iNumBytesOut = sizeof(g_ucHuffmanBuffer);

	pBuffer->ulCurrentSize = pBuffer->CalcSize();

	// Nothing to do.
	if ( pBuffer->ulCurrentSize == 0 )
		return;

	// Convert the IP address to a socket address.
	struct sockaddr_in SocketAddress = Address.ToSocketAddress();

	HUFFMAN_Encode( (unsigned char *)pBuffer->pbData, g_ucHuffmanBuffer, pBuffer->ulCurrentSize, &iNumBytesOut );

	lNumBytes = sendto( g_NetworkSocket, (const char*)g_ucHuffmanBuffer, iNumBytesOut, 0, (struct sockaddr *)&SocketAddress, sizeof( SocketAddress ));

	// If sendto returns -1, there was an error.
	if ( lNumBytes == -1 )
	{
#ifdef __WIN32__
		INT	iError = WSAGetLastError( );

		// Wouldblock is silent.
		if ( iError == WSAEWOULDBLOCK )
			return;

		switch ( iError )
		{
		case WSAEACCES:

			printf( "NETWORK_LaunchPacket: Error #%d, WSAEACCES: Permission denied for address: %s\n", iError, Address.ToString() );
			return;
		case WSAEADDRNOTAVAIL:

			printf( "NETWORK_LaunchPacket: Error #%d, WSAEADDRENOTAVAIL: Address %s not available\n", iError, Address.ToString() );
			return;
		case WSAEHOSTUNREACH:

			printf( "NETWORK_LaunchPacket: Error #%d, WSAEHOSTUNREACH: Address %s unreachable\n", iError, Address.ToString() );
			return;				
		default:

			printf( "NETWORK_LaunchPacket: Error #%d\n", iError );
			return;
		}
#else
	if ( errno == EWOULDBLOCK )
return;

          if ( errno == ECONNREFUSED )
              return;

		printf( "NETWORK_LaunchPacket: %s\n", strerror( errno ));
		printf( "NETWORK_LaunchPacket: Address %s\n", Address.ToString() );

#endif
	}
}

//*****************************************************************************
//
NETADDRESS_s NETWORK_GetLocalAddress( void )
{
	char				szBuffer[512];
	struct sockaddr_in	SocketAddress;
	int					iNameLength;

#ifndef __WINE__
	gethostname( szBuffer, 512 );
#endif
	szBuffer[512-1] = 0;

	// Convert the host name to our local 
	NETADDRESS_s Address ( szBuffer );

	iNameLength = sizeof( SocketAddress );
#ifndef	WIN32
	if ( getsockname ( g_NetworkSocket, (struct sockaddr *)&SocketAddress, (socklen_t *)&iNameLength) == -1 )
#else
	if ( getsockname ( g_NetworkSocket, (struct sockaddr *)&SocketAddress, &iNameLength ) == -1 )
#endif
	{
		printf( "NETWORK_GetLocalAddress: Error getting socket name: %s", strerror( errno ));
	}

	Address.usPort = SocketAddress.sin_port;
	return ( Address );
}

//*****************************************************************************
//
NETBUFFER_s *NETWORK_GetNetworkMessageBuffer( void )
{
	return ( &g_NetworkMessage );
}

//*****************************************************************************
//
ULONG NETWORK_ntohs( ULONG ul )
{
	return ( ntohs( (u_short)ul ));
}

//*****************************************************************************
//
USHORT NETWORK_GetLocalPort( void )
{
	return ( g_usLocalPort );
}

//*****************************************************************************
//*****************************************************************************
//
void network_Error( const char *pszError )
{
	printf( "\\cd%s\n", pszError );
}

//*****************************************************************************
//
static SOCKET network_AllocateSocket( void )
{
	SOCKET	Socket;

	// Allocate a socket.
	Socket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( Socket == INVALID_SOCKET )
		network_Error( "network_AllocateSocket: Couldn't create socket!" );

	return ( Socket );
}

//*****************************************************************************
//
bool network_BindSocketToPort( SOCKET Socket, ULONG ulInAddr, USHORT usPort, bool bReUse )
{
	int		iErrorCode;
	struct sockaddr_in address;

	// setsockopt needs an int, bool won't work
	int		enable = 1;

	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = ulInAddr;
	address.sin_port = htons( usPort );

	// Allow the network socket to broadcast.
	setsockopt( Socket, SOL_SOCKET, SO_BROADCAST, (const char *)&enable, sizeof( enable ));
	if ( bReUse )
		setsockopt( Socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof( enable ));

	iErrorCode = bind( Socket, (sockaddr *)&address, sizeof( address ));
	if ( iErrorCode == SOCKET_ERROR )
		return ( false );

	return ( true );
}


#ifndef	WIN32
extern int	stdin_ready;
extern int	do_stdin;
#endif

// [BB] We only need this for the server console input under Linux.
void I_DoSelect (void)
{
#ifdef		WIN32
	// [BC] We need this code here to be executed. The point of this function is to
	// make the thread sleep until a packet is received. That way, the thread doesn't
	// use 100% of the CPU.
    struct timeval   timeout;
    fd_set           fdset;

    FD_ZERO(&fdset);
    FD_SET(g_NetworkSocket, &fdset);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (select (static_cast<int>(g_NetworkSocket)+1, &fdset, NULL, NULL, &timeout) == -1)
        return;
#else
    struct timeval   timeout;
    fd_set           fdset;

    FD_ZERO(&fdset);
    if (do_stdin)
    	FD_SET(0, &fdset);

    FD_SET(g_NetworkSocket, &fdset);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (select (static_cast<int>(g_NetworkSocket)+1, &fdset, NULL, NULL, &timeout) == -1)
        return;

    stdin_ready = FD_ISSET(0, &fdset);
#endif
} 
