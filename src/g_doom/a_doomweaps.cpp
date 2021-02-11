/*
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gi.h"
#include "templates.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_punch ("Punch");
static FRandom pr_saw ("Saw");
static FRandom pr_fireshotgun2 ("FireSG2");
static FRandom pr_fireplasma ("FirePlasma");
static FRandom pr_firerail ("FireRail");
static FRandom pr_bfgspray ("BFGSpray");
static FRandom pr_oldbfg ("OldBFG");

//
// A_Punch
//
DEFINE_ACTION_FUNCTION(AActor, A_Punch)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	AActor		*linetarget;

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL && !(weapon->WeaponFlags & WIF_DEHAMMO))
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
	}

	damage = (pr_punch()%10+1)<<1;

	if (self->FindInventory<APowerStrength>())	
		damage *= 10;

	angle = self->angle;

	angle += pr_punch.Random2() << 18;
	pitch = P_AimLineAttack (self, angle, MELEERANGE, &linetarget);

	P_LineAttack (self, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, LAF_ISMELEEATTACK, &linetarget);

	// [BC] Apply spread.
	if (( self->player ) && ( self->player->cheats2 & CF2_SPREAD ))
	{
		P_LineAttack( self, angle + ( ANGLE_45 / 3 ), MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, LAF_ISMELEEATTACK);
		P_LineAttack( self, angle - ( ANGLE_45 / 3 ), MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, LAF_ISMELEEATTACK);
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( self->player )
	{
		if ( self->player->bStruckPlayer )
			PLAYER_StruckPlayer( self->player );
		else
			self->player->ulConsecutiveHits = 0;

		// Tell all the bots that a weapon was fired.
		BOTS_PostWeaponFiredEvent( ULONG( self->player - players ), BOTEVENT_USEDFIST, BOTEVENT_ENEMY_USEDFIST, BOTEVENT_PLAYER_USEDFIST );
	}

	// turn to face target
	if (linetarget)
	{
		S_Sound (self, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		self->angle = R_PointToAngle2 (self->x,
										self->y,
										linetarget->x,
										linetarget->y);

		// [BC] Play the hit sound to clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "*fist", 1, ATTN_NORM );
			SERVERCOMMANDS_SetThingAngle( self );
		}
	}
}

//
// A_FirePistol
//
void A_CustomFireBullets( AActor *self,
						  angle_t Spread_XY,
						  angle_t Spread_Z, 
						  int NumberOfBullets,
						  int DamagePerBullet,
						  const PClass * PuffType,
						  const char *AttackSound = NULL,
						  int Flags = 1,
						  fixed_t Range = 0,
						  const bool pPlayAttacking = true );

DEFINE_ACTION_FUNCTION(AActor, A_FirePistol)
{
	// [BB] A_FirePistol is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 5.6 * ANGLE_1), angle_t( 0 * ANGLE_1), 1, 5, PClass::FindClass("BulletPuff"), "weapons/pistol", true, 0, false );
	A_GunFlash ( self );
/*
	bool accurate;

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
				return;

			P_SetPsprite (self->player, ps_flash, weapon->FindState(NAME_Flash));
			self->player->psprites[ps_flash].processPending = true;
		}
		self->player->mo->PlayAttacking2 ();

		// [BC] If we're the server, tell clients to update this player's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetPlayerState( ULONG( self->player - players ), STATE_PLAYER_ATTACK2, ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );

		accurate = !self->player->refire;
	}
	else
	{
		accurate = true;
	}

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( actor->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/pistol", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );

	S_Sound (self, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);


	// [BC] Weapons are handled by the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	P_GunShot (self, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));

	// [BC] Apply spread.
	if (( actor->player ) && ( actor->player->cheats2 & CF2_SPREAD ))
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = actor->angle;
		actor->angle += ( ANGLE_45 / 3 );
		P_GunShot (actor, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (actor));
		actor->angle = SavedActorAngle;

		SavedActorAngle = actor->angle;
		actor->angle -= ( ANGLE_45 / 3 );
		P_GunShot (actor, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (actor));
		actor->angle = SavedActorAngle;
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( actor->player )
	{
		if ( actor->player->bStruckPlayer )
			PLAYER_StruckPlayer( actor->player );
		else
			actor->player->ulConsecutiveHits = 0;

		// Tell all the bots that a weapon was fired.
		BOTS_PostWeaponFiredEvent( ULONG( actor->player - players ), BOTEVENT_FIREDPISTOL, BOTEVENT_ENEMY_FIREDPISTOL, BOTEVENT_PLAYER_FIREDPISTOL );
	}
*/
}

