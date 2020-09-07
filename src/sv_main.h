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
// Filename: sv_commands.h
//
// Description: Contains variables and routines related to the server portion
// of the program.
//
//-----------------------------------------------------------------------------

#ifndef __SV_MAIN_H__
#define __SV_MAIN_H__

#include "actor.h"
#include "d_player.h"
#include "i_net.h"
#include "networkshared.h"
#include "s_sndseq.h"
#include "r_data/sprites.h"
#include "network/packetarchive.h"
#include <list>
#include <queue>

//*****************************************************************************
//	DEFINES

// Interval of time that clients' chat instances are reset at.
#define	CHATINSTANCE_RESET_INTERVAL	( TICRATE * 2 )

// Maximum amount of chat instance times we store.
#define	MAX_CHATINSTANCE_STORAGE	4

// Maximum amount of user info change instance times we store.
#define	MAX_USERINFOINSTANCE_STORAGE	4

// Number of seconds before a client times out.
#define CLIENT_TIMEOUT				40

// Maximum size of the packets sent out by the server.
//#define	MAX_UDP_PACKET				8192

// Amount of time the client has to report his checksum of the level.
#define	CLIENT_CHECKSUM_WAITTIME	( 15 * TICRATE )

// This is for the server console, but since we normally can't include that (win32 stuff),
// we can just put it here.
#define	UDF_NAME					0x00000001
#define	UDF_FRAGS					0x00000002
#define	UDF_PING					0x00000004
#define	UDF_TIME					0x00000008

#define	MAX_OVERMOVEMENT_LEVEL		( 2 * TICRATE )

#define	KILOBYTE					1024
#define	MEGABYTE					( KILOBYTE * 1024 )
#define	GIGABYTE					( MEGABYTE * 1024 )

#define	MINUTE						60
#define	HOUR						( MINUTE * 60 )
#define	DAY							( HOUR * 24 )
#define WEEK						( DAY * 7 )
#define YEAR						( DAY * 365 )

// Server query flags.
#define	SQF_NAME					0x00000001
#define	SQF_URL						0x00000002
#define	SQF_EMAIL					0x00000004
#define	SQF_MAPNAME					0x00000008
#define	SQF_MAXCLIENTS				0x00000010
#define	SQF_MAXPLAYERS				0x00000020
#define	SQF_PWADS					0x00000040
#define	SQF_GAMETYPE				0x00000080
#define	SQF_GAMENAME				0x00000100
#define	SQF_IWAD					0x00000200
#define	SQF_FORCEPASSWORD			0x00000400
#define	SQF_FORCEJOINPASSWORD		0x00000800
#define	SQF_GAMESKILL				0x00001000
#define	SQF_BOTSKILL				0x00002000
#define	SQF_DMFLAGS					0x00004000
#define	SQF_LIMITS					0x00010000
#define	SQF_TEAMDAMAGE				0x00020000
#define	SQF_TEAMSCORES				0x00040000
#define	SQF_NUMPLAYERS				0x00080000
#define	SQF_PLAYERDATA				0x00100000
#define SQF_TEAMINFO_NUMBER			0x00200000
#define SQF_TEAMINFO_NAME			0x00400000
#define SQF_TEAMINFO_COLOR			0x00800000
#define SQF_TEAMINFO_SCORE			0x01000000
#define SQF_TESTING_SERVER			0x02000000
#define SQF_DATA_MD5SUM				0x04000000
#define SQF_ALL_DMFLAGS				0x08000000
#define SQF_SECURITY_SETTINGS		0x10000000
#define SQF_OPTIONAL_WADS			0x20000000
#define SQF_DEH						0x40000000

#define	SQF_ALL						( SQF_NAME|SQF_URL|SQF_EMAIL|SQF_MAPNAME|SQF_MAXCLIENTS|SQF_MAXPLAYERS| \
									  SQF_PWADS|SQF_GAMETYPE|SQF_GAMENAME|SQF_IWAD|SQF_FORCEPASSWORD|SQF_FORCEJOINPASSWORD|SQF_GAMESKILL| \
									  SQF_BOTSKILL|SQF_DMFLAGS|SQF_LIMITS|SQF_TEAMDAMAGE|SQF_TEAMSCORES|SQF_NUMPLAYERS|SQF_PLAYERDATA|SQF_TEAMINFO_NUMBER|SQF_TEAMINFO_NAME|SQF_TEAMINFO_COLOR|SQF_TEAMINFO_SCORE| \
									  SQF_TESTING_SERVER|SQF_DATA_MD5SUM|SQF_ALL_DMFLAGS|SQF_SECURITY_SETTINGS|SQF_OPTIONAL_WADS|SQF_DEH )

