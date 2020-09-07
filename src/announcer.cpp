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
// Filename: announcer.cpp
//
// Description: Contains announcer functions
//
//-----------------------------------------------------------------------------

#include "announcer.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "doomstat.h"
#include "d_player.h"
#include "deathmatch.h"
#include "doomtype.h"
#include "gamemode.h"
#include "i_system.h"
#include "s_sound.h"
#include "sc_man.h"
#include "team.h"
#include "w_wad.h"

//*****************************************************************************
//  STRUCTURES

class AnnouncerProfile
{
	public:
		AnnouncerProfile() : name("UNNAMED ANNOUNCER")
		{
		}

		// Adds an entry to the announcer.
		void AddEntry(const FName &entry, const FString &sound)
		{
			entries[entry] = sound;
		}

		// Returns true if the entry has a sound for this announcer.
		bool EntryExists(const FName &entry) const
		{
			return entries.CheckKey(entry) != NULL;
		}

		// Returns the sound for an announcer entry or an null string if no
		// sound is set.
		FString FindEntry(const FName &entry) const
		{
			if ( EntryExists(entry) )
				return *entries.CheckKey(entry);
			return FString();
		}

		const FString &GetName() const
		{
			return name;
		}

		// Pull in all defined sounds.
		void Merge(const AnnouncerProfile &other)
		{
			EntryMap::ConstIterator iter(other.entries);
			EntryMap::ConstPair *pair;

			while ( iter.NextPair(pair) )
				entries.Insert(pair->Key, pair->Value);
		}

		void SetName(const FString &name)
		{
			this->name = name;
		}

	private:
		typedef TMap<FName, FString> EntryMap;

		FString		name;
		EntryMap	entries;
};

//*****************************************************************************
//	VARIABLES

static	TArray<AnnouncerProfile>	g_AnnouncerProfile;
static	AnnouncerProfile			*g_DefaultAnnouncer;

// Have the "Three frags left!", etc. sounds been played yet?
static	bool			g_bThreeFragsLeftSoundPlayed;
static	bool			g_bTwoFragsLeftSoundPlayed;
static	bool			g_bOneFragLeftSoundPlayed;

// Have the "Three points left!", etc. sounds been played yet?
static	bool			g_bThreePointsLeftSoundPlayed;
static	bool			g_bTwoPointsLeftSoundPlayed;
static	bool			g_bOnePointLeftSoundPlayed;

static	LONG			g_lLastSoundID = 0;

