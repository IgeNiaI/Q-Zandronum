/*
** d_netinfo.cpp
** Manages transport of user and "server" cvars across a network
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_netinf.h"
#include "d_net.h"
#include "d_protocol.h"
#include "d_player.h"
#include "c_dispatch.h"
#include "v_palette.h"
#include "v_video.h"
#include "i_system.h"
#include "r_state.h"
#include "sbar.h"
#include "gi.h"
#include "m_random.h"
#include "teaminfo.h"
#include "r_data/r_translate.h"
#include "templates.h"
#include "cmdlib.h"
#include "farchive.h"
// [BC] New #includes.
#include "network.h"
#include "cl_commands.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "deathmatch.h"
#include "gamemode.h"
#include "team.h"
#include "menu/menu.h"

static FRandom pr_pickteam ("PickRandomTeam");

extern bool st_firsttime;
EXTERN_CVAR (Bool, teamplay)

CVAR (Float,	autoaim,				5000.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	name,					"Player",	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Color,	color,					0x40cf00,	CVAR_USERINFO | CVAR_ARCHIVE);
// [BB] For now Zandronum doesn't let the player use the color sets.
const int colorset = -1;
//CVAR (Int,		colorset,				0,			CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	skin,					"base",		CVAR_USERINFO | CVAR_ARCHIVE);
// [BC] "team" is no longer a cvar.
//CVAR (Int,		team,					TEAM_NONE,	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	gender,					"male",		CVAR_USERINFO | CVAR_ARCHIVE);
// [BC] Changed "neverswitchonpickup" to allow it to be set 3 different ways, instead of "on/off".
// [TP] switchonpickup, movebob and stillbob are not synced to other clients.
CVAR (Int,		switchonpickup,			1,			CVAR_USERINFO | CVAR_UNSYNCED_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	movebob,				0.25f,		CVAR_USERINFO | CVAR_UNSYNCED_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	stillbob,				0.f,		CVAR_USERINFO | CVAR_UNSYNCED_USERINFO | CVAR_ARCHIVE);
CVAR (String,	playerclass,			"Fighter",	CVAR_USERINFO | CVAR_ARCHIVE);
// [BC] New userinfo entries for Skulltag.
CVAR (Int,		railcolor,				0,			CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		handicap,				0,			CVAR_USERINFO | CVAR_ARCHIVE);
// [Spleen] Let the user enable or disable unlagged shots for themselves. [CK] Now a bitfield.
CVAR (Flag,		cl_unlagged,				cl_clientflags, CLIENTFLAGS_UNLAGGED );
// [BB] Let the user decide whether he wants to respawn when pressing fire. [CK] Now a bitfield.
CVAR (Flag, 	cl_respawnonfire, 			cl_clientflags, CLIENTFLAGS_RESPAWNONFIRE );
// [CK] Unlagged settings where we can choose ping unlagged.
CVAR (Flag,		cl_ping_unlagged,			cl_clientflags, CLIENTFLAGS_PING_UNLAGGED );
// [BB] Let the user control how often the server sends updated player positions to him.
CVAR (Int,		cl_ticsperupdate,			3,		CVAR_USERINFO | CVAR_ARCHIVE);
// [BB] Let the user control specify his connection speed (higher is faster).
CVAR (Int,		cl_connectiontype,			1,		CVAR_USERINFO | CVAR_ARCHIVE);
// [CK] Let the user control if they want clientside puffs or not.
CVAR (Flag,		cl_clientsidepuffs,			cl_clientflags, CLIENTFLAGS_CLIENTSIDEPUFFS );

// [TP] Userinfo changes yet to be sent.
static UserInfoChanges PendingUserinfoChanges;

// [CK] CVARs that affect cl_clientflags
CUSTOM_CVAR ( Int, cl_clientflags, CLIENTFLAGS_DEFAULT, CVAR_USERINFO | CVAR_ARCHIVE )
{
	// Predicted puffs will look really bad if unlagged is off, therefore anyone
	// who turns off unlagged will also turn off clientside puffs.
	// We will invert the clientside puff flag, then AND it to turn off the
	// clientside puff flag since unlagged is off.
	if ( ( ( self & CLIENTFLAGS_UNLAGGED ) == 0 ) && ( self & CLIENTFLAGS_CLIENTSIDEPUFFS ) )
	{
		self = ( self & ( ~CLIENTFLAGS_CLIENTSIDEPUFFS ) );
		Printf( "Clientside puffs have been disabled as unlagged has been turned off.\n" );
	}
}

// ============================================================================
//
// [TP] cl_overrideplayercolors
//
// Lets the user force custom ally/enemy colors.
//
enum OverridePlayerColorsValue
{
	CL_OPC_Never, // never override
	CL_OPC_NoTeams, // only in non-team gamemodes
	CL_OPC_Max2Teams, // not with more than 2 teams
	CL_OPC_Always, // always

	CL_OPC_NumValues
};

// ============================================================================
//
CUSTOM_CVAR( Color, cl_allycolor, 0xFFFFFF, CVAR_ARCHIVE )
{
	D_UpdatePlayerColors();
}

CUSTOM_CVAR( Color, cl_enemycolor, 0x707070, CVAR_ARCHIVE )
{
	D_UpdatePlayerColors();
}

CUSTOM_CVAR( Int, cl_overrideplayercolors, CL_OPC_Never, CVAR_ARCHIVE )
{
	if ( self < 0 )
		self = 0;
	else if ( self > CL_OPC_NumValues - 1 )
		self = CL_OPC_NumValues - 1;
	else
		D_UpdatePlayerColors();
}

// ============================================================================
//
// [TP] Should we be overriding player colors?
//
bool D_ShouldOverridePlayerColors()
{
	// Sure as heck not overriding any colors as the server.
	if ( NETWORK_GetState() == NETSTATE_SERVER )
		return false;

	bool withteams = !!( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS );

	switch ( OverridePlayerColorsValue ( (int) cl_overrideplayercolors ))
	{
		case CL_OPC_NumValues:
		case CL_OPC_Never:
			return false;

		case CL_OPC_NoTeams:
			return withteams == false;

		case CL_OPC_Max2Teams:
			return ( withteams == false ) || ( TEAM_GetNumAvailableTeams() <= 2 );

		case CL_OPC_Always:
			return true;
	}

	return false;
}

// ============================================================================
//
// [TP] Update player colors now
//
void D_UpdatePlayerColors( ULONG ulPlayer )
{
	if ( ulPlayer == MAXPLAYERS )
		R_BuildAllPlayerTranslations();
	else
		R_BuildPlayerTranslation( ulPlayer );

	// [TP] Reattach the status bar to refresh the mugshot.
	if ( StatusBar != NULL )
		StatusBar->AttachToPlayer( StatusBar->CPlayer );
}

// [BB] Two variables to keep track of client side name changes.
static	ULONG	g_ulLastNameChangeTime = 0;
static	FString g_oldPlayerName;

enum
{
	INFO_Name,
	INFO_Autoaim,
	INFO_Color,
	INFO_Skin,
	//INFO_Team,
	INFO_Gender,
	INFO_SwitchOnPickup,
	INFO_Railcolor,
	INFO_Handicap,
	INFO_MoveBob,
	INFO_StillBob,
	INFO_PlayerClass,
	INFO_ColorSet,

	// [BB]
	INFO_Ticsperupdate,
	// [BB]
	INFO_ConnectionType,
	// [CK}
	INFO_ClientFlags
};

const char *GenderNames[3] = { "male", "female", "other" };

// Replace \ with %/ and % with %%
FString D_EscapeUserInfo (const char *str)
{
	FString ret;

	for (; *str != '\0'; ++str)
	{
		if (*str == '\\')
		{
			ret << '%' << '/';
		}
		else if (*str == '%')
		{
			ret << '%' << '%';
		}
		else
		{
			ret << *str;
		}
	}
	return ret;
}

// Replace %/ with \ and %% with %
FString D_UnescapeUserInfo (const char *str, size_t len)
{
	const char *end = str + len;
	FString ret;

	while (*str != '\0' && str < end)
	{
		if (*str == '%')
		{
			if (*(str + 1) == '/')
			{
				ret << '\\';
				str += 2;
				continue;
			}
			else if (*(str + 1) == '%')
			{
				str++;
			}
		}
		ret << *str++;
	}
	return ret;
}

int D_GenderToInt (const char *gender)
{
	if ( !stricmp( gender, "0" ))
		return ( GENDER_MALE );
	if ( !stricmp( gender, "1" ))
		return ( GENDER_FEMALE );
	if ( !stricmp( gender, "2" ))
		return ( GENDER_NEUTER );

	if (!stricmp (gender, "female"))
		return GENDER_FEMALE;
	else if (!stricmp (gender, "other") || !stricmp (gender, "cyborg"))
		return GENDER_NEUTER;
	else
		return GENDER_MALE;
}

int D_PlayerClassToInt (const char *classname)
{
	if (PlayerClasses.Size () > 1)
	{
		for (unsigned int i = 0; i < PlayerClasses.Size (); ++i)
		{
			const PClass *type = PlayerClasses[i].Type;

			if (stricmp (type->Meta.GetMetaString (APMETA_DisplayName), classname) == 0)
			{
				return i;
			}
		}
		return -1;
	}
	else
	{
		return 0;
	}
}

void D_GetPlayerColor (int player, float *h, float *s, float *v, FPlayerColorSet **set)
{
	userinfo_t *info = &players[player].userinfo;
	FPlayerColorSet *colorset = NULL;
	uint32 color;
	// [BB] New team code by Karate Chris. Currently not used in ST.
	//int team;

	if (players[player].mo != NULL)
	{
		colorset = P_GetPlayerColorSet(players[player].mo->GetClass()->TypeName, info->GetColorSet());
	}
	if (colorset != NULL)
	{
		color = GPalette.BaseColors[GPalette.Remap[colorset->RepresentativeColor]];
	}
	else
	{
		color = info->GetColor();
	}

	RGBtoHSV (RPART(color)/255.f, GPART(color)/255.f, BPART(color)/255.f,
		h, s, v);

/* [BB] New team code by Karate Chris. Currently not used in ST.
	if (teamplay && TeamLibrary.IsValidTeam((team = info->GetTeam())) && !Teams[team].GetAllowCustomPlayerColor())
	{
		// In team play, force the player to use the team's hue
		// and adjust the saturation and value so that the team
		// hue is visible in the final color.
		float ts, tv;
		int tcolor = Teams[team].GetPlayerColor ();

		RGBtoHSV (RPART(tcolor)/255.f, GPART(tcolor)/255.f, BPART(tcolor)/255.f,
			h, &ts, &tv);

		*s = clamp(ts + *s * 0.15f - 0.075f, 0.f, 1.f);
		*v = clamp(tv + *v * 0.5f - 0.25f, 0.f, 1.f);

		// Make sure not to pass back any colorset in teamplay.
		colorset = NULL;
	}
*/
	if (set != NULL)
	{
		*set = colorset;
	}

	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		if ( players[player].bOnTeam && ( TEAM_IsCustomPlayerColorAllowed ( players[player].ulTeam ) == false ) )
		{
			int			nColor;

			// Get the color string from the team object.
			nColor = TEAM_GetColor( players[player].ulTeam );

			// Convert.
			RGBtoHSV( RPART( nColor ) / 255.f, GPART( nColor ) / 255.f, BPART( nColor ) / 255.f,
				h, s, v );
		}
	}

	// [Dusk] The user can override these colors.
	int cameraplayer;
	if (( D_ShouldOverridePlayerColors() )
		&& ( players[consoleplayer].camera != NULL )
		&& ( PLAYER_IsValidPlayerWithMo( cameraplayer = players[consoleplayer].camera->player - players ))
		&& ( PLAYER_IsValidPlayerWithMo( player ))
		&& ( players[cameraplayer].bSpectating == false ))
	{
		bool isally = players[cameraplayer].mo->IsTeammate( players[player].mo );
		int color = isally ? cl_allycolor : cl_enemycolor;
		RGBtoHSV( RPART( color ) / 255.f, GPART( color ) / 255.f, BPART( color ) / 255.f, h, s, v );
	}
}

