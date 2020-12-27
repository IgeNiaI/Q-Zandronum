//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003 Brad Carney
// Copyright (C) 2012 Benjamin Berkels
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
// Filename: network_enums.h
//
// Description: Contains enums used for the netcode.
//
//-----------------------------------------------------------------------------

#if ( !defined(__NETWORK_ENUMS_H__) || defined(GENERATE_ENUM_STRINGS) )

#if (!defined(GENERATE_ENUM_STRINGS))
	#define __NETWORK_ENUMS_H__
#endif

#include "EnumToString.h"

//*****************************************************************************
BEGIN_ENUM( ServerConnectionCommand )
{
	// The server has properly received the client's challenge, and is telling
	// the client to authenticate his map.
	ENUM_ELEMENT( SVCC_AUTHENTICATE ),

	// The server received the client's checksum, and it's valid. Now the server
	// is telling the client to load the map.
	ENUM_ELEMENT( SVCC_MAPLOAD ),

	// There was an error during the course of the client trying to connect.
	ENUM_ELEMENT( SVCC_ERROR ),

	ENUM_ELEMENT( NUM_SERVERCONNECT_COMMANDS )
}
END_ENUM( ServerConnectionCommand )

//*****************************************************************************
// Note: If the number of enumerated messages goes beyond 255, commands will need 
// to be changed to a short. Hopefully that won't have to happen.
BEGIN_ENUM( SVC )
{
	ENUM_ELEMENT2 ( SVC_HEADER, NUM_SERVERCONNECT_COMMANDS ),	// GENERAL PROTOCOL COMMANDS
	ENUM_ELEMENT ( SVC_UNRELIABLEPACKET ),
	ENUM_ELEMENT ( SVC_PING ),
	ENUM_ELEMENT ( SVC_NOTHING ),
	ENUM_ELEMENT ( SVC_BEGINSNAPSHOT ),
	ENUM_ELEMENT ( SVC_ENDSNAPSHOT ),
	ENUM_ELEMENT ( SVC_SPAWNPLAYER ),					// PLAYER COMMANDS
	ENUM_ELEMENT ( SVC_SPAWNMORPHPLAYER ),
	ENUM_ELEMENT ( SVC_MOVEPLAYER ),
	ENUM_ELEMENT ( SVC_DAMAGEPLAYER ),
	ENUM_ELEMENT ( SVC_KILLPLAYER ),
	ENUM_ELEMENT ( SVC_SETPLAYERHEALTH ),
	ENUM_ELEMENT ( SVC_SETPLAYERARMOR ),
	ENUM_ELEMENT ( SVC_SETPLAYERSTATE ),
	ENUM_ELEMENT ( SVC_SETPLAYERUSERINFO ),
	ENUM_ELEMENT ( SVC_SETPLAYERFRAGS ),
	ENUM_ELEMENT ( SVC_SETPLAYERPOINTS ),
	ENUM_ELEMENT ( SVC_SETPLAYERWINS ),
	ENUM_ELEMENT ( SVC_SETPLAYERKILLCOUNT ),
	ENUM_ELEMENT ( SVC_SETPLAYERCHATSTATUS ),
	ENUM_ELEMENT ( SVC_SETPLAYERCONSOLESTATUS ),
	ENUM_ELEMENT ( SVC_SETPLAYERMENUSTATUS ),
	ENUM_ELEMENT ( SVC_SETPLAYERLAGGINGSTATUS ),
	ENUM_ELEMENT ( SVC_SETPLAYERREADYTOGOONSTATUS ),
	ENUM_ELEMENT ( SVC_SETPLAYERTEAM ),
	ENUM_ELEMENT ( SVC_SETPLAYERCAMERA ),
	ENUM_ELEMENT ( SVC_SETPLAYERPOISONCOUNT ),
	ENUM_ELEMENT ( SVC_SETPLAYERAMMOCAPACITY ),
	ENUM_ELEMENT ( SVC_SETPLAYERCHEATS ),
	ENUM_ELEMENT ( SVC_SETPLAYERPENDINGWEAPON ),
	ENUM_ELEMENT ( SVC_SETPLAYERPIECES ),
	ENUM_ELEMENT ( SVC_SETPLAYERPSPRITE ),
	ENUM_ELEMENT ( SVC_SETPLAYERBLEND ),
	ENUM_ELEMENT ( SVC_SETPLAYERMAXHEALTH ),
	ENUM_ELEMENT ( SVC_SETPLAYERLIVESLEFT ),
	ENUM_ELEMENT ( SVC_UPDATEPLAYERPING ),
	ENUM_ELEMENT ( SVC_UPDATEPLAYERTIME ),
	ENUM_ELEMENT ( SVC_MOVELOCALPLAYER ),
	ENUM_ELEMENT ( SVC_DISCONNECTPLAYER ),
	ENUM_ELEMENT ( SVC_SETCONSOLEPLAYER ),
	ENUM_ELEMENT ( SVC_CONSOLEPLAYERKICKED ),
	ENUM_ELEMENT ( SVC_GIVEPLAYERMEDAL ),
	ENUM_ELEMENT ( SVC_RESETALLPLAYERSFRAGCOUNT ),
	ENUM_ELEMENT ( SVC_PLAYERISSPECTATOR ),
	ENUM_ELEMENT ( SVC_PLAYERSAY ),
	ENUM_ELEMENT ( SVC_PLAYERTAUNT ),
	ENUM_ELEMENT ( SVC_PLAYERRESPAWNINVULNERABILITY ),
	ENUM_ELEMENT ( SVC_PLAYERUSEINVENTORY ),
	ENUM_ELEMENT ( SVC_PLAYERDROPINVENTORY ),
	ENUM_ELEMENT ( SVC_SPAWNTHING ),						// THING COMMANDS
	ENUM_ELEMENT ( SVC_SPAWNTHINGNONETID ),
	ENUM_ELEMENT ( SVC_SPAWNTHINGEXACT ),
	ENUM_ELEMENT ( SVC_SPAWNTHINGEXACTNONETID ),
	ENUM_ELEMENT ( SVC_MOVETHING ),
	ENUM_ELEMENT ( SVC_MOVETHINGEXACT ),
	ENUM_ELEMENT ( SVC_KILLTHING ),
	ENUM_ELEMENT ( SVC_SETTHINGSTATE ),
	ENUM_ELEMENT ( SVC_SETTHINGTARGET ),
	ENUM_ELEMENT ( SVC_DESTROYTHING ),
	ENUM_ELEMENT ( SVC_SETTHINGANGLE ),
	ENUM_ELEMENT ( SVC_SETTHINGANGLEEXACT ),
	ENUM_ELEMENT ( SVC_SETTHINGWATERLEVEL ),
	ENUM_ELEMENT ( SVC_SETTHINGFLAGS ),
	ENUM_ELEMENT ( SVC_SETTHINGARGUMENTS ),
	ENUM_ELEMENT ( SVC_SETTHINGTRANSLATION ),
	ENUM_ELEMENT ( SVC_SETTHINGSOUND ),
	ENUM_ELEMENT ( SVC_SETTHINGSPAWNPOINT ),
	ENUM_ELEMENT ( SVC_SETTHINGSPECIAL1 ),
	ENUM_ELEMENT ( SVC_SETTHINGSPECIAL2 ),
	ENUM_ELEMENT ( SVC_SETTHINGTICS ),
	ENUM_ELEMENT ( SVC_SETTHINGTID ),
	ENUM_ELEMENT ( SVC_SETTHINGGRAVITY ),
	ENUM_ELEMENT ( SVC_SETTHINGFRAME ),
	ENUM_ELEMENT ( SVC_SETTHINGFRAMENF ),
	ENUM_ELEMENT ( SVC_SETACTORPROPERTY ),
	ENUM_ELEMENT ( SVC_SETWEAPONAMMOGIVE ),
	ENUM_ELEMENT ( SVC_THINGISCORPSE ),
	ENUM_ELEMENT ( SVC_HIDETHING ),
	ENUM_ELEMENT ( SVC_TELEPORTTHING ),
	ENUM_ELEMENT ( SVC_THINGACTIVATE ),
	ENUM_ELEMENT ( SVC_THINGDEACTIVATE ),
	ENUM_ELEMENT ( SVC_RESPAWNDOOMTHING ),
	ENUM_ELEMENT ( SVC_RESPAWNRAVENTHING ),
	ENUM_ELEMENT ( SVC_SPAWNBLOOD ),
	ENUM_ELEMENT ( SVC_SPAWNPUFF ),
	ENUM_ELEMENT ( SVC_SPAWNPUFFNONETID ),
	ENUM_ELEMENT ( SVC_PRINT ),							// PRINT COMMANDS
	ENUM_ELEMENT ( SVC_PRINTMID ),
	ENUM_ELEMENT ( SVC_PRINTMOTD ),
	ENUM_ELEMENT ( SVC_PRINTHUDMESSAGE ),
	ENUM_ELEMENT ( SVC_PRINTHUDMESSAGEFADEOUT ),
	ENUM_ELEMENT ( SVC_PRINTHUDMESSAGEFADEINOUT ),
	ENUM_ELEMENT ( SVC_PRINTHUDMESSAGETYPEONFADEOUT ),
	ENUM_ELEMENT ( SVC_SETGAMEMODE ),					// GAME COMMANDS
	ENUM_ELEMENT ( SVC_SETGAMESKILL ),
	ENUM_ELEMENT ( SVC_SETGAMEDMFLAGS ),
	ENUM_ELEMENT ( SVC_SETGAMEMODELIMITS ),
	ENUM_ELEMENT ( SVC_SETGAMEENDLEVELDELAY ),
	ENUM_ELEMENT ( SVC_SETGAMEMODESTATE ),
	ENUM_ELEMENT ( SVC_SETDUELNUMDUELS ),
	ENUM_ELEMENT ( SVC_SETLMSSPECTATORSETTINGS ),
	ENUM_ELEMENT ( SVC_SETLMSALLOWEDWEAPONS ),
	ENUM_ELEMENT ( SVC_SETINVASIONNUMMONSTERSLEFT ),
	ENUM_ELEMENT ( SVC_SETINVASIONWAVE ),
	ENUM_ELEMENT ( SVC_SETSIMPLECTFSTMODE ),
	ENUM_ELEMENT ( SVC_DOPOSSESSIONARTIFACTPICKEDUP ),
	ENUM_ELEMENT ( SVC_DOPOSSESSIONARTIFACTDROPPED ),
	ENUM_ELEMENT ( SVC_DOGAMEMODEFIGHT ),
	ENUM_ELEMENT ( SVC_DOGAMEMODECOUNTDOWN ),
	ENUM_ELEMENT ( SVC_DOGAMEMODEWINSEQUENCE ),
	ENUM_ELEMENT ( SVC_SETDOMINATIONSTATE ),
	ENUM_ELEMENT ( SVC_SETDOMINATIONPOINTOWNER ),
	ENUM_ELEMENT ( SVC_SETTEAMFRAGS ),					// TEAM COMMANDS
	ENUM_ELEMENT ( SVC_SETTEAMSCORE ),
	ENUM_ELEMENT ( SVC_SETTEAMWINS ),
	ENUM_ELEMENT ( SVC_SETTEAMRETURNTICKS ),
	ENUM_ELEMENT ( SVC_TEAMFLAGRETURNED ),
	ENUM_ELEMENT ( SVC_TEAMFLAGDROPPED ),
	ENUM_ELEMENT ( SVC_SPAWNMISSILE ),					// MISSILE COMMANDS
	ENUM_ELEMENT ( SVC_SPAWNMISSILEEXACT ),
	ENUM_ELEMENT ( SVC_MISSILEEXPLODE ),
	ENUM_ELEMENT ( SVC_WEAPONSOUND ),					// WEAPON COMMANDS
	ENUM_ELEMENT ( SVC_WEAPONCHANGE ),
	ENUM_ELEMENT ( SVC_WEAPONRAILGUN ),
	ENUM_ELEMENT ( SVC_SETSECTORFLOORPLANE ),			// SECTOR COMMANDS
	ENUM_ELEMENT ( SVC_SETSECTORCEILINGPLANE ),
	ENUM_ELEMENT ( SVC_SETSECTORFLOORPLANESLOPE ),
	ENUM_ELEMENT ( SVC_SETSECTORCEILINGPLANESLOPE ),
	ENUM_ELEMENT ( SVC_SETSECTORLIGHTLEVEL ),
	ENUM_ELEMENT ( SVC_SETSECTORCOLOR ),
	ENUM_ELEMENT ( SVC_SETSECTORCOLORBYTAG ),
	ENUM_ELEMENT ( SVC_SETSECTORFADE ),
	ENUM_ELEMENT ( SVC_SETSECTORFADEBYTAG ),
	ENUM_ELEMENT ( SVC_SETSECTORFLAT ),
	ENUM_ELEMENT ( SVC_SETSECTORPANNING ),
	ENUM_ELEMENT ( SVC_SETSECTORROTATION ),
	ENUM_ELEMENT ( SVC_SETSECTORROTATIONBYTAG ),
	ENUM_ELEMENT ( SVC_SETSECTORSCALE ),
	ENUM_ELEMENT ( SVC_SETSECTORSPECIAL ),
	ENUM_ELEMENT ( SVC_SETSECTORFRICTION ),
	ENUM_ELEMENT ( SVC_SETSECTORANGLEYOFFSET ),
	ENUM_ELEMENT ( SVC_SETSECTORGRAVITY ),
	ENUM_ELEMENT ( SVC_SETSECTORREFLECTION ),
	ENUM_ELEMENT ( SVC_STOPSECTORLIGHTEFFECT ),
	ENUM_ELEMENT ( SVC_DESTROYALLSECTORMOVERS ),
	ENUM_ELEMENT ( SVC_DOSECTORLIGHTFIREFLICKER ),		// SECTOR LIGHT COMMANDS
	ENUM_ELEMENT ( SVC_DOSECTORLIGHTFLICKER ),
	ENUM_ELEMENT ( SVC_DOSECTORLIGHTLIGHTFLASH ),
	ENUM_ELEMENT ( SVC_DOSECTORLIGHTSTROBE ),
	ENUM_ELEMENT ( SVC_DOSECTORLIGHTGLOW ),
	ENUM_ELEMENT ( SVC_DOSECTORLIGHTGLOW2 ),
	ENUM_ELEMENT ( SVC_DOSECTORLIGHTPHASED ),
	ENUM_ELEMENT ( SVC_SETLINEALPHA ),					// LINE COMMANDS
	ENUM_ELEMENT ( SVC_SETLINETEXTURE ),
	ENUM_ELEMENT ( SVC_SETLINETEXTUREBYID ),
	ENUM_ELEMENT ( SVC_SETSOMELINEFLAGS ),
	ENUM_ELEMENT ( SVC_SETSIDEFLAGS ),					// SIDE COMMANDS
	ENUM_ELEMENT ( SVC_ACSSCRIPTEXECUTE ),				// ACS COMMANDS
	ENUM_ELEMENT ( SVC_SOUND ),							// SOUND COMMANDS
	ENUM_ELEMENT ( SVC_SOUNDACTOR ),
	ENUM_ELEMENT ( SVC_SOUNDACTORIFNOTPLAYING ),
	ENUM_ELEMENT ( SVC_SOUNDPOINT ),
	ENUM_ELEMENT ( SVC_STARTSECTORSEQUENCE ),			// SECTOR SEQUENCE COMMANDS
	ENUM_ELEMENT ( SVC_STOPSECTORSEQUENCE ),
	ENUM_ELEMENT ( SVC_CALLVOTE ),						// VOTING COMMANDS
	ENUM_ELEMENT ( SVC_PLAYERVOTE ),
	ENUM_ELEMENT ( SVC_VOTEENDED ),
	ENUM_ELEMENT ( SVC_MAPLOAD ),						// MAP COMMANDS
	ENUM_ELEMENT ( SVC_MAPNEW ),
	ENUM_ELEMENT ( SVC_MAPEXIT ),
	ENUM_ELEMENT ( SVC_MAPAUTHENTICATE ),
	ENUM_ELEMENT ( SVC_SETMAPTIME ),
	ENUM_ELEMENT ( SVC_SETMAPNUMKILLEDMONSTERS ),
	ENUM_ELEMENT ( SVC_SETMAPNUMFOUNDITEMS ),
	ENUM_ELEMENT ( SVC_SETMAPNUMFOUNDSECRETS ),
	ENUM_ELEMENT ( SVC_SETMAPNUMTOTALMONSTERS ),
	ENUM_ELEMENT ( SVC_SETMAPNUMTOTALITEMS ),
	ENUM_ELEMENT ( SVC_SETMAPMUSIC ),
	ENUM_ELEMENT ( SVC_SETMAPSKY ),
	ENUM_ELEMENT ( SVC_GIVEINVENTORY ),					// INVENTORY COMMANDS
	ENUM_ELEMENT ( SVC_TAKEINVENTORY ),
	ENUM_ELEMENT ( SVC_GIVEPOWERUP ),
	ENUM_ELEMENT ( SVC_DOINVENTORYPICKUP ),
	ENUM_ELEMENT ( SVC_DESTROYALLINVENTORY ),
	ENUM_ELEMENT ( SVC_DODOOR ),						// DOOR COMMANDS
	ENUM_ELEMENT ( SVC_DOFLOOR ),						// FLOOR COMMANDS
	ENUM_ELEMENT ( SVC_DOCEILING ),						// CEILING COMMANDS
	ENUM_ELEMENT ( SVC_DOPLAT ),						// PLAT COMMANDS
	ENUM_ELEMENT ( SVC_DOELEVATOR ),					// ELEVATOR COMMANDS
	ENUM_ELEMENT ( SVC_DOPILLAR ),						// PILLAR COMMANDS
	ENUM_ELEMENT ( SVC_DOWAGGLE ),						// WAGGLE COMMANDS
	ENUM_ELEMENT ( SVC_DOROTATEPOLY ),					// POLY COMMANDS
	ENUM_ELEMENT ( SVC_DOMOVEPOLY ),
	ENUM_ELEMENT ( SVC_DOMOVEPOLYTO ),
	ENUM_ELEMENT ( SVC_DOPOLYDOOR ),
	ENUM_ELEMENT ( SVC_EARTHQUAKE ),					// MISC. COMMANDS
	ENUM_ELEMENT ( SVC_DOSCROLLER ),
	ENUM_ELEMENT ( SVC_SETSCROLLER ),
	ENUM_ELEMENT ( SVC_SETWALLSCROLLER ),
	ENUM_ELEMENT ( SVC_DOFLASHFADER ),
	ENUM_ELEMENT ( SVC_GENERICCHEAT ),
	ENUM_ELEMENT ( SVC_SETCAMERATOTEXTURE ),
	ENUM_ELEMENT ( SVC_CREATETRANSLATION ),
	ENUM_ELEMENT ( SVC_IGNOREPLAYER ),
	ENUM_ELEMENT ( SVC_SPAWNBLOODSPLATTER ),
	ENUM_ELEMENT ( SVC_SPAWNBLOODSPLATTER2 ),
	ENUM_ELEMENT ( SVC_CREATETRANSLATION2 ),
	ENUM_ELEMENT ( SVC_REPLACETEXTURES ),
	ENUM_ELEMENT ( SVC_SETSECTORLINK ),
	ENUM_ELEMENT ( SVC_DOPUSHER ),
	ENUM_ELEMENT ( SVC_ADJUSTPUSHER ),
	ENUM_ELEMENT ( SVC_ANNOUNCERSOUND ),
	ENUM_ELEMENT ( SVC_EXTENDEDCOMMAND ),

	ENUM_ELEMENT ( NUM_SERVER_COMMANDS ),
}
END_ENUM( SVC )

