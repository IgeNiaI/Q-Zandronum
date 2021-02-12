//===========================================================================
// Korax Variables
//	tracer		last teleport destination
//	special2	set if "below half" script not yet run
//
// Korax Scripts (reserved)
//	249		Tell scripts that we are below half health
//	250-254	Control scripts (254 is only used when less than half health)
//	255		Death script
//
// Korax TIDs (reserved)
//	245		Reserved for Korax himself
//  248		Initial teleport destination
//	249		Teleport destination
//	250-254	For use in respective control scripts
//	255		For use in death script (spawn spots)
//===========================================================================

/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_spec.h"
#include "s_sound.h"
#include "a_action.h"
#include "m_random.h"
#include "i_system.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

const int KORAX_SPIRIT_LIFETIME = 5*TICRATE/5;	// 5 seconds
const int KORAX_COMMAND_HEIGHT	= 120;
const int KORAX_COMMAND_OFFSET	= 27;

const int KORAX_TID					= 245;
const int KORAX_FIRST_TELEPORT_TID	= 248;
const int KORAX_TELEPORT_TID		= 249;

const int KORAX_DELTAANGLE			= 85*ANGLE_1;
const int KORAX_ARM_EXTENSION_SHORT	= 40;
const int KORAX_ARM_EXTENSION_LONG	= 55;

const int KORAX_ARM1_HEIGHT			= 108*FRACUNIT;
const int KORAX_ARM2_HEIGHT			= 82*FRACUNIT;
const int KORAX_ARM3_HEIGHT			= 54*FRACUNIT;
const int KORAX_ARM4_HEIGHT			= 104*FRACUNIT;
const int KORAX_ARM5_HEIGHT			= 86*FRACUNIT;
const int KORAX_ARM6_HEIGHT			= 53*FRACUNIT;

const int KORAX_BOLT_HEIGHT			= 48*FRACUNIT;
const int KORAX_BOLT_LIFETIME		= 3;



static FRandom pr_koraxchase ("KoraxChase");
static FRandom pr_kspiritinit ("KSpiritInit");
static FRandom pr_koraxdecide ("KoraxDecide");
static FRandom pr_koraxmissile ("KoraxMissile");
static FRandom pr_koraxcommand ("KoraxCommand");
static FRandom pr_kspiritweave ("KSpiritWeave");
static FRandom pr_kspiritseek ("KSpiritSeek");
static FRandom pr_kspiritroam ("KSpiritRoam");
static FRandom pr_kmissile ("SKoraxMissile");

void A_KoraxChase (AActor *);
void A_KoraxStep (AActor *);
void A_KoraxStep2 (AActor *);
void A_KoraxDecide (AActor *);
void A_KoraxBonePop (AActor *);
void A_KoraxMissile (AActor *);
void A_KoraxCommand (AActor *);
void A_KSpiritRoam (AActor *);
void A_KBolt (AActor *);
void A_KBoltRaise (AActor *);

void KoraxFire (AActor *actor, const PClass *type, int arm);
void KSpiritInit (AActor *spirit, AActor *korax);
AActor *P_SpawnKoraxMissile (fixed_t x, fixed_t y, fixed_t z,
	AActor *source, AActor *dest, const PClass *type);

extern void SpawnSpiritTail (AActor *spirit);

//============================================================================
//
// A_KoraxChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxChase)
{
	AActor *spot;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		if (pr_koraxchase()<30)
		{
			S_Sound (self, CHAN_VOICE, "KoraxActive", 1, ATTN_NONE);
		}
		return;
	}

	if ((!self->special2) && (self->health <= (self->SpawnHealth()/2)))
	{
		FActorIterator iterator (KORAX_FIRST_TELEPORT_TID);
		spot = iterator.Next ();
		if (spot != NULL)
		{
			P_Teleport (self, spot->x, spot->y, ONFLOORZ, spot->angle, false, true, true, false);
		}

		P_StartScript (self, NULL, 249, NULL, NULL, 0, 0);
		self->special2 = 1;	// Don't run again

		return;
	}

	if (!self->target) return;
	if (pr_koraxchase()<30)
	{
		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingState( self, STATE_MISSILE );

		self->SetState (self->MissileState);
	}
	else if (pr_koraxchase()<30)
	{
		S_Sound (self, CHAN_VOICE, "KoraxActive", 1, ATTN_NONE);
	}

	// Teleport away
	if (self->health < (self->SpawnHealth()>>1))
	{
		if (pr_koraxchase()<10)
		{
			FActorIterator iterator (KORAX_TELEPORT_TID);

			if (self->tracer != NULL)
			{	// Find the previous teleport destination
				do
				{
					spot = iterator.Next ();
				} while (spot != NULL && spot != self->tracer);
			}

			// Go to the next teleport destination
			spot = iterator.Next ();
			self->tracer = spot;
			if (spot)
			{
				P_Teleport (self, spot->x, spot->y, ONFLOORZ, spot->angle, false, true, true, false);
			}
		}
	}
}