/* [BB] New team code by Karate Chris. Currently not used in ST.
// Find out which teams are present. If there is only one,
// then another team should be chosen at random.
//
// Otherwise, join whichever team has fewest players. If
// teams are tied for fewest players, pick one of those
// at random.

void D_PickRandomTeam (int player)
{
	static char teamline[8] = "\\team\\X";

	BYTE *foo = (BYTE *)teamline;
	teamline[6] = (char)D_PickRandomTeam() + '0';
	D_ReadUserInfoStrings (player, &foo, teamplay);
}

int D_PickRandomTeam ()
{
	for (unsigned int i = 0; i < Teams.Size (); i++)
	{
		Teams[i].m_iPresent = 0;
		Teams[i].m_iTies = 0;
	}

	int numTeams = 0;
	int team;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			team = players[i].userinfo.GetTeam();
			if (TeamLibrary.IsValidTeam(team))
			{
				if (Teams[team].m_iPresent++ == 0)
				{
					numTeams++;
				}
			}
		}
	}

	if (numTeams < 2)
	{
		do
		{
			team = pr_pickteam() % Teams.Size();
		} while (Teams[team].m_iPresent != 0);
	}
	else
	{
		int lowest = INT_MAX, lowestTie = 0;
		unsigned int i;

		for (i = 0; i < Teams.Size (); ++i)
		{
			if (Teams[i].m_iPresent > 0)
			{
				if (Teams[i].m_iPresent < lowest)
				{
					lowest = Teams[i].m_iPresent;
					lowestTie = 0;
					Teams[0].m_iTies = i;
				}
				else if (Teams[i].m_iPresent == lowest)
				{
					Teams[++lowestTie].m_iTies = i;
				}
			}
		}
		if (lowestTie == 0)
		{
			team = Teams[0].m_iTies;
		}
		else
		{
			team = Teams[pr_pickteam() % (lowestTie+1)].m_iTies;
		}
	}

	return team;
}

static void UpdateTeam (int pnum, int team, bool update)
{
	userinfo_t *info = &players[pnum].userinfo;

	if ((dmflags2 & DF2_NO_TEAM_SWITCH) && (alwaysapplydmflags || deathmatch) && TeamLibrary.IsValidTeam (info->GetTeam()))
	{
		Printf ("Team changing has been disabled!\n");
		return;
	}

	int oldteam;

	if (!TeamLibrary.IsValidTeam (team))
	{
		team = TEAM_NONE;
	}
	oldteam = info->GetTeam();
	team = info->TeamChanged(team);

	if (update && oldteam != team)
	{
		if (TeamLibrary.IsValidTeam (team))
			Printf ("%s joined the %s team\n", info->GetName(), Teams[team].GetName ());
		else
			Printf ("%s is now a loner\n", info->GetName());
	}
	// Let the player take on the team's color
	R_BuildPlayerTranslation (pnum);
	if (StatusBar != NULL && StatusBar->GetPlayer() == pnum)
	{
		StatusBar->AttachToPlayer (&players[pnum]);
	}
	// Double-check
	if (!TeamLibrary.IsValidTeam (team))
	{
		*static_cast<FIntCVar *>((*info)[NAME_Team]) = TEAM_NONE;
	}
}
*/
int D_GetFragCount (player_t *player)
{
/* [BB] New team code by Karate Chris. Currently not used in ST.
	const int team = player->userinfo.GetTeam();
	if (!teamplay || !TeamLibrary.IsValidTeam(team))
	{
		return player->fragcount;
	}
	else
	{
		// Count total frags for this player's team
		int count = 0;

		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].userinfo.GetTeam() == team)
			{
				count += players[i].fragcount;
			}
		}
		return count;
	}
*/
	if (( teamplay == false ) || player->bOnTeam == false )
	{
		return player->fragcount;
	}
	else
	{
		return ( TEAM_GetFragCount( player->ulTeam ));
	}
}