//*****************************************************************************
// [BB] We are almost at 255 normal commands. In order to prevent having to use a short to identify all of them,
// collect less used ones in SVC2_* and mark those with SVC_EXTENDEDCOMMAND. Unless we need more than 512 commands,
// this is more effective than indexing by a short.
BEGIN_ENUM( SVC2 )
{
	ENUM_ELEMENT ( SVC2_SETINVENTORYICON ),
	ENUM_ELEMENT ( SVC2_FULLUPDATECOMPLETED ),
	ENUM_ELEMENT ( SVC2_SETIGNOREWEAPONSELECT ),
	ENUM_ELEMENT ( SVC2_CLEARCONSOLEPLAYERWEAPON ),
	ENUM_ELEMENT ( SVC2_LIGHTNING ),
	ENUM_ELEMENT ( SVC2_CANCELFADE ),
	ENUM_ELEMENT ( SVC2_PLAYBOUNCESOUND ),
	ENUM_ELEMENT ( SVC2_GIVEWEAPONHOLDER ),
	ENUM_ELEMENT ( SVC2_SETHEXENARMORSLOTS ),
	ENUM_ELEMENT ( SVC2_SETTHINGREACTIONTIME ),
	ENUM_ELEMENT ( SVC2_SETFASTCHASESTRAFECOUNT ),
	ENUM_ELEMENT ( SVC2_RESETMAP ),
	ENUM_ELEMENT ( SVC2_SETPOWERUPBLENDCOLOR ),
	ENUM_ELEMENT ( SVC2_SETPLAYERHAZARDCOUNT ),
	ENUM_ELEMENT ( SVC2_SCROLL3DMIDTEX ),
	ENUM_ELEMENT ( SVC2_SETPLAYERLOGNUMBER ),
	ENUM_ELEMENT ( SVC2_SETTHINGSPECIAL ),
	ENUM_ELEMENT ( SVC2_SYNCPATHFOLLOWER ),
	ENUM_ELEMENT ( SVC2_SETTHINGHEALTH ),
	ENUM_ELEMENT ( SVC2_SETCVAR ),
	ENUM_ELEMENT ( SVC2_BUILDSTAIR ),
	ENUM_ELEMENT ( SVC2_SETMUGSHOTSTATE ),
	ENUM_ELEMENT ( SVC2_SETTHINGSCALE ),
	ENUM_ELEMENT ( SVC2_SETMAPNUMTOTALSECRETS ),
	ENUM_ELEMENT ( SVC2_SOUNDSECTOR ),
	ENUM_ELEMENT ( SVC2_SETPLAYERVIEWHEIGHT ),
	ENUM_ELEMENT ( SVC2_SHOOTDECAL ),
	ENUM_ELEMENT ( SVC2_SYNCJOINQUEUE ),
	ENUM_ELEMENT ( SVC2_PUSHTOJOINQUEUE ),
	ENUM_ELEMENT ( SVC2_REMOVEFROMJOINQUEUE ),
	ENUM_ELEMENT ( SVC2_SETDEFAULTSKYBOX ),
	ENUM_ELEMENT ( SVC2_FLASHSTEALTHMONSTER ),
	ENUM_ELEMENT ( SVC2_LEVELSPAWNTHING ),
	ENUM_ELEMENT ( SVC2_LEVELSPAWNTHINGNONETID ),
	ENUM_ELEMENT ( SVC2_SETPLAYERACCOUNTNAME ),
	ENUM_ELEMENT ( SVC2_SECRETFOUND ),
	ENUM_ELEMENT ( SVC2_SECRETMARKSECTORFOUND ),
	ENUM_ELEMENT ( SVC2_SETPLAYERDEATHS ),
	// [BB] Commands necessary for the account system.
	ENUM_ELEMENT ( SVC2_SRP_USER_START_AUTHENTICATION ),
	ENUM_ELEMENT ( SVC2_SRP_USER_PROCESS_CHALLENGE ),
	ENUM_ELEMENT ( SVC2_SRP_USER_VERIFY_SESSION ),

	ENUM_ELEMENT ( NUM_SVC2_COMMANDS ),
}
END_ENUM( SVC2 )

