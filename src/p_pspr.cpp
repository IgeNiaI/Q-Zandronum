
//**************************************************************************
//**
//** p_pspr.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: p_pspr.c,v $
//** $Revision: 1.105 $
//** $Date: 96/01/06 03:23:35 $
//** $Author: bgokey $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "doomdef.h"
#include "d_event.h"
#include "c_cvars.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "gi.h"
#include "p_pspr.h"
#include "templates.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "farchive.h"
#include "d_player.h"
// [BB] New #includes.
#include "deathmatch.h"
#include "network.h"
#include "cl_demo.h"
#include "p_effect.h"
#include "sv_commands.h"
#include "unlagged.h"


// MACROS ------------------------------------------------------------------

#define LOWERSPEED				FRACUNIT*6
#define RAISESPEED				FRACUNIT*6

// [CK] The minimum binary angle for autoaim to trigger against other players.
// This was determined by making a triangle from the max autoaim range (1024) by
// player radius (16) to get an angle of ~0.89 degrees. This below value is the
// binary value of this angle.
#define AUTOAIM_MINANGLE		0xA20500

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// [SO] 1=Weapons states are all 1 tick
//		2=states with a function 1 tick, others 0 ticks.
CUSTOM_CVAR( Int, sv_fastweapons, 0, CVAR_SERVERINFO )
{
	if ( self >= 3 )
		self = 2;
	if ( self < 0 )
		self = 0;

	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %d\n", self.GetName( ), (int)self );
		SERVERCOMMANDS_SetGameModeLimits( );
	}
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FRandom pr_wpnreadysnd ("WpnReadySnd");
static FRandom pr_gunshot ("GunShot");

// CODE --------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// PROC P_NewPspriteTick
//
//---------------------------------------------------------------------------

void P_NewPspriteTick()
{
	// This function should be called after the beginning of a tick, before any possible
	// prprite-event, or near the end, after any possible psprite event.
	// Because data is reset for every tick (which it must be) this has no impact on savegames.
	for (int i = 0; i<MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			pspdef_t *pspdef = players[i].psprites;
			for (int j = 0;j < NUMPSPRITES; j++)
			{
				pspdef[j].processPending = true;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC P_SetPsprite
//
//---------------------------------------------------------------------------

void P_SetPsprite (player_t *player, int position, FState *state, bool nofunction)
{
	pspdef_t *psp;

	if (position == ps_weapon && !nofunction)
	{ // A_WeaponReady will re-set these as needed
		player->WeaponState &= ~(WF_WEAPONREADY | WF_WEAPONREADYALT | WF_WEAPONBOBBING | WF_WEAPONSWITCHOK | WF_WEAPONRELOADOK | WF_WEAPONZOOMOK);
	}

	psp = &player->psprites[position];
	// [BB] This ZDoom fix causes client/server weapon sync problems.
	// Till those are fixed, this stays disabled.
	//psp->processPending = false; // Do not subsequently perform periodic processing within the same tick.

	do
	{
		if (state == NULL)
		{ // Object removed itself.
			psp->state = NULL;
			break;
		}
		psp->state = state;

		if (state->sprite != SPR_FIXED)
		{ // okay to change sprite and/or frame
			if (!state->GetSameFrame())
			{ // okay to change frame
				psp->frame = state->GetFrame();
			}
			if (state->sprite != SPR_NOCHANGE)
			{ // okay to change sprite
				psp->sprite = state->sprite;
			}
		}


		if (sv_fastweapons >= 2 && position == ps_weapon)
			psp->tics = state->ActionFunc == NULL? 0 : 1;
		else if (sv_fastweapons)
			psp->tics = 1;		// great for producing decals :)
		else
			psp->tics = state->GetTics(); // could be 0

		if (state->GetMisc1())
		{ // Set coordinates.
			psp->sx = state->GetMisc1()<<FRACBITS;
		}
		if (state->GetMisc2())
		{
			psp->sy = state->GetMisc2()<<FRACBITS;
		}

		// [BB] Some action functions rely on the fact that ReadyWeapon is not NULL.
		if (!nofunction && player->mo != NULL && player->ReadyWeapon)
		{
			if (state->CallAction(player->mo, player->ReadyWeapon))
			{
				if (!psp->state)
				{
					break;
				}
			}
		}

		state = psp->state->GetNextState();
	} while (!psp->tics); // An initial state of 0 could cycle through.
}

//---------------------------------------------------------------------------
//
// PROC P_BringUpWeapon
//
// Starts bringing the pending weapon up from the bottom of the screen.
// This is only called to start the rising, not throughout it.
//
//---------------------------------------------------------------------------

void P_BringUpWeapon (player_t *player)
{
	FState *newstate;
	AWeapon *weapon;

	if (player->PendingWeapon == WP_NOCHANGE)
	{
		if (player->ReadyWeapon != NULL)
		{
			player->psprites[ps_weapon].sy = WEAPONTOP;
			P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetReadyState());
		}
		return;
	}

	weapon = player->PendingWeapon;

	// If the player has a tome of power, use this weapon's powered up
	// version, if one is available.
	if (weapon != NULL &&
		weapon->SisterWeapon &&
		weapon->SisterWeapon->WeaponFlags & WIF_POWERED_UP &&
		player->mo->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2), true))
	{
		weapon = weapon->SisterWeapon;
	}

	if (weapon != NULL)
	{
		if (weapon->UpSound)
		{
			S_Sound (player->mo, CHAN_WEAPON, weapon->UpSound, 1, ATTN_NORM);
		}
		newstate = weapon->GetUpState ();
		player->refire = 0;
	}
	else
	{
		newstate = NULL;
	}
	player->PendingWeapon = WP_NOCHANGE;
	player->ReadyWeapon = weapon;
	player->psprites[ps_weapon].sy = player->cheats & CF_INSTANTWEAPSWITCH
		? WEAPONTOP : WEAPONBOTTOM;
	P_SetPsprite (player, ps_weapon, newstate);
}


