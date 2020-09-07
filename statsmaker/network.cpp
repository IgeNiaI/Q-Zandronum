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
#include "network.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ctype.h>
#include <math.h>

#include "../src/huffman/huffman.h"
#include "main.h"

//*****************************************************************************
//	VARIABLES

// Buffer that holds the data from the most recently received packet.
static	NETBUFFER_s		g_NetworkMessage;

// Network address that the most recently received packet came from.
static	NETADDRESS_s	g_AddressFrom;

// Our network socket.
static	SOCKET			g_NetworkSocket;

// Socket for listening for LAN games.
static	SOCKET			g_LANSocket;

// Our local port.
static	USHORT			g_usLocalPort;

// Buffer for the Huffman encoding.
static	UCHAR			g_ucHuffmanBuffer[131072];

//*****************************************************************************
//	PROTOTYPES

static	void			network_Error( char *pszError );
static	SOCKET			network_AllocateSocket( void );
static	bool			network_BindSocketToPort( SOCKET Socket, USHORT usPort, bool bReUse );

//*****************************************************************************
//	FUNCTIONS

void NETWORK_Construct( USHORT usPort, bool bAllocateLANSocket )
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

	Printf( "Winsock initialization succeeded!\n" );
#endif

	g_usLocalPort = usPort;

	// Allocate a socket, and attempt to bind it to the given port.
	g_NetworkSocket = network_AllocateSocket( );
	if ( network_BindSocketToPort( g_NetworkSocket, g_usLocalPort, false ) == false )
	{
		bSuccess = true;
		usNewPort = g_usLocalPort;
		while ( network_BindSocketToPort( g_NetworkSocket, ++usNewPort, false ) == false )
		{
			// Didn't find an available port. Oh well...
			if ( usNewPort == g_usLocalPort )
			{
				usNewPort = false;
				break;
			}
		}

		if ( bSuccess == false )
		{
			sprintf( szString, "NETWORK_Construct: Couldn't bind socket to port: %d\n", g_usLocalPort );
			network_Error( szString );
		}
		else
		{
			Printf( "NETWORK_Construct: Couldn't bind to %d. Binding to %d instead...\n", g_usLocalPort, usNewPort );
			g_usLocalPort = usNewPort;
		}
	}

	ulArg = true;
	if ( ioctlsocket( g_NetworkSocket, FIONBIO, &ulArg ) == -1 )
		printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));

	// If we're not starting a server, setup a socket to listen for LAN servers.
	if ( bAllocateLANSocket )
	{
		g_LANSocket = network_AllocateSocket( );
		if ( network_BindSocketToPort( g_LANSocket, DEFAULT_BROADCAST_PORT, true ) == false )
		{
			sprintf( szString, "network_BindSocketToPort: Couldn't bind LAN socket to port: %d. You will not be able to see LAN servers in the browser.", DEFAULT_BROADCAST_PORT );
			network_Error( szString );
		}

		if ( ioctlsocket( g_LANSocket, FIONBIO, &ulArg ) == -1 )
			printf( "network_AllocateSocket: ioctl FIONBIO: %s", strerror( errno ));
	}

	// Init our read buffer.
	NETWORK_InitBuffer( &g_NetworkMessage, MAX_UDP_PACKET, BUFFERTYPE_READ );
	NETWORK_ClearBuffer( &g_NetworkMessage );

	// Print out our local IP address.
	LocalAddress = NETWORK_GetLocalAddress( );
	Printf( "IP address %s\n", NETWORK_AddressToString( LocalAddress ));

	// Call NETWORK_Destruct() when Skulltag closes.
	atexit( NETWORK_Destruct );

	Printf( "UDP Initialized.\n" );
}

//*****************************************************************************
//
void NETWORK_Destruct( void )
{
	// Free the network message buffer.
	NETWORK_FreeBuffer( &g_NetworkMessage );
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
            return false;

		// Connection reset by peer. Doesn't mean anything to the server.
		if ( errno == WSAECONNRESET )
			return false;

        if ( errno == WSAEMSGSIZE )
		{
             Printf( "NETWORK_GetPackets:  WARNING! Oversize packet from %s\n", NETWORK_AddressToString( g_AddressFrom ));
             return false;
        }

        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
		return false;
#else
        if ( errno == EWOULDBLOCK )
            return false;

        if ( errno == ECONNREFUSED )
            return false;

        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
        return false;
#endif
    }

	// No packets or an error, so don't process anything.
	if ( lNumBytes <= 0 )
		return ( 0 );

	// If the number of bytes we're receiving exceeds our buffer size, ignore the packet.
	if ( lNumBytes >= static_cast<LONG>(g_NetworkMessage.ulMaxSize) )
		return ( 0 );

	// Decode the huffman-encoded message we received.