BEGIN_ENUM( CLCC )
{
	// Client is telling the server he wishes to connect.
	ENUM_ELEMENT( CLCC_ATTEMPTCONNECTION ),

	// Client is attempting to authenticate the map.
	ENUM_ELEMENT( CLCC_ATTEMPTAUTHENTICATION ),

	// Client has loaded the map, and is requesting the snapshot.
	ENUM_ELEMENT( CLCC_REQUESTSNAPSHOT ),

	ENUM_ELEMENT( NUM_CLIENTCONNECT_COMMANDS )
}
END_ENUM( CLCC )

BEGIN_ENUM( CLC )
{
	ENUM_ELEMENT2( CLC_USERINFO, NUM_CLIENTCONNECT_COMMANDS ),
	ENUM_ELEMENT( CLC_QUIT ),
	ENUM_ELEMENT( CLC_STARTCHAT ),
	ENUM_ELEMENT( CLC_ENDCHAT ),
	ENUM_ELEMENT( CLC_SAY ),
	ENUM_ELEMENT( CLC_CLIENTMOVE ),
	ENUM_ELEMENT( CLC_MISSINGPACKET ),
	ENUM_ELEMENT( CLC_PONG ),
	ENUM_ELEMENT( CLC_WEAPONSELECT ),
	ENUM_ELEMENT( CLC_TAUNT ),
	ENUM_ELEMENT( CLC_SPECTATE ),
	ENUM_ELEMENT( CLC_REQUESTJOIN ),
	ENUM_ELEMENT( CLC_REQUESTRCON ),
	ENUM_ELEMENT( CLC_RCONCOMMAND ),
	ENUM_ELEMENT( CLC_SUICIDE ),
	ENUM_ELEMENT( CLC_CHANGETEAM ),
	ENUM_ELEMENT( CLC_SPECTATEINFO ),
	ENUM_ELEMENT( CLC_GENERICCHEAT ),
	ENUM_ELEMENT( CLC_GIVECHEAT ),
	ENUM_ELEMENT( CLC_TAKECHEAT ),
	ENUM_ELEMENT( CLC_SUMMONCHEAT ),
	ENUM_ELEMENT( CLC_READYTOGOON ),
	ENUM_ELEMENT( CLC_CHANGEDISPLAYPLAYER ),
	ENUM_ELEMENT( CLC_AUTHENTICATELEVEL ),
	ENUM_ELEMENT( CLC_CALLVOTE ),
	ENUM_ELEMENT( CLC_VOTEYES ),
	ENUM_ELEMENT( CLC_VOTENO ),
	ENUM_ELEMENT( CLC_INVENTORYUSEALL ),
	ENUM_ELEMENT( CLC_INVENTORYUSE ),
	ENUM_ELEMENT( CLC_INVENTORYDROP ),
	ENUM_ELEMENT( CLC_SUMMONFRIENDCHEAT ),
	ENUM_ELEMENT( CLC_SUMMONFOECHEAT ),
	ENUM_ELEMENT( CLC_ENTERCONSOLE ),
	ENUM_ELEMENT( CLC_EXITCONSOLE ),
	ENUM_ELEMENT( CLC_ENTERMENU ),
	ENUM_ELEMENT( CLC_EXITMENU ),
	ENUM_ELEMENT( CLC_IGNORE ),
	ENUM_ELEMENT( CLC_PUKE ),
	ENUM_ELEMENT( CLC_MORPHEX ),
	ENUM_ELEMENT( CLC_FULLUPDATE ),
	ENUM_ELEMENT( CLC_INFOCHEAT ),
	ENUM_ELEMENT( CLC_WARPCHEAT ),
	ENUM_ELEMENT( CLC_KILLCHEAT ),
	ENUM_ELEMENT( CLC_SPECIALCHEAT ),
	ENUM_ELEMENT( CLC_SETWANTHIDEACCOUNT ),
	ENUM_ELEMENT( CLC_SETVIDEORESOLUTION ),

	ENUM_ELEMENT( NUM_CLIENT_COMMANDS )
}
END_ENUM( CLC )