//---------------------------------------------------------------------------
//
// PROC P_FireWeapon
//
//---------------------------------------------------------------------------

void P_FireWeapon (player_t *player, FState *state)
{
	AWeapon *weapon;

	// [SO] 9/2/02: People were able to do an awful lot of damage
	// when they were observers...
/* [BB] Zandronum doesn't use ZDoom's bot code.
	if (!player->isbot && bot_observer)
	{
		return;
	}
*/

	weapon = player->ReadyWeapon;
	if (weapon == NULL || !weapon->CheckAmmo (AWeapon::PrimaryFire, true))
	{
		// [BC] We need to do this, otherwise with the BFG10K, you can fire,
		// run out of ammo, find new ammo, switch back, and fire without
		// charging back up.
		player->refire = false;
		return;
	}

	// [BC] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BB] Except for the consoleplayer, the server handles this.
	if (( NETWORK_InClientMode() == false ) ||
		(( player - players ) == consoleplayer ))
	{
		player->mo->PlayAttacking ();
	}

	weapon->bAltFire = false;
	if (state == NULL)
	{
		state = weapon->GetAtkState(!!player->refire);
	}
	P_SetPsprite (player, ps_weapon, state);
	if (!(weapon->WeaponFlags & WIF_NOALERT))
	{
		P_NoiseAlert (player->mo, player->mo, false);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_FireWeaponAlt
//
//---------------------------------------------------------------------------

void P_FireWeaponAlt (player_t *player, FState *state)
{
	AWeapon *weapon;

/* [BB] Zandronum doesn't use ZDoom's bot code.
	// [SO] 9/2/02: People were able to do an awful lot of damage
	// when they were observers...
	if (!player->isbot && bot_observer)
	{
		return;
	}
*/

	weapon = player->ReadyWeapon;
	if (weapon == NULL || weapon->FindState(NAME_AltFire) == NULL || !weapon->CheckAmmo (AWeapon::AltFire, true))
	{
		return;
	}

	// [BB] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK_ALTFIRE, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BB] Except for the consoleplayer, the server handles this.
	if (( NETWORK_InClientMode() == false ) ||
		(( player - players ) == consoleplayer ))
	{
		player->mo->PlayAttacking ();
	}
	weapon->bAltFire = true;

	if (state == NULL)
	{
		state = weapon->GetAltAtkState(!!player->refire);
	}

	P_SetPsprite (player, ps_weapon, state);
	if (!(weapon->WeaponFlags & WIF_NOALERT))
	{
		P_NoiseAlert (player->mo, player->mo, false);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_ReloadWeapon
//
//---------------------------------------------------------------------------

void P_ReloadWeapon (player_t *player, FState *state)
{
	AWeapon *weapon;
	/* [BB] Zandronum doesn't use ZDoom's bot code.
	if (!player->isbot && bot_observer)
	{
		return;
	}
	*/

	weapon = player->ReadyWeapon;
	if (weapon == NULL)
	{
		return;
	}

	if (state == NULL)
	{
		state = weapon->GetRelState();
	}
	// [XA] don't change state if still null, so if the modder sets 
	// WRF_RELOAD to true but forgets to define the Reload state, the weapon
	// won't disappear. ;)
	if (state != NULL)
		P_SetPsprite (player, ps_weapon, state);
}

//---------------------------------------------------------------------------
//
// PROC P_ZoomWeapon
//
//---------------------------------------------------------------------------

void P_ZoomWeapon (player_t *player, FState *state)
{
	AWeapon *weapon;
	/* [BB] Zandronum doesn't use ZDoom's bot code.
	if (!player->isbot && bot_observer)
	{
		return;
	}
	*/

	weapon = player->ReadyWeapon;
	if (weapon == NULL)
	{
		return;
	}

	if (state == NULL)
	{
		state = weapon->GetZoomState();
	}
	// [XA] don't change state if still null. Same reasons as above.
	if (state != NULL)
		P_SetPsprite (player, ps_weapon, state);
}

//---------------------------------------------------------------------------
//
// PROC P_DropWeapon
//
// The player died, so put the weapon away.
//
//---------------------------------------------------------------------------

void P_DropWeapon (player_t *player)
{
	if (player == NULL)
	{
		return;
	}
	// Since the weapon is dropping, stop blocking switching.
	player->WeaponState &= ~WF_DISABLESWITCH;
	if (player->ReadyWeapon != NULL)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetDownState());
	}
}

//============================================================================
//
// P_BobWeapon
//
// [RH] Moved this out of A_WeaponReady so that the weapon can bob every
// tic and not just when A_WeaponReady is called. Not all weapons execute
// A_WeaponReady every tic, and it looks bad if they don't bob smoothly.
//
// [XA] Added new bob styles and exposed bob properties. Thanks, Ryan Cordell!
//
//============================================================================

void P_BobWeapon (player_t *player, pspdef_t *psp, fixed_t *x, fixed_t *y)
{
	static fixed_t curbob;

	AWeapon *weapon;
	fixed_t bobtarget;

	// [BC] Don't bob weapon if the player is spectating.
	if ( player->bSpectating )
		return;

	weapon = player->ReadyWeapon;

	if (weapon == NULL || weapon->WeaponFlags & WIF_DONTBOB)
	{
		*x = *y = 0;
		return;
	}

	// [XA] Get the current weapon's bob properties.
	int bobstyle = weapon->BobStyle;
	int bobspeed = (weapon->BobSpeed * 128) >> 16;
	fixed_t rangex = weapon->BobRangeX;
	fixed_t rangey = weapon->BobRangeY;

	// Bob the weapon based on movement speed.
	int angle = (bobspeed*35/TICRATE*level.time)&FINEMASK;

	// [RH] Smooth transitions between bobbing and not-bobbing frames.
	// This also fixes the bug where you can "stick" a weapon off-center by
	// shooting it when it's at the peak of its swing.
	bobtarget = (player->WeaponState & WF_WEAPONBOBBING) ? player->bob : 0;
	if (curbob != bobtarget)
	{
		if (abs (bobtarget - curbob) <= 1*FRACUNIT)
		{
			curbob = bobtarget;
		}
		else
		{
			fixed_t zoom = MAX<fixed_t> (1*FRACUNIT, abs (curbob - bobtarget) / 40);
			if (curbob > bobtarget)
			{
				curbob -= zoom;
			}
			else
			{
				curbob += zoom;
			}
		}
	}

	if (curbob != 0)
	{
		fixed_t bobx = FixedMul(player->bob, rangex);
		fixed_t boby = FixedMul(player->bob, rangey);
		switch (bobstyle)
		{
		case AWeapon::BobNormal:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;
			
		case AWeapon::BobInverse:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = boby - FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;
			
		case AWeapon::BobAlpha:
			*x = FixedMul(bobx, finesine[angle]);
			*y = FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;
			
		case AWeapon::BobInverseAlpha:
			*x = FixedMul(bobx, finesine[angle]);
			*y = boby - FixedMul(boby, finesine[angle & (FINEANGLES/2-1)]);
			break;
			
		case AWeapon::BobSmooth:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = (boby - FixedMul(boby, finecosine[angle*2 & (FINEANGLES-1)])) / 2;
			break;

		case AWeapon::BobInverseSmooth:
			*x = FixedMul(bobx, finecosine[angle]);
			*y = (FixedMul(boby, finecosine[angle*2 & (FINEANGLES-1)]) + boby) / 2;
		}
	}
	else
	{
		*x = 0;
		*y = 0;
	}
}

//============================================================================
//
// PROC A_WeaponReady
//
// Readies a weapon for firing or bobbing with its three ancillary functions,
// DoReadyWeaponToSwitch(), DoReadyWeaponToFire() and DoReadyWeaponToBob().
// [XA] Added DoReadyWeaponToReload() and DoReadyWeaponToZoom()
//
//============================================================================

void DoReadyWeaponToSwitch (AActor *self, bool switchable)
{
	// Prepare for switching action.
	player_t *player;
	if (self && (player = self->player))
	{
		if (switchable)
		{
			player->WeaponState |= WF_WEAPONSWITCHOK | WF_REFIRESWITCHOK;
		}
		else
		{
			// WF_WEAPONSWITCHOK is automatically cleared every tic by P_SetPsprite().
			player->WeaponState &= ~WF_REFIRESWITCHOK;
		}
	}
}

void DoReadyWeaponDisableSwitch (AActor *self, INTBOOL disable)
{
	// Discard all switch attempts?
	player_t *player;
	if (self && (player = self->player))
	{
		if (disable)
		{
			player->WeaponState |= WF_DISABLESWITCH;
			player->WeaponState &= ~WF_REFIRESWITCHOK;
		}
		else
		{
			player->WeaponState &= ~WF_DISABLESWITCH;
		}
	}
}

void DoReadyWeaponToFire (AActor *self, bool prim, bool alt)
{
	player_t *player;
	AWeapon *weapon;

	if (!self || !(player = self->player) || !(weapon = player->ReadyWeapon))
	{
		return;
	}

	// Change player from attack state
	if (self->InStateSequence(self->state, self->MissileState) ||
		self->InStateSequence(self->state, self->MeleeState))
	{
		static_cast<APlayerPawn *>(self)->PlayIdle ();
	}

	// Play ready sound, if any.
	if (weapon->ReadySound && player->psprites[ps_weapon].state == weapon->FindState(NAME_Ready))
	{
		if (!(weapon->WeaponFlags & WIF_READYSNDHALF) || pr_wpnreadysnd() < 128)
		{
			S_Sound (self, CHAN_WEAPON, weapon->ReadySound, 1, ATTN_NORM);

			// [BC] If we're the server, tell other clients to play the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( weapon->ReadySound ), 1, ATTN_NORM, ULONG( player - players ), SVCF_SKIPTHISCLIENT );
		}
	}

	// Prepare for firing action.
	player->WeaponState |= ((prim ? WF_WEAPONREADY : 0) | (alt ? WF_WEAPONREADYALT : 0));
	return;
}

void DoReadyWeaponToBob (AActor *self)
{
	if (self && self->player && self->player->ReadyWeapon)
	{
		// Prepare for bobbing action.
		self->player->WeaponState |= WF_WEAPONBOBBING;
		self->player->psprites[ps_weapon].sx = 0;
		self->player->psprites[ps_weapon].sy = WEAPONTOP;
	}
}

void DoReadyWeaponToReload (AActor *self)
{
	// Prepare for reload action.
	player_t *player;
	if (self && (player = self->player))
		player->WeaponState |= WF_WEAPONRELOADOK;
	return;
}

void DoReadyWeaponToZoom (AActor *self)
{
	// Prepare for reload action.
	player_t *player;
	if (self && (player = self->player))
		player->WeaponState |= WF_WEAPONZOOMOK;
	return;
}

// This function replaces calls to A_WeaponReady in other codepointers.
void DoReadyWeapon(AActor *self)
{
	DoReadyWeaponToBob(self);
	DoReadyWeaponToFire(self);
	DoReadyWeaponToSwitch(self);
	DoReadyWeaponToReload(self);
	DoReadyWeaponToZoom(self);
}

enum EWRF_Options
{
	WRF_NoBob = 1,
	WRF_NoSwitch = 2,
	WRF_NoPrimary = 4,
	WRF_NoSecondary = 8,
	WRF_NoFire = WRF_NoPrimary + WRF_NoSecondary,
	WRF_AllowReload = 16,
	WRF_AllowZoom = 32,
	WRF_DisableSwitch = 64,
};

DEFINE_ACTION_FUNCTION_PARAMS(AInventory, A_WeaponReady)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(paramflags, 0);

													DoReadyWeaponToSwitch(self, !(paramflags & WRF_NoSwitch));
	if ((paramflags & WRF_NoFire) != WRF_NoFire)	DoReadyWeaponToFire(self, !(paramflags & WRF_NoPrimary), !(paramflags & WRF_NoSecondary));
	if (!(paramflags & WRF_NoBob))					DoReadyWeaponToBob(self);
	if ((paramflags & WRF_AllowReload))				DoReadyWeaponToReload(self);
	if ((paramflags & WRF_AllowZoom))				DoReadyWeaponToZoom(self);

	DoReadyWeaponDisableSwitch(self, paramflags & WRF_DisableSwitch);
}

//---------------------------------------------------------------------------
//
// PROC P_CheckWeaponFire
//
// The player can fire the weapon.
// [RH] This was in A_WeaponReady before, but that only works well when the
// weapon's ready frames have a one tic delay.
//
//---------------------------------------------------------------------------

void P_CheckWeaponFire (player_t *player)
{
	AWeapon *weapon = player->ReadyWeapon;

	if (weapon == NULL)
		return;

	// Check for fire. Some weapons do not auto fire.
	if ((player->WeaponState & WF_WEAPONREADY) && (player->cmd.ucmd.buttons & BT_ATTACK))
	{
		if (!player->attackdown || !(weapon->WeaponFlags & WIF_NOAUTOFIRE))
		{
			player->attackdown = true;
			P_FireWeapon (player, NULL);
			return;
		}
	}
	else if ((player->WeaponState & WF_WEAPONREADYALT) && (player->cmd.ucmd.buttons & BT_ALTATTACK))
	{
		if (!player->attackdown || !(weapon->WeaponFlags & WIF_NOAUTOFIRE))
		{
			player->attackdown = true;
			P_FireWeaponAlt (player, NULL);
			return;
		}
	}
	else
	{
		player->attackdown = false;
	}
}

//---------------------------------------------------------------------------
//
// PROC P_CheckWeaponSwitch
//
// The player can change to another weapon at this time.
// [GZ] This was cut from P_CheckWeaponFire.
//
//---------------------------------------------------------------------------

void P_CheckWeaponSwitch (player_t *player)
{
	if (player == NULL)
	{
		return;
	}
	if ((player->WeaponState & WF_DISABLESWITCH) || // Weapon changing has been disabled.
		player->morphTics != 0)					// Morphed classes cannot change weapons.
	{ // ...so throw away any pending weapon requests.
		player->PendingWeapon = WP_NOCHANGE;
	}

	// Put the weapon away if the player has a pending weapon or has died, and
	// we're at a place in the state sequence where dropping the weapon is okay.
	if ((player->PendingWeapon != WP_NOCHANGE || player->health <= 0) &&
		player->WeaponState & WF_WEAPONSWITCHOK)
	{
		P_DropWeapon(player);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_CheckWeaponReload
//
// The player can reload the weapon.
//
//---------------------------------------------------------------------------

void P_CheckWeaponReload (player_t *player)
{
	AWeapon *weapon = player->ReadyWeapon;

	if (weapon == NULL)
		return;

	// Check for reload.
	if ((player->WeaponState & WF_WEAPONRELOADOK) && (player->cmd.ucmd.buttons & BT_RELOAD))
	{
		P_ReloadWeapon (player, NULL);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_CheckWeaponZoom
//
// The player can use the weapon's zoom function.
//
//---------------------------------------------------------------------------

void P_CheckWeaponZoom (player_t *player)
{
	AWeapon *weapon = player->ReadyWeapon;

	if (weapon == NULL)
		return;

	// Check for zoom.
	if ((player->WeaponState & WF_WEAPONZOOMOK) && (player->cmd.ucmd.buttons & BT_ZOOM))
	{
		P_ZoomWeapon (player, NULL);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_ReFire
//
// The player can re-fire the weapon without lowering it entirely.
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AInventory, A_ReFire)
{
	ACTION_PARAM_START(1)
	ACTION_PARAM_STATE(state, 0);

	A_ReFire(self, state);
}

void A_ReFire(AActor *self, FState *state)
{
	player_t *player = self->player;
	bool pending;

	if (NULL == player)
	{
		return;
	}
	pending = player->PendingWeapon != WP_NOCHANGE && (player->WeaponState & WF_REFIRESWITCHOK);
	if ((player->cmd.ucmd.buttons & BT_ATTACK)
		&& !player->ReadyWeapon->bAltFire && !pending && player->health > 0)
	{
		player->refire++;
		P_FireWeapon (player, state);
	}
	else if ((player->cmd.ucmd.buttons & BT_ALTATTACK)
		&& player->ReadyWeapon->bAltFire && !pending && player->health > 0)
	{
		player->refire++;
		P_FireWeaponAlt (player, state);
	}
	else
	{
		player->refire = 0;
		player->ReadyWeapon->CheckAmmo (player->ReadyWeapon->bAltFire
			? AWeapon::AltFire : AWeapon::PrimaryFire, true);
	}
}

DEFINE_ACTION_FUNCTION(AInventory, A_ClearReFire)
{
	player_t *player = self->player;

	if (NULL != player)
	{
		player->refire = 0;
	}
}

//---------------------------------------------------------------------------
//
// PROC A_CheckReload
//
// Present in Doom, but unused. Also present in Strife, and actually used.
// This and what I call A_XBowReFire are actually the same thing in Strife,
// not two separate functions as I have them here.
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AInventory, A_CheckReload)
{
	if (self->player != NULL)
	{
		self->player->ReadyWeapon->CheckAmmo (
			self->player->ReadyWeapon->bAltFire ? AWeapon::AltFire
			: AWeapon::PrimaryFire, true);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_Lower
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AInventory, A_Lower)
{
	player_t *player = self->player;
	pspdef_t *psp;

	if (NULL == player)
	{
		return;
	}
	psp = &player->psprites[ps_weapon];

	// [BC] If we're a spectator, lower weapon completely and do not raise it.
	if ( player->bSpectating )
	{
		psp->sy = WEAPONBOTTOM;
		return;
	}

	if (player->morphTics || player->cheats & CF_INSTANTWEAPSWITCH)
	{
		psp->sy = WEAPONBOTTOM;
	}
	else
	{
		psp->sy += LOWERSPEED;
	}
	if (psp->sy < WEAPONBOTTOM)
	{ // Not lowered all the way yet
		return;
	}
	if (player->playerstate == PST_DEAD)
	{ // Player is dead, so don't bring up a pending weapon
		psp->sy = WEAPONBOTTOM;
	
		// Player is dead, so keep the weapon off screen
		P_SetPsprite (player,  ps_weapon, NULL);
		return;
	}
	// [RH] Clear the flash state. Only needed for Strife.
	P_SetPsprite (player, ps_flash, NULL);
	P_BringUpWeapon (player);
}

//---------------------------------------------------------------------------
//
// PROC A_Raise
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AInventory, A_Raise)
{
	if (self == NULL)
	{
		return;
	}
	player_t *player = self->player;
	pspdef_t *psp;

	if (NULL == player)
	{
		return;
	}
	// [BB] ZACOMPATF_OLD_WEAPON_SWITCH also restores the original weapon switch cancellation behavior.
	// [CK] Changed to now be separate from ZACOMPATF_OLD_WEAPON_SWITCH
	if (player->PendingWeapon != WP_NOCHANGE && !( zacompatflags & ZACOMPATF_FULL_WEAPON_LOWER ))
	{
		P_DropWeapon(player);
		return;
	}
	psp = &player->psprites[ps_weapon];
	psp->sy -= RAISESPEED;
	if (psp->sy > WEAPONTOP)
	{ // Not raised all the way yet
		return;
	}
	psp->sy = WEAPONTOP;
	if (player->ReadyWeapon != NULL)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetReadyState());
	}
	else
	{
		player->psprites[ps_weapon].state = NULL;
	}

	// [BC] If this player has respawn invulnerability, disable it if they're done raising
	// a weapon that isn't the pistol or their fist.
	if (( player->mo ) &&
		( NETWORK_InClientMode() == false ))
	{
		APowerInvulnerable	*pInvulnerability;

		pInvulnerability = static_cast<APowerInvulnerable *>( player->mo->FindInventory( RUNTIME_CLASS( APowerInvulnerable )));
		if (( pInvulnerability ) &&
			( player->ReadyWeapon ) &&
			(( player->ReadyWeapon->WeaponFlags & WIF_ALLOW_WITH_RESPAWN_INVUL ) == false ) &&
			(( player->mo->effects & FX_VISIBILITYFLICKER ) || ( player->mo->effects & FX_RESPAWNINVUL )))
		{
			pInvulnerability->Destroy( );

			// If we're the server, tell clients to take this player's powerup away.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( ULONG( player - players ), RUNTIME_CLASS( APowerInvulnerable ), 0 );
		}
	}
}




//
// A_GunFlash
//
enum GF_Flags
{
	GFF_NOEXTCHANGE = 1,
};

DEFINE_ACTION_FUNCTION_PARAMS(AInventory, A_GunFlash)
{
	ACTION_PARAM_START(2)
	ACTION_PARAM_STATE(flash, 0);
	ACTION_PARAM_INT(Flags, 1);

	// [BB] Zandronum needs A_GunFlash in a_doomweaps, so I moved the code into a function.
	A_GunFlash(self, flash, Flags);
}

void A_GunFlash(AActor *self, FState *flash, const int Flags)
{
	player_t *player = self->player;

	if (NULL == player)
	{
		return;
	}
	if(!(Flags & GFF_NOEXTCHANGE))
	{
		// [BC] Since the player can be dead at this point as a result of shooting a player with
		// the reflection rune, we need to make sure the player is alive before playing the
		// attacking animation.
		if ( player->mo->health > 0 )
		{
			// [BB] If we're the server, tell clients to update this player's state.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

			// [BB] Clients only do this for "their" player.
			if ( NETWORK_IsConsolePlayerOrNotInClientMode( player ) )
				player->mo->PlayAttacking2 ();
		}
	}

	if (flash == NULL)
	{
		if (player->ReadyWeapon->bAltFire) flash = player->ReadyWeapon->FindState(NAME_AltFlash);
		if (flash == NULL) flash = player->ReadyWeapon->FindState(NAME_Flash);
	}
	P_SetPsprite (player, ps_flash, flash);
}



//
// WEAPON ATTACKS
//

//
// P_BulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//

angle_t P_BulletSlope (AActor *mo, AActor **pLineTarget)
{
	static const int angdiff[15] = {
		AUTOAIM_MINANGLE * -1, AUTOAIM_MINANGLE * 1, AUTOAIM_MINANGLE * -2, AUTOAIM_MINANGLE * 2,
		AUTOAIM_MINANGLE * -3, AUTOAIM_MINANGLE * 3, AUTOAIM_MINANGLE * -4, AUTOAIM_MINANGLE * 4,
		AUTOAIM_MINANGLE * -5, AUTOAIM_MINANGLE * 5, AUTOAIM_MINANGLE * -6, AUTOAIM_MINANGLE * 6,
		-( 1<<26 ), 1<<26, 0 }; // [CK] New angles
	int i;
	angle_t an;
	angle_t pitch;
	AActor *linetarget;
	int endIndex = zacompatflags & ZACOMPATF_AUTOAIM ? 12 : 0; // [CK/TP] Our ending index depends on compatflags.

	// [Spleen]
	UNLAGGED_Reconcile( mo );
	UNLAGGED_AddReconciliationBlocker( );

	// see which target is to be aimed at
	i = 14; // [TP/CK] Now 14
	do
	{
		an = mo->angle + angdiff[i];
		pitch = P_AimLineAttack (mo, an, 16*64*FRACUNIT, &linetarget);

		if (mo->player != NULL &&
			level.IsFreelookAllowed() &&
			mo->player->userinfo.GetAimDist() <= ANGLE_1/2)
		{
			break;
		}
	} while (linetarget == NULL && --i >= endIndex); // [TP] 0 changed to endIndex
	if (pLineTarget != NULL)
	{
		*pLineTarget = linetarget;
	}

	// [Spleen]
	UNLAGGED_RemoveReconciliationBlocker( );
	UNLAGGED_Restore( mo );

	return pitch;
}


//
// P_GunShot
//
void P_GunShot (AActor *mo, bool accurate, const PClass *pufftype, angle_t pitch)
{
	angle_t 	angle;
	int 		damage;
		
	damage = 5*(pr_gunshot()%3+1);
	angle = mo->angle;

	if (!accurate)
	{
		angle += pr_gunshot.Random2 () << 18;
	}

	P_LineAttack (mo, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, pufftype);
}

DEFINE_ACTION_FUNCTION(AInventory, A_Light0)
{
	if (self->player != NULL)
	{
		self->player->extralight = 0;
	}
}

DEFINE_ACTION_FUNCTION(AInventory, A_Light1)
{
	if (self->player != NULL)
	{
		self->player->extralight = 1;
	}
}

DEFINE_ACTION_FUNCTION(AInventory, A_Light2)
{
	if (self->player != NULL)
	{
		self->player->extralight = 2;
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AInventory, A_Light)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(light, 0);

	if (self->player != NULL)
	{
		self->player->extralight = clamp<int>(light, -20, 20);
	}
}

//------------------------------------------------------------------------
//
// PROC P_SetupPsprites
//
// Called at start of level for each player
//
//------------------------------------------------------------------------

void P_SetupPsprites(player_t *player, bool startweaponup)
{
	int i;

	// Remove all psprites
	for (i = 0; i < NUMPSPRITES; i++)
	{
		player->psprites[i].state = NULL;
	}
	// Spawn the ready weapon
	player->PendingWeapon = !startweaponup ? player->ReadyWeapon : WP_NOCHANGE;
	P_BringUpWeapon (player);
}

//------------------------------------------------------------------------
//
// PROC P_MovePsprites
//
// Called every tic by player thinking routine
//
//------------------------------------------------------------------------

void P_MovePsprites (player_t *player)
{
	int i;
	pspdef_t *psp;
	FState *state;

	// [RH] If you don't have a weapon, then the psprites should be NULL.
	if (player->ReadyWeapon == NULL && (player->health > 0 || player->mo->DamageType != NAME_Fire))
	{
		P_SetPsprite (player, ps_weapon, NULL);
		P_SetPsprite (player, ps_flash, NULL);
		if (player->PendingWeapon != WP_NOCHANGE)
		{
			P_BringUpWeapon (player);
		}
	}
	else
	{
		psp = &player->psprites[0];
		for (i = 0; i < NUMPSPRITES; i++, psp++)
		{
			if ((state = psp->state) != NULL && psp->processPending) // a null state means not active
			{
				// drop tic count and possibly change state
				if (psp->tics != -1)	// a -1 tic count never changes
				{
					psp->tics--;

					// [BC] Apply double firing speed.
					if ( psp->tics && (player->cheats & CF_DOUBLEFIRINGSPEED))
						psp->tics--;

					if(!psp->tics)
					{
						P_SetPsprite (player, i, psp->state->GetNextState());
					}
				}
			}
		}
		player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
		player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
		P_CheckWeaponSwitch (player);
		if (player->WeaponState & (WF_WEAPONREADY | WF_WEAPONREADYALT))
		{
			P_CheckWeaponFire (player);
		}
		if (player->WeaponState & WF_WEAPONRELOADOK)
		{
			P_CheckWeaponReload (player);
		}
		if (player->WeaponState & WF_WEAPONZOOMOK)
		{
			P_CheckWeaponZoom (player);
		}
	}
}

FArchive &operator<< (FArchive &arc, pspdef_t &def)
{
	arc << def.state << def.tics << def.sx << def.sy
		<< def.sprite << def.frame;
	return arc;
}
