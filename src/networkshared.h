//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Brad Carney, Benjamin Berkels
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
// Filename: networkshared.h
//
// Description: Contains shared network code shared between Skulltag and its satellites (master server, statsmaker, rcon utility, etc).
//
//-----------------------------------------------------------------------------

#ifndef __NETWORKSHARED_H__
#define __NETWORKSHARED_H__

#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <list>
#include <time.h>
#include <ctype.h>
#include <math.h>

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- DEFINES ---------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

// Maximum size of the packets sent out by the server.
#define	MAX_UDP_PACKET				8192

// [BB] Number of packets that are stored to recover from packet loss.
#define PACKET_BUFFER_SIZE			2048

//*****************************************************************************
enum BUFFERTYPE_e
{
	BUFFERTYPE_READ,
	BUFFERTYPE_WRITE,

};

//*****************************************************************************
enum
{
	MSC_BEGINSERVERLIST,
	MSC_SERVER,
	MSC_ENDSERVERLIST,
	MSC_IPISBANNED,
	MSC_REQUESTIGNORED,
	MSC_WRONGVERSION,
	MSC_BEGINSERVERLISTPART,
	MSC_ENDSERVERLISTPART,
	MSC_SERVERBLOCK,

};

//*****************************************************************************
enum
{
	// Server is letting master server of its existence.
	SERVER_MASTER_CHALLENGE = 5660020,

	// [RC] This is no longer used.
	/*
		// Server is letting master server of its existence, along with sending an IP the master server
		// should use for this server.
		SERVER_MASTER_CHALLENGE_OVERRIDE = 5660021,
	*/

	// Server is sending some statistics to the master server.
	SERVER_MASTER_STATISTICS = 5660022,

	// Server is sending its info to the launcher.
	SERVER_LAUNCHER_CHALLENGE,

	// Server is telling a launcher that it's ignoring it.
	SERVER_LAUNCHER_IGNORING,

	// Server is telling a launcher that his IP is banned from the server.
	SERVER_LAUNCHER_BANNED,

	// Client is trying to create a new account with the master server.
	CLIENT_MASTER_NEWACCOUNT,

	// Client is trying to log in with the master server.
	CLIENT_MASTER_LOGIN,

	// [BB] Launcher is querying the master server for a full server list, possibly split into several packets.
	LAUNCHER_MASTER_CHALLENGE,

	// [BB] Server is answering a MasterBanlistVerificationString verification request.
	SERVER_MASTER_VERIFICATION,

	// [BB] Server is acknowledging the receipt of a ban list.
	SERVER_MASTER_BANLIST_RECEIPT,
};

// [BB] Protocol version of the master server, currently only used in conjunction with LAUNCHER_MASTER_CHALLENGE.
#define MASTER_SERVER_VERSION		2

// Launcher is querying the server, or master server.
#define	LAUNCHER_SERVER_CHALLENGE	199

enum
{
	// Master server is sending its banlist to a server.
	MASTER_SERVER_BANLIST = 205,

	// [BB] Master is asking the server to verify its MasterBanlistVerificationString.
	MASTER_SERVER_VERIFICATION,

	// [BB] Master server is sending a part of its banlist to a server.
	MASTER_SERVER_BANLISTPART,
};

// [BB] Various enums used in MASTER_SERVER_BANLISTPART packets.
enum
{
	MSB_BAN,
	MSB_BANEXEMPTION,
	MSB_ENDBANLISTPART,
	MSB_ENDBANLIST,
};

#define	DEFAULT_SERVER_PORT			10666
#define	DEFAULT_CLIENT_PORT			10667
#define	DEFAULT_MASTER_PORT			15300
#define	DEFAULT_BROADCAST_PORT		15101
#define	DEFAULT_STATS_PORT			15201

// This is the longest possible string we can pass over the network.
#define	MAX_NETWORK_STRING			2048

//*****************************************************************************
// [BB] Allows to easily use char[4][4] references as function arguments.
typedef char IPStringArray[4][4];

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- STRUCTURES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//*****************************************************************************
struct NETADDRESS_s
{
	// Four digit IP address.
	BYTE		abIP[4];

	// The IP address's port extension.
	USHORT		usPort;

	// What's this for?
	USHORT		usPad;

	NETADDRESS_s();
	explicit NETADDRESS_s ( const char* string, bool* ok = NULL );

	bool Compare ( const NETADDRESS_s& other, bool ignorePort = false ) const;
	bool CompareNoPort ( const NETADDRESS_s& other ) const { return Compare( other, true ); }
	const char* ToHostName() const;
	void ToIPStringArray ( IPStringArray& address ) const;
	struct sockaddr_in ToSocketAddress() const;
	void SetPort ( USHORT port );
	const char* ToString() const;
	const char* ToStringNoPort() const;
	bool LoadFromString( const char* string );
	void LoadFromSocketAddress ( const struct sockaddr_in& sockaddr );

private:
	bool operator==( const NETADDRESS_s& );
	bool operator!=( const NETADDRESS_s& );
};

