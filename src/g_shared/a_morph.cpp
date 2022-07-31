#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_sharedglobal.h"
#include "sbar.h"
#include "a_morph.h"
#include "doomstat.h"
#include "g_level.h"
#include "farchive.h"
// [BB] New #includes.
#include "sv_commands.h"

static FRandom pr_morphmonst ("MorphMonster");

void EndAllPowerupEffects(AInventory *item);
void InitAllPowerupEffects(AInventory *item);

//---------------------------------------------------------------------------
//
// FUNC P_MorphPlayer
//
// Returns true if the player gets turned into a chicken/pig.
//
// TODO: Allow morphed players to receive weapon sets (not just one weapon),
// since they have their own weapon slots now.
//
//---------------------------------------------------------------------------

bool P_MorphPlayer (player_t *activator, player_t *p, const PClass *spawntype, int duration, int style, const PClass *enter_flash, const PClass *exit_flash)
{
	AInventory *item;
	APlayerPawn *morphed;
	APlayerPawn *actor;

	actor = p->mo;
	if (actor == NULL)
	{
		return false;
	}
	if (actor->flags3 & MF3_DONTMORPH)
	{
		return false;
	}
	if ((p->mo->flags2 & MF2_INVULNERABLE) && ((p != activator) || (!(style & MORPH_WHENINVULNERABLE))))
	{ // Immune when invulnerable unless this is a power we activated
		return false;
	}
	if (p->morphTics)
	{ // Player is already a beast
		if ((p->mo->GetClass() == spawntype)
			&& (p->mo->PlayerFlags & PPF_CANSUPERMORPH)
			&& (p->morphTics < (((duration) ? duration : MORPHTICS) - TICRATE))
			&& (p->mo->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2), true) == NULL))
		{ // Make a super chicken
			p->mo->GiveInventoryType (RUNTIME_CLASS(APowerWeaponLevel2));
		}
		return false;
	}
	if (p->health <= 0)
	{ // Dead players cannot morph
		return false;
	}
	if (spawntype == NULL)
	{
		return false;
	}
	if (!spawntype->IsDescendantOf (RUNTIME_CLASS(APlayerPawn)))
	{
		return false;
	}
	if (spawntype == p->mo->GetClass())
	{
		return false;
	}

	morphed = static_cast<APlayerPawn *>(Spawn (spawntype, actor->x, actor->y, actor->z, NO_REPLACE));
	EndAllPowerupEffects(actor->Inventory);
	DObject::StaticPointerSubstitution (actor, morphed);
	if ((actor->tid != 0) && (style & MORPH_NEWTIDBEHAVIOUR))
	{
		morphed->tid = actor->tid;
		morphed->AddToHash ();
		actor->RemoveFromHash ();
		actor->tid = 0;
	}
	morphed->angle = actor->angle;
	morphed->target = actor->target;
	morphed->tracer = actor;
	morphed->FriendPlayer = actor->FriendPlayer;
	morphed->DesignatedTeam = actor->DesignatedTeam;
	morphed->Score = actor->Score;
	p->PremorphWeapon = p->ReadyWeapon;
	morphed->special2 = actor->flags & ~MF_JUSTHIT;
	morphed->player = p;
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->special2 |= MF_JUSTHIT;
	}
	if (morphed->ViewHeight > p->viewheight && p->deltaviewheight == 0)
	{ // If the new view height is higher than the old one, start moving toward it.
		p->deltaviewheight = p->GetDeltaViewHeight();
	}
	morphed->flags  |= actor->flags & (MF_SHADOW|MF_NOGRAVITY);
	morphed->flags2 |= actor->flags2 & MF2_FLY;
	morphed->flags3 |= actor->flags3 & MF3_GHOST;
	// [WS] We need a fog pointer.
	AActor *fog = Spawn(((enter_flash) ? enter_flash : RUNTIME_CLASS(ATeleportFog)), actor->x, actor->y, actor->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	if ( fog )
		fog->target = morphed;
	// [WS] Inform the clients of the fog.
	// [BB] Needs a net id otherwise not all code pointers work on the fog.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnThing( fog );
		SERVERCOMMANDS_SetThingTarget ( fog );
	}

	actor->player = NULL;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	p->morphTics = (duration) ? duration : MORPHTICS;

	// [MH] Used by SBARINFO to speed up face drawing
	p->MorphedPlayerClass = spawntype;

	p->MorphStyle = style;
	p->MorphExitFlash = (exit_flash) ? exit_flash : RUNTIME_CLASS(ATeleportFog);
	p->health = morphed->health;
	p->mo = morphed;
	p->velx = p->vely = 0;
	morphed->ObtainInventory (actor);
	// Remove all armor
	for (item = morphed->Inventory; item != NULL; )
	{
		AInventory *next = item->Inventory;
		if (item->IsKindOf (RUNTIME_CLASS(AArmor)))
		{
			if (item->IsKindOf (RUNTIME_CLASS(AHexenArmor)))
			{
				// Set the HexenArmor slots to 0, except the class slot.
				AHexenArmor *hxarmor = static_cast<AHexenArmor *>(item);
				hxarmor->Slots[0] = 0;
				hxarmor->Slots[1] = 0;
				hxarmor->Slots[2] = 0;
				hxarmor->Slots[3] = 0;
				hxarmor->Slots[4] = spawntype->Meta.GetMetaFixed (APMETA_Hexenarmor0);
			}
			else if (item->ItemFlags & IF_KEEPDEPLETED)
			{
				// Set depletable armor to 0 (this includes BasicArmor).
				item->Amount = 0;
			}
			else
			{
				item->Destroy ();
			}
		}
		item = next;
	}
	InitAllPowerupEffects(morphed->Inventory);
	morphed->ActivateMorphWeapon ();
	if (p->camera == actor)
	{
		p->camera = morphed;
	}
	morphed->ScoreIcon = actor->ScoreIcon;	// [GRB]

	// [BB] Tell the clients to morph the player.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		const ULONG ulPlayer = static_cast<ULONG> ( morphed->player-players );
		SERVERCOMMANDS_SpawnPlayer( ulPlayer, PST_LIVE, MAXPLAYERS, 0, true );
		SERVER_ResetInventory( ulPlayer );
		// [BB] SERVER_ResetInventory only informs the player ulPlayer. Let the others know of at least the ammo of the player.
		SERVERCOMMANDS_SyncPlayerAmmoAmount ( ulPlayer, ulPlayer, SVCF_SKIPTHISCLIENT );
		SERVERCOMMANDS_SetThingFlags( morphed, FLAGSET_FLAGS );
		SERVERCOMMANDS_SetThingFlags( morphed, FLAGSET_FLAGS2 );
		SERVERCOMMANDS_SetThingFlags( morphed, FLAGSET_FLAGS3 );
		if ( morphed->tid != 0 )
			SERVERCOMMANDS_SetThingTID( morphed );
	}

	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_UndoPlayerMorph
