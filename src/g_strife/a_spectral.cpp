/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"
*/

AActor *P_SpawnSubMissile (AActor *source, const PClass *type, AActor *target);

class ASpectralMonster : public AActor
{
	DECLARE_CLASS (ASpectralMonster, AActor)
public:
	void Touch (AActor *toucher);
};

IMPLEMENT_CLASS (ASpectralMonster)

void ASpectralMonster::Touch (AActor *toucher)
{
	P_DamageMobj (toucher, this, this, 5, NAME_Melee);
}


DEFINE_ACTION_FUNCTION(AActor, A_SpectralLightningTail)
{
	AActor *foo = Spawn("SpectralLightningHTail", self->x - self->velx, self->y - self->vely, self->z, ALLOW_REPLACE);

	foo->angle = self->angle;
	foo->FriendPlayer = self->FriendPlayer;
}

DEFINE_ACTION_FUNCTION(AActor, A_SpectralBigBallLightning)
{
	const PClass *cls = PClass::FindClass("SpectralLightningH3");
	if (cls)
	{
		self->angle += ANGLE_90;
		P_SpawnSubMissile (self, cls, self->target);
		self->angle += ANGLE_180;
		P_SpawnSubMissile (self, cls, self->target);
		self->angle += ANGLE_90;
		P_SpawnSubMissile (self, cls, self->target);
	}
}

static FRandom pr_zap5 ("Zap5");

DEFINE_ACTION_FUNCTION(AActor, A_SpectralLightning)
{
	AActor *flash;
	fixed_t x, y;

	// [CW] Clients may not do this.
	if ( NETWORK_InClientMode() )
		return;

	if (self->threshold != 0)
		--self->threshold;

	self->velx += pr_zap5.Random2(3) << FRACBITS;
	self->vely += pr_zap5.Random2(3) << FRACBITS;

	// [CW] Tell clients to spawn the actor.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThing( self, CM_VELX|CM_VELY );

	x = self->x + pr_zap5.Random2(3) * FRACUNIT * 50;
	y = self->y + pr_zap5.Random2(3) * FRACUNIT * 50;

	flash = Spawn (self->threshold > 25 ? PClass::FindClass(NAME_SpectralLightningV2) :
		PClass::FindClass(NAME_SpectralLightningV1), x, y, ONCEILINGZ, ALLOW_REPLACE);

	flash->target = self->target;
	flash->velz = -18*FRACUNIT;
	flash->FriendPlayer = self->FriendPlayer;

	flash = Spawn(NAME_SpectralLightningV2, self->x, self->y, ONCEILINGZ, ALLOW_REPLACE);

	// [CW] Tell clients to spawn the actor.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissile( flash );


	flash->target = self->target;
	flash->velz = -18*FRACUNIT;
	flash->FriendPlayer = self->FriendPlayer;

	// [CW] Tell clients to spawn the missile.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnMissile( flash );
}
// In Strife, this number is stored in the data segment, but it doesn't seem to be
// altered anywhere.
#define TRACEANGLE (0xe000000)

DEFINE_ACTION_FUNCTION(AActor, A_Tracer2)
{
	AActor *dest;
	angle_t exact;
	fixed_t dist;
	fixed_t slope;

	// [BC] Server takes care of movement.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	dest = self->tracer;

	if (!dest || dest->health <= 0 || self->Speed == 0 || !self->CanSeek(dest))
		return;

	// change angle
	exact = R_PointToAngle2 (self->x, self->y, dest->x, dest->y);

	if (exact != self->angle)
	{
		if (exact - self->angle > 0x80000000)
		{
			self->angle -= TRACEANGLE;
			if (exact - self->angle < 0x80000000)
				self->angle = exact;
		}
		else
		{
			self->angle += TRACEANGLE;
			if (exact - self->angle > 0x80000000)
				self->angle = exact;
		}
	}

	exact = self->angle >> ANGLETOFINESHIFT;
	self->velx = FixedMul (self->Speed, finecosine[exact]);
	self->vely = FixedMul (self->Speed, finesine[exact]);

	if (!(self->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
	{
		// change slope
		dist = P_AproxDistance (dest->x - self->x, dest->y - self->y);
		dist /= self->Speed;

		if (dist < 1)
		{
			dist = 1;
		}
		if (dest->height >= 56*FRACUNIT)
		{
			slope = (dest->z+40*FRACUNIT - self->z) / dist;
		}
		else
		{
			slope = (dest->z + self->height*2/3 - self->z) / dist;
		}
		if (slope < self->velz)
		{
			self->velz -= FRACUNIT/8;
		}
		else
		{
			self->velz += FRACUNIT/8;
		}
	}

	// [BC] Update the thing's position, angle and velocity.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThing( self, CM_X|CM_Y|CM_Z|CM_ANGLE|CM_VELX|CM_VELY|CM_VELZ );
}
