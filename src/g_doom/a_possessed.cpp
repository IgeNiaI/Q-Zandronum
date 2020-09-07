/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_posattack ("PosAttack");
static FRandom pr_sposattack ("SPosAttack");
static FRandom pr_cposattack ("CPosAttack");
static FRandom pr_cposrefire ("CPosRefire");

//
// A_PosAttack
//
DEFINE_ACTION_FUNCTION(AActor, A_PosAttack)
{
	int angle;
	int damage;
	int slope;
		
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_InClientMode() )
	{
		S_Sound( self, CHAN_WEAPON, "grunt/attack", 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;
				
	A_FaceTarget (self);
	angle = self->angle;
	slope = P_AimLineAttack (self, angle, MISSILERANGE);

	S_Sound (self, CHAN_WEAPON, "grunt/attack", 1, ATTN_NORM);
	angle += pr_posattack.Random2() << 20;
	damage = ((pr_posattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
}

static void A_SPosAttack2 (AActor *self)
{
	int i;
	int bangle;
	int slope;
		
	A_FaceTarget (self);
	bangle = self->angle;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	for (i=0 ; i<3 ; i++)
    {
		int angle = bangle + (pr_sposattack.Random2() << 20);
		int damage = ((pr_sposattack()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
    }
}

DEFINE_ACTION_FUNCTION(AActor, A_SPosAttackUseAtkSound)
{
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_InClientMode() )
	{
		S_Sound ( self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_SPosAttack2 (self);
}

// This version of the function, which uses a hard-coded sound, is
// meant for Dehacked only.
DEFINE_ACTION_FUNCTION(AActor, A_SPosAttack)
{
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_InClientMode() )
	{
		S_Sound ( self, CHAN_WEAPON, "shotguy/attack", 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;

	S_Sound (self, CHAN_WEAPON, "shotguy/attack", 1, ATTN_NORM);
	A_SPosAttack2 (self);
}

DEFINE_ACTION_FUNCTION(AActor, A_CPosAttack)
{
	int angle;
	int bangle;
	int damage;
	int slope;
		
	// [BC] Server takes care of the rest of this.
	if ( NETWORK_InClientMode() )
	{
		// [RH] Andy Baker's stealth monsters
		if (self->flags & MF_STEALTH)
		{
			self->visdir = 1;
		}

		S_Sound ( self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM );
		return;
	}

	if (!self->target)
		return;

	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = 1;
	}

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	A_FaceTarget (self);
	bangle = self->angle;
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	angle = bangle + (pr_cposattack.Random2() << 20);
	damage = ((pr_cposattack()%5)+1)*3;
	P_LineAttack (self, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
}

DEFINE_ACTION_FUNCTION(AActor, A_CPosRefire)
{
	// keep firing unless target got out of sight
	A_FaceTarget (self);

	// [BC] Client chaingunners continue to fire until told by the server to stop.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (pr_cposrefire() < 40)
		return;

	if (!self->target
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES))
	{
		// [BC] If we're the server, tell clients to update this thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );

		self->SetState (self->SeeState);
	}
}
