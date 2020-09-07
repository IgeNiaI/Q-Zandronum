//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2007 Brad Carney
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
// Date created:  7/2/07
//
//
// Filename: cl_demo.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include "c_console.h"
#include "c_dispatch.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "cmdlib.h"
#include "d_event.h"
#include "d_net.h"
#include "d_netinf.h"
#include "d_protocol.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_misc.h"
#include "m_random.h"
#include "network.h"
#include "networkshared.h"
#include "p_local.h"
#include "p_tick.h"
#include "r_draw.h"
#include "r_state.h"
#include "sbar.h"
#include "version.h"
#include "templates.h"
#include "r_data/r_translate.h"
#include "m_cheat.h"
#include "network_enums.h"

//*****************************************************************************
enum 
{
	// [BC] Message headers with bytes starting with 0 and going sequentially
	// isn't very distinguishing from other formats (such as normal ZDoom demos),
	// but does that matter?
	CLD_DEMOLENGTH = NUM_SERVER_COMMANDS,
	CLD_DEMOVERSION,
	CLD_CVARS,
	CLD_USERINFO,
	CLD_BODYSTART,
	CLD_TICCMD,
	CLD_LOCALCOMMAND, // [Dusk]
	CLD_DEMOEND,
	CLD_DEMOWADS, // [Dusk]

	NUM_DEMO_COMMANDS
};

//*****************************************************************************
//	PROTOTYPES

static	void				clientdemo_CheckDemoBuffer( ULONG ulSize );

//*****************************************************************************
//	VARIABLES

// Are we recording a demo?
static	bool				g_bDemoRecording;

// Are we playing a demo?
static	bool				g_bDemoPlaying;
static	bool				g_bDemoPlayingHonest;

// [BB] Is the demo we are playing paused?
static	bool				g_bDemoPaused = false;

// [BB] Do we want to skip to the next map in the demo we are playing at the moment?
static	bool				g_bSkipToNextMap = false;

// [BB] How many tics are we still supposed to skip in the demo we are playing at the moment?
static	ULONG				g_ulTicsToSkip = 0;

// Buffer for our demo.
static	BYTE				*g_pbDemoBuffer;

// Our byte stream that points to where we are in our demo.
static	BYTESTREAM_s		g_ByteStream;

// [BB] Position in the demo stream for CLIENTDEMO_InsertPacketAtMarkedPosition.
static	BYTE				*g_pbMarkedStreamPosition;

// Name of our demo.
static	FString				g_DemoName;

// Length of the demo.
static	LONG				g_lDemoLength;

// This is the gametic we started playing the demo on.
static	LONG				g_lGameticOffset;

// Maximum length our current demo can be.
static	LONG				g_lMaxDemoLength;

// [BB] Special player that is used to control the camera when playing demos in free spectate mode.
static	player_t			g_demoCameraPlayer;

// [Dusk] ZCLD magic number signature
static	const DWORD			g_demoSignature = MAKE_ID( 'Z', 'C', 'L', 'D' );

// [Dusk] Should we perform demo authentication?
CUSTOM_CVAR( Bool, demo_pure, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG )
{
	// Since unsetting this CVAR can screw up demo playback pretty badly,
	// I think it's reasonable to inform the user that this CVAR
	// should be used with caution.
	if ( !self )
		Printf( "\\ckWarning: demo_pure is false. Demo authentication is therefore disabled. "
		"Demos may get played back with completely incorrect WADs!\\c-\n" );
}

//*****************************************************************************
//	FUNCTIONS