#define	MAX_STORED_QUERY_IPS		512

//*****************************************************************************
enum CLIENTSTATE_e
{
	// Client slot can be used for a new connection.
	CLS_FREE,

	// Client slot has just received a connection signal.
	CLS_CHALLENGE,

	// [BB] Connection has been received, but the player hasn't authenticated or been spawned in the game yet.
	CLS_CONNECTED,

	// [BB] The map has changed after the client authenticated his level, but
	// before he finished loading the level.
	CLS_AUTHENTICATED_BUT_OUTDATED_MAP,

	// Client has authenticated his level.
	CLS_AUTHENTICATED,

	// [BB] Client is spawned, but needs to reauthenticate because the map was changed.
	CLS_SPAWNED_BUT_NEEDS_AUTHENTICATION,

	// Client is in the game.
	CLS_SPAWNED,

};

//*****************************************************************************
struct STORED_QUERY_IP_s
{
	// Address of the person who queried us.
	NETADDRESS_s	Address;

	// Gametic when we allow another query.
	LONG			lNextAllowedGametic;

};

//*****************************************************************************
struct CLIENT_MOVE_COMMAND_s
{
	ticcmd_t		cmd;
	angle_t			angle;
	angle_t			pitch;
	USHORT			usWeaponNetworkIndex;
	ULONG				ulGametic;
	ULONG			ulServerGametic;

	// [BB] We want to process the command from the lowest gametic first.
	// This puts the lowest gametic on top of the queue. 
	bool operator<(const CLIENT_MOVE_COMMAND_s& other) const {
	  return ( ulGametic > other.ulGametic );
	}
};

//*****************************************************************************
class ClientCommand
{
public:
	ClientCommand () {};

	virtual ~ClientCommand () {};

	virtual bool process ( const ULONG ulClient ) const = 0;

	virtual bool isMoveCmd ( ) const
	{
		return false;
	}
};

//*****************************************************************************
struct CLIENT_s
{
	// The network address of this client.
	NETADDRESS_s	Address;

	// Client state (free, in game, etc.).
	CLIENTSTATE_e	State;

	// The buffer in which this client's commands are written.
	NETBUFFER_s		PacketBuffer;

	// A seperate buffer for non-critical commands that do not require sequencing.
	NETBUFFER_s		UnreliablePacketBuffer;

	// We back up the last PACKET_BUFFER_SIZE packets we've sent to the client so that we can
	// retransmit them if necessary.
	OutgoingPacketBuffer	SavedPackets;

	// This is the last tic in which we received a command from this client. Used for timeouts.
	ULONG			ulLastCommandTic;

	// Used for calculating pings.
    ULONG			ulLastGameTic;

	// Can client remotely control server?
	bool			bRCONAccess;

	// Which pair of eyes is this client spying through (spectator)?
	ULONG			ulDisplayPlayer;

	// This is the gametic that the client sent to us. We simply send this back to him
	// so he can determine whether or not he's lagging.
	ULONG			ulClientGameTic;

	// Client wants tp start each round as a spectator.
	bool			bWantStartAsSpectator;

	// Client doesn't want his fragcount restored if he is reconnecting to the server.
	bool			bWantNoRestoreFrags;

	// [BB] Client doesn't want his country to be revealed to the other players.
	bool			bWantHideCountry;

	// [TP] Client doesn't want his account to be revealed to the other players.
	bool			WantHideAccount;

	// [BB] Did the client not yet acknowledge receiving the last full update?
	bool			bFullUpdateIncomplete;

	// [BB] A record of the gametics the client called protected commands, e.g. send_password.
	RingBuffer<LONG, 6> commandInstances;

	// [BB] A record of the gametics the client called protected minor commands, e.g. toggleconsole.
	RingBuffer<LONG, 100> minorCommandInstances;

	// A record of the gametic the client spoke at. We store the last MAX_CHATINSTANCE_STORAGE
	// times the client chatted. This is used to chat spam protection.
	LONG			lChatInstances[MAX_CHATINSTANCE_STORAGE];
	ULONG			ulLastChatInstance;

	// A record of the gametic the client spoke at. We store the last MAX_CHATINSTANCE_STORAGE
	// times the client chatted. This is used to chat spam protection.
	LONG			lUserInfoInstances[MAX_USERINFOINSTANCE_STORAGE];
	ULONG			ulLastUserInfoInstance;

	// Record the last time this player changed teams, so we can potentially forbid him from
	// doing it again.
	ULONG			ulLastChangeTeamTime;

