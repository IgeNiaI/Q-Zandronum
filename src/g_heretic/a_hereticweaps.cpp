/*
#include "templates.h"
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "gi.h"
#include "r_data/r_translate.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_sap ("StaffAtkPL1");
static FRandom pr_sap2 ("StaffAtkPL2");
static FRandom pr_fgw ("FireWandPL1");
static FRandom pr_fgw2 ("FireWandPL2");
static FRandom pr_boltspark ("BoltSpark");
static FRandom pr_macerespawn ("MaceRespawn");
static FRandom pr_maceatk ("FireMacePL1");
static FRandom pr_gatk ("GauntletAttack");
static FRandom pr_bfx1 ("BlasterFX1");
static FRandom pr_ripd ("RipperD");
static FRandom pr_fb1 ("FireBlasterPL1");
static FRandom pr_bfx1t ("BlasterFX1Tick");
static FRandom pr_hrfx2 ("HornRodFX2");
static FRandom pr_rp ("RainPillar");
static FRandom pr_fsr1 ("FireSkullRodPL1");
static FRandom pr_storm ("SkullRodStorm");
static FRandom pr_impact ("RainImpact");
static FRandom pr_pfx1 ("PhoenixFX1");
static FRandom pr_pfx2 ("PhoenixFX2");
static FRandom pr_fp2 ("FirePhoenixPL2");

#define FLAME_THROWER_TICS (10*TICRATE)

void P_DSparilTeleport (AActor *actor);

#define USE_BLSR_AMMO_1 1
#define USE_BLSR_AMMO_2 5
#define USE_SKRD_AMMO_1 1
#define USE_SKRD_AMMO_2 5
#define USE_PHRD_AMMO_1 1
#define USE_PHRD_AMMO_2 1
#define USE_MACE_AMMO_1 1
#define USE_MACE_AMMO_2 5

extern bool P_AutoUseChaosDevice (player_t *player);

// --- Staff ----------------------------------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_StaffAttackPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_StaffAttack)
{
	angle_t angle;
	int slope;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}

	ACTION_PARAM_START(2);
	ACTION_PARAM_INT(damage, 0);
	ACTION_PARAM_CLASS(puff, 1);

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (puff == NULL) puff = PClass::FindClass(NAME_BulletPuff);	// just to be sure
	angle = self->angle;
	angle += pr_sap.Random2() << 18;
	slope = P_AimLineAttack (self, angle, MELEERANGE, &linetarget);
	P_LineAttack (self, angle, MELEERANGE, slope, damage, NAME_Melee, puff, true, &linetarget);

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		P_LineAttack(self, angle + ( ANGLE_45 / 3 ), MELEERANGE, slope, damage, NAME_Melee, puff, true);
		P_LineAttack(self, angle - ( ANGLE_45 / 3 ), MELEERANGE, slope, damage, NAME_Melee, puff, true);
	}

	if (linetarget)
	{
		//S_StartSound(player->mo, sfx_stfhit);
		// turn to face target
		self->angle = R_PointToAngle2 (self->x,
			self->y, linetarget->x, linetarget->y);

		// [BC] If we're the server, tell clients to adjust the player's angle.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingAngle( player->mo );
	}
}


//----------------------------------------------------------------------------
//
// PROC A_FireGoldWandPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireGoldWandPL1)
{
	angle_t angle;
	int damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] If we're the client, just play the sound and get out.
	if ( NETWORK_InClientMode() )
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM );
		return;
	}

	angle_t pitch = P_BulletSlope(self);
	damage = 7+(pr_fgw()&7);
	angle = self->angle;
	if (player->refire)
	{
		angle += pr_fgw.Random2() << 18;
	}
	P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, "GoldWandPuff1");

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		angle = self->angle;
		if (player->refire)
		{
			angle += pr_fgw.Random2() << 18;
		}
		P_LineAttack(self, angle + ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, "GoldWandPuff1");

		angle = self->angle;
		if (player->refire)
		{
			angle += pr_fgw.Random2() << 18;
		}
		P_LineAttack(self, angle - ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, "GoldWandPuff1");
	}

	// [BB] If the player hit a player with his attack, potentially give him a medal.
	PLAYER_CheckStruckPlayer ( self );

	S_Sound (self, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/wandhit", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//----------------------------------------------------------------------------
//
// PROC A_FireGoldWandPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireGoldWandPL2)
{
	int i;
	angle_t angle;
	int damage;
	fixed_t velz;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] If we're the client, just play the sound and get out.
	if ( NETWORK_InClientMode() )
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM );
		return;
	}

	angle_t pitch = P_BulletSlope(self);
	velz = FixedMul (GetDefaultByName("GoldWandFX2")->Speed,
		finetangent[FINEANGLES/4-((signed)pitch>>ANGLETOFINESHIFT)]);
	P_SpawnMissileAngle (self, PClass::FindClass("GoldWandFX2"), self->angle-(ANG45/8), velz, true); // [BB] Inform clients
	P_SpawnMissileAngle (self, PClass::FindClass("GoldWandFX2"), self->angle+(ANG45/8), velz, true); // [BB] Inform clients
	angle = self->angle-(ANG45/8);
	for(i = 0; i < 5; i++)
	{
		damage = 1+(pr_fgw2()&7);
		P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff2");
		angle += ((ANG45/8)*2)/4;
	}

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		P_SpawnMissileAngle( self, PClass::FindClass("GoldWandFX2"), self->angle - ( ANG45 / 8 ) + ( ANGLE_45 / 3 ), velz, true); // [BB] Inform clients
		P_SpawnMissileAngle( self, PClass::FindClass("GoldWandFX2"), self->angle + ( ANG45 / 8 ) + ( ANGLE_45 / 3 ), velz, true); // [BB] Inform clients
		angle = self->angle - ( ANG45 / 8 ) + ( ANGLE_45 / 3 );
		for ( i = 0; i < 5; i++ )
		{
			damage = 1+(pr_fgw2()&7);
			P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, "GoldWandPuff2");
			angle += ((ANG45/8)*2)/4;
		}

		P_SpawnMissileAngle( self, PClass::FindClass("GoldWandFX2"), self->angle - ( ANG45 / 8 ) - ( ANGLE_45 / 3 ), velz, true); // [BB] Inform clients
		P_SpawnMissileAngle( self, PClass::FindClass("GoldWandFX2"), self->angle + ( ANG45 / 8 ) - ( ANGLE_45 / 3 ), velz, true); // [BB] Inform clients
		angle = self->angle - ( ANG45 / 8 ) - ( ANGLE_45 / 3 );
		for ( i = 0; i < 5; i++ )
		{
			damage = 1+(pr_fgw2()&7);
			P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, "GoldWandPuff2");
			angle += ((ANG45/8)*2)/4;
		}
	}
	S_Sound (self, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM);


	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/wandhit", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//----------------------------------------------------------------------------
//
// PROC A_FireCrossbowPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireCrossbowPL1)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// [BB] Spread
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX1"));
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX3"), self->angle-(ANG45/10));
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX3"), self->angle+(ANG45/10));
}

//----------------------------------------------------------------------------
//
// PROC A_FireCrossbowPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireCrossbowPL2)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{		
		return;
	}

	// [BB] Spread
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX2"));
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX2"), self->angle-(ANG45/10));
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX2"), self->angle+(ANG45/10));
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX3"), self->angle-(ANG45/5));
	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("CrossbowFX3"), self->angle+(ANG45/5));
}

//---------------------------------------------------------------------------
//
// PROC A_GauntletAttack
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GauntletAttack)
{
	angle_t angle;
	int damage;
	int slope;
	int randVal;
	fixed_t dist;
	player_t *player;
	const PClass *pufftype;
	AActor *linetarget;
	int actualdamage = 0;

	if (NULL == (player = self->player))
	{
		return;
	}

	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(power, 0);

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	player->psprites[ps_weapon].sx = ((pr_gatk()&3)-2) * FRACUNIT;
	player->psprites[ps_weapon].sy = WEAPONTOP + (pr_gatk()&3) * FRACUNIT;
	angle = self->angle;
	if (power)
	{
		damage = pr_gatk.HitDice (2);
		dist = 4*MELEERANGE;
		angle += pr_gatk.Random2() << 17;
		pufftype = PClass::FindClass("GauntletPuff2");
	}
	else
	{
		damage = pr_gatk.HitDice (2);
		dist = MELEERANGE+1;
		angle += pr_gatk.Random2() << 18;
		pufftype = PClass::FindClass("GauntletPuff1");
	}
	slope = P_AimLineAttack (self, angle, dist, &linetarget);
	P_LineAttack (self, angle, dist, slope, damage, NAME_Melee, pufftype, false, &linetarget, &actualdamage);
	if (!linetarget)
	{
		if (pr_gatk() > 64)
		{
			player->extralight = !player->extralight;
		}
		S_Sound (self, CHAN_AUTO, "weapons/gauntletson", 1, ATTN_NORM);
		return;
	}
	randVal = pr_gatk();
	if (randVal < 64)
	{
		player->extralight = 0;
	}
	else if (randVal < 160)
	{
		player->extralight = 1;
	}
	else
	{
		player->extralight = 2;
	}

	// [EP] Clients should not execute this, let the server know what's needed.
	if ( NETWORK_InClientMode() == false )
	{
		if (power)
		{
			// [EP] Save the current health value for bandwidth.
			const int prevhealth = self->health;

			if (!(linetarget->flags5 & MF5_DONTDRAIN)) P_GiveBody (self, actualdamage>>1);
			S_Sound (self, CHAN_AUTO, "weapons/gauntletspowhit", 1, ATTN_NORM);

			// [EP] Inform the clients about the sound and the possible health change.
			if ( NETWORK_GetState() == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SoundActor( self, CHAN_AUTO, "weapons/gauntletspowhit", 1, ATTN_NORM );
				if ( prevhealth != self->health )
					 SERVERCOMMANDS_SetPlayerHealth( player - players );
			}
		}
		else
		{
			S_Sound (self, CHAN_AUTO, "weapons/gauntletshit", 1, ATTN_NORM);

			// [EP] Inform the clients about the sound.
			if ( NETWORK_GetState() == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( self, CHAN_AUTO, "weapons/gauntletshit", 1, ATTN_NORM );
		}
	}
	// turn to face target
	angle = R_PointToAngle2 (self->x, self->y,
		linetarget->x, linetarget->y);
	if (angle-self->angle > ANG180)
	{
		if ((int)(angle-self->angle) < -ANG90/20)
			self->angle = angle+ANG90/21;
		else
			self->angle -= ANG90/20;
	}
	else
	{
		if (angle-self->angle > ANG90/20)
			self->angle = angle-ANG90/21;
		else
			self->angle += ANG90/20;
	}
	self->flags |= MF_JUSTATTACKED;
}

// --- Mace -----------------------------------------------------------------

#define MAGIC_JUNK 1234

// Mace FX4 -----------------------------------------------------------------

class AMaceFX4 : public AActor
{
	DECLARE_CLASS (AMaceFX4, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AMaceFX4)

int AMaceFX4::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if ((target->flags2 & MF2_BOSS) || (target->flags3 & MF3_DONTSQUASH) || target->IsTeammate (this->target))
	{ // Don't allow cheap boss kills and don't instagib teammates
		return damage;
	}
	else if (target->player)
	{ // Player specific checks
		if (target->player->mo->flags2 & MF2_INVULNERABLE)
		{ // Can't hurt invulnerable players
			return -1;
		}
		if (P_AutoUseChaosDevice (target->player))
		{ // Player was saved using chaos device
			return -1;
		}
	}
	return TELEFRAG_DAMAGE; // Something's gonna die
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL1B
//
//----------------------------------------------------------------------------

void FireMacePL1B (AActor *actor)
{
	AActor *ball;
	angle_t angle;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	ball = Spawn("MaceFX2", actor->x, actor->y, actor->z + 28*FRACUNIT 
		- actor->floorclip, ALLOW_REPLACE);
	ball->velz = 2*FRACUNIT+/*((player->lookdir)<<(FRACBITS-5))*/
		finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
	angle = actor->angle;
	ball->target = actor;
	ball->angle = angle;
	ball->z += 2*finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
	angle >>= ANGLETOFINESHIFT;
	ball->velx = (actor->velx>>1) + FixedMul(ball->Speed, finecosine[angle]);
	ball->vely = (actor->vely>>1) + FixedMul(ball->Speed, finesine[angle]);
	S_Sound (ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM);

	// [BC] If we're the server, spawn the ball and play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnMissile( ball );
		SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM );
	}

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		ball = Spawn("MaceFX2", actor->x, actor->y, actor->z + 28*FRACUNIT 
			- actor->floorclip, ALLOW_REPLACE);
		ball->velz = 2*FRACUNIT+/*((player->lookdir)<<(FRACBITS-5))*/
			finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle = actor->angle + ( ANGLE_45 / 3 );
		ball->target = actor;
		ball->angle = angle;
		ball->z += 2*finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle >>= ANGLETOFINESHIFT;
		ball->velx = (actor->velx>>1)+FixedMul(ball->Speed, finecosine[angle]);
		ball->vely = (actor->vely>>1)+FixedMul(ball->Speed, finesine[angle]);
		S_Sound (ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM);

		// [BC] If we're the server, spawn the ball and play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnMissile( ball );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM );
		}

		ball = Spawn("MaceFX2", actor->x, actor->y, actor->z + 28*FRACUNIT 
			- actor->floorclip, ALLOW_REPLACE);
		ball->velz = 2*FRACUNIT+/*((player->lookdir)<<(FRACBITS-5))*/
			finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle = actor->angle - ( ANGLE_45 / 3 );
		ball->target = actor;
		ball->angle = angle;
		ball->z += 2*finetangent[FINEANGLES/4-(actor->pitch>>ANGLETOFINESHIFT)];
		angle >>= ANGLETOFINESHIFT;
		ball->velx = (actor->velx>>1)+FixedMul(ball->Speed, finecosine[angle]);
		ball->vely = (actor->vely>>1)+FixedMul(ball->Speed, finesine[angle]);
		S_Sound (ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM);

		// [BC] If we're the server, spawn the ball and play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnMissile( ball );
			SERVERCOMMANDS_SoundActor( ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM );
		}
	}

	P_CheckMissileSpawn (ball, actor->radius);
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireMacePL1)
{
	AActor *ball;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	if (pr_maceatk() < 28)
	{
		FireMacePL1B (self);
		return;
	}
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	player->psprites[ps_weapon].sx = ((pr_maceatk()&3)-2)*FRACUNIT;
	player->psprites[ps_weapon].sy = WEAPONTOP+(pr_maceatk()&3)*FRACUNIT;

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	ball = P_SpawnPlayerMissile (self, PClass::FindClass("MaceFX1"),
		self->angle+(((pr_maceatk()&7)-4)<<24));
	if (ball)
	{
		ball->special1 = 16; // tics till dropoff
	}

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		ball = P_SpawnPlayerMissile (self, PClass::FindClass("MaceFX1"),
			self->angle+(((pr_maceatk()&7)-4)<<24) + ( ANGLE_45 / 3 ));
		if (ball)
		{
			ball->special1 = 16; // tics till dropoff
		}

		ball = P_SpawnPlayerMissile (self, PClass::FindClass("MaceFX1"),
			self->angle+(((pr_maceatk()&7)-4)<<24) - ( ANGLE_45 / 3 ));
		if (ball)
		{
			ball->special1 = 16; // tics till dropoff
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MacePL1Check
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MacePL1Check)
{
	// [BC] Let the server handle this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (self->special1 == 0)
	{
		return;
	}
	self->special1 -= 4;
	if (self->special1 > 0)
	{
		return;
	}
	self->special1 = 0;
	self->flags &= ~MF_NOGRAVITY;
	self->gravity = FRACUNIT/8;
	// [RH] Avoid some precision loss by scaling the velocity directly
#if 0
	// This is the original code, for reference.
	angle_t angle = self->angle>>ANGLETOFINESHIFT;
	self->velx = FixedMul(7*FRACUNIT, finecosine[angle]);
	self->vely = FixedMul(7*FRACUNIT, finesine[angle]);
#else
	double velscale = sqrt ((double)self->velx * (double)self->velx +
							 (double)self->vely * (double)self->vely);
	velscale = 458752 / velscale;
	self->velx = (int)(self->velx * velscale);
	self->vely = (int)(self->vely * velscale);
#endif
	self->velz -= self->velz >> 1;

	// [BC] If we're the server, tell clients to move the object.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
		SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS2 );
		SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
		SERVERCOMMANDS_SetThingGravity( self );
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MaceBallImpact
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MaceBallImpact)
{
	// [BC] Let the server handle this.
	if ( NETWORK_InClientMode() )
	{
		// We need to make sure the ball doesn't temporary go into it's death frame.
		if ( self->flags & MF_INBOUNCE )
			self->SetState (self->SpawnState);

		return;
	}

	if ((self->health != MAGIC_JUNK) && (self->flags & MF_INBOUNCE))
	{ // Bounce
		self->health = MAGIC_JUNK;
		self->velz = (self->velz * 192) >> 8;
		self->BounceFlags = BOUNCE_None;

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_SetThingState( self, STATE_SPAWN );
			SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
			SERVERCOMMANDS_SoundActor( self, CHAN_BODY, "weapons/macebounce", 1, ATTN_NORM );
		}

		self->SetState (self->SpawnState);
		S_Sound (self, CHAN_BODY, "weapons/macebounce", 1, ATTN_NORM);
	}
	else
	{ // Explode
		self->velx = self->vely = self->velz = 0;
		self->flags |= MF_NOGRAVITY;
		self->gravity = FRACUNIT;
		S_Sound (self, CHAN_BODY, "weapons/macehit", 1, ATTN_NORM);

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
			SERVERCOMMANDS_SetThingGravity( self );
			SERVERCOMMANDS_SoundActor( self, CHAN_BODY, "weapons/macebounce", 1, ATTN_NORM );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MaceBallImpact2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MaceBallImpact2)
{
	AActor *tiny;
	angle_t angle;

	// [BC] Let the server handle this.
	if ( NETWORK_InClientMode() )
	{
		// We need to make sure the ball doesn't temporary go into it's death frame.
		if ( self->flags & MF_INBOUNCE )
		{
			fixed_t floordist = self->z - self->floorz;
			fixed_t ceildist = self->ceilingz - self->z;
			fixed_t vel;

			if (floordist <= ceildist)
			{
				vel = MulScale32 (self->velz, self->Sector->floorplane.c);
			}
			else
			{
				vel = MulScale32 (self->velz, self->Sector->ceilingplane.c);
			}
			if (vel >= 2)
				self->SetState (self->SpawnState);
		}

		return;
	}

	if (self->flags & MF_INBOUNCE)
	{
		fixed_t floordist = self->z - self->floorz;
		fixed_t ceildist = self->ceilingz - self->z;
		fixed_t vel;

		if (floordist <= ceildist)
		{
			vel = MulScale32 (self->velz, self->Sector->floorplane.c);
		}
		else
		{
			vel = MulScale32 (self->velz, self->Sector->ceilingplane.c);
		}
		if (vel < 2)
		{
			goto boom;
		}

		// Bounce
		self->velz = (self->velz * 192) >> 8;
		self->SetState (self->SpawnState);

		// [BC] If we're the server, send the state change and move it.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );

		tiny = Spawn("MaceFX3", self->x, self->y, self->z, ALLOW_REPLACE);
		angle = self->angle+ANG90;
		tiny->target = self->target;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->velx = (self->velx>>1) + FixedMul(self->velz-FRACUNIT, finecosine[angle]);
		tiny->vely = (self->vely>>1) + FixedMul(self->velz-FRACUNIT, finesine[angle]);
		tiny->velz = self->velz;

		// [BC] If we're the server, spawn this missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( tiny );

		P_CheckMissileSpawn (tiny, self->radius);

		tiny = Spawn("MaceFX3", self->x, self->y, self->z, ALLOW_REPLACE);
		angle = self->angle-ANG90;
		tiny->target = self->target;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->velx = (self->velx>>1) + FixedMul(self->velz-FRACUNIT, finecosine[angle]);
		tiny->vely = (self->vely>>1) + FixedMul(self->velz-FRACUNIT, finesine[angle]);
		tiny->velz = self->velz;

		// [BC] If we're the server, spawn this missile.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( tiny );

		P_CheckMissileSpawn (tiny, self->radius);
	}
	else
	{ // Explode
boom:
		self->velx = self->vely = self->velz = 0;
		self->flags |= MF_NOGRAVITY;
		self->BounceFlags = BOUNCE_None;
		self->gravity = FRACUNIT;

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingState( self, STATE_DEATH );
			SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_SetThingGravity( self );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireMacePL2)
{
	AActor *mo;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] If we're the client, play the sound and get out.
	if ( NETWORK_InClientMode() )
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/maceshoot", 1, ATTN_NORM );
		return;
	}

	mo = P_SpawnPlayerMissile (self, 0,0,0, RUNTIME_CLASS(AMaceFX4), self->angle, &linetarget);
	if (mo)
	{
		mo->velx += self->velx;
		mo->vely += self->vely;
		mo->velz = 2*FRACUNIT+
			clamp<fixed_t>(finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)], -5*FRACUNIT, 5*FRACUNIT);
		if (linetarget)
		{
			mo->tracer = linetarget;
		}
	}
	S_Sound (self, CHAN_WEAPON, "weapons/maceshoot", 1, ATTN_NORM);

	// [BC] If we're the server, play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_MoveThing( mo, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/maceshoot", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
	}

	if ( player->cheats2 & CF2_SPREAD )
	{
		mo = P_SpawnPlayerMissile (self, 0,0,0, RUNTIME_CLASS(AMaceFX4), self->angle + ( ANGLE_45 / 3 ), &linetarget);
		if (mo)
		{
			mo->velx += self->velx;
			mo->vely += self->vely;
			mo->velz = 2*FRACUNIT+
				clamp<fixed_t>(finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)], -5*FRACUNIT, 5*FRACUNIT);
			if (linetarget)
			{
				mo->tracer = linetarget;
			}

			// [BC] If we're the server, play the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_MoveThing( mo, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
		}

		mo = P_SpawnPlayerMissile (self, 0,0,0, RUNTIME_CLASS(AMaceFX4), self->angle - ( ANGLE_45 / 3 ), &linetarget);
		if (mo)
		{
			mo->velx += self->velx;
			mo->vely += self->vely;
			mo->velz = 2*FRACUNIT+
				clamp<fixed_t>(finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)], -5*FRACUNIT, 5*FRACUNIT);
			if (linetarget)
			{
				mo->tracer = linetarget;
			}

			// [BC] If we're the server, play the sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_MoveThing( mo, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_DeathBallImpact
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_DeathBallImpact)
{
	int i;
	AActor *target;
	angle_t angle = 0;
	bool newAngle;
	AActor *linetarget;

	// [BC] Let the server handle this.
	if ( NETWORK_InClientMode() )
	{
		// We need to make sure the self doesn't temporary go into it's death frame.
		if ( self->flags & MF_INBOUNCE )
		{
			fixed_t floordist = self->z - self->floorz;
			fixed_t ceildist = self->ceilingz - self->z;
			fixed_t vel;

			if (floordist <= ceildist)
			{
				vel = MulScale32 (self->velz, self->Sector->floorplane.c);
			}
			else
			{
				vel = MulScale32 (self->velz, self->Sector->ceilingplane.c);
			}
			if (vel >= 2)
				self->SetState (self->SpawnState);
		}

		return;
	}

	if ((self->z <= self->floorz) && P_HitFloor (self))
	{ // Landed in some sort of liquid
		self->Destroy ();
		return;
	}
	if (self->flags & MF_INBOUNCE)
	{
		fixed_t floordist = self->z - self->floorz;
		fixed_t ceildist = self->ceilingz - self->z;
		fixed_t vel;

		if (floordist <= ceildist)
		{
			vel = MulScale32 (self->velz, self->Sector->floorplane.c);
		}
		else
		{
			vel = MulScale32 (self->velz, self->Sector->ceilingplane.c);
		}
		if (vel < 2)
		{
			goto boom;
		}

		// Bounce
		newAngle = false;
		target = self->tracer;
		if (target)
		{
			if (!(target->flags&MF_SHOOTABLE))
			{ // Target died
				self->tracer = NULL;
			}
			else
			{ // Seek
				angle = R_PointToAngle2(self->x, self->y,
					target->x, target->y);
				newAngle = true;
			}
		}
		else
		{ // Find new target
			angle = 0;
			for (i = 0; i < 16; i++)
			{
				P_AimLineAttack (self, angle, 10*64*FRACUNIT, &linetarget, 0, ALF_NOFRIENDS, NULL, self->target);
				if (linetarget && self->target != linetarget)
				{
					self->tracer = linetarget;
					angle = R_PointToAngle2 (self->x, self->y,
						linetarget->x, linetarget->y);
					newAngle = true;
					break;
				}
				angle += ANGLE_45/2;
			}
		}
		if (newAngle)
		{
			self->angle = angle;
			angle >>= ANGLETOFINESHIFT;
			self->velx = FixedMul (self->Speed, finecosine[angle]);
			self->vely = FixedMul (self->Speed, finesine[angle]);
		}
		self->SetState (self->SpawnState);
		S_Sound (self, CHAN_BODY, "weapons/macestop", 1, ATTN_NORM);

		// [BC] If we're the server, send the state change and move it.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
			SERVERCOMMANDS_SoundActor( self, CHAN_BODY, "weapons/macestop", 1, ATTN_NORM );
		}
	}
	else
	{ // Explode
boom:
		self->velx = self->vely = self->velz = 0;
		self->flags |= MF_NOGRAVITY;
		self->gravity = FRACUNIT;
		S_Sound (self, CHAN_BODY, "weapons/maceexplode", 1, ATTN_NORM);

		// [BC] If we're the server, do some stuff.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingState( self, STATE_DEATH );
			SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ );
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_SetThingGravity( self );
			SERVERCOMMANDS_SoundActor( self, CHAN_BODY, "weapons/maceexplode", 1, ATTN_NORM );
		}
	}
}