void CLIENTDEMO_BeginRecording( const char *pszDemoName )
{
	if ( pszDemoName == NULL )
		return;

	// First, setup the demo name.
	g_DemoName = pszDemoName;
	FixPathSeperator( g_DemoName );
	DefaultExtension( g_DemoName, ".cld" );

	// Allocate 128KB of memory for the demo buffer.
	g_bDemoRecording = true;
	g_lMaxDemoLength = 0x20000;
	g_pbDemoBuffer = (BYTE *)M_Malloc( g_lMaxDemoLength );
	g_ByteStream.pbStream = g_pbDemoBuffer;
	g_ByteStream.pbStreamEnd = g_pbDemoBuffer + g_lMaxDemoLength;

	// Write our header.
	// [Dusk] Write a static "ZCLD" which is consistent between
	// different Zandronum versions.
	NETWORK_WriteLong( &g_ByteStream, g_demoSignature );

	// Write the length of the demo. Of course, we can't complete this quite yet!
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOLENGTH );
	g_ByteStream.pbStream += 4;

	// Write version information helpful for this demo.
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOVERSION );
	NETWORK_WriteShort( &g_ByteStream, DEMOGAMEVERSION );
	NETWORK_WriteString( &g_ByteStream, GetVersionStringRev() );
	NETWORK_WriteByte( &g_ByteStream, BUILD_ID );
	NETWORK_WriteLong( &g_ByteStream, rngseed );

	// [Dusk] Write the amount of WADs and their names, incl. IWAD
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOWADS );
	ULONG ulWADCount = 1 + NETWORK_GetPWADList().Size( ); // 1 for IWAD
	NETWORK_WriteShort( &g_ByteStream, ulWADCount );
	NETWORK_WriteString( &g_ByteStream, NETWORK_GetIWAD ( ) );

	for ( unsigned int i = 0; i < NETWORK_GetPWADList().Size(); ++i )
		NETWORK_WriteString( &g_ByteStream, NETWORK_GetPWADList()[i].name );

	// [Dusk] Write the network authentication string, we need it to
	// ensure we have the right WADs loaded.
	NETWORK_WriteString( &g_ByteStream, g_lumpsAuthenticationChecksum.GetChars( ) );

	// [Dusk] Also generate and write the map collection checksum so we can
	// authenticate the maps.
	NETWORK_MakeMapCollectionChecksum( );
	NETWORK_WriteString( &g_ByteStream, g_MapCollectionChecksum.GetChars( ) );

/*
	// Write cvars chunk.
	StartChunk( CLD_CVARS, &g_pbDemoBuffer );
	C_WriteCVars( &g_pbDemoBuffer, CVAR_SERVERINFO|CVAR_DEMOSAVE );
	FinishChunk( &g_pbDemoBuffer );
*/
	// Write the console player's userinfo.
	CLIENTDEMO_WriteUserInfo( );

	// Indicate that we're done with header information, and are ready
	// to move onto the body of the demo.
	NETWORK_WriteByte( &g_ByteStream, CLD_BODYSTART );

	CLIENT_SetServerLagging( false );
}

//*****************************************************************************
//
bool CLIENTDEMO_ProcessDemoHeader( void )
{
	bool	bBodyStart;
	LONG	lDemoVersion;
	LONG	lCommand;

	// [Dusk] Check ZCLD instead of CLD_DEMOSTART
	if ( NETWORK_ReadLong( &g_ByteStream ) != g_demoSignature )
	{
		// [Dusk] Rewind back and try see if this is an old demo. Old demos started
		// with a CLD_DEMOSTART (which was non-constant), followed by 12345678.
		g_ByteStream.pbStream -= 4;
		NETWORK_ReadByte( &g_ByteStream ); // Skip CLD_DEMOSTART
		if ( NETWORK_ReadLong( &g_ByteStream ) == 12345678 )
		{
			// [Dusk] Dig out the version string. It should be 13 bytes in.
			g_ByteStream.pbStream = g_pbDemoBuffer + 13;
			I_Error( "CLIENTDEMO_ProcessDemoHeader: This is an old, version %s demo file.\n", 
				NETWORK_ReadString( &g_ByteStream ));
		}

		// [Dusk] Otherwise, this file is just garbage.
		I_Error( "CLIENTDEMO_ProcessDemoHeader: This is not a " GAMENAME " demo file!\n" );
		return ( false );
	}

	if ( NETWORK_ReadByte( &g_ByteStream ) != CLD_DEMOLENGTH )
	{
		I_Error( "CLIENTDEMO_ProcessDemoHeader: Expected CLD_DEMOLENGTH.\n" );
		return ( false );
	}

	g_lDemoLength = NETWORK_ReadLong( &g_ByteStream );
	g_ByteStream.pbStreamEnd = g_pbDemoBuffer + g_lDemoLength + ( g_lDemoLength & 1 );

	// Continue to read header commands until we reach the body of the demo.
	bBodyStart = false;
	while ( bBodyStart == false )
	{  
		lCommand = NETWORK_ReadByte( &g_ByteStream );

		switch ( lCommand )
		{
		case CLD_DEMOVERSION:

			// Read in the DEMOGAMEVERSION the demo was recorded with.
			lDemoVersion = NETWORK_ReadShort( &g_ByteStream );
			if ( lDemoVersion < MINDEMOVERSION )
				I_Error( "Demo requires an older version of " GAMENAME "!\n" );

			// Read in the DOTVERSIONSTR the demo was recorded with.
			Printf( "Version %s demo\n", NETWORK_ReadString( &g_ByteStream ));

			// [Dusk] BUILD_ID is now stored in the demo. We don't do anything
			// with it - it's of interest for external applications only. Just
			// skip it here.
			NETWORK_ReadByte( &g_ByteStream );

			// Read in the random number generator seed.
			rngseed = NETWORK_ReadLong( &g_ByteStream );
			FRandom::StaticClearRandom( );
			break;
/*
		case CLD_CVARS:

			C_ReadCVars( &g_pbDemoBuffer );
			break;
*/
		case CLD_USERINFO:

			CLIENTDEMO_ReadUserInfo( );
			break;
		case CLD_BODYSTART:

			bBodyStart = true;
			break;

		// [Dusk]
		case CLD_DEMOWADS:
			CLIENTDEMO_ReadDemoWads( );
			break;

		// [Dusk] Bad headers shouldn't just be ignored, that's just asking for trouble.
		default:
			I_Error( "Unknown demo header %ld!\n", lCommand );
			break;
		}
	}

	return ( bBodyStart );
}