//	HUFFMAN_Decode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, lNumBytes, &iDecodedNumBytes );
	memcpy( g_NetworkMessage.pbData, g_ucHuffmanBuffer, lNumBytes );
	g_NetworkMessage.ulCurrentSize = lNumBytes;
	g_NetworkMessage.ByteStream.pbStream = g_NetworkMessage.pbData;
	g_NetworkMessage.ByteStream.pbStreamEnd = g_NetworkMessage.ByteStream.pbStream + g_NetworkMessage.ulCurrentSize;

	// Store the IP address of the sender.
    NETWORK_SocketAddressToNetAddress( &SocketFrom, &g_AddressFrom );

	return ( g_NetworkMessage.ulCurrentSize );
}

//*****************************************************************************
//
void NETWORK_DecodePacket( void )
{
	INT		iDecodedNumBytes = sizeof(g_ucHuffmanBuffer);

	memcpy( g_ucHuffmanBuffer, g_NetworkMessage.pbData, g_NetworkMessage.ulCurrentSize );

	HUFFMAN_Decode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, g_NetworkMessage.ulCurrentSize, &iDecodedNumBytes );

	g_NetworkMessage.ulCurrentSize = iDecodedNumBytes;
	g_NetworkMessage.ByteStream.pbStream = g_NetworkMessage.pbData;
	g_NetworkMessage.ByteStream.pbStreamEnd = g_NetworkMessage.ByteStream.pbStream + g_NetworkMessage.ulCurrentSize;
}

