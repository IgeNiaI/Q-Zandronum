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
// Filename: netcommand.h
//
//-----------------------------------------------------------------------------

#pragma once
#include "network_enums.h"
#include "sv_commands.h"

/**
 * \brief Iterate over all clients, possibly skipping one or all but one.
 *
 * \author Benjamin Berkels
 */
class ClientIterator {
	const ULONG _ulPlayerExtra;
	const ServerCommandFlags _flags;
	ULONG _current;

	bool isCurrentValid ( ) const;
	void incremntCurrentTillValid ( );

public:
	ClientIterator ( const ULONG ulPlayerExtra = MAXPLAYERS, const ServerCommandFlags flags = 0 );

	inline bool notAtEnd ( ) const {
		return ( _current < MAXPLAYERS );
	}

	ULONG operator* ( ) const;
	ULONG operator++ ( );
};

/**
 * \brief Creates and sends network commands to the clients.
 *
 * \author Benjamin Berkels
 */
class NetCommand {
	NETBUFFER_s	_buffer;
	bool		_unreliable;

public:
	NetCommand ( const SVC Header );
	NetCommand ( const SVC2 Header2 );
	~NetCommand ( );

	const char *getHeaderAsString() const;

	void addInteger( const int IntValue, const int Size );
	void addByte ( const int ByteValue );
	void addShort ( const int ShortValue );
	void addLong ( const SDWORD LongValue );
	void addFloat ( const float FloatValue );
	void addString ( const char *pszString );
	void addName ( FName name );
	void addBit ( const bool value );
	void addVariable ( const int value );
	void addShortByte ( int value, int bits );
	void writeCommandToStream ( BYTESTREAM_s &ByteStream ) const;
	NETBUFFER_s& getBufferForClient( ULONG i ) const;
	BYTESTREAM_s& getBytestreamForClient( ULONG i ) const;
	void sendCommandToClients ( ULONG ulPlayerExtra = MAXPLAYERS, ServerCommandFlags flags = 0 );
	void sendCommandToOneClient( ULONG i );
	bool isUnreliable() const;
	void setUnreliable ( bool a );
	int calcSize() const;
};
