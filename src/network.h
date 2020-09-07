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
// Filename: network.h
//
// Description: Contains network definitions and functions not specifically
// related to the server or client.
//
//-----------------------------------------------------------------------------

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "c_cvars.h"
#include "d_player.h"
#include "i_net.h"
#include "p_setup.h"
#include "sv_main.h"
#include "tflags.h"

//*****************************************************************************
//	DEFINES

// Movement stuff.
enum
{
	CM_X			= 1 << 0,
	CM_Y			= 1 << 1,
	CM_Z			= 1 << 2,
	CM_ANGLE		= 1 << 3,
	CM_VELX			= 1 << 4,
	CM_VELY			= 1 << 5,
	CM_VELZ			= 1 << 6,
	CM_PITCH		= 1 << 7,
	CM_MOVEDIR		= 1 << 8,
	CM_REUSE_X		= 1 << 9,
	CM_REUSE_Y		= 1 << 10,
	CM_REUSE_Z		= 1 << 11,
	CM_LAST_X		= 1 << 12,
	CM_LAST_Y		= 1 << 13,
	CM_LAST_Z		= 1 << 14,
	CM_NOLAST		= 1 << 15, // [WS] We do not have enough room for more flags here.
};

// [BB] Flags for client_MovePlayer/SERVERCOMMANDS_MovePlayer
enum
{
	PLAYER_VISIBLE		= 1 << 0,
	PLAYER_ATTACK			= 1 << 1,
	PLAYER_ALTATTACK	= 1 << 2,
};

/* [BB] This is not used anywhere anymore.
// Should we use huffman compression?
#define	USE_HUFFMAN_COMPRESSION

// Extra player update info for spectators.
#define	PLAYER_UPDATE_WEAPON	1
#define	PLAYER_UPDATE_PITCH		2
*/

// Movement flags being sent by the client.
#define	CLIENT_UPDATE_YAW				0x01
#define	CLIENT_UPDATE_PITCH				0x02
#define	CLIENT_UPDATE_ROLL				0x04
#define	CLIENT_UPDATE_BUTTONS			0x08
#define	CLIENT_UPDATE_FORWARDMOVE		0x10
#define	CLIENT_UPDATE_SIDEMOVE			0x20
#define	CLIENT_UPDATE_UPMOVE			0x40
#define	CLIENT_UPDATE_BUTTONS_LONG		0x80

// [BC/BB] Flags to handle jumps in code pointers
enum ClientJumpUpdateFlag
{
	CLIENTUPDATE_FRAME			= 1,
	CLIENTUPDATE_POSITION		= 2,
	CLIENTUPDATE_SKIPPLAYER		= 4
};

typedef TFlags<ClientJumpUpdateFlag, unsigned int> ClientJumpUpdateFlags;
DEFINE_TFLAGS_OPERATORS (ClientJumpUpdateFlags)

// Identifying states (the cheap & easy way out)
enum NetworkActorState
{
	STATE_SPAWN = 1,
	STATE_SEE,
	STATE_PAIN,
	STATE_MELEE,
	STATE_MISSILE,
	STATE_DEATH,
	STATE_XDEATH,
	STATE_RAISE,
	STATE_HEAL,
	STATE_CRASH,
	STATE_IDLE,
	STATE_WOUND, // [Dusk]
	STATE_ARCHVILE_HEAL, // [EP]
};

// Identifying player states (again, cheap & easy)
enum PLAYERSTATE_e
{
	STATE_PLAYER_IDLE,
	STATE_PLAYER_SEE,
	STATE_PLAYER_ATTACK,
	STATE_PLAYER_ATTACK2,
	STATE_PLAYER_ATTACK_ALTFIRE,
};

// HUD message types.
#define	HUDMESSAGETYPE_NORMAL			1
#define	HUDMESSAGETYPE_FADEOUT			2
#define	HUDMESSAGETYPE_TYPEONFADEOUT	3
#define	HUDMESSAGETYPE_FADEINOUT		4

// Different levels of network messages.
#define	NETMSG_LITE		0
#define	NETMSG_MEDIUM	1
#define	NETMSG_HIGH		2

// Which actor flags are being updated?
enum FlagSet
{
	FLAGSET_UNKNOWN,
	FLAGSET_FLAGS,
	FLAGSET_FLAGS2,
	FLAGSET_FLAGS3,
	FLAGSET_FLAGS4,
	FLAGSET_FLAGS5,
	FLAGSET_FLAGS6,
	FLAGSET_FLAGS7,
	FLAGSET_FLAGSST
};

enum ActorScaleFlag
{
	ACTORSCALE_X = 1,
	ACTORSCALE_Y = 2
};

// Which actor sound is being updated?
#define	ACTORSOUND_SEESOUND			1
#define	ACTORSOUND_ATTACKSOUND		2
#define	ACTORSOUND_PAINSOUND		3
#define	ACTORSOUND_DEATHSOUND		4
#define	ACTORSOUND_ACTIVESOUND		5

// [BB]: Some optimization. For some actors that are sent in bunches, to reduce the size,
// just send some key letter that identifies the actor, instead of the full name.
#define NUMBER_OF_ACTOR_NAME_KEY_LETTERS	3
#define NUMBER_OF_WEAPON_NAME_KEY_LETTERS	10

// [BB] 5 = 1 + 4 (SVC_HEADER + packet number)
const int PACKET_HEADER_SIZE = 5;

//*****************************************************************************
enum
{
	// Program is being run in single player mode.
	NETSTATE_SINGLE,

	// Program is being run in single player mode, emulating a network game (bots, etc).
	NETSTATE_SINGLE_MULTIPLAYER,