// Blaster FX 1 -------------------------------------------------------------

//----------------------------------------------------------------------------
//
// Thinker for the ultra-fast blaster PL2 ripper-spawning missile.
//
//----------------------------------------------------------------------------

class ABlasterFX1 : public AFastProjectile
{
	DECLARE_CLASS(ABlasterFX1, AFastProjectile)
public:
	void Effect ();
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

int ABlasterFX1::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->IsKindOf (PClass::FindClass ("Ironlich")))
	{ // Less damage to Ironlich bosses
		damage = pr_bfx1() & 1;
		if (!damage)
		{
			return -1;
		}
	}
	return damage;
}

void ABlasterFX1::Effect ()
{
	if (pr_bfx1t() < 64)
	{
		Spawn("BlasterSmoke", x, y, MAX<fixed_t> (z - 8 * FRACUNIT, floorz), ALLOW_REPLACE);
	}
}

IMPLEMENT_CLASS(ABlasterFX1)

// Ripper -------------------------------------------------------------------


class ARipper : public AActor
{
	DECLARE_CLASS (ARipper, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS(ARipper)

int ARipper::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->IsKindOf (PClass::FindClass ("Ironlich")))
	{ // Less damage to Ironlich bosses
		damage = pr_ripd() & 1;
		if (!damage)
		{
			return -1;
		}
	}
	return damage;
}

