//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2012 Benjamin Berkels
// Copyright (C) 2012 Skulltag Development Team
// Copyright (C) 2012 Zandronum Development Team
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
// 3. Neither the name of the Zandronum Development Team nor the names of its
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
// Filename: netcommand.cpp
//
//-----------------------------------------------------------------------------

#include "netcommand.h"

//*****************************************************************************
//
ClientIterator::ClientIterator ( const ULONG ulPlayerExtra, const ServerCommandFlags flags )
	: _ulPlayerExtra ( ulPlayerExtra ),
		_flags ( flags ),
		_current ( 0 )
{
	incremntCurrentTillValid();
}

//*****************************************************************************
//
bool ClientIterator::isCurrentValid ( ) const {
	if ( SERVER_IsValidClient( _current ) == false )
		return false;

	if ((( _flags & SVCF_SKIPTHISCLIENT ) && ( _ulPlayerExtra == _current )) ||
		(( _flags & SVCF_ONLYTHISCLIENT ) && ( _ulPlayerExtra != _current )))
	{
		return false;
	}

	if ( ( _flags & SVCF_ONLY_CONNECTIONTYPE_0 ) && ( players[_current].userinfo.GetConnectionType() != 0 ) )
		return false;

	if ( ( _flags & SVCF_ONLY_CONNECTIONTYPE_1 ) && ( players[_current].userinfo.GetConnectionType() != 1 ) )
		return false;

	return true;
}

//*****************************************************************************
//
void ClientIterator::incremntCurrentTillValid ( ) {
	while ( ( isCurrentValid() == false ) && notAtEnd() )
		++_current;
}

//*****************************************************************************
//
ULONG ClientIterator::operator* ( ) const {
	return ( _current );
}

//*****************************************************************************
//
ULONG ClientIterator::operator++ ( ) {
	++_current;
	incremntCurrentTillValid();
	return ( _current );
}