void D_SetupUserInfo ()
{
	int i;
	userinfo_t *coninfo;

	// [BC] Servers and client demos don't do this.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) ||
		( CLIENTDEMO_IsPlaying( )))
	{
		return;
	}

	// Reset everybody's userinfo to a default state.
	// [BC] Don't reset everyone's userinfo in multiplayer games, since we don't want to
	// erase EVERYONE'S userinfo if we change ours.
	if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			players[i].userinfo.Reset();
		}
	}
	// [BB] We rebuild coninfo, so better reset it first.
	else
		players[consoleplayer].userinfo.Reset();
	// Initialize the console player's user info
	coninfo = &players[consoleplayer].userinfo;

	for (FBaseCVar *cvar = CVars; cvar != NULL; cvar = cvar->GetNext())
	{
		if ((cvar->GetFlags() & (CVAR_USERINFO|CVAR_IGNORE)) == CVAR_USERINFO)
		{
			FBaseCVar **newcvar;
			FName cvarname(cvar->GetName());

			switch (cvarname.GetIndex())
			{
			// Some cvars don't copy their original value directly.
			// [BB] Zandronum still uses its own team code.
			//case NAME_Team:			coninfo->TeamChanged(team); break;
			case NAME_Skin:			coninfo->SkinChanged(skin, players[consoleplayer].CurrentPlayerClass); break;
			case NAME_Gender:		coninfo->GenderChanged(gender); break;
			case NAME_PlayerClass:	coninfo->PlayerClassChanged(playerclass); break;
			// [BB]
			case NAME_RailColor:			coninfo->RailColorChanged(railcolor); break;
			case NAME_Handicap:				coninfo->HandicapChanged(handicap); break;
			case NAME_CL_TicsPerUpdate:		coninfo->TicsPerUpdateChanged(cl_ticsperupdate); break;
			case NAME_CL_ConnectionType:	coninfo->ConnectionTypeChanged(cl_connectiontype); break;
			case NAME_CL_ClientFlags:		coninfo->ClientFlagsChanged(cl_clientflags); break;

			// The rest do.
			default:
				newcvar = coninfo->CheckKey(cvarname);
				(*newcvar)->SetGenericRep(cvar->GetGenericRep(CVAR_String), CVAR_String);
				break;
			}
		}
	}
	R_BuildPlayerTranslation(consoleplayer);
}

void userinfo_t::Reset()
{
	// Clear this player's userinfo.
	TMapIterator<FName, FBaseCVar *> it(*this);
	TMap<FName, FBaseCVar *>::Pair *pair;

	while (it.NextPair(pair))
	{
		delete pair->Value;
	}
	Clear();

	// Create userinfo vars for this player, initialized to their defaults.
	for (FBaseCVar *cvar = CVars; cvar != NULL; cvar = cvar->GetNext())
	{
		if ((cvar->GetFlags() & (CVAR_USERINFO|CVAR_IGNORE)) == CVAR_USERINFO)
		{
			ECVarType type;
			FName cvarname(cvar->GetName());
			FBaseCVar *newcvar;

			// Some cvars have different types for their shadow copies.
			switch (cvarname.GetIndex())
			{
			case NAME_Skin:			type = CVAR_Int; break;
			case NAME_Gender:		type = CVAR_Int; break;
			case NAME_PlayerClass:	type = CVAR_Int; break;
			default:				type = cvar->GetRealType(); break;
			}
			// [TP] Also respect CVAR_UNSYNCED_USERINFO
			newcvar = C_CreateCVar(NULL, type, cvar->GetFlags() & ( CVAR_MOD | CVAR_UNSYNCED_USERINFO ));
			newcvar->SetGenericRepDefault(cvar->GetGenericRepDefault(CVAR_String), CVAR_String);
			Insert(cvarname, newcvar);
		}
	}
}