	// Record the last time this player suicided, so we can potentially forbid him from
	// doing it again.
	ULONG			ulLastSuicideTime;

	// Last tick the client requested missing packets.
	LONG			lLastPacketLossTick;

	// Last tick we received a movement command.
	LONG			lLastMoveTick;

	// We keep track of how many extra movement commands we get from the client. If it
	// exceeds a certain level over time, we kick him.
	LONG			lOverMovementLevel;

	// When the client authenticates his level, should enter scripts be run as well?
	bool			bRunEnterScripts;

	// [BB] Did we just ask the client to change its weapon?
	bool			bWeaponChangeRequested;

	// [BB] Last tick we asked the client to change its weapon.
	LONG			bLastWeaponChangeRequestTick;

	// [BB] Did we notice anything suspicious about this client?
	bool			bSuspicious;

	// [BB] Amount of the consistency warnings the client caused since connecting to the server.
	ULONG			ulNumConsistencyWarnings;

	// What is the name of the client's skin?
	char			szSkin[MAX_SKIN_NAME+1];

	// [RC] List of IP addresses that this client is ignoring.
	std::list<STORED_QUERY_IP_s> IgnoredAddresses;

	// [K6] Last tic we got some action from the client. Used to determine his presence.
	LONG			lLastActionTic;

	// [BB] Buffer storing all movement commands received from the client we haven't executed yet.
	TArray<ClientCommand*>	MoveCMDs;

	// [BB] Variables for the account system
	FString username;
	unsigned int clientSessionID;
	int SRPsessionID;
	bool loggedIn;
	TArray<unsigned char> bytesA;
	TArray<unsigned char> bytesB;
	TArray<unsigned char> bytesM;
	TArray<unsigned char> bytesHAMK;
	TArray<unsigned char> salt;

	// [CK] The client communicates back to us with the last gametic from the server it saw
	LONG			lLastServerGametic;

	// [TP] The size of this client's screen, for ACS.
	WORD			ScreenWidth;
	WORD			ScreenHeight;

	FString GetAccountName() const;
};

//*****************************************************************************
struct EDITEDTRANSLATION_s
{
	// Which index in the list of translations is this?
	ULONG			ulIdx;

	// [BB] Type of the translation, i.e. PCD_TRANSLATIONRANGE1 or PCD_TRANSLATIONRANGE2
	ULONG			ulType;

	// The start/end range of the translation.
	ULONG			ulStart;
	ULONG			ulEnd;

	// translation using palette shifting (compare PCD_TRANSLATIONRANGE1)
	ULONG			ulPal1;
	ULONG			ulPal2;

	// [BB] translation like PCD_TRANSLATIONRANGE2
	ULONG			ulR1;
	ULONG			ulG1;
	ULONG			ulB1;
	ULONG			ulR2;
	ULONG			ulG2;
	ULONG			ulB2;


};

//*****************************************************************************
struct SECTORLINK_s
{
	// [BB] Which sector is linked?
	ULONG			ulSector;

	// [BB] Link arguments.
	int				iArg1;
	int				iArg2;
	int				iArg3;

};

//*****************************************************************************
//	PROTOTYPES

void		SERVER_Construct( void );
void		SERVER_Destruct( void );
void		SERVER_Tick( void );