//----------------------------------------------------------------------------
//
// PROC A_FireBlasterPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireBlasterPL1)
{
	angle_t angle;
	int damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	// [BC] If we're the client, just play the sound and get out.
	if ( NETWORK_InClientMode() )
	{
		S_Sound( player->mo, CHAN_WEAPON, "weapons/blastershoot", 1, ATTN_NORM );
		return;
	}

	angle_t pitch = P_BulletSlope(self);
	damage = pr_fb1.HitDice (4);
	angle = self->angle;
	if (player->refire)
	{
		angle += pr_fb1.Random2() << 18;
	}
	P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, "BlasterPuff");

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		P_LineAttack( self, angle + ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, "BlasterPuff");
		P_LineAttack( self, angle - ( ANGLE_45 / 3 ), PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, "BlasterPuff");
	}

	// [BB] If the player hit a player with his attack, potentially give him a medal.
	PLAYER_CheckStruckPlayer ( self );

	S_Sound (self, CHAN_WEAPON, "weapons/blastershoot", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/blastershoot", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//----------------------------------------------------------------------------
//
// PROC A_SpawnRippers
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_SpawnRippers)
{
	unsigned int i;
	angle_t angle;
	AActor *ripper;

	for(i = 0; i < 8; i++)
	{
		ripper = Spawn<ARipper> (self->x, self->y, self->z, ALLOW_REPLACE);
		angle = i*ANG45;
		ripper->target = self->target;
		ripper->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		ripper->velx = FixedMul (ripper->Speed, finecosine[angle]);
		ripper->vely = FixedMul (ripper->Speed, finesine[angle]);
		P_CheckMissileSpawn (ripper, self->radius);
	}
}

