/*
** thingdef_data.cpp
**
** DECORATE data tables
**
**---------------------------------------------------------------------------
** Copyright 2002-2008 Christoph Oelckers
** Copyright 2004-2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "thingdef.h"
#include "actor.h"
#include "d_player.h"
#include "p_effect.h"
#include "autosegs.h"

static TArray<FPropertyInfo*> properties;
static TArray<AFuncDesc> AFTable;
static TArray<FVariableInfo*> variables;

//==========================================================================
//
// List of all flags
//
//==========================================================================

// [RH] Keep GCC quiet by not using offsetof on Actor types.
#define DEFINE_FLAG(prefix, name, type, variable) { (unsigned int)prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable) }
#define DEFINE_FLAG2(symbol, name, type, variable) { (unsigned int)symbol, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable) }
#define DEFINE_DEPRECATED_FLAG(name) { DEPF_##name, #name, -1, 0 }
#define DEFINE_DUMMY_FLAG(name) { DEPF_UNUSED, #name, -1, 0 }

static FFlagDef ActorFlags[]=
{
	DEFINE_FLAG(MF, PICKUP, APlayerPawn, flags),
	DEFINE_FLAG(MF, SPECIAL, APlayerPawn, flags),
	DEFINE_FLAG(MF, SOLID, AActor, flags),
	DEFINE_FLAG(MF, SHOOTABLE, AActor, flags),
	DEFINE_FLAG(MF, NOSECTOR, AActor, flags),
	DEFINE_FLAG(MF, NOBLOCKMAP, AActor, flags),
	DEFINE_FLAG(MF, AMBUSH, AActor, flags),
	DEFINE_FLAG(MF, JUSTHIT, AActor, flags),
	DEFINE_FLAG(MF, JUSTATTACKED, AActor, flags),
	DEFINE_FLAG(MF, SPAWNCEILING, AActor, flags),
	DEFINE_FLAG(MF, NOGRAVITY, AActor, flags),
	DEFINE_FLAG(MF, DROPOFF, AActor, flags),
	DEFINE_FLAG(MF, NOCLIP, AActor, flags),
	DEFINE_FLAG(MF, FLOAT, AActor, flags),
	DEFINE_FLAG(MF, TELEPORT, AActor, flags),
	DEFINE_FLAG(MF, MISSILE, AActor, flags),
	DEFINE_FLAG(MF, DROPPED, AActor, flags),
	DEFINE_FLAG(MF, SHADOW, AActor, flags),
	DEFINE_FLAG(MF, NOBLOOD, AActor, flags),
	DEFINE_FLAG(MF, CORPSE, AActor, flags),
	DEFINE_FLAG(MF, INFLOAT, AActor, flags),
	DEFINE_FLAG(MF, COUNTKILL, AActor, flags),
	DEFINE_FLAG(MF, COUNTITEM, AActor, flags),
	DEFINE_FLAG(MF, SKULLFLY, AActor, flags),
	DEFINE_FLAG(MF, NOTDMATCH, AActor, flags),
	DEFINE_FLAG(MF, SPAWNSOUNDSOURCE, AActor, flags),
	DEFINE_FLAG(MF, FRIENDLY, AActor, flags),
	DEFINE_FLAG(MF, NOLIFTDROP, AActor, flags),
	DEFINE_FLAG(MF, STEALTH, AActor, flags),
	DEFINE_FLAG(MF, ICECORPSE, AActor, flags),

	DEFINE_FLAG(MF2, DONTREFLECT, AActor, flags2),
	DEFINE_FLAG(MF2, WINDTHRUST, AActor, flags2),
	DEFINE_FLAG(MF2, DONTSEEKINVISIBLE, AActor, flags2),
	DEFINE_FLAG(MF2, BLASTED, AActor, flags2),
	DEFINE_FLAG(MF2, FLOORCLIP, AActor, flags2),
	DEFINE_FLAG(MF2, SPAWNFLOAT, AActor, flags2),
	DEFINE_FLAG(MF2, NOTELEPORT, AActor, flags2),
	DEFINE_FLAG2(MF2_RIP, RIPPER, AActor, flags2),
	DEFINE_FLAG(MF2, PUSHABLE, AActor, flags2),
	DEFINE_FLAG2(MF2_SLIDE, SLIDESONWALLS, AActor, flags2),
	DEFINE_FLAG2(MF2_PASSMOBJ, CANPASS, AActor, flags2),
	DEFINE_FLAG(MF2, CANNOTPUSH, AActor, flags2),
	DEFINE_FLAG(MF2, THRUGHOST, AActor, flags2),
	DEFINE_FLAG(MF2, BOSS, AActor, flags2),
	DEFINE_FLAG2(MF2_NODMGTHRUST, NODAMAGETHRUST, AActor, flags2),
	DEFINE_FLAG(MF2, DONTTRANSLATE, AActor, flags2),
	DEFINE_FLAG(MF2, TELESTOMP, AActor, flags2),
	DEFINE_FLAG(MF2, FLOATBOB, AActor, flags2),
	DEFINE_FLAG(MF2, THRUACTORS, AActor, flags2),
	DEFINE_FLAG2(MF2_IMPACT, ACTIVATEIMPACT, AActor, flags2),
	DEFINE_FLAG2(MF2_PUSHWALL, CANPUSHWALLS, AActor, flags2),
	DEFINE_FLAG2(MF2_MCROSS, ACTIVATEMCROSS, AActor, flags2),
	DEFINE_FLAG2(MF2_PCROSS, ACTIVATEPCROSS, AActor, flags2),
	DEFINE_FLAG(MF2, CANTLEAVEFLOORPIC, AActor, flags2),
	DEFINE_FLAG(MF2, NONSHOOTABLE, AActor, flags2),
	DEFINE_FLAG(MF2, INVULNERABLE, AActor, flags2),
	DEFINE_FLAG(MF2, DORMANT, AActor, flags2),
	DEFINE_FLAG(MF2, SEEKERMISSILE, AActor, flags2),
	DEFINE_FLAG(MF2, REFLECTIVE, AActor, flags2),

	DEFINE_FLAG(MF3, FLOORHUGGER, AActor, flags3),
	DEFINE_FLAG(MF3, CEILINGHUGGER, AActor, flags3),
	DEFINE_FLAG(MF3, NORADIUSDMG, AActor, flags3),
	DEFINE_FLAG(MF3, GHOST, AActor, flags3),
	DEFINE_FLAG(MF3, SPECIALFLOORCLIP, AActor, flags3),
	DEFINE_FLAG(MF3, ALWAYSPUFF, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSPLASH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTOVERLAP, AActor, flags3),
	DEFINE_FLAG(MF3, DONTMORPH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSQUASH, AActor, flags3),
	DEFINE_FLAG(MF3, EXPLOCOUNT, AActor, flags3),
	DEFINE_FLAG(MF3, FULLVOLACTIVE, AActor, flags3),
	DEFINE_FLAG(MF3, ISMONSTER, AActor, flags3),
	DEFINE_FLAG(MF3, SKYEXPLODE, AActor, flags3),
	DEFINE_FLAG(MF3, STAYMORPHED, AActor, flags3),
	DEFINE_FLAG(MF3, DONTBLAST, AActor, flags3),
	DEFINE_FLAG(MF3, CANBLAST, AActor, flags3),
	DEFINE_FLAG(MF3, NOTARGET, AActor, flags3),
	DEFINE_FLAG(MF3, DONTGIB, AActor, flags3),
	DEFINE_FLAG(MF3, NOBLOCKMONST, AActor, flags3),
	DEFINE_FLAG(MF3, FULLVOLDEATH, AActor, flags3),
	DEFINE_FLAG(MF3, AVOIDMELEE, AActor, flags3),
	DEFINE_FLAG(MF3, SCREENSEEKER, AActor, flags3),
	DEFINE_FLAG(MF3, FOILINVUL, AActor, flags3),
	DEFINE_FLAG(MF3, NOTELEOTHER, AActor, flags3),
	DEFINE_FLAG(MF3, BLOODLESSIMPACT, AActor, flags3),
	DEFINE_FLAG(MF3, NOEXPLODEFLOOR, AActor, flags3),
	DEFINE_FLAG(MF3, PUFFONACTORS, AActor, flags3),

	DEFINE_FLAG(MF4, QUICKTORETALIATE, AActor, flags4),
	DEFINE_FLAG(MF4, NOICEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, RANDOMIZE, AActor, flags4),
	DEFINE_FLAG(MF4, FIXMAPTHINGPOS , AActor, flags4),
	DEFINE_FLAG(MF4, ACTLIKEBRIDGE, AActor, flags4),
	DEFINE_FLAG(MF4, STRIFEDAMAGE, AActor, flags4),
	DEFINE_FLAG(MF4, CANUSEWALLS, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEMORE, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEEVENMORE, AActor, flags4),
	DEFINE_FLAG(MF4, FORCERADIUSDMG, AActor, flags4),
	DEFINE_FLAG(MF4, DONTFALL, AActor, flags4),
	DEFINE_FLAG(MF4, SEESDAGGERS, AActor, flags4),
	DEFINE_FLAG(MF4, INCOMBAT, AActor, flags4),
	DEFINE_FLAG(MF4, LOOKALLAROUND, AActor, flags4),
	DEFINE_FLAG(MF4, STANDSTILL, AActor, flags4),
	DEFINE_FLAG(MF4, SPECTRAL, AActor, flags4),
	DEFINE_FLAG(MF4, NOSPLASHALERT, AActor, flags4),
	DEFINE_FLAG(MF4, SYNCHRONIZED, AActor, flags4),
	DEFINE_FLAG(MF4, NOTARGETSWITCH, AActor, flags4),
	DEFINE_FLAG(MF4, DONTHARMCLASS, AActor, flags4),
	DEFINE_FLAG2(MF4_DONTHARMCLASS, DONTHURTSPECIES, AActor, flags4), // Deprecated name as an alias
	DEFINE_FLAG(MF4, SHIELDREFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, DEFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, ALLOWPARTICLES, AActor, flags4),
	DEFINE_FLAG(MF4, EXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, NOEXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, FRIGHTENED, AActor, flags4),
	DEFINE_FLAG(MF4, NOSKIN, AActor, flags4),
	DEFINE_FLAG(MF4, BOSSDEATH, AActor, flags4),

	DEFINE_FLAG(MF5, DONTDRAIN, AActor, flags5),
	DEFINE_FLAG(MF5, NODROPOFF, AActor, flags5),
	DEFINE_FLAG(MF5, NOFORWARDFALL, AActor, flags5),
	DEFINE_FLAG(MF5, COUNTSECRET, AActor, flags5),
	DEFINE_FLAG(MF5, NODAMAGE, AActor, flags5),
	DEFINE_FLAG(MF5, BLOODSPLATTER, AActor, flags5),
	DEFINE_FLAG(MF5, OLDRADIUSDMG, AActor, flags5),
	DEFINE_FLAG(MF5, DEHEXPLOSION, AActor, flags5),
	DEFINE_FLAG(MF5, PIERCEARMOR, AActor, flags5),
	DEFINE_FLAG(MF5, NOBLOODDECALS, AActor, flags5),
	DEFINE_FLAG(MF5, USESPECIAL, AActor, flags5),
	DEFINE_FLAG(MF5, NOPAIN, AActor, flags5),
	DEFINE_FLAG(MF5, ALWAYSFAST, AActor, flags5),
	DEFINE_FLAG(MF5, NEVERFAST, AActor, flags5),
	DEFINE_FLAG(MF5, ALWAYSRESPAWN, AActor, flags5),
	DEFINE_FLAG(MF5, NEVERRESPAWN, AActor, flags5),
	DEFINE_FLAG(MF5, DONTRIP, AActor, flags5),
	DEFINE_FLAG(MF5, NOINFIGHTING, AActor, flags5),
	DEFINE_FLAG(MF5, NOINTERACTION, AActor, flags5),
	DEFINE_FLAG(MF5, NOTIMEFREEZE, AActor, flags5),
	DEFINE_FLAG(MF5, PUFFGETSOWNER, AActor, flags5), // [BB] added PUFFGETSOWNER
	DEFINE_FLAG(MF5, SPECIALFIREDAMAGE, AActor, flags5),
	DEFINE_FLAG(MF5, SUMMONEDMONSTER, AActor, flags5),
	DEFINE_FLAG(MF5, NOVERTICALMELEERANGE, AActor, flags5),
	DEFINE_FLAG(MF5, BRIGHT, AActor, flags5),
	DEFINE_FLAG(MF5, CANTSEEK, AActor, flags5),
	DEFINE_FLAG(MF5, PAINLESS, AActor, flags5),
	DEFINE_FLAG(MF5, MOVEWITHSECTOR, AActor, flags5),

	DEFINE_FLAG(MF6, NOBOSSRIP, AActor, flags6),
	DEFINE_FLAG(MF6, THRUSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, MTHRUSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, FORCEPAIN, AActor, flags6),
	DEFINE_FLAG(MF6, NOFEAR, AActor, flags6),
	DEFINE_FLAG(MF6, BUMPSPECIAL, AActor, flags6),
	DEFINE_FLAG(MF6, DONTHARMSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, STEPMISSILE, AActor, flags6),
	DEFINE_FLAG(MF6, NOTELEFRAG, AActor, flags6),
	DEFINE_FLAG(MF6, TOUCHY, AActor, flags6),
	DEFINE_FLAG(MF6, CANJUMP, AActor, flags6),
	DEFINE_FLAG(MF6, JUMPDOWN, AActor, flags6),
	DEFINE_FLAG(MF6, VULNERABLE, AActor, flags6),
	DEFINE_FLAG(MF6, NOTRIGGER, AActor, flags6),
	DEFINE_FLAG(MF6, ADDITIVEPOISONDAMAGE, AActor, flags6),
	DEFINE_FLAG(MF6, ADDITIVEPOISONDURATION, AActor, flags6),
	DEFINE_FLAG(MF6, BLOCKEDBYSOLIDACTORS, AActor, flags6),
	DEFINE_FLAG(MF6, NOMENU, AActor, flags6),
	DEFINE_FLAG(MF6, SEEINVISIBLE, AActor, flags6),
	DEFINE_FLAG(MF6, DONTCORPSE, AActor, flags6),
	DEFINE_FLAG(MF6, DOHARMSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, POISONALWAYS, AActor, flags6),
	DEFINE_FLAG(MF6, NOTAUTOAIMED, AActor, flags6),
	DEFINE_FLAG(MF6, NOTONAUTOMAP, AActor, flags6),
	DEFINE_FLAG(MF6, RELATIVETOFLOOR, AActor, flags6),

	DEFINE_FLAG(MF7, NEVERTARGET, AActor, flags7),
	DEFINE_FLAG(MF7, NOTELESTOMP, AActor, flags7),
	DEFINE_FLAG(MF7, ALWAYSTELEFRAG, AActor, flags7),

	DEFINE_FLAG(MF7, HITTARGET, AActor, flags7),
	DEFINE_FLAG(MF7, HITMASTER, AActor, flags7),
	DEFINE_FLAG(MF7, HITTRACER, AActor, flags7),
	
	// [ZK] Decal flags
	DEFINE_FLAG(MF7, NODECAL, AActor, flags7),
	DEFINE_FLAG(MF7, FORCEDECAL, AActor, flags7),

	// [BB] Out of order ZDoom backport.
	DEFINE_FLAG(MF7, USEKILLSCRIPTS, AActor, flags7),
	DEFINE_FLAG(MF7, NOKILLSCRIPTS, AActor, flags7),
		
	// [geNia] New flags
	DEFINE_FLAG(MF8, NOEXPLODECEILING, AActor, flags8),
	DEFINE_FLAG(MF8, SEEKERMISSILENOZ, AActor, flags8),

	// [Ivory] extra movement flags
	DEFINE_FLAG(MV, CROUCHSLIDE, AActor, mvFlags),
	DEFINE_FLAG(MV, WALLJUMP, AActor, mvFlags),
	DEFINE_FLAG(MV, WALLJUMPV2, AActor, mvFlags),
	DEFINE_FLAG(MV, DOUBLETAPJUMP, AActor, mvFlags),
	DEFINE_FLAG(MV, WALLCLIMB, AActor, mvFlags),
	DEFINE_FLAG(MV, RAMPJUMP, AActor, mvFlags),
	DEFINE_FLAG(MV, SILENT, AActor, mvFlags),

	// [BC] New DECORATE flag defines here.
	DEFINE_FLAG(STFL, BLUETEAM, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, REDTEAM, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, BASEHEALTH, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, SUPERHEALTH, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, BASEARMOR, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, SUPERARMOR, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, SCOREPILLAR, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, NODE, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, USESTBOUNCESOUND, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, EXPLODEONDEATH, AActor, ulSTFlags),
	DEFINE_FLAG(STFL, DONTIDENTIFYTARGET, AActor, ulSTFlags), // [CK]

	// [BB] New DECORATE network related flag defines here.
	DEFINE_FLAG(NETFL, NONETID, AActor, ulNetworkFlags),
	DEFINE_FLAG(NETFL, ALLOWCLIENTSPAWN, AActor, ulNetworkFlags),
	DEFINE_FLAG(NETFL, CLIENTSIDEONLY, AActor, ulNetworkFlags),
	DEFINE_FLAG(NETFL, SERVERSIDEONLY, AActor, ulNetworkFlags),

	// Effect flags
	DEFINE_FLAG(FX, VISIBILITYPULSE, AActor, effects),
	DEFINE_FLAG2(FX_ROCKET, ROCKETTRAIL, AActor, effects),
	DEFINE_FLAG2(FX_GRENADE, GRENADETRAIL, AActor, effects),
	DEFINE_FLAG(RF, INVISIBLE, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEYBILLBOARD, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEXYBILLBOARD, AActor, renderflags),
	DEFINE_FLAG(RF, INVISIBLE_TO_TARGET, AActor, renderflags),

	// Bounce flags
	DEFINE_FLAG2(BOUNCE_Walls, BOUNCEONWALLS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Floors, BOUNCEONFLOORS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Ceilings, BOUNCEONCEILINGS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Actors, ALLOWBOUNCEONACTORS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_AutoOff, BOUNCEAUTOOFF, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_HereticType, BOUNCELIKEHERETIC, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_CanBounceWater, CANBOUNCEWATER, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_NoWallSound, NOWALLBOUNCESND, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Quiet, NOBOUNCESOUND, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_AllActors, BOUNCEONACTORS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_ExplodeOnWater, EXPLODEONWATER, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_MBF, MBFBOUNCER, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_AutoOffFloorOnly, BOUNCEAUTOOFFFLOORONLY, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_UseBounceState, USEBOUNCESTATE, AActor, BounceFlags),

	// Deprecated flags. Handling must be performed in HandleDeprecatedFlags
	DEFINE_DEPRECATED_FLAG(FIREDAMAGE),
	DEFINE_DEPRECATED_FLAG(ICEDAMAGE),
	DEFINE_DEPRECATED_FLAG(LOWGRAVITY),
	DEFINE_DEPRECATED_FLAG(SHORTMISSILERANGE),
	DEFINE_DEPRECATED_FLAG(LONGMELEERANGE),
	DEFINE_DEPRECATED_FLAG(QUARTERGRAVITY),
	DEFINE_DEPRECATED_FLAG(FIRERESIST),
	DEFINE_DEPRECATED_FLAG(HERETICBOUNCE),
	DEFINE_DEPRECATED_FLAG(HEXENBOUNCE),
	DEFINE_DEPRECATED_FLAG(DOOMBOUNCE),

	// Deprecated flags with no more existing functionality.
	DEFINE_DUMMY_FLAG(FASTER),				// obsolete, replaced by 'Fast' state flag
	DEFINE_DUMMY_FLAG(FASTMELEE),			// obsolete, replaced by 'Fast' state flag

	// [BB] ST supports these flags.
	/*
	// Various Skulltag flags that are quite irrelevant to ZDoom
	DEFINE_DUMMY_FLAG(NONETID),				// netcode-based
	DEFINE_DUMMY_FLAG(ALLOWCLIENTSPAWN),	// netcode-based
	DEFINE_DUMMY_FLAG(CLIENTSIDEONLY),	    // netcode-based
	DEFINE_DUMMY_FLAG(SERVERSIDEONLY),		// netcode-based
	DEFINE_DUMMY_FLAG(EXPLODEONDEATH),	    // seems useless
	*/
};