//*****************************************************************************
//
void CLIENTDEMO_WriteUserInfo( void )
{
	// First, make sure we have enough space to write this command. If not, add
	// more space.
	clientdemo_CheckDemoBuffer( 18 +
		(ULONG)strlen( players[consoleplayer].userinfo.GetName() ) +
		(ULONG)strlen( skins[players[consoleplayer].userinfo.GetSkin()].name ) +
		(ULONG)strlen( PlayerClasses[players[consoleplayer].CurrentPlayerClass].Type->Meta.GetMetaString( APMETA_DisplayName )));

	// Write the header.
	NETWORK_WriteByte( &g_ByteStream, CLD_USERINFO );

	// Write the player's userinfo.
	NETWORK_WriteString( &g_ByteStream, players[consoleplayer].userinfo.GetName() );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.GetGender() );
	NETWORK_WriteLong( &g_ByteStream, players[consoleplayer].userinfo.GetColor() );
	NETWORK_WriteLong( &g_ByteStream, players[consoleplayer].userinfo.GetAimDist() );
	NETWORK_WriteString( &g_ByteStream, skins[players[consoleplayer].userinfo.GetSkin()].name );
	NETWORK_WriteLong( &g_ByteStream, players[consoleplayer].userinfo.GetRailColor() );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.GetHandicap() );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.GetTicsPerUpdate() );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.GetConnectionType() );
	NETWORK_WriteByte( &g_ByteStream, players[consoleplayer].userinfo.GetClientFlags() ); // [CK] List of booleans
	NETWORK_WriteString( &g_ByteStream, PlayerClasses[players[consoleplayer].CurrentPlayerClass].Type->Meta.GetMetaString( APMETA_DisplayName ));
}

//*****************************************************************************
//
void CLIENTDEMO_ReadUserInfo( void )
{
	userinfo_t &info = players[consoleplayer].userinfo;
	*static_cast<FStringCVar *>(info[NAME_Name]) =  NETWORK_ReadString( &g_ByteStream );
	// [BB] Make sure that the gender is valid.
	*static_cast<FIntCVar *>(info[NAME_Gender]) = clamp ( NETWORK_ReadByte( &g_ByteStream ), 0, 2 );
	info.ColorChanged( NETWORK_ReadLong( &g_ByteStream ) );
	*static_cast<FFloatCVar *>(info[NAME_Autoaim]) = static_cast<float> ( NETWORK_ReadLong( &g_ByteStream ) ) / ANGLE_1 ;
	*static_cast<FIntCVar *>(info[NAME_Skin]) = R_FindSkin( NETWORK_ReadString( &g_ByteStream ), players[consoleplayer].CurrentPlayerClass );
	*static_cast<FIntCVar *>(info[NAME_RailColor]) = NETWORK_ReadLong( &g_ByteStream );
	*static_cast<FIntCVar *>(info[NAME_Handicap]) = NETWORK_ReadByte( &g_ByteStream );
	info.TicsPerUpdateChanged ( NETWORK_ReadByte( &g_ByteStream ) );
	info.ConnectionTypeChanged ( NETWORK_ReadByte( &g_ByteStream ) );
	info.ClientFlagsChanged ( NETWORK_ReadByte( &g_ByteStream ) ); // [CK] Client booleans
	info.PlayerClassChanged ( NETWORK_ReadString( &g_ByteStream ));

	R_BuildPlayerTranslation( consoleplayer );
	if ( StatusBar )
		StatusBar->AttachToPlayer( &players[consoleplayer] );

	// Apply the skin properties to the player's body.
	if ( players[consoleplayer].mo != NULL )
	{
		if (players[consoleplayer].cls != NULL &&
			players[consoleplayer].mo->state->sprite ==
			GetDefaultByType (players[consoleplayer].cls)->SpawnState->sprite)
		{ // Only change the sprite if the player is using a standard one
			players[consoleplayer].mo->sprite = skins[players[consoleplayer].userinfo.GetSkin()].sprite;
			players[consoleplayer].mo->scaleX = skins[players[consoleplayer].userinfo.GetSkin()].ScaleX;
			players[consoleplayer].mo->scaleY = skins[players[consoleplayer].userinfo.GetSkin()].ScaleY;
		}
	}
}