void		SERVER_SendOutPackets( void );
void		SERVER_SendClientPacket( ULONG ulClient, bool bReliable );
void		SERVER_CheckClientBuffer( ULONG ulClient, ULONG ulSize, bool bReliable );
LONG		SERVER_FindFreeClientSlot( void );
LONG		SERVER_FindClientByAddress( NETADDRESS_s Address );
CLIENT_s	*SERVER_GetClient( ULONG ulIdx );
ULONG		SERVER_CalcNumConnectedClients( void );
ULONG		SERVER_CalcNumPlayers( void );
ULONG		SERVER_CountPlayers( bool bCountBots );
ULONG		SERVER_CalcNumNonSpectatingPlayers( ULONG ulExcludePlayer );
void		SERVER_CheckTimeouts( void );
void		SERVER_GetPackets( void );
void		SERVER_SendChatMessage( ULONG ulPlayer, ULONG ulMode, const char *pszString );
void		SERVER_DetermineConnectionType( BYTESTREAM_s *pByteStream );
void		SERVER_SetupNewConnection( BYTESTREAM_s *pByteStream, bool bNewPlayer );
void		SERVER_RequestClientToAuthenticate( ULONG ulClient );
void		SERVER_AuthenticateClientLevel( BYTESTREAM_s *pByteStream );
bool		SERVER_PerformAuthenticationChecksum( BYTESTREAM_s *pByteStream );
void		SERVER_ConnectNewPlayer( BYTESTREAM_s *pByteStream );
bool		SERVER_GetUserInfo( BYTESTREAM_s *pByteStream, bool bAllowKick, bool bEnforceRequired = false );
void		SERVER_ConnectionError( NETADDRESS_s Address, const char *pszMessage, ULONG ulErrorCode );
void		SERVER_ClientError( ULONG ulClient, ULONG ulErrorCode );
void		SERVER_SendFullUpdate( ULONG ulClient );
void		SERVER_WriteCommands( void );
bool		SERVER_IsValidClient( ULONG ulClient );
void		SERVER_AdjustPlayersReactiontime( const ULONG ulPlayer );
void		SERVER_DisconnectClient( ULONG ulClient, bool bBroadcast, bool bSaveInfo );
void		SERVER_SendHeartBeat( void );
void		STACK_ARGS SERVER_Printf( ULONG ulPrintLevel, const char *pszString, ... ) GCCPRINTF(2,3);
void		STACK_ARGS SERVER_Printf( const char *pszString, ... ) GCCPRINTF(1,2);
void		STACK_ARGS SERVER_PrintfPlayer( ULONG ulPrintLevel, ULONG ulPlayer, const char *pszString, ... ) GCCPRINTF(3,4);
void		STACK_ARGS SERVER_PrintfPlayer( ULONG ulPlayer, const char *pszString, ... ) GCCPRINTF(2,3);
void		SERVER_VPrintf( int printlevel, const char* format, va_list argptr, int playerToPrintTo );
void		SERVER_UpdateSectors( ULONG ulClient );
void		SERVER_UpdateMovers( ULONG ulClient );
void		SERVER_UpdateLines( ULONG ulClient );
void		SERVER_UpdateSides( ULONG ulClient );
void		SERVER_UpdateActorProperties( AActor *pActor, ULONG ulClient );
void		SERVER_ReconnectNewLevel( const char *pszMapName );
void		SERVER_LoadNewLevel( const char *pszMapName );
void		SERVER_KickAllPlayers( const char *pszReason );
void		SERVER_KickPlayer( ULONG ulPlayer, const char *pszReason );
void		SERVER_ForceToSpectate( ULONG ulPlayer, const char *pszReason );
void		SERVER_AddCommand( const char *pszCommand );
void		SERVER_DeleteCommand( void );
bool		SERVER_IsEveryoneReadyToGoOn( void );
LONG		SERVER_GetPlayerIgnoreTic( ULONG ulPlayer, NETADDRESS_s Address ); // [RC]
bool		SERVER_IsPlayerVisible( ULONG ulPlayer, ULONG ulPlayer2 );
bool		SERVER_IsPlayerAllowedToKnowHealth( ULONG ulPlayer, ULONG ulPlayer2 );
const char	*SERVER_GetCurrentFont( void );
void		SERVER_SetCurrentFont( const char *pszFont );
LONG		SERVER_AdjustDoorDirection( LONG lDirection );
LONG		SERVER_AdjustFloorDirection( LONG lDirection );
LONG		SERVER_AdjustCeilingDirection( LONG lDirection );
LONG		SERVER_AdjustElevatorDirection( LONG lDirection );
ULONG		SERVER_GetMaxPacketSize( void );
const char	*SERVER_GetMapMusic( void );
int			SERVER_GetMapMusicOrder( void );
void		SERVER_SetMapMusic( const char *pszMusic, int order );
void		SERVER_ResetInventory( ULONG ulClient, const bool bChangeClientWeapon = true );
void		SERVER_AddEditedTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulPal1, ULONG ulPal2 );
void		SERVER_AddEditedTranslation( ULONG ulTranslation, ULONG ulStart, ULONG ulEnd, ULONG ulR1, ULONG ulG1, ULONG ulB1, ULONG ulR2, ULONG ulG2, ULONG ulB2 );
void		SERVER_RemoveEditedTranslation( ULONG ulTranslation );
bool		SERVER_IsTranslationEdited( ULONG ulTranslation );
void		SERVER_ClearEditedTranslations( void );
void		SERVER_AddSectorLink( ULONG ulSector, int iArg1, int iArg2, int iArg3 );
void		SERVER_ClearSectorLinks( void );
void		SERVER_ErrorCleanup( void );
void		SERVER_ParsePacket( BYTESTREAM_s *pByteStream );
bool		SERVER_ProcessCommand( LONG lCommand, BYTESTREAM_s *pByteStream );
ULONG		SERVER_GetPlayerIndexFromName( const char *pszName, bool bIgnoreColors, bool bReturnBots );
LONG		SERVER_GetCurrentClient( void );
void		SERVER_GiveInventoryToPlayer( const player_t *player, AInventory *pInventory );
void		SERVER_HandleWeaponStateJump( ULONG ulPlayer, FState *pState, LONG lPosition );
void		SERVER_SetThingNonZeroAngleAndVelocity( AActor *pActor );
void		SERVER_IgnoreIP( NETADDRESS_s Address );
IPList		*SERVER_GetAdminList( void );
const FString& SERVER_GetMasterBanlistVerificationString( void );
void		SERVER_UpdateThingVelocity( AActor *pActor, bool updateZ, bool updateXY = true );
void		SERVER_SyncSharedKeys( int playerToSync, bool withmessage );
void		SERVER_SyncServerModCVars ( const int PlayerToSync );
void		SERVER_KillCheat( const char* what );
void STACK_ARGS SERVER_PrintWarning( const char* format, ... ) GCCPRINTF( 1, 2 );