static FFlagDef InventoryFlags[] =
{
	// Inventory flags
	DEFINE_FLAG(IF, QUIET, AInventory, ItemFlags),
	DEFINE_FLAG(IF, AUTOACTIVATE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, UNDROPPABLE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, INVBAR, AInventory, ItemFlags),
	DEFINE_FLAG(IF, HUBPOWER, AInventory, ItemFlags),
	DEFINE_FLAG(IF, UNTOSSABLE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ADDITIVETIME, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ALWAYSPICKUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, FANCYPICKUPSOUND, AInventory, ItemFlags),
	DEFINE_FLAG(IF, BIGPOWERUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, KEEPDEPLETED, AInventory, ItemFlags),
	DEFINE_FLAG(IF, IGNORESKILL, AInventory, ItemFlags),
	DEFINE_FLAG(IF, NOATTENPICKUPSOUND, AInventory, ItemFlags),
	DEFINE_FLAG(IF, PERSISTENTPOWER, AInventory, ItemFlags),
	DEFINE_FLAG(IF, RESTRICTABSOLUTELY, AInventory, ItemFlags),
	DEFINE_FLAG(IF, NEVERRESPAWN, AInventory, ItemFlags),
	DEFINE_FLAG(IF, NOSCREENFLASH, AInventory, ItemFlags),
	DEFINE_FLAG(IF, TOSSED, AInventory, ItemFlags),
	// [BB] New ST flags.
	DEFINE_FLAG(IF, FORCERESPAWNINSURVIVAL, AInventory, ItemFlags),

	DEFINE_DEPRECATED_FLAG(PICKUPFLASH),
	DEFINE_DEPRECATED_FLAG(INTERHUBSTRIP),};