//
//----------------------------------------------------------------------------

bool P_UndoPlayerMorph (player_t *activator, player_t *player, int unmorphflag, bool force)
{
	AWeapon *beastweap;
	APlayerPawn *mo;
	APlayerPawn *pmo;
	angle_t angle;

	pmo = player->mo;
	// [MH]
	// Checks pmo as well; the PowerMorph destroyer will
	// try to unmorph the player; if the destroyer runs
	// because the level or game is ended while morphed,
	// by the time it gets executed the morphed player
	// pawn instance may have already been destroyed.
	if (pmo == NULL || pmo->tracer == NULL)
	{
		return false;
	}

	bool DeliberateUnmorphIsOkay = !!(MORPH_STANDARDUNDOING & unmorphflag);

    if ((pmo->flags2 & MF2_INVULNERABLE) // If the player is invulnerable
        && ((player != activator)       // and either did not decide to unmorph,
        || (!((player->MorphStyle & MORPH_WHENINVULNERABLE)  // or the morph style does not allow it
        || (DeliberateUnmorphIsOkay))))) // (but standard morph styles always allow it),
	{ // Then the player is immune to the unmorph.
		return false;
	}

	mo = barrier_cast<APlayerPawn *>(pmo->tracer);
	mo->SetOrigin (pmo->x, pmo->y, pmo->z);
	mo->flags |= MF_SOLID;
	pmo->flags &= ~MF_SOLID;
	if (!force && !P_TestMobjLocation (mo))
	{ // Didn't fit
		mo->flags &= ~MF_SOLID;
		pmo->flags |= MF_SOLID;
		player->morphTics = 2*TICRATE;
		return false;
	}
	pmo->player = NULL;

	// Remove the morph power if the morph is being undone prematurely.
	for (AInventory *item = pmo->Inventory, *next = NULL; item != NULL; item = next)
	{
		next = item->Inventory;
		if (item->IsKindOf(RUNTIME_CLASS(APowerMorph)))
		{
			static_cast<APowerMorph *>(item)->SetNoCallUndoMorph();
			item->Destroy();
		}
	}
	EndAllPowerupEffects(pmo->Inventory);
	mo->ObtainInventory (pmo);
	DObject::StaticPointerSubstitution (pmo, mo);
	if ((pmo->tid != 0) && (player->MorphStyle & MORPH_NEWTIDBEHAVIOUR))
	{
		mo->tid = pmo->tid;
		mo->AddToHash ();
	}
	mo->angle = pmo->angle;
	mo->player = player;
	mo->reactiontime = 18;
	mo->flags = pmo->special2 & ~MF_JUSTHIT;
	mo->velx = 0;
	mo->vely = 0;
	player->velx = 0;
	player->vely = 0;
	mo->velz = pmo->velz;
	if (!(pmo->special2 & MF_JUSTHIT))
	{
		mo->renderflags &= ~RF_INVISIBLE;
	}
	mo->flags  = (mo->flags & ~(MF_SHADOW|MF_NOGRAVITY)) | (pmo->flags & (MF_SHADOW|MF_NOGRAVITY));
	mo->flags2 = (mo->flags2 & ~MF2_FLY) | (pmo->flags2 & MF2_FLY);
	mo->flags3 = (mo->flags3 & ~MF3_GHOST) | (pmo->flags3 & MF3_GHOST);
	mo->Score = pmo->Score;
	InitAllPowerupEffects(mo->Inventory);

	const PClass *exit_flash = player->MorphExitFlash;
	bool correctweapon = !!(player->MorphStyle & MORPH_LOSEACTUALWEAPON);
	bool undobydeathsaves = !!(player->MorphStyle & MORPH_UNDOBYDEATHSAVES);

	player->morphTics = 0;
	player->MorphedPlayerClass = 0;
	player->MorphStyle = 0;
	player->MorphExitFlash = NULL;
	player->viewheight = mo->ViewHeight;
	AInventory *level2 = mo->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2), true);
	if (level2 != NULL)
	{
		level2->Destroy ();
	}

	if ((player->health > 0) || undobydeathsaves)
	{
		player->health = mo->health = mo->SpawnHealth();
	}
	else // killed when morphed so stay dead
	{
		mo->health = player->health;
	}

	player->mo = mo;
	if (player->camera == pmo)
	{
		player->camera = mo;
	}

	// [MH]
	// If the player that was morphed is the one
	// taking events, reset up the face, if any;
	// this is only needed for old-skool skins
	// and for the original DOOM status bar.
	if (player == &players[consoleplayer])
	{
		const char *face = pmo->GetClass()->Meta.GetMetaString (APMETA_Face);
		if (face != NULL && strcmp(face, "None") != 0)
		{
			// Assume root-level base skin to begin with
			size_t skinindex = 0;
			// If a custom skin was in use, then reload it
			// or else the base skin for the player class.
			if ((unsigned int)player->userinfo.GetSkin() >= PlayerClasses.Size () &&
				(size_t)player->userinfo.GetSkin() < skins.Size())
			{

				skinindex = player->userinfo.GetSkin();
			}
			else if (PlayerClasses.Size () > 1)
			{
				const PClass *whatami = player->mo->GetClass();
				for (unsigned int i = 0; i < PlayerClasses.Size (); ++i)
				{
					if (PlayerClasses[i].Type == whatami)
					{
						skinindex = i;
						break;
					}
				}
			}
		}
	}

	angle = mo->angle >> ANGLETOFINESHIFT;
	if (exit_flash != NULL)
	{
		// [WS] We need a fog pointer.
		AActor *fog = Spawn(exit_flash, pmo->x + 20*finecosine[angle], pmo->y + 20*finesine[angle], pmo->z + TELEFOGHEIGHT, ALLOW_REPLACE);
		if ( fog )
			fog->target = mo;
		// [WS] Inform the clients of the fog.
		// [BB] Needs a net id otherwise not all code pointers work on the fog.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SpawnThing( fog );
			SERVERCOMMANDS_SetThingTarget ( fog );
		}
	}
	mo->SetupWeaponSlots();		// Use original class's weapon slots.
	beastweap = player->ReadyWeapon;
	if (player->PremorphWeapon != NULL)
	{
		player->PremorphWeapon->PostMorphWeapon ();
	}
	else
	{
		player->ReadyWeapon = player->PendingWeapon = NULL;
	}
	if (correctweapon)
	{ // Better "lose morphed weapon" semantics
		const PClass *morphweapon = PClass::FindClass (pmo->MorphWeapon);
		if (morphweapon != NULL && morphweapon->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
		{
			AWeapon *OriginalMorphWeapon = static_cast<AWeapon *>(mo->FindInventory (morphweapon));
			if ((OriginalMorphWeapon != NULL) && (OriginalMorphWeapon->GivenAsMorphWeapon))
			{ // You don't get to keep your morphed weapon.
				if (OriginalMorphWeapon->SisterWeapon != NULL)
				{
					OriginalMorphWeapon->SisterWeapon->Destroy ();
				}
				OriginalMorphWeapon->Destroy ();
			}
		}
 	}
	else // old behaviour (not really useful now)
	{ // Assumptions made here are no longer valid
		if (beastweap != NULL)
		{ // You don't get to keep your morphed weapon.
			if (beastweap->SisterWeapon != NULL)
			{
				beastweap->SisterWeapon->Destroy ();
			}
			beastweap->Destroy ();
		}
	}
	pmo->tracer = NULL;
	pmo->Destroy ();
	// Restore playerclass armor to its normal amount.
	AHexenArmor *hxarmor = mo->FindInventory<AHexenArmor>();
	if (hxarmor != NULL)
	{
		hxarmor->Slots[4] = mo->GetClass()->Meta.GetMetaFixed (APMETA_Hexenarmor0);
	}
	// [BB] Tell the clients to unmorph the player, also give back the original inventory to the player.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// [BB] The clients start their reactiontime later on their end. Try to adjust for this.
		SERVER_AdjustPlayersReactiontime ( static_cast<ULONG> ( player - players ) );

		SERVERCOMMANDS_SpawnPlayer( ULONG( player-players ), PST_LIVE );
		SERVER_ResetInventory( ULONG( player-players ));
		SERVERCOMMANDS_SetThingFlags( player->mo, FLAGSET_FLAGS );
		SERVERCOMMANDS_SetThingFlags( player->mo, FLAGSET_FLAGS2 );
		SERVERCOMMANDS_SetThingFlags( player->mo, FLAGSET_FLAGS3 );
		// [WS] Inform clients the state of the unmorphed player.
		SERVERCOMMANDS_SetThingFrame( player->mo, player->mo->state, MAXPLAYERS, 0, false );
		if ( player->mo->tid != 0 )
			SERVERCOMMANDS_SetThingTID( player->mo );
	}
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_MorphMonster
//
// Returns true if the monster gets turned into a chicken/pig.
//
//---------------------------------------------------------------------------