//*****************************************************************************
//
void CLIENTDEMO_WriteTiccmd( ticcmd_t *pCmd )
{
	// First, make sure we have enough space to write this command. If not, add
	// more space.
	clientdemo_CheckDemoBuffer( 14 );

	// Write the header.
	NETWORK_WriteByte( &g_ByteStream, CLD_TICCMD );

	// Write the contents of the ticcmd.
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.yaw );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.roll );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.pitch );
	NETWORK_WriteByte( &g_ByteStream, pCmd->ucmd.buttons );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.upmove );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.forwardmove );
	NETWORK_WriteShort( &g_ByteStream, pCmd->ucmd.sidemove );
}

//*****************************************************************************
//
void CLIENTDEMO_ReadTiccmd( ticcmd_t *pCmd )
{
	// Read the contents of ticcmd.
	pCmd->ucmd.yaw = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.roll = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.pitch = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.buttons = NETWORK_ReadByte( &g_ByteStream );
	pCmd->ucmd.upmove = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.forwardmove = NETWORK_ReadShort( &g_ByteStream );
	pCmd->ucmd.sidemove = NETWORK_ReadShort( &g_ByteStream );
}

//*****************************************************************************
//
void CLIENTDEMO_WritePacket( BYTESTREAM_s *pByteStream )
{
	// First, make sure we have enough space to write this command. If not, add
	// more space.
	clientdemo_CheckDemoBuffer( pByteStream->pbStreamEnd - pByteStream->pbStream );

	NETWORK_WriteBuffer( &g_ByteStream, pByteStream->pbStream, pByteStream->pbStreamEnd - pByteStream->pbStream );
}

//*****************************************************************************
//
void CLIENTDEMO_InsertPacketAtMarkedPosition( BYTESTREAM_s *pByteStream )
{
	// [BB] We can write to the current position of our stream without any special treatment.
	if ( g_pbMarkedStreamPosition == CLIENTDEMO_GetDemoStream()->pbStream )
		CLIENTDEMO_WritePacket( pByteStream );
	// [BB] If we are supposed to write to a previous position, we have to move what's already there..
	else if ( g_pbMarkedStreamPosition < CLIENTDEMO_GetDemoStream()->pbStream )
	{
		// [BB] Save the stuff currently at the desired position.
		const int bytesToCopy = CLIENTDEMO_GetDemoStream()->pbStream - g_pbMarkedStreamPosition;
		BYTE *copyBuffer = new BYTE[bytesToCopy];
		memcpy( copyBuffer, g_pbMarkedStreamPosition, bytesToCopy );

		// [BB] Make sure we have enough space for the new command.
		clientdemo_CheckDemoBuffer( pByteStream->pbStreamEnd - pByteStream->pbStream );

		// [BB] Change our demo stream to the desired position and write the incoming packet there.
		// [BB] clientdemo_CheckDemoBuffer updates g_pbMarkedStreamPosition if necessary.
		CLIENTDEMO_GetDemoStream()->pbStream = g_pbMarkedStreamPosition;
		CLIENTDEMO_WritePacket( pByteStream );

		// [BB] Append the saved stuff.
		BYTESTREAM_s stream;
		stream.pbStream = copyBuffer;
		stream.pbStreamEnd = copyBuffer + bytesToCopy;
		CLIENTDEMO_WritePacket( &stream );

		delete[] copyBuffer;
	}
	else
		Printf ( "CLIENTDEMO_InsertPacket Error: Can't write here!\n" );
}

