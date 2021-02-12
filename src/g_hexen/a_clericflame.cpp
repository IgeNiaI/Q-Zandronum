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

const fixed_t FLAMESPEED	= fixed_t(0.45*FRACUNIT);
const fixed_t CFLAMERANGE	= 12*64*FRACUNIT;
const fixed_t FLAMEROTSPEED	= 2*FRACUNIT;

static FRandom pr_missile ("CFlameMissile");

void A_CFlameAttack (AActor *);
void A_CFlameRotate (AActor *);
void A_CFlamePuff (AActor *);
void A_CFlameMissile (AActor *);

// Flame Missile ------------------------------------------------------------

class ACFlameMissile : public AFastProjectile
{
	DECLARE_CLASS (ACFlameMissile, AFastProjectile)
public:
	void BeginPlay ();
	void Effect ();
};

IMPLEMENT_CLASS (ACFlameMissile)

void ACFlameMissile::BeginPlay ()
{
	special1 = 2;
}

void ACFlameMissile::Effect ()
{
	fixed_t newz;

	if (!--special1)
	{
		special1 = 4;
		newz = z-12*FRACUNIT;
		if (newz < floorz)
		{
			newz = floorz;
		}
		AActor *mo = Spawn ("CFlameFloor", x, y, newz, ALLOW_REPLACE);
		if (mo)
		{
			mo->angle = angle;
		}
	}
}

//============================================================================
//
// A_CFlameAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlameAttack)
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
		S_Sound (self, CHAN_WEAPON, "ClericFlameFire", 1, ATTN_NORM);
		return;
	}

	P_SpawnPlayerMissileWithPossibleSpread (self, RUNTIME_CLASS(ACFlameMissile)); // [BB] Spread
	S_Sound (self, CHAN_WEAPON, "ClericFlameFire", 1, ATTN_NORM);

	// [BC] If we're the server, tell other clients to make the sound.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_WeaponSound( ULONG( player - players ), "ClericFlameFire", ULONG( player - players ), SVCF_SKIPTHISCLIENT );
}

//============================================================================
//
// A_CFlamePuff
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlamePuff)
{
	self->renderflags &= ~RF_INVISIBLE;
	self->velx = 0;
	self->vely = 0;
	self->velz = 0;
	S_Sound (self, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);
}

//============================================================================
//
// A_CFlameMissile
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlameMissile)
{
	int i;
	int an, an90;
	fixed_t dist;
	AActor *mo;
	
	self->renderflags &= ~RF_INVISIBLE;
	S_Sound (self, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);

	// [BC] Let the server handle this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	AActor *BlockingMobj = self->BlockingMobj;
	if (BlockingMobj && BlockingMobj->flags&MF_SHOOTABLE)
	{ // Hit something, so spawn the flame circle around the thing
		dist = BlockingMobj->radius+18*FRACUNIT;
		for (i = 0; i < 4; i++)
		{
			an = (i*ANG45)>>ANGLETOFINESHIFT;
			an90 = (i*ANG45+ANG90)>>ANGLETOFINESHIFT;
			mo = Spawn ("CircleFlame", BlockingMobj->x+FixedMul(dist, finecosine[an]),
				BlockingMobj->y+FixedMul(dist, finesine[an]), 
				BlockingMobj->z+5*FRACUNIT, ALLOW_REPLACE);
			if (mo)
			{
				mo->angle = an<<ANGLETOFINESHIFT;
				mo->target = self->target;
				mo->velx = mo->special1 = FixedMul(FLAMESPEED, finecosine[an]);
				mo->vely = mo->special2 = FixedMul(FLAMESPEED, finesine[an]);
				mo->tics -= pr_missile()&3;

				// [BC] If we're the server, spawn this to clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SpawnThing( mo );
			}
			mo = Spawn ("CircleFlame", BlockingMobj->x-FixedMul(dist, finecosine[an]),
				BlockingMobj->y-FixedMul(dist, finesine[an]), 
				BlockingMobj->z+5*FRACUNIT, ALLOW_REPLACE);
			if(mo)
			{
				mo->angle = ANG180+(an<<ANGLETOFINESHIFT);
				mo->target = self->target;
				mo->velx = mo->special1 = FixedMul(-FLAMESPEED, finecosine[an]);
				mo->vely = mo->special2 = FixedMul(-FLAMESPEED, finesine[an]);
				mo->tics -= pr_missile()&3;

				// [BC] If we're the server, spawn this to clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SpawnThing( mo );
			}
		}
		self->SetState (self->SpawnState);
	}
}

//============================================================================
//
// A_CFlameRotate
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CFlameRotate)
{
	int an;

	an = (self->angle+ANG90)>>ANGLETOFINESHIFT;
	self->velx = self->special1+FixedMul(FLAMEROTSPEED, finecosine[an]);
	self->vely = self->special2+FixedMul(FLAMEROTSPEED, finesine[an]);
	self->angle += ANG90/15;
}
