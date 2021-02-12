/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "thingdef/thingdef.h"
*/

#define FIREDEMON_ATTACK_RANGE	64*8*FRACUNIT

static FRandom pr_firedemonrock ("FireDemonRock");
static FRandom pr_smbounce ("SMBounce");
static FRandom pr_firedemonchase ("FiredChase");
static FRandom pr_firedemonsplotch ("FiredSplotch");

//============================================================================
// Fire Demon AI
//
// special1			index into floatbob
// special2			whether strafing or not
//============================================================================

//============================================================================
//
// A_FiredSpawnRock
//
//============================================================================

void A_FiredSpawnRock (AActor *actor)
{
	AActor *mo;
	int x,y,z;
	const PClass *rtype;

	switch (pr_firedemonrock() % 5)
	{
		case 0:
			rtype = PClass::FindClass ("FireDemonRock1");
			break;
		case 1:
			rtype = PClass::FindClass ("FireDemonRock2");
			break;
		case 2:
			rtype = PClass::FindClass ("FireDemonRock3");
			break;
		case 3:
			rtype = PClass::FindClass ("FireDemonRock4");
			break;
		case 4:
		default:
			rtype = PClass::FindClass ("FireDemonRock5");
			break;
	}

	x = actor->x + ((pr_firedemonrock() - 128) << 12);
	y = actor->y + ((pr_firedemonrock() - 128) << 12);
	z = actor->z + ( pr_firedemonrock() << 11);
	mo = Spawn (rtype, x, y, z, ALLOW_REPLACE);
	if (mo)
	{
		// [BB] Clients spawn these on their own. In order to prevent the 
		// server from printing warnings when the server calls P_ExplodeMissile,
		// we also mark this as SERVERSIDEONLY.
		if ( NETWORK_GetState () == NETSTATE_SERVER )
		{
			mo->ulNetworkFlags |= NETFL_SERVERSIDEONLY;
			mo->FreeNetID ();
		}

		mo->target = actor;
		mo->velx = (pr_firedemonrock() - 128) <<10;
		mo->vely = (pr_firedemonrock() - 128) <<10;
		mo->velz = (pr_firedemonrock() << 10);
		mo->special1 = 2;		// Number bounces
	}

	// Initialize fire demon
	actor->special2 = 0;
	actor->flags &= ~MF_JUSTATTACKED;
}

//============================================================================
//
// A_FiredRocks
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredRocks)
{
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
	A_FiredSpawnRock (self);
}

//============================================================================
//
// A_SmBounce
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SmBounce)
{
	// give some more velocity (x,y,&z)
	self->z = self->floorz + FRACUNIT;
	self->velz = (2*FRACUNIT) + (pr_smbounce() << 10);
	self->velx = pr_smbounce()%3<<FRACBITS;
	self->vely = pr_smbounce()%3<<FRACBITS;
}