//*****************************************************************************
//
void CLIENTDEMO_MarkCurrentPosition( void )
{
	g_pbMarkedStreamPosition = CLIENTDEMO_GetDemoStream()->pbStream;
}

//*****************************************************************************
//
void CLIENTDEMO_ReadPacket( void )
{
	LONG		lCommand;
	const char	*pszString;

	while ( 1 )
	{  
		lCommand = NETWORK_ReadByte( &g_ByteStream );

		// [TP/BB] Reset the bit reading buffer.
		g_ByteStream.bitBuffer = NULL;
		g_ByteStream.bitShift = -1;

		// End of message.
		if ( lCommand == -1 )
		{
			// [BB] When we reach the end of the demo stream, we need to stop the demo.
			CLIENTDEMO_FinishPlaying( );
			break;
		}

		switch ( lCommand )
		{
		case CLD_USERINFO:

			CLIENTDEMO_ReadUserInfo( );
			break;
		case CLD_TICCMD:

			CLIENTDEMO_ReadTiccmd( &players[consoleplayer].cmd );

			// After we write our ticcmd, we're done for this tic.
			if ( CLIENTDEMO_IsSkipping() == false )
				return;
			// [BB] If we don't return here, we essentially skip a tic and have to adjust the tic offset.
			else
			{
				g_lGameticOffset--;
				// [BB] If we are supposed to skip over a certain amount of tics, record that we have skipped one now.
				if ( g_ulTicsToSkip > 0 )
				{
					// [BB] When skipping a tic, we still need to process the current ticcmd_t.
					P_Ticker ();
					--g_ulTicsToSkip;
				}
			}
			break;
		case CLD_LOCALCOMMAND:

			switch( static_cast<ClientDemoLocalCommand>( NETWORK_ReadByte( &g_ByteStream )))
			{
			case CLD_LCMD_INVUSE:

				{
					AInventory	*pInventory;

					pszString = NETWORK_ReadString( &g_ByteStream );

					if ( players[consoleplayer].mo )
					{
						pInventory = players[consoleplayer].mo->FindInventory( pszString );
						if ( pInventory )
							players[consoleplayer].mo->UseInventory( pInventory );
					}
				}
				break;
			case CLD_LCMD_CENTERVIEW:

				Net_DoCommand( DEM_CENTERVIEW, NULL, consoleplayer );
				break;
			case CLD_LCMD_TAUNT:

				PLAYER_Taunt( &players[consoleplayer] );
				break;
			case CLD_LCMD_CHEAT:

				cht_DoCheat( &players[consoleplayer], NETWORK_ReadByte( &g_ByteStream ));
				break;
			case CLD_LCMD_WARPCHEAT:

				{
					fixed_t x = NETWORK_ReadLong( &g_ByteStream );
					fixed_t y = NETWORK_ReadLong( &g_ByteStream );
					Printf( "warp %g %g\n", FIXED2FLOAT( x ), FIXED2FLOAT( y ));
					P_TeleportMove( players[consoleplayer].mo, x, y, ONFLOORZ, true );
				}
				break;
			}
			break;
		case CLD_DEMOEND:

			CLIENTDEMO_FinishPlaying( );
			return;
		default:

			// Option to print commands for debugging purposes.
			if ( cl_showcommands )
				CLIENT_PrintCommand( lCommand );

			CLIENT_ProcessCommand( lCommand, &g_ByteStream );
			break;
		}
	}
}

//*****************************************************************************
//
void CLIENTDEMO_FinishRecording( void )
{
	LONG			lDemoLength;
	BYTESTREAM_s	ByteStream;

	// Write our header.
	NETWORK_WriteByte( &g_ByteStream, CLD_DEMOEND );

	// Go back real quick and write the length of this demo.
	lDemoLength = g_ByteStream.pbStream - g_pbDemoBuffer;
	ByteStream.pbStream = g_pbDemoBuffer + 5;
	ByteStream.pbStreamEnd = g_ByteStream.pbStreamEnd;
	NETWORK_WriteLong( &ByteStream, lDemoLength );

	// Write the contents of the buffer to the file, and free the memory we
	// allocated for the demo.
	M_WriteFile( g_DemoName.GetChars(), g_pbDemoBuffer, lDemoLength ); 
	M_Free( g_pbDemoBuffer );
	g_pbDemoBuffer = NULL;

	// We're no longer recording a demo.
	g_bDemoRecording = false;

	// All done!
	Printf( "Demo \"%s\" successfully recorded!\n", g_DemoName.GetChars() ); 
}