/* [BB] Zandronum still uses its own team code.
int userinfo_t::TeamChanged(int team)
{
	if (teamplay && !TeamLibrary.IsValidTeam(team))
	{ // Force players onto teams in teamplay mode
		team = D_PickRandomTeam();
	}
	*static_cast<FIntCVar *>((*this)[NAME_Team]) = team;
	return team;
}
*/

int userinfo_t::SkinChanged(const char *skinname, int playerclass)
{
	int skinnum = R_FindSkin(skinname, playerclass);
	*static_cast<FIntCVar *>((*this)[NAME_Skin]) = skinnum;
	return skinnum;
}

int userinfo_t::SkinNumChanged(int skinnum)
{
	*static_cast<FIntCVar *>((*this)[NAME_Skin]) = skinnum;
	return skinnum;
}

int userinfo_t::GenderChanged(const char *gendername)
{
	int gendernum = D_GenderToInt(gendername);
	*static_cast<FIntCVar *>((*this)[NAME_Gender]) = gendernum;
	return gendernum;
}

int userinfo_t::PlayerClassChanged(const char *classname)
{
	int classnum = D_PlayerClassToInt(classname);
	*static_cast<FIntCVar *>((*this)[NAME_PlayerClass]) = classnum;
	return classnum;
}

int userinfo_t::PlayerClassNumChanged(int classnum)
{
	*static_cast<FIntCVar *>((*this)[NAME_PlayerClass]) = classnum;
	return classnum;
}

int userinfo_t::ColorSetChanged(int setnum)
{
	*static_cast<FIntCVar *>((*this)[NAME_ColorSet]) = setnum;
	return setnum;
}

uint32 userinfo_t::ColorChanged(const char *colorname)
{
	FColorCVar *color = static_cast<FColorCVar *>((*this)[NAME_Color]);
	assert(color != NULL);
	UCVarValue val;
	val.String = const_cast<char *>(colorname);
	color->SetGenericRep(val, CVAR_String);
	// [BB] For now Zandronum doesn't let the player use the color sets.
	//*static_cast<FIntCVar *>((*this)[NAME_ColorSet]) = -1;
	return *color;
}

uint32 userinfo_t::ColorChanged(uint32 colorval)
{
	FColorCVar *color = static_cast<FColorCVar *>((*this)[NAME_Color]);
	assert(color != NULL);
	UCVarValue val;
	val.Int = colorval;
	color->SetGenericRep(val, CVAR_Int);
	// This version is called by the menu code. Do not implicitly set colorset.
	return colorval;
}

// [BB]
void userinfo_t::NameChanged(const char *name)
{
	FString cleanedName = name;
	// [BB] Don't allow the CVAR to be longer than MAXPLAYERNAME, userinfo_t::netname
	// can't be longer anyway. Note: The length limit needs to be applied in colorized form.
	cleanedName = cleanedName.Left(MAXPLAYERNAME);
	V_CleanPlayerName ( cleanedName );
	*static_cast<FStringCVar *>((*this)[NAME_Name]) = cleanedName;
}

// [BB]
int userinfo_t::SwitchOnPickupChanged(int switchonpickup)
{
	switchonpickup = clamp ( switchonpickup, 0, 3 );
	*static_cast<FIntCVar *>((*this)[NAME_SwitchOnPickup]) = switchonpickup;
	return switchonpickup;
}

// [BB]
int userinfo_t::GenderNumChanged(int gendernum)
{
	// [BB] Make sure that the gender is valid.
	gendernum = clamp ( gendernum, 0, 2 );
	*static_cast<FIntCVar *>((*this)[NAME_Gender]) = gendernum;
	return gendernum;
}

// [BB]
int userinfo_t::RailColorChanged(int railcolor)
{
	if ( (*this)[NAME_RailColor] == NULL )
	{
		Printf ( "Error: No RailColor key found!\n" );
		return 0;
	}
	*static_cast<FIntCVar *>((*this)[NAME_RailColor]) = railcolor;
	return railcolor;
}

// [BB]
int userinfo_t::HandicapChanged(int handicap)
{
	if ( (*this)[NAME_Handicap] == NULL )
	{
		Printf ( "Error: No Handicap key found!\n" );
		return 0;
	}
	handicap = clamp ( handicap, 0, deh.MaxSoulsphere );
	*static_cast<FIntCVar *>((*this)[NAME_Handicap]) = handicap;
	return handicap;
}

// [BB]
int userinfo_t::TicsPerUpdateChanged(int ticsperupdate)
{
	if ( (*this)[NAME_CL_TicsPerUpdate] == NULL )
	{
		Printf ( "Error: No TicsPerUpdate key found!\n" );
		return 0;
	}
	ticsperupdate = clamp ( ticsperupdate, 1, 3 );
	*static_cast<FIntCVar *>((*this)[NAME_CL_TicsPerUpdate]) = ticsperupdate;
	return ticsperupdate;
}

// [BB]
int userinfo_t::ConnectionTypeChanged(int connectiontype)
{
	if ( (*this)[NAME_CL_ConnectionType] == NULL )
	{
		Printf ( "Error: No ConnectionType key found!\n" );
		return 0;
	}
	connectiontype = clamp ( connectiontype, 0, 1 );
	*static_cast<FIntCVar *>((*this)[NAME_CL_ConnectionType]) = connectiontype;
	return connectiontype;
}

// [BB]
int userinfo_t::ClientFlagsChanged(int flags)
{
	if ( (*this)[NAME_CL_ClientFlags] == NULL )
	{
		Printf ( "Error: No ClientFlags key found!\n" );
		return 0;
	}
	*static_cast<FIntCVar *>((*this)[NAME_CL_ClientFlags]) = flags;
	return flags;
}

