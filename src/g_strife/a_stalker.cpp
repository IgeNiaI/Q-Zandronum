/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_stalker ("Stalker");


DEFINE_ACTION_FUNCTION(AActor, A_StalkerChaseDecide)
{
	// [BC] This is handled server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!(self->flags & MF_NOGRAVITY))
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("SeeFloor") );

		self->SetState (self->FindState("SeeFloor"));
	}
	else if (self->ceilingz - self->height > self->z)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Drop") );

		self->SetState (self->FindState("Drop"));
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerLookInit)
{
	FState *state;

	// [BC] This is handled server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (self->flags & MF_NOGRAVITY)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("LookCeiling") );

		state = self->FindState("LookCeiling");
	}
	else
	{
		state = self->FindState("LookFloor");
	}
	if (self->state->NextState != state)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, state );

		self->SetState (state);

	}
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerDrop)
{
	self->flags5 &= ~MF5_NOVERTICALMELEERANGE;
	self->flags &= ~MF_NOGRAVITY;
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerAttack)
{
	// [BC] This is handled server-side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (self->flags & MF_NOGRAVITY)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Drop") );

		self->SetState (self->FindState("Drop"));
	}
	else if (self->target != NULL)
	{
		A_FaceTarget (self);
		if (self->CheckMeleeRange ())
		{
			int damage = (pr_stalker() & 7) * 2 + 2;

			int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		}
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_StalkerWalk)
{
	S_Sound (self, CHAN_BODY, "stalker/walk", 1, ATTN_NORM);
	A_Chase (self);
}