//
// A_Saw
//
enum SAW_Flags
{
	SF_NORANDOM = 1,
	SF_RANDOMLIGHTMISS = 2,
	SF_RANDOMLIGHTHIT = 4,
	SF_NOUSEAMMOMISS = 8,
	SF_NOUSEAMMO = 16,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Saw)
{
	angle_t angle;
	angle_t slope;
	player_t *player;
	AActor *linetarget;
	int actualdamage;

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}
	
	ACTION_PARAM_START(9);
	ACTION_PARAM_SOUND(fullsound, 0);
	ACTION_PARAM_SOUND(hitsound, 1);
	ACTION_PARAM_INT(damage, 2);
	ACTION_PARAM_CLASS(pufftype, 3);
	ACTION_PARAM_INT(Flags, 4);
	ACTION_PARAM_FIXED(Range, 5);
	ACTION_PARAM_ANGLE(Spread_XY, 6);
	ACTION_PARAM_ANGLE(Spread_Z, 7);
	ACTION_PARAM_FIXED(LifeSteal, 8);



	if (NULL == (player = self->player))
	{
		return;
	}

	if (pufftype == NULL) pufftype = PClass::FindClass(NAME_BulletPuff);
	if (damage == 0) damage = 2;

	if (!(Flags & SF_NORANDOM))
		damage *= (pr_saw()%10+1);
	
	// use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
	if (Range == 0) Range = MELEERANGE+1;

	angle = self->angle + (pr_saw.Random2() * (Spread_XY / 255));
	slope = P_AimLineAttack (self, angle, Range, &linetarget) + (pr_saw.Random2() * (Spread_Z / 255));


	AWeapon *weapon = self->player->ReadyWeapon;
	if ((weapon != NULL) && !(Flags & SF_NOUSEAMMO) && !(!linetarget && (Flags & SF_NOUSEAMMOMISS)) && !(weapon->WeaponFlags & WIF_DEHAMMO))
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	P_LineAttack (self, angle, Range, slope, damage, NAME_Melee, pufftype, false, &linetarget, &actualdamage);

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		P_LineAttack( self, angle + ( ANGLE_45 / 3 ), Range,
					  P_AimLineAttack( self, angle + ( ANGLE_45 / 3 ), Range, &linetarget ), damage,
					  NAME_Melee, pufftype, false );

		P_LineAttack( self, angle - ( ANGLE_45 / 3 ), Range,
					  P_AimLineAttack( self, angle - ( ANGLE_45 / 3 ), Range, &linetarget ), damage,
					  NAME_Melee, pufftype, false );
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( self->player->bStruckPlayer )
		PLAYER_StruckPlayer( self->player );
	else
		self->player->ulConsecutiveHits = 0;

	// [BC] If we're the server, tell clients that a weapon is being fired.