// --- Skull rod ------------------------------------------------------------


// Horn Rod FX 2 ------------------------------------------------------------

class AHornRodFX2 : public AActor
{
	DECLARE_CLASS (AHornRodFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AHornRodFX2)

int AHornRodFX2::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->IsKindOf (PClass::FindClass("Sorcerer2")) && pr_hrfx2() < 96)
	{ // D'Sparil teleports away
		P_DSparilTeleport (target);
		return -1;
	}
	return damage;
}

// Rain pillar 1 ------------------------------------------------------------

class ARainPillar : public AActor
{
	DECLARE_CLASS (ARainPillar, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (ARainPillar)

int ARainPillar::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->flags2 & MF2_BOSS)
	{ // Decrease damage for bosses
		damage = (pr_rp() & 7) + 1;
	}
	return damage;
}

// Rain tracker "inventory" item --------------------------------------------

class ARainTracker : public AInventory
{
	DECLARE_CLASS (ARainTracker, AInventory)
public:
	void Serialize (FArchive &arc);
	AActor *Rain1, *Rain2;
};

IMPLEMENT_CLASS (ARainTracker)
	
void ARainTracker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Rain1 << Rain2;
}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireSkullRodPL1)
{
	AActor *mo;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	mo = P_SpawnPlayerMissile (self, PClass::FindClass("HornRodFX1"));
	// Randomize the first frame
	if (mo && pr_fsr1() > 128)
	{
		mo->SetState (mo->state->GetNextState());
	}


	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		mo = P_SpawnPlayerMissile( self, PClass::FindClass("HornRodFX1"), self->angle + ( ANGLE_45 / 3 ));
		// Randomize the first frame
		if (mo && pr_fsr1() > 128)
		{
			mo->SetState (mo->state->GetNextState());
		}

		mo = P_SpawnPlayerMissile( self, PClass::FindClass("HornRodFX1"), self->angle - ( ANGLE_45 / 3 ));
		// Randomize the first frame
		if (mo && pr_fsr1() > 128)
		{
			mo->SetState (mo->state->GetNextState());
		}
	}

}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL2
//
// The special2 field holds the player number that shot the rain missile.
// The special1 field holds the id of the rain sound.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireSkullRodPL2)
{
	player_t *player;
	AActor *MissileActor;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	P_SpawnPlayerMissile (self, 0,0,0, RUNTIME_CLASS(AHornRodFX2), self->angle, &linetarget, &MissileActor);
	// Use MissileActor instead of the return value from
	// P_SpawnPlayerMissile because we need to give info to the mobj
	// even if it exploded immediately.
	if (MissileActor != NULL)
	{
		MissileActor->special2 = (int)(player - players);

		// [BB] Set the special, otherwise the translation of the rain spawned later will be wrong.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingSpecial2( MissileActor );

		if (linetarget)
		{
			MissileActor->tracer = linetarget;
		}
		S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);

		// [BC] If we're the server, play this sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM );
	}

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		P_SpawnPlayerMissile (self, 0,0,0, RUNTIME_CLASS(AHornRodFX2), self->angle + ( ANGLE_45 / 3 ), &linetarget, &MissileActor);
		// Use MissileActor instead of the return value from
		// P_SpawnPlayerMissile because we need to give info to the mobj
		// even if it exploded immediately.
		if (MissileActor != NULL)
		{
			MissileActor->special2 = (int)(player - players);

			// [BB] Set the special, otherwise the translation of the rain spawned later will be wrong.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingSpecial2( MissileActor );

			if (linetarget)
			{
				MissileActor->tracer = linetarget;
			}
			S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);

			// [BC] If we're the server, play this sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM );
		}

		P_SpawnPlayerMissile (self, 0,0,0, RUNTIME_CLASS(AHornRodFX2), self->angle - ( ANGLE_45 / 3 ), &linetarget, &MissileActor);
		// Use MissileActor instead of the return value from
		// P_SpawnPlayerMissile because we need to give info to the mobj
		// even if it exploded immediately.
		if (MissileActor != NULL)
		{
			MissileActor->special2 = (int)(player - players);

			// [BB] Set the special, otherwise the translation of the rain spawned later will be wrong.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingSpecial2( MissileActor );

			if (linetarget)
			{
				MissileActor->tracer = linetarget;
			}
			S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);

			// [BC] If we're the server, play this sound.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_AddPlayerRain
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_AddPlayerRain)
{
	ARainTracker *tracker;

	// [BC] Let the server spawn rain.
	if ( NETWORK_InClientMode() )
		return;

	if (self->target == NULL || self->target->health <= 0)
	{ // Shooter is dead or nonexistant
		return;
	}

	tracker = self->target->FindInventory<ARainTracker> ();

	// They player is only allowed two rainstorms at a time. Shooting more
	// than that will cause the oldest one to terminate.
	if (tracker != NULL)
	{
		if (tracker->Rain1 && tracker->Rain2)
		{ // Terminate an active rain
			if (tracker->Rain1->health < tracker->Rain2->health)
			{
				if (tracker->Rain1->health > 16)
				{
					tracker->Rain1->health = 16;

					// [Dusk] Sync the new health value to clients.
					if ( NETWORK_GetState() == NETSTATE_SERVER )
						SERVERCOMMANDS_SetThingHealth( tracker->Rain1 );
				}
				tracker->Rain1 = NULL;
			}
			else
			{
				if (tracker->Rain2->health > 16)
				{
					tracker->Rain2->health = 16;

					// [Dusk] Sync the new health value to clients.
					if ( NETWORK_GetState() == NETSTATE_SERVER )
						SERVERCOMMANDS_SetThingHealth( tracker->Rain2 );
				}
				tracker->Rain2 = NULL;
			}
		}
	}
	else
	{
		tracker = static_cast<ARainTracker *> (self->target->GiveInventoryType (RUNTIME_CLASS(ARainTracker)));
	}
	// Add rain mobj to list
	if (tracker->Rain1)
	{
		tracker->Rain2 = self;
	}
	else
	{
		tracker->Rain1 = self;
	}
	self->special1 = S_FindSound ("misc/rain");
}