bool P_MorphMonster (AActor *actor, const PClass *spawntype, int duration, int style, const PClass *enter_flash, const PClass *exit_flash)
{
	AMorphedMonster *morphed;

	if (actor == NULL || actor->player || spawntype == NULL ||
		actor->flags3 & MF3_DONTMORPH ||
		!(actor->flags3 & MF3_ISMONSTER) ||
		!spawntype->IsDescendantOf (RUNTIME_CLASS(AMorphedMonster)))
	{
		return false;
	}

	morphed = static_cast<AMorphedMonster *>(Spawn (spawntype, actor->x, actor->y, actor->z, NO_REPLACE));
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DestroyThing( actor );

	DObject::StaticPointerSubstitution (actor, morphed);
	morphed->tid = actor->tid;
	morphed->angle = actor->angle;
	morphed->UnmorphedMe = actor;
	morphed->alpha = actor->alpha;
	morphed->RenderStyle = actor->RenderStyle;
	morphed->Score = actor->Score;

	morphed->UnmorphTime = level.time + ((duration) ? duration : MORPHTICS) + pr_morphmonst();
	morphed->MorphStyle = style;
	morphed->MorphExitFlash = (exit_flash) ? exit_flash : RUNTIME_CLASS(ATeleportFog);
	morphed->FlagsSave = actor->flags & ~MF_JUSTHIT;
	morphed->special = actor->special;
	memcpy (morphed->args, actor->args, sizeof(actor->args));
	morphed->CopyFriendliness (actor, true);
	morphed->flags |= actor->flags & MF_SHADOW;
	morphed->flags3 |= actor->flags3 & MF3_GHOST;
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->FlagsSave |= MF_JUSTHIT;
	}
	morphed->AddToHash ();
	actor->RemoveFromHash ();
	actor->special = 0;
	actor->tid = 0;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( morphed );
	AActor* fog = Spawn(((enter_flash) ? enter_flash : RUNTIME_CLASS(ATeleportFog)), actor->x, actor->y, actor->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	if ( fog )
		fog->target = morphed;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnThing( fog );
		SERVERCOMMANDS_SetThingTarget ( fog );
	}
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_UndoMonsterMorph
//
// Returns true if the monster unmorphs.
//
//----------------------------------------------------------------------------