//	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
//		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), linetarget ? hitsound : fullsound, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	if (!linetarget)
	{
		if ((Flags & SF_RANDOMLIGHTMISS) && (pr_saw() > 64))
		{
			player->extralight = !player->extralight;
		}
		S_Sound (self, CHAN_WEAPON, fullsound, 1, ATTN_NORM);

		// [BC] If we're the server, tell clients to play the saw sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( fullsound ), 1, ATTN_NORM );
		return;
	}

	if (Flags & SF_RANDOMLIGHTHIT)
	{
		int randVal = pr_saw();
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
	}

	// [EP] Is the actor's health changed by the life steal?
	const int prevhealth = self->health;

	if (LifeSteal && !(linetarget->flags5 & MF5_DONTDRAIN))
		P_GiveBody (self, (actualdamage * LifeSteal) >> FRACBITS);

	// [EP] Inform the clients about the player health change if needed.
	if ( ( NETWORK_GetState() == NETSTATE_SERVER ) && self->player && prevhealth != self->health )
		SERVERCOMMANDS_SetPlayerHealth( self->player - players );

	S_Sound (self, CHAN_WEAPON, hitsound, 1, ATTN_NORM);
	// [BC] If we're the server, tell clients to play the saw sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, S_GetName( hitsound ), 1, ATTN_NORM );
		
	// turn to face target
	angle = R_PointToAngle2 (self->x, self->y,
							 linetarget->x, linetarget->y);
	if (angle - self->angle > ANG180)
	{
		if (angle - self->angle < (angle_t)(-ANG90/20))
			self->angle = angle + ANG90/21;
		else
			self->angle -= ANG90/20;
	}
	else
	{
		if (angle - self->angle > ANG90/20)
			self->angle = angle - ANG90/21;
		else
			self->angle += ANG90/20;
	}
	self->flags |= MF_JUSTATTACKED;

	// [BC] If we're the server, tell clients to adjust the player's angle.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetThingAngle( self );

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( self->player - players ), BOTEVENT_USEDCHAINSAW, BOTEVENT_ENEMY_USEDCHAINSAW, BOTEVENT_PLAYER_USEDCHAINSAW );
}

//
// A_FireShotgun
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun)
{
	// [BB] A_FireShotgun is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 5.6 * ANGLE_1), angle_t( 0 * ANGLE_1), 7, 5, PClass::FindClass("BulletPuff"), "weapons/shotgf", true, 0, false );
	A_GunFlash ( self );
/*
	int i;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/shotgf", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	S_Sound (self, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
		self->player->psprites[ps_flash].processPending = true;
	}
	player->mo->PlayAttacking2 ();

	angle_t pitch = P_BulletSlope (self);

	// [BC] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BC] Weapons are handled by the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;


	for (i=0 ; i<7 ; i++)
		P_GunShot (self, false, PClass::FindClass(NAME_BulletPuff), pitch);

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = self->angle;
		self->angle += ( ANGLE_45 / 3 );
		for (i=0 ; i<7 ; i++)
			P_GunShot (self, false, PClass::FindClass(NAME_BulletPuff), pitch);
		self->angle = SavedActorAngle;

		SavedActorAngle = actor->angle;
		self->angle -= ( ANGLE_45 / 3 );
		for (i=0 ; i<7 ; i++)
			P_GunShot (self, false, PClass::FindClass(NAME_BulletPuff), pitch);
		self->angle = SavedActorAngle;
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( player->bStruckPlayer )
		PLAYER_StruckPlayer( player );
	else
		player->ulConsecutiveHits = 0;

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDSHOTGUN, BOTEVENT_ENEMY_FIREDSHOTGUN, BOTEVENT_PLAYER_FIREDSHOTGUN );
*/
}

//
// A_FireShotgun2
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun2)
{
	// [BB] A_FireShotgun2 is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 11.2 * ANGLE_1), angle_t( 7.1 * ANGLE_1), 20, 5, PClass::FindClass("BulletPuff"), "weapons/sshotf", true, 0, false );
	A_GunFlash ( self );
/*
	int 		i;
	angle_t 	angle;
	int 		damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/sshotf", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	S_Sound (self, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 2))
			return;
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
		self->player->psprites[ps_flash].processPending = true;
	}
	player->mo->PlayAttacking2 ();

	// [BC] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BC] Weapons are handled by the server.
	// [BB] To make hitscan decals kinda work online, we may not stop here yet.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT && !cl_hitscandecalhack )
		return;

	angle_t pitch = P_BulletSlope (self);
		
	for (i=0 ; i<20 ; i++)
	{
		damage = 5*(pr_fireshotgun2()%3+1);
		angle = self->angle;
		angle += pr_fireshotgun2.Random2() << 19;

		// Doom adjusts the bullet slope by shifting a random number [-255,255]
		// left 5 places. At 2048 units away, this means the vertical position
		// of the shot can deviate as much as 255 units from nominal. So using
		// some simple trigonometry, that means the vertical angle of the shot
		// can deviate by as many as ~7.097 degrees or ~84676099 BAMs.

		P_LineAttack (self,
					  angle,
					  PLAYERMISSILERANGE,
					  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
					  NAME_Hitscan, NAME_BulletPuff);

		// [BC] Apply spread.
		if ( player->cheats2 & CF2_SPREAD )
		{
			P_LineAttack (actor,
						  angle + ( ANGLE_45 / 3 ),
						  PLAYERMISSILERANGE,
						  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
						  NAME_Hitscan, NAME_BulletPuff);

			P_LineAttack (actor,
						  angle - ( ANGLE_45 / 3 ),
						  PLAYERMISSILERANGE,
						  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
						  NAME_Hitscan, NAME_BulletPuff);
		}
	}

	// [BB] Even with the online hitscan decal hack, a client has to stop here.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( player->bStruckPlayer )
		PLAYER_StruckPlayer( player );
	else
		player->ulConsecutiveHits = 0;

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDSSG, BOTEVENT_ENEMY_FIREDSSG, BOTEVENT_PLAYER_FIREDSSG );
*/
}