void D_UserInfoChanged (FBaseCVar *cvar)
{
	UCVarValue val;
	FString escaped_val;
	char foo[256];

	// Server doesn't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	if (cvar == &autoaim)
	{
		if (autoaim < 0.0f)
		{
			autoaim = 0.0f;
			return;
		}
		else if (autoaim > 5000.0f)
		{
			autoaim = 5000.f;
			return;
		}
	}
	// Allow users to colorize their name.
	else if ( cvar == &name )
	{
		val = cvar->GetGenericRep( CVAR_String );
		FString cleanedName = val.String;
		// [BB] V_CleanPlayerName removes all backslashes, including those from '\c'.
		// To clean the name, we first convert the color codes, clean the name and
		// then restore the color codes again.
		V_ColorizeString ( cleanedName );
		// [BB] Don't allow the CVAR to be longer than MAXPLAYERNAME, userinfo_t::netname
		// can't be longer anyway. Note: The length limit needs to be applied in colorized form.
		cleanedName = cleanedName.Left(MAXPLAYERNAME);
		V_CleanPlayerName ( cleanedName );
		V_UnColorizeString ( cleanedName );
		// [BB] The name needed to be cleaned. Update the CVAR name with the cleaned
		// string and return (updating the name calls D_UserInfoChanged again).
		if ( strcmp ( cleanedName.GetChars(), val.String ) != 0 )
		{
			name = cleanedName;
			return;
		}
		// [BB] Get rid of this cast.
		V_ColorizeString( const_cast<char *> ( val.String ) );

		// [BB] We don't want clients to change their name too often.
		if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		{
			// [BB] The name was not actually changed, so no need to do anything.
			if ( strcmp ( g_oldPlayerName.GetChars(), val.String ) == 0 )
			{
				// [BB] The client insists on changing to a name already in use.
				if ( stricmp ( val.String, players[consoleplayer].userinfo.GetName() ) != 0 )
				{
					Printf ( "The server already reported that this name is in use!\n" );
					g_oldPlayerName = players[consoleplayer].userinfo.GetName();
					return;
				}
			}
			// [BB] The client recently changed its name, don't allow to change it again yet.
			// [TP] Made conditional with sv_limitcommands
			else if (( sv_limitcommands ) && ( g_ulLastNameChangeTime > 0 ) && ( (ULONG)gametic < ( g_ulLastNameChangeTime + ( TICRATE * 30 ))))
			{
				Printf( "You must wait at least 30 seconds before changing your name again.\n" );
				name = g_oldPlayerName;
				return;
			}
			// [BB] The client made a valid name change, keep track of this.
			else
			{
				g_ulLastNameChangeTime = gametic;
				g_oldPlayerName = val.String;
			}
		}
	}
	else if ( cvar == &handicap )
	{
		if ( handicap < 0 )
		{
			handicap = 0;
			return;
		}
		if ( handicap > 200 )
		{
			handicap = 200;
			return;
		}
	}
	// [BB] Negative movebob values cause graphic glitches.
	else if ( cvar == &movebob )
	{
		if ( movebob < 0 )
		{
			movebob = 0;
			return;
		}
	}
	// [WS] We need to handle the values for stillbob to prevent cheating.
	else if ( cvar == &stillbob )
	{
		if ( stillbob < -16 )
		{
			stillbob = -16;
			return;
		}
		if ( stillbob > 16 )
		{
			stillbob = 16;
			return;
		}
	}
	// [BB]
	else if ( cvar == &cl_ticsperupdate )
	{
		if ( cl_ticsperupdate < 1 )
		{
			cl_ticsperupdate = 1;
			return;
		}
		if ( cl_ticsperupdate > 3 )
		{
			cl_ticsperupdate = 3;
			return;
		}
	}
	// [BB]
	else if ( cvar == &cl_connectiontype )
	{
		if ( cl_connectiontype < 0 )
		{
			cl_connectiontype = 0;
			return;
		}
		if ( cl_connectiontype > 1 )
		{
			cl_connectiontype = 1;
			return;
		}
	}

	val = cvar->GetGenericRep (CVAR_String);
	escaped_val = D_EscapeUserInfo(val.String);
	if (4 + strlen(cvar->GetName()) + escaped_val.Len() > 256)
		I_Error ("User info descriptor too big");

	mysnprintf (foo, countof(foo), "\\%s\\%s", cvar->GetName(), escaped_val.GetChars());

	// [BC] In client mode, we don't execute DEM_* commands, so we need to execute it
	// here.
	if ( NETWORK_InClientMode() )
	{
		BYTE	*pStream;

		// This is a lot of work just to convert foo from (char *) to a (BYTE **) :(
		pStream = (BYTE *)M_Malloc( strlen( foo ) + 1 );
		WriteString( foo, &pStream );
		pStream -= ( strlen( foo ) + 1 );
		D_ReadUserInfoStrings( consoleplayer, &pStream, false );
		pStream -= ( strlen( foo ) + 1 );
		M_Free( pStream );
	}
	// Is the demo stuff necessary at all for ST?
	else
	{
		Net_WriteByte (DEM_UINFCHANGED);
		Net_WriteString (foo);
	}

	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		PendingUserinfoChanges.insert( FName ( cvar->GetName() ) );

	// [TP] Send pending changes but only if we're not in a menu
	if ( DMenu::CurrentMenu == NULL )
		D_SendPendingUserinfoChanges();
}

void D_SendPendingUserinfoChanges()
{
	// Send updated userinfo to the server.
	if (( NETWORK_GetState() == NETSTATE_CLIENT )
		&& ( CLIENT_GetConnectionState() >= CTS_REQUESTINGSNAPSHOT )
		&& ( PendingUserinfoChanges.size() > 0 ))
	{
		CLIENTCOMMANDS_UserInfo( PendingUserinfoChanges );

		if ( CLIENTDEMO_IsRecording( ))
			CLIENTDEMO_WriteUserInfo( );

		PendingUserinfoChanges.clear();
	}
}

static const char *SetServerVar (char *name, ECVarType type, BYTE **stream, bool singlebit)
{
	FBaseCVar *var = FindCVar (name, NULL);
	UCVarValue value;

	if (singlebit)
	{
		if (var != NULL)
		{
			int bitdata;
			int mask;

			value = var->GetFavoriteRep (&type);
			if (type != CVAR_Int)
			{
				return NULL;
			}
			bitdata = ReadByte (stream);
			mask = 1 << (bitdata & 31);
			if (bitdata & 32)
			{
				value.Int |= mask;
			}
			else
			{
				value.Int &= ~mask;
			}
		}
	}
	else
	{
		switch (type)
		{
		case CVAR_Bool:		value.Bool = ReadByte (stream) ? 1 : 0;	break;
		case CVAR_Int:		value.Int = ReadLong (stream);			break;
		case CVAR_Float:	value.Float = ReadFloat (stream);		break;
		case CVAR_String:	value.String = ReadString (stream);		break;
		default: break;	// Silence GCC
		}
	}

	if (var)
	{
		var->ForceSet (value, type);
	}

	if (type == CVAR_String)
	{
		delete[] value.String;
	}
/* [BB] New team code by Karate Chris. Currently not used in ST.
	if (var == &teamplay)
	{
		// Put players on teams if teamplay turned on
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				UpdateTeam (i, players[i].userinfo.GetTeam(), true);
			}
		}
	}
*/
	if (var)
	{
		value = var->GetGenericRep (CVAR_String);
		return value.String;
	}

	return NULL;
}

