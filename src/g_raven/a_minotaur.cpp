#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_enemy.h"
#include "ravenshared.h"
#include "a_action.h"
#include "gi.h"
#include "w_wad.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "doomstat.h"
#include "farchive.h"
// [BB] New #includes.
#include "cl_demo.h"
#include "deathmatch.h"
#include "network.h"
#include "sv_commands.h"

#define MAULATORTICS (25*35)

static FRandom pr_minotauratk1 ("MinotaurAtk1");
static FRandom pr_minotaurdecide ("MinotaurDecide");
static FRandom pr_atk ("MinotaurAtk2");
static FRandom pr_minotauratk3 ("MinotaurAtk3");
static FRandom pr_fire ("MntrFloorFire");
static FRandom pr_minotaurslam ("MinotaurSlam");
static FRandom pr_minotaurroam ("MinotaurRoam");
static FRandom pr_minotaurchase ("MinotaurChase");

void P_MinotaurSlam (AActor *source, AActor *target);

DECLARE_ACTION(A_MinotaurLook)

IMPLEMENT_CLASS(AMinotaur)

void AMinotaur::Tick ()
{
	Super::Tick ();

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// The unfriendly Minotaur (Heretic's) is invulnerable while charging
	if (!(flags5 & MF5_SUMMONEDMONSTER))
	{
		// Get MF_SKULLFLY bit and shift it so it matches MF2_INVULNERABLE
		DWORD flying = (flags & MF_SKULLFLY) << 3;
		if ((flags2 & MF2_INVULNERABLE) != flying)
		{
			flags2 ^= MF2_INVULNERABLE;

			// [BC] If we're the server, tell clients to update this thing's flags.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingFlags( this, FLAGSET_FLAGS2 );
		}
	}
}

bool AMinotaur::Slam (AActor *thing)
{
	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return ( false );
	}

	// Slamming minotaurs shouldn't move non-creatures
	if (!(thing->flags3&MF3_ISMONSTER) && !thing->player)
	{
		return false;
	}
	return Super::Slam (thing);
}

int AMinotaur::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	damage = Super::DoSpecialDamage (target, damage, damagetype);
	if ((damage != -1) && (flags & MF_SKULLFLY))
	{ // Slam only when in charge mode
		P_MinotaurSlam (this, target);
		return -1;
	}
	return damage;
}

// Minotaur Friend ----------------------------------------------------------

IMPLEMENT_CLASS(AMinotaurFriend)

void AMinotaurFriend::BeginPlay ()
{
	Super::BeginPlay ();
	StartTime = -1;
}

void AMinotaurFriend::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << StartTime;
}

void AMinotaurFriend::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	Super::Die (source, inflictor, dmgflags);

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (tracer && tracer->health > 0 && tracer->player)
	{
		// Search thinker list for minotaur
		TThinkerIterator<AMinotaurFriend> iterator;
		AMinotaurFriend *mo;

		while ((mo = iterator.Next()) != NULL)
		{
			if (mo->health <= 0) continue;
			// [RH] Minotaurs can't be morphed, so this isn't needed
			//if (!(mo->flags&MF_COUNTKILL)) continue;		// for morphed minotaurs
			if (mo->flags&MF_CORPSE) continue;
			if (mo->StartTime >= 0 && (level.maptime - StartTime) >= MAULATORTICS) continue;
			if (mo->tracer != NULL && mo->tracer->player == tracer->player) break;
		}

		if (mo == NULL)
		{
			AInventory *power = tracer->FindInventory (PClass::FindClass("PowerMinotaur"));
			if (power != NULL)
			{
				power->Destroy ();
			}
		}
	}
}

bool AMinotaurFriend::OkayToSwitchTarget (AActor *other)
{
	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return ( false );
	}

	if (other == tracer) return false;		// Do not target the master
	return Super::OkayToSwitchTarget (other);
}

