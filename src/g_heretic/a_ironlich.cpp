/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

static FRandom pr_foo ("WhirlwindDamage");
static FRandom pr_atk ("LichAttack");
static FRandom pr_seek ("WhirlwindSeek");

class AWhirlwind : public AActor
{
	DECLARE_CLASS (AWhirlwind, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS(AWhirlwind)

int AWhirlwind::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	int randVal;

	target->angle += pr_foo.Random2() << 20;
	target->velx += pr_foo.Random2() << 10;
	target->vely += pr_foo.Random2() << 10;
	if ((level.time & 16) && !(target->flags2 & MF2_BOSS))
	{
		randVal = pr_foo();
		if (randVal > 160)
		{
			randVal = 160;
		}
		target->velz += randVal << 11;
		if (target->velz > 12*FRACUNIT)
		{
			target->velz = 12*FRACUNIT;
		}
	}
	if (!(level.time & 7))
	{
		P_DamageMobj (target, NULL, this->target, 3, NAME_Melee);
	}
	return -1;
}

//----------------------------------------------------------------------------
//
// PROC A_LichAttack
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_LichAttack)
{
	int i;
	AActor *fire;
	AActor *baseFire;
	AActor *mo;
	AActor *target;
	int randAttack;
	static const int atkResolve1[] = { 50, 150 };
	static const int atkResolve2[] = { 150, 200 };
	int dist;

	// [BB] This is server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Ice ball		(close 20% : far 60%)
	// Fire column	(close 40% : far 20%)
	// Whirlwind	(close 40% : far 20%)
	// Distance threshold = 8 cells

	target = self->target;
	if (target == NULL)
	{
		return;
	}
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		int damage = pr_atk.HitDice (6);
		int newdam = P_DamageMobj (target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, target, self);
		return;
	}
	dist = P_AproxDistance (self->x-target->x, self->y-target->y)
		> 8*64*FRACUNIT;
	randAttack = pr_atk ();
	if (randAttack < atkResolve1[dist])
	{ // Ice ball
		P_SpawnMissile (self, target, PClass::FindClass("HeadFX1"), NULL, true); // [BB] Inform clients
		S_Sound (self, CHAN_BODY, "ironlich/attack2", 1, ATTN_NORM, true);	// [BB] Inform the clients.
	}
	else if (randAttack < atkResolve2[dist])
	{ // Fire column
		baseFire = P_SpawnMissile (self, target, PClass::FindClass("HeadFX3"));
		if (baseFire != NULL)
		{
			// [BB] If we're the server, tell the clients to spawn this missile and to update this thing's state.
			if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			{
				SERVERCOMMANDS_SpawnMissile( baseFire );
				SERVERCOMMANDS_SetThingFrame( baseFire, baseFire->FindState("NoGrow") );
			}

			baseFire->SetState (baseFire->FindState("NoGrow"));
			for (i = 0; i < 5; i++)
			{
				fire = Spawn("HeadFX3", baseFire->x, baseFire->y,
					baseFire->z, ALLOW_REPLACE);
				if (i == 0)
				{
					S_Sound (self, CHAN_BODY, "ironlich/attack1", 1, ATTN_NORM, true);	// [BB] Inform the clients.
				}
				fire->target = baseFire->target;
				fire->angle = baseFire->angle;
				fire->velx = baseFire->velx;
				fire->vely = baseFire->vely;
				fire->velz = baseFire->velz;
				fire->Damage = 0;
				fire->health = (i+1) * 2;

				// [BB] If we're the server, tell the clients to spawn the fire as missle using SERVERCOMMANDS_SpawnMissile
				// (just using SERVERCOMMANDS_SpawnThing + SERVERCOMMANDS_MoveThing doesn't work properly).
				if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
					SERVERCOMMANDS_SpawnMissile( fire );

				P_CheckMissileSpawn (fire, self->radius);
			}
		}
	}
	else
	{ // Whirlwind
		mo = P_SpawnMissile (self, target, RUNTIME_CLASS(AWhirlwind));
		if (mo != NULL)
		{
			mo->z -= 32*FRACUNIT;
			mo->tracer = target;
			mo->special1 = 60;
			mo->special2 = 50; // Timer for active sound
			mo->health = 20*TICRATE; // Duration
			S_Sound (self, CHAN_BODY, "ironlich/attack3", 1, ATTN_NORM, true);	// [BB] Inform the clients.

			// [BB] If we're the server, the tell clients to spawn this missile.
			if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			{
				SERVERCOMMANDS_SpawnMissile( mo );
 			}
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_WhirlwindSeek
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_WhirlwindSeek)
{
	self->health -= 3;
	if (self->health < 0)
	{
		self->velx = self->vely = self->velz = 0;
		self->SetState (self->FindState(NAME_Death));
		self->flags &= ~MF_MISSILE;
		return;
	}
	if ((self->special2 -= 3) < 0)
	{
		self->special2 = 58 + (pr_seek() & 31);
		S_Sound (self, CHAN_BODY, "ironlich/attack3", 1, ATTN_NORM);
	}
	if (self->tracer && self->tracer->flags&MF_SHADOW)
	{
		return;
	}
	P_SeekerMissile (self, ANGLE_1*10, ANGLE_1*30);
}

//----------------------------------------------------------------------------
//
// PROC A_LichIceImpact
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_LichIceImpact)
{
	unsigned int i;
	angle_t angle;
	AActor *shard;

	for (i = 0; i < 8; i++)
	{
		shard = Spawn("HeadFX2", self->x, self->y, self->z, ALLOW_REPLACE);
		angle = i*ANG45;
		shard->target = self->target;
		shard->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		shard->velx = FixedMul (shard->Speed, finecosine[angle]);
		shard->vely = FixedMul (shard->Speed, finesine[angle]);
		shard->velz = -FRACUNIT*6/10;
		P_CheckMissileSpawn (shard, self->radius);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_LichFireGrow
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_LichFireGrow)
{
	// [BB] This is server-side. The client can't do it, cause it doesn't know the health of the fire.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	self->health--;
	self->z += 9*FRACUNIT;

	// [BB] Tell clients of the changed z coord.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThing( self, CM_Z );

	if (self->health == 0)
	{
		self->Damage = self->GetDefault()->Damage;

		// [BB] Update the thing's state on the clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("NoGrow") );

		self->SetState (self->FindState("NoGrow"));
	}
}