//*****************************************************************************
struct IPADDRESSBAN_s
{
	// The IP address in char form (can be a number or a wildcard).
	char		szIP[4][4];

	// Comment regarding the banned address.
	char		szComment[128];

	// [RC] Time that the ban expires, or NULL for an infinite ban.
	time_t		tExpirationDate;

};

//*****************************************************************************
struct BYTESTREAM_s
{
	BYTESTREAM_s();
	void EnsureBitSpace( int bits, bool writing );

	// Pointer to our stream of data.
	BYTE		*pbStream;

	// Pointer to the end of the stream. When pbStream > pbStreamEnd, the
	// entire stream has been read.
	BYTE		*pbStreamEnd;

	BYTE		*bitBuffer;
	int			bitShift;

#ifdef CREATE_PACKET_LOG
	// [RC] Pointer to the start of the stream.
	BYTE		*pbStreamBeginning;

	// [RC] Whether or not we've logged this.
	bool		bPacketAlreadyLogged;
#endif
};


//*****************************************************************************
struct NETBUFFER_s
{
	// This is the data in our packet.
	BYTE			*pbData;

	// The maximum amount of data this packet can hold.
	ULONG			ulMaxSize;

	// How much data is currently in this packet?
	ULONG			ulCurrentSize;

	// Byte stream for this buffer for managing our current position and where
	// the end of the stream is.
	BYTESTREAM_s	ByteStream;

	// Is this a buffer that we write to, or read from?
	BUFFERTYPE_e	BufferType;

	NETBUFFER_s ( );
	NETBUFFER_s ( const NETBUFFER_s &Buffer );

	void			Init( ULONG ulLength, BUFFERTYPE_e BufferType );
	void			Free();
	void			Clear();
	LONG			CalcSize() const;
	LONG			WriteTo( BYTESTREAM_s &ByteStream ) const;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- PROTOTYPES ------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

void			NETWORK_WriteBuffer( BYTESTREAM_s *pByteStream, const void *pvBuffer, int nLength );

void			NETWORK_StartTrafficMeasurement ( );
int				NETWORK_StopTrafficMeasurement ( );

int				NETWORK_ReadByte( BYTESTREAM_s *pByteStream );
int				NETWORK_ReadShort( BYTESTREAM_s *pByteStream );
int				NETWORK_ReadLong( BYTESTREAM_s *pByteStream );
float			NETWORK_ReadFloat( BYTESTREAM_s *pByteStream );
const char		*NETWORK_ReadString( BYTESTREAM_s *pByteStream );
bool			NETWORK_ReadBit( BYTESTREAM_s *byteStream );
int				NETWORK_ReadVariable( BYTESTREAM_s *byteStream );
int				NETWORK_ReadShortByte ( BYTESTREAM_s* byteStream, int bits );
void			NETWORK_WriteByte( BYTESTREAM_s *pByteStream, int Byte );
void			NETWORK_WriteShort( BYTESTREAM_s *pByteStream, int Short );
void			NETWORK_WriteLong( BYTESTREAM_s *pByteStream, int Long );
void			NETWORK_WriteFloat( BYTESTREAM_s *pByteStream, float Float );
void			NETWORK_WriteString( BYTESTREAM_s *pByteStream, const char *pszString );
void			NETWORK_WriteBit( BYTESTREAM_s *byteStream, bool bit );
void			NETWORK_WriteVariable( BYTESTREAM_s *byteStream, int value );
void			NETWORK_WriteShortByte( BYTESTREAM_s *byteStream, int value, int bits );

void			NETWORK_WriteHeader( BYTESTREAM_s *pByteStream, int Byte );
bool			NETWORK_StringToIP( const char *pszAddress, char *pszIP0, char *pszIP1, char *pszIP2, char *pszIP3 );

//--------------------------------------------------------------------------------------------------------------------------------------------------
//-- CLASSES ---------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------

//==========================================================================
//
// IPFileParser
//
// Reads a list of IPs from a file (ie, a banlist). Supports wildcards and comments.
// @author Benjamin Berkels, Brad Carney
//
//==========================================================================

class IPFileParser
{
	const unsigned int	_listLength;
	ULONG				_numberOfEntries;
	char				_errorMessage[1024];

//*************************************************************************
public:
	IPFileParser( const int IPListLength ) : _listLength( IPListLength )
	{
		_errorMessage[0] = '\0';
	}
		
	const char* getErrorMessage( )
	{
		return _errorMessage;
	}

	ULONG getNumberOfEntries ( )
	{
		return _numberOfEntries;
	}

