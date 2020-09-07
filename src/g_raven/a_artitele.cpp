#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "s_sound.h"
#include "m_random.h"
#include "doomstat.h"
#include "g_game.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"
#include "teaminfo.h"
#include "gamemode.h"
#include "team.h"
#include "g_game.h"
#include "i_system.h"

static FRandom pr_tele ("TeleportSelf");

// Teleport (self) ----------------------------------------------------------

class AArtiTeleport : public AInventory
{
	DECLARE_CLASS (AArtiTeleport, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiTeleport)

bool AArtiTeleport::Use (bool pickup)
{
	fixed_t destX;
	fixed_t destY;
	angle_t destAngle;

	// [BC] Let the server decide where we go.
	if ( NETWORK_InClientMode() )
	{
		return ( true );
	}

	// [BB] If this is a team game and there are valid team starts for the team
	// the owner is on, teleport to one of the team starts.
	const ULONG ownerTeam = Owner->player ? Owner->player->ulTeam : teams.Size( );
	if ( ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	     && TEAM_CheckIfValid ( ownerTeam )
	     && ( teams[ownerTeam].TeamStarts.Size( ) > 0 ) )
	{
		unsigned int selections = teams[ownerTeam].TeamStarts.Size ();
		unsigned int i = pr_tele() % selections;
		destX = teams[ownerTeam].TeamStarts[i].x;
		destY = teams[ownerTeam].TeamStarts[i].y;
		destAngle = ANG45 * (teams[ownerTeam].TeamStarts[i].angle/45);
	}
	else if (deathmatch)
	{
		unsigned int selections = deathmatchstarts.Size ();
		unsigned int i = pr_tele() % selections;
		destX = deathmatchstarts[i].x;
		destY = deathmatchstarts[i].y;
		destAngle = ANG45 * (deathmatchstarts[i].angle/45);
	}
	else
	{
		FPlayerStart *pSpot = NULL;
		// [BB] If there is a designated start for this player use it.
		if ( playerstarts[Owner->player - players].type != 0 )
			pSpot = &playerstarts[Owner->player - players];
		// [BB] Otherwise we just have to select a start at random from all available player starts.
		else
			pSpot = SelectRandomCooperativeSpot( Owner->player - players );

		if ( pSpot != NULL )
		{
			destX = pSpot->x;
			destY = pSpot->y;
			destAngle = ANG45 * (pSpot->angle/45);
		}
		else
		{
			// [BB] Silence uninitialized warnings.
			destX = destY = destAngle = 0;
			I_Error( "ArtiTeleport: No player start found!" );
		}
	}
	P_Teleport (Owner, destX, destY, ONFLOORZ, destAngle, true, true, false);
	bool canlaugh = true;
 	if (Owner->player->morphTics && (Owner->player->MorphStyle & MORPH_UNDOBYCHAOSDEVICE))
 	{ // Teleporting away will undo any morph effects (pig)
		if (!P_UndoPlayerMorph (Owner->player, Owner->player, MORPH_UNDOBYCHAOSDEVICE)
			&& (Owner->player->MorphStyle & MORPH_FAILNOLAUGH))
		{
			canlaugh = false;
		}
 	}
	if (canlaugh)
 	{ // Full volume laugh
 		S_Sound (Owner, CHAN_VOICE, "*evillaugh", 1, ATTN_NONE, true);	// [BC] Inform the clients.
 	}
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_AutoUseChaosDevice
//
//---------------------------------------------------------------------------

bool P_AutoUseChaosDevice (player_t *player)
{
	AInventory *arti = player->mo->FindInventory(PClass::FindClass("ArtiTeleport"));

	if (arti != NULL)
	{
		player->mo->UseInventory (arti);
		player->health = player->mo->health = (player->health+1)/2;
		return true;
	}
	return false;
}
