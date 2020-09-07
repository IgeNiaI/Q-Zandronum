/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "a_action.h"
#include "m_random.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_boom ("BishopBoom");
static FRandom pr_atk ("BishopAttack");
static FRandom pr_decide ("BishopDecide");
static FRandom pr_doblur ("BishopDoBlur");
static FRandom pr_sblur ("BishopSpawnBlur");
static FRandom pr_pain ("BishopPainBlur");

//============================================================================
//
// A_BishopAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopAttack)
{
	// [BB] This is server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NORM, true);	// [BB] Inform the clients.
	if (self->CheckMeleeRange())
	{
		int damage = pr_atk.HitDice (4);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return;
	}
	self->special1 = (pr_atk() & 3) + 5;
}

//============================================================================
//
// A_BishopAttack2
//
//		Spawns one of a string of bishop missiles
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopAttack2)
{
	AActor *mo;

	// [BB] This is server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target || !self->special1)
	{
		self->special1 = 0;
		self->SetState (self->SeeState);

		// [BB] If we're the server, tell the clients of the state change.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );

		return;
	}
	mo = P_SpawnMissile (self, self->target, PClass::FindClass("BishopFX"), NULL, true); // [BB] Inform clients
	if (mo != NULL)
	{
		mo->tracer = self->target;
	}
	self->special1--;
}

//============================================================================
//
// A_BishopMissileWeave
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopMissileWeave)
{
	A_Weave(self, 2, 2, 2*FRACUNIT, FRACUNIT);
}

//============================================================================
//
// A_BishopDecide
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopDecide)
{
	// [BB] This is server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (pr_decide() < 220)
	{
		return;
	}
	else
	{
		// [BB] If we're the server, tell the clients to update this thing's state.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState ("Blur") );

		self->SetState (self->FindState ("Blur"));
	}		
}

//============================================================================
//
// A_BishopDoBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopDoBlur)
{
	// [BB] This is server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	self->special1 = (pr_doblur() & 3) + 3; // Random number of blurs
	if (pr_doblur() < 120)
	{
		P_ThrustMobj (self, self->angle + ANG90, 11*FRACUNIT);
	}
	else if (pr_doblur() > 125)
	{
		P_ThrustMobj (self, self->angle - ANG90, 11*FRACUNIT);
	}
	else
	{ // Thrust forward
		P_ThrustMobj (self, self->angle, 11*FRACUNIT);
	}

	// [BB] If we're the server, update the thing's velocity.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_MoveThingExact( self, CM_VELX|CM_VELY );
	}

	S_Sound (self, CHAN_BODY, "BishopBlur", 1, ATTN_NORM, true);	// [BB] Inform the clients.
}

//============================================================================
//
// A_BishopSpawnBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopSpawnBlur)
{
	// [BB] This is server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	AActor *mo;

	if (!--self->special1)
	{
		self->velx = 0;
		self->vely = 0;

		// [BB] If we're the server, update the thing's velocity.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThingExact( self, CM_VELX|CM_VELY );

		if (pr_sblur() > 96)
		{
			// [BB] If we're the server, tell the clients of the state change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( self, STATE_SEE );

			self->SetState (self->SeeState);
		}
		else
		{
			// [BB] If we're the server, tell the clients of the state change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( self, STATE_MISSILE );
			
			self->SetState (self->MissileState);
		}
	}
	mo = Spawn ("BishopBlur", self->x, self->y, self->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = self->angle;

		// [BB] If we're the server, tell the clients to spawn the thing and set its angle.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( mo );
			SERVERCOMMANDS_SetThingAngle( mo );
		}
	}
}

//============================================================================
//
// A_BishopChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopChase)
{
	// [BB] This is server-side. The z coordinate seems to go out of sync
	// on client and server, if you make this client side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	self->z -= finesine[self->special2 << BOBTOFINESHIFT] * 4;
	self->special2 = (self->special2 + 4) & 63;
	self->z += finesine[self->special2 << BOBTOFINESHIFT] * 4;

	// [BB] If we're the server, update the thing's z coordinate.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( self, CM_Z );
}

//============================================================================
//
// A_BishopPuff
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopPuff)
{
	AActor *mo;

	mo = Spawn ("BishopPuff", self->x, self->y, self->z + 40*FRACUNIT, ALLOW_REPLACE);
	if (mo)
	{
		mo->velz = FRACUNIT/2;
	}
}

//============================================================================
//
// A_BishopPainBlur
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BishopPainBlur)
{
	AActor *mo;

	// [BB] This is server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (pr_pain() < 64)
	{
		// [BB] If we're the server, tell the clients to update this thing's state.
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState ("Blur") );

		self->SetState (self->FindState ("Blur"));
		return;
	}
	fixed_t x = self->x + (pr_pain.Random2()<<12);
	fixed_t y = self->y + (pr_pain.Random2()<<12);
	fixed_t z = self->z + (pr_pain.Random2()<<11);
	mo = Spawn ("BishopPainBlur", x, y, z, ALLOW_REPLACE);
	if (mo)
	{
		mo->angle = self->angle;

		// [BB] If we're the server, tell the clients to spawn the thing and set its angle.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( mo );
			SERVERCOMMANDS_SetThingAngle( mo );
		}
	}
}