// From sv_master.cpp
void		SERVER_MASTER_Construct( void );
void		SERVER_MASTER_Destruct( void );
void		SERVER_MASTER_Tick( void );
void		SERVER_MASTER_Broadcast( void );
void		SERVER_MASTER_SendServerInfo( NETADDRESS_s Address, ULONG ulFlags, ULONG ulTime, bool bBroadcasting );
const char	*SERVER_MASTER_GetGameName( void );
NETADDRESS_s SERVER_MASTER_GetMasterAddress( void );
void		SERVER_MASTER_HandleVerificationRequest( BYTESTREAM_s *pByteStream );
void		SERVER_MASTER_SendBanlistReceipt( void );

// Statistic functions.
LONG		SERVER_STATISTIC_GetTotalSecondsElapsed( void );
LONG		SERVER_STATISTIC_GetTotalPlayers( void );
LONG		SERVER_STATISTIC_GetMaxNumPlayers( void );
LONG		SERVER_STATISTIC_GetTotalFrags( void );
void		SERVER_STATISTIC_AddToTotalFrags( void );
QWORD		SERVER_STATISTIC_GetTotalOutboundDataTransferred( void );
LONG		SERVER_STATISTIC_GetPeakOutboundDataTransfer( void );
void		SERVER_STATISTIC_AddToOutboundDataTransfer( ULONG ulNumBytes );
LONG		SERVER_STATISTIC_GetCurrentOutboundDataTransfer( void );
QWORD		SERVER_STATISTIC_GetTotalInboundDataTransferred( void );
LONG		SERVER_STATISTIC_GetPeakInboundDataTransfer( void );
void		SERVER_STATISTIC_AddToInboundDataTransfer( ULONG ulNumBytes );
LONG		SERVER_STATISTIC_GetCurrentInboundDataTransfer( void );

//*****************************************************************************
//	EXTERNAL CONSOLE VARIABLES

EXTERN_CVAR( String, sv_motd );
EXTERN_CVAR( Bool, sv_defaultdmflags );
EXTERN_CVAR( Bool, sv_forcepassword );
EXTERN_CVAR( Bool, sv_forcejoinpassword );
EXTERN_CVAR( Int, sv_forcerespawntime ); // [RK] Delay used for forced respawn
EXTERN_CVAR( Bool, sv_showlauncherqueries );
EXTERN_CVAR( Int, sv_maxclients );
EXTERN_CVAR( Int, sv_maxplayers );
EXTERN_CVAR( String, sv_password );
EXTERN_CVAR( String, sv_joinpassword );
EXTERN_CVAR( String, sv_rconpassword );
EXTERN_CVAR( Int, sv_maxpacketsize );
EXTERN_CVAR( Bool, sv_timestamp );
EXTERN_CVAR( Int, sv_timestampformat );
EXTERN_CVAR( Int, sv_colorstripmethod );
EXTERN_CVAR( Bool, sv_minimizetosystray )
EXTERN_CVAR( Int, sv_queryignoretime )
EXTERN_CVAR( Bool, sv_forcelogintojoin )
EXTERN_CVAR( Bool, sv_limitcommands )

// From sv_master.cpp
EXTERN_CVAR( Bool, sv_updatemaster );
EXTERN_CVAR( Bool, sv_broadcast );
EXTERN_CVAR( String, sv_hostname );
EXTERN_CVAR( String, sv_website );
EXTERN_CVAR( String, sv_hostemail );
EXTERN_CVAR( String, masterhostname );

#endif	// __SV_MAIN_H__