	bool parseIPList( const char* FileName, std::vector<IPADDRESSBAN_s> &IPArray );

//*************************************************************************
private:
	char		skipWhitespace( FILE *pFile );
	char		skipComment( FILE *pFile );
	void		readReason( FILE *pFile, char *Reason, const int MaxReasonLength );
	time_t		readExpirationDate( FILE *pFile );
	bool		parseNextLine( FILE *pFile, IPADDRESSBAN_s &IP, ULONG &BanIdx );
};

//==========================================================================
//
// IPList
//
// Stores a list of IPs. Supports wildcards.
// @author Benjamin Berkels
//
//==========================================================================

class IPList
{
	std::vector<IPADDRESSBAN_s>		_ipVector;
	std::string						_filename;
	std::string						_error;

//*************************************************************************
public:
	bool			clearAndLoadFromFile( const char *Filename );
	ULONG			getFirstMatchingEntryIndex( const IPStringArray &szAddress ) const;
	ULONG			getFirstMatchingEntryIndex( const NETADDRESS_s &Address ) const;
	bool			isIPInList( const IPStringArray &szAddress ) const;
	bool			isIPInList( const NETADDRESS_s &Address ) const;
	ULONG			doesEntryExist( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3 ) const;
	IPADDRESSBAN_s	getEntry( const ULONG ulIdx ) const;
	std::string		getEntryAsString( const ULONG ulIdx, bool bIncludeComment = true, bool bIncludeExpiration = true, bool bInludeNewline = true ) const;
	ULONG			getEntryIndex( const NETADDRESS_s &Address ) const; // [RC]
	const char		*getEntryComment( const NETADDRESS_s &Address ) const; // [RC]
	time_t			getEntryExpiration( const NETADDRESS_s &Address ) const; // [RC]
	void			addEntry( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3, const char *pszPlayerName, const char *pszComment, std::string &Message, time_t tExpiration );
	void			addEntry( const char *pszIPAddress, const char *pszPlayerName, const char *pszComment, std::string &Message, time_t tExpiration );
	void			removeEntry( const char *pszIP0, const char *pszIP1, const char *pszIP2, const char *pszIP3, std::string &Message );
	void			removeEntry( const char *pszIPAddress, std::string &Message );
	void			removeEntry( ULONG ulEntryIdx ); // [RC]
	void			copy( IPList &destination ); // [RC]
	void			sort(); // [RC]
	void			removeExpiredEntries( void ); // [RC]

	unsigned int	size() const { return static_cast<unsigned int>( _ipVector.size( )); }
	void			clear() { _ipVector.clear(); }
	void			push_back ( IPADDRESSBAN_s &IP ) { _ipVector.push_back(IP); }
	const char*		getErrorMessage() const { return _error.c_str(); }
	
	std::vector<IPADDRESSBAN_s>&	getVector() { return _ipVector; }

//*************************************************************************
private:
	bool rewriteListToFile ();
};

//==========================================================================
//
// QueryIPQueue
//
// Stores IPs that have recently queried us to prevent flooding.
// @author Benjamin Berkels, Rivecoder
//
//==========================================================================

class QueryIPQueue
{
	//*************************************************************************
	struct STORED_QUERY_IP_t
	{
		// The IP address.
		NETADDRESS_s		Address;

		// Expiration date.
		long				lNextAllowedTime;

	};

	// The maximum number of entries that we can store.
	static const unsigned int	MAX_QUERY_IPS = 512;

	// The array of IPs.
	STORED_QUERY_IP_t			_IPQueue[MAX_QUERY_IPS];

	// Head and tail of the queue.
	unsigned int				_iQueueHead;
	unsigned int				_iQueueTail;

	// How long entries will last (seconds).
	unsigned int				_iEntryLength;

//*************************************************************************
public:
	QueryIPQueue( int iEntryLength ) : _iQueueHead( 0 ), _iQueueTail( 0 ), _iEntryLength( iEntryLength )
	{
	}

	void	adjustHead( const LONG CurrentTime );
	bool	addressInQueue( const NETADDRESS_s AddressFrom ) const;
	void	addAddress( const NETADDRESS_s AddressFrom, const LONG lCurrentTime, std::ostream *errorOut = NULL );
	bool	isFull( ) const;
};

//==========================================================================
//
// RingBuffer
//
// @author Benjamin Berkels
//
//==========================================================================
template<typename DataType, int Length>
class RingBuffer
{
	DataType _data[Length];
	// Position of the entry that will be overwritten next AKA the oldest entry.
	unsigned int _position;
public:
	RingBuffer ( )
	{
		clear();
	}
	void clear ( )
	{
		memset( _data, 0, sizeof( _data ));
		_position = 0;
	}
	void put ( DataType Entry )
	{
		_data[_position] = Entry;
		_position = (_position+1) % Length;
	}
	DataType getOldestEntry ( unsigned int Offset = 0 ) const
	{
		return _data[ ( _position + Offset ) % Length ];
	}
};

#endif	// __NETWORKSHARED_H__
