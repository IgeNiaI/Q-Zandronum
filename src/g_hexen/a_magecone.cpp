/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

const int SHARDSPAWN_LEFT	= 1;
const int SHARDSPAWN_RIGHT	= 2;
const int SHARDSPAWN_UP		= 4;
const int SHARDSPAWN_DOWN	= 8;

static FRandom pr_cone ("FireConePL1");

void A_FireConePL1 (AActor *actor);
void A_ShedShard (AActor *);

// Frost Missile ------------------------------------------------------------

class AFrostMissile : public AActor
{
	DECLARE_CLASS (AFrostMissile, AActor)
public:
	int DoSpecialDamage (AActor *victim, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AFrostMissile)

int AFrostMissile::DoSpecialDamage (AActor *victim, int damage, FName damagetype)
{
	if (special2 > 0)
	{
		damage <<= special2;
	}
	return damage;
}

//============================================================================
//
// A_FireConePL1
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireConePL1)
{
	angle_t angle;
	int damage;
	int slope;
	int i;
	AActor *mo;
	bool conedone=false;
	player_t *player;
	AActor *linetarget;

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
	S_Sound (self, CHAN_WEAPON, "MageShardsFire", 1, ATTN_NORM);

	// [BC] Weapons are handled by the server.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// [BC] If we're the server, play the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "MageShardsFire", ULONG( player - players ), SVCF_SKIPTHISCLIENT );

	damage = 90+(pr_cone()&15);
	for (i = 0; i < 16; i++)
	{
		angle = self->angle+i*(ANG45/16);
		slope = P_AimLineAttack (self, angle, MELEERANGE, &linetarget, 0, ALF_CHECK3D);
		if (linetarget)
		{
			P_DamageMobj (linetarget, self, self, damage, NAME_Ice);

			// [BC] Apply spread.
			if ( player->cheats2 & CF2_SPREAD )
				P_DamageMobj (linetarget, self, self, damage * 2, NAME_Ice);

			conedone = true;
			break;
		}
	}

	// didn't find any creatures, so fire projectiles
	if (!conedone)
	{
		mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(AFrostMissile));
		if (mo)
		{
			mo->special1 = SHARDSPAWN_LEFT|SHARDSPAWN_DOWN|SHARDSPAWN_UP
				|SHARDSPAWN_RIGHT;
			mo->special2 = 3; // Set sperm count (levels of reproductivity)
			mo->target = self;
			mo->args[0] = 3;		// Mark Initial shard as super damage
		}

		// [BC] Apply spread.
		if ( player->cheats2 & CF2_SPREAD )
		{
			mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(AFrostMissile), self->angle + ( ANGLE_45 / 3 ));
			if (mo)
			{
				mo->special1 = SHARDSPAWN_LEFT|SHARDSPAWN_DOWN|SHARDSPAWN_UP
					|SHARDSPAWN_RIGHT;
				mo->special2 = 3; // Set sperm count (levels of reproductivity)
				mo->target = self;
				mo->args[0] = 3;		// Mark Initial shard as super damage
			}

			mo = P_SpawnPlayerMissile (self, RUNTIME_CLASS(AFrostMissile), self->angle - ( ANGLE_45 / 3 ));
			if (mo)
			{
				mo->special1 = SHARDSPAWN_LEFT|SHARDSPAWN_DOWN|SHARDSPAWN_UP
					|SHARDSPAWN_RIGHT;
				mo->special2 = 3; // Set sperm count (levels of reproductivity)
				mo->target = self;
				mo->args[0] = 3;		// Mark Initial shard as super damage
			}
		}
	}
}

//============================================================================
//
// A_ShedShard
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ShedShard)
{
	AActor *mo;
	int spawndir = self->special1;
	int spermcount = self->special2;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (spermcount <= 0) return;				// No sperm left
	self->special2 = 0;
	spermcount--;

	// every so many calls, spawn a new missile in its set directions
	if (spawndir & SHARDSPAWN_LEFT)
	{
		mo = P_SpawnMissileAngleZSpeed (self, self->z, RUNTIME_CLASS(AFrostMissile), self->angle+(ANG45/9),
											 0, (20+2*spermcount)<<FRACBITS, self->target);
		if (mo)
		{
			mo->special1 = SHARDSPAWN_LEFT;
			mo->special2 = spermcount;
			mo->velz = self->velz;
			mo->args[0] = (spermcount==3)?2:0;

			// [BC] Tell clients to spawn the shard.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnMissile( mo );
		}
	}
	if (spawndir & SHARDSPAWN_RIGHT)
	{
		mo = P_SpawnMissileAngleZSpeed (self, self->z, RUNTIME_CLASS(AFrostMissile), self->angle-(ANG45/9),
											 0, (20+2*spermcount)<<FRACBITS, self->target);
		if (mo)
		{
			mo->special1 = SHARDSPAWN_RIGHT;
			mo->special2 = spermcount;
			mo->velz = self->velz;
			mo->args[0] = (spermcount==3)?2:0;

			// [BC] Tell clients to spawn the shard.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnMissile( mo );
		}
	}
	if (spawndir & SHARDSPAWN_UP)
	{
		mo = P_SpawnMissileAngleZSpeed (self, self->z+8*FRACUNIT, RUNTIME_CLASS(AFrostMissile), self->angle, 
											 0, (15+2*spermcount)<<FRACBITS, self->target);
		if (mo)
		{
			mo->velz = self->velz;
			if (spermcount & 1)			// Every other reproduction
				mo->special1 = SHARDSPAWN_UP | SHARDSPAWN_LEFT | SHARDSPAWN_RIGHT;
			else
				mo->special1 = SHARDSPAWN_UP;
			mo->special2 = spermcount;
			mo->args[0] = (spermcount==3)?2:0;

			// [BC] Tell clients to spawn the shard.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnMissile( mo );
		}
	}
	if (spawndir & SHARDSPAWN_DOWN)
	{
		mo = P_SpawnMissileAngleZSpeed (self, self->z-4*FRACUNIT, RUNTIME_CLASS(AFrostMissile), self->angle, 
											 0, (15+2*spermcount)<<FRACBITS, self->target);
		if (mo)
		{
			mo->velz = self->velz;
			if (spermcount & 1)			// Every other reproduction
				mo->special1 = SHARDSPAWN_DOWN | SHARDSPAWN_LEFT | SHARDSPAWN_RIGHT;
			else
				mo->special1 = SHARDSPAWN_DOWN;
			mo->special2 = spermcount;
			mo->target = self->target;
			mo->args[0] = (spermcount==3)?2:0;

			// [BC] Tell clients to spawn the shard.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnMissile( mo );
		}
	}
}