	// Program is a client playing a network game.
	NETSTATE_CLIENT,

	// Program is a server, hosting a game.
	NETSTATE_SERVER,

	NUM_NETSTATES
};

//*****************************************************************************
//	VARIABLES

extern FString g_lumpsAuthenticationChecksum;
extern FString g_MapCollectionChecksum;

//*****************************************************************************
//	PROTOTYPES

void			NETWORK_Construct( USHORT usPort, bool bAllocateLANSocket );
void			NETWORK_Destruct( void );

int				NETWORK_GetPackets( void );
int				NETWORK_GetLANPackets( void );
NETADDRESS_s	NETWORK_GetFromAddress( void );
void			NETWORK_LaunchPacket( NETBUFFER_s *pBuffer, NETADDRESS_s Address );
NETADDRESS_s	NETWORK_GetLocalAddress( void );
NETADDRESS_s	NETWORK_GetCachedLocalAddress( void );
NETBUFFER_s		*NETWORK_GetNetworkMessageBuffer( void );
USHORT			NETWORK_ntohs( ULONG ul );
bool			NETWORK_IsGeoIPAvailable( void );
FString			NETWORK_GetCountryCodeFromAddress( NETADDRESS_s Address );
USHORT			NETWORK_GetLocalPort( void );

// [TP] Now a struct
struct NetworkPWAD
{
	FString name;
	FString checksum;
	int wadnum; // [TP] Added wadnum
};

const TArray<NetworkPWAD>&	NETWORK_GetPWADList( void ); // [RC]
const char		*NETWORK_GetIWAD( void );
void			NETWORK_AddLumpForAuthentication( const LONG LumpNumber );
void			NETWORK_GenerateMapLumpMD5Hash( MapData *Map, const LONG LumpNumber, FString &MD5Hash );
void			NETWORK_GenerateLumpMD5Hash( const int LumpNum, FString &MD5Hash );
FString			NETWORK_MapCollectionChecksum( ); // [Dusk]
void			NETWORK_MakeMapCollectionChecksum( ); // [Dusk]

enum { NO_SCRIPT_NETID = -1 };
int				NETWORK_ACSScriptFromNetID( int netid );
int				NETWORK_ACSScriptToNetID( int script );
void			NETWORK_MakeScriptNameIndex();
FName			NETWORK_ReadName( BYTESTREAM_s* bytestream );
void			NETWORK_WriteName( BYTESTREAM_s* bytestream, FName name );

const char		*NETWORK_GetClassNameFromIdentification( USHORT usActorNetworkIndex );
const PClass	*NETWORK_GetClassFromIdentification( USHORT usActorNetworkIndex );

// [BB] Returns true if either "NETSTATE_CLIENT" or a client side demo is played.
bool			NETWORK_InClientMode( );
bool			NETWORK_IsConsolePlayerOrNotInClientMode( const player_t *pPlayer );
bool			NETWORK_IsConsolePlayer( const AActor *pActor );
bool			NETWORK_IsConsolePlayerOrSpiedByConsolePlayerOrNotInClientMode( const player_t *pPlayer );
bool			NETWORK_IsActorClientHandled( const AActor *pActor );
bool			NETWORK_InClientModeAndActorNotClientHandled( const AActor *pActor );
bool			NETWORK_IsClientPredictedSpecial( const int Special );
bool			NETWORK_ShouldActorNotBeSpawned ( const AActor *pSpawner, const PClass *pSpawnType, const bool bForceClientSide = false );

// [BB] Generate a checksum from a ticcmd_t.
SDWORD			NETWORK_Check ( ticcmd_t *pCmd );

// [TP] Announce something to all clients if the server or just print if not
void STACK_ARGS NETWORK_Printf( const char* format, ... ) GCCPRINTF(1,2);

// [BB] Sound attenuation is a float, but we only want to sent a byte for the 
// attenuation to instructs clients to play a sound. The enum is used for the
// conversion of the neccessary attenuation values.
enum
{
	ATTN_INT_NONE,
	ATTN_INT_NORM,
	ATTN_INT_IDLE,
	ATTN_INT_STATIC,
	ATTN_INT_COUNT
};

int				NETWORK_AttenuationFloatToInt ( const float fAttenuation );
float			NETWORK_AttenuationIntToFloat ( const int iAttenuation );


// Access functions.
LONG			NETWORK_GetState( void );
void			NETWORK_SetState( LONG lState );

void			I_DoSelect( void );

// DEBUG FUNCTION!
#ifdef	_DEBUG
void	NETWORK_FillBufferWithShit( NETBUFFER_s *pBuffer, ULONG ulSize );
#endif

/**
 * \author BB
 */
template <typename T>
LONG NETWORK_GetFirstFreeID ( void )
{
	T	*pT;

	std::vector<bool> idUsed ( 8192 );

	TThinkerIterator<T>		Iterator;

	while (( pT = Iterator.Next( )))
	{
		if ( (pT->GetID( ) >= 0) )
			idUsed[pT->GetID( )] = true;
	}

	for ( unsigned int i = 0; i < idUsed.size(); i++ )
	{
		if ( idUsed[i] == false )
			return i;
	}

	Printf( "NETWORK_GetFirstFreeID: ID limit reached (>=8192)\n" );
	return ( -1 );
}

// [BB] Legacy interface that redirects all calls to Skulltag's old MD5 code to the ZDoom MD5 code.
class CMD5Checksum  
{
public:
	static void GetMD5 ( const BYTE* pBuf, UINT nLength, FString &OutString );
};

#endif	// __NETWORK_H__