//----------------------------------------------------------------------------
//
// PROC A_SkullRodStorm
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_SkullRodStorm)
{
	fixed_t x;
	fixed_t y;
	AActor *mo;
	ARainTracker *tracker;

	if (self->health-- == 0)
	{
		S_StopSound (self, CHAN_BODY);
		if (self->target == NULL)
		{ // Player left the game
			self->Destroy ();
			return;
		}
		tracker = self->target->FindInventory<ARainTracker> ();
		if (tracker != NULL)
		{
			if (tracker->Rain1 == self)
			{
				tracker->Rain1 = NULL;
			}
			else if (tracker->Rain2 == self)
			{
				tracker->Rain2 = NULL;
			}
		}
		self->Destroy ();
		return;
	}
	if (pr_storm() < 25)
	{ // Fudge rain frequency
		return;
	}
	x = self->x + ((pr_storm()&127) - 64) * FRACUNIT;
	y = self->y + ((pr_storm()&127) - 64) * FRACUNIT;
	mo = Spawn<ARainPillar> (x, y, ONCEILINGZ, ALLOW_REPLACE);
#ifdef _3DFLOORS
	// We used bouncecount to store the 3D floor index in A_HideInCeiling
	if (!mo) return;
	fixed_t newz;
	if (self->bouncecount >= 0 
		&& (unsigned)self->bouncecount < self->Sector->e->XFloor.ffloors.Size())
		newz = self->Sector->e->XFloor.ffloors[self->bouncecount]->bottom.plane->ZatPoint(x, y);// - 40 * FRACUNIT;
	else
		newz = self->Sector->ceilingplane.ZatPoint(x, y);
	int moceiling = P_Find3DFloor(NULL, x, y, newz, false, false, newz);
	if (moceiling >= 0)
		mo->z = newz - mo->height;
#endif
	mo->Translation = ( NETWORK_GetState( ) != NETSTATE_SINGLE ) ?
		TRANSLATION(TRANSLATION_PlayersExtra,self->special2) : 0;
	mo->target = self->target;
	mo->velx = 1; // Force collision detection
	mo->velz = -mo->Speed;
	mo->special2 = self->special2; // Transfer player number
	P_CheckMissileSpawn (mo, self->radius);
	if (self->special1 != -1 && !S_IsActorPlayingSomething (self, CHAN_BODY, -1))
	{
		S_Sound (self, CHAN_BODY|CHAN_LOOP, self->special1, 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_RainImpact
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_RainImpact)
{
	if (self->z > self->floorz)
	{
		self->SetState (self->FindState("NotFloor"));
	}
	else if (pr_impact() < 40)
	{
		P_HitFloor (self);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_HideInCeiling
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_HideInCeiling)
{
#ifdef _3DFLOORS
	// We use bouncecount to store the 3D floor index
	fixed_t foo;
	for (unsigned int i=0; i< self->Sector->e->XFloor.ffloors.Size(); i++)
	{
		F3DFloor * rover = self->Sector->e->XFloor.ffloors[i];
		if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;
		 
		if ((foo = rover->bottom.plane->ZatPoint(self->x, self->y)) >= (self->z + self->height))
		{
			self->z = foo + 4*FRACUNIT;
			self->bouncecount = i;
			return;
		}
	}
	self->bouncecount = -1;
#endif
	self->z = self->ceilingz + 4*FRACUNIT;
}

// --- Phoenix Rod ----------------------------------------------------------

class APhoenixRod : public AWeapon
{
	DECLARE_CLASS (APhoenixRod, AWeapon)
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << FlameCount;
	}
	int FlameCount;		// for flamethrower duration
};

class APhoenixRodPowered : public APhoenixRod
{
	DECLARE_CLASS (APhoenixRodPowered, APhoenixRod)
public:
	void EndPowerup ();
};

IMPLEMENT_CLASS (APhoenixRod)
IMPLEMENT_CLASS (APhoenixRodPowered)

void APhoenixRodPowered::EndPowerup ()
{
	P_SetPsprite (Owner->player, ps_weapon, SisterWeapon->GetReadyState());
	DepleteAmmo (bAltFire);
	Owner->player->refire = 0;
	S_StopSound (Owner, CHAN_WEAPON);
	Owner->player->ReadyWeapon = SisterWeapon;
}

class APhoenixFX1 : public AActor
{
	DECLARE_CLASS (APhoenixFX1, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};


IMPLEMENT_CLASS (APhoenixFX1)

int APhoenixFX1::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->IsKindOf (PClass::FindClass("Sorcerer2")) && pr_hrfx2() < 96)
	{ // D'Sparil teleports away
		P_DSparilTeleport (target);
		return -1;
	}
	return damage;
}

// Phoenix FX 2 -------------------------------------------------------------

class APhoenixFX2 : public AActor
{
	DECLARE_CLASS (APhoenixFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (APhoenixFX2)

int APhoenixFX2::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->player && pr_pfx2 () < 128)
	{ // Freeze player for a bit
		target->reactiontime += 4;
	}
	return damage;
}

//----------------------------------------------------------------------------
//
// PROC A_FirePhoenixPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FirePhoenixPL1)
{
	angle_t angle;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	P_SpawnPlayerMissileWithPossibleSpread (self, RUNTIME_CLASS(APhoenixFX1)); // [BB] Spread
	angle = self->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	self->velx += FixedMul (4*FRACUNIT, finecosine[angle]);
	self->vely += FixedMul (4*FRACUNIT, finesine[angle]);

	// [BC] Push the player back even more if they are using spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		self->velx += FixedMul( 4*FRACUNIT, finecosine[angle] ) * 2;
		self->vely += FixedMul( 4*FRACUNIT, finesine[angle] ) * 2;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_PhoenixPuff
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_PhoenixPuff)
{
	AActor *puff;
	angle_t angle;

	//[RH] Heretic never sets the target for seeking
	//P_SeekerMissile (self, ANGLE_1*5, ANGLE_1*10);
	puff = Spawn("PhoenixPuff", self->x, self->y, self->z, ALLOW_REPLACE);
	angle = self->angle + ANG90;
	angle >>= ANGLETOFINESHIFT;
	puff->velx = FixedMul (FRACUNIT*13/10, finecosine[angle]);
	puff->vely = FixedMul (FRACUNIT*13/10, finesine[angle]);
	puff->velz = 0;
	puff = Spawn("PhoenixPuff", self->x, self->y, self->z, ALLOW_REPLACE);
	angle = self->angle - ANG90;
	angle >>= ANGLETOFINESHIFT;
	puff->velx = FixedMul (FRACUNIT*13/10, finecosine[angle]);
	puff->vely = FixedMul (FRACUNIT*13/10, finesine[angle]);
	puff->velz = 0;
}

//----------------------------------------------------------------------------
//
// PROC A_InitPhoenixPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_InitPhoenixPL2)
{
	if (self->player != NULL)
	{
		APhoenixRod *flamethrower = static_cast<APhoenixRod *> (self->player->ReadyWeapon);
		if (flamethrower != NULL)
		{
			flamethrower->FlameCount = FLAME_THROWER_TICS;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FirePhoenixPL2
//
// Flame thrower effect.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FirePhoenixPL2)
{
	AActor *mo;
	angle_t angle;
	fixed_t x, y, z;

	fixed_t slope;
	FSoundID soundid;
	player_t *player;
	APhoenixRod *flamethrower;

	if (NULL == (player = self->player))
	{
		return;
	}

	soundid = "weapons/phoenixpowshoot";

	flamethrower = static_cast<APhoenixRod *> (player->ReadyWeapon);
	if (flamethrower == NULL || --flamethrower->FlameCount == 0)
	{ // Out of flame
		P_SetPsprite (player, ps_weapon, flamethrower->FindState("Powerdown"));
		player->refire = 0;
		S_StopSound (self, CHAN_WEAPON);
		return;
	}
	// [BC] If we're the client, just play the sound and get out.
	if ( NETWORK_InClientMode() )
	{
		if (!player->refire || !S_IsActorPlayingSomething (player->mo, CHAN_WEAPON, -1))
		{
			S_Sound (player->mo, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
		}
		return;
	}

	angle = self->angle;
	x = self->x + (pr_fp2.Random2() << 9);
	y = self->y + (pr_fp2.Random2() << 9);
	z = self->z + 26*FRACUNIT + finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)];
	z -= self->floorclip;
	slope = finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)] + (FRACUNIT/10);
	mo = Spawn("PhoenixFX2", x, y, z, ALLOW_REPLACE);
	mo->target = self;
	mo->angle = angle;
	mo->velx = self->velx + FixedMul (mo->Speed, finecosine[angle>>ANGLETOFINESHIFT]);
	mo->vely = self->vely + FixedMul (mo->Speed, finesine[angle>>ANGLETOFINESHIFT]);
	mo->velz = FixedMul (mo->Speed, slope);
	if (!player->refire || !S_IsActorPlayingSomething (self, CHAN_WEAPON, -1))
	{
		S_Sound (self, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
	}	

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		angle = self->angle + ( ANGLE_45 / 3 );
		mo = Spawn<APhoenixFX2> (x, y, z, ALLOW_REPLACE);
		mo->target = self;
		mo->angle = angle;
		mo->velx = self->velx + FixedMul (mo->Speed, finecosine[angle>>ANGLETOFINESHIFT]);
		mo->vely = self->vely + FixedMul (mo->Speed, finesine[angle>>ANGLETOFINESHIFT]);
		mo->velz = FixedMul (mo->Speed, slope);
		if (!player->refire || !S_IsActorPlayingSomething (self, CHAN_WEAPON, -1))
		{
			S_Sound (self, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
		}

		angle = self->angle - ( ANGLE_45 / 3 );
		mo = Spawn<APhoenixFX2> (x, y, z, ALLOW_REPLACE);
		mo->target = self;
		mo->angle = angle;
		mo->velx = self->velx + FixedMul (mo->Speed, finecosine[angle>>ANGLETOFINESHIFT]);
		mo->vely = self->vely + FixedMul (mo->Speed, finesine[angle>>ANGLETOFINESHIFT]);
		mo->velz = FixedMul (mo->Speed, slope);
		if (!player->refire || !S_IsActorPlayingSomething (self, CHAN_WEAPON, -1))
		{
			S_Sound (self, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
		}
	}

	// [BC] If we're the server, spawn this missile.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissile( mo );

	P_CheckMissileSpawn (mo, self->radius);
}

//----------------------------------------------------------------------------
//
// PROC A_ShutdownPhoenixPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ShutdownPhoenixPL2)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	S_StopSound (self, CHAN_WEAPON);
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FlameEnd
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FlameEnd)
{
	self->velz += FRACUNIT*3/2;
}

//----------------------------------------------------------------------------
//
// PROC A_FloatPuff
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FloatPuff)
{
	self->velz += FRACUNIT*18/10;
}