//*****************************************************************************
//
void CLIENTDEMO_DoPlayDemo( const char *pszDemoName )
{
	LONG	lDemoLump;
	LONG	lDemoLength;
	FString demoName = pszDemoName;

	// First, check if the demo is in a lump.
	lDemoLump = Wads.CheckNumForName( demoName );
	if ( lDemoLump >= 0 )
	{
		lDemoLength = Wads.LumpLength( lDemoLump );

		// Read the data from the lump into our demo buffer.
		g_pbDemoBuffer = new BYTE[lDemoLength];
		Wads.ReadLump( lDemoLump, g_pbDemoBuffer );
	}
	else
	{
		FixPathSeperator( demoName );
		DefaultExtension( demoName, ".cld" );
		lDemoLength = M_ReadFile( demoName, &g_pbDemoBuffer );
	}

	g_ByteStream.pbStream = g_pbDemoBuffer;
	g_ByteStream.pbStreamEnd = g_pbDemoBuffer + lDemoLength;

	if ( CLIENTDEMO_ProcessDemoHeader( ))
	{
		C_HideConsole( );
		g_bDemoPlaying = true;
		g_bDemoPlayingHonest = true;
		CLIENTDEMO_SetSkippingToNextMap ( false );

		g_lGameticOffset = gametic;
	}
	else
	{
		gameaction = ga_nothing;
		g_bDemoPlaying = false;
	}
}

//*****************************************************************************
//
void CLIENTDEMO_FinishPlaying( void )
{
//	C_RestoreCVars ();		// [RH] Restore cvars demo might have changed

	// Free our demo buffer.
	delete[] ( g_pbDemoBuffer );
	g_pbDemoBuffer = NULL;

	// We're no longer playing a demo.
	g_bDemoPlaying = false;
	g_bDemoPlayingHonest = false;
	CLIENTDEMO_SetSkippingToNextMap ( false );
	g_ulTicsToSkip = 0;

	// Clear out the existing players.
	CLIENT_ClearAllPlayers();
	// [BB] Also the special spectator player.
	CLIENTDEMO_ClearFreeSpectatorPlayer();

	consoleplayer = 0;
//	playeringame[consoleplayer] = true;
	players[consoleplayer].camera = NULL;
	if ( StatusBar )
		StatusBar->AttachToPlayer( &players[0] );

	if ( gameaction == ga_nothing )
	{
		D_AdvanceDemo( );

		// Go back to the full console.
		gameaction = ga_fullconsole;
		gamestate = GS_FULLCONSOLE;
	}

	// View is no longer active.
	viewactive = false;

	Printf( "Demo ended.\n" );
}

//*****************************************************************************
//
LONG CLIENTDEMO_GetGameticOffset( void )
{
	if ( g_bDemoPlaying == false )
		return ( 0 );

	return ( g_lGameticOffset );
}

//*****************************************************************************
//
void CLIENTDEMO_SetGameticOffset( LONG lOffset )
{
	g_lGameticOffset = lOffset;
}

//*****************************************************************************
//
void CLIENTDEMO_WriteLocalCommand( ClientDemoLocalCommand command, const char* pszArg )
{
	if ( pszArg != NULL )
		clientdemo_CheckDemoBuffer( (ULONG)strlen( pszArg ) + 2 );
	else
		clientdemo_CheckDemoBuffer( 2 );

	NETWORK_WriteByte( &g_ByteStream, CLD_LOCALCOMMAND );
	NETWORK_WriteByte( &g_ByteStream, command );

	if ( pszArg != NULL )
		NETWORK_WriteString( &g_ByteStream, pszArg );
}

//*****************************************************************************
//
// [TP] Writes a cheat into the demo
//
void CLIENTDEMO_WriteCheat ( ECheatCommand cheat )
{
	clientdemo_CheckDemoBuffer( 3 );
	NETWORK_WriteByte( &g_ByteStream, CLD_LOCALCOMMAND );
	NETWORK_WriteByte( &g_ByteStream, CLD_LCMD_CHEAT );
	NETWORK_WriteByte( &g_ByteStream, cheat );
}

