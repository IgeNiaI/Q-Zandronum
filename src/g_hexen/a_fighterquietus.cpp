/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_quietusdrop ("QuietusDrop");

//==========================================================================

//============================================================================
//
// A_DropQuietusPieces
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DropWeaponPieces)
{
	ACTION_PARAM_START(3);
	ACTION_PARAM_CLASS(p1, 0);
	ACTION_PARAM_CLASS(p2, 1);
	ACTION_PARAM_CLASS(p3, 2);

	for (int i = 0, j = 0, fineang = 0; i < 3; ++i)
	{
		const PClass *cls = j==0? p1 : j==1? p2 : p3;
		if (cls)
		{
			AActor *piece = Spawn (cls, self->x, self->y, self->z, ALLOW_REPLACE);
			if (piece != NULL)
			{
				piece->velx = self->velx + finecosine[fineang];
				piece->vely = self->vely + finesine[fineang];
				piece->velz = self->velz;
				piece->flags |= MF_DROPPED;
				fineang += FINEANGLES/3;
				j = (j == 0) ? (pr_quietusdrop() & 1) + 1 : 3-j;
			}
		}
	}
}



// Fighter Sword Missile ----------------------------------------------------

class AFSwordMissile : public AActor
{
	DECLARE_CLASS (AFSwordMissile, AActor)
public:
	int DoSpecialDamage(AActor *victim, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AFSwordMissile)

int AFSwordMissile::DoSpecialDamage(AActor *victim, int damage, FName damagetype)
{
	if (victim->player)
	{
		damage -= damage >> 2;
	}
	return damage;
}

//============================================================================
//
// A_FSwordAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FSwordAttack)
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
	// [geNia] Unless clientside functions are allowed.
	if ( !NETWORK_ClientsideFunctionsAllowedOrIsServer( self ) )
	{
		S_Sound (self, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM);
		return;
	}

	P_SpawnPlayerMissile (self, 0, 0, -10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/4);
	P_SpawnPlayerMissile (self, 0, 0,  -5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/8);
	P_SpawnPlayerMissile (self, 0, 0,   0,		   RUNTIME_CLASS(AFSwordMissile), self->angle);
	P_SpawnPlayerMissile (self, 0, 0,   5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/8);
	P_SpawnPlayerMissile (self, 0, 0,  10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/4);

	// [BC] Apply spread.
	if ( player->cheats2 & CF2_SPREAD )
	{
		P_SpawnPlayerMissile (self, 0, 0, -10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/4 + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  -5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/8 + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   0,		   RUNTIME_CLASS(AFSwordMissile), self->angle + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/8 + ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/4 + ( ANGLE_45 / 3 ));

		P_SpawnPlayerMissile (self, 0, 0, -10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/4 - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  -5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle+ANGLE_45/8 - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   0,		   RUNTIME_CLASS(AFSwordMissile), self->angle - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,   5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/8 - ( ANGLE_45 / 3 ));
		P_SpawnPlayerMissile (self, 0, 0,  10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), self->angle-ANGLE_45/4 - ( ANGLE_45 / 3 ));
	}

	S_Sound (self, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM, true);
}

//============================================================================
//
// A_FSwordFlames
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FSwordFlames)
{
	int i;

	for (i = 1+(self->actorRandom()&3); i; i--)
	{
		fixed_t x = self->x+((self->actorRandom()-128)<<12);
		fixed_t y = self->y+((self->actorRandom()-128)<<12);
		fixed_t z = self->z+((self->actorRandom()-128)<<11);
		Spawn ("FSwordFlame", x, y, z, ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_FighterAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FighterAttack)
{
	// [Dusk] Zedek's attack is handled by the server
	// [geNia] Unless clientside functions are allowed.
	if ( !NETWORK_ClientsideFunctionsAllowedOrIsServer( self ) )
		return;

	if (!self->target) return;

	angle_t angle = self->angle;

	// [Dusk] Using a for-loop to avoid a copy/paste nightmare here.
	/*
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle+ANG45/4, 0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle+ANG45/8, 0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle,         0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle-ANG45/8, 0);
	P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle-ANG45/4, 0);
	*/

	for (int i = -2; i <= 2; i++) {
		P_SpawnMissileAngle (self, RUNTIME_CLASS(AFSwordMissile), angle + (i*ANG45)/8, 0, true); // [BB] Inform clients
	}

	S_Sound (self, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM, true);	// [TP] Inform the clients.
}