bool P_UndoMonsterMorph (AMorphedMonster *beast, bool force)
{
	AActor *actor;

	if (beast->UnmorphTime == 0 || 
		beast->UnmorphedMe == NULL ||
		beast->flags3 & MF3_STAYMORPHED ||
		beast->UnmorphedMe->flags3 & MF3_STAYMORPHED)
	{
		return false;
	}
	actor = beast->UnmorphedMe;
	actor->SetOrigin (beast->x, beast->y, beast->z);
	actor->flags |= MF_SOLID;
	beast->flags &= ~MF_SOLID;
	int beastflags6 = beast->flags6;
	beast->flags6 &= ~MF6_TOUCHY;
	if (!force && !P_TestMobjLocation (actor))
	{ // Didn't fit
		actor->flags &= ~MF_SOLID;
		beast->flags |= MF_SOLID;
		beast->flags6 = beastflags6;
		beast->UnmorphTime = level.time + 5*TICRATE; // Next try in 5 seconds
		return false;
	}
	actor->angle = beast->angle;
	actor->target = beast->target;
	actor->FriendPlayer = beast->FriendPlayer;
	actor->flags = beast->FlagsSave & ~MF_JUSTHIT;
	actor->flags  = (actor->flags & ~(MF_FRIENDLY|MF_SHADOW)) | (beast->flags & (MF_FRIENDLY|MF_SHADOW));
	actor->flags3 = (actor->flags3 & ~(MF3_NOSIGHTCHECK|MF3_HUNTPLAYERS|MF3_GHOST))
					| (beast->flags3 & (MF3_NOSIGHTCHECK|MF3_HUNTPLAYERS|MF3_GHOST));
	actor->flags4 = (actor->flags4 & ~MF4_NOHATEPLAYERS) | (beast->flags4 & MF4_NOHATEPLAYERS);
	if (!(beast->FlagsSave & MF_JUSTHIT))
		actor->renderflags &= ~RF_INVISIBLE;
	actor->health = actor->SpawnHealth();
	actor->velx = beast->velx;
	actor->vely = beast->vely;
	actor->velz = beast->velz;
	actor->tid = beast->tid;
	actor->special = beast->special;
	actor->Score = beast->Score;
	memcpy (actor->args, beast->args, sizeof(actor->args));
	actor->AddToHash ();
	beast->UnmorphedMe = NULL;
	DObject::StaticPointerSubstitution (beast, actor);
	const PClass *exit_flash = beast->MorphExitFlash;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DestroyThing( beast );
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnThing( actor );
	beast->Destroy ();
	AActor* fog = Spawn(exit_flash, beast->x, beast->y, beast->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	if ( fog )
		fog->target = actor;
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnThing( fog );
		SERVERCOMMANDS_SetThingTarget ( fog );
	}
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_UpdateMorphedMonster
//
// Returns true if the monster unmorphs.
//
//----------------------------------------------------------------------------

bool P_UpdateMorphedMonster (AMorphedMonster *beast)
{
	if (beast->UnmorphTime > level.time)
	{
		return false;
	}
	return P_UndoMonsterMorph (beast);
}

//----------------------------------------------------------------------------
//
// FUNC P_MorphedDeath
//
// Unmorphs the actor if possible.
// Returns the unmorphed actor, the style with which they were morphed and the
// health (of the AActor, not the player_t) they last had before unmorphing.
//
//----------------------------------------------------------------------------

bool P_MorphedDeath(AActor *actor, AActor **morphed, int *morphedstyle, int *morphedhealth)
{
	// May be a morphed player
	if ((actor->player) &&
		(actor->player->morphTics) &&
		(actor->player->MorphStyle & MORPH_UNDOBYDEATH) &&
		(actor->player->mo) &&
		(actor->player->mo->tracer))
	{
		AActor *realme = actor->player->mo->tracer;
		int realstyle = actor->player->MorphStyle;
		int realhealth = actor->health;
		if (P_UndoPlayerMorph(actor->player, actor->player, 0, !!(actor->player->MorphStyle & MORPH_UNDOBYDEATHFORCED)))
		{
			*morphed = realme;
			*morphedstyle = realstyle;
			*morphedhealth = realhealth;
			return true;
		}
		return false;
	}

	// May be a morphed monster
	if (actor->GetClass()->IsDescendantOf(RUNTIME_CLASS(AMorphedMonster)))
	{
		AMorphedMonster *fakeme = static_cast<AMorphedMonster *>(actor);
		if ((fakeme->UnmorphTime) &&
			(fakeme->MorphStyle & MORPH_UNDOBYDEATH) &&
			(fakeme->UnmorphedMe))
		{
			AActor *realme = fakeme->UnmorphedMe;
			int realstyle = fakeme->MorphStyle;
			int realhealth = fakeme->health;
			if (P_UndoMonsterMorph(fakeme, !!(fakeme->MorphStyle & MORPH_UNDOBYDEATHFORCED)))
			{
				*morphed = realme;
				*morphedstyle = realstyle;
				*morphedhealth = realhealth;
				return true;
			}
		}
		fakeme->flags3 |= MF3_STAYMORPHED; // moved here from AMorphedMonster::Die()
		return false;
	}

	// Not a morphed player or monster
	return false;
}

//===========================================================================
//
// EndAllPowerupEffects
//
// Calls EndEffect() on every Powerup in the inventory list.
//
//===========================================================================

void EndAllPowerupEffects(AInventory *item)
{
	while (item != NULL)
	{
		if (item->IsKindOf(RUNTIME_CLASS(APowerup)))
		{
			static_cast<APowerup *>(item)->EndEffect();
		}
		item = item->Inventory;
	}
}

//===========================================================================
//
// InitAllPowerupEffects
//
// Calls InitEffect() on every Powerup in the inventory list.
//
//===========================================================================

void InitAllPowerupEffects(AInventory *item)
{
	while (item != NULL)
	{
		if (item->IsKindOf(RUNTIME_CLASS(APowerup)))
		{
			static_cast<APowerup *>(item)->InitEffect();
		}
		item = item->Inventory;
	}
}

// Base class for morphing projectiles --------------------------------------

IMPLEMENT_CLASS(AMorphProjectile)

int AMorphProjectile::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	const PClass *morph_flash = PClass::FindClass (MorphFlash);
	const PClass *unmorph_flash = PClass::FindClass (UnMorphFlash);
	if (target->player)
	{
		const PClass *player_class = PClass::FindClass (PlayerClass);
		P_MorphPlayer (NULL, target->player, player_class, Duration, MorphStyle, morph_flash, unmorph_flash);
	}
	else
	{
		const PClass *monster_class = PClass::FindClass (MonsterClass);
		P_MorphMonster (target, monster_class, Duration, MorphStyle, morph_flash, unmorph_flash);
	}
	return -1;
}