//============================================================================
//
// A_FiredAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredAttack)
{
	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (self->target == NULL)
		return;
	AActor *mo = P_SpawnMissile (self, self->target, PClass::FindClass ("FireDemonMissile"));
	if (mo)
	{
		// [BC] If we're the server, spawn this for clients.
		if ( NETWORK_GetState() == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( mo );

		S_Sound (self, CHAN_BODY, "FireDemonAttack", 1, ATTN_NORM, true);	// [BC] Inform the clients.
	}
}

//============================================================================
//
// A_FiredChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredChase)
{
	int weaveindex = self->special1;
	AActor *target = self->target;
	angle_t ang;
	fixed_t dist;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		// make active sound
		if (pr_firedemonchase() < 3)
		{
			self->PlayActiveSound ();
		}

		// Normal movement
		if ( self->special2 == 0 )
			P_Move( self );

		return;
	}

	if (self->reactiontime) self->reactiontime--;
	if (self->threshold) self->threshold--;

	// Float up and down
	self->z += finesine[weaveindex << BOBTOFINESHIFT] * 8;
	self->special1 = (weaveindex + 2) & 63;

	// Ensure it stays above certain height
	if (self->z < self->floorz + (64*FRACUNIT))
	{
		self->z += 2*FRACUNIT;
	}

	// [BC] If we're the server, update the thing's z position.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThing( self, CM_Z );

	if(!self->target || !(self->target->flags&MF_SHOOTABLE))
	{	// Invalid target
		P_LookForPlayers (self,true, NULL);
		return;
	}

	// Strafe
	if (self->special2 > 0)
	{
		self->special2--;
	}
	else
	{
		self->special2 = 0;
		self->velx = self->vely = 0;
		dist = P_AproxDistance (self->x - target->x, self->y - target->y);
		if (dist < FIREDEMON_ATTACK_RANGE)
		{
			if (pr_firedemonchase() < 30)
			{
				ang = R_PointToAngle2 (self->x, self->y, target->x, target->y);
				if (pr_firedemonchase() < 128)
					ang += ANGLE_90;
				else
					ang -= ANGLE_90;
				ang >>= ANGLETOFINESHIFT;
				self->velx = finecosine[ang] << 3; //FixedMul (8*FRACUNIT, finecosine[ang]);
				self->vely = finesine[ang] << 3; //FixedMul (8*FRACUNIT, finesine[ang]);
				self->special2 = 3;		// strafe time
			}
		}

		// [BC] If we're the server, update the thing's z position.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_MoveThing( self, CM_VELX|CM_VELY );
			SERVERCOMMANDS_SetThingSpecial2( self );
		}
	}

	FaceMovementDirection (self);

	// Normal movement
	if (!self->special2)
	{
		if (--self->movecount<0 || !P_Move (self))
		{
			P_NewChaseDir (self);
		}
	}

	// Do missile attack
	if (!(self->flags & MF_JUSTATTACKED))
	{
		if (P_CheckMissileRange (self) && (pr_firedemonchase() < 20))
		{
			// [BC] If we're the server, tell clients to change this monster's state.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( self, STATE_MISSILE );

			self->SetState (self->MissileState);
			self->flags |= MF_JUSTATTACKED;
			return;
		}
	}
	else
	{
		self->flags &= ~MF_JUSTATTACKED;
	}

	// make active sound
	if (pr_firedemonchase() < 3)
	{
		self->PlayActiveSound ();
	}
}

//============================================================================
//
// A_FiredSplotch
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FiredSplotch)
{
	AActor *mo;

	mo = Spawn ("FireDemonSplotch1", self->x, self->y, self->z, ALLOW_REPLACE);
	if (mo)
	{
		// [BB] Clients spawn these on their own. In order to prevent the 
		// server from printing warnings when the server calls P_ExplodeMissile,
		// we also mark this as SERVERSIDEONLY.
		if ( NETWORK_GetState () == NETSTATE_SERVER )
		{
			mo->ulNetworkFlags |= NETFL_SERVERSIDEONLY;
			mo->FreeNetID ();
		}

		mo->velx = (pr_firedemonsplotch() - 128) << 11;
		mo->vely = (pr_firedemonsplotch() - 128) << 11;
		mo->velz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
	mo = Spawn ("FireDemonSplotch2", self->x, self->y, self->z, ALLOW_REPLACE);
	if (mo)
	{
		// [BB] Clients spawn these on their own. In order to prevent the 
		// server from printing warnings when the server calls P_ExplodeMissile,
		// we also mark this as SERVERSIDEONLY.
		if ( NETWORK_GetState () == NETSTATE_SERVER )
		{
			mo->ulNetworkFlags |= NETFL_SERVERSIDEONLY;
			mo->FreeNetID ();
		}

		mo->velx = (pr_firedemonsplotch() - 128) << 11;
		mo->vely = (pr_firedemonsplotch() - 128) << 11;
		mo->velz = (pr_firedemonsplotch() << 10) + FRACUNIT*3;
	}
}