static FFlagDef WeaponFlags[] =
{
	// Weapon flags
	DEFINE_FLAG(WIF, NOAUTOFIRE, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NOPRIMAUTOFIRE, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NOALTAUTOFIRE, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, READYSNDHALF, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, DONTBOB, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AXEBLOOD, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NOALERT, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AMMO_OPTIONAL, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, ALT_AMMO_OPTIONAL, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, PRIMARY_USES_BOTH, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, WIMPY_WEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, POWERED_UP, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, STAFF2_KICKBACK, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, EXPLOSIVE, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, MELEEWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, BFG, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, CHEATNOTWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NO_AUTO_SWITCH, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AMMO_CHECKBOTH, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NOAUTOAIM, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, ALLOW_WITH_RESPAWN_INVUL, AWeapon, WeaponFlags), // [BB] Marks weapons that can be used while respawn invulnerability is active.
	DEFINE_FLAG(WIF, NOLMS, AWeapon, WeaponFlags), // [BB] Marks weapons that are not given to the player in LMS.
	DEFINE_FLAG(WIF, ALT_USES_BOTH, AWeapon, WeaponFlags),
};

static FFlagDef PlayerPawnFlags[] =
{
	// PlayerPawn flags
	DEFINE_FLAG(PPF, NOTHRUSTWHENINVUL, APlayerPawn, PlayerFlags),
	DEFINE_FLAG(PPF, CANSUPERMORPH, APlayerPawn, PlayerFlags),
	DEFINE_FLAG(PPF, CROUCHABLEMORPH, APlayerPawn, PlayerFlags),
};