//*****************************************************************************
//
int NETWORK_GetLANPackets( void )
{
	LONG				lNumBytes;
	INT					iDecodedNumBytes = sizeof(g_ucHuffmanBuffer);
	struct sockaddr_in	SocketFrom;
	INT					iSocketFromLength;

    iSocketFromLength = sizeof( SocketFrom );

#ifdef	WIN32
	lNumBytes = recvfrom( g_LANSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, &iSocketFromLength );
#else
	lNumBytes = recvfrom( g_LANSocket, (char *)g_ucHuffmanBuffer, sizeof( g_ucHuffmanBuffer ), 0, (struct sockaddr *)&SocketFrom, (socklen_t *)&iSocketFromLength );
#endif

	// If the number of bytes returned is -1, an error has occured.
    if ( lNumBytes == -1 ) 
    { 
#ifdef __WIN32__
        errno = WSAGetLastError( );

        if ( errno == WSAEWOULDBLOCK )
            return false;

		// Connection reset by peer. Doesn't mean anything to the server.
		if ( errno == WSAECONNRESET )
			return false;

        if ( errno == WSAEMSGSIZE )
		{
             Printf( "NETWORK_GetPackets:  WARNING! Oversize packet from %s\n", NETWORK_AddressToString( g_AddressFrom ));
             return false;
        }

        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
		return false;
#else
        if ( errno == EWOULDBLOCK )
            return false;

        if ( errno == ECONNREFUSED )
            return false;

        Printf( "NETWORK_GetPackets: WARNING!: Error #%d: %s\n", errno, strerror( errno ));
        return false;
#endif
    }

	// No packets or an error, dont process anything.
	if ( lNumBytes <= 0 )
		return ( 0 );

	// If the number of bytes we're receiving exceeds our buffer size, ignore the packet.
	if ( lNumBytes >= static_cast<LONG>(g_NetworkMessage.ulMaxSize) )
		return ( 0 );

	// Decode the huffman-encoded message we received.
	HUFFMAN_Decode( g_ucHuffmanBuffer, (unsigned char *)g_NetworkMessage.pbData, lNumBytes, &iDecodedNumBytes );
	g_NetworkMessage.ulCurrentSize = iDecodedNumBytes;
	g_NetworkMessage.ByteStream.pbStream = g_NetworkMessage.pbData;
	g_NetworkMessage.ByteStream.pbStreamEnd = g_NetworkMessage.ByteStream.pbStream + g_NetworkMessage.ulCurrentSize;

	// Store the IP address of the sender.
    NETWORK_SocketAddressToNetAddress( &SocketFrom, &g_AddressFrom );

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
void NETWORK_LaunchPacket( NETBUFFER_s *pBuffer, NETADDRESS_s Address, bool bEncode )
{
	LONG				lNumBytes;
	INT					iNumBytesOut = sizeof(g_ucHuffmanBuffer);
	struct sockaddr_in	SocketAddress;

	pBuffer->ulCurrentSize = NETWORK_CalcBufferSize( pBuffer );

	// Nothing to do.
	if ( pBuffer->ulCurrentSize == 0 )
		return;

	// Convert the IP address to a socket address.
	NETWORK_NetAddressToSocketAddress( Address, SocketAddress );

	if ( bEncode )
		HUFFMAN_Encode( (unsigned char *)pBuffer->pbData, g_ucHuffmanBuffer, pBuffer->ulCurrentSize, &iNumBytesOut );
	else
	{
		memcpy( g_ucHuffmanBuffer, pBuffer->pbData, pBuffer->ulCurrentSize );
		iNumBytesOut = pBuffer->ulCurrentSize;
	}

	lNumBytes = sendto( g_NetworkSocket, (const char*)g_ucHuffmanBuffer, iNumBytesOut, 0, (struct sockaddr *)&SocketAddress, sizeof( SocketAddress ));

	// If sendto returns -1, there was an error.
	if ( lNumBytes == -1 )
	{
#ifdef __WIN32__
		INT	iError = WSAGetLastError( );

		// Wouldblock is silent.
		if ( iError == WSAEWOULDBLOCK || iError == 10004 )
			return;

		switch ( iError )
		{
		case WSAEACCES:

			Printf( "NETWORK_LaunchPacket: Error #%d, WSAEACCES: Permission denied for address: %s\n", iError, NETWORK_AddressToString( Address ));
			return;
		case WSAEADDRNOTAVAIL:

			Printf( "NETWORK_LaunchPacket: Error #%d, WSAEADDRENOTAVAIL: Address %s not available\n", iError, NETWORK_AddressToString( Address ));
			return;
		case WSAEHOSTUNREACH:

			Printf( "NETWORK_LaunchPacket: Error #%d, WSAEHOSTUNREACH: Address %s unreachable\n", iError, NETWORK_AddressToString( Address ));
			return;				
		default:

			Printf( "NETWORK_LaunchPacket: Error #%d\n", iError );
			return;
		}
#else
	if ( errno == EWOULDBLOCK )
return;

          if ( errno == ECONNREFUSED )
              return;

		Printf( "NETWORK_LaunchPacket: %s\n", strerror( errno ));
		Printf( "NETWORK_LaunchPacket: Address %s\n", NETWORK_AddressToString( Address ));

#endif
	}
}

//*****************************************************************************
//
char *NETWORK_AddressToString( NETADDRESS_s Address )
{
	static char	s_szAddress[64];

	sprintf( s_szAddress, "%i.%i.%i.%i:%i", Address.abIP[0], Address.abIP[1], Address.abIP[2], Address.abIP[3], ntohs( Address.usPort ));

	return ( s_szAddress );
}

//*****************************************************************************
//
char *NETWORK_AddressToStringIgnorePort( NETADDRESS_s Address )
{
	static char	s_szAddress[64];

	sprintf( s_szAddress, "%i.%i.%i.%i", Address.abIP[0], Address.abIP[1], Address.abIP[2], Address.abIP[3] );

	return ( s_szAddress );
}

//*****************************************************************************
//
void NETWORK_SetAddressPort( NETADDRESS_s &Address, USHORT usPort )
{
	Address.usPort = htons( usPort );
}

//*****************************************************************************
//
NETADDRESS_s NETWORK_GetLocalAddress( void )
{
	char				szBuffer[512];
	struct sockaddr_in	SocketAddress;
	NETADDRESS_s		Address;
	int					iNameLength;

#ifndef __WINE__
	gethostname( szBuffer, 512 );
#endif
	szBuffer[512-1] = 0;

	// Convert the host name to our local 
	NETWORK_StringToAddress( szBuffer, &Address );

	iNameLength = sizeof( SocketAddress );
#ifndef	WIN32
	if ( getsockname ( g_NetworkSocket, (struct sockaddr *)&SocketAddress, (socklen_t *)&iNameLength) == -1 )
#else
	if ( getsockname ( g_NetworkSocket, (struct sockaddr *)&SocketAddress, &iNameLength ) == -1 )
#endif
	{
		Printf( "NETWORK_GetLocalAddress: Error getting socket name: %s", strerror( errno ));
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
void network_Error( char *pszError )
{
	Printf( "\\cd%s\n", pszError );
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
bool network_BindSocketToPort( SOCKET Socket, USHORT usPort, bool bReUse )
{
	int		iErrorCode;
	struct sockaddr_in address;

	// setsockopt needs an int, bool won't work
	int		enable = 1;

	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( usPort );

	// Allow the network socket to broadcast.
	setsockopt( Socket, SOL_SOCKET, SO_BROADCAST, (const char *)&enable, sizeof( enable ));
	if ( bReUse )
		setsockopt( Socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof( enable ));

	iErrorCode = bind( Socket, (sockaddr *)&address, sizeof( address ));
	if ( iErrorCode == SOCKET_ERROR )
		return false;

	return true;
}


#ifndef	WIN32
extern int	stdin_ready;
extern int	do_stdin;
#endif

// [BB] We only need this for the server console input under Linux.
void I_DoSelect (void)
{
#ifdef		WIN32
/*
    struct timeval   timeout;
    fd_set           fdset;

    FD_ZERO(&fdset);
    FD_SET(g_NetworkSocket, &fdset);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (select (static_cast<int>(g_NetworkSocket)+1, &fdset, NULL, NULL, &timeout) == -1)
        return;
*/
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