EXTERN_CVAR (Float, sv_gravity)

void D_SendServerInfoChange (const FBaseCVar *cvar, UCVarValue value, ECVarType type)
{
	size_t namelen;

	// Server doesn't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	namelen = strlen (cvar->GetName ());

	Net_WriteByte (DEM_SINFCHANGED);
	Net_WriteByte ((BYTE)(namelen | (type << 6)));
	Net_WriteBytes ((BYTE *)cvar->GetName (), (int)namelen);
	switch (type)
	{
	case CVAR_Bool:		Net_WriteByte (value.Bool);		break;
	case CVAR_Int:		Net_WriteLong (value.Int);		break;
	case CVAR_Float:	Net_WriteFloat (value.Float);	break;
	case CVAR_String:	Net_WriteString (value.String);	break;
	default: break; // Silence GCC
	}
}

void D_SendServerFlagChange (const FBaseCVar *cvar, int bitnum, bool set)
{
	int namelen;

	namelen = (int)strlen (cvar->GetName ());

	Net_WriteByte (DEM_SINFCHANGEDXOR);
	Net_WriteByte ((BYTE)namelen);
	Net_WriteBytes ((BYTE *)cvar->GetName (), namelen);
	Net_WriteByte (BYTE(bitnum | (set << 5)));
}

void D_DoServerInfoChange (BYTE **stream, bool singlebit)
{
	const char *value;
	char name[64];
	int len;
	int type;

	// Server doesn't do this.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	len = ReadByte (stream);
	type = len >> 6;
	len &= 0x3f;
	if (len == 0)
		return;
	memcpy (name, *stream, len);
	*stream += len;
	name[len] = 0;

	if ( (value = SetServerVar (name, (ECVarType)type, stream, singlebit)) && ( NETWORK_GetState( ) != NETSTATE_SINGLE ))
	{
		Printf ("%s changed to %s\n", name, value);
	}
}

static int STACK_ARGS userinfosortfunc(const void *a, const void *b)
{
	TMap<FName, FBaseCVar *>::ConstPair *pair1 = *(TMap<FName, FBaseCVar *>::ConstPair **)a;
	TMap<FName, FBaseCVar *>::ConstPair *pair2 = *(TMap<FName, FBaseCVar *>::ConstPair **)b;
	return stricmp(pair1->Key.GetChars(), pair2->Key.GetChars());
}

static int STACK_ARGS namesortfunc(const void *a, const void *b)
{
	FName *name1 = (FName *)a;
	FName *name2 = (FName *)b;
	return stricmp(name1->GetChars(), name2->GetChars());
}

void D_WriteUserInfoStrings (int pnum, BYTE **stream, bool compact)
{
	if (pnum >= MAXPLAYERS)
	{
		WriteByte (0, stream);
		return;
	}

	userinfo_t *info = &players[pnum].userinfo;
	TArray<TMap<FName, FBaseCVar *>::Pair *> userinfo_pairs(info->CountUsed());
	TMap<FName, FBaseCVar *>::Iterator it(*info);
	TMap<FName, FBaseCVar *>::Pair *pair;
	UCVarValue cval;

	// Create a simple array of all userinfo cvars
	while (it.NextPair(pair))
	{
		userinfo_pairs.Push(pair);
	}
	// For compact mode, these need to be sorted. Verbose mode doesn't matter.
	if (compact)
	{
		qsort(&userinfo_pairs[0], userinfo_pairs.Size(), sizeof(pair), userinfosortfunc);
		// Compact mode is signified by starting the string with two backslash characters.
		// We output one now. The second will be output as part of the first value.
		*(*stream)++ = '\\';
	}
	for (unsigned int i = 0; i < userinfo_pairs.Size(); ++i)
	{
		pair = userinfo_pairs[i];

		if (!compact)
		{ // In verbose mode, prepend the cvar's name
			*stream += sprintf(*((char **)stream), "\\%s", pair->Key.GetChars());
		}
		// A few of these need special handling for compatibility reasons.
		switch (pair->Key.GetIndex())
		{
		case NAME_Gender:
			*stream += sprintf(*((char **)stream), "\\%s",
				*static_cast<FIntCVar *>(pair->Value) == GENDER_FEMALE ? "female" :
				*static_cast<FIntCVar *>(pair->Value) == GENDER_NEUTER ? "other" : "male");
			break;

		case NAME_PlayerClass:
			*stream += sprintf(*((char **)stream), "\\%s", info->GetPlayerClassNum() == -1 ? "Random" :
				D_EscapeUserInfo(info->GetPlayerClassType()->Meta.GetMetaString(APMETA_DisplayName)).GetChars());
			break;

		case NAME_Skin:
			*stream += sprintf(*((char **)stream), "\\%s", D_EscapeUserInfo(skins[info->GetSkin()].name).GetChars());
			break;

		default:
			cval = pair->Value->GetGenericRep(CVAR_String);
			*stream += sprintf(*((char **)stream), "\\%s", cval.String);
			break;
		}
	}
	*(*stream)++ = '\0';
}