//============================================================================
//
// A_KoraxBonePop
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxBonePop)
{
	AActor *mo;
	int i;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Spawn 6 spirits equalangularly
	for (i = 0; i < 6; ++i)
	{
		mo = P_SpawnMissileAngle (self, PClass::FindClass("KoraxSpirit"), ANGLE_60*i, 5*FRACUNIT, true); // [BB] Inform clients
		if (mo) KSpiritInit (mo, self);
	}

	P_StartScript (self, NULL, 255, NULL, NULL, 0, 0);		// Death script
}

//============================================================================
//
// KSpiritInit
//
//============================================================================

void KSpiritInit (AActor *spirit, AActor *korax)
{
	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	spirit->health = KORAX_SPIRIT_LIFETIME;

	spirit->tracer = korax;						// Swarm around korax
	spirit->special2 = FINEANGLES/2 + pr_kspiritinit(8 << BOBTOFINESHIFT);	// Float bob index
	spirit->args[0] = 10; 						// initial turn value
	spirit->args[1] = 0; 						// initial look angle

	// Spawn a tail for spirit
	SpawnSpiritTail (spirit);
}

//============================================================================
//
// A_KoraxDecide
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxDecide)
{
	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if (pr_koraxdecide()<220)
	{
		// [BC] Set the thing's frame.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Attack") );

		self->SetState (self->FindState("Attack"));
	}
	else
	{
		// [BC] Set the thing's frame.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Command") );

		self->SetState (self->FindState("Command"));
	}
}

//============================================================================
//
// A_KoraxMissile
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxMissile)
{
	static const struct { const char *type, *sound; } choices[6] =
	{
		{ "WraithFX1", "WraithMissileFire" },
		{ "Demon1FX1", "DemonMissileFire" },
		{ "Demon2FX1", "DemonMissileFire" },
		{ "FireDemonMissile", "FireDemonAttack" },
		{ "CentaurFX", "CentaurLeaderAttack" },
		{ "SerpentFX", "CentaurLeaderAttack" }
	};

	int type = pr_koraxmissile()%6;
	int i;
	const PClass *info;

	S_Sound (self, CHAN_VOICE, "KoraxAttack", 1, ATTN_NORM);

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		S_Sound (self, CHAN_WEAPON, choices[type].sound, 1, ATTN_NONE);
		return;
	}

	info = PClass::FindClass (choices[type].type);
	if (info == NULL)
	{
		I_Error ("Unknown Korax missile: %s\n", choices[type].type);
	}

	// Fire all 6 missiles at once
	S_Sound (self, CHAN_WEAPON, choices[type].sound, 1, ATTN_NONE);
	for (i = 0; i < 6; ++i)
	{
		KoraxFire (self, info, i);
	}
}

//============================================================================
//
// A_KoraxCommand
//
// Call action code scripts (250-254)
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxCommand)
{
	fixed_t x,y,z;
	angle_t ang;
	int numcommands;
	// [BC]
	AActor	*pBolt;

	S_Sound (self, CHAN_VOICE, "KoraxCommand", 1, ATTN_NORM);

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Shoot stream of lightning to ceiling
	ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
	x = self->x + KORAX_COMMAND_OFFSET * finecosine[ang];
	y = self->y + KORAX_COMMAND_OFFSET * finesine[ang];
	z = self->z + KORAX_COMMAND_HEIGHT*FRACUNIT;
	pBolt = Spawn("KoraxBolt", x, y, z, ALLOW_REPLACE);

	// [BC] Spawn the bolt to clients.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		( pBolt ))
	{
		SERVERCOMMANDS_SpawnThing( pBolt );
	}

	if (self->health <= (self->SpawnHealth() >> 1))
	{
		numcommands = 5;
	}
	else
	{
		numcommands = 4;
	}

	P_StartScript (self, NULL, 250+(pr_koraxcommand()%numcommands), NULL, NULL, 0, 0);
}

//============================================================================
//
// KoraxFire
//
// Arm projectiles
//		arm positions numbered:
//			1	top left
//			2	middle left
//			3	lower left
//			4	top right
//			5	middle right
//			6	lower right
//
//============================================================================

void KoraxFire (AActor *actor, const PClass *type, int arm)
{
	static const int extension[6] =
	{
		KORAX_ARM_EXTENSION_SHORT,
		KORAX_ARM_EXTENSION_LONG,
		KORAX_ARM_EXTENSION_LONG,
		KORAX_ARM_EXTENSION_SHORT,
		KORAX_ARM_EXTENSION_LONG,
		KORAX_ARM_EXTENSION_LONG
	};
	static const fixed_t armheight[6] =
	{
		KORAX_ARM1_HEIGHT,
		KORAX_ARM2_HEIGHT,
		KORAX_ARM3_HEIGHT,
		KORAX_ARM4_HEIGHT,
		KORAX_ARM5_HEIGHT,
		KORAX_ARM6_HEIGHT
	};

	angle_t ang;
	fixed_t x,y,z;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	ang = (actor->angle + (arm < 3 ? -KORAX_DELTAANGLE : KORAX_DELTAANGLE))
		>> ANGLETOFINESHIFT;
	x = actor->x + extension[arm] * finecosine[ang];
	y = actor->y + extension[arm] * finesine[ang];
	z = actor->z - actor->floorclip + armheight[arm];
	P_SpawnKoraxMissile (x, y, z, actor, actor->target, type);
}