//*****************************************************************************
//
void CLIENTDEMO_WriteWarpCheat ( fixed_t x, fixed_t y )
{
	clientdemo_CheckDemoBuffer( 10 );
	NETWORK_WriteByte( &g_ByteStream, CLD_LOCALCOMMAND );
	NETWORK_WriteByte( &g_ByteStream, CLD_LCMD_WARPCHEAT );
	NETWORK_WriteLong( &g_ByteStream, x );
	NETWORK_WriteLong( &g_ByteStream, y );
}

//*****************************************************************************
//
bool CLIENTDEMO_IsRecording( void )
{
	return ( g_bDemoRecording );
}

//*****************************************************************************
//
void CLIENTDEMO_SetRecording( bool bRecording )
{
	g_bDemoRecording = bRecording;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsPlaying( void )
{
	return ( g_bDemoPlaying || g_bDemoPlayingHonest );
}

//*****************************************************************************
//
void CLIENTDEMO_SetPlaying( bool bPlaying )
{
	g_bDemoPlaying = bPlaying;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsPaused( void )
{
	if ( CLIENTDEMO_IsPlaying() == false )
		return false;

	// [BB] Allow to skip while the playback is paused. This allows to go
	// through a demo tic by tic.
	if ( CLIENTDEMO_IsSkipping() == true )
		return false;

	return g_bDemoPaused;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsSkipping( void )
{
	return ( g_ulTicsToSkip > 0 ) || CLIENTDEMO_IsSkippingToNextMap();
}

//*****************************************************************************
//
bool CLIENTDEMO_IsSkippingToNextMap( void )
{
	return g_bSkipToNextMap;
}

//*****************************************************************************
//
void CLIENTDEMO_SetSkippingToNextMap( bool bSkipToNextMap )
{
	g_bSkipToNextMap = bSkipToNextMap;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsInFreeSpectateMode( void )
{
	const AActor *pCamera = players[consoleplayer].camera;
	return ( pCamera && ( pCamera == g_demoCameraPlayer.mo ) );
}
//*****************************************************************************
//
void CLIENTDEMO_SetFreeSpectatorTiccmd( ticcmd_t *pCmd )
{
	memcpy( &(g_demoCameraPlayer.cmd), pCmd, sizeof( ticcmd_t ));
}

//*****************************************************************************
//
void CLIENTDEMO_FreeSpectatorPlayerThink( bool bTickBody )
{
	P_PlayerThink ( &g_demoCameraPlayer );
	if ( bTickBody )
		g_demoCameraPlayer.mo->Tick();
}

//*****************************************************************************
//
player_t *CLIENTDEMO_GetFreeSpectatorPlayer( void )
{
	return &g_demoCameraPlayer;
}

//*****************************************************************************
//
bool CLIENTDEMO_IsFreeSpectatorPlayer( player_t *pPlayer )
{
	return ( &g_demoCameraPlayer == pPlayer );
}

//*****************************************************************************
//
void CLIENTDEMO_ClearFreeSpectatorPlayer( void )
{
	if ( g_demoCameraPlayer.mo != NULL )
	{
		if ( players[consoleplayer].camera == g_demoCameraPlayer.mo )
			players[consoleplayer].camera = players[consoleplayer].mo;

		g_demoCameraPlayer.mo->Destroy();
		g_demoCameraPlayer.mo = NULL;
	}

	player_t *p = &g_demoCameraPlayer;
	// Reset player structure to its defaults
	p->~player_t();
	::new(p) player_t;
}

//*****************************************************************************
// [Dusk] Read the WAD list and perform demo authentication.
void CLIENTDEMO_ReadDemoWads( void )
{
	// Read the count of WADs
	ULONG ulWADCount = NETWORK_ReadShort( &g_ByteStream );

	// Read the names of WADs and store them in the array
	TArray<FString> WadNames;
	for ( ULONG i = 0; i < ulWADCount; i++ )
		WadNames.Push( NETWORK_ReadString( &g_ByteStream ) );

	// Read the authentication strings and check that it matches our current
	// checksum. If not, inform the user that the demo authentication failed,
	// and display some hopefully helpful information
	FString demoHash = NETWORK_ReadString( &g_ByteStream );
	FString demoMapHash = NETWORK_ReadString( &g_ByteStream );

	// Generate the map collection checksum on our end
	NETWORK_MakeMapCollectionChecksum( );

	if ( demoHash.Compare( g_lumpsAuthenticationChecksum ) != 0 ||
	     demoMapHash.Compare( g_MapCollectionChecksum ) != 0 )
	{
		if ( demo_pure )
		{
			// Tell the user what WADs was the demo recorded with.
			FString error = "\\ciDemo authentication failed. Please ensure that you have the "
				"correct WADs to play back this demo and try again. "
				"This demo uses the following WADs:\n";

			for ( ULONG i = 0; i < ulWADCount; i++ )
				error.AppendFormat( "\\cc- %s%s\\c-\n",
				WadNames[i].GetChars( ), (!i) ? " (IWAD)" : "");

			I_Error( "%s", error.GetChars() );
		}
		else
		{
			// If demo_pure is false, we play the demo back with no
			// regard to the fact that authentication failed. This
			// may cause the demo to be played back incorrectly.
			// Warn the user about the consequences.
			Printf( "\\ckWARNING: Demo authentication failed and demo_pure is false. "
				"The demo is being played back with incorrect WADs and "
				"may get played back incorrectly.\\c-\n" );
		}
	}
	else
		Printf( "Demo authentication successful.\n" );
}

//*****************************************************************************
//
BYTESTREAM_s *CLIENTDEMO_GetDemoStream( void )
{
	return &g_ByteStream;
}

//*****************************************************************************
//*****************************************************************************
//
static void clientdemo_CheckDemoBuffer( ULONG ulSize )
{
	LONG	lPosition;

	// We may need to allocate more memory for our demo buffer.
	if (( g_ByteStream.pbStream + ulSize ) > g_ByteStream.pbStreamEnd )
	{
		// Give us another 128KB of memory.
		g_lMaxDemoLength += 0x20000;
		lPosition = g_ByteStream.pbStream - g_pbDemoBuffer;
		// [BB] Convert our marked position to an offset.
		const LONG markedOffset = g_pbMarkedStreamPosition - g_pbDemoBuffer;
		g_pbDemoBuffer = (BYTE *)M_Realloc( g_pbDemoBuffer, g_lMaxDemoLength );
		g_ByteStream.pbStream = g_pbDemoBuffer + lPosition;
		g_ByteStream.pbStreamEnd = g_pbDemoBuffer + g_lMaxDemoLength;
		// [BB] Restore the marked position based on the new pointer.
		g_pbMarkedStreamPosition = g_pbDemoBuffer + markedOffset;
	}
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD( demo_pause )
{
	// [BB] This command shouldn't do anything if a demo isn't playing.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		return;

	if (g_bDemoPaused)
	{
		g_bDemoPaused = false;
		S_ResumeSound (false);
	}
	else
	{
		g_bDemoPaused = true;
		S_PauseSound (false, false);
	}
}

CCMD( demo_skiptonextmap )
{
	// [Spleen] This command shouldn't do anything if a demo isn't playing.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		return;

	CLIENTDEMO_SetSkippingToNextMap ( true );
}

CCMD( demo_skiptics )
{
	// This command shouldn't do anything if a demo isn't playing.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		return;

	if ( argv.argc() > 1 )
	{
		const int ticsToSkip = atoi ( argv[1] );
		if ( ticsToSkip >= 0 )
			g_ulTicsToSkip = static_cast<ULONG> ( ticsToSkip );
		else
			Printf ( "You can't skip a negative amount of tics!\n" );
	}
}

CCMD( demo_spectatefreely )
{
	// [Spleen] This command shouldn't do anything if a demo isn't playing.
	if ( CLIENTDEMO_IsPlaying( ) == false )
		return;

	const AActor *pCamera = players[consoleplayer].camera;
	if ( pCamera != g_demoCameraPlayer.mo )
	{
		CLIENTDEMO_ClearFreeSpectatorPlayer();
		player_t *p = &g_demoCameraPlayer;
		p->bSpectating = true;
		p->cls = PlayerClasses[p->CurrentPlayerClass].Type;
		p->mo = static_cast<APlayerPawn *> (Spawn (p->cls, pCamera->x, pCamera->y, pCamera->z + pCamera->height , NO_REPLACE));
		p->mo->angle = pCamera->angle;
		p->mo->flags |= (MF_NOGRAVITY);
		p->mo->player = p;
		p->DesiredFOV = p->FOV = 90.f;
		p->crouchfactor = FRACUNIT;
		PLAYER_SetDefaultSpectatorValues ( p );
		players[consoleplayer].camera = g_demoCameraPlayer.mo;
		p->camera = p->mo;
		if ( StatusBar )
			StatusBar->AttachToPlayer ( p );
	}
}
