//-----------------------------------------------------------------------------
//
// Skulltag Master Server Source
// Copyright (C) 2004-2005 Brad Carney
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
// Date (re)created:  3/7/05
//
//
// Filename: main.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef	__MAIN_H__
#define	__MAIN_H__

#ifndef _WIN32
typedef int32_t __int32;
#endif

//*****************************************************************************
//	DEFINES

// This is the maximum number of servers we can store in our list. Hopefully ST won't grow so big that this number can't hold them all!
#define	MAX_SERVERS						512

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// The IP address of this server.
	NETADDRESS_s	Address;

	// [BB] lLastReceived and bHasLatestBanList don't affect the ordering in the set we are going to
	// organize the servers in. Thus we can mark them as mutable, which in turn allows us to change
	// both values using the set iterators.

	// The last time we heard from this server (used for timeouts).
	mutable long	lLastReceived;

	// [BB] Does the server have the latest version of the holy banlist?
	mutable bool	bHasLatestBanList;

	// [BB] Did the server acknowledge the receipt the holy banlist?
	mutable bool	bVerifiedLatestBanList;

	// [BB] Does the server enforce the holy banlist locally?
	mutable bool	bEnforcesBanList;

	// [BB] Is the server using the latest Skulltag version
	bool	bNewFormatServer;

	// [BB] Code revision number the server was built with
	int		iServerRevision;

	// [BB] String the server has sent to use to verify ban list packets.
	std::string		MasterBanlistVerificationString;

	// [BB] Number that we send the server along with our verification request.
	__int32 ServerVerificationInt;

} SERVER_s;

#endif	// __MAIN_H__