//============================================================================
//
// A_KSpiritWeave
// [BL] Was identical to CHolyWeave so lets just use that
//
//============================================================================

void CHolyWeave (AActor *actor, FRandom &pr_random);

//============================================================================
//
// A_KSpiritSeeker
//
//============================================================================

void A_KSpiritSeeker (AActor *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;
	fixed_t newZ;
	fixed_t deltaZ;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	target = actor->tracer;
	if (target == NULL)
	{
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if(delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if(dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->velx = FixedMul (actor->Speed, finecosine[angle]);
	actor->vely = FixedMul (actor->Speed, finesine[angle]);

	if (!(level.time&15) 
		|| actor->z > target->z+(target->GetDefault()->height)
		|| actor->z+actor->height < target->z)
	{
		newZ = target->z+((pr_kspiritseek()*target->GetDefault()->height)>>8);
		deltaZ = newZ-actor->z;
		if (abs(deltaZ) > 15*FRACUNIT)
		{
			if(deltaZ > 0)
			{
				deltaZ = 15*FRACUNIT;
			}
			else
			{
				deltaZ = -15*FRACUNIT;
			}
		}
		dist = P_AproxDistance (target->x-actor->x, target->y-actor->y);
		dist = dist/actor->Speed;
		if (dist < 1)
		{
			dist = 1;
		}
		actor->velz = deltaZ/dist;
	}

	// [BC] Move the thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThing( actor, CM_X|CM_Y|CM_Z|CM_ANGLE|CM_VELX|CM_VELY|CM_VELZ );

	return;
}

//============================================================================
//
// A_KSpiritRoam
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KSpiritRoam)
{
	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		if (pr_kspiritroam()<50)
		{
			S_Sound (self, CHAN_VOICE, "SpiritActive", 1, ATTN_NONE);
		}

		return;
	}

	if (self->health-- <= 0)
	{
		S_Sound (self, CHAN_VOICE, "SpiritDie", 1, ATTN_NORM, true);	// [BC] Inform the clients.

		// [BC] Set the thing's state.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetThingFrame( self, self->FindState("Death") );
		}

		self->SetState (self->FindState("Death"));
	}
	else
	{
		if (self->tracer)
		{
			A_KSpiritSeeker (self, self->args[0]*ANGLE_1,
							 self->args[0]*ANGLE_1*2);
		}
		CHolyWeave(self, pr_kspiritweave);
		if (pr_kspiritroam()<50)
		{
			S_Sound (self, CHAN_VOICE, "SpiritActive", 1, ATTN_NONE);
		}
	}
}

//============================================================================
//
// A_KBolt
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KBolt)
{
	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Countdown lifetime
	if (self->special1-- <= 0)
	{
		// [BC] Destroy the thing.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( self );

		self->Destroy ();
	}
}

//============================================================================
//
// A_KBoltRaise
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KBoltRaise)
{
	AActor *mo;
	fixed_t z;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	// Spawn a child upward
	z = self->z + KORAX_BOLT_HEIGHT;

	if ((z + KORAX_BOLT_HEIGHT) < self->ceilingz)
	{
		mo = Spawn("KoraxBolt", self->x, self->y, z, ALLOW_REPLACE);
		if (mo)
		{
			mo->special1 = KORAX_BOLT_LIFETIME;

			// [BC] Spawn it to clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( mo );
		}
	}
	else
	{
		// Maybe cap it off here
	}
}

//============================================================================
//
// P_SpawnKoraxMissile
//
//============================================================================

AActor *P_SpawnKoraxMissile (fixed_t x, fixed_t y, fixed_t z,
	AActor *source, AActor *dest, const PClass *type)
{
	AActor *th;
	angle_t an;
	int dist;

	// [BC] Let the server do this.
	if ( NETWORK_InClientMode() )
	{
		return ( NULL );
	}

	z -= source->floorclip;
	th = Spawn (type, x, y, z, ALLOW_REPLACE);
	th->target = source; // Originator
	an = R_PointToAngle2(x, y, dest->x, dest->y);
	if (dest->flags & MF_SHADOW)
	{ // Invisible target
		an += pr_kmissile.Random2()<<21;
	}
	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	th->velx = FixedMul (th->Speed, finecosine[an]);
	th->vely = FixedMul (th->Speed, finesine[an]);
	dist = P_AproxDistance (dest->x - x, dest->y - y);
	dist = dist/th->Speed;
	if (dist < 1)
	{
		dist = 1;
	}
	th->velz = (dest->z-z+(30*FRACUNIT))/dist;

	// [BC] Spawn this missile to clients.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissile( th );

	return (P_CheckMissileSpawn(th, source->radius) ? th : NULL);
}