void D_ReadUserInfoStrings (int pnum, BYTE **stream, bool update)
{
	userinfo_t *info = &players[pnum].userinfo;
	TArray<FName> compact_names(info->CountUsed());
	FBaseCVar **cvar_ptr;
	const char *ptr = *((const char **)stream);
	const char *breakpt;
	FString value;
	bool compact;
	FName keyname;
	unsigned int infotype = 0;

	if (*ptr++ != '\\')
		return;

	compact = (*ptr == '\\') ? ptr++, true : false;

	// We need the cvar names in sorted order for compact mode
	if (compact)
	{
		TMap<FName, FBaseCVar *>::Iterator it(*info);
		TMap<FName, FBaseCVar *>::Pair *pair;

		while (it.NextPair(pair))
		{
			compact_names.Push(pair->Key);
		}
		qsort(&compact_names[0], compact_names.Size(), sizeof(FName), namesortfunc);
	}

	if (pnum < MAXPLAYERS)
	{
		for (breakpt = ptr; breakpt != NULL; ptr = breakpt + 1)
		{
			breakpt = strchr(ptr, '\\');

			if (compact)
			{
				// Compact has just the value.
				if (infotype >= compact_names.Size())
				{ // Too many entries! OMG!
					break;
				}
				keyname = compact_names[infotype++];
				value = D_UnescapeUserInfo(ptr, breakpt != NULL ? breakpt - ptr : strlen(ptr));
			}
			else
			{
				// Verbose has both the key name and its value.
				assert(breakpt != NULL);
				// A malicious remote machine could invalidate the above assert.
				if (breakpt == NULL)
				{
					break;
				}
				const char *valstart = breakpt + 1;
				if ( (breakpt = strchr (valstart, '\\')) != NULL )
				{
					value = D_UnescapeUserInfo(valstart, breakpt - valstart);
				}
				else
				{
					value = D_UnescapeUserInfo(valstart, strlen(valstart));
				}
				keyname = FName(ptr, valstart - ptr - 1, true);
			}
			
			// A few of these need special handling.
			switch (keyname)
			{
			case NAME_Gender:
				info->GenderChanged(value);
				break;

			case NAME_PlayerClass:
				info->PlayerClassChanged(value);
				break;

			case NAME_Skin:
				info->SkinChanged(value, players[pnum].CurrentPlayerClass);

				// [BC] If the skin was hidden, reveal it!
				if ( skins[info->GetSkin()].bRevealed == false )
				{
					Printf( "Hidden skin \"%s\\c-\" has now been revealed!\n", skins[info->GetSkin()].name );
					skins[info->GetSkin()].bRevealed = true;
				}

				if (players[pnum].mo != NULL)
				{
					if (players[pnum].cls != NULL &&
						!(players[pnum].mo->flags4 & MF4_NOSKIN) &&
						players[pnum].mo->state->sprite ==
						GetDefaultByType (players[pnum].cls)->SpawnState->sprite)
					{ // Only change the sprite if the player is using a standard one
						players[pnum].mo->sprite = skins[info->GetSkin()].sprite;
					}
				}
				// Rebuild translation in case the new skin uses a different range
				// than the old one.
				R_BuildPlayerTranslation(pnum);
				break;

			/* [BB] Zandronum still uses its own team code.
			case NAME_Team:
				UpdateTeam(pnum, atoi(value), update);
				break;
			*/

			case NAME_Color:
				info->ColorChanged(value);
				break;

			// [BB]
			case NAME_SwitchOnPickup:
				if (value[0] >= '0' && value[0] <= '9')
				{
					info->SwitchOnPickupChanged ( atoi (value) );
				}
				else if (stricmp (value, "true") == 0)
				{
					info->SwitchOnPickupChanged ( 2 );
				}
				else
				{
					info->SwitchOnPickupChanged ( 0 );
				}
				break;

			// [BB]
			case NAME_RailColor:
				info->RailColorChanged ( atoi( value ) );
				break;

			// [BB]
			case NAME_Handicap:
				info->HandicapChanged ( atoi( value ) );
				break;

			// [BB]
			case NAME_CL_TicsPerUpdate:
				info->TicsPerUpdateChanged ( atoi (value) );
				break;

			// [BB]
			case NAME_CL_ConnectionType:
				info->ConnectionTypeChanged ( atoi (value) );
				break;

			// [CK]
			case NAME_CL_ClientFlags:
				info->ClientFlagsChanged ( atoi( value ) );
				break;

			default:
				cvar_ptr = info->CheckKey(keyname);
				if (cvar_ptr != NULL)
				{
					assert(*cvar_ptr != NULL);
					UCVarValue val;
					FString oldname;

					if (keyname == NAME_Name)
					{
						val = (*cvar_ptr)->GetGenericRep(CVAR_String);
						oldname = val.String;
					}
					val.String = CleanseString(value.LockBuffer());
					(*cvar_ptr)->SetGenericRep(val, CVAR_String);
					value.UnlockBuffer();
					if (keyname == NAME_Name && update && oldname.Compare (value))
					{
						// [BB] Added "\\c-"
						Printf("%s \\c-is now known as %s\n", oldname.GetChars(), value.GetChars());
					}
				}
				break;
			}
			if (keyname == NAME_Color || keyname == NAME_ColorSet)
			{
				R_BuildPlayerTranslation(pnum);
				if (StatusBar != NULL && pnum == StatusBar->GetPlayer())
				{
					StatusBar->AttachToPlayer(&players[pnum]);
				}
			}
		}
	}
	*stream += strlen (*((char **)stream)) + 1;
}

void ReadCompatibleUserInfo(FArchive &arc, userinfo_t &info)
{
	char netname[MAXPLAYERNAME + 1];
	BYTE team;
	int aimdist, color, colorset, skin, gender;
	bool neverswitch;
	//fixed_t movebob, stillbob;	These were never serialized!
	//int playerclass;				"

	info.Reset();

	arc.Read(&netname, sizeof(netname));
	arc << team << aimdist << color << skin << gender << neverswitch << colorset;

	*static_cast<FStringCVar *>(info[NAME_Name]) = netname;
	*static_cast<FIntCVar *>(info[NAME_Team]) = team;
	*static_cast<FFloatCVar *>(info[NAME_Autoaim]) = (float)aimdist / ANGLE_1;
	*static_cast<FIntCVar *>(info[NAME_Skin]) = skin;
	*static_cast<FIntCVar *>(info[NAME_Gender]) = gender;
	// [BB]
	*static_cast<FIntCVar *>(info[NAME_SwitchOnPickup]) = switchonpickup;
	// [BB] For now Zandronum doesn't let the player use the color sets.
	//*static_cast<FIntCVar *>(info[NAME_ColorSet]) = colorset;

	UCVarValue val;
	val.Int = color;
	static_cast<FColorCVar *>(info[NAME_Color])->SetGenericRep(val, CVAR_Int);
}