void AMorphProjectile::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PlayerClass << MonsterClass << Duration << MorphStyle << MorphFlash << UnMorphFlash;
}


// Morphed Monster (you must subclass this to do something useful) ---------

IMPLEMENT_POINTY_CLASS (AMorphedMonster)
 DECLARE_POINTER (UnmorphedMe)
END_POINTERS

void AMorphedMonster::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << UnmorphedMe << UnmorphTime << MorphStyle << MorphExitFlash << FlagsSave;
}

void AMorphedMonster::Destroy ()
{
	if (UnmorphedMe != NULL)
	{
		UnmorphedMe->Destroy ();
	}
	Super::Destroy ();
}

void AMorphedMonster::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	// Dead things don't unmorph
//	flags3 |= MF3_STAYMORPHED;
	// [MH]
	// But they can now, so that line above has been
	// moved into P_MorphedDeath() and is now set by
	// that function if and only if it is needed.
	Super::Die (source, inflictor, dmgflags);
	if (UnmorphedMe != NULL && (UnmorphedMe->flags & MF_UNMORPHED))
	{
		UnmorphedMe->health = health;
		UnmorphedMe->Die (source, inflictor, dmgflags);
	}
}

void AMorphedMonster::Tick ()
{
	if (!P_UpdateMorphedMonster (this))
	{
		Super::Tick ();
	}
}
