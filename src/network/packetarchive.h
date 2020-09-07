//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2003 Brad Carney
// Copyright (C) 2016 Teemu Piippo
// Copyright (C) 2016 Zandronum Development Team
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
// Filename: packetarchive.h
//
// Description: Encapsulates the packet loss recovery routines
//
//-----------------------------------------------------------------------------

#pragma once
#include "../networkshared.h"

class PacketArchive
{
public:
	PacketArchive();
	~PacketArchive();

	void Initialize( size_t maxPacketSize );
	void Free();
	void Clear();
	unsigned int StorePacket( const NETBUFFER_s& packet );
	bool FindPacket( unsigned int packetNumber, const BYTE*& data, size_t& size ) const;

private:
	struct Record
	{
		size_t position; // The position of this packet within _packetData.
		size_t size; // The packet size of the stored packet.
		unsigned int sequenceNumber; // The corresponding sequence number of this packet.
	};

	// Buffer containing all data of the saved packets.
	NETBUFFER_s _packetData;

	// Last packet number sent to this client.
	unsigned int _sequenceNumber;

	// Records of all saved packets.
	Record _records[PACKET_BUFFER_SIZE];

	// Is this initialized or not?
	bool _initialized;
};

//==========================================================================
//
// OutgoingPacketBuffer
//
// @author Benjamin Berkels
//
//==========================================================================
class OutgoingPacketBuffer : public PacketArchive
{
	unsigned int _packetsSentThisTick;
	unsigned int _clientIdx;
	TArray<unsigned int> _scheduledPacketIndices;
	TArray<NETBUFFER_s> _unsentPackets;
private:
	bool SendPacket( unsigned int packetNumber, const NETADDRESS_s &Address ) const;
public:
	OutgoingPacketBuffer ( );
	void SetClientIndex ( const unsigned int ClientIdx );
	void ScheduleUnsentPacket ( const NETBUFFER_s &Packet );
	bool SchedulePacket( unsigned int packetNumber );
	void ClearScheduling();
	void ForceSendAll();
	void Clear();
	void Tick ( );
};