void WriteUserInfo(FArchive &arc, userinfo_t &info)
{
	TMapIterator<FName, FBaseCVar *> it(info);
	TMap<FName, FBaseCVar *>::Pair *pair;
	FName name;
	UCVarValue val;
	int i;

	while (it.NextPair(pair))
	{
		name = pair->Key;
		arc << name;
		switch (name.GetIndex())
		{
		case NAME_Skin:
			arc.WriteString(skins[info.GetSkin()].name);
			break;

		case NAME_PlayerClass:
			i = info.GetPlayerClassNum();
			arc.WriteString(i == -1 ? "Random" : PlayerClasses[i].Type->Meta.GetMetaString(APMETA_DisplayName));
			break;

		default:
			val = pair->Value->GetGenericRep(CVAR_String);
			arc.WriteString(val.String);
			break;
		}
	}
	name = NAME_None;
	arc << name;
}

void ReadUserInfo(FArchive &arc, userinfo_t &info, FString &skin)
{
	FName name;
	FBaseCVar **cvar;
	char *str = NULL;
	UCVarValue val;

	if (SaveVersion < 4253)
	{
		ReadCompatibleUserInfo(arc, info);
		return;
	}

	info.Reset();
	skin = NULL;
	for (arc << name; name != NAME_None; arc << name)
	{
		cvar = info.CheckKey(name);
		arc << str;
		if (cvar != NULL && *cvar != NULL)
		{
			switch (name)
			{
			// [BB] Zandronum still uses its own team code.
			//case NAME_Team:			info.TeamChanged(atoi(str)); break;
			case NAME_Skin:			skin = str; break;	// Caller must call SkinChanged() once current calss is known
			case NAME_PlayerClass:	info.PlayerClassChanged(str); break;
			default:
				val.String = str;
				(*cvar)->SetGenericRep(val, CVAR_String);
				break;
			}
		}
	}
	if (str != NULL)
	{
		delete[] str;
	}
}

CCMD (playerinfo)
{
	if (argv.argc() < 2)
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				// [BB] Only call Printf once to prevent problems with sv_logfiletimestamp.
				FString infoString;
				infoString.AppendFormat("\\c%c%d. %s", PLAYER_IsTrueSpectator( &players[i] ) ? 'k' : 'j', i, players[i].userinfo.GetName());

				// [RC] Are we the server? Draw their IPs as well.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					infoString.AppendFormat("\\c%c - IP %s", PLAYER_IsTrueSpectator( &players[i] ) ? 'k' : 'j', SERVER_GetClient( i )->Address.ToString() );
					// [BB] If we detected suspicious behavior of this client, print this now.
					if ( SERVER_GetClient( i )->bSuspicious )
						infoString.AppendFormat ( " * %lu", SERVER_GetClient( i )->ulNumConsistencyWarnings );

					// [K6/BB] Show the player's country, if the GeoIP db is available.
					if ( NETWORK_IsGeoIPAvailable() )
						infoString.AppendFormat ( "\\ce - FROM %s", NETWORK_GetCountryCodeFromAddress ( SERVER_GetClient( i )->Address ).GetChars() );
				}

				if ( PLAYER_IsTrueSpectator( &players[i] ))
					infoString.AppendFormat("\\ck (SPEC)");

				Printf("%s\n", infoString.GetChars());
			}
		}
	}
	else
	{
		int i = atoi(argv[1]);

		if (i < 0 || i >= MAXPLAYERS)
		{
			Printf("Bad player number\n");
			return;
		}
		userinfo_t *ui = &players[i].userinfo;

		if (!playeringame[i])
		{
			Printf(TEXTCOLOR_ORANGE "Player %d is not in the game\n", i);
			return;
		}

		// Print special info
		Printf("%20s: %s\n",      "Name", ui->GetName());
		// [BB] Zandronum still uses its own team code.
		Printf("%20s: %s (%d)\n", "Team", players[i].bOnTeam ? TEAM_GetName( players[i].ulTeam ) : "NONE", static_cast<unsigned int> (players[i].ulTeam) );
		Printf("%20s: %s (%d)\n", "Skin", skins[ui->GetSkin()].name, ui->GetSkin());
		Printf("%20s: %s (%d)\n", "Gender", GenderNames[ui->GetGender()], ui->GetGender());
		Printf("%20s: %s (%d)\n", "PlayerClass",
			ui->GetPlayerClassNum() == -1 ? "Random" : ui->GetPlayerClassType()->Meta.GetMetaString (APMETA_DisplayName),
			ui->GetPlayerClassNum());

		// [TP] Show account name
		if ( NETWORK_GetState() == NETSTATE_SERVER )
		{
			Printf( "%20s: %s\n", "Account", SERVER_GetClient( i )->GetAccountName().GetChars() );
		}
		else if ( NETWORK_InClientMode() )
		{
			Printf( "%20s: %s\n", "Account", CLIENT_GetPlayerAccountName( i ).GetChars() );
		}

		// Print generic info
		TMapIterator<FName, FBaseCVar *> it(*ui);
		TMap<FName, FBaseCVar *>::Pair *pair;

		while (it.NextPair(pair))
		{
			// [TP] Some userinfo settings are unsynced.
			if ((( NETWORK_GetState() == NETSTATE_SERVER ) || ( i != consoleplayer ))
				&& ( pair->Value->GetFlags() & CVAR_UNSYNCED_USERINFO ))
			{
				Printf( "%20s: <unknown>\n", pair->Key.GetChars() );
				continue;
			}

			if (pair->Key != NAME_Name && pair->Key != NAME_Team && pair->Key != NAME_Skin &&
				pair->Key != NAME_Gender && pair->Key != NAME_PlayerClass)
			{
				UCVarValue val = pair->Value->GetGenericRep(CVAR_String);
				Printf("%20s: %s\n", pair->Key.GetChars(), val.String);
			}
		}
		if (argv.argc() > 2)
		{
			// [BB] The extended info is only available if cheats are allowed.
			if ( CheckCheatmode() == false )
				PrintMiscActorInfo(players[i].mo);
		}
	}
}

userinfo_t::~userinfo_t()
{
	TMapIterator<FName, FBaseCVar *> it(*this);
	TMap<FName, FBaseCVar *>::Pair *pair;

	while (it.NextPair(pair))
	{
		delete pair->Value;
	}
	this->Clear();
}

#ifdef _DEBUG
// [BC] Debugging function.
CCMD( listinventory )
{
	AInventory	*pInventory;

	if ( players[consoleplayer].mo == NULL )
		return;

	pInventory = players[consoleplayer].mo->Inventory;
	while ( pInventory )
	{
		Printf( "%s\n", pInventory->GetClass( )->TypeName.GetChars( ));
		pInventory = pInventory->Inventory;
	}
}
#endif