// Action functions for the minotaur ----------------------------------------

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk1
//
// Melee attack.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurDeath)
{
	if (Wads.CheckNumForName ("MNTRF1", ns_sprites) < 0 &&
		Wads.CheckNumForName ("MNTRF0", ns_sprites) < 0)
		self->SetState(self->FindState ("FadeOut"));
}

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurAtk1)
{
	player_t *player;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_WEAPON, "minotaur/melee", 1, ATTN_NORM, true);	// [BC] Inform the clients.
	if (self->CheckMeleeRange())
	{
		int damage = pr_minotauratk1.HitDice (4);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		if ((player = self->target->player) != NULL &&
			player->mo == self->target)
		{ // Squish the player
			player->deltaviewheight = -16*FRACUNIT;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurDecide
//
// Choose a missile attack.
//
//----------------------------------------------------------------------------

#define MNTR_CHARGE_SPEED (13*FRACUNIT)

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurDecide)
{
	bool friendly = !!(self->flags5 & MF5_SUMMONEDMONSTER);
	angle_t angle;
	AActor *target;
	int dist;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	target = self->target;
	if (!target)
	{
		return;
	}
	if (!friendly)
	{
		S_Sound (self, CHAN_WEAPON, "minotaur/sight", 1, ATTN_NORM, true);	// [BC] Inform the clients.
	}
	dist = P_AproxDistance (self->x-target->x, self->y-target->y);
	if (target->z+target->height > self->z
		&& target->z+target->height < self->z+self->height
		&& dist < (friendly ? 16*64*FRACUNIT : 8*64*FRACUNIT)
		&& dist > 1*64*FRACUNIT
		&& pr_minotaurdecide() < 150)
	{ // Charge attack

		// [BC] If we're the server, set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState ("Charge"), MAXPLAYERS, 0, false );

		// Don't call the state function right away
		self->SetState (self->FindState ("Charge"), true);
		self->flags |= MF_SKULLFLY;
		if (!friendly)
		{ // Heretic's Minotaur is invulnerable during charge attack
			self->flags2 |= MF2_INVULNERABLE;

			// [BB] If we're the server, tell clients to update this thing's flags.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS2 );
		}
		A_FaceTarget (self);
		angle = self->angle>>ANGLETOFINESHIFT;
		self->velx = FixedMul (MNTR_CHARGE_SPEED, finecosine[angle]);
		self->vely = FixedMul (MNTR_CHARGE_SPEED, finesine[angle]);
		self->special1 = TICRATE/2; // Charge duration

		// [BB] If we're the server, update the thing's velocity and set the MF_SKULLFLY flag.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_MoveThing( self, CM_VELX|CM_VELY );
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
		}
	}
	else if (target->z == target->floorz
		&& dist < 9*64*FRACUNIT
		&& pr_minotaurdecide() < (friendly ? 100 : 220))
	{ // Floor fire attack
		// [BC] If we're the server, set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState ("Hammer") );

		self->SetState (self->FindState ("Hammer"));
		self->special2 = 0;
	}
	else
	{ // Swing attack
		A_FaceTarget (self);
		// Don't need to call P_SetMobjState because the current state
		// falls through to the swing attack
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurCharge
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurCharge)
{
	AActor *puff;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target) return;

	if (self->special1 > 0)
	{
		const PClass *type;

		if (gameinfo.gametype == GAME_Heretic)
		{
			type = PClass::FindClass ("PhoenixPuff");
		}
		else
		{
			type = PClass::FindClass ("PunchPuff");
		}
		puff = Spawn (type, self->x, self->y, self->z, ALLOW_REPLACE);
		puff->velz = 2*FRACUNIT;
		self->special1--;

		// [BC] If we're the server, tell clients to spawn the puff.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( puff );
			SERVERCOMMANDS_MoveThing( puff, CM_VELZ );
		}
	}
	else
	{
		self->flags &= ~MF_SKULLFLY;
		self->flags2 &= ~MF2_INVULNERABLE;

		// [BC] If we're the server, tell clients to update this thing's flags and state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS );
			SERVERCOMMANDS_SetThingFlags( self, FLAGSET_FLAGS2 );
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );
		}

		self->SetState (self->SeeState);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk2