//*****************************************************************************
//
NetCommand::NetCommand ( const SVC Header ) :
	_unreliable( false )
{
	_buffer.Init( MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	_buffer.Clear();
	addByte( Header );
}

//*****************************************************************************
//
NetCommand::NetCommand ( const SVC2 Header2 ) :
	_unreliable( false )
{
	_buffer.Init( MAX_UDP_PACKET, BUFFERTYPE_WRITE );
	_buffer.Clear();
	addByte( SVC_EXTENDEDCOMMAND );
	addByte( Header2 );
}

//*****************************************************************************
//
NetCommand::~NetCommand ( )
{
	_buffer.Free();
}

//*****************************************************************************
//
const char *NetCommand::getHeaderAsString() const
{
	const SVC header = static_cast<SVC>( _buffer.pbData[0] );
	if ( header != SVC_EXTENDEDCOMMAND )
		return GetStringSVC( header );

	return GetStringSVC2( static_cast<SVC2>( _buffer.pbData[1] ) );
}

//*****************************************************************************
//
void NetCommand::addInteger( const int IntValue, const int Size )
{
	if ( ( _buffer.ByteStream.pbStream + Size ) > _buffer.ByteStream.pbStreamEnd )
	{
		Printf( "NetCommand::AddInteger: Overflow! Header: %s\n", getHeaderAsString() );
		return;
	}

	for ( int i = 0; i < Size; ++i )
		_buffer.ByteStream.pbStream[i] = ( IntValue >> ( 8*i ) ) & 0xff;

	_buffer.ByteStream.pbStream += Size;
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
//
void NetCommand::addByte ( const int ByteValue )
{
	addInteger( static_cast<BYTE> ( ByteValue ), sizeof( BYTE ) );
}

//*****************************************************************************
//
void NetCommand::addShort ( const int ShortValue )
{
	addInteger( static_cast<SWORD> ( ShortValue ), sizeof( SWORD ) );
}

//*****************************************************************************
//
void NetCommand::addLong ( const SDWORD LongValue )
{
	addInteger( LongValue, sizeof( SDWORD ) );
}

//*****************************************************************************
//
void NetCommand::addFloat ( const float FloatValue )
{
	union
	{
		float	f;
		SDWORD	l;
	} dat;
	dat.f = FloatValue;
	addInteger ( dat.l, sizeof( SDWORD ) );
}

//*****************************************************************************
//
void NetCommand::addBit( const bool value )
{
	NETWORK_WriteBit( &_buffer.ByteStream, value );
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
//
void NetCommand::addVariable( const int value )
{
	NETWORK_WriteVariable( &_buffer.ByteStream, value );
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
// [TP]
//
void NetCommand::addShortByte ( int value, int bits )
{
	NETWORK_WriteShortByte( &_buffer.ByteStream, value, bits );
	_buffer.ulCurrentSize = _buffer.CalcSize();
}

//*****************************************************************************
//
void NetCommand::addString ( const char *pszString )
{
	const int len = ( pszString != NULL ) ? strlen( pszString ) : 0;

	if ( len > MAX_NETWORK_STRING )
	{
		Printf( "NETWORK_WriteString: String exceeds %d characters! Header: %s\n", MAX_NETWORK_STRING , getHeaderAsString() );
		return;
	}

	for ( int i = 0; i < len; ++i )
		addByte( pszString[i] );
	addByte( 0 );
}

//*****************************************************************************
//
void NetCommand::addName ( FName name )
{
	if ( name.IsPredefined() )
	{
		addShort( name );
	}
	else
	{
		addShort( -1 );
		addString( name );
	}
}

//*****************************************************************************
//
void NetCommand::writeCommandToStream ( BYTESTREAM_s &ByteStream ) const
{
	// [BB] This also handles the traffic counting (NETWORK_StartTrafficMeasurement/NETWORK_StopTrafficMeasurement).
	_buffer.WriteTo ( ByteStream );
}

//*****************************************************************************
//
NETBUFFER_s& NetCommand::getBufferForClient( ULONG i ) const
{
	if ( _unreliable )
		return SERVER_GetClient( i )->UnreliablePacketBuffer;

	return SERVER_GetClient( i )->PacketBuffer;
}

//*****************************************************************************
// [TP]
//
BYTESTREAM_s& NetCommand::getBytestreamForClient( ULONG i ) const
{
	return getBufferForClient( i ).ByteStream;
}

//*****************************************************************************
//
void NetCommand::sendCommandToClients ( ULONG ulPlayerExtra, ServerCommandFlags flags )
{
	for ( ClientIterator it ( ulPlayerExtra, flags ); it.notAtEnd(); ++it )
		sendCommandToOneClient( *it );
}

//*****************************************************************************
//
void NetCommand::sendCommandToOneClient( ULONG i )
{
	SERVER_CheckClientBuffer( i, _buffer.ulCurrentSize, _unreliable == false );

	// [BB] 5 = 1 + 4 (SVC_HEADER + packet number)
	const unsigned int estimateSize = getBufferForClient( i ).CalcSize() + _buffer.ulCurrentSize + 5;
	if ( estimateSize >= SERVER_GetMaxPacketSize( ) )
	{
		// [BB] This should never happen.
		if ( getBufferForClient( i ).CalcSize() > 0 )
			SERVER_PrintWarning ( "NetCommand %s didn't create a new packet to client %lu even though the command doesn't fit within the current packet!\n", getHeaderAsString(), i );
		// [BB] This happens if the current command alone is already too big for one packet.
		else
			SERVER_PrintWarning ( "NetCommand %s created a packet to client %lu exceeding sv_maxpacketsize (%d >= %lu)!\n", getHeaderAsString(), i, estimateSize, SERVER_GetMaxPacketSize( ));
	}

	writeCommandToStream( getBytestreamForClient( i ));
}

//*****************************************************************************
// [TP]
//
bool NetCommand::isUnreliable() const
{
	return _unreliable;
}

//*****************************************************************************
// [TP]
//
void NetCommand::setUnreliable ( bool a )
{
	_unreliable = a;
}

//*****************************************************************************
// [TP] Returns the size of this net command.
//
int NetCommand::calcSize() const
{
	return _buffer.CalcSize();
}