static FFlagDef PowerSpeedFlags[] =
{
	// PowerSpeed flags
	DEFINE_FLAG(PSF, NOTRAIL, APowerSpeed, SpeedFlags),
};

static const struct FFlagList { const PClass *Type; FFlagDef *Defs; int NumDefs; } FlagLists[] =
{
	{ RUNTIME_CLASS(AActor), 		ActorFlags,		countof(ActorFlags) },
	{ RUNTIME_CLASS(AInventory), 	InventoryFlags,	countof(InventoryFlags) },
	{ RUNTIME_CLASS(AWeapon), 		WeaponFlags,	countof(WeaponFlags) },
	{ RUNTIME_CLASS(APlayerPawn),	PlayerPawnFlags,countof(PlayerPawnFlags) },
	{ RUNTIME_CLASS(APowerSpeed),	PowerSpeedFlags,countof(PowerSpeedFlags) },
};
#define NUM_FLAG_LISTS (countof(FlagLists))

//==========================================================================
//
// Find a flag by name using a binary search
//
//==========================================================================
static FFlagDef *FindFlag (FFlagDef *flags, int numflags, const char *flag)
{
	int min = 0, max = numflags - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (flag, flags[mid].name);
		if (lexval == 0)
		{
			return &flags[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

//==========================================================================
//
// Finds a flag that may have a qualified name
//
//==========================================================================

FFlagDef *FindFlag (const PClass *type, const char *part1, const char *part2)
{
	FFlagDef *def;
	size_t i;

	if (part2 == NULL)
	{ // Search all lists
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			if (type->IsDescendantOf (FlagLists[i].Type))
			{
				def = FindFlag (FlagLists[i].Defs, FlagLists[i].NumDefs, part1);
				if (def != NULL)
				{
					return def;
				}
			}
		}
	}
	else
	{ // Search just the named list
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			if (stricmp (FlagLists[i].Type->TypeName.GetChars(), part1) == 0)
			{
				if (type->IsDescendantOf (FlagLists[i].Type))
				{
					return FindFlag (FlagLists[i].Defs, FlagLists[i].NumDefs, part2);
				}
				else
				{
					return NULL;
				}
			}
		}
	}
	return NULL;
}


//==========================================================================
//
// Gets the name of an actor flag 
//
//==========================================================================

const char *GetFlagName(unsigned int flagnum, int flagoffset)
{
	for(unsigned i = 0; i < countof(ActorFlags); i++)
	{
		if (ActorFlags[i].flagbit == flagnum && ActorFlags[i].structoffset == flagoffset)
		{
			return ActorFlags[i].name;
		}
	}
	return "(unknown)";	// return something printable
}

//==========================================================================
//
// Find a property by name using a binary search
//
//==========================================================================

FPropertyInfo *FindProperty(const char * string)
{
	int min = 0, max = properties.Size()-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, properties[mid]->name);
		if (lexval == 0)
		{
			return properties[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

//==========================================================================
//
// Find a function by name using a binary search
//
//==========================================================================

AFuncDesc *FindFunction(const char * string)
{
	int min = 0, max = AFTable.Size()-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, AFTable[mid].Name);
		if (lexval == 0)
		{
			return &AFTable[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}


//==========================================================================
//
// Find a variable by name using a binary search
//
//==========================================================================

FVariableInfo *FindVariable(const char * string, const PClass *cls)
{
	int min = 0, max = variables.Size()-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval;
		
		if (cls < variables[mid]->owner) lexval = -1;
		else if (cls > variables[mid]->owner) lexval = 1;
		else lexval = stricmp (string, variables[mid]->name);

		if (lexval == 0)
		{
			return variables[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}


//==========================================================================
//
// Find an action function in AActor's table
//
//==========================================================================

PSymbolActionFunction *FindGlobalActionFunction(const char *name)
{
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol(name, false);
	if (sym != NULL && sym->SymbolType == SYM_ActionFunction)
		return static_cast<PSymbolActionFunction*>(sym);
	else
		return NULL;
}


//==========================================================================
//
// Sorting helpers
//
//==========================================================================

static int STACK_ARGS flagcmp (const void * a, const void * b)
{
	return stricmp( ((FFlagDef*)a)->name, ((FFlagDef*)b)->name);
}

static int STACK_ARGS propcmp(const void * a, const void * b)
{
	return stricmp( (*(FPropertyInfo**)a)->name, (*(FPropertyInfo**)b)->name);
}

static int STACK_ARGS funccmp(const void * a, const void * b)
{
	return stricmp( ((AFuncDesc*)a)->Name, ((AFuncDesc*)b)->Name);
}

static int STACK_ARGS varcmp(const void * a, const void * b)
{
	FVariableInfo *A = *(FVariableInfo**)a;
	FVariableInfo *B = *(FVariableInfo**)b;
	if (A->owner < B->owner) return -1;
	if (A->owner > B->owner) return 1;
	return stricmp(A->name, B->name);
}

//==========================================================================
//
// Initialization
//
//==========================================================================

void InitThingdef()
{
	// Sort the flag lists
	for (size_t i = 0; i < NUM_FLAG_LISTS; ++i)
	{
		qsort (FlagLists[i].Defs, FlagLists[i].NumDefs, sizeof(FFlagDef), flagcmp);
	}

	// Create a sorted list of properties
	if (properties.Size() == 0)
	{
		FAutoSegIterator probe(GRegHead, GRegTail);

		while (*++probe != NULL)
		{
			properties.Push((FPropertyInfo *)*probe);
		}
		properties.ShrinkToFit();
		qsort(&properties[0], properties.Size(), sizeof(properties[0]), propcmp);
	}

	// Create a sorted list of native action functions
	if (AFTable.Size() == 0)
	{
		FAutoSegIterator probe(ARegHead, ARegTail);

		while (*++probe != NULL)
		{
			AFTable.Push(*(AFuncDesc *)*probe);
		}
		AFTable.ShrinkToFit();
		qsort(&AFTable[0], AFTable.Size(), sizeof(AFTable[0]), funccmp);
	}

	// Create a sorted list of native variables
	if (variables.Size() == 0)
	{
		FAutoSegIterator probe(MRegHead, MRegTail);

		while (*++probe != NULL)
		{
			variables.Push((FVariableInfo *)*probe);
		}
		variables.ShrinkToFit();
		qsort(&variables[0], variables.Size(), sizeof(variables[0]), varcmp);
	}
}