DEFINE_ACTION_FUNCTION(AActor, A_OpenShotgun2)
{
	// [BB] Clients only do this for "their" player.
	if ( NETWORK_IsConsolePlayerOrSpiedByConsolePlayerOrNotInClientMode( self->player ) )
		S_Sound (self, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( self->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/sshoto", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
}

DEFINE_ACTION_FUNCTION(AActor, A_LoadShotgun2)
{
	// [BB] Clients only do this for "their" player.
	if ( NETWORK_IsConsolePlayerOrSpiedByConsolePlayerOrNotInClientMode( self->player ) )
		S_Sound (self, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( self->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/sshotl", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
}

DEFINE_ACTION_FUNCTION(AActor, A_CloseShotgun2)
{
	// [BB] Clients only do this for "their" player.
	if ( NETWORK_IsConsolePlayerOrSpiedByConsolePlayerOrNotInClientMode( self->player ) )
		S_Sound (self, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
	A_ReFire (self);

	// [BC] If we're the server, tell clients that a weapon is being fired.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( self->player ))
		SERVERCOMMANDS_WeaponSound( ULONG( self->player - players ), "weapons/sshotc", ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
}

//------------------------------------------------------------------------------------
//
// Setting a random flash like some of Doom's weapons can easily crash when the
// definition is overridden incorrectly so let's check that the state actually exists.
// Be aware though that this will not catch all DEHACKED related problems. But it will
// find all DECORATE related ones.
//
//------------------------------------------------------------------------------------

void P_SetSafeFlash(AWeapon *weapon, player_t *player, FState *flashstate, int index)
{

	const PClass * cls = weapon->GetClass();
	while (cls != RUNTIME_CLASS(AWeapon))
	{
		FActorInfo * info = cls->ActorInfo;
		if (flashstate >= info->OwnedStates && flashstate < info->OwnedStates + info->NumOwnedStates)
		{
			// The flash state belongs to this class.
			// Now let's check if the actually wanted state does also
			if (flashstate+index < info->OwnedStates + info->NumOwnedStates)
			{
				// we're ok so set the state
				P_SetPsprite (player, ps_flash, flashstate + index);
				player->psprites[ps_flash].processPending = true;
				return;
			}
			else
			{
				// oh, no! The state is beyond the end of the state table so use the original flash state.
				P_SetPsprite (player, ps_flash, flashstate);
				player->psprites[ps_flash].processPending = true;
				return;
			}
		}
		// try again with parent class
		cls = cls->ParentClass;
	}
	// if we get here the state doesn't seem to belong to any class in the inheritance chain
	// This can happen with Dehacked if the flash states are remapped. 
	// The only way to check this would be to go through all Dehacked modifiable actors, convert
	// their states into a single flat array and find the correct one.
	// Rather than that, just check to make sure it belongs to something.
	if (FState::StaticFindStateOwner(flashstate + index) == NULL)
	{ // Invalid state. With no index offset, it should at least be valid.
		index = 0;
	}
	P_SetPsprite (player, ps_flash, flashstate + index);
	player->psprites[ps_flash].processPending = true;
}

//
// A_FireCGun
//
DEFINE_ACTION_FUNCTION(AActor, A_FireCGun)
{
	// [BC] I guess we have to do all this old stuff to ensure that the flash state on the
	// chaingun ends up on the right frame. Shouldn't it just work?
	player_t *player = self->player;

	if (NULL == player)
	{
		return;
	}
	player->mo->PlayAttacking2 ();

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			FState * atk = weapon->FindState(NAME_Fire);

			int theflash = clamp (int(player->psprites[ps_weapon].state - atk), 0, 1);

			if (flash[theflash].sprite != flash->sprite)
			{
				theflash = 0;
			}
			P_SetSafeFlash (weapon, player, flash, theflash);
		}
	}
	// [BB] A_FireCGun is only kept to stay compatible with Dehacked.
	A_CustomFireBullets( self, angle_t( 5.6 * ANGLE_1), angle_t( 0 * ANGLE_1), 1, 5, PClass::FindClass("BulletPuff"), "weapons/chngun" );
//	A_GunFlash( self );
/*
	player_t *player;

	if (self == NULL || NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;
		
		S_Sound (self, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);

		// [BC] If we're the server, tell clients that a weapon is being fired.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "weapons/chngun", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			// [RH] Fix for Sparky's messed-up Dehacked patch! Blargh!
			FState * atk = weapon->FindState(NAME_Fire);

			int theflash = clamp (int(player->psprites[ps_weapon].state - atk), 0, 1);

			if (flash[theflash].sprite != flash->sprite)
			{
				theflash = 0;
			}

			P_SetSafeFlash (weapon, player, flash, theflash);
		}

	}
	player->mo->PlayAttacking2 ();

	// [BC] If we're the server, tell clients to update this player's state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetPlayerState( ULONG( player - players ), STATE_PLAYER_ATTACK2, ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	// [BC] Weapons are handled by the server.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return;

	P_GunShot (self, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));

	// [BC] Apply apread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		fixed_t		SavedActorAngle;

		SavedActorAngle = actor->angle;
		self->angle += ( ANGLE_45 / 3 );
		P_GunShot (self, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));
		self->angle = SavedActorAngle;
	
		SavedActorAngle = self->angle;
		self->angle -= ( ANGLE_45 / 3 );
		P_GunShot (self, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));
		self->angle = SavedActorAngle;
	}

	// [BC] If the player hit a player with his attack, potentially give him a medal.
	if ( player->bStruckPlayer )
		PLAYER_StruckPlayer( player );
	else
		player->ulConsecutiveHits = 0;

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDCHAINGUN, BOTEVENT_ENEMY_FIREDCHAINGUN, BOTEVENT_PLAYER_FIREDCHAINGUN );
*/
}

//
// A_FireMissile
//
DEFINE_ACTION_FUNCTION(AActor, A_FireMissile)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("Rocket")); // [BB] Spread

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDROCKET, BOTEVENT_ENEMY_FIREDROCKET, BOTEVENT_PLAYER_FIREDROCKET );
}

// [BC]
class AGrenade : public AActor
{
	DECLARE_CLASS (AGrenade, AActor)
public:
	bool	FloorBounceMissile( secplane_t &plane );
};

IMPLEMENT_CLASS ( AGrenade )

bool AGrenade::FloorBounceMissile( secplane_t &plane )
{
	fixed_t bouncevelx = velx / 4;
	fixed_t bouncevely = vely / 4;
	fixed_t bouncevelz = FixedMul (velz, (fixed_t)(-0.6*FRACUNIT));
/*
	if (abs (bouncevelz) < (FRACUNIT/2))
	{
		P_ExplodeMissile( this, NULL );
	}
	else
	{
*/
		if (!Super::FloorBounceMissile (plane))
		{
			velx = bouncevelx;
			vely = bouncevely;
			velz = bouncevelz;
			return false;
		}
//	}
	
	return true;
}

//
// A_FireSTGrenade: not exactly backported from ST, but should work the same
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireSTGrenade)
{
	player_t *player;
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(grenade, 0);
	if (grenade == NULL) return;

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
		
	// Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Temporarily raise the pitch to send the grenade slightly upwards
	fixed_t SavedPlayerPitch = self->pitch;
	self->pitch -= (1152 << FRACBITS);
	P_SpawnPlayerMissileWithPossibleSpread(self, grenade); // [BB] Spread
	self->pitch = SavedPlayerPitch;

	// Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDGRENADE, BOTEVENT_ENEMY_FIREDGRENADE, BOTEVENT_PLAYER_FIREDGRENADE );
}