//
// Swing attack.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurAtk2)
{
	AActor *mo;
	angle_t angle;
	fixed_t velz;
	fixed_t z;
	bool friendly = !!(self->flags5 & MF5_SUMMONEDMONSTER);

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_WEAPON, "minotaur/attack2", 1, ATTN_NORM, true);	// [BC] Inform the clients.
	if (self->CheckMeleeRange())
	{
		int damage;
		damage = pr_atk.HitDice (friendly ? 3 : 5);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		return;
	}
	z = self->z + 40*FRACUNIT;
	const PClass *fx = PClass::FindClass("MinotaurFX1");
	if (fx)
	{
		mo = P_SpawnMissileZ (self, z, self->target, fx, true); // [BB] Inform clients
		if (mo != NULL)
		{
//			S_Sound (mo, CHAN_WEAPON, "minotaur/attack2", 1, ATTN_NORM);
			velz = mo->velz;
			angle = mo->angle;
			P_SpawnMissileAngleZ (self, z, fx, angle-(ANG45/8), velz, true); // [BB] Inform clients
			P_SpawnMissileAngleZ (self, z, fx, angle+(ANG45/8), velz, true); // [BB] Inform clients
			P_SpawnMissileAngleZ (self, z, fx, angle-(ANG45/16), velz, true); // [BB] Inform clients
			P_SpawnMissileAngleZ (self, z, fx, angle+(ANG45/16), velz, true); // [BB] Inform clients
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MinotaurAtk3
//
// Floor fire attack.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurAtk3)
{
	AActor *mo;
	player_t *player;
	bool friendly = !!(self->flags5 & MF5_SUMMONEDMONSTER);

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (!self->target)
	{
		return;
	}
	S_Sound (self, CHAN_VOICE, "minotaur/attack3", 1, ATTN_NORM, true);	// [BC] Inform the clients.
	if (self->CheckMeleeRange())
	{
		int damage;
		
		damage = pr_minotauratk3.HitDice (friendly ? 3 : 5);
		int newdam = P_DamageMobj (self->target, self, self, damage, NAME_Melee);
		P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
		if ((player = self->target->player) != NULL &&
			player->mo == self->target)
		{ // Squish the player
			player->deltaviewheight = -16*FRACUNIT;
		}
	}
	else
	{
		if (self->floorclip > 0 && (i_compatflags & COMPATF_MINOTAUR))
		{
			// only play the sound. 
			S_Sound (self, CHAN_WEAPON, "minotaur/fx2hit", 1, ATTN_NORM, true);	// [BC] Inform the clients.
		}
		else
		{
			mo = P_SpawnMissile (self, self->target, PClass::FindClass("MinotaurFX2"));
			if (mo != NULL)
			{
				// [BC] If we're the server, tell clients to spawn the missile.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SpawnMissile( mo );
				}

				S_Sound (mo, CHAN_WEAPON, "minotaur/attack1", 1, ATTN_NORM, true);	// [BC] Inform the clients.
			}
		}
	}
	if (pr_minotauratk3() < 192 && self->special2 == 0)
	{
		// [BC] If we're the server, set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState ("HammerLoop") );

		self->SetState (self->FindState ("HammerLoop"));
		self->special2 = 1;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MntrFloorFire
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MntrFloorFire)
{
	AActor *mo;
	fixed_t x, y;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	self->z = self->floorz;
	x = self->x + (pr_fire.Random2 () << 10);
	y = self->y + (pr_fire.Random2 () << 10);
	mo = Spawn("MinotaurFX3", x, y, self->floorz, ALLOW_REPLACE);
	mo->target = self->target;
	mo->velx = 1; // Force block checking

	// [BC] If we're the server, tell clients to spawn this thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( mo );

	P_CheckMissileSpawn (mo, self->radius);
}

//---------------------------------------------------------------------------
//
// FUNC P_MinotaurSlam
//
//---------------------------------------------------------------------------

void P_MinotaurSlam (AActor *source, AActor *target)
{
	angle_t angle;
	fixed_t thrust;
	int damage;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	angle = R_PointToAngle2 (source->x, source->y, target->x, target->y);
	angle >>= ANGLETOFINESHIFT;
	thrust = 16*FRACUNIT+(pr_minotaurslam()<<10);
	target->velx += FixedMul (thrust, finecosine[angle]);
	target->vely += FixedMul (thrust, finesine[angle]);
	damage = pr_minotaurslam.HitDice (static_cast<AMinotaur *>(source) ? 4 : 6);
	int newdam = P_DamageMobj (target, NULL, NULL, damage, NAME_Melee);
	P_TraceBleed (newdam > 0 ? newdam : damage, target, angle, 0);
	if (target->player)
	{
		target->reactiontime = 14+(pr_minotaurslam()&7);
	}
}

//----------------------------------------------------------------------------
//
// Minotaur variables
//
//	special1		charge duration countdown
//	special2		internal to minotaur AI
//	StartTime		minotaur start time
// 	tracer			pointer to player that spawned it
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//
// A_MinotaurRoam
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurRoam)
{
	// In case pain caused him to skip his fade in.
	self->RenderStyle = STYLE_Normal;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (self->IsKindOf(RUNTIME_CLASS(AMinotaurFriend)))
	{
		AMinotaurFriend *self1 = static_cast<AMinotaurFriend *> (self);

		if (self1->StartTime >= 0 && (level.maptime - self1->StartTime) >= MAULATORTICS)
		{
			P_DamageMobj (self1, NULL, NULL, TELEFRAG_DAMAGE, NAME_None);
			return;
		}
	}

	if (pr_minotaurroam() < 30)
		CALL_ACTION(A_MinotaurLook, self);		// adjust to closest target

	if (pr_minotaurroam() < 6)
	{
		//Choose new direction
		self->movedir = pr_minotaurroam() % 8;
		FaceMovementDirection (self);
	}
	if (!P_Move(self))
	{
		// Turn
		if (pr_minotaurroam() & 1)
			self->movedir = (self->movedir + 1) % 8;
		else
			self->movedir = (self->movedir + 7) % 8;
		FaceMovementDirection (self);
	}
}


//----------------------------------------------------------------------------
//
//	PROC A_MinotaurLook
//
// Look for enemy of player
//----------------------------------------------------------------------------
#define MINOTAUR_LOOK_DIST		(16*54*FRACUNIT)

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurLook)
{
	if (!self->IsKindOf(RUNTIME_CLASS(AMinotaurFriend)))
	{
		CALL_ACTION(A_Look, self);
		return;
	}

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	AActor *mo = NULL;
	player_t *player;
	fixed_t dist;
	int i;
	AActor *master = self->tracer;

	self->target = NULL;
	if (deathmatch)					// Quick search for players
	{
    	for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i]) continue;
			player = &players[i];
			mo = player->mo;
			if (mo == master) continue;
			if (mo->health <= 0) continue;
			dist = P_AproxDistance(self->x - mo->x, self->y - mo->y);
			if (dist > MINOTAUR_LOOK_DIST) continue;
			self->target = mo;
			break;
		}
	}

	if (!self->target)				// Near player monster search
	{
		if (master && (master->health>0) && (master->player))
			mo = P_RoughMonsterSearch(master, 20);
		else
			mo = P_RoughMonsterSearch(self, 20);
		self->target = mo;
	}

	if (!self->target)				// Normal monster search
	{
		FActorIterator iterator (0);

		while ((mo = iterator.Next()) != NULL)
		{
			if (!(mo->flags3 & MF3_ISMONSTER)) continue;
			if (mo->health <= 0) continue;
			if (!(mo->flags & MF_SHOOTABLE)) continue;
			dist = P_AproxDistance (self->x - mo->x, self->y - mo->y);
			if (dist > MINOTAUR_LOOK_DIST) continue;
			if ((mo == master) || (mo == self)) continue;
			if ((mo->flags5 & MF5_SUMMONEDMONSTER) && (mo->tracer == master)) continue;
			self->target = mo;
			break;			// Found actor to attack
		}
	}

	if (self->target)
	{
		// [BC] If we're the server, set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_SEE );

		self->SetState (self->SeeState, true);
	}
	else
	{
		// [BC] If we're the server, set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState ("Roam") );

		self->SetState (self->FindState ("Roam"), true);
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_MinotaurChase)
{
	if (!self->IsKindOf(RUNTIME_CLASS(AMinotaurFriend)))
	{
		A_Chase (self);
		return;
	}

	AMinotaurFriend *self1 = static_cast<AMinotaurFriend *> (self);

	// In case pain caused him to skip his fade in.
	self1->RenderStyle = STYLE_Normal;

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		A_Chase( self );

		// Active sound
		if (pr_minotaurchase() < 6)
		{
			self->PlayActiveSound ();
		}

		return;
	}

	if (self1->StartTime >= 0 && (level.maptime - self1->StartTime) >= MAULATORTICS)
	{
		P_DamageMobj (self1, NULL, NULL, TELEFRAG_DAMAGE, NAME_None);
		return;
	}

	if (pr_minotaurchase() < 30)
		CALL_ACTION(A_MinotaurLook, self1);		// adjust to closest target

	if (!self1->target || (self1->target->health <= 0) ||
		!(self1->target->flags&MF_SHOOTABLE))
	{ // look for a new target
		// [BC] If we're the server, set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self1, STATE_IDLE );

		self1->SetIdle();
		return;
	}

	FaceMovementDirection (self1);
	self1->reactiontime = 0;

	// Melee attack
	if (self1->MeleeState && self1->CheckMeleeRange ())
	{
		if (self1->AttackSound)
		{
			S_Sound (self1, CHAN_WEAPON, self1->AttackSound, 1, ATTN_NORM, true);	// [BC] Inform the clients.
		}

		// [BC] If we're the server, update the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingState( self1, STATE_MELEE );
		}

		self1->SetState (self1->MeleeState);
		return;
	}

	// Missile attack
	if (self1->MissileState && P_CheckMissileRange(self1))
	{
		// [BC] If we're the server, update the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self1, STATE_MISSILE );

		self1->SetState (self1->MissileState);
		return;
	}

	// chase towards target
	if (!P_Move (self1))
	{
		P_NewChaseDir (self1);
		FaceMovementDirection (self1);
	}

	// Active sound
	if (pr_minotaurchase() < 6)
	{
		self1->PlayActiveSound ();
	}
}

