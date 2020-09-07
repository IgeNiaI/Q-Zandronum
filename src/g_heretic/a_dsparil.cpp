/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "a_specialspot.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

static FRandom pr_s2fx1 ("S2FX1");
static FRandom pr_scrc1atk ("Srcr1Attack");
static FRandom pr_dst ("D'SparilTele");
static FRandom pr_s2d ("Srcr2Decide");
static FRandom pr_s2a ("Srcr2Attack");
static FRandom pr_bluespark ("BlueSpark");

//----------------------------------------------------------------------------
//
// PROC A_Sor1Pain
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor1Pain)
{
	self->special1 = 20; // Number of steps to walk fast
	CALL_ACTION(A_Pain, self);
}

//----------------------------------------------------------------------------
//
// PROC A_Sor1Chase
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor1Chase)
{
	if (self->special1)
	{
		self->special1--;
		self->tics -= 3;
	}
	A_Chase(self);
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr1Attack
//
// Sorcerer demon attack.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Srcr1Attack)
{
	AActor *mo;
	fixed_t velz;
	angle_t angle;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NORM, true);	// [EP] Inform the clients.
	if (self->CheckMeleeRange ())
	{
		int damage = pr_scrc1atk.HitDice (8);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return;
	}

	const PClass *fx = PClass::FindClass("SorcererFX1");
	if (self->health > (self->SpawnHealth()/3)*2)
	{ // Spit one fireball
		P_SpawnMissileZ (self, self->z + 48*FRACUNIT, self->target, fx, true); // [BB] Inform clients
	}
	else
	{ // Spit three fireballs
		mo = P_SpawnMissileZ (self, self->z + 48*FRACUNIT, self->target, fx, true); // [BB] Inform clients
		if (mo != NULL)
		{
			velz = mo->velz;
			angle = mo->angle;
			P_SpawnMissileAngleZ (self, self->z + 48*FRACUNIT, fx, angle-ANGLE_1*3, velz, true); // [BB] Inform clients
			P_SpawnMissileAngleZ (self, self->z + 48*FRACUNIT, fx, angle+ANGLE_1*3, velz, true); // [BB] Inform clients
		}
		if (self->health < self->SpawnHealth()/3)
		{ // Maybe attack again
			if (self->special1)
			{ // Just attacked, so don't attack again
				self->special1 = 0;
			}
			else
			{ // Set state to attack again
				self->special1 = 1;

				// [BB] Update the thing's state on the clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingFrame( self, self->FindState("Missile2") );

				self->SetState (self->FindState("Missile2"));
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SorcererRise
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_SorcererRise)
{
	AActor *mo;

	self->flags &= ~MF_SOLID;

	// [BC] Let the server spawn this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	mo = Spawn("Sorcerer2", self->x, self->y, self->z, ALLOW_REPLACE);
	mo->Translation = self->Translation;
	mo->SetState (mo->FindState("Rise"));
	mo->angle = self->angle;
	mo->CopyFriendliness (self, true);

	// [BC/BB] If we're the server, spawn the sorcerer for clients and also set the state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnThing( mo );
		SERVERCOMMANDS_SetThingFrame( mo, mo->FindState("Rise") );
	}
}

//----------------------------------------------------------------------------
//
// PROC P_DSparilTeleport
//
//----------------------------------------------------------------------------

void P_DSparilTeleport (AActor *actor)
{
	fixed_t prevX;
	fixed_t prevY;
	fixed_t prevZ;
	AActor *mo;
	AActor *spot;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	DSpotState *state = DSpotState::GetSpotState();
	if (state == NULL) return;

	spot = state->GetSpotWithMinMaxDistance(PClass::FindClass("BossSpot"), actor->x, actor->y, 128*FRACUNIT, 0);
	if (spot == NULL) return;

	prevX = actor->x;
	prevY = actor->y;
	prevZ = actor->z;
	if (P_TeleportMove (actor, spot->x, spot->y, spot->z, false))
	{
		mo = Spawn("Sorcerer2Telefade", prevX, prevY, prevZ, ALLOW_REPLACE);
		if (mo) mo->Translation = actor->Translation;

		// [BC] Spawn the actor to clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( mo );
			// [BB] Also notify the clients of the state change.
			SERVERCOMMANDS_SetThingFrame( actor, actor->FindState("Teleport") );
		}

		S_Sound (mo, CHAN_BODY, "misc/teleport", 1, ATTN_NORM, true);	// [BC] Inform the clients.
		actor->SetState (actor->FindState("Teleport"));
		S_Sound (actor, CHAN_BODY, "misc/teleport", 1, ATTN_NORM, true);	// [BC] Inform the clients.
		actor->z = actor->floorz;
		actor->angle = spot->angle;
		actor->velx = actor->vely = actor->velz = 0;

		// [BB] Tell clients of the new position of "actor".
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( actor, CM_X|CM_Y|CM_Z|CM_ANGLE|CM_VELX|CM_VELY|CM_VELZ );

	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Decide
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Srcr2Decide)
{

	static const int chance[] =
	{
		192, 120, 120, 120, 64, 64, 32, 16, 0
	};

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	unsigned int chanceindex = self->health / ((self->SpawnHealth()/8 == 0) ? 1 : self->SpawnHealth()/8);
	if (chanceindex >= countof(chance))
	{
		chanceindex = countof(chance) - 1;
	}

	if (pr_s2d() < chance[chanceindex])
	{
		P_DSparilTeleport (self);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Srcr2Attack
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Srcr2Attack)
{
	int chance;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_BODY, self->AttackSound, 1, ATTN_NONE, true);	// [EP] Inform the clients.
	if (self->CheckMeleeRange())
	{
		int damage = pr_s2a.HitDice (20);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return;
	}
	chance = self->health < self->SpawnHealth()/2 ? 96 : 48;
	if (pr_s2a() < chance)
	{ // Wizard spawners

		const PClass *fx = PClass::FindClass("Sorcerer2FX2");
		if (fx)
		{
			P_SpawnMissileAngle (self, fx, self->angle-ANG45, FRACUNIT/2, true); // [BB] Inform clients
			P_SpawnMissileAngle (self, fx, self->angle+ANG45, FRACUNIT/2, true); // [BB] Inform clients
		}
	}
	else
	{ // Blue bolt
		P_SpawnMissile (self, self->target, PClass::FindClass("Sorcerer2FX1"), NULL, true); // [BB] Inform clients
	}
}

//----------------------------------------------------------------------------
//
// PROC A_BlueSpark
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BlueSpark)
{
	int i;
	AActor *mo;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	for (i = 0; i < 2; i++)
	{
		mo = Spawn("Sorcerer2FXSpark", self->x, self->y, self->z, ALLOW_REPLACE);
		mo->velx = pr_bluespark.Random2() << 9;
		mo->vely = pr_bluespark.Random2() << 9;
		mo->velz = FRACUNIT + (pr_bluespark()<<8);

		// [BC]
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnMissile( mo );
	}
}

//----------------------------------------------------------------------------
//
// PROC A_GenWizard
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_GenWizard)
{
	AActor *mo;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	mo = Spawn("Wizard", self->x, self->y, self->z, ALLOW_REPLACE);
	if (mo != NULL)
	{
		mo->z -= mo->GetDefault()->height/2;
		if (!P_TestMobjLocation (mo))
		{ // Didn't fit
			mo->ClearCounters();
			mo->Destroy ();
		}
		else
		{ // [RH] Make the new wizards inherit D'Sparil's target
			mo->CopyFriendliness (self->target, true);

			self->velx = self->vely = self->velz = 0;
			self->SetState (self->FindState(NAME_Death));
			self->flags &= ~MF_MISSILE;
			mo->master = self->target;

			// [BC]
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( mo );

			// Heretic did not offset it by TELEFOGHEIGHT, so I won't either.
			mo = Spawn<ATeleportFog> (self->x, self->y, self->z, ALLOW_REPLACE);

			// [BC]
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( mo );
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthInit
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor2DthInit)
{
	self->special1 = 7; // Animation loop counter
	P_Massacre (); // Kill monsters early
}

//----------------------------------------------------------------------------
//
// PROC A_Sor2DthLoop
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Sor2DthLoop)
{
	if (--self->special1)
	{ // Need to loop
		self->SetState (self->FindState("DeathLoop"));
	}
}