//
// A_FirePlasma
//
DEFINE_ACTION_FUNCTION(AActor, A_FirePlasma)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_fireplasma()&1));
		}
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	P_SpawnPlayerMissileWithPossibleSpread (self, PClass::FindClass("PlasmaBall")); // [BB] Spread

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDPLASMA, BOTEVENT_ENEMY_FIREDPLASMA, BOTEVENT_PLAYER_FIREDPLASMA );
}

//
// [RH] A_FireRailgun
// [TP] Now takes a puff class
//
static void FireRailgun(AActor *self, int offset_xy, const PClass* puffType = NULL )
{
	int damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_firerail()&1));
		}
	}

	// Weapons are handled by the server.
	// [Spleen] But railgun is an exception if it's unlagged, to make it look nicer
	if ( NETWORK_InClientMode()
		&& !UNLAGGED_DrawRailClientside( self ) )
	{
		return;
	}

	damage = 200;
	if ( deathmatch || teamgame )
	{
		if ( instagib )
			damage = 1000;
		else
			damage = 75;
	}

	// [BB] This also handles color and spread.
	// [TP] Now takes a puff too.
	P_RailAttackWithPossibleSpread (self, damage, offset_xy, 0, 0, 0, 0, 0, puffType );

	// [BC] Tell all the bots that a weapon was fired.
	BOTS_PostWeaponFiredEvent( ULONG( player - players ), BOTEVENT_FIREDRAILGUN, BOTEVENT_ENEMY_FIREDRAILGUN, BOTEVENT_PLAYER_FIREDRAILGUN );
}