BEGIN_ENUM( CLC_SRP )
{
	ENUM_ELEMENT2( CLC_SRP_USER_REQUEST_LOGIN, NUM_CLIENT_COMMANDS ),
	ENUM_ELEMENT( CLC_SRP_USER_START_AUTHENTICATION ),
	ENUM_ELEMENT( CLC_SRP_USER_PROCESS_CHALLENGE ),
}
END_ENUM( CLC_SRP )

BEGIN_ENUM ( NetworkErrorCode )
{
	// Client has the wrong password.
	ENUM_ELEMENT( NETWORK_ERRORCODE_WRONGPASSWORD ),

	// Client has the wrong version.
	ENUM_ELEMENT( NETWORK_ERRORCODE_WRONGVERSION ),

	// Client is using a version with different network protocol.
	ENUM_ELEMENT( NETWORK_ERRORCODE_WRONGPROTOCOLVERSION ),

	// Client has been banned.
	ENUM_ELEMENT( NETWORK_ERRORCODE_BANNED ),

	// The server is full.
	ENUM_ELEMENT( NETWORK_ERRORCODE_SERVERISFULL ),

	// Client has the wrong version of the current level.
	ENUM_ELEMENT( NETWORK_ERRORCODE_AUTHENTICATIONFAILED ),

	// [RC] Too many connections from the IP.
	ENUM_ELEMENT( NETWORK_ERRORCODE_TOOMANYCONNECTIONSFROMIP ),

	// [BB] The protected lump authentication failed.
	ENUM_ELEMENT( NETWORK_ERRORCODE_PROTECTED_LUMP_AUTHENTICATIONFAILED ),

	// [TP] The client sent bad userinfo
	ENUM_ELEMENT( NETWORK_ERRORCODE_USERINFOREJECTED ),

	ENUM_ELEMENT( NUM_NETWORK_ERRORCODES )
}
END_ENUM ( NetworkErrorCode )

BEGIN_ENUM ( SpawnFlags )
{
	ENUM_ELEMENT2( SPAWNFLAG_PUFF, 1 ),
	ENUM_ELEMENT2( SPAWNFLAG_LEVELTHING, 2 ),
}
END_ENUM ( SpawnFlags )

BEGIN_ENUM ( SecretFoundFlags )
{
	ENUM_ELEMENT2( SECRETFOUND_SOUND, 1 ),
	ENUM_ELEMENT2( SECRETFOUND_MESSAGE, 2 ),
}
END_ENUM( SecretFoundFlags )

#endif	// ( !defined(__NETWORK_ENUMS_H__) || defined(GENERATE_ENUM_STRINGS) )