CVAR (Bool, cl_alwaysplayfragsleft, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, cl_allowmultipleannouncersounds, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// [WS] Adjusts the announcer's volume.
CUSTOM_CVAR (Float, snd_announcervolume, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
}

//*****************************************************************************
//	PROTOTYPES

static	void					announcer_ParseAnnouncerInfoLump( FScanner &sc );

//*****************************************************************************
//	FUNCTIONS

void ANNOUNCER_Construct( void )
{
	AnnouncerProfile def;
	def.SetName("Default");
	g_AnnouncerProfile.Push(def);

	g_DefaultAnnouncer = &g_AnnouncerProfile[0];
}

//*****************************************************************************
//
void ANNOUNCER_Destruct( void )
{
}

//*****************************************************************************
//
void ANNOUNCER_ParseAnnouncerInfo( void )
{
	LONG		lCurLump;
	LONG		lLastLump = 0;

	// Search through all loaded wads for a lump called "ANCRINFO".
	while (( lCurLump = Wads.FindLump( "ANCRINFO", (int *)&lLastLump )) != -1 )
	{
		// Make pszBotInfo point to the raw data (which should be a text file) in the ANCRINFO lump.
		FScanner sc( lCurLump );

		// Parse the lump.
		announcer_ParseAnnouncerInfoLump( sc );
	}
}

//*****************************************************************************
//
ULONG ANNOUNCER_GetNumProfiles( void )
{
	return g_AnnouncerProfile.Size();
}

//*****************************************************************************
//

bool ANNOUNCER_DoesEntryExist( ULONG ulProfileIdx, const char *pszEntry )
{
	// Return false if the profile index is invalid.
	if ( ulProfileIdx >= ANNOUNCER_GetNumProfiles() )
		return ( false );

	// If the entry exists in the profile, return true.
	return g_AnnouncerProfile[ulProfileIdx].EntryExists(pszEntry);
}

//*****************************************************************************
//

void ANNOUNCER_PlayEntry( ULONG ulProfileIdx, const char *pszEntry )
{
	// Return false if the profile index is invalid
	if ( ulProfileIdx >= ANNOUNCER_GetNumProfiles() )
		return;

	FString sound;
	if ( g_AnnouncerProfile[ulProfileIdx].EntryExists(pszEntry) )
		sound = g_AnnouncerProfile[ulProfileIdx].FindEntry(pszEntry);
	else if ( g_DefaultAnnouncer && g_DefaultAnnouncer->EntryExists(pszEntry) )
		sound = g_DefaultAnnouncer->FindEntry(pszEntry);

	// If the entry exists and has a sound, play it.
	if ( !sound.IsEmpty() )
	{
		// Stop any existing announcer sounds.
		// [BB] Only do this, if the user doesn't like multiple announcer sounds at once
		if ( cl_allowmultipleannouncersounds == false )
			S_StopSoundID( g_lLastSoundID, CHAN_VOICE );

		// Play the sound.
		g_lLastSoundID = S_FindSound( sound );
		// [WS] Added snd_announcervolume.
		S_Sound( CHAN_VOICE | CHAN_UI, sound, snd_announcervolume, ATTN_NONE );
	}
}

//*****************************************************************************
//
void ANNOUNCER_PotentiallyPlayFragsLeftSound( LONG lFragsLeft )
{
	switch ( lFragsLeft )
	{
	case 3:

		if ( (( g_bThreeFragsLeftSoundPlayed == false ) &&
			( g_bTwoFragsLeftSoundPlayed == false ) &&
			( g_bOneFragLeftSoundPlayed == false ))
			|| cl_alwaysplayfragsleft )
		{
			ANNOUNCER_PlayEntry( cl_announcer, "ThreeFragsLeft" );
			g_bThreeFragsLeftSoundPlayed = true;
		}
		break;
	case 2:

		if ( (( g_bTwoFragsLeftSoundPlayed == false ) &&
			( g_bOneFragLeftSoundPlayed == false ))
			|| cl_alwaysplayfragsleft )
		{
			ANNOUNCER_PlayEntry( cl_announcer, "TwoFragsLeft" );
			g_bTwoFragsLeftSoundPlayed = true;
		}
		break;
	case 1:

		if ( (g_bOneFragLeftSoundPlayed == false)
			|| cl_alwaysplayfragsleft )
		{
			ANNOUNCER_PlayEntry( cl_announcer, "OneFragLeft" );
			g_bOneFragLeftSoundPlayed = true;
		}
		break;
	}
}

//*****************************************************************************
//
void ANNOUNCER_PlayFragSounds( ULONG ulPlayer, LONG lOldFragCount, LONG lNewFragCount )
{
	ULONG			ulIdx;
	LONG			lOldLeaderFrags;
	LONG			lNewLeaderFrags;
	LEADSTATE_e		OldLeadState;
	LEADSTATE_e		LeadState;

	// Announce a lead change. Only do if the consoleplayer's camera is valid.
	if (( players[consoleplayer].mo != NULL ) &&
		( players[consoleplayer].camera != NULL ) &&
		( players[consoleplayer].camera->player != NULL ) &&
		( players[consoleplayer].camera->player->bSpectating == false ))
	{
		// Find the highest fragcount of a player other than the player whose eye's we're
		// looking through. This is compared to the player's old fragcount, and the player's
		// new fragcount. If the player's lead state has changed, play a sound.
		lOldLeaderFrags = INT_MIN;
		lNewLeaderFrags = INT_MIN;
		for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
		{
			// Don't factor our fragcount into the highest fragcount.
			if ( players[ulIdx].mo->CheckLocalView( consoleplayer ))
				continue;

			// Don't factor in players who are not in the game, or who are spectating.
			if (( playeringame[ulIdx] == false ) || ( players[ulIdx].bSpectating ))
				continue;

			if ( ulIdx == ulPlayer )
			{
				if ( lOldLeaderFrags < lOldFragCount )
					lOldLeaderFrags = lOldFragCount;
				if ( lNewLeaderFrags < lNewFragCount )
					lNewLeaderFrags = lNewFragCount;
			}
			else
			{
				if ( lOldLeaderFrags < players[ulIdx].fragcount )
					lOldLeaderFrags = players[ulIdx].fragcount;
				if ( lNewLeaderFrags < players[ulIdx].fragcount )
					lNewLeaderFrags = players[ulIdx].fragcount;
			}
		}

		// If players were found in game, just break out, since our lead couldn't have possibly changed.
		if (( lOldLeaderFrags == INT_MIN ) && ( lNewLeaderFrags == INT_MIN ))
			return;

		if ( players[ulPlayer].mo->CheckLocalView( consoleplayer ))
		{
			OldLeadState = ( lOldFragCount > lOldLeaderFrags ) ? LEADSTATE_INTHELEAD : ( lOldFragCount == lOldLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
			LeadState = ( lNewFragCount > lNewLeaderFrags ) ? LEADSTATE_INTHELEAD : ( lNewFragCount == lNewLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
		}
		else
		{
			OldLeadState = ( players[consoleplayer].camera->player->fragcount > lOldLeaderFrags ) ? LEADSTATE_INTHELEAD : ( players[consoleplayer].camera->player->fragcount == lOldLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
			LeadState = ( players[consoleplayer].camera->player->fragcount > lNewLeaderFrags ) ? LEADSTATE_INTHELEAD : ( players[consoleplayer].camera->player->fragcount == lNewLeaderFrags ) ? LEADSTATE_TIEDFORTHELEAD : LEADSTATE_NOTINTHELEAD;
		}

		// If our lead state has changed, play a sound.
		if ( OldLeadState != LeadState )
		{
			switch ( LeadState )
			{
			// Display player has taken the lead!
			case LEADSTATE_INTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouveTakenTheLead" );
				break;
			// Display player is tied for the lead!
			case LEADSTATE_TIEDFORTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouAreTiedForTheLead" );
				break;
			// Display player has lost the lead.
			case LEADSTATE_NOTINTHELEAD:

				ANNOUNCER_PlayEntry( cl_announcer, "YouveLostTheLead" );
				break;
			}
		}
	}

	// Potentially play the "3 frags left", etc. announcer sounds.
	if (( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSEARNFRAGS ) && ( fraglimit ))
	{
		ANNOUNCER_PotentiallyPlayFragsLeftSound( fraglimit - lNewFragCount );
	}
}

//*****************************************************************************
//
void ANNOUNCER_PlayTeamFragSounds( ULONG ulTeam, LONG lOldFragCount, LONG lNewFragCount )
{
	LONG			lScore[MAX_TEAMS];
	LONG			lHighestFragCount = LONG_MIN;
	bool			bPossibleTeams[MAX_TEAMS];
	ULONG			ulNumberOfPossibleTeams = 0;
	ULONG			ulMaxPossibleTeams = 0;

	// The given team is not valid.
	if ( TEAM_CheckIfValid( ulTeam ) == false )
		return;

	// [BB] Don't announce anything if there is at most one team with players.
	if ( TEAM_TeamsWithPlayersOn() < 2 )
		return;

	// Set all possible teams to false.
	for ( ULONG i = 0; i < MAX_TEAMS; i++ )
	{
		bPossibleTeams[i] = false;
		lScore[i] = LONG_MIN;
	}

	// Find the scores for each of the teams.
	// [BB] Only set the frag count of the available teams (the others stay at LONG_MIN).
	for ( ULONG i = 0; i < TEAM_GetNumAvailableTeams(); i++ )
	{
		if ( i == ulTeam )
			lScore[i] = lNewFragCount;
		else
			lScore[i] = TEAM_GetFragCount( i );

		if ( lScore[i] > lHighestFragCount)
			lHighestFragCount = lScore[i];
	}

	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		// Set the possible teams.
		if ( lScore[i] == lHighestFragCount )
		{
			bPossibleTeams[i] = true;
			ulNumberOfPossibleTeams++;
		}

		// Count the maximum teams used.
		if ( TEAM_CountPlayers( i ) < 1 )
			continue;

		ulMaxPossibleTeams++;
	}

	// Play the "teams are tied" sound if all active teams have equal scores.
	if ( ulNumberOfPossibleTeams == ulMaxPossibleTeams )
	{
		// [BB] Make sure that the next leading team will be announced again.
		for ( ULONG j = 0; j < teams.Size( ); ++j )
			TEAM_SetAnnouncedLeadState( j, false );

		ANNOUNCER_PlayEntry( cl_announcer, "TeamsAreTied");
		return;
	}

	// Don't allow any sounds to play if more than one teams are leading.
	if ( ulNumberOfPossibleTeams > 1 )
	{
		return;
	}

	// Announce the team leading message.
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( bPossibleTeams[i] == false || TEAM_GetAnnouncedLeadState( i ))
			continue;

		// Lead state - make sure we only announce a team leading once and not
		// every frag.
		for ( ULONG j = 0; j < teams.Size( ); j++ )
			TEAM_SetAnnouncedLeadState( j, false );

		TEAM_SetAnnouncedLeadState( i, true );

		// Build the message.
		// Whatever the team's name is, is the first part of the message. For example:
		// if the "blue" team has been defined then the message will be "BlueLeads".
		// This way we don't have to change every announcer to use a new system. 
		FString name;
		name += TEAM_GetName( i );
		name += "Leads";

		// Finally, play the built message.
		ANNOUNCER_PlayEntry( cl_announcer, name.GetChars( ));
	}

	// Potentially play the "3 frags left", etc. announcer sounds.
	if ( fraglimit )
	{
		ANNOUNCER_PotentiallyPlayFragsLeftSound( fraglimit - lNewFragCount );
	}
}

//*****************************************************************************
//
void ANNOUNCER_AllowNumFragsAndPointsLeftSounds( void )
{
	g_bThreeFragsLeftSoundPlayed = false;
	g_bTwoFragsLeftSoundPlayed = false;
	g_bOneFragLeftSoundPlayed = false;

	g_bThreePointsLeftSoundPlayed = false;
	g_bTwoPointsLeftSoundPlayed = false;
	g_bOnePointLeftSoundPlayed = false;
}

//*****************************************************************************
//*****************************************************************************
//
const char *ANNOUNCER_GetName( ULONG ulIdx )
{
	if ( ulIdx >= ANNOUNCER_GetNumProfiles() )
		return ( NULL );

	return ( g_AnnouncerProfile[ulIdx].GetName() );
}

//*****************************************************************************
//*****************************************************************************
//
static void announcer_ParseAnnouncerInfoLump( FScanner &sc )
{
	// Begin parsing that text. COM_Parse will create a token (com_token), and
	// pszBotInfo will skip past the token.
	while ( sc.GetString( ))
	{
		// Initialize our announcer info variable.
		AnnouncerProfile prof;

		while ( sc.String[0] != '{' )
			sc.GetString( );

		// We've encountered a starting bracket. Now continue to parse until we hit an end bracket.
		while ( sc.String[0] != '}' )
		{
			// The current token should be our key. (key = value) If it's an end bracket, break.
			sc.GetString( );
			if ( sc.String[0] == '}' )
				break;
			FString key = sc.String;

			// The following key must be an = sign. If not, the user made an error!
			sc.GetString( );
			if ( stricmp(sc.String, "=") != 0 )
					I_Error( "ANNOUNCER_ParseAnnouncerInfo: Missing \"=\" in ANCRINFO lump for field \"%s\".\n", key.GetChars() );

			// The last token should be our value.
			sc.GetString( );
			FString value = sc.String;

			// If we're specifying the name of the profile, set it here.
			if ( key.CompareNoCase("name") == 0 )
				prof.SetName(value);
			// Add the new key, along with its value to the profile.
			else
			{
				prof.AddEntry(key, value);

				// Populate the default announcer with the first occurace of
				// any sound unless it is explicity defined.
				if ( !g_DefaultAnnouncer->EntryExists(key) )
					g_DefaultAnnouncer->AddEntry(key, value);
			}
		}

		// Add our completed announcer profile.
		if (prof.GetName().CompareNoCase(g_DefaultAnnouncer->GetName()) == 0)
			g_DefaultAnnouncer->Merge(prof);
		else
		{
			g_AnnouncerProfile.Push(prof);
			// [BB] Pushing a new profile may invalidate the pointer to the default announcer.
			g_DefaultAnnouncer = &g_AnnouncerProfile[0];
		}
	}
}

//*****************************************************************************
//	CONSOLE VARIABLES/COMMANDS

CVAR( Int, cl_announcer, 0, CVAR_ARCHIVE )

// [Dark-Assassin] Announcer Pickup ------------------------------------------------
// Toggle to play Announcer Sounds on Pickups
CVAR (Bool, cl_announcepickups, 1, CVAR_ARCHIVE);

// Display all the announce profiles that are loaded.
CCMD( announcers )
{
	ULONG	ulIdx;
	ULONG	ulNumProfiles = 0;

	for ( ulIdx = 0; ulIdx < ANNOUNCER_GetNumProfiles(); ulIdx++ )
	{
		Printf( "%d. %s\n", static_cast<unsigned int> (ulIdx + 1), g_AnnouncerProfile[ulIdx].GetName().GetChars() );
		ulNumProfiles++;
	}

	Printf( "\n%d announcer profile(s) loaded.\n", static_cast<unsigned int> (ulNumProfiles) );
}