// [TP] This now takes a puff type to retain Skulltag's railgun's ability to pierce armor.
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireRailgun)
{
	ACTION_PARAM_START( 1 )
	ACTION_PARAM_CLASS( puffType, 0 );
	FireRailgun(self, 0, puffType );
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunRight)
{
	FireRailgun(self, 10);
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunLeft)
{
	FireRailgun(self, -10);
}

DEFINE_ACTION_FUNCTION(AActor, A_RailWait)
{
	// Okay, this was stupid. Just use a NULL function instead of this.
}

DEFINE_ACTION_FUNCTION(AActor, A_CheckRailReload)
{
	if ( self->player == NULL )
		return;

	self->player->ulRailgunShots++;
	// If we have not made our 4th shot...
	if ((( self->player->ulRailgunShots % 4 ) == 0 ) == false )
	{
		// Go back to the refire frames, instead of continuing on to the reload frames.
		if ( self->player->ReadyWeapon->GetClass( ) == PClass::FindClass("Railgun" ))
			P_SetPsprite( self->player, ps_weapon, self->player->ReadyWeapon->FindState(NAME_Fire) + 8 );
	}
	else
		// We need to reload. However, don't reload if we're out of ammo.
		self->player->ReadyWeapon->CheckAmmo( false, false );
}

//
// A_FireBFG
//

DEFINE_ACTION_FUNCTION(AActor, A_FireBFG)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, deh.BFGCells))
			return;
	}

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}


	P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindClass("BFGBall"), self->angle, NULL, NULL, !(dmflags2 & DF2_YES_FREEAIMBFG));

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindClass("BFGBall"), self->angle + ( ANGLE_45 / 3 ), NULL, NULL, !(dmflags2 & DF2_YES_FREEAIMBFG), false );
		P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindClass("BFGBall"), self->angle - ( ANGLE_45 / 3 ), NULL, NULL, !(dmflags2 & DF2_YES_FREEAIMBFG), false );
	}

}

//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BFGSpray)
{
	int 				i;
	int 				j;
	int 				damage;
	angle_t 			an;
	AActor				*thingToHit;
	AActor				*linetarget;

	// [BC] This is not done on the client end.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(spraytype, 0);
	ACTION_PARAM_INT(numrays, 1);
	ACTION_PARAM_INT(damagecnt, 2);

	if (spraytype == NULL) spraytype = PClass::FindClass("BFGExtra");
	if (numrays <= 0) numrays = 40;
	if (damagecnt <= 0) damagecnt = 15;

	// [RH] Don't crash if no target
	if (!self->target)
		return;

	// offset angles from its attack angle
	for (i = 0; i < numrays; i++)
	{
		an = self->angle - ANG90/2 + ANG90/numrays*i;

		// self->target is the originator (player) of the missile
		P_AimLineAttack (self->target, an, 16*64*FRACUNIT, &linetarget, ANGLE_1*32);

		if (!linetarget)
			continue;

		AActor *spray = Spawn (spraytype, linetarget->x, linetarget->y,
			linetarget->z + (linetarget->height>>2), ALLOW_REPLACE);

		if (spray && (spray->flags5 & MF5_PUFFGETSOWNER))
			spray->target = self->target;
		
		// [BC] Tell clients to spawn the tracers.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( spray ))
			SERVERCOMMANDS_SpawnThing( spray );

		damage = 0;
		for (j = 0; j < damagecnt; ++j)
			damage += (pr_bfgspray() & 7) + 1;

		thingToHit = linetarget;
		int newdam = P_DamageMobj (thingToHit, self->target, self->target, damage, spray != NULL? FName(spray->DamageType) : FName(NAME_BFGSplash), 
			spray != NULL && (spray->flags3 & MF3_FOILINVUL)? DMG_FOILINVUL : 0);
		P_TraceBleed (newdam > 0 ? newdam : damage, thingToHit, self->target);
	}
}

//
// A_BFGsound
//
DEFINE_ACTION_FUNCTION(AActor, A_BFGsound)
{
	// [BB] Clients only do this for "their" player.
	if ( NETWORK_IsConsolePlayerOrSpiedByConsolePlayerOrNotInClientMode( self->player ) )
		S_Sound (self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM);

	// [BC] Tell the clients to trigger the BFG firing sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( self->player )
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM, ULONG( self->player - players ), SVCF_SKIPTHISCLIENT );
		else
			SERVERCOMMANDS_SoundActor( self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM );
	}

	if ( self->player )
	{
		// Tell all the bots that a weapon was fired.
		BOTS_PostWeaponFiredEvent( ULONG( self->player - players ), BOTEVENT_FIREDBFG, BOTEVENT_ENEMY_FIREDBFG, BOTEVENT_PLAYER_FIREDBFG );
	}
}

//
// A_FireOldBFG
//
// This function emulates Doom's Pre-Beta BFG
// By Lee Killough 6/6/98, 7/11/98, 7/19/98, 8/20/98
//
// This code may not be used in other mods without appropriate credit given.
// Code leeches will be telefragged.

DEFINE_ACTION_FUNCTION(AActor, A_FireOldBFG)
{
	const PClass * plasma[] = {PClass::FindClass("PlasmaBall1"), PClass::FindClass("PlasmaBall2")};
	AActor * mo = NULL;

	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;
	}
	self->player->extralight = 2;

	// Save values temporarily
	angle_t SavedPlayerAngle = self->angle;
	fixed_t SavedPlayerPitch = self->pitch;
	bool doesautoaim = !(self->player->ReadyWeapon->WeaponFlags & WIF_NOAUTOAIM);
	self->player->ReadyWeapon->WeaponFlags |= WIF_NOAUTOAIM; // No autoaiming that gun
	for (int i = 0; i < 2; i++) // Spawn two plasma balls in sequence
    {
		self->angle += ((pr_oldbfg()&127) - 64) * (ANG90/768);
		self->pitch += ((pr_oldbfg()&127) - 64) * (ANG90/640);
		mo = P_SpawnPlayerMissile (self, plasma[i]);
		// Restore saved values
		self->angle = SavedPlayerAngle;
		self->pitch = SavedPlayerPitch;
    }
	if (doesautoaim) self->player->ReadyWeapon->WeaponFlags &= ~WIF_NOAUTOAIM; // Restore autoaim setting
}
