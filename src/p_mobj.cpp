// Emacs style mode select	 -*- C++ -*- 
// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Moving object handling. Spawn functions.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

// [BB] network.h has to be included before stats.h under Linux.
// The reason should be investigated.
#include "network.h"

#include "templates.h"
#include "i_system.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "doomstat.h"
#include "v_video.h"
#include "c_cvars.h"
#include "c_dispatch.h"
// [BB] Zandronum uses different bot code.
//#include "b_bot.h"	//Added by MC:
#include "stats.h"
#include "a_hexenglobal.h"
#include "a_sharedglobal.h"
#include "gi.h"
#include "sbar.h"
#include "p_acs.h"
#include "cmdlib.h"
#include "decallib.h"
#include "ravenshared.h"
#include "a_action.h"
#include "a_keys.h"
#include "p_conversation.h"
#include "thingdef/thingdef.h"
#include "g_game.h"
#include "teaminfo.h"
#include "r_data/r_translate.h"
#include "r_sky.h"
#include "g_level.h"
#include "d_event.h"
#include "colormatcher.h"
#include "v_palette.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "farchive.h"
#include "r_data/colormaps.h"
#include "r_renderer.h"
// [BB] New #includes.
#include "deathmatch.h"
#include "duel.h"
#include "gamemode.h"
#include "cl_main.h"
#include "cooperative.h"
#include "invasion.h"
#include "scoreboard.h"
#include "team.h"
#include "sv_commands.h"
#include "cl_demo.h"
#include "survival.h"
#include "network/nettraffic.h"
#include "unlagged.h"
#include "d_netinf.h"
#include "domination.h"
#include "network_enums.h"
#include <set>

// MACROS ------------------------------------------------------------------

#define WATER_SINK_FACTOR		3
#define WATER_SINK_SMALL_FACTOR	4
#define WATER_SINK_SPEED		(FRACUNIT/2)
#define WATER_JUMP_SPEED		(FRACUNIT*7/2)

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// [BB] Added bGiveInventory and moved the declaration to g_game.h.
//void G_PlayerReborn (int player);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PlayerLandedOnThing (AActor *mo, AActor *onmobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

/* [BB] Zandronum uses different bot code.
extern cycle_t BotSupportCycles;
extern int BotWTG;
*/
EXTERN_CVAR (Int,  cl_rockettrails)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FRandom pr_explodemissile ("ExplodeMissile");
FRandom pr_bounce ("Bounce");
static FRandom pr_reflect ("Reflect");
static FRandom pr_nightmarerespawn ("NightmareRespawn");
static FRandom pr_botspawnmobj ("BotSpawnActor");
static FRandom pr_spawnmapthing ("SpawnMapThing");
static FRandom pr_spawnpuff ("SpawnPuff");
static FRandom pr_spawnblood ("SpawnBlood");
static FRandom pr_splatter ("BloodSplatter");
static FRandom pr_takedamage ("TakeDamage");
static FRandom pr_splat ("FAxeSplatter");
static FRandom pr_ripperblood ("RipperBlood");
static FRandom pr_chunk ("Chunk");
static FRandom pr_checkmissilespawn ("CheckMissileSpawn");
static FRandom pr_spawnmissile ("SpawnMissile");
static FRandom pr_missiledamage ("MissileDamage");
static FRandom pr_multiclasschoice ("MultiClassChoice");
static FRandom pr_rockettrail("RocketTrail");
static FRandom pr_uniquetid("UniqueTID");

/*static*/	IDList<AActor> g_NetIDList;
static	ULONG		g_ulFirstFreeNetID = 1;

static	LONG	g_lSpawnCount = 0;
static	cycle_t	g_SpawnCycles;
static	LONG	g_lStaleSpawnCount = 0;
static	cycle_t	g_StaleSpawnCycles;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FRandom pr_spawnmobj ("SpawnActor");

CUSTOM_CVAR (Float, sv_gravity, 800.f, CVAR_SERVERINFO|CVAR_NOSAVE)
{
	level.gravity = self;

	// [BB] Notify the clients about the change.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( gamestate != GS_STARTUP ))
	{
		SERVER_Printf( "%s changed to: %f\n", self.GetName( ), self.GetGenericRep( CVAR_Float ).Float );
		SERVERCOMMANDS_SetGameModeLimits( );
	}
}


CVAR (Bool, cl_missiledecals, true, CVAR_ARCHIVE)
CVAR (Bool, addrocketexplosion, false, CVAR_ARCHIVE)
CVAR (Int, cl_pufftype, 0, CVAR_ARCHIVE);
CVAR (Int, cl_bloodtype, 0, CVAR_ARCHIVE);

// [BB]
CVAR (Bool, sv_showspawnnames, false, CVAR_DEBUGONLY)

// CODE --------------------------------------------------------------------

IMPLEMENT_POINTY_CLASS (AActor)
 DECLARE_POINTER (target)
 DECLARE_POINTER (lastenemy)
 DECLARE_POINTER (tracer)
 DECLARE_POINTER (goal)
 DECLARE_POINTER (LastLookActor)
 DECLARE_POINTER (Inventory)
 DECLARE_POINTER (LastHeard)
 DECLARE_POINTER (master)
 DECLARE_POINTER (Poisoner)
 // [BC] Declare these so refrences to them get fixed if they're removed.
 DECLARE_POINTER (pMonsterSpot)
 DECLARE_POINTER (pPickupSpot)
 DECLARE_POINTER (Rune) // [TP]
END_POINTERS

AActor::~AActor ()
{
	// Please avoid calling the destructor directly (or through delete)!
	// Use Destroy() instead.
}

void AActor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	if (arc.IsStoring ())
	{
		arc.WriteSprite (sprite);
	}
	else
	{
		sprite = arc.ReadSprite ();
	}

	arc << x
		<< y
		<< z
		<< angle
		<< frame
		<< scaleX
		<< scaleY
		<< RenderStyle
		<< renderflags
		<< picnum
		<< floorpic
		<< ceilingpic
		<< TIDtoHate
		<< LastLookPlayerNumber
		<< LastLookActor
		<< effects
		<< alpha
		<< fillcolor
		<< pitch
		<< roll
		<< Sector
		<< floorz
		<< ceilingz
		<< dropoffz
		<< floorsector
		<< ceilingsector
		<< radius
		<< height
		<< projectilepassheight
		<< velx
		<< vely
		<< velz
		<< tics
		<< state
		<< Damage;
	if (SaveVersion >= 3227)
	{
		arc << projectileKickback;
	}
	arc	<< flags
		<< flags2
		<< flags3
		<< flags4
		<< flags5
		<< flags6;
	if (SaveVersion >= 4504)
	{
		arc << flags7;
	}
	arc	<< special1
		<< special2
		<< health
		<< movedir
		<< visdir
		<< movecount
		<< strafecount
		<< target
		<< lastenemy
		<< LastHeard
		<< reactiontime
		<< threshold
		<< player
		<< SpawnPoint[0] << SpawnPoint[1] << SpawnPoint[2]
		<< SpawnAngle;
	if (SaveVersion >= 4506)
	{
		arc << StartHealth;
	}
	arc << skillrespawncount
		<< tracer
		<< floorclip
		<< tid
		<< special;
	if (P_IsACSSpecial(special))
	{
		P_SerializeACSScriptNumber(arc, args[0], false);
	}
	else
	{
		arc << args[0];
	}
	arc << args[1] << args[2] << args[3] << args[4];
	if (SaveVersion >= 3427)
	{
		arc << accuracy << stamina;
	}
	arc << goal
		<< waterlevel
		<< MinMissileChance
		<< SpawnFlags
		<< Inventory
		<< InventoryID
		<< id
		<< FloatBobPhase
		<< Translation
		<< SeeSound
		<< AttackSound
		<< PainSound
		<< DeathSound
		<< ActiveSound
		<< UseSound
		<< BounceSound
		<< WallBounceSound
		<< CrushPainSound
		<< Speed
		<< FloatSpeed
		<< Mass
		<< PainChance
		<< SpawnState
		<< SeeState
		<< MeleeState
		<< MissileState
		<< MaxDropOffHeight 
		<< MaxStepHeight
		<< BounceFlags
		<< bouncefactor
		<< wallbouncefactor
		<< bouncecount
		<< maxtargetrange
		<< meleethreshold
		<< meleerange
		<< DamageType;
	if (SaveVersion >= 4501)
	{
		arc << DamageTypeReceived;
	}
	if (SaveVersion >= 3237) 
	{
		arc
		<< PainType
		<< DeathType;
	}
	arc	<< gravity
		<< FastChaseStrafeCount
		<< master
		<< smokecounter
		<< BlockingMobj
		<< BlockingLine
		<< VisibleToTeam // [BB]
		<< pushfactor
		<< Species
		<< Score;
	if (SaveVersion >= 3113)
	{
		arc << DesignatedTeam;
	}
	arc << lastpush << lastbump
		<< PainThreshold
		<< DamageFactor
		<< WeaveIndexXY << WeaveIndexZ
		<< PoisonDamageReceived << PoisonDurationReceived << PoisonPeriodReceived << Poisoner
		<< PoisonDamage << PoisonDuration << PoisonPeriod;
	if (SaveVersion >= 3235)
	{
		arc << PoisonDamageType << PoisonDamageTypeReceived;
	}
	arc << ConversationRoot << Conversation;
	
	// [BB] Zandronum additions.
	arc << ulLimitedToTeam // [BB]
		<< lFixedColormap // [BB]
		// [BB] Before the snapshot is loaded, player bodies are spawned, which invalidates the old netIDs.
		//<< lNetID // [BC] We need to archive this so that it's restored properly when going between maps in a hub.
		<< ulSTFlags
		<< ulNetworkFlags
		<< ulInvasionWave
		<< pMonsterSpot
		<< pPickupSpot
		<< Rune;

	{
		FString tagstr;
		if (arc.IsStoring() && Tag != NULL && Tag->Len() > 0) tagstr = *Tag;
		arc << tagstr;
		if (arc.IsLoading())
		{
			if (tagstr.Len() == 0) Tag = NULL;
			else Tag = mStringPropertyData.Alloc(tagstr);
		}
	}

	if (arc.IsLoading ())
	{
		// [BB] If the the actor needs one, generate a new netID.
		if ( !( ulNetworkFlags & NETFL_NONETID ) && !( ulNetworkFlags & NETFL_SERVERSIDEONLY ) )
		{
			lNetID = g_NetIDList.getNewID( );
			g_NetIDList.useID ( lNetID, this );
		}

		touching_sectorlist = NULL;
		LinkToWorld (Sector);
		AddToHash ();
		SetShade (fillcolor);
		if (player)
		{
			if (playeringame[player - players] && 
				player->cls != NULL &&
				!(flags4 & MF4_NOSKIN) &&
				state->sprite == GetDefaultByType (player->cls)->SpawnState->sprite)
			{ // Give player back the skin
				sprite = skins[player->userinfo.GetSkin()].sprite;
			}
			if (Speed == 0)
			{
				Speed = GetDefault()->Speed;
			}
		}
		PrevX = x;
		PrevY = y;
		PrevZ = z;
		PrevAngle = angle;
		UpdateWaterLevel(z, false);
	}
}

void FMapThing::Serialize (FArchive &arc)
{
	arc << thingid << x << y << z << angle << type << flags << special
		<< args[0] << args[1] << args[2] << args[3] << args[4];
}

AActor::AActor () throw()
{
}

AActor::AActor (const AActor &other) throw()
	: DThinker()
{
	memcpy (&x, &other.x, (BYTE *)&this[1] - (BYTE *)&x);
}

AActor &AActor::operator= (const AActor &other)
{
	memcpy (&x, &other.x, (BYTE *)&this[1] - (BYTE *)&x);
	return *this;
}

//==========================================================================
//
// AActor::InStateSequence
//
// Checks whether the current state is in a contiguous sequence that
// starts with basestate
//
//==========================================================================

bool AActor::InStateSequence(FState * newstate, FState * basestate)
{
	if (basestate == NULL) return false;

	FState * thisstate = basestate;
	do
	{
		if (newstate == thisstate) return true;
		basestate = thisstate;
		thisstate = thisstate->GetNextState();
	}
	while (thisstate == basestate+1);
	return false;
}

//==========================================================================
//
// AActor::GetTics
//
// Get the actual duration of the next state
// We are using a state flag now to indicate a state that should be
// accelerated in Fast mode or slowed in Slow mode.
//
//==========================================================================

int AActor::GetTics(FState * newstate)
{
	int tics = newstate->GetTics();
	if (isFast() && newstate->Fast)
	{
		return tics - (tics>>1);
	}
	else if (isSlow() && newstate->Slow)
	{
		return tics<<1;
	}
	return tics;
}

// [BB] To print the client side infinite loop workaround warning only once per actor type.
static std::set<unsigned short> clientInfiniteLoopWarningPrintedActors;

//==========================================================================
//
// AActor::SetState
//
// Returns true if the mobj is still present.
//
//==========================================================================

bool AActor::SetState (FState *newstate, bool nofunction)
{
/*
	if (debugfile && player && (player->cheats & CF_PREDICTING))
		fprintf (debugfile, "for pl %td: SetState while predicting!\n", player-players);
*/
	// [BB] Workaround to break client side infinite loops in DECORATE definitions.
	int numActions = 0;

	do
	{
		if (newstate == NULL)
		{
			state = NULL;

			// [BC] If we're playing a game mode in which the map resets, and this is something
			// that is level spawned, don't destroy it. Instead, put it in a temporary invisibile
			// state.
			HideOrDestroyIfSafe();
			return false;
		}
		// [BC] What's this block for? It's currently breaking Heretic's tomed
		// hellstaff's rain.
/*
		// [BB] Dead things should not have positive health.
		if ( (newstate == DeathState) && (health > 0) )
			health = 0;
*/
		int prevsprite, newsprite;

		if (state != NULL)
		{
			prevsprite = state->sprite;
		}
		else
		{
			prevsprite = -1;
		}
		state = newstate;
		tics = GetTics(newstate);
		renderflags = (renderflags & ~RF_FULLBRIGHT) | newstate->GetFullbright();
		newsprite = newstate->sprite;
		if (newsprite != SPR_FIXED)
		{ // okay to change sprite and/or frame
			if (!newstate->GetSameFrame())
			{ // okay to change frame
				frame = newstate->GetFrame();
			}
			if (newsprite != SPR_NOCHANGE)
			{ // okay to change sprite
				if (!(flags4 & MF4_NOSKIN) && newsprite == SpawnState->sprite)
				{ // [RH] If the new sprite is the same as the original sprite, and
				// this actor is attached to a player, use the player's skin's
				// sprite. If a player is not attached, do not change the sprite
				// unless it is different from the previous state's sprite; a
				// player may have been attached, died, and respawned elsewhere,
				// and we do not want to lose the skin on the body. If it wasn't
				// for Dehacked, I would move sprite changing out of the states
				// altogether, since actors rarely change their sprites after
				// spawning.
					if (player != NULL && ( skins.Size() > static_cast<unsigned int> ( player->userinfo.GetSkin() ) ) ) // [BB] Adapted the skins check
					{
						sprite = skins[player->userinfo.GetSkin()].sprite;
					}
					else if (newsprite != prevsprite)
					{
						sprite = newsprite;
					}
				}
				else
				{
					sprite = newsprite;
				}
			}
		}

		if (!nofunction && newstate->CallAction(this, this))
		{
			// Check whether the called action function resulted in destroying the actor
			if (ObjectFlags & OF_EuthanizeMe)
				return false;
		}
		newstate = newstate->GetNextState();

		// [BB] Workaround to break client side infinite loops in DECORATE definitions.
		if ( NETWORK_InClientMode() && ( numActions++ > 10000 ) )
		{
			if ( clientInfiniteLoopWarningPrintedActors.find ( this->GetClass( )->getActorNetworkIndex() ) == clientInfiniteLoopWarningPrintedActors.end() )
			{
				Printf ( "Warning: Breaking infinite loop in actor %s.\nCurrent offset from spawn state is %ld\n", this->GetClass()->TypeName.GetChars(), static_cast<LONG>(this->state - this->SpawnState) );
				clientInfiniteLoopWarningPrintedActors.insert ( this->GetClass( )->getActorNetworkIndex() );
			}
			break;
		}
	} while (tics == 0);

	if (Renderer != NULL)
	{
		Renderer->StateChanged(this);
	}
	return true;
}

//----------------------------------------------------------------------------
//
// [BB] AActor::HideOrDestroyIfSafe
//
//----------------------------------------------------------------------------

void AActor::HideOrDestroyIfSafe ()
{
	// [BB] If we're playing a game mode in which the map resets, and this is something
	// that is level spawned, don't destroy it. Instead, put it in a temporary invisibile
	// state.
	if (( GAMEMODE_GetCurrentFlags() & GMF_MAPRESETS ) &&
		( ulSTFlags & STFL_LEVELSPAWNED ) &&
		( NETWORK_InClientMode() == false ))
	{
		// [BB] Do any actor specific things that are necessary to properly hide this thing.
		PrepareForHiding();

		// unlink from sector and block lists
		UnlinkFromWorld ();
		flags |= MF_NOSECTOR|MF_NOBLOCKMAP;

		// Delete all nodes on the current sector_list			phares 3/16/98
		P_DelSector_List();

		// [BB] The dormant flag stops removed invasion spawners from spawning things when hidden.
		flags2 |= MF2_DORMANT;
		flags &= ~MF_SOLID;
		SetState( RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideIndefinitely") );

		// [BB] Remember that this was hidden. These things sometimes have to be explicitly excluded, e.g. in CheckBossDeath.
		ulSTFlags |= STFL_HIDDEN_INSTEAD_OF_DESTROYED;
	}
	else
		Destroy();
}

//----------------------------------------------------------------------------
//
// [BB] AActor::PrepareForHiding
//
//----------------------------------------------------------------------------

void AActor::PrepareForHiding ()
{
}

//----------------------------------------------------------------------------
//
// [BC] AActor::InSpawnState
//
//----------------------------------------------------------------------------

bool AActor::InSpawnState( )
{
	FState	*pSpawnState;
	FState	*pState;

	if ( state != NULL )
	{
		pSpawnState = InitialState;
		while ( pSpawnState != NULL )
		{
			// If our current state matches one of the frames in the spawn state, then
			// we're in the spawn state.
			if ( state == pSpawnState )
				return ( true );

			pState = pSpawnState;
			pSpawnState = pSpawnState->GetNextState( );

			// If the next state skips off somewhere else, then just break out.
			if ( pSpawnState != pState + 1 )
				break;
		}
	}

	// Our current state didn't match any of the frames of the spawn state.
	return ( false );
}

//----------------------------------------------------------------------------
//
// AActor::InDeathState
//
//----------------------------------------------------------------------------

bool AActor::InDeathState()
{
	if( state != NULL )
	{
		FState *pDeadState = FindState(NAME_Death);
		FState *pState = NULL;
		while ( pDeadState != NULL )
		{
			if ( state == pDeadState )
				return true;
			pState = pDeadState;
			pDeadState = pDeadState->GetNextState( );

			// If the state loops back to the beginning of the death state, or to itself, break out.
			// [BC] The bell in Hexen goes back to its idle state at the end of its death state.
			// Therefore, let's instead break out if the death state jumps around.
//			if (( pDeadState == DeathState ) || ( pState == pDeadState ))
			if ( pDeadState != pState + 1 )
				break;
		}
	}
	return false;
}

//----------------------------------------------------------------------------
//
// [BB] AActor::InState
//
//----------------------------------------------------------------------------

bool AActor::InState(FState *pState, unsigned int *pOffset, FState *pCurrentActorStateOverride ) const
{
	FState *pCurrentActorState =  ( pCurrentActorStateOverride != NULL ) ? pCurrentActorStateOverride : state;
	if( pCurrentActorState != NULL )
	{
		FState *pTempState = pState;
		std::vector<FState*> checkedFrames;
		while ( pTempState != NULL )
		{
			if ( pCurrentActorState == pTempState )
			{
				if ( pOffset != NULL )
					*pOffset = checkedFrames.size();
				return true;
			}

			bool breakLoop = false;
			// [BB] Check if we already encountered pTempState.
			for ( unsigned int i = 0; i < checkedFrames.size(); i++ )
			{
				if ( pTempState == checkedFrames[i] )
					breakLoop = true;
			}
			// [BB] Save the frame pointer, necessary to check if we encounter this frame again.
			checkedFrames.push_back( pTempState );

			pTempState = pTempState->GetNextState( );

			// [BB] If the state loops back to any state we already encountered, break out to prevent an infinite loop.
			if ( breakLoop )
				break;
		}
	}
	return false;
}

bool AActor::InState(FName label) const
{
	return InState ( FindState(label) );
}

//============================================================================
//
// AActor :: AddInventory
//
//============================================================================

void AActor::AddInventory (AInventory *item)
{
	// Check if it's already attached to an actor
	if (item->Owner != NULL)
	{
		// Is it attached to us?
		if (item->Owner == this)
			return;

		// No, then remove it from the other actor first
		item->Owner->RemoveInventory (item);
	}

	item->Owner = this;
	item->Inventory = Inventory;
	Inventory = item;

	// Each item receives an unique ID when added to an actor's inventory.
	// This is used by the DEM_INVUSE command to identify the item. Simply
	// using the item's position in the list won't work, because ticcmds get
	// run sometime in the future, so by the time it runs, the inventory
	// might not be in the same state as it was when DEM_INVUSE was sent.
	Inventory->InventoryID = InventoryID++;
}

//============================================================================
//
// AActor :: RemoveInventory
//
//============================================================================

void AActor::RemoveInventory(AInventory *item)
{
	AInventory *inv, **invp;

	if (item != NULL && item->Owner != NULL)	// can happen if the owner was destroyed by some action from an item's use state.
	{
		invp = &item->Owner->Inventory;
		for (inv = *invp; inv != NULL; invp = &inv->Inventory, inv = *invp)
		{
			if (inv == item)
			{
				*invp = item->Inventory;
				item->DetachFromOwner();
				item->Owner = NULL;
				break;
			}
		}
	}
}

//============================================================================
//
// AActor :: DestroyAllInventory
//
//============================================================================

void AActor::DestroyAllInventory ()
{
	while (Inventory != NULL)
	{
		AInventory *item = Inventory;
		// [BC] In certain modes, we may need to keep this item around.
		if (( item->ulSTFlags & STFL_LEVELSPAWNED ) &&
			( GAMEMODE_GetCurrentFlags() & GMF_MAPRESETS ))
		{
			item->HideIndefinitely( );

			// And now, some code from AInventory::Destroy() to remove this object
			// from the owner's inventory..
			if ( item->Owner != NULL )
				item->Owner->RemoveInventory( item );

			item->Inventory = NULL;
		}
		else
			item->Destroy ();
		assert (item != Inventory);
	}
}

//============================================================================
//
// AActor :: FirstInv
//
// Returns the first item in this actor's inventory that has IF_INVBAR set.
//
//============================================================================

AInventory *AActor::FirstInv ()
{
	if (Inventory == NULL)
	{
		return NULL;
	}
	if (Inventory->ItemFlags & IF_INVBAR)
	{
		return Inventory;
	}
	return Inventory->NextInv ();
}

//============================================================================
//
// AActor :: UseInventory
//
// Attempts to use an item. If the use succeeds, one copy of the item is
// removed from the inventory. If all copies are removed, then the item is
// destroyed.
//
//============================================================================

bool AActor::UseInventory (AInventory *item)
{
	// No using items if you're dead.
	if (health <= 0)
	{
		return false;
	}
	// Don't use it if you don't actually have any of it.
	if (item->Amount <= 0 || (item->ObjectFlags & OF_EuthanizeMe))
	{
		return false;
	}
	if (!item->Use (false))
	{
		return false;
	}

	if (dmflags2 & DF2_INFINITE_INVENTORY)
		return true;

	if (--item->Amount <= 0 && !(item->ItemFlags & IF_KEEPDEPLETED))
	{
		item->Destroy ();
	}
	return true;
}

//===========================================================================
//
// AActor :: DropInventory
//
// Removes a single copy of an item and throws it out in front of the actor.
//
//===========================================================================

AInventory *AActor::DropInventory (AInventory *item)
{
	angle_t an;
	AInventory *drop = item->CreateTossable ();

	// [BC] Don't do this in client mode.
	if ( NETWORK_InClientMode() )
	{
		return ( NULL );
	}

	if (drop == NULL)
	{
		return NULL;
	}
	an = angle >> ANGLETOFINESHIFT;
	drop->SetOrigin(x, y, z + 10*FRACUNIT);
	drop->angle = angle;
	drop->velx = velx + 5 * finecosine[an];
	drop->vely = vely + 5 * finesine[an];
	drop->velz = velz + FRACUNIT;
	drop->flags &= ~MF_NOGRAVITY;	// Don't float
	drop->ClearCounters();	// do not count for statistics again

	// [BC] Create the drop for clients.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnMissile( drop );
		SERVERCOMMANDS_SetThingFlags( drop, FLAGSET_FLAGS );
		if ( player )
			SERVERCOMMANDS_PlayerDropInventory( ULONG( player - players ), item );
	}

	return drop;
}

//============================================================================
//
// AActor :: FindInventory
//
//============================================================================

AInventory *AActor::FindInventory (const PClass *type, bool subclass)
{
	AInventory *item;

	if (type == NULL) return NULL;

	assert (type->ActorInfo != NULL);
	for (item = Inventory; item != NULL; item = item->Inventory)
	{
		if (!subclass)
		{
			if (item->GetClass() == type)
			{
				break;
			}
		}
		else
		{
			if (item->IsKindOf(type))
			{
				break;
			}
		}
	}
	return item;
}

AInventory *AActor::FindInventory (FName type)
{
	return FindInventory(PClass::FindClass(type));
}

//============================================================================
//
// AActor :: GiveInventoryType
//
//============================================================================

AInventory *AActor::GiveInventoryType (const PClass *type)
{
	AInventory *item = NULL;

	if (type != NULL)
	{
		item = static_cast<AInventory *>(Spawn (type, 0,0,0, NO_REPLACE));
		if (!item->CallTryPickup (this))
		{
			item->Destroy ();
			return NULL;
		}
	}
	return item;
}

//============================================================================
//
// [BB] AActor :: GiveInventoryTypeRespectingReplacements
//
//============================================================================

AInventory *AActor::GiveInventoryTypeRespectingReplacements (const PClass *type)
{
	const PClass *pReplacementClass = type->ActorInfo->GetReplacement( )->Class;
	// [BB] Special handling for DehackedPickup: In this case the original actor is
	// already modified and we need to give it to him instead of the replacement.
	if ( pReplacementClass->IsDescendantOf ( PClass::FindClass( "DehackedPickup" ) ) )
		return GiveInventoryType ( type );
	// [BB] If the replacement is something, that is not of type AInventory, we
	// can't give it to the actor.
	else if ( pReplacementClass->IsDescendantOf( RUNTIME_CLASS( AInventory )) )
		return GiveInventoryType ( pReplacementClass );
	else
		return NULL;
}

//============================================================================
//
// AActor :: GiveAmmo
//
// Returns true if the ammo was added, false if not.
//
//============================================================================

bool AActor::GiveAmmo (const PClass *type, int amount)
{
	// [BB] The server tells the client how much ammo it gets.
	if ( NETWORK_GetState( ) == NETSTATE_CLIENT )
		return false;

	if (type != NULL)
	{
		AInventory *item = static_cast<AInventory *>(Spawn (type, 0, 0, 0, NO_REPLACE));
		if (item)
		{
			item->Amount = amount;
			item->flags |= MF_DROPPED;
			if (!item->CallTryPickup (this))
			{
				item->Destroy ();
				return false;
			}

			// [BB] The server tells the client how much ammo it gets.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( player ))
				SERVERCOMMANDS_GiveInventory( ULONG(player - players), item );

			return true;
		}
	}
	return false;
}

//============================================================================
//
// AActor :: ClearInventory
//
// Clears the inventory of a single actor.
//
//============================================================================

void AActor::ClearInventory()
{
	// In case destroying an inventory item causes another to be destroyed
	// (e.g. Weapons destroy their sisters), keep track of the pointer to
	// the next inventory item rather than the next inventory item itself.
	// For example, if a weapon is immediately followed by its sister, the
	// next weapon we had tracked would be to the sister, so it is now
	// invalid and we won't be able to find the complete inventory by
	// following it.
	//
	// When we destroy an item, we leave invp alone, since the destruction
	// process will leave it pointing to the next item we want to check. If
	// we don't destroy an item, then we move invp to point to its Inventory
	// pointer.
	//
	// It should be safe to assume that an item being destroyed will only
	// destroy items further down in the chain, because if it was going to
	// destroy something we already processed, we've already destroyed it,
	// so it won't have anything to destroy.

	AInventory **invp = &Inventory;

	while (*invp != NULL)
	{
		AInventory *inv = *invp;
		if (!(inv->ItemFlags & IF_UNDROPPABLE))
		{
			// For the sake of undroppable weapons, never remove ammo once
			// it has been acquired; just set its amount to 0.
			if (inv->IsKindOf(RUNTIME_CLASS(AAmmo)))
			{
				AAmmo *ammo = static_cast<AAmmo*>(inv);
				ammo->Amount = 0;
				invp = &inv->Inventory;
			}
			// [BB] Don't delete BasicArmor, just clear it. Probably ZDoom should do it like this
			// as well, but to be on the safe side only do this on the client for now.
			else if ( NETWORK_InClientMode() && ( inv->GetClass() == RUNTIME_CLASS(ABasicArmor) ) )
			{
				ABasicArmor *barmor = static_cast<ABasicArmor *> (inv);
				barmor->SavePercent = 0;
				barmor->Amount = 0;
				invp = &inv->Inventory;
			}
			else
			{
				inv->Destroy ();
			}
		}
		else if (inv->GetClass() == RUNTIME_CLASS(AHexenArmor))
		{
			AHexenArmor *harmor = static_cast<AHexenArmor *> (inv);
			harmor->Slots[3] = harmor->Slots[2] = harmor->Slots[1] = harmor->Slots[0] = 0;
			invp = &inv->Inventory;
		}
		else
		{
			invp = &inv->Inventory;
		}
	}
	if (player != NULL)
	{
		player->ReadyWeapon = NULL;
		player->PendingWeapon = WP_NOCHANGE;
		player->psprites[ps_weapon].state = NULL;
		player->psprites[ps_flash].state = NULL;
		// [BB] If we're the server, tell the client he lost his inventory.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ))
			SERVERCOMMANDS_DestroyAllInventory( player - players );
	}
}

//============================================================================
//
// AActor :: CopyFriendliness
//
// Makes this actor hate (or like) the same things another actor does.
//
//============================================================================

void AActor::CopyFriendliness (AActor *other, bool changeTarget, bool resetHealth)
{
	level.total_monsters -= CountsAsKill();

	// [BB] If the number of total monsters was reduced, update the invasion monster count accordingly.
	if ( CountsAsKill() )
		INVASION_UpdateMonsterCount( this, true );

	TIDtoHate = other->TIDtoHate;
	LastLookActor = other->LastLookActor;
	LastLookPlayerNumber = other->LastLookPlayerNumber;
	flags  = (flags & ~MF_FRIENDLY) | (other->flags & MF_FRIENDLY);
	flags3 = (flags3 & ~(MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS)) | (other->flags3 & (MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS));
	flags4 = (flags4 & ~(MF4_NOHATEPLAYERS | MF4_BOSSSPAWNED)) | (other->flags4 & (MF4_NOHATEPLAYERS | MF4_BOSSSPAWNED));
	FriendPlayer = other->FriendPlayer;
	DesignatedTeam = other->DesignatedTeam;
	if (changeTarget && other->target != NULL && !(other->target->flags3 & MF3_NOTARGET) && !(other->target->flags7 & MF7_NEVERTARGET))
	{
		// LastHeard must be set as well so that A_Look can react to the new target if called
		LastHeard = target = other->target;
	}	
	if (resetHealth) health = SpawnHealth();	
	level.total_monsters += CountsAsKill();

	// [BB] If the number of total monsters was increased, update the invasion monster count accordingly.
	if ( CountsAsKill() )
		INVASION_UpdateMonsterCount( this, false );
}

//============================================================================
//
// AActor :: ObtainInventory
//
// Removes the items from the other actor and puts them in this actor's
// inventory. The actor receiving the inventory must not have any items.
//
//============================================================================

void AActor::ObtainInventory (AActor *other)
{
	assert (Inventory == NULL);

	Inventory = other->Inventory;
	InventoryID = other->InventoryID;
	other->Inventory = NULL;
	other->InventoryID = 0;

	if (other->IsKindOf(RUNTIME_CLASS(APlayerPawn)) && this->IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{
		APlayerPawn *you = static_cast<APlayerPawn *>(other);
		APlayerPawn *me = static_cast<APlayerPawn *>(this);
		me->InvFirst = you->InvFirst;
		me->InvSel = you->InvSel;
		you->InvFirst = NULL;
		you->InvSel = NULL;
	}

	AInventory *item = Inventory;
	while (item != NULL)
	{
		// [BB] Due to the server side ammo handling, the server has
		// to inform the client here about the ammo amounts.
		if ( (NETWORK_GetState( ) == NETSTATE_SERVER) && player && item->IsKindOf(RUNTIME_CLASS(AAmmo)))
			SERVERCOMMANDS_GiveInventory( player - players, item );

		item->Owner = this;
		item = item->Inventory;
	}
}

//============================================================================
//
// AActor :: CheckLocalView
//
// Returns true if this actor is local for the player. Here, local means the
// player is either looking out this actor's eyes, or this actor is the player
// and the player is looking out the eyes of something non-"sentient."
//
//============================================================================

bool AActor::CheckLocalView (int playernum) const
{
	if (players[playernum].camera == this)
	{
		return true;
	}
	if (players[playernum].mo != this || players[playernum].camera == NULL)
	{
		return false;
	}
	if (players[playernum].camera->player == NULL &&
		!(players[playernum].camera->flags3 & MF3_ISMONSTER))
	{
		return true;
	}
	return false;
}

//============================================================================
//
// AActor :: IsVisibleToPlayer
//
// Returns true if this actor should be seen by the console player.
//
//============================================================================

bool AActor::IsVisibleToPlayer() const
{
	// [BB] Safety check. This should never be NULL. Nevertheless, we return true to leave the default ZDoom behavior unaltered.
	if ( players[consoleplayer].camera == NULL )
		return true;
 
	// [BB] We continue to use our old team visibility check.
	//if (VisibleToTeam != 0 && teamplay &&
	//	(signed)(VisibleToTeam-1) != players[consoleplayer].userinfo.GetTeam())
	if ( TEAM_IsActorVisibleToPlayer( this, players[consoleplayer].camera->player ) == false )
		return false;

	const player_t* pPlayer = players[consoleplayer].camera->player;

	if(pPlayer && pPlayer->mo && GetClass()->ActorInfo->VisibleToPlayerClass.Size() > 0)
	{
		bool visible = false;
		for(unsigned int i = 0;i < GetClass()->ActorInfo->VisibleToPlayerClass.Size();++i)
		{
			const PClass *cls = GetClass()->ActorInfo->VisibleToPlayerClass[i];
			if(cls && pPlayer->mo->GetClass()->IsDescendantOf(cls))
			{
				visible = true;
				break;
			}
		}
		if(!visible)
			return false;
	}

	// [BB] Passed all checks.
	return true;
}

//============================================================================
//
// AActor :: ConversationAnimation
//
// Plays a conversation-related animation:
//	 0 = greeting
//   1 = "yes"
//   2 = "no"
//
//============================================================================

void AActor::ConversationAnimation (int animnum)
{
	FState * state = NULL;
	switch (animnum)
	{
	case 0:
		state = FindState(NAME_Greetings);
		break;
	case 1:
		state = FindState(NAME_Yes);
		break;
	case 2:
		state = FindState(NAME_No);
		break;
	}
	if (state != NULL) SetState(state);
}

//============================================================================
//
// AActor :: Touch
//
// Something just touched this actor. Normally used only for inventory items,
// but some Strife monsters also use it.
//
//============================================================================

void AActor::Touch (AActor *toucher)
{
}

//============================================================================
//
// AActor :: Grind
//
// Handles the an actor being crushed by a door, crusher or polyobject.
// Originally part of P_DoCrunch(), it has been made into its own actor
// function so that it could be called from a polyobject without hassle.
// Bool items is true if it should destroy() dropped items, false otherwise.
//============================================================================

bool AActor::Grind(bool items)
{
	// crunch bodies to giblets
	if ((flags & MF_CORPSE) && !(flags3 & MF3_DONTGIB) && (health <= 0))
	{
		FState * state = FindState(NAME_Crush);
		bool isgeneric = false;
		// ZDoom behavior differs from standard as crushed corpses cannot be raised.
		// The reason for the change was originally because of a problem with players,
		// see rh_log entry for February 21, 1999. Don't know if it is still relevant.
		if (state == NULL 									// Only use the default crushed state if:
			&& !(flags & MF_NOBLOOD)						// 1. the monster bleeeds,
			&& (i_compatflags & COMPATF_CORPSEGIBS)			// 2. the compat setting is on,
			&& player == NULL)								// 3. and the thing isn't a player.
		{
			isgeneric = true;
			state = FindState(NAME_GenericCrush);
			if (state != NULL && (sprites[state->sprite].numframes <= 0))
				state = NULL; // If one of these tests fails, do not use that state.
		}
		if (state != NULL && !(flags & MF_ICECORPSE))
		{
			if (this->flags4 & MF4_BOSSDEATH) 
			{
				CALL_ACTION(A_BossDeath, this);
			}
			flags &= ~MF_SOLID;
			flags3 |= MF3_DONTGIB;
			height = radius = 0;
			SetState (state);
			if (isgeneric)	// Not a custom crush state, so colorize it appropriately.
			{
				S_Sound (this, CHAN_BODY, "misc/fallingsplat", 1, ATTN_IDLE);
				PalEntry bloodcolor = GetBloodColor();
				if (bloodcolor!=0) Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
			}
			return false;
		}
		if (!(flags & MF_NOBLOOD))
		{
			if (this->flags4 & MF4_BOSSDEATH) 
			{
				CALL_ACTION(A_BossDeath, this);
			}

			const PClass *i = PClass::FindClass("RealGibs");

			if (i != NULL)
			{
				i = i->GetReplacement();

				const AActor *defaults = GetDefaultByType (i);
				if (defaults->SpawnState == NULL ||
					sprites[defaults->SpawnState->sprite].numframes == 0)
				{ 
					i = NULL;
				}
			}
			if (i == NULL)
			{
				// if there's no gib sprite don't crunch it.
				flags &= ~MF_SOLID;
				flags3 |= MF3_DONTGIB;
				height = radius = 0;
				return false;
			}

			AActor *gib = Spawn (i, x, y, z, ALLOW_REPLACE);
			if (gib != NULL)
			{
				gib->RenderStyle = RenderStyle;
				gib->alpha = alpha;
				gib->height = 0;
				gib->radius = 0;

				// [BB] Apparently Skulltag always has let the clients spawn the gibs.
				// Whether or not this is intentional, if the clients spawn the gibs on
				// their own, they have to mark them as CLIENTSIDEONLY.
				if( NETWORK_InClientMode() )
					gib->ulNetworkFlags |= NETFL_CLIENTSIDEONLY;

				PalEntry bloodcolor = GetBloodColor();
				if (bloodcolor != 0)
					gib->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
			}
			S_Sound (this, CHAN_BODY, "misc/fallingsplat", 1, ATTN_IDLE);
		}
		if (flags & MF_ICECORPSE)
		{
			tics = 1;
			velx = vely = velz = 0;

			// [BC] If we're the server, tell clients to update this thing's tics and
			// velocity.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SetThingTics( this );
				SERVERCOMMANDS_MoveThing( this, CM_VELX|CM_VELY|CM_VELZ );
			}
		}
		else if (player)
		{
			flags |= MF_NOCLIP;
			flags3 |= MF3_DONTGIB;
			renderflags |= RF_INVISIBLE;
		}
		else
		{
			// [BB] Only destroy the actor if it's not needed for a map reset. Otherwise just hide it.
			HideOrDestroyIfSafe ();
		}
		return false;		// keep checking
	}

	// killough 11/98: kill touchy things immediately
	if (flags6 & MF6_TOUCHY && (flags6 & MF6_ARMED || IsSentient()))
    {
		flags6 &= ~MF6_ARMED; // Disarm
		P_DamageMobj (this, NULL, NULL, health, NAME_Crush, DMG_FORCED);  // kill object
		return true;   // keep checking
    }

	if (!(flags & MF_SOLID) || (flags & MF_NOCLIP))
	{
		return false;
	}

	if (!(flags & MF_SHOOTABLE))
	{
		return false;		// assume it is bloody gibs or something
	}
	return true;
}

//============================================================================
//
// AActor :: Massacre
//
// Called by the massacre cheat to kill monsters. Returns true if the monster
// was killed and false if it was already dead.
//============================================================================

bool AActor::Massacre ()
{
	int prevhealth;

	if (health > 0)
	{
		flags |= MF_SHOOTABLE;
		flags2 &= ~(MF2_DORMANT|MF2_INVULNERABLE);
		do
		{
			prevhealth = health;
			P_DamageMobj (this, NULL, NULL, TELEFRAG_DAMAGE, NAME_Massacre);
		}
		while (health != prevhealth && health > 0);	//abort if the actor wasn't hurt.
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------
//
// PROC P_ExplodeMissile
//
//----------------------------------------------------------------------------

void P_ExplodeMissile (AActor *mo, line_t *line, AActor *target)
{
	if (mo->flags3 & MF3_EXPLOCOUNT)
	{
		if (++mo->special2 < mo->special1)
		{
			return;
		}
	}
	mo->velx = mo->vely = mo->velz = 0;
	mo->effects = 0;		// [RH]
	mo->flags &= ~MF_SHOOTABLE;
	
	FState *nextstate=NULL;
	
	// [BB] If a missile hits and kills a player, it removes the SHOOTABLE flag from
	// the killed player. Therefore, the SHOOTABLE check below is never fulfilled.
	// As workaround we check if the target is a player.
	if (target != NULL && ((target->flags & (MF_SHOOTABLE|MF_CORPSE)) || (target->flags6 & MF6_KILLED) || target->player ) )
	{
		if (mo->flags7 & MF7_HITTARGET)	mo->target = target;
		if (mo->flags7 & MF7_HITMASTER)	mo->master = target;
		if (mo->flags7 & MF7_HITTRACER)	mo->tracer = target;
		if (target->flags & MF_NOBLOOD) nextstate = mo->FindState(NAME_Crash);
		if (nextstate == NULL) nextstate = mo->FindState(NAME_Death, NAME_Extreme);
	}
	if (nextstate == NULL) nextstate = mo->FindState(NAME_Death);

	// [BC] Tell clients that this missile blew up.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// No need to do this if the line struck a horizon line.
		if (( line == NULL ) ||
			( line->special != Line_Horizon ))
		{
			SERVERCOMMANDS_MissileExplode( mo, line );
			// [BB] Since SERVERCOMMANDS_MissileExplode doesn't tell the clients the target, the clients
			// always select the Death state. In case another state is used, inform the clients.
			if ( ( nextstate != NULL ) && ( nextstate != mo->FindState(NAME_Death) ) )
				SERVERCOMMANDS_SetThingFrame( mo, nextstate );
		}
	}
	
	if (line != NULL && line->special == Line_Horizon && !(mo->flags3 & MF3_SKYEXPLODE))
	{
		// [RH] Don't explode missiles on horizon lines.
		mo->Destroy ();
		return;
	}

	if (line != NULL && cl_missiledecals)
	{
		int side = P_PointOnLineSide (mo->x, mo->y, line);
		if (line->sidedef[side] == NULL)
			side ^= 1;
		if (line->sidedef[side] != NULL)
		{
			FDecalBase *base = mo->DecalGenerator;
			if (base != NULL)
			{
				// Find the nearest point on the line, and stick a decal there
				fixed_t x, y, z;
				SQWORD num, den;

				den = (SQWORD)line->dx*line->dx + (SQWORD)line->dy*line->dy;
				if (den != 0)
				{
					SDWORD frac;

					num = (SQWORD)(mo->x-line->v1->x)*line->dx+(SQWORD)(mo->y-line->v1->y)*line->dy;
					if (num <= 0)
					{
						frac = 0;
					}
					else if (num >= den)
					{
						frac = 1<<30;
					}
					else
					{
						frac = (SDWORD)(num / (den>>30));
					}

					x = line->v1->x + MulScale30 (line->dx, frac);
					y = line->v1->y + MulScale30 (line->dy, frac);
					z = mo->z;

					F3DFloor * ffloor=NULL;
#ifdef _3DFLOORS
					if (line->sidedef[side^1] != NULL)
					{
						sector_t * backsector = line->sidedef[side^1]->sector;
						extsector_t::xfloor &xf = backsector->e->XFloor;
						// find a 3D-floor to stick to
						for(unsigned int i=0;i<xf.ffloors.Size();i++)
						{
							F3DFloor * rover=xf.ffloors[i];

							if ((rover->flags&(FF_EXISTS|FF_SOLID|FF_RENDERSIDES))==(FF_EXISTS|FF_SOLID|FF_RENDERSIDES))
							{
								if (z<=rover->top.plane->ZatPoint(x, y) && z>=rover->bottom.plane->ZatPoint( x, y))
								{
									ffloor=rover;
									break;
								}
							}
						}
					}
#endif

					// [BC] Servers don't need to spawn decals.
					if ( NETWORK_GetState( ) != NETSTATE_SERVER )
					{
						DImpactDecal::StaticCreate (base->GetDecal (),
							x, y, z, line->sidedef[side], ffloor);
					}
				}
			}
		}
	}

	// play the sound before changing the state, so that AActor::Destroy can call S_RelinkSounds on it and the death state can override it.
	if (mo->DeathSound)
	{
		S_Sound (mo, CHAN_VOICE, mo->DeathSound, 1,
			(mo->flags3 & MF3_FULLVOLDEATH) ? ATTN_NONE : ATTN_NORM);
	}

	// [BB] We need to keep track if the state change changes the flags.
	const DWORD dwSavedMoFlags = mo->flags;
	// [BB] If nextstate is still equal to NULL, mo->SetState (nextstate)
	// returns false and we have to break out here.
	if (!(mo->SetState (nextstate)))
		return;
	// [BB] If the flags were just changed, we'll have to take special care later.
	const bool bFlagsChanged = ( mo->flags != dwSavedMoFlags );
	if (!(mo->ObjectFlags & OF_EuthanizeMe))
	{
		// The rest only applies if the missile actor still exists.
		// [RH] Change render style of exploding rockets
		if (mo->flags5 & MF5_DEHEXPLOSION)
		{
			if (deh.ExplosionStyle == 255)
			{
				if (addrocketexplosion)
				{
					mo->RenderStyle = STYLE_Add;
					mo->alpha = FRACUNIT;
				}
				else
				{
					mo->RenderStyle = STYLE_Translucent;
					mo->alpha = FRACUNIT*2/3;
				}
			}
			else
			{
				mo->RenderStyle = ERenderStyle(deh.ExplosionStyle);
				mo->alpha = deh.ExplosionAlpha;
			}
		}

		if (mo->flags4 & MF4_RANDOMIZE)
		{
			mo->tics -= (pr_explodemissile() & 3) * TICRATE / 35;
			if (mo->tics < 1)
				mo->tics = 1;
		}

		mo->flags &= ~MF_MISSILE;

		// [BB] Even though the client removes the missle flag on its own, due to timing issues
		// (e.g. A_ChangeFlag at the beginning of the death state of the missile) we have to
		// resync the flags in case mo->SetState (nextstate) altered the flags. This is because
		// the server tells the client to explode the missile before the server calls
		// mo->SetState (nextstate).
		if ( bFlagsChanged && ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			SERVERCOMMANDS_SetThingFlags( mo, FLAGSET_FLAGS );

	}
}


void AActor::PlayBounceSound(bool onfloor)
{
	// [BB] Skulltag's old hacky bounce sound code. Should be removed eventually.
	// [BC] Actors don't yet have a bounce sound, so this will have to be hacked for now.
	if ( ulSTFlags & STFL_USESTBOUNCESOUND )
	{
		S_Sound( this, CHAN_VOICE, "weapons/grbnce", 1, ATTN_IDLE );
		return;
	}

	if (!onfloor && (BounceFlags & BOUNCE_NoWallSound))
	{
		return;
	}

	if (!(BounceFlags & BOUNCE_Quiet))
	{
		if (BounceFlags & BOUNCE_UseSeeSound)
		{
			S_Sound (this, CHAN_VOICE, SeeSound, 1, ATTN_IDLE);
		}
		else if (onfloor || WallBounceSound <= 0)
		{
			S_Sound (this, CHAN_VOICE, BounceSound, 1, ATTN_IDLE);
		}
		else
		{
			S_Sound (this, CHAN_VOICE, WallBounceSound, 1, ATTN_IDLE);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_FloorBounceMissile
//
// Returns true if the missile was destroyed
//----------------------------------------------------------------------------

bool AActor::FloorBounceMissile (secplane_t &plane)
{
	if (z <= floorz && P_HitFloor (this))
	{
		// Landed in some sort of liquid
		if (BounceFlags & BOUNCE_ExplodeOnWater)
		{
			if (flags & MF_MISSILE)
				P_ExplodeMissile(this, NULL, NULL);
			else
				Die(NULL, NULL);
			return true;
		}
		if (!(BounceFlags & BOUNCE_CanBounceWater))
		{
			// [BB] Inform the clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( this );

			Destroy ();
			return true;
		}
	}

	if (plane.c < 0)
	{ // on ceiling
		if (!(BounceFlags & BOUNCE_Ceilings))
			return true;
	}
	else
	{ // on floor
		if (!(BounceFlags & BOUNCE_Floors))
			return true;
	}

	// The amount of bounces is limited
	if (bouncecount>0 && --bouncecount==0)
	{
		if (flags & MF_MISSILE)
			P_ExplodeMissile(this, NULL, NULL);
		else
			Die(NULL, NULL);
		return true;
	}

	fixed_t dot = TMulScale16 (velx, plane.a, vely, plane.b, velz, plane.c);

	if (BounceFlags & (BOUNCE_HereticType | BOUNCE_MBF))
	{
		velx -= MulScale15 (plane.a, dot);
		vely -= MulScale15 (plane.b, dot);
		velz -= MulScale15 (plane.c, dot);
		angle = R_PointToAngle2 (0, 0, velx, vely);
		if (!(BounceFlags & BOUNCE_MBF)) // Heretic projectiles die, MBF projectiles don't.
		{
			flags |= MF_INBOUNCE;
			SetState (FindState(NAME_Death));
			flags &= ~MF_INBOUNCE;
			return false;
		}
		else velz = FixedMul(velz, bouncefactor);
	}
	else // Don't run through this for MBF-style bounces
	{
		// The reflected velocity keeps only about 70% of its original speed
		velx = FixedMul (velx - MulScale15 (plane.a, dot), bouncefactor);
		vely = FixedMul (vely - MulScale15 (plane.b, dot), bouncefactor);
		velz = FixedMul (velz - MulScale15 (plane.c, dot), bouncefactor);
		angle = R_PointToAngle2 (0, 0, velx, vely);
	}

	PlayBounceSound(true);

	// Set bounce state
	if (BounceFlags & BOUNCE_UseBounceState)
	{
		FName names[2];
		FState *bouncestate;

		names[0] = NAME_Bounce;
		names[1] = plane.c < 0 ? NAME_Ceiling : NAME_Floor;
		bouncestate = FindState(2, names);
		if (bouncestate != NULL)
		{
			SetState(bouncestate);
		}
	}

	if (BounceFlags & BOUNCE_MBF) // Bring it to rest below a certain speed
	{
		if (abs(velz) < (fixed_t)(Mass * GetGravity() / 64))
			velz = 0;
	}
	else if (BounceFlags & (BOUNCE_AutoOff|BOUNCE_AutoOffFloorOnly))
	{
		if (plane.c > 0 || (BounceFlags & BOUNCE_AutoOff))
		{
			// AutoOff only works when bouncing off a floor, not a ceiling (or in compatibility mode.)
			if (!(flags & MF_NOGRAVITY) && (velz < 3*FRACUNIT))
				BounceFlags &= ~BOUNCE_TypeMask;
		}
	}
	return false;
}

//----------------------------------------------------------------------------
//
// PROC P_ThrustMobj
//
//----------------------------------------------------------------------------

void P_ThrustMobj (AActor *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;
	mo->velx += FixedMul (move, finecosine[angle]);
	mo->vely += FixedMul (move, finesine[angle]);
}

//----------------------------------------------------------------------------
//
// FUNC P_FaceMobj
//
// Returns 1 if 'source' needs to turn clockwise, or 0 if 'source' needs
// to turn counter clockwise.  'delta' is set to the amount 'source'
// needs to turn.
//
//----------------------------------------------------------------------------

int P_FaceMobj (AActor *source, AActor *target, angle_t *delta)
{
	angle_t diff;
	angle_t angle1;
	angle_t angle2;

	angle1 = source->angle;
	angle2 = R_PointToAngle2 (source->x, source->y, target->x, target->y);
	if (angle2 > angle1)
	{
		diff = angle2 - angle1;
		if (diff > ANGLE_180)
		{
			*delta = ANGLE_MAX - diff;
			return 0;
		}
		else
		{
			*delta = diff;
			return 1;
		}
	}
	else
	{
		diff = angle1 - angle2;
		if (diff > ANGLE_180)
		{
			*delta = ANGLE_MAX - diff;
			return 1;
		}
		else
		{
			*delta = diff;
			return 0;
		}
	}
}

//----------------------------------------------------------------------------
//
// CanSeek
//
// Checks if a seeker missile can home in on its target
//
//----------------------------------------------------------------------------

bool AActor::CanSeek(AActor *target) const
{
	if (target->flags5 & MF5_CANTSEEK) return false;
	if ((flags2 & MF2_DONTSEEKINVISIBLE) && 
		((target->flags & MF_SHADOW) || 
		 (target->renderflags & RF_INVISIBLE) || 
		 !target->RenderStyle.IsVisible(target->alpha)
		)
	   ) return false;
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_SeekerMissile
//
// The missile's tracer field must be the target.  Returns true if
// target was tracked, false if not.
//
//----------------------------------------------------------------------------

bool P_SeekerMissile (AActor *actor, angle_t thresh, angle_t turnMax, bool precise, bool usecurspeed)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;
	fixed_t speed;

	// [BC] This is handled server-side.
	if ( NETWORK_InClientMode() )
	{
		return ( false );
	}

	speed = !usecurspeed ? actor->Speed : xs_CRoundToInt(TVector3<double>(actor->velx, actor->vely, actor->velz).Length());
	target = actor->tracer;
	if (target == NULL || !actor->CanSeek(target))
	{
		return false;
	}
	if (!(target->flags & MF_SHOOTABLE))
	{ // Target died
		actor->tracer = NULL;
		return false;
	}
	if (speed == 0)
	{ // Technically, we're not seeking since our speed is 0, but the target *is* seekable.
		return true;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if (delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if (dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	
	if (!precise)
	{
		actor->velx = FixedMul (speed, finecosine[angle]);
		actor->vely = FixedMul (speed, finesine[angle]);

		if (!(actor->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
		{
			if (actor->z + actor->height < target->z ||
				target->z + target->height < actor->z)
			{ // Need to seek vertically
				dist = P_AproxDistance (target->x - actor->x, target->y - actor->y);
				dist = dist / speed;
				if (dist < 1)
				{
					dist = 1;
				}
				actor->velz = ((target->z+target->height/2) - (actor->z+actor->height/2)) / dist;
			}
		}
	}
	else
	{
		angle_t pitch = 0;
		if (!(actor->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER)))
		{ // Need to seek vertically
			double dist = MAX(1.0, FVector2(target->x - actor->x, target->y - actor->y).Length());
			// Aim at a player's eyes and at the middle of the actor for everything else.
			fixed_t aimheight = target->height/2;
			if (target->IsKindOf(RUNTIME_CLASS(APlayerPawn)))
			{
				aimheight = static_cast<APlayerPawn *>(target)->ViewHeight;
			}
			pitch = R_PointToAngle2(0, actor->z + actor->height/2, xs_CRoundToInt(dist), target->z + aimheight);
			pitch >>= ANGLETOFINESHIFT;
		}

		fixed_t xyscale = FixedMul(speed, finecosine[pitch]);
		actor->velz = FixedMul(speed, finesine[pitch]);
		actor->velx = FixedMul(xyscale, finecosine[angle]);
		actor->vely = FixedMul(xyscale, finesine[angle]);
	}


	// [BC] Update the thing's angle and velocity.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_MoveThingExact( actor, CM_X|CM_Y|CM_Z|CM_ANGLE|CM_VELX|CM_VELY|CM_VELZ );

	return true;
}


//
// P_XYMovement
//
// Returns the actor's old floorz.
//
#define STOPSPEED			0x1000
#define CARRYSTOPSPEED		(STOPSPEED*32/3)

fixed_t P_XYMovement (AActor *mo, fixed_t scrollx, fixed_t scrolly) 
{
	static int pushtime = 0;
	bool bForceSlide = scrollx || scrolly;
	angle_t angle;
	fixed_t ptryx, ptryy;
	player_t *player;
	fixed_t xmove, ymove;
	const secplane_t * walkplane;
	static const int windTab[3] = {2048*5, 2048*10, 2048*25};
	int steps, step, totalsteps;
	fixed_t startx, starty;
	fixed_t oldfloorz = mo->floorz;

	fixed_t maxmove = (mo->waterlevel < 1) || (mo->flags & MF_MISSILE) || 
					  (mo->player && mo->player->crouchoffset<-10*FRACUNIT) ? MAXMOVE : MAXMOVE/4;

	if (mo->flags2 & MF2_WINDTHRUST && mo->waterlevel < 2 && !(mo->flags & MF_NOCLIP)
		&& ( !mo->player || !mo->player->bSpectating ) ) // [BB] Don't affect spectators.
	{
		int special = mo->Sector->special;
		switch (special)
		{
			case 40: case 41: case 42: // Wind_East
				P_ThrustMobj (mo, 0, windTab[special-40]);
				break;
			case 43: case 44: case 45: // Wind_North
				P_ThrustMobj (mo, ANG90, windTab[special-43]);
				break;
			case 46: case 47: case 48: // Wind_South
				P_ThrustMobj (mo, ANG270, windTab[special-46]);
				break;
			case 49: case 50: case 51: // Wind_West
				P_ThrustMobj (mo, ANG180, windTab[special-49]);
				break;
		}
	}

	// [RH] No need to clamp these now. However, wall running needs it so
	// that large thrusts can't propel an actor through a wall, because wall
	// running depends on the player's original movement continuing even after
	// it gets blocked.
	if ((mo->player != NULL && (i_compatflags & COMPATF_WALLRUN)) || (mo->waterlevel >= 1) ||
		(mo->player != NULL && mo->player->crouchfactor < FRACUNIT*3/4))
	{
		// preserve the direction instead of clamping x and y independently.
		xmove = clamp (mo->velx, -maxmove, maxmove);
		ymove = clamp (mo->vely, -maxmove, maxmove);

		fixed_t xfac = FixedDiv(xmove, mo->velx);
		fixed_t yfac = FixedDiv(ymove, mo->vely);
		fixed_t fac = MIN(xfac, yfac);

		xmove = mo->velx = FixedMul(mo->velx, fac);
		ymove = mo->vely = FixedMul(mo->vely, fac);
	}
	else
	{
		xmove = mo->velx;
		ymove = mo->vely;
	}
	// [RH] Carrying sectors didn't work with low speeds in BOOM. This is
	// because BOOM relied on the speed being fast enough to accumulate
	// despite friction. If the speed is too low, then its movement will get
	// cancelled, and it won't accumulate to the desired speed.
	mo->flags4 &= ~MF4_SCROLLMOVE;
	if (abs(scrollx) > CARRYSTOPSPEED)
	{
		scrollx = FixedMul (scrollx, CARRYFACTOR);
		mo->velx += scrollx;
		mo->flags4 |= MF4_SCROLLMOVE;
	}
	if (abs(scrolly) > CARRYSTOPSPEED)
	{
		scrolly = FixedMul (scrolly, CARRYFACTOR);
		mo->vely += scrolly;
		mo->flags4 |= MF4_SCROLLMOVE;
	}
	xmove += scrollx;
	ymove += scrolly;

	if ((xmove | ymove) == 0)
	{
		if ( (mo->flags & MF_SKULLFLY)
			 && ( NETWORK_InClientMode() == false ) )
		{
			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->velx = mo->vely = mo->velz = 0;

			// [BB] If we're the server, tell clients of MF_SKULLFLY.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingFlags( mo, FLAGSET_FLAGS );

			if (!(mo->flags2 & MF2_DORMANT))
			{
				// [BC] If we are the server, tell clients about the state change and the
				// velocity change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetThingState( mo, mo->SeeState != NULL ? STATE_SEE : STATE_IDLE );
					SERVERCOMMANDS_MoveThing( mo, CM_VELX|CM_VELY|CM_VELZ );
				}

				if (mo->SeeState != NULL) mo->SetState (mo->SeeState);
				else mo->SetIdle();
			}
			else
			{
				// [BB] If we are the server, tell clients about the state change and the
				// velocity change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SetThingState( mo, STATE_IDLE );
					SERVERCOMMANDS_MoveThing( mo, CM_VELX|CM_VELY|CM_VELZ );
				}

				mo->SetIdle();
				mo->tics = -1;

				// [BB] If we're the server, tell clients to update this thing's tics.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingTics( mo );
			}
		}
		return oldfloorz;
	}

	player = mo->player;

	// [RH] Adjust player movement on sloped floors
	fixed_t startxmove = xmove;
	fixed_t startymove = ymove;
	walkplane = P_CheckSlopeWalk (mo, xmove, ymove);

	// [RH] Take smaller steps when moving faster than the object's size permits.
	// Moving as fast as the object's "diameter" is bad because it could skip
	// some lines because the actor could land such that it is just touching the
	// line. For Doom to detect that the line is there, it needs to actually cut
	// through the actor.

	{
		maxmove = mo->radius - FRACUNIT;

		if (maxmove <= 0)
		{ // gibs can have radius 0, so don't divide by zero below!
			maxmove = MAXMOVE;
		}

		const fixed_t xspeed = abs (xmove);
		const fixed_t yspeed = abs (ymove);

		steps = 1;

		if (xspeed > yspeed)
		{
			if (xspeed > maxmove)
			{
				steps = 1 + xspeed / maxmove;
			}
		}
		else
		{
			if (yspeed > maxmove)
			{
				steps = 1 + yspeed / maxmove;
			}
		}
	}

	// P_SlideMove needs to know the step size before P_CheckSlopeWalk
	// because it also calls P_CheckSlopeWalk on its clipped steps.
	fixed_t onestepx = startxmove / steps;
	fixed_t onestepy = startymove / steps;

	startx = mo->x;
	starty = mo->y;
	step = 1;
	totalsteps = steps;

	// [RH] Instead of doing ripping damage each step, do it each tic.
	// This makes it compatible with Heretic and Hexen, which only did
	// one step for their missiles with ripping damage (excluding those
	// that don't use P_XYMovement). It's also more intuitive since it
	// makes the damage done dependant on the amount of time the projectile
	// spends inside a target rather than on the projectile's size. The
	// last actor ripped through is recorded so that if the projectile
	// passes through more than one actor this tic, each one takes damage
	// and not just the first one.
	pushtime++;

	FCheckPosition tm(!!(mo->flags2 & MF2_RIP));


	do
	{
		if (i_compatflags & COMPATF_WALLRUN) pushtime++;
		tm.PushTime = pushtime;

		ptryx = startx + Scale (xmove, step, steps);
		ptryy = starty + Scale (ymove, step, steps);

/*		if (mo->player)
		Printf ("%d,%d/%d: %d %d %d %d %d %d %d\n", level.time, step, steps, startxmove, Scale(xmove,step,steps), startymove, Scale(ymove,step,steps), mo->x, mo->y, mo->z);
*/
		// [RH] If walking on a slope, stay on the slope
		// killough 3/15/98: Allow objects to drop off
		fixed_t startvelx = mo->velx, startvely = mo->vely;

		if (!P_TryMove (mo, ptryx, ptryy, true, walkplane, tm))
		{
			// blocked move
			AActor *BlockingMobj = mo->BlockingMobj;
			line_t *BlockingLine = mo->BlockingLine;

			if (!(mo->flags & MF_MISSILE) && (mo->BounceFlags & BOUNCE_MBF) 
				&& (BlockingMobj != NULL ? P_BounceActor(mo, BlockingMobj, false) : P_BounceWall(mo)))
			{
				// Do nothing, relevant actions already done in the condition.
				// This allows to avoid setting velocities to 0 in the final else of this series.
			}
			else if ((mo->flags2 & (MF2_SLIDE|MF2_BLASTED) || bForceSlide) && !(mo->flags&MF_MISSILE))
			{	// try to slide along it
				if (BlockingMobj == NULL)
				{ // slide against wall
					if (BlockingLine != NULL &&
						mo->player && mo->waterlevel && mo->waterlevel < 3 &&
						(mo->player->cmd.ucmd.forwardmove | mo->player->cmd.ucmd.sidemove) &&
						mo->BlockingLine->sidedef[1] != NULL)
					{
						mo->velz = WATER_JUMP_SPEED;
					}
					// If the blocked move executed any push specials that changed the
					// actor's velocity, do not attempt to slide.
					if (mo->velx == startvelx && mo->vely == startvely)
					{
						if (player && (i_compatflags & COMPATF_WALLRUN))
						{
						// [RH] Here is the key to wall running: The move is clipped using its full speed.
						// If the move is done a second time (because it was too fast for one move), it
						// is still clipped against the wall at its full speed, so you effectively
						// execute two moves in one tic.
							P_SlideMove (mo, mo->velx, mo->vely, 1);
						}
						else
						{
							P_SlideMove (mo, onestepx, onestepy, totalsteps);
						}
						if ((mo->velx | mo->vely) == 0)
						{
							steps = 0;
						}
						else
						{
							if (!player || !(i_compatflags & COMPATF_WALLRUN))
							{
								xmove = mo->velx;
								ymove = mo->vely;
								onestepx = xmove / steps;
								onestepy = ymove / steps;
								P_CheckSlopeWalk (mo, xmove, ymove);
							}
							startx = mo->x - Scale (xmove, step, steps);
							starty = mo->y - Scale (ymove, step, steps);
						}
					}
					else
					{
						steps = 0;
					}
				}
				else
				{ // slide against another actor
					fixed_t tx, ty;
					tx = 0, ty = onestepy;
					walkplane = P_CheckSlopeWalk (mo, tx, ty);
					if (P_TryMove (mo, mo->x + tx, mo->y + ty, true, walkplane, tm))
					{
						mo->velx = 0;
					}
					else
					{
						tx = onestepx, ty = 0;
						walkplane = P_CheckSlopeWalk (mo, tx, ty);
						if (P_TryMove (mo, mo->x + tx, mo->y + ty, true, walkplane, tm))
						{
							mo->vely = 0;
						}
						else
						{
							mo->velx = mo->vely = 0;
						}
					}
					if (player && player->mo == mo)
					{
						if (mo->velx == 0)
							player->velx = 0;
						if (mo->vely == 0)
							player->vely = 0;
					}
					steps = 0;
				}
			}
			else if (mo->flags & MF_MISSILE)
			{
				steps = 0;
				if (BlockingMobj)
				{
					// [BB] The server handles this.
					if ( (mo->BounceFlags & BOUNCE_Actors) && ( NETWORK_InClientMode() == false ) )
					{
						// Bounce test and code moved to P_BounceActor
						if (!P_BounceActor(mo, BlockingMobj, false))
						{	// Struck a player/creature
						
							// Potentially reward the player who shot this missile with an accuracy/precision medal.
							if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
							{
								if ( mo->target->player->bStruckPlayer )
									PLAYER_StruckPlayer( mo->target->player );
								else
									mo->target->player->ulConsecutiveHits = 0;
							}

							P_ExplodeMissile (mo, NULL, BlockingMobj);
						}
						return oldfloorz;
					}
				}
				else
				{
					// Struck a wall
					if (P_BounceWall (mo))
					{
						// [BC] If the object was destroyed in P_BounceWall, no need to play
						// the bounce sound.
						if (( mo->ObjectFlags & OF_EuthanizeMe ) == false )
						{
							mo->PlayBounceSound(false);
						}
						return oldfloorz;
					}
				}
				if (BlockingMobj && (BlockingMobj->flags2 & MF2_REFLECTIVE))
				{
					angle = R_PointToAngle2(BlockingMobj->x, BlockingMobj->y, mo->x, mo->y);

					// Change angle for deflection/reflection
					if (mo->AdjustReflectionAngle (BlockingMobj, angle))
					{
						goto explode;
					}

					// Reflect the missile along angle
					mo->angle = angle;
					angle >>= ANGLETOFINESHIFT;
					mo->velx = FixedMul (mo->Speed>>1, finecosine[angle]);
					mo->vely = FixedMul (mo->Speed>>1, finesine[angle]);
					mo->velz = -mo->velz/2;
					if (mo->flags2 & MF2_SEEKERMISSILE)
					{
						mo->tracer = mo->target;
					}
					mo->target = BlockingMobj;

					// [CK/BB] Inform the client about the reflection.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						mo->ulNetworkFlags |= NETFL_BOUNCED_OFF_ACTOR;

					return oldfloorz;
				}
explode:
				// explode a missile
				if (!(mo->flags3 & MF3_SKYEXPLODE))
				{
					if (tm.ceilingline &&
						tm.ceilingline->backsector &&
						tm.ceilingline->backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
						mo->z >= tm.ceilingline->backsector->ceilingplane.ZatPoint (mo->x, mo->y))
					{
						// Hack to prevent missiles exploding against the sky.
						// Does not handle sky floors.

						// Player didn't strike another player with this missile.
						if ( mo->target && mo->target->player )
							mo->target->player->ulConsecutiveHits = 0;

						// [Dusk] Tell the clients that the mobj was deleted
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DestroyThing( mo );

						mo->Destroy ();
						return oldfloorz;
					}

					// Potentially reward the player who shot this missile with an accuracy/precision medal.
					if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
					{
						if ( mo->target->player->bStruckPlayer )
							PLAYER_StruckPlayer( mo->target->player );
						else
							mo->target->player->ulConsecutiveHits = 0;
					}

					// [RH] Don't explode on horizon lines.
					if (mo->BlockingLine != NULL && mo->BlockingLine->special == Line_Horizon)
					{
						// [Dusk] Tell the clients that the mobj was deleted
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DestroyThing( mo );

						mo->Destroy ();
						return oldfloorz;
					}
				}

				// [BB] Clients may not explode server handled bouncing missiles on their own if they hit another actor.
				if ( ( NETWORK_InClientMode() == false ) || ( !mo->BounceFlags ) || NETWORK_IsActorClientHandled ( mo ) || ( mo->BlockingLine != NULL ) )
					P_ExplodeMissile (mo, mo->BlockingLine, BlockingMobj);
				return oldfloorz;
			}
			else
			{
				mo->velx = mo->vely = 0;
				steps = 0;
			}
		}
		else
		{
			if (mo->x != ptryx || mo->y != ptryy)
			{
				// If the new position does not match the desired position, the player
				// must have gone through a teleporter, so stop moving right now if it
				// was a regular teleporter. If it was a line-to-line or fogless teleporter,
				// the move should continue, but startx and starty need to change.
				if (mo->velx == 0 && mo->vely == 0)
				{
					step = steps;
				}
				else
				{
					startx = mo->x - Scale (xmove, step, steps);
					starty = mo->y - Scale (ymove, step, steps);
				}
			}
		}
	} while (++step <= steps);

	// Friction

	if (player && player->mo == mo && player->cheats & CF_NOVELOCITY)
	{ // debug option for no sliding at all
		mo->velx = mo->vely = 0;
		player->velx = player->vely = 0;
		return oldfloorz;
	}

	if (mo->flags & (MF_MISSILE | MF_SKULLFLY))
	{ // no friction for missiles
		return oldfloorz;
	}

	if (mo->z > mo->floorz && !(mo->flags2 & MF2_ONMOBJ) &&
		!mo->IsNoClip2() &&
		(!(mo->flags2 & MF2_FLY) || !(mo->flags & MF_NOGRAVITY)) &&
		!mo->waterlevel)
	{ // [RH] Friction when falling is available for larger aircontrols
		if (player != NULL && level.airfriction != FRACUNIT)
		{
			mo->velx = FixedMul (mo->velx, level.airfriction);
			mo->vely = FixedMul (mo->vely, level.airfriction);

			if (player->mo == mo)		//  Not voodoo dolls
			{
				player->velx = FixedMul (player->velx, level.airfriction);
				player->vely = FixedMul (player->vely, level.airfriction);
			}
		}
		return oldfloorz;
	}

	// killough 8/11/98: add bouncers
	// killough 9/15/98: add objects falling off ledges
	// killough 11/98: only include bouncers hanging off ledges
	if ((mo->flags & MF_CORPSE) || (mo->BounceFlags & BOUNCE_MBF && mo->z > mo->dropoffz) || (mo->flags6 & MF6_FALLING))
	{ // Don't stop sliding if halfway off a step with some velocity
		if (mo->velx > FRACUNIT/4 || mo->velx < -FRACUNIT/4 || mo->vely > FRACUNIT/4 || mo->vely < -FRACUNIT/4)
		{
			if (mo->floorz > mo->Sector->floorplane.ZatPoint (mo->x, mo->y))
			{
				if (mo->dropoffz != mo->floorz) // 3DMidtex or other special cases that must be excluded
				{
#ifdef _3DFLOORS
					unsigned i;
					for(i=0;i<mo->Sector->e->XFloor.ffloors.Size();i++)
					{
						// Sliding around on 3D floors looks extremely bad so
						// if the floor comes from one in the current sector stop sliding the corpse!
						F3DFloor * rover=mo->Sector->e->XFloor.ffloors[i];
						if (!(rover->flags&FF_EXISTS)) continue;
						if (rover->flags&FF_SOLID && rover->top.plane->ZatPoint(mo->x,mo->y)==mo->floorz) break;
					}
					if (i==mo->Sector->e->XFloor.ffloors.Size()) 
#endif
						return oldfloorz;
				}
			}
		}
	}

	// killough 11/98:
	// Stop voodoo dolls that have come to rest, despite any
	// moving corresponding player:
	if (mo->velx > -STOPSPEED && mo->velx < STOPSPEED
		&& mo->vely > -STOPSPEED && mo->vely < STOPSPEED
		&& (!player || (player->mo != mo)
			|| !(player->cmd.ucmd.forwardmove | player->cmd.ucmd.sidemove)))
	{
		// if in a walking frame, stop moving
		// killough 10/98:
		// Don't affect main player when voodoo dolls stop:
		if ((player && player->mo == mo) && ( CLIENT_PREDICT_IsPredicting( ) == false ))// && !(player->cheats & CF_PREDICTING))
		{
			// [BC] In client mode, we don't know if other players have any forwardmove or
			// sidemove values, so the server will tell us when to put other players in
			// idle mode.
			if (( NETWORK_InClientMode() == false ) ||
				(( player - players ) == consoleplayer ))
			{
				player->mo->PlayIdle ();
			}
		}

		mo->velx = mo->vely = 0;
		mo->flags4 &= ~MF4_SCROLLMOVE;

		// killough 10/98: kill any bobbing velocity too (except in voodoo dolls)
		if (player && player->mo == mo)
			player->velx = player->vely = 0; 
	}
	else
	{
		// phares 3/17/98
		// Friction will have been adjusted by friction thinkers for icy
		// or muddy floors. Otherwise it was never touched and
		// remained set at ORIG_FRICTION
		//
		// killough 8/28/98: removed inefficient thinker algorithm,
		// instead using touching_sectorlist in P_GetFriction() to
		// determine friction (and thus only when it is needed).
		//
		// killough 10/98: changed to work with new bobbing method.
		// Reducing player velocity is no longer needed to reduce
		// bobbing, so ice works much better now.

		fixed_t friction = P_GetFriction (mo, NULL);

		mo->velx = FixedMul (mo->velx, friction);
		mo->vely = FixedMul (mo->vely, friction);

		// killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
		// This prevents problems with bobbing on ice, where it was not being
		// reduced fast enough, leading to all sorts of kludges being developed.

		if (player && player->mo == mo)		//  Not voodoo dolls
		{
			player->velx = FixedMul (player->velx, ORIG_FRICTION);
			player->vely = FixedMul (player->vely, ORIG_FRICTION);
		}
	}
	return oldfloorz;
}

//*****************************************************************************
// prBoom version of XY movement.
fixed_t P_OldXYMovement( AActor *mo )
{
	player_t *player;
	fixed_t ptryx, ptryy;
	fixed_t xmove, ymove;
	fixed_t	maxmove;
	fixed_t oldfloorz = mo->floorz;

	maxmove = MAXMOVE;

	xmove = clamp( mo->velx, -maxmove, maxmove );
	ymove = clamp( mo->vely, -maxmove, maxmove );

	if (!(mo->velx | mo->vely)) // Any velocity?
	{
		if (mo->flags & MF_SKULLFLY)
		{

			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->velz = 0;

			mo->SetState (mo->SeeState != NULL ? mo->SeeState : mo->SpawnState);
		}
		return oldfloorz;
	}

	player = mo->player;

	maxmove /= 2;

	do
	{
		// killough 8/9/98: fix bug in original Doom source:
		// Large negative displacements were never considered.
		// This explains the tendency for Mancubus fireballs
		// to pass through walls.
		// CPhipps - compatibility optioned
/*
		if ( 0 )
//		if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2 ||
//			xmove < -MAXMOVE/2 || ymove < -MAXMOVE/2)
		{
			ptryx = mo->x + xmove/2;
			ptryy = mo->y + ymove/2;
			xmove >>= 1;
			ymove >>= 1;
		}
*/
		if (xmove > maxmove || ymove > maxmove)
		{
			ptryx = mo->x + (xmove >>= 1);
			ptryy = mo->y + (ymove >>= 1);
		}
		else
		{
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
			xmove = ymove = 0;
		}

		// killough 3/15/98: Allow objects to drop off

		if (!P_OldTryMove (mo, ptryx, ptryy, true))
		{
			// blocked move

			// killough 8/11/98: bouncing off walls
			// killough 10/98:
			// Add ability for objects other than players to bounce on ice
/*	  
			if (!(mo->flags & MF_MISSILE) && 
				(mo->flags & MF_BOUNCES ||
				(!player && blockline &&
				variable_friction && mo->z <= mo->floorz &&
				P_GetFriction(mo, NULL) > ORIG_FRICTION)))
			{
				if (blockline)
				{
					fixed_t r = ((blockline->dx >> FRACBITS) * mo->velx +
						(blockline->dy >> FRACBITS) * mo->vely) /
						((blockline->dx >> FRACBITS)*(blockline->dx >> FRACBITS)+
						(blockline->dy >> FRACBITS)*(blockline->dy >> FRACBITS));
				
					fixed_t x = FixedMul(r, blockline->dx);
					fixed_t y = FixedMul(r, blockline->dy);

					// reflect momentum away from wall
					mo->velx = x*2 - mo->velx;
					mo->vely = y*2 - mo->vely;

					// if under gravity, slow down in
					// direction perpendicular to wall.
					if (!(mo->flags & MF_NOGRAVITY))
					{
						mo->velx = (mo->velx + x)/2;
						mo->vely = (mo->vely + y)/2;
					}
				}
				else
					mo->velx = mo->vely = 0;
			}
*/
			if ( 0 )
			{
			}
			else
			{
				if (player)   // try to slide along it
					P_OldSlideMove (mo);

				// [BC] This function is only potentially being used for player movement.
/*
				else 
				{
					if (mo->flags & MF_MISSILE)
					{
						// explode a missile

						if (ceilingline &&
							ceilingline->backsector &&
							ceilingline->backsector->ceilingpic == skyflatnum)
							if (demo_compatibility ||  // killough
							mo->z > ceilingline->backsector->ceilingheight)
						{
							// Hack to prevent missiles exploding
							// against the sky.
							// Does not handle sky floors.

							P_RemoveMobj (mo);
							return;
						}
						P_ExplodeMissile (mo);
					}
					else // whatever else it is, it is now standing still in (x,y)
						mo->velx = mo->vely = 0;
				}
*/
			}
		}
	} while (xmove || ymove);

	// slow down

	/* no friction for missiles or skulls ever, no friction when airborne */
	if (mo->flags & (MF_MISSILE | MF_SKULLFLY) || mo->z > mo->floorz)
		return oldfloorz; 

	if (mo->flags & MF_CORPSE)
	{ // Don't stop sliding if halfway off a step with some momentum
		if (mo->velx > FRACUNIT/4 || mo->velx < -FRACUNIT/4
			|| mo->vely > FRACUNIT/4 || mo->vely < -FRACUNIT/4)
		{
			if (mo->floorz > mo->Sector->floorplane.ZatPoint (mo->x, mo->y))
				return oldfloorz;
		}
	}

	// killough 11/98:
	// Stop voodoo dolls that have come to rest, despite any
	// moving corresponding player:
	if (mo->velx > -STOPSPEED && mo->velx < STOPSPEED
		&& mo->vely > -STOPSPEED && mo->vely < STOPSPEED
		&& (!player || (player->mo != mo)
			|| !(player->cmd.ucmd.forwardmove | player->cmd.ucmd.sidemove)))
	{
		// if in a walking frame, stop moving
		// killough 10/98:
		// Don't affect main player when voodoo dolls stop:
		if ((player && player->mo == mo) && ( CLIENT_PREDICT_IsPredicting( ) == false ))// && !(player->cheats & CF_PREDICTING))
		{
			// [BC] In client mode, we don't know if other players have any forwardmove or
			// sidemove values, so the server will tell us when to put other players in
			// idle mode.
			if (( NETWORK_InClientMode() == false ) ||
				(( player - players ) == consoleplayer ))
			{
				player->mo->PlayIdle ();
			}
		}

		mo->velx = mo->vely = 0;

		// killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
		if (player && player->mo == mo)
			player->velx = player->vely = 0; 
	}
	else
	{
		/* phares 3/17/98
		*
		* Friction will have been adjusted by friction thinkers for
		* icy or muddy floors. Otherwise it was never touched and
		* remained set at ORIG_FRICTION
		*
		* killough 8/28/98: removed inefficient thinker algorithm,
		* instead using touching_sectorlist in P_GetFriction() to
		* determine friction (and thus only when it is needed).
		*
		* killough 10/98: changed to work with new bobbing method.
		* Reducing player momentum is no longer needed to reduce
		* bobbing, so ice works much better now.
		*
		* cph - DEMOSYNC - need old code for Boom demos?
		*/

		fixed_t friction = P_GetFriction(mo, NULL);

		mo->velx = FixedMul(mo->velx, friction);
		mo->vely = FixedMul(mo->vely, friction);

		/* killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
		* This prevents problems with bobbing on ice, where it was not being
		* reduced fast enough, leading to all sorts of kludges being developed.
		*/

		if (player && player->mo == mo)     /* Not voodoo dolls */
		{
			player->velx = FixedMul(player->velx, ORIG_FRICTION);
			player->vely = FixedMul(player->vely, ORIG_FRICTION);
		}

    }
	return oldfloorz;
}

// Move this to p_inter ***
void P_MonsterFallingDamage (AActor *mo)
{
	int damage;
	int vel;

	if (!(level.flags2 & LEVEL2_MONSTERFALLINGDAMAGE))
		return;
	if (mo->floorsector->Flags & SECF_NOFALLINGDAMAGE)
		return;

	vel = abs(mo->velz);
	if (vel > 35*FRACUNIT)
	{ // automatic death
		damage = TELEFRAG_DAMAGE;
	}
	else
	{
		damage = ((vel - (23*FRACUNIT))*6)>>FRACBITS;
	}
	damage = TELEFRAG_DAMAGE;	// always kill 'em
	P_DamageMobj (mo, NULL, NULL, damage, NAME_Falling);
}

//
// P_ZMovement
//

void P_ZMovement (AActor *mo, fixed_t oldfloorz)
{
	fixed_t dist;
	fixed_t delta;
	fixed_t oldz = mo->z;
	fixed_t grav = mo->GetGravity();

//
// check for smooth step up
//
	// [BC] Don't adjust viewheight while predicting.
	if ( CLIENT_PREDICT_IsPredicting( ) == false )
	{
		if (mo->player && mo->player->mo == mo && mo->z < mo->floorz)
		{
			mo->player->viewheight -= mo->floorz - mo->z;
			mo->player->deltaviewheight = mo->player->GetDeltaViewHeight();
		}
	}

	// [W] Added old ZDoom physics compatibility
	if (!(zacompatflags & ZACOMPATF_OLD_ZDOOM_ZMOVEMENT))
		mo->z += mo->velz;

//
// apply gravity
//
	if (mo->z > mo->floorz && !(mo->flags & MF_NOGRAVITY))
	{
		fixed_t startvelz = mo->velz;

		if (mo->waterlevel == 0 || (mo->player &&
			!(mo->player->cmd.ucmd.forwardmove | mo->player->cmd.ucmd.sidemove)))
		{
			// [RH] Double gravity only if running off a ledge. Coming down from
			// an upward thrust (e.g. a jump) should not double it.
			// [AM] Old versions of ZDoom didn't have double gravity.
			if (mo->velz == 0 && oldfloorz > mo->floorz && mo->z == oldfloorz &&
				!(zacompatflags & ZACOMPATF_OLD_ZDOOM_ZMOVEMENT))
			{
				mo->velz -= grav + grav;
			}
			else
			{
				mo->velz -= grav;
			}
		}
		if (mo->player == NULL)
		{
			if (mo->waterlevel >= 1)
			{
				fixed_t sinkspeed;

				if ((mo->flags & MF_SPECIAL) && !(mo->flags3 & MF3_ISMONSTER))
				{ // Pickup items don't sink if placed and drop slowly if dropped
					sinkspeed = (mo->flags & MF_DROPPED) ? -WATER_SINK_SPEED / 8 : 0;
				}
				else
				{
					sinkspeed = -WATER_SINK_SPEED;

					// If it's not a player, scale sinkspeed by its mass, with
					// 100 being equivalent to a player.
					if (mo->player == NULL)
					{
						sinkspeed = Scale(sinkspeed, clamp(mo->Mass, 1, 4000), 100);
					}
				}
				if (mo->velz < sinkspeed)
				{ // Dropping too fast, so slow down toward sinkspeed.
					mo->velz -= MAX(sinkspeed*2, -FRACUNIT*8);
					if (mo->velz > sinkspeed)
					{
						mo->velz = sinkspeed;
					}
				}
				else if (mo->velz > sinkspeed)
				{ // Dropping too slow/going up, so trend toward sinkspeed.
					mo->velz = startvelz + MAX(sinkspeed/3, -FRACUNIT*8);
					if (mo->velz < sinkspeed)
					{
						mo->velz = sinkspeed;
					}
				}
			}
		}
		else
		{
			if (mo->waterlevel > 1)
			{
				fixed_t sinkspeed = -WATER_SINK_SPEED;

				if (mo->velz < sinkspeed)
				{
					mo->velz = (startvelz < sinkspeed) ? startvelz : sinkspeed;
				}
				else
				{
					mo->velz = startvelz + ((mo->velz - startvelz) >>
						(mo->waterlevel == 1 ? WATER_SINK_SMALL_FACTOR : WATER_SINK_FACTOR));
				}
			}
		}
	}

	// [W/BB] Apply the old ZDoom gravity here instead
	if ((zacompatflags & ZACOMPATF_OLD_ZDOOM_ZMOVEMENT))
		mo->z += mo->velz;

	// [BC] Mark this item as having moved.
	if ( mo->z != oldz )
		mo->ulSTFlags |= STFL_POSITIONCHANGED;
//
// adjust height
//
	// [BC] Don't float in client mode.
	if ((mo->flags & MF_FLOAT) && !(mo->flags2 & MF2_DORMANT) && mo->target &&
		( NETWORK_InClientMode() == false ))
	{	// float down towards target if too close
		if (!(mo->flags & (MF_SKULLFLY | MF_INFLOAT)))
		{
			dist = P_AproxDistance (mo->x - mo->target->x, mo->y - mo->target->y);
			delta = (mo->target->z + (mo->height>>1)) - mo->z;
			if (delta < 0 && dist < -(delta*3))
			{
				mo->z -= mo->FloatSpeed;

				// [BC] If we're the server, tell clients to update the thing's Z position.
				// [WS] Inform clients of the velocity.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_MoveThingExact( mo, CM_Z|CM_VELZ );
			}
			else if (delta > 0 && dist < (delta*3))
			{
				mo->z += mo->FloatSpeed;

				// [BC] If we're the server, tell clients to update the thing's Z position.
				// [WS] Inform clients of the velocity.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_MoveThingExact( mo, CM_Z|CM_VELZ );
			}
		}
	}
	if (mo->player && (mo->flags & MF_NOGRAVITY) && (mo->z > mo->floorz))
	{
		if (!mo->IsNoClip2())
		{
			mo->z += finesine[(FINEANGLES/80*level.maptime)&FINEMASK]/8;
		}
		mo->velz = FixedMul (mo->velz, FRICTION_FLY);
	}
	if (mo->waterlevel && !(mo->flags & MF_NOGRAVITY))
	{
		mo->velz = FixedMul (mo->velz, mo->Sector->friction);
	}

//
// clip movement
//
	// [WS] For clients, check to see if we are allowed to clip our actor's movement.
	if (mo->z <= mo->floorz && CLIENT_CanClipMovement(mo))
	{	// Hit the floor
		// [BC] Why "!mo->player"? This makes jump pads and stuff not work. They only work
		// if you walk onto the pad while on the ground (ex. you can't jump onto it), but
		// with that check, why would they even work at all? Odd.
/*
		if ((!mo->player || !(mo->player->cheats & CF_PREDICTING)) &&
			mo->Sector->SecActTarget != NULL &&
*/
		if  (mo->Sector->SecActTarget != NULL &&
			mo->Sector->floorplane.ZatPoint (mo->x, mo->y) == mo->floorz)
		{ // [RH] Let the sector do something to the actor
			mo->Sector->SecActTarget->TriggerAction (mo, SECSPAC_HitFloor);
		}
		P_CheckFor3DFloorHit(mo);
		// [RH] Need to recheck this because the sector action might have
		// teleported the actor so it is no longer below the floor.
		if (mo->z <= mo->floorz)
		{
			// [BC] We need to do the sky check first, otherwise bouncy things
			// can potentially bounce off the sky (such as grenades).
			// [BB] But only do this for bouncy things!
			if (( mo->flags & MF_MISSILE ) && ( mo->floorpic == skyflatnum ) && (( mo->flags3 & MF3_SKYEXPLODE ) == false ) && (mo->BounceFlags & BOUNCE_Floors))
			{
				// Player didn't strike another player with this missile.
				if ( mo->target && mo->target->player )
					mo->target->player->ulConsecutiveHits = 0;

				// [Dusk] Tell the clients that the mobj was deleted
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( mo );

				mo->Destroy( );
				return;
			}

			if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
			{
				mo->z = mo->floorz;
				if (mo->BounceFlags & BOUNCE_Floors)
				{
					mo->FloorBounceMissile (mo->floorsector->floorplane);
					/* if (!(mo->flags6 & MF6_CANJUMP)) */ return;
				}
				else if (mo->flags3 & MF3_NOEXPLODEFLOOR)
				{
					P_HitFloor (mo);
					mo->velz = 0;

					// [TP] Tell clients the missile stopped moving vertically
					if ( NETWORK_GetState() == NETSTATE_SERVER )
						SERVERCOMMANDS_MoveThing( mo, CM_VELZ );

					return;
				}
				else if (mo->flags3 & MF3_FLOORHUGGER)
				{ // Floor huggers can go up steps
					return;
				}
				else
				{
					if (mo->floorpic == skyflatnum && !(mo->flags3 & MF3_SKYEXPLODE))
					{
						// [RH] Just remove the missile without exploding it
						//		if this is a sky floor.
						// [Dusk] Tell the clients that the mobj was deleted
						if ( NETWORK_GetState( ) == NETSTATE_SERVER )
							SERVERCOMMANDS_DestroyThing( mo );

						mo->Destroy ();
						return;
					}
					P_HitFloor (mo);

					// [BC] Potentially reward the player who shot this missile with an accuracy/precision medal.
					if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
					{
						if ( mo->target->player->bStruckPlayer )
							PLAYER_StruckPlayer( mo->target->player );
						else
							mo->target->player->ulConsecutiveHits = 0;
					}

					P_ExplodeMissile (mo, NULL, NULL);
					return;
				}
			}
			else if (mo->BounceFlags & BOUNCE_MBF && mo->velz) // check for MBF-like bounce on non-missiles
			{
				mo->FloorBounceMissile(mo->floorsector->floorplane);
			}

			// [BC] Apply spring pads.
			if ( mo->floorsector->GetFlags(sector_t::floor) & PLANEF_SPRINGPAD )
			{
				mo->velz = -mo->velz;
				mo->z = mo->floorz;
				return;
			}

			if (mo->flags3 & MF3_ISMONSTER)		// Blasted mobj falling
			{
				if (mo->velz < -(23*FRACUNIT))
				{
					P_MonsterFallingDamage (mo);
				}
			}
			mo->z = mo->floorz;
			if (mo->velz < 0)
			{
				const fixed_t minvel = -8*FRACUNIT;	// landing speed from a jump with normal gravity

				// Spawn splashes, etc.
				P_HitFloor (mo);
				if (mo->DamageType == NAME_Ice && mo->velz < minvel)
				{
					mo->tics = 1;
					mo->velx = 0;
					mo->vely = 0;
					mo->velz = 0;
					return;
				}
				// Let the actor do something special for hitting the floor
				mo->HitFloor ();
				if (mo->player)
				{
					// [BB] ZDoom changed the jumping here revision 2238.
					// The old behavior is necessary for existing jumpmaze wads.
					if ( ( zacompatflags & ZACOMPATF_SKULLTAG_JUMPING ) || mo->player->jumpTics < 0 || mo->velz < minvel)
					{ // delay any jumping for a short while
						mo->player->jumpTics = 7;
					}
					if (mo->velz < minvel && !(mo->flags & MF_NOGRAVITY))
					{
						// Squat down.
						// Decrease viewheight for a moment after hitting the ground (hard),
						// and utter appropriate sound.
						PlayerLandedOnThing (mo, NULL);
					}
				}
				mo->velz = 0;
			}
			if (mo->flags & MF_SKULLFLY)
			{ // The skull slammed into something
				mo->velz = -mo->velz;
			}
			mo->Crash();
		}
	}

	if (mo->flags2 & MF2_FLOORCLIP)
	{
		mo->AdjustFloorClip ();
	}
	// [WS] For clients, check to see if we are allowed to clip our actor's movement.
	if (mo->z + mo->height > mo->ceilingz && CLIENT_CanClipMovement(mo))
	{ // hit the ceiling
		if (/*(!mo->player || !(mo->player->cheats & CF_PREDICTING)) &&*/
			mo->Sector->SecActTarget != NULL &&
			mo->Sector->ceilingplane.ZatPoint (mo->x, mo->y) == mo->ceilingz)
		{ // [RH] Let the sector do something to the actor
			mo->Sector->SecActTarget->TriggerAction (mo, SECSPAC_HitCeiling);
		}
		P_CheckFor3DCeilingHit(mo);
		// [RH] Need to recheck this because the sector action might have
		// teleported the actor so it is no longer above the ceiling.
		if (mo->z + mo->height > mo->ceilingz)
		{
			// [BC] We need to do the sky check first, otherwise bouncy things
			// can potentially bounce off the sky (such as grenades).
			// [BB] But only do this for bouncy things!
			if (( mo->flags & MF_MISSILE ) && ( mo->ceilingpic == skyflatnum ) && (( mo->flags3 & MF3_SKYEXPLODE ) == false ) && (mo->BounceFlags & BOUNCE_Ceilings))
			{
				// Player didn't strike another player with this missile.
				if ( mo->target && mo->target->player )
					mo->target->player->ulConsecutiveHits = 0;

				// [Dusk] Tell the clients that the mobj was deleted
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing( mo );

				mo->Destroy( );
				return;
			}

			mo->z = mo->ceilingz - mo->height;
			if (mo->BounceFlags & BOUNCE_Ceilings)
			{	// ceiling bounce
				mo->FloorBounceMissile (mo->ceilingsector->ceilingplane);
				/*if (!(mo->flags6 & MF6_CANJUMP))*/ return;
			}
			if (mo->flags & MF_SKULLFLY)
			{	// the skull slammed into something
				mo->velz = -mo->velz;
			}
			if (mo->velz > 0)
				mo->velz = 0;
			if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
			{
				if (mo->flags3 & MF3_CEILINGHUGGER)
				{
					return;
				}
				if (mo->ceilingpic == skyflatnum &&  !(mo->flags3 & MF3_SKYEXPLODE))
				{
					// [Dusk] Tell the clients that the mobj was deleted
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
						SERVERCOMMANDS_DestroyThing( mo );

					mo->Destroy ();
					return;
				}

				// [BC] Potentially reward the player who shot this missile with an accuracy/precision medal.
				if ((( mo->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && mo->target && mo->target->player )
				{
					if ( mo->target->player->bStruckPlayer )
						PLAYER_StruckPlayer( mo->target->player );
					else
						mo->target->player->ulConsecutiveHits = 0;
				}

				P_ExplodeMissile (mo, NULL, NULL);
				return;
			}
		}
	}
	P_CheckFakeFloorTriggers (mo, oldz);

	// [TIHan/BB] If it's a missile that is bounceable and it bounced, send info to the client
	if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( mo->ulNetworkFlags & NETFL_BOUNCED_OFF_ACTOR ) )
	{
		SERVERCOMMANDS_MoveThingExact( mo, CM_X|CM_Y|CM_Z|CM_VELX|CM_VELY|CM_VELZ|CM_ANGLE );
		// [BB] Remove the mark, the syncing is done now.
		mo->ulNetworkFlags &= ~NETFL_BOUNCED_OFF_ACTOR;
	}
}

void P_CheckFakeFloorTriggers (AActor *mo, fixed_t oldz, bool oldz_has_viewheight)
{
/*
	if (mo->player && (mo->player->cheats & CF_PREDICTING))
	{
		return;
	}
*/
	sector_t *sec = mo->Sector;
	assert (sec != NULL);
	if (sec == NULL)
	{
		return;
	}
	if (sec->heightsec != NULL && sec->SecActTarget != NULL)
	{
		sector_t *hs = sec->heightsec;
		fixed_t waterz = hs->floorplane.ZatPoint (mo->x, mo->y);
		fixed_t newz;
		fixed_t viewheight;

		if (mo->player != NULL)
		{
			viewheight = mo->player->viewheight;
		}
		else
		{
			viewheight = mo->height / 2;
		}

		if (oldz > waterz && mo->z <= waterz)
		{ // Feet hit fake floor
			sec->SecActTarget->TriggerAction (mo, SECSPAC_HitFakeFloor);
		}

		newz = mo->z + viewheight;
		if (!oldz_has_viewheight)
		{
			oldz += viewheight;
		}

		if (oldz <= waterz && newz > waterz)
		{ // View went above fake floor
			sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesSurface);
		}
		else if (oldz > waterz && newz <= waterz)
		{ // View went below fake floor
			sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesDive);
		}

		if (!(hs->MoreFlags & SECF_FAKEFLOORONLY))
		{
			waterz = hs->ceilingplane.ZatPoint (mo->x, mo->y);
			if (oldz <= waterz && newz > waterz)
			{ // View went above fake ceiling
				sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesAboveC);
			}
			else if (oldz > waterz && newz <= waterz)
			{ // View went below fake ceiling
				sec->SecActTarget->TriggerAction (mo, SECSPAC_EyesBelowC);
			}
		}
	}
}

//===========================================================================
//
// PlayerLandedOnThing
//
//===========================================================================

static void PlayerLandedOnThing (AActor *mo, AActor *onmobj)
{
	bool grunted;

	if (!mo->player)
		return;

	// [BB] Don't do this while predicting.
	if ( CLIENT_PREDICT_IsPredicting( ))
		return;

	if (mo->player->mo == mo)
	{
		mo->player->deltaviewheight = mo->velz >> 3;
	}

/* [BB] I don't know why ZDoom needs to adjust deltaviewheight while predicting.
        Skulltag may not do this, otherwise the view skins into the floor when
        jumpding down a ledge with high ping.
	if (mo->player->cheats & CF_PREDICTING)
		return;
*/
	P_FallingDamage (mo);

	// [RH] only make noise if alive
	// [WS/BB] As client only play the sound for the consoleplayer.
	if (!mo->player->morphTics && mo->health > 0 && NETWORK_IsConsolePlayerOrNotInClientMode( mo->player ))
	{
		grunted = false;
		// Why should this number vary by gravity?
		// [BB] For unassigned voodoo dolls, mo->player->mo is NULL.
		if (mo->player->mo && mo->health > 0 && mo->velz < -mo->player->mo->GruntSpeed)
		{
			S_Sound (mo, CHAN_VOICE, "*grunt", 1, ATTN_NORM);
			grunted = true;

			// [BC] Tell players that this player struck the ground (hard!)
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SoundActor( mo, CHAN_VOICE, "*grunt", 1, ATTN_NORM, ULONG( mo->player - players ), SVCF_SKIPTHISCLIENT );
		}
		if (onmobj != NULL || !Terrains[P_GetThingFloorType (mo)].IsLiquid)
		{
			if (!grunted || !S_AreSoundsEquivalent (mo, "*grunt", "*land"))
			{
				S_Sound (mo, CHAN_AUTO, "*land", 1, ATTN_NORM);

				// [BC] Tell players that this player struck the ground (hard!)
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SoundActor( mo, CHAN_AUTO, "*land", 1, ATTN_NORM, ULONG( mo->player - players ), SVCF_SKIPTHISCLIENT );
			}
		}
	}
//	mo->player->centering = true;
}



//
// P_NightmareRespawn
//
void P_NightmareRespawn (AActor *mobj)
{
	fixed_t x, y, z;
	AActor *mo;
	AActor *info = mobj->GetDefault();

	mobj->skillrespawncount++;

	// spawn the new monster (assume the spawn will be good)
	if (info->flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else if (info->flags2 & MF2_SPAWNFLOAT)
		z = FLOATRANDZ;
	else if (info->flags2 & MF2_FLOATBOB)
		z = mobj->SpawnPoint[2];
	else
		z = ONFLOORZ;

	// spawn it
	x = mobj->SpawnPoint[0];
	y = mobj->SpawnPoint[1];
	mo = AActor::StaticSpawn(RUNTIME_TYPE(mobj), x, y, z, NO_REPLACE, true);

	if (z == ONFLOORZ)
	{
		mo->z += mobj->SpawnPoint[2];
		if (mo->z < mo->floorz)
		{ // Do not respawn monsters in the floor, even if that's where they
		  // started. The initial P_ZMovement() call would have put them on
		  // the floor right away, but we need them on the floor now so we
		  // can use P_CheckPosition() properly.
			mo->z = mo->floorz;
		}
		if (mo->z + mo->height > mo->ceilingz)
		{
			mo->z = mo->ceilingz - mo->height;
		}
	}
	else if (z == ONCEILINGZ)
	{
		mo->z -= mobj->SpawnPoint[2];
	}

	// If there are 3D floors, we need to find floor/ceiling again.
	P_FindFloorCeiling(mo, FFCF_SAMESECTOR | FFCF_ONLY3DFLOORS | FFCF_3DRESTRICT);

	if (z == ONFLOORZ)
	{
		if (mo->z < mo->floorz)
		{ // Do not respawn monsters in the floor, even if that's where they
		  // started. The initial P_ZMovement() call would have put them on
		  // the floor right away, but we need them on the floor now so we
		  // can use P_CheckPosition() properly.
			mo->z = mo->floorz;
		}
		if (mo->z + mo->height > mo->ceilingz)
		{ // Do the same for the ceiling.
			mo->z = mo->ceilingz - mo->height;
		}
	}

	// something is occupying its position?
	if (!P_CheckPosition(mo, mo->x, mo->y, true))
	{
		//[GrafZahl] MF_COUNTKILL still needs to be checked here.
		mo->ClearCounters();
		mo->Destroy ();
		return;		// no respawn
	}

	z = mo->z;

	// inherit attributes from deceased one
	mo->SpawnPoint[0] = mobj->SpawnPoint[0];
	mo->SpawnPoint[1] = mobj->SpawnPoint[1];
	mo->SpawnPoint[2] = mobj->SpawnPoint[2];
	mo->SpawnAngle = mobj->SpawnAngle;
	mo->SpawnFlags = mobj->SpawnFlags & ~MTF_DORMANT;	// It wasn't dormant when it died, so it's not dormant now, either.
	mo->angle = ANG45 * (mobj->SpawnAngle/45);

	mo->HandleSpawnFlags ();
	mo->reactiontime = 18;
	mo->CopyFriendliness (mobj, false);
	mo->Translation = mobj->Translation;

	mo->skillrespawncount = mobj->skillrespawncount;

	mo->PrevZ = z;		// Do not interpolate Z position if we changed it since spawning.

	// [BB] The new actor has to inherit the STFL_LEVELSPAWNED flag from the old one.
	// Otherwise level spawned actors respawned by P_NightmareRespawn won't be restored
	// during a call of GAME_ResetMap.
	if ( mobj->ulSTFlags & STFL_LEVELSPAWNED )
		mo->ulSTFlags |= STFL_LEVELSPAWNED;

	// [BC] If we're the server, tell clients to spawn the thing.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SpawnThing( mo );

		if ( mo->angle != 0 )
			SERVERCOMMANDS_SetThingAngle( mo );

		// [EP] Since AActor::HandleSpawnFlags has been called, we need to inform the clients about its effects.
		SERVERCOMMANDS_UpdateThingFlagsNotAtDefaults( mo, MAXPLAYERS, 0 );
		if ( mo->alpha != mo->GetDefault()->alpha )
			SERVERCOMMANDS_SetThingProperty( mo, APROP_Alpha );
		if ( mo->RenderStyle.AsDWORD != mo->GetDefault()->RenderStyle.AsDWORD )
			SERVERCOMMANDS_SetThingProperty( mo, APROP_RenderStyle );
	}

	// spawn a teleport fog at old spot because of removal of the body?
	mo = Spawn ("TeleportFog", mobj->x, mobj->y, mobj->z, ALLOW_REPLACE);
	if (mo != NULL)
	{
		mo->z += TELEFOGHEIGHT;

		// [BC] If we're the server, tell clients to spawn the thing.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( mo );
	}

	// spawn a teleport fog at the new spot
	mo = Spawn ("TeleportFog", x, y, z, ALLOW_REPLACE);
	if (mo != NULL)
	{
		mo->z += TELEFOGHEIGHT;

		// [BC] If we're the server, tell clients to spawn the thing.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SpawnThing( mo );
	}

	// [BC] Tell clients to destroy the old monster.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_DestroyThing( mobj );

	// remove the old monster
	mobj->Destroy ();
}


AActor *AActor::TIDHash[128];

//
// P_ClearTidHashes
//
// Clears the tid hashtable.
//

void AActor::ClearTIDHashes ()
{
	memset(TIDHash, 0, sizeof(TIDHash));
}

//
// P_AddMobjToHash
//
// Inserts an mobj into the correct chain based on its tid.
// If its tid is 0, this function does nothing.
//
void AActor::AddToHash ()
{
	if (tid == 0)
	{
		iprev = NULL;
		inext = NULL;
		return;
	}
	else
	{
		int hash = TIDHASH (tid);

		inext = TIDHash[hash];
		iprev = &TIDHash[hash];
		TIDHash[hash] = this;
		if (inext)
		{
			inext->iprev = &inext;
		}
	}
}

//
// P_RemoveMobjFromHash
//
// Removes an mobj from its hash chain.
//
void AActor::RemoveFromHash ()
{
	if (tid != 0 && iprev)
	{
		*iprev = inext;
		if (inext)
		{
			inext->iprev = iprev;
		}
		iprev = NULL;
		inext = NULL;
	}
	tid = 0;
}

//==========================================================================
//
// P_IsTIDUsed
//
// Returns true if there is at least one actor with the specified TID
// (dead or alive).
//
//==========================================================================

bool P_IsTIDUsed(int tid)
{
	AActor *probe = AActor::TIDHash[tid & 127];
	while (probe != NULL)
	{
		if (probe->tid == tid)
		{
			return true;
		}
		probe = probe->inext;
	}
	return false;
}

//==========================================================================
//
// P_FindUniqueTID
//
// Returns an unused TID. If start_tid is 0, then a random TID will be
// chosen. Otherwise, it will perform a linear search starting from
// start_tid. If limit is non-0, then it will not check more than <limit>
// number of TIDs. Returns 0 if no suitable TID was found.
//
//==========================================================================

int P_FindUniqueTID(int start_tid, int limit)
{
	int tid;

	if (start_tid != 0)
	{ // Do a linear search.
		if (start_tid > INT_MAX-limit+1)
		{ // If 'limit+start_tid-1' overflows, clamp 'limit' to INT_MAX
			limit = INT_MAX;
		}
		else
		{
			limit += start_tid-1;
		}
		for (tid = start_tid; tid <= limit; ++tid)
		{
			if (tid != 0 && !P_IsTIDUsed(tid))
			{
				return tid;
			}
		}
		// Nothing free found.
		return 0;
	}
	// Do a random search. To try and be a bit more performant, this
	// actually does several linear searches. In the case of *very*
	// dense TID usage, this could potentially perform worse than doing
	// a complete linear scan starting at 1. However, you would need
	// to use an absolutely ridiculous number of actors before this
	// becomes a real concern.
	if (limit == 0)
	{
		limit = INT_MAX;
	}
	for (int i = 0; i < limit; i += 5)
	{
		// Use a positive starting TID.
		tid = pr_uniquetid.GenRand32() & INT_MAX;
		tid = P_FindUniqueTID(tid == 0 ? 1 : tid, 5);
		if (tid != 0)
		{
			return tid;
		}
	}
	// Nothing free found.
	return 0;
}

CCMD(utid)
{
	Printf("%d\n",
		P_FindUniqueTID(argv.argc() > 1 ? atoi(argv[1]) : 0,
		(argv.argc() > 2 && atoi(argv[2]) >= 0) ? atoi(argv[2]) : 0));
}

//==========================================================================
//
// AActor :: GetMissileDamage
//
// If the actor's damage amount is an expression, evaluate it and return
// the result. Otherwise, return ((random() & mask) + add) * damage.
//
//==========================================================================

int AActor::GetMissileDamage (int mask, int add)
{
	if ((Damage & 0xC0000000) == 0x40000000)
	{
		return EvalExpressionI (Damage & 0x3FFFFFFF, this);
	}
	if (Damage == 0)
	{
		return 0;
	}
	else if (mask == 0)
	{
		return add * Damage;
	}
	else
	{
		return ((pr_missiledamage() & mask) + add) * Damage;
	}
}

void AActor::Howl ()
{
	int howl = GetClass()->Meta.GetMetaInt(AMETA_HowlSound);
	if (!S_IsActorPlayingSomething(this, CHAN_BODY, howl))
	{
		S_Sound (this, CHAN_BODY, howl, 1, ATTN_NORM);
	}
}

void AActor::HitFloor ()
{
}

bool AActor::Slam (AActor *thing)
{
	// [BB] This is server side.
	if ( NETWORK_InClientMode() )
	{
		return false;
	}

	flags &= ~MF_SKULLFLY;
	velx = vely = velz = 0;

	// [BB] If we are the server, tell clients about MF_SKULLFLY and the velocity change.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		SERVERCOMMANDS_SetThingFlags( this, FLAGSET_FLAGS );
		SERVERCOMMANDS_MoveThing( this, CM_VELX|CM_VELY|CM_VELZ );
	}

	if (health > 0)
	{
		if (!(flags2 & MF2_DORMANT))
		{
			int dam = GetMissileDamage (7, 1);
			int newdam = P_DamageMobj (thing, this, this, dam, NAME_Melee);
			P_TraceBleed (newdam > 0 ? newdam : dam, thing, this);
			// The charging monster may have died by the target's actions here.
			if (health > 0)
			{
				// [BC] If we are the server, tell clients about the state change.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_SetThingState( this, SeeState != NULL ? STATE_SEE : STATE_IDLE );

				if (SeeState != NULL) SetState (SeeState);
				else SetIdle();
			}
		}
		else
		{
			// [BB] If we are the server, tell clients about the state change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingState( this, STATE_IDLE );

			SetIdle();
			tics = -1;

			// [BB] If we're the server, tell clients to update this thing's tics.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetThingTics( this );
		}
	}
	return false;			// stop moving
}

bool AActor::SpecialBlastHandling (AActor *source, fixed_t strength)
{
	return true;
}

int AActor::SpecialMissileHit (AActor *victim)
{
	return -1;
}

bool AActor::AdjustReflectionAngle (AActor *thing, angle_t &angle)
{
	if (flags2 & MF2_DONTREFLECT) return true;

	// Change angle for reflection
	if (thing->flags4&MF4_SHIELDREFLECT)
	{
		// Shield reflection (from the Centaur
		if (absangle(angle - thing->angle)>>24 > 45)
			return true;	// Let missile explode

		if (thing->IsKindOf (RUNTIME_CLASS(AHolySpirit)))	// shouldn't this be handled by another flag???
			return true;

		if (pr_reflect () < 128)
			angle += ANGLE_45;
		else
			angle -= ANGLE_45;

	}
	else if (thing->flags4&MF4_DEFLECT)
	{
		// deflect (like the Heresiarch)
		if(pr_reflect() < 128) 
			angle += ANG45;
		else 
			angle -= ANG45;
	}
	else 
		angle += ANGLE_1 * ((pr_reflect()%16)-8);
	return false;
}

void AActor::PlayActiveSound ()
{
	if (ActiveSound && !S_IsActorPlayingSomething (this, CHAN_VOICE, -1))
	{
		S_Sound (this, CHAN_VOICE, ActiveSound, 1,
			(flags3 & MF3_FULLVOLACTIVE) ? ATTN_NONE : ATTN_IDLE);
	}
}

bool AActor::IsOkayToAttack (AActor *link)
{
	// [BB] Don't do this in client mode for MinotaurFriend.
	// That's the only actor that is adapted such that the server handles this check.
	if ( NETWORK_InClientMode() && this->IsKindOf( PClass::FindClass("MinotaurFriend") ) )
		return ( false );

	if (!(player							// Original AActor::IsOkayToAttack was only for players
	//	|| (flags  & MF_FRIENDLY)			// Maybe let friendly monsters use the function as well?
		|| (flags5 & MF5_SUMMONEDMONSTER)	// AMinotaurFriend has its own version, generalized to other summoned monsters
		|| (flags2 & MF2_SEEKERMISSILE)))	// AHolySpirit and AMageStaffFX2 as well, generalized to other seeker missiles
	{	// Normal monsters and other actors always return false.
		return false;
	}
	// Standard things to eliminate: an actor shouldn't attack itself,
	// or a non-shootable, dormant, non-player-and-non-monster actor.
	if (link == this)									return false;
	if (!(link->player||(link->flags3 & MF3_ISMONSTER)))return false;
	if (!(link->flags & MF_SHOOTABLE))					return false;
	if (link->flags2 & MF2_DORMANT)						return false;

	// An actor shouldn't attack friendly actors. The reference depends
	// on the type of actor: for a player's actor, itself; for a projectile,
	// its target; and for a summoned minion, its tracer.
	AActor * Friend = NULL;
	if (player)											Friend = this;
	else if (flags5 & MF5_SUMMONEDMONSTER)				Friend = tracer;
	else if (flags2 & MF2_SEEKERMISSILE)				Friend = target;
	else if ((flags & MF_FRIENDLY) && FriendPlayer)		Friend = players[FriendPlayer-1].mo;

	// Friend checks
	if (link == Friend)									return false;
	if (Friend == NULL)									return false;
	if (Friend->IsFriend(link))							return false;
	if ((link->flags5 & MF5_SUMMONEDMONSTER)			// No attack against minions on the same side
		&& (link->tracer == Friend))					return false;
	// [BB] Changed "multiplayer" check
	// [RK] Check for CTF and team games
	if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) && !deathmatch && !teamgame						// No attack against fellow players in coop
		&& link->player && Friend->player)				return false;
	if (((flags & link->flags) & MF_FRIENDLY)			// No friendly infighting amongst minions
		&& IsFriend(link))								return false;

	// Now that all the actor checks are made, the line of sight can be checked
	if (P_CheckSight (this, link))
	{
		// AMageStaffFX2::IsOkayToAttack had an extra check here, generalized with a flag,
		// to only allow the check to succeed if the enemy was in a ~84� FOV of the player
		if (flags3 & MF3_SCREENSEEKER)
		{
			angle_t angle = R_PointToAngle2(Friend->x, 
				Friend->y, link->x, link->y) - Friend->angle;
			angle >>= 24;
			if (angle>226 || angle<30)
			{
				return true;
			}
		}
		// Other actors are not concerned by this check
		else return true;
	}
	// The sight check was failed, or the angle wasn't right for a screenseeker
	return false;
}

void AActor::SetShade (DWORD rgb)
{
	PalEntry *entry = (PalEntry *)&rgb;
	fillcolor = rgb | (ColorMatcher.Pick (entry->r, entry->g, entry->b) << 24);
}

void AActor::SetShade (int r, int g, int b)
{
	fillcolor = MAKEARGB(ColorMatcher.Pick (r, g, b), r, g, b);
}

void AActor::SetPitch(int p, bool interpolate)
{
	if (p != pitch)
	{
		pitch = p;
		if (player != NULL && interpolate)
		{
			player->cheats |= CF_INTERPVIEW;
		}
	}
}

void AActor::SetAngle(angle_t ang, bool interpolate)
{
	if (ang != angle)
	{
		angle = ang;
		if (player != NULL && interpolate)
		{
			player->cheats |= CF_INTERPVIEW;
		}
	}
}

//
// P_MobjThinker
//
void AActor::Tick ()
{
	// [BB] Start to measure how much outbound net traffic this call of AActor::Tick() needs.
	NETWORK_StartTrafficMeasurement ( );

	// [RH] Data for Heretic/Hexen scrolling sectors
	static const BYTE HexenScrollDirs[8] = { 64, 0, 192, 128, 96, 32, 224, 160 };
	static const BYTE HexenSpeedMuls[3] = { 5, 10, 25 };
	static const SBYTE HexenScrollies[24][2] =
	{
		{  0,  1 }, {  0,  2 }, {  0,  4 },
		{ -1,  0 }, { -2,  0 }, { -4,  0 },
		{  0, -1 }, {  0, -2 }, {  0, -4 },
		{  1,  0 }, {  2,  0 }, {  4,  0 },
		{  1,  1 }, {  2,  2 }, {  4,  4 },
		{ -1,  1 }, { -2,  2 }, { -4,  4 },
		{ -1, -1 }, { -2, -2 }, { -4, -4 },
		{  1, -1 }, {  2, -2 }, {  4, -4 }
	};

	static const BYTE HereticScrollDirs[4] = { 6, 9, 1, 4 };
	static const BYTE HereticSpeedMuls[5] = { 5, 10, 25, 30, 35 };


	AActor *onmo;
	// [BB] Zandronum's bot handling doesn't need this 'i'.
	//int i;

	//assert (state != NULL);
	if (state == NULL)
	{
		Printf("Actor of type %s at (%f,%f) left without a state\n", GetClass()->TypeName.GetChars(),
			x/65536., y/65536.);
		Destroy();
		return;
	}

	// This is necessary to properly interpolate movement outside this function
	// like from an ActorMover
	PrevX = x;
	PrevY = y;
	PrevZ = z;
	PrevAngle = angle;

	// [BC] There are times when we don't want to tick this actor if it's a player.
	// [BB] Voodoo dolls are an exemption.
	if ( player && player->mo == this )
	{
		// In server mode, only allow the ticking of a player if he's a client currently
		// having his movement commands executed.
		if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
			( SERVER_GetCurrentClient( ) != ( player - players )) &&
			( player->bIsBot == false ))
		{
			return;
		}
	}

	if (flags5 & MF5_NOINTERACTION)
	{
		// only do the minimally necessary things here to save time:
		// Check the time freezer
		// apply velocity
		// ensure that the actor is not linked into the blockmap

		if (!(flags5 & MF5_NOTIMEFREEZE))
		{
			//Added by MC: Freeze mode.
			if (/*bglobal.freeze ||*/ level.flags2 & LEVEL2_FROZEN)
			{
				// Boss cubes shouldn't be accelerated by timefreeze
				if (flags6 & MF6_BOSSCUBE)
				{
					special2++;
				}
				return;
			}
		}

		UnlinkFromWorld ();
		flags |= MF_NOBLOCKMAP;
		x += velx;
		y += vely;
		z += velz;
		LinkToWorld ();
	}
	else
	{
		AInventory * item = Inventory;

		// Handle powerup effects here so that the order is controlled
		// by the order in the inventory, not the order in the thinker table
		while (item != NULL && item->Owner == this)
		{
			item->DoEffect();
			item = item->Inventory;
		}

		if (flags & MF_UNMORPHED)
		{
			return;
		}

		if (!(flags5 & MF5_NOTIMEFREEZE))
		{
			// Boss cubes shouldn't be accelerated by timefreeze
			if (flags6 & MF6_BOSSCUBE)
			{
				special2++;
			}
			// Apply freeze mode.
			// [WS] Added Skulltag specific code for checking to see if the player is a spectator, if the player is then don't apply freeze mode.
			if ((level.flags2 & LEVEL2_FROZEN) && (player == NULL || ( player->timefreezer == 0 && !player->bSpectating )))
			{
				return;
			}
		}


		if (effects & FX_ROCKET) 
		{
			if (++smokecounter == 4)
			{
				// add some smoke behind the rocket 
				smokecounter = 0;
				AActor *th = Spawn("RocketSmokeTrail", x-velx, y-vely, z-velz, ALLOW_REPLACE);
				if (th)
				{
					th->tics -= pr_rockettrail()&3;
					if (th->tics < 1) th->tics = 1;
					if (!(cl_rockettrails & 2)) th->renderflags |= RF_INVISIBLE;
				}
			}
		}
		else if (effects & FX_GRENADE) 
		{
			if (++smokecounter == 8)
			{
				smokecounter = 0;
				angle_t moveangle = R_PointToAngle2(0,0,velx,vely);
				AActor * th = Spawn("GrenadeSmokeTrail", 
					x - FixedMul (finecosine[(moveangle)>>ANGLETOFINESHIFT], radius*2) + (pr_rockettrail()<<10),
					y - FixedMul (finesine[(moveangle)>>ANGLETOFINESHIFT], radius*2) + (pr_rockettrail()<<10),
					z - (height>>3) * (velz>>16) + (2*height)/3, ALLOW_REPLACE);
				if (th)
				{
					th->tics -= pr_rockettrail()&3;
					if (th->tics < 1) th->tics = 1;
					if (!(cl_rockettrails & 2)) th->renderflags |= RF_INVISIBLE;
				}
			}
		}

		fixed_t oldz = z;

		// [RH] Give the pain elemental vertical friction
		// This used to be in APainElemental::Tick but in order to use
		// A_PainAttack with other monsters it has to be here
		if (flags4 & MF4_VFRICTION)
		{
			if (health >0)
			{
				if (abs (velz) < FRACUNIT/4)
				{
					velz = 0;
					flags4 &= ~MF4_VFRICTION;
				}
				else
				{
					velz = FixedMul (velz, 0xe800);
				}
			}
		}

		// [BC] Flicker this objects visibility... ala starman in SMB.
		if ( effects & FX_VISIBILITYFLICKER )
		{
			switch ( M_Random( ) % 3 )
			{
			case 0:		alpha = TRANSLUC25;	break;
			case 1:		alpha = TRANSLUC50;	break;
			case 2:		alpha = TRANSLUC75;	break;
			}
		}
		// [RH] Pulse in and out of visibility
		else if (effects & FX_VISIBILITYPULSE)
		{
			if (visdir > 0)
			{
				alpha += 0x800;
				if (alpha >= OPAQUE)
				{
					alpha = OPAQUE;
					visdir = -1;
				}
			}
			else
			{
				alpha -= 0x800;
				if (alpha <= TRANSLUC25)
				{
					alpha = TRANSLUC25;
					visdir = 1;
				}
			}
		}
		else if (flags & MF_STEALTH)
		{
		if ( zacompatflags & ZACOMPATF_DISABLESTEALTHMONSTERS )
		{
			alpha = OPAQUE;
			visdir = 0;
		}
		else
		{
			// [RH] Fade a stealth monster in and out of visibility
			RenderStyle.Flags &= ~STYLEF_Alpha1;
			if (visdir > 0)
			{
				alpha += 2*FRACUNIT/TICRATE;
				if (alpha > OPAQUE)
				{
					alpha = OPAQUE;
					visdir = 0;
				}
			}
			else if (visdir < 0)
			{
				alpha -= 3*FRACUNIT/TICRATE/2;
				if (alpha < 0)
				{
					alpha = 0;
					visdir = 0;
				}
			}
		}
		}

		// [RH] Consider carrying sectors here
		fixed_t cummx = 0, cummy = 0;
		if ((level.Scrolls != NULL || player != NULL) && !(flags & MF_NOCLIP) && !(flags & MF_NOSECTOR))
		{
			fixed_t height, waterheight;	// killough 4/4/98: add waterheight
			const msecnode_t *node;
			int countx, county;

			// killough 3/7/98: Carry things on floor
			// killough 3/20/98: use new sector list which reflects true members
			// killough 3/27/98: fix carrier bug
			// killough 4/4/98: Underwater, carry things even w/o gravity

			// Move objects only if on floor or underwater,
			// non-floating, and clipped.

			countx = county = 0;

			for (node = touching_sectorlist; node; node = node->m_tnext)
			{
				const sector_t *sec = node->m_sector;
				fixed_t scrollx, scrolly;

				if (level.Scrolls != NULL)
				{
					const FSectorScrollValues *scroll = &level.Scrolls[sec - sectors];
					scrollx = scroll->ScrollX;
					scrolly = scroll->ScrollY;
				}
				else
				{
					scrollx = scrolly = 0;
				}

				if (player != NULL)
				{
					int scrolltype = sec->special & 0xff;

					if (scrolltype >= Scroll_North_Slow &&
						scrolltype <= Scroll_SouthWest_Fast)
					{ // Hexen scroll special
						scrolltype -= Scroll_North_Slow;
						if (i_compatflags&COMPATF_RAVENSCROLL)
						{
							angle_t fineangle = HexenScrollDirs[scrolltype / 3] * 32;
							fixed_t carryspeed = DivScale32 (HexenSpeedMuls[scrolltype % 3], 32*CARRYFACTOR);
							scrollx += FixedMul (carryspeed, finecosine[fineangle]);
							scrolly += FixedMul (carryspeed, finesine[fineangle]);
						}
						else
						{
							// Use speeds that actually match the scrolling textures!
							scrollx -= HexenScrollies[scrolltype][0] << (FRACBITS-1);
							scrolly += HexenScrollies[scrolltype][1] << (FRACBITS-1);
						}
					}
					else if (scrolltype >= Carry_East5 &&
							 scrolltype <= Carry_West35)
					{ // Heretic scroll special
						scrolltype -= Carry_East5;
						BYTE dir = HereticScrollDirs[scrolltype / 5];
						fixed_t carryspeed = DivScale32 (HereticSpeedMuls[scrolltype % 5], 32*CARRYFACTOR);
						if (scrolltype<=Carry_East35 && !(i_compatflags&COMPATF_RAVENSCROLL)) 
						{
							// Use speeds that actually match the scrolling textures!
							carryspeed = (1 << ((scrolltype%5) + FRACBITS-1));
						}
						scrollx += carryspeed * ((dir & 3) - 1);
						scrolly += carryspeed * (((dir & 12) >> 2) - 1);
					}
					else if (scrolltype == dScroll_EastLavaDamage)
					{ // Special Heretic scroll special
						if (i_compatflags&COMPATF_RAVENSCROLL)
						{
							scrollx += DivScale32 (28, 32*CARRYFACTOR);
						}
						else
						{
							// Use a speed that actually matches the scrolling texture!
							scrollx += DivScale32 (12, 32*CARRYFACTOR);
						}
					}
					else if (scrolltype == Scroll_StrifeCurrent)
					{ // Strife scroll special
						int anglespeed = sec->tag - 100;
						fixed_t carryspeed = DivScale32 (anglespeed % 10, 16*CARRYFACTOR);
						angle_t fineangle = (anglespeed / 10) << (32-3);
						fineangle >>= ANGLETOFINESHIFT;
						scrollx += FixedMul (carryspeed, finecosine[fineangle]);
						scrolly += FixedMul (carryspeed, finesine[fineangle]);
					}
				}

				if ((scrollx | scrolly) == 0)
				{
					continue;
				}
				sector_t *heightsec = sec->GetHeightSec();
				if (flags & MF_NOGRAVITY && heightsec == NULL)
				{
					continue;
				}
				height = sec->floorplane.ZatPoint (x, y);
				if (z > height)
				{
					if (heightsec == NULL)
					{
						continue;
					}

					waterheight = heightsec->floorplane.ZatPoint (x, y);
					if (waterheight > height && z >= waterheight)
					{
						continue;
					}
				}

				cummx += scrollx;
				cummy += scrolly;
				if (scrollx) countx++;
				if (scrolly) county++;
			}

			// Some levels designed with Boom in mind actually want things to accelerate
			// at neighboring scrolling sector boundaries. But it is only important for
			// non-player objects.
			if (player != NULL || !(i_compatflags & COMPATF_BOOMSCROLL))
			{
				if (countx > 1)
				{
					cummx /= countx;
				}
				if (county > 1)
				{
					cummy /= county;
				}
			}
		}

		// [RH] If standing on a steep slope, fall down it
		if ((flags & MF_SOLID) && !(flags & (MF_NOCLIP|MF_NOGRAVITY)) &&
			!(flags & MF_NOBLOCKMAP) &&
			velz <= 0 &&
			floorz == z)
		{
			secplane_t floorplane = floorsector->floorplane;

#ifdef _3DFLOORS
			// Check 3D floors as well
			floorplane = P_FindFloorPlane(floorsector, x, y, floorz);
#endif

			if (floorplane.c < STEEPSLOPE &&
				floorplane.ZatPoint (x, y) <= floorz)
			{
				const msecnode_t *node;
				bool dopush = true;

				if (floorplane.c > STEEPSLOPE*2/3)
				{
					for (node = touching_sectorlist; node; node = node->m_tnext)
					{
						const sector_t *sec = node->m_sector;
						if (sec->floorplane.c >= STEEPSLOPE)
						{
							if (floorplane.ZatPoint (x, y) >= z - MaxStepHeight)
							{
								dopush = false;
								break;
							}
						}
					}
				}
				if (dopush)
				{
					velx += floorplane.a;
					vely += floorplane.b;
				}
			}
		}

		// [RH] Missiles moving perfectly vertical need some X/Y movement, or they
		// won't hurt anything. Don't do this if damage is 0! That way, you can
		// still have missiles that go straight up and down through actors without
		// damaging anything.
		if ((flags & MF_MISSILE) && (velx|vely) == 0 && Damage != 0)
		{
			velx = 1;
		}

		// Handle X and Y velocities
		BlockingMobj = NULL;
		fixed_t oldfloorz = 0;
		if ( player && ( player->bSpectating == false ) && ( zacompatflags & ZACOMPATF_PLASMA_BUMP_BUG ))
			oldfloorz = P_OldXYMovement( this );
		else
			oldfloorz = P_XYMovement (this, cummx, cummy);
		if (ObjectFlags & OF_EuthanizeMe)
		{ // actor was destroyed
			return;
		}
		if ((velx | vely) == 0) // Actors at rest
		{
			if (flags2 & MF2_BLASTED)
			{ // Reset to not blasted when velocities are gone
				flags2 &= ~MF2_BLASTED;
			}
			if ((flags6 & MF6_TOUCHY) && !IsSentient())
			{ // Arm a mine which has come to rest
				flags6 |= MF6_ARMED;
			}

		}
		if (velz || BlockingMobj || z != floorz)
		{	// Handle Z velocity and gravity
			if (((flags2 & MF2_PASSMOBJ) || (flags & MF_SPECIAL)) && !(i_compatflags & COMPATF_NO_PASSMOBJ))
			{
				if (!(onmo = P_CheckOnmobj (this)))
				{
					P_ZMovement (this, oldfloorz);
					flags2 &= ~MF2_ONMOBJ;
				}
				else
				{
					if (player)
					{
						if (velz < (fixed_t)(level.gravity * Sector->gravity * -655.36f)
							&& !(flags&MF_NOGRAVITY))
						{
							PlayerLandedOnThing (this, onmo);
						}
					}
					if (onmo->z + onmo->height - z <= MaxStepHeight)
					{
						if (player && player->mo == this)
						{
							// [BC] Don't alter viewheight if we're just predicting.
							if ( CLIENT_PREDICT_IsPredicting( ) == false )
							{
								player->viewheight -= onmo->z + onmo->height - z;
								fixed_t deltaview = player->GetDeltaViewHeight();
								if (deltaview > player->deltaviewheight)
								{
									player->deltaviewheight = deltaview;
								}
							} 
						}
						z = onmo->z + onmo->height;
					}
					// Check for MF6_BUMPSPECIAL
					// By default, only players can activate things by bumping into them
					// We trigger specials as long as we are on top of it and not just when
					// we land on it. This could be considered as gravity making us continually
					// bump into it, but it also avoids having to worry about walking on to
					// something without dropping and not triggering anything.
					if ((onmo->flags6 & MF6_BUMPSPECIAL) && ((player != NULL)
						|| ((onmo->activationtype & THINGSPEC_MonsterTrigger) && (flags3 & MF3_ISMONSTER))
						|| ((onmo->activationtype & THINGSPEC_MissileTrigger) && (flags & MF_MISSILE))
						) && (level.maptime > onmo->lastbump)) // Leave the bumper enough time to go away
					{
						// [BB]
						//if (player == NULL || !(player->cheats & CF_PREDICTING))
						{
							if (P_ActivateThingSpecial(onmo, this))
								onmo->lastbump = level.maptime + TICRATE;
						}
					}
					if (velz != 0 && (BounceFlags & BOUNCE_Actors))
					{
						P_BounceActor(this, onmo, true);
					}
					else
					{
						flags2 |= MF2_ONMOBJ;
						velz = 0;
						Crash();
					}
				}
			}
			else
			{
				P_ZMovement (this, oldfloorz);
			}

			if (ObjectFlags & OF_EuthanizeMe)
				return;		// actor was destroyed
		}
		else if (z <= floorz)
		{
			Crash();
			if (ObjectFlags & OF_EuthanizeMe)
				return;		// actor was destroyed
		}

		UpdateWaterLevel (oldz);

/*
		// [RH] Don't advance if predicting a player
		if (player && (player->cheats & CF_PREDICTING))
		{
			return;
		}
*/

		// Check for poison damage, but only once per PoisonPeriod tics (or once per second if none).
		if (PoisonDurationReceived && (level.time % (PoisonPeriodReceived ? PoisonPeriodReceived : TICRATE) == 0))
		{
			P_DamageMobj(this, NULL, Poisoner, PoisonDamageReceived, PoisonDamageTypeReceived ? PoisonDamageTypeReceived : (FName)NAME_Poison, 0);

			--PoisonDurationReceived;

			// Must clear damage when duration is done, otherwise it
			// could be added to with ADDITIVEPOISONDAMAGE.
			if (!PoisonDurationReceived) PoisonDamageReceived = 0;
		}
	}
	// [BC] Don't tick states while predicting.
	if ( CLIENT_PREDICT_IsPredicting( ))
		return;

	// [BB] Spectators shall stay in their spawn state and don't execute any code pointers.
	if ( this->player && this->player->bSpectating )
		return;

	assert (state != NULL);
	if (state == NULL)
	{
		Destroy();
		return;
	}
	if ((flags7 & MF7_HANDLENODELAY) && !(flags2 & MF2_DORMANT))
	{
		flags7 &= ~MF7_HANDLENODELAY;
		if (state->GetNoDelay())
		{
			// For immediately spawned objects with the NoDelay flag set for their
			// Spawn state, explicitly set the current state so that it calls its
			// action and chains 0-tic states.
			int starttics = tics;
			if (!SetState(state))
				return;				// freed itself
			// If the initial state had a duration of 0 tics, let the next state run
			// normally. Otherwise, increment tics by 1 so that we don't double up ticks.
			else if (starttics > 0 && tics >= 0)
			{
				tics++;
			}
		}
	}
	// cycle through states, calling action functions at transitions
	if (tics != -1)
	{
		// [RH] Use tics <= 0 instead of == 0 so that spawnstates
		// of 0 tics work as expected.
		if (--tics <= 0)
		{
			// [BB] Active player bodies my not be deleted!
			if ( ( state->GetNextState() == NULL ) && player && ( player->mo == this ) ) {
				Printf ( PRINT_BOLD, "WARNING: The active body of a player does not have a next state, but the current state has a finite duration! Freezing player body in its current state.\n" );
				tics = -1;
				return;
			}

			if (!SetState(state->GetNextState()))
				return; 		// freed itself
		}
	}
	else
	{
		// [BC] The rest is server-side.
		if ( NETWORK_InClientMode() )
		{
			return;
		}

		int respawn_monsters = G_SkillProperty(SKILLP_Respawn);
		// check for nightmare respawn
		if (!(flags5 & MF5_ALWAYSRESPAWN))
		{
			if (!respawn_monsters || !(flags3 & MF3_ISMONSTER) || (flags2 & MF2_DORMANT) || (flags5 & MF5_NEVERRESPAWN))
				return;

			int limit = G_SkillProperty (SKILLP_RespawnLimit);
			if (limit > 0 && skillrespawncount >= limit)
				return;
		}

		movecount++;

		if (movecount < respawn_monsters)
			return;

		if (level.time & 31)
			return;

		if (pr_nightmarerespawn() > 4)
			return;

		P_NightmareRespawn (this);
	}

	// [BB] Stop the net traffic measurement and add the result to this actor's traffic.
	NETTRAFFIC_AddActorTraffic ( this, NETWORK_StopTrafficMeasurement ( ) );
}

//==========================================================================
//
// AActor :: CheckSectorTransition
//
// Fire off some sector triggers if the actor has changed sectors.
//
//==========================================================================

void AActor::CheckSectorTransition(sector_t *oldsec)
{
	if (oldsec != Sector)
	{
		if (oldsec->SecActTarget != NULL)
		{
			oldsec->SecActTarget->TriggerAction(this, SECSPAC_Exit);
		}
		if (Sector->SecActTarget != NULL)
		{
			int act = SECSPAC_Enter;
			if (z <= Sector->floorplane.ZatPoint(x, y))
			{
				act |= SECSPAC_HitFloor;
			}
			if (z + height >= Sector->ceilingplane.ZatPoint(x, y))
			{
				act |= SECSPAC_HitCeiling;
			}
			if (Sector->heightsec != NULL && z == Sector->heightsec->floorplane.ZatPoint(x, y))
			{
				act |= SECSPAC_HitFakeFloor;
			}
			Sector->SecActTarget->TriggerAction(this, act);
		}
		if (z == floorz)
		{
			P_CheckFor3DFloorHit(this);
		}
		if (z + height == ceilingz)
		{
			P_CheckFor3DCeilingHit(this);
		}

		// [BL] Trigger Domination check if player enters a new sector in Domination
		if (this->player)
		{
			DOMINATION_EnterSector(this->player);
		}
	}
}

//==========================================================================
//
// AActor::UpdateWaterLevel
//
// Returns true if actor should splash
//
//==========================================================================

bool AActor::UpdateWaterLevel (fixed_t oldz, bool dosplash)
{
	BYTE lastwaterlevel = waterlevel;
	fixed_t fh = FIXED_MIN;
	bool reset=false;

	waterlevel = 0;

	if (Sector == NULL)
	{
		return false;
	}

	if (Sector->MoreFlags & SECF_UNDERWATER)	// intentionally not SECF_UNDERWATERMASK
	{
		waterlevel = 3;
	}
	else
	{
		const sector_t *hsec = Sector->GetHeightSec();
		if (hsec != NULL)
		{
			fh = hsec->floorplane.ZatPoint (x, y);
			//if (hsec->MoreFlags & SECF_UNDERWATERMASK)	// also check Boom-style non-swimmable sectors
			{
				if (z < fh)
				{
					waterlevel = 1;
					if (z + height/2 < fh)
					{
						waterlevel = 2;
						if ((player && z + player->viewheight <= fh) ||
							(z + height <= fh))
						{
							waterlevel = 3;
						}
					}
				}
				else if (!(hsec->MoreFlags & SECF_FAKEFLOORONLY) && (z + height > hsec->ceilingplane.ZatPoint (x, y)))
				{
					waterlevel = 3;
				}
				else
				{
					waterlevel = 0;
				}
			}
			// even non-swimmable deep water must be checked here to do the splashes correctly
			// But the water level must be reset when this function returns
			if (!(hsec->MoreFlags&SECF_UNDERWATERMASK))
			{
				reset = true;
			}
		}
#ifdef _3DFLOORS
		else
		{
			// Check 3D floors as well!
			for(unsigned int i=0;i<Sector->e->XFloor.ffloors.Size();i++)
			{
				F3DFloor*  rover=Sector->e->XFloor.ffloors[i];

				if (!(rover->flags & FF_EXISTS)) continue;
				if(!(rover->flags & FF_SWIMMABLE) || rover->flags & FF_SOLID) continue;

				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(x, y);
				fixed_t ff_top=rover->top.plane->ZatPoint(x, y);

				if(ff_top <= z || ff_bottom > (z + (height >> 1))) continue;
				
				fh=ff_top;
				if (z < fh)
				{
					waterlevel = 1;
					if (z + height/2 < fh)
					{
						waterlevel = 2;
						if ((player && z + player->viewheight <= fh) ||
							(z + height <= fh))
						{
							waterlevel = 3;
						}
					}
				}

				break;
			}
		}
#endif
	}
		
	// some additional checks to make deep sectors like Boom's splash without setting
	// the water flags. 
	if (boomwaterlevel == 0 && waterlevel != 0 && dosplash) 
	{
		P_HitWater(this, Sector, FIXED_MIN, FIXED_MIN, fh, true);
	}
	boomwaterlevel = waterlevel;
	if (reset)
	{
		waterlevel = lastwaterlevel;
	}
	return false;	// we did the splash ourselves
}


//==========================================================================
//
//*****************************************************************************
//
template <typename T>
void IDList<T>::clear( void )
{
	for ( ULONG ulIdx = 0; ulIdx < MAX_NETID; ulIdx++ )
		freeID ( ulIdx );

	_firstFreeID = 1;
}

//*****************************************************************************
//
template <typename T>
void IDList<T>::rebuild( void )
{
	clear();

	T *pActor;

	TThinkerIterator<T> it;

	while ( (pActor = it.Next()) )
	{
		if (( pActor->lNetID > 0 ) && ( pActor->lNetID < MAX_NETID ))
			useID ( pActor->lNetID, pActor );
	}
}

//*****************************************************************************
//
template <typename T>
void IDList<T>::useID ( const LONG lNetID, T *pActor )
{
	if ( isIndexValid ( lNetID ) )
	{
		if ( ( _entries[lNetID].bFree == false ) && ( _entries[lNetID].pActor != pActor ) )
			SERVER_PrintWarning ( "IDList<T>::useID is using an already used ID.\n" );

		_entries[lNetID].bFree = false;
		_entries[lNetID].pActor = pActor;
	}
}

//*****************************************************************************
//
void CountActors ( ); // [BB]

template <typename T>
ULONG IDList<T>::getNewID( void )
{
	// Actor's network ID is the first availible net ID.
	ULONG ulID = _firstFreeID;

	do
	{
		_firstFreeID++;
		if ( _firstFreeID >= MAX_NETID )
			_firstFreeID = 1;

		if ( _firstFreeID == ulID )
		{
			// [BB] In case there is no free netID, the server has to abort the current game.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				// [BB] We can only spawn (MAX_NETID-2) actors with netID, because ID zero is reserved and
				// we already check that a new ID for the next actor is available when assign a net ID.
				Printf( "ACTOR_GetNewNetID: Network ID limit reached (>=%d actors)\n", MAX_NETID - 1 );
				CountActors ( );
				I_Error ("Network ID limit reached (>=%d actors)!\n", MAX_NETID - 1 );
			}

			return ( 0 );
		}
	} while ( _entries[_firstFreeID].bFree == false );

	return ( ulID );
}

template class IDList<AActor>;

// [BB] AActor::FreeNetID
//
//==========================================================================

void AActor::FreeNetID ()
{
	g_NetIDList.freeID ( lNetID );
	lNetID = -1;
}

//==========================================================================
//
// P_SpawnMobj
//
//==========================================================================

AActor *AActor::StaticSpawn (const PClass *type, fixed_t ix, fixed_t iy, fixed_t iz, replace_t allowreplacement, bool SpawningMapThing)
{
	g_lSpawnCount++;
	g_SpawnCycles.Clock();

	if (type == NULL)
	{
		I_Error ("Tried to spawn a class-less actor\n");
	}

	if (type->ActorInfo == NULL)
	{
		I_Error ("%s is not an actor\n", type->TypeName.GetChars());
	}

	if (allowreplacement)
		type = type->GetReplacement();


	AActor *actor;
	
	actor = static_cast<AActor *>(const_cast<PClass *>(type)->CreateNew ());

	// Set default dialogue
	actor->ConversationRoot = GetConversation(actor->GetClass()->TypeName);
	if (actor->ConversationRoot != -1)
	{
		actor->Conversation = StrifeDialogues[actor->ConversationRoot];
	}
	else
	{
		actor->Conversation = NULL;
	}

	actor->x = actor->PrevX = ix;
	actor->y = actor->PrevY = iy;
	actor->z = actor->PrevZ = iz;

	// [CK] Desync issues occur due to not having marked spawning actors with
	// the proper lastX/Y/Z, this will hopefully fix the floating glitch where
	// desync's between the server and client are fixed.
	actor->lastX = actor->x;
	actor->lastY = actor->y;
	actor->lastZ = actor->z;

	actor->picnum.SetInvalid();
	actor->health = actor->SpawnHealth();

	// Actors with zero gravity need the NOGRAVITY flag set.
	if (actor->gravity == 0) actor->flags |= MF_NOGRAVITY;

	FRandom &rng = pr_spawnmobj;

	if (actor->isFast() && actor->flags3 & MF3_ISMONSTER)
		actor->reactiontime = 0;

	if (actor->flags3 & MF3_ISMONSTER)
	{
		actor->LastLookPlayerNumber = rng() % MAXPLAYERS;
		actor->TIDtoHate = 0;
	}

	// Set the state, but do not use SetState, because action
	// routines can't be called yet.  If the spawnstate has an action
	// routine, it will not be called.
	FState *st = actor->SpawnState;
	actor->state = st;
	actor->tics = st->GetTics();
	
	actor->sprite = st->sprite;
	actor->frame = st->GetFrame();
	actor->renderflags = (actor->renderflags & ~RF_FULLBRIGHT) | st->GetFullbright();
	actor->touching_sectorlist = NULL;	// NULL head of sector list // phares 3/13/98
	if (G_SkillProperty(SKILLP_FastMonsters))
		actor->Speed = actor->GetClass()->Meta.GetMetaFixed(AMETA_FastSpeed, actor->Speed);

	// [BC]
	actor->InitialState = actor->state;

	// set subsector and/or block links
	actor->LinkToWorld (SpawningMapThing);

	actor->dropoffz =			// killough 11/98: for tracking dropoffs
	actor->floorz = actor->Sector->floorplane.ZatPoint (ix, iy);
	actor->ceilingz = actor->Sector->ceilingplane.ZatPoint (ix, iy);

	// The z-coordinate needs to be set once before calling P_FindFloorCeiling
	// For FLOATRANDZ just use the floor here.
	if (iz == ONFLOORZ || iz == FLOATRANDZ)
	{
		actor->z = actor->floorz;
	}
	else if (iz == ONCEILINGZ)
	{
		actor->z = actor->ceilingz - actor->height;
	}

	if (SpawningMapThing || !type->IsDescendantOf (RUNTIME_CLASS(APlayerPawn)))
	{
		// Check if there's something solid to stand on between the current position and the
		// current sector's floor. For map spawns this must be delayed until after setting the
		// z-coordinate.
		if (!SpawningMapThing) 
		{
			P_FindFloorCeiling(actor, FFCF_ONLYSPAWNPOS);
		}
		else
		{
			actor->floorsector = actor->Sector;
			actor->floorpic = actor->floorsector->GetTexture(sector_t::floor);
			actor->ceilingsector = actor->Sector;
			actor->ceilingpic = actor->ceilingsector->GetTexture(sector_t::ceiling);
		}
	}
	else if (!(actor->flags5 & MF5_NOINTERACTION))
	{
		P_FindFloorCeiling (actor);
	}
	else
	{
		actor->floorpic = actor->Sector->GetTexture(sector_t::floor);
		actor->floorsector = actor->Sector;
		actor->ceilingpic = actor->Sector->GetTexture(sector_t::ceiling);
		actor->ceilingsector = actor->Sector;
	}

	actor->SpawnPoint[0] = ix;
	actor->SpawnPoint[1] = iy;

	if (iz == ONFLOORZ)
	{
		actor->z = actor->floorz;
	}
	else if (iz == ONCEILINGZ)
	{
		actor->z = actor->ceilingz - actor->height;
	}
	else if (iz == FLOATRANDZ)
	{
		fixed_t space = actor->ceilingz - actor->height - actor->floorz;
		if (space > 48*FRACUNIT)
		{
			space -= 40*FRACUNIT;
			actor->z = MulScale8 (space, rng()) + actor->floorz + 40*FRACUNIT;
		}
		else
		{
			actor->z = actor->floorz;
		}
	}
	else
	{
		actor->SpawnPoint[2] = (actor->z - actor->Sector->floorplane.ZatPoint(actor->x,actor->y));
	}

	if (actor->FloatBobPhase == (BYTE)-1) actor->FloatBobPhase = rng();	// Don't make everything bob in sync (unless deliberately told to do)
	if (actor->flags2 & MF2_FLOORCLIP)
	{
		actor->AdjustFloorClip ();
	}
	else
	{
		actor->floorclip = 0;
	}
	actor->UpdateWaterLevel (actor->z, false);
	if (!SpawningMapThing)
	{
		actor->BeginPlay ();
		if (actor->ObjectFlags & OF_EuthanizeMe)
		{
			g_SpawnCycles.Unclock();
			return NULL;
		}
	}
	if (level.flags & LEVEL_NOALLIES && !actor->player)
	{
		actor->flags &= ~MF_FRIENDLY;
	}
	// [RH] Count monsters whenever they are spawned.
	if (actor->CountsAsKill())
	{
		level.total_monsters++;

		// [BB] The number of total monsters was increased, update the invasion monster count accordingly.
		INVASION_UpdateMonsterCount( actor, false );
	}
	// [RH] Same, for items
	if (actor->flags & MF_COUNTITEM)
	{
		level.total_items++;
	}
	// And for secrets
	if (actor->flags5 & MF5_COUNTSECRET)
	{
		// [Zandronum] The total amount of secrets counter should be synched with server.
		if ( !NETWORK_InClientMode( ) )
			level.total_secrets++;
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetMapNumTotalSecrets();
	}

	if ((( actor->ulNetworkFlags & NETFL_NONETID ) == false ) && ( ( actor->ulNetworkFlags & NETFL_SERVERSIDEONLY ) == false ) &&
		( NETWORK_InClientMode() == false ))
	{
		actor->lNetID = g_NetIDList.getNewID( );
		g_NetIDList.useID ( actor->lNetID, actor );
		if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && sv_showspawnnames )
			Printf ( "%s %d\n", actor->GetClass()->TypeName.GetChars(), actor->lNetID );
	}
	else
		actor->lNetID = -1;

	// [BB] Initilize the colormap of this actor.
	actor->lFixedColormap = NOFIXEDCOLORMAP;

	// Check if the flag or skull has spawned in an instant return zone.
	if (( TEAM_SpawningTemporaryFlag( ) == false ) &&
		( actor->Sector->MoreFlags & SECF_RETURNZONE ) &&
		( NETWORK_InClientMode() == false ))
	{
		for ( ULONG i = 0; i < teams.Size( ); i++ )
		{
			if ( actor->GetClass( ) == TEAM_GetItem( i ))
				TEAM_ExecuteReturnRoutine( i, NULL );
		}

		if (( oneflagctf ) && ( actor->GetClass( ) == PClass::FindClass( "WhiteFlag" )))
			TEAM_ExecuteReturnRoutine( teams.Size( ), NULL );
	}

	g_SpawnCycles.Unclock();
	return actor;
}

AActor *Spawn (const char *type, fixed_t x, fixed_t y, fixed_t z, replace_t allowreplacement)
{
	FName classname(type, true);
	if (classname == NAME_None)
	{
		I_Error("Attempt to spawn actor of unknown type '%s'\n", type);
	}
	return Spawn(classname, x, y, z, allowreplacement);
}

AActor *Spawn (FName classname, fixed_t x, fixed_t y, fixed_t z, replace_t allowreplacement)
{
	const PClass *cls = PClass::FindClass(classname);
	if (cls == NULL) 
	{
		I_Error("Attempt to spawn actor of unknown type '%s'\n", classname.GetChars());
	}
	return AActor::StaticSpawn (cls, x, y, z, allowreplacement);
}

void AActor::LevelSpawned ()
{
	if (tics > 0 && !(flags4 & MF4_SYNCHRONIZED))
	{
		tics = 1 + (pr_spawnmapthing() % tics);
	}
	// [RH] Clear MF_DROPPED flag if the default version doesn't have it set.
	// (AInventory::BeginPlay() makes all inventory items spawn with it set.)
	if (!(GetDefault()->flags & MF_DROPPED))
	{
		flags &= ~MF_DROPPED;
	}
	HandleSpawnFlags ();

	// [BC] Mark this item as having been spawned on the map.
	ulSTFlags |= STFL_LEVELSPAWNED;
}

void AActor::HandleSpawnFlags ()
{
	if (SpawnFlags & MTF_AMBUSH)
	{
		flags |= MF_AMBUSH;
	}
	if (SpawnFlags & MTF_DORMANT)
	{
		Deactivate (NULL);
	}
	if (SpawnFlags & MTF_STANDSTILL)
	{
		flags4 |= MF4_STANDSTILL;
	}
	if (SpawnFlags & MTF_FRIENDLY)
	{
		flags |= MF_FRIENDLY;
		// Friendlies don't count as kills!
		if (flags & MF_COUNTKILL)
		{
			flags &= ~MF_COUNTKILL;
			level.total_monsters--;
		}
	}
	if (SpawnFlags & MTF_SHADOW)
	{
		flags |= MF_SHADOW;
		RenderStyle = STYLE_Translucent;
		alpha = TRANSLUC25;
	}
	else if (SpawnFlags & MTF_ALTSHADOW)
	{
		RenderStyle = STYLE_None;
	}
	if (SpawnFlags & MTF_SECRET)
	{
		if (!(flags5 & MF5_COUNTSECRET))
		{
			//Printf("Secret %s in sector %i!\n", GetTag(), Sector->sectornum);
			flags5 |= MF5_COUNTSECRET;
			level.total_secrets++;
		}
	}

	// [BC]
	InitialState = state;
}

void AActor::BeginPlay ()
{
	// If the actor is spawned with the dormant flag set, clear it, and use
	// the normal deactivation logic to make it properly dormant.
	if (flags2 & MF2_DORMANT)
	{
		flags2 &= ~MF2_DORMANT;
		Deactivate (NULL);
	}
}

void AActor::PostBeginPlay ()
{
	if (Renderer != NULL)
	{
		Renderer->StateChanged(this);
	}
	PrevAngle = angle;
	flags7 |= MF7_HANDLENODELAY;
}

void AActor::MarkPrecacheSounds() const
{
	SeeSound.MarkUsed();
	AttackSound.MarkUsed();
	PainSound.MarkUsed();
	DeathSound.MarkUsed();
	ActiveSound.MarkUsed();
	UseSound.MarkUsed();
	BounceSound.MarkUsed();
	WallBounceSound.MarkUsed();
	CrushPainSound.MarkUsed();
}

bool AActor::isFast()
{
	if (flags5&MF5_ALWAYSFAST) return true;
	if (flags5&MF5_NEVERFAST) return false;
	return !!G_SkillProperty(SKILLP_FastMonsters);
}

bool AActor::isSlow()
{
	return !!G_SkillProperty(SKILLP_SlowMonsters);
}

void AActor::Activate (AActor *activator)
{
	if ((flags3 & MF3_ISMONSTER) && (health > 0 || (flags & MF_ICECORPSE)))
	{
		if (flags2 & MF2_DORMANT)
		{
			flags2 &= ~MF2_DORMANT;
			FState *state = FindState(NAME_Active);
			if (state != NULL) 
			{
				SetState(state);
			}
			else
			{
				tics = 1;
			}
		}
	}
}

void AActor::Deactivate (AActor *activator)
{
	if ((flags3 & MF3_ISMONSTER) && (health > 0 || (flags & MF_ICECORPSE)))
	{
		if (!(flags2 & MF2_DORMANT))
		{
			flags2 |= MF2_DORMANT;
			FState *state = FindState(NAME_Inactive);
			if (state != NULL) 
			{
				SetState(state);
			}
			else
			{
				tics = -1;
			}
		}
	}
}

//=============================================================================
//
//	[BC] AActor::IsActive
//
//	Returns true if the actor is active, and false if the actor has been
//	deactivated.
//
//=============================================================================

bool AActor::IsActive( void ) const
{
	return (( flags2 & MF2_DORMANT ) == false );
}

//
// P_RemoveMobj
//

void AActor::Destroy ()
{
	// [BC/BB] Free it's network ID.
	g_NetIDList.freeID ( lNetID );

	lNetID = -1;

	// [BB] If this is a monster corpse, we potentially have to NULL out the reference to it.
	if ( invasion )
		INVASION_ClearMonsterCorpsePointer( this );

	// [BC] If this is a bot's goal object, tell the bot it's been removed.
	BOTS_RemoveGoal ( this );

	// [RH] Destroy any inventory this actor is carrying
	DestroyAllInventory ();

	// [RH] Unlink from tid chain
	RemoveFromHash ();

	// unlink from sector and block lists
	UnlinkFromWorld ();
	flags |= MF_NOSECTOR|MF_NOBLOCKMAP;

	// Delete all nodes on the current sector_list			phares 3/16/98
	P_DelSector_List();

	// Transform any playing sound into positioned, non-actor sounds.
	S_RelinkSound (this, NULL);

	Super::Destroy ();
}

//===========================================================================
//
// AdjustFloorClip
//
//===========================================================================

void AActor::AdjustFloorClip ()
{
	if (flags3 & MF3_SPECIALFLOORCLIP)
	{
		return;
	}

	fixed_t oldclip = floorclip;
	fixed_t shallowestclip = FIXED_MAX;
	const msecnode_t *m;

	// possibly standing on a 3D-floor!
	if (Sector->e->XFloor.ffloors.Size() && z>Sector->floorplane.ZatPoint(x,y)) floorclip=0;

	// [RH] clip based on shallowest floor player is standing on
	// If the sector has a deep water effect, then let that effect
	// do the floorclipping instead of the terrain type.
	for (m = touching_sectorlist; m; m = m->m_tnext)
	{
		sector_t *hsec = m->m_sector->GetHeightSec();
		if (hsec == NULL && m->m_sector->floorplane.ZatPoint (x, y) == z)
		{
			fixed_t clip = Terrains[TerrainTypes[m->m_sector->GetTexture(sector_t::floor)]].FootClip;
			if (clip < shallowestclip)
			{
				shallowestclip = clip;
			}
		}
	}
	if (shallowestclip == FIXED_MAX)
	{
		floorclip = 0;
	}
	else
	{
		floorclip = shallowestclip;
	}
	if (player && player->mo == this && oldclip != floorclip)
	{
		// [BC] Don't adjust viewheight if we're just predicting.
		if ( CLIENT_PREDICT_IsPredicting( ) == false )
		{
			player->viewheight -= oldclip - floorclip;
			player->deltaviewheight = player->GetDeltaViewHeight();
		}
	}
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged between levels.
//
EXTERN_CVAR (Bool, chasedemo)

extern bool demonew;

APlayerPawn *P_SpawnPlayer (FPlayerStart *mthing, int playernum, int flags)
{
	player_t *p;
	APlayerPawn *mobj, *oldactor;
	// [BB] I want this to be const, so we need declare it later.
	//BYTE	  state;
	fixed_t spawn_x, spawn_y, spawn_z;
	angle_t spawn_angle;
	// [BC]
	LONG		lSkin;
	AInventory	*pInventory;

	// not playing?
	if ((unsigned)playernum >= (unsigned)MAXPLAYERS || !playeringame[playernum])
		return NULL;

	p = &players[playernum];

	// [BB] Make sure that the player only uses a class available to his team.
	if ( !(flags & SPF_TEMPPLAYER) )
		TEAM_EnsurePlayerHasValidClass ( p );

	// [BB] We may not filter coop inventory if the player changed the player class.
	// Thus we need to keep track of the old class.
	const BYTE oldPlayerClass = p->CurrentPlayerClass;

	// [BB] The (p->userinfo.GetPlayerClassNum() != p->CurrentPlayerClass) check allows the player to change its class when respawning.
	// We have to make sure though that the class is not changed when traveling from one map to the next, because a travelling
	// player gets its inventory from the last map (which of course belongs to the previous class) after being spawned completely.
	if (p->cls == NULL || ( (p->userinfo.GetPlayerClassNum() != p->CurrentPlayerClass) && ( p->playerstate != PST_LIVE ) ) )
	{
		// [GRB] Pick a class from player class list
		if (PlayerClasses.Size () > 1)
		{
			int type;

			// [BC] Cooperative is !deathmatch && !teamgame.
			// [BB] The server host always picks the multiplayer class choice.
			if ( ((!deathmatch && !teamgame) || ( NETWORK_GetState( ) == NETSTATE_SINGLE )) && !( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			{
				type = SinglePlayerClass[playernum];
			}
			else
			{
				type = p->userinfo.GetPlayerClassNum();
				if (type < 0)
				{
					// [BB] If the player is on a team, only a class valid for this team may be selected.
					if ( p->bOnTeam )
						type = TEAM_SelectRandomValidPlayerClass( p->ulTeam );
					else
						type = pr_multiclasschoice() % PlayerClasses.Size ();
				}
			}
			p->CurrentPlayerClass = type;
		}
		else
		{
			p->CurrentPlayerClass = 0;
		}
		p->cls = PlayerClasses[p->CurrentPlayerClass].Type;
	}

	if (( dmflags2 & DF2_SAME_SPAWN_SPOT ) &&
		(( p->playerstate == PST_REBORN ) || ( p->playerstate == PST_REBORNNOINVENTORY )) && 
		( deathmatch == false ) &&
		( teamgame == false ) &&
		( gameaction != ga_worlddone ) &&
		( p->bSpawnOkay ) && 
		( p->mo != NULL ) && 
		( !(p->mo->Sector->Flags & SECF_NORESPAWN) ) &&
		( (p->mo->Sector->special & 255) != Damage_InstantDeath ))
	{
		spawn_x = p->mo->x;
		spawn_y = p->mo->y;
		spawn_angle = p->mo->angle;
	}
	else
	{
		spawn_x = mthing->x;
		spawn_y = mthing->y;
		// Allow full angular precision but avoid roundoff errors for multiples of 45 degrees.
		if (mthing->angle % 45 != 0)
		{
			spawn_angle = mthing->angle * (ANG45 / 45);
		}
		else
		{
			spawn_angle = ANG45 * (mthing->angle / 45);
		}
		if (i_compatflags2 & COMPATF2_BADANGLES)
		{
			spawn_angle += 1 << ANGLETOFINESHIFT;
		}
	}

	if (GetDefaultByType(p->cls)->flags & MF_SPAWNCEILING)
		spawn_z = ONCEILINGZ;
	else if (GetDefaultByType(p->cls)->flags2 & MF2_SPAWNFLOAT)
		spawn_z = FLOATRANDZ;
	else
		spawn_z = ONFLOORZ;

	mobj = static_cast<APlayerPawn *>
		(Spawn (p->cls, spawn_x, spawn_y, spawn_z, NO_REPLACE));

	if (level.flags & LEVEL_USEPLAYERSTARTZ)
	{
		if (spawn_z == ONFLOORZ)
			mobj->z += mthing->z;
		else if (spawn_z == ONCEILINGZ)
			mobj->z -= mthing->z;
		P_FindFloorCeiling(mobj, FFCF_SAMESECTOR | FFCF_ONLY3DFLOORS | FFCF_3DRESTRICT);
	}

	mobj->FriendPlayer = playernum + 1;	// [RH] players are their own friends
	oldactor = p->mo;
	p->mo = mobj;
	mobj->player = p;
	// [BB] The code below assumes that state won't be altered.
	const BYTE state = p->playerstate;
	// [BC] Added PST_REBORNNOINVENTORY, PST_ENTERNOINVENTORY.
	if (state == PST_REBORN || state == PST_ENTER || state == PST_REBORNNOINVENTORY || state == PST_ENTERNOINVENTORY)
	{
		// [BB] The server may not start handing out the inventory yet, see further down.
		G_PlayerReborn (playernum, ( NETWORK_GetState( ) != NETSTATE_SERVER ) );
	}
	else if (oldactor != NULL && oldactor->player == p && !(flags & SPF_TEMPPLAYER))
	{
		// Move the voodoo doll's inventory to the new player.
		mobj->ObtainInventory (oldactor);
		FBehavior::StaticStopMyScripts (oldactor);	// cancel all ENTER/RESPAWN scripts for the voodoo doll
	}

	// [GRB] Reset skin
	p->userinfo.SkinNumChanged(R_FindSkin (skins[p->userinfo.GetSkin()].name, p->CurrentPlayerClass));

	if (!(mobj->flags2 & MF2_DONTTRANSLATE))
	{
		// [RH] Be sure the player has the right translation
		R_BuildPlayerTranslation (playernum);

		// [RH] set color translations for player sprites
		mobj->Translation = TRANSLATION(TRANSLATION_Players,playernum);
	}

	mobj->angle = spawn_angle;
	mobj->pitch = mobj->roll = 0;
	mobj->health = p->health;
	mobj->lFixedColormap = NOFIXEDCOLORMAP;

	//Added by MC: Identification (number in the players[MAXPLAYERS] array)
    mobj->id = playernum;

	// [RH] Set player sprite based on skin
	// [BC] Handle cl_skins here.
	if ( cl_skins <= 0 )
	{
		lSkin = R_FindSkin( "base", p->CurrentPlayerClass );
		mobj->flags4 |= MF4_NOSKIN;
	}
	else if ( cl_skins >= 2 )
	{
		if ( skins[p->userinfo.GetSkin()].bCheat )
		{
			lSkin = R_FindSkin( "base", p->CurrentPlayerClass );
			mobj->flags4 |= MF4_NOSKIN;
		}
		else
			lSkin = p->userinfo.GetSkin();
	}
	else
		lSkin = p->userinfo.GetSkin();

	if (( lSkin < 0 ) || ( static_cast<unsigned> (lSkin) >= skins.Size() ))
		lSkin = R_FindSkin( "base", p->CurrentPlayerClass );

	if (!(mobj->flags4 & MF4_NOSKIN))
	{
		mobj->sprite = skins[lSkin].sprite;
	}

	p->DesiredFOV = p->FOV = 90.f;
	p->camera = p->mo;
	p->playerstate = PST_LIVE;
	p->refire = 0;
	p->damagecount = 0;
	p->bonuscount = 0;
	p->morphTics = 0;
	p->MorphedPlayerClass = 0;
	p->MorphStyle = 0;
	p->MorphExitFlash = NULL;
	p->extralight = 0;
	p->fixedcolormap = NOFIXEDCOLORMAP;
	p->fixedlightlevel = -1;
	p->viewheight = mobj->ViewHeight;
	p->attacker = NULL;
	p->BlendR = p->BlendG = p->BlendB = p->BlendA = 0.f;
	p->mo->ResetAirSupply(false);
	p->Uncrouch();
	p->MinPitch = p->MaxPitch = 0;	// will be filled in by PostBeginPlay()/netcode
	p->cheats &= ~CF_FLY;

	p->velx = p->vely = 0;		// killough 10/98: initialize bobbing to 0.

	if ( NETWORK_GetState( ) != NETSTATE_SERVER )
	{
		for (int ii = 0; ii < MAXPLAYERS; ++ii)
		{
			if (playeringame[ii] && players[ii].camera == oldactor)
			{
				players[ii].camera = mobj;
			}
		}
	}

	// [RH] Allow chasecam for demo watching
	if ((demoplayback || demonew) && chasedemo)
		p->cheats = CF_CHASECAM;

	if ( p->bSpectating )
	{
		if ( p->mo )
		{
			// Don't allow "dead" spectators!
			players[playernum].playerstate = PST_LIVE;
			players[playernum].health = players[playernum].mo->health = deh.StartHealth;

			// [BB] Set a bunch of stuff, e.g. make the player unshootable, etc.
			PLAYER_SetDefaultSpectatorValues ( p );
		}

		if ( p->pSkullBot )
			p->pSkullBot->PostEvent( BOTEVENT_SPECTATING );
	}
	// [BB] If this not a coop game and cheats are not allowed, remove the chasecam.
	else if ( !( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE ) && ( sv_cheats == false ) )
	{
		p->cheats &= ~(CF_CHASECAM);
	}

	// [BB] Moved this here, needs to be done before spawning the player on the clients.
	// "Fix" for one of the starts on exec.wad MAP01: If you start inside the ceiling,
	// drop down below it, even if that means sinking into the floor.
	if (mobj->z + mobj->height > mobj->ceilingz)
	{
		mobj->z = mobj->ceilingz - mobj->height;
	}

	// Tell clients about the respawning player.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		if ( flags & SPF_CLIENTUPDATE )
			SERVERCOMMANDS_SpawnPlayer( playernum, state );

		// [BB] We may only start handing out the inventory when the player is already spawned on the clients.
		// Otherwise items that use certain code pointers (e.g. A_GiveInventory) during the first tic of their
		// Pickup state don't work properly as start items.
		if (state == PST_REBORN || state == PST_ENTER || state == PST_REBORNNOINVENTORY || state == PST_ENTERNOINVENTORY)
		{
			if (gamestate != GS_TITLELEVEL)
			{
				p->mo->GiveDefaultInventory ();
				// [BB] The default inventory possibly alters the player's health. Thus we need to make sure
				// that the health of the player's body matches the player's health.
				mobj->health = p->health;
			}
		}

		// [Dusk] If we are sharing keys, give this player the keys that have been found.
		if (( flags & SPF_CLIENTUPDATE ) &&
			( zadmflags & ZADF_SHARE_KEYS ) &&
			( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
			( state == PST_ENTER || state == PST_ENTERNOINVENTORY ))
		{
			SERVER_SyncSharedKeys( p - players, true );
		}
	}

	// setup gun psprite
	if (!(flags & SPF_TEMPPLAYER))
	{ // This can also start a script so don't do it for the dummy player.
		P_SetupPsprites (p, !!(flags & SPF_WEAPONFULLYUP));
	}

	if (deathmatch)
	{ // Give all cards in death match mode.
		p->mo->GiveDeathmatchInventory ();
	}
	// [BC] Don't filter coop inventory in teamgame mode.
	// [BB] Also don't do so if the player changed the player class.
	else if ((( NETWORK_GetState( ) != NETSTATE_SINGLE ) || (level.flags2 & LEVEL2_ALLOWRESPAWN) ) && state == PST_REBORN && oldactor != NULL && ( teamgame == false ) && ( oldPlayerClass == p->CurrentPlayerClass ) )
	{ // Special inventory handling for respawning in coop
		p->mo->FilterCoopRespawnInventory (oldactor);
	}
	if (oldactor != NULL)
	{ // Remove any inventory left from the old actor. Coop handles
	  // it above, but the other modes don't.
		oldactor->DestroyAllInventory();
	}

	// [BC] Apply temporary invulnerability when respawned.
	if (( NETWORK_InClientMode() == false ) &&
		// [BB] Added PST_REBORNNOINVENTORY, PST_ENTERNOINVENTORY.
		(state == PST_REBORN || state == PST_ENTER || state == PST_REBORNNOINVENTORY || state == PST_ENTERNOINVENTORY) &&
		(( dmflags2 & DF2_NO_RESPAWN_INVUL ) == false ) &&
		( deathmatch || teamgame || alwaysapplydmflags ) &&
		( p->bSpectating == false ))
	{
		APowerup *invul = static_cast<APowerup*>(p->mo->GiveInventoryType (RUNTIME_CLASS(APowerInvulnerable)));
		// [BB] It's possible that giving the powerup fails, e.g. in Cutman's Level Master.
		if ( invul != NULL )
		{
			invul->EffectTics = 3*TICRATE;
			invul->BlendColor = 0;			// don't mess with the view
			invul->ItemFlags |= IF_UNDROPPABLE;	// Don't drop this
			// [BB] The clients are informed about the powerup and these adjustments later.

			// Apply respawn invulnerability effect.
			switch ( cl_respawninvuleffect )
			{
			case 1:

				p->mo->RenderStyle = STYLE_Translucent;
				p->mo->effects |= FX_VISIBILITYFLICKER;
				break;
			case 2:

				p->mo->effects |= FX_RESPAWNINVUL;	// [RH] special effect
				break;
			}
		}
	}

	if (StatusBar != NULL && (playernum == consoleplayer || StatusBar->GetPlayer() == playernum))
	{
		StatusBar->AttachToPlayer (p);
	}

	// [BC] Don't spawn fog for dead spectators. They are spawned when they leave their
	// body to disassociate with their corpse.
	// [BB] Don't spawn fog for spectators at all.
	// [BB] Don't spawn fog for temp players.
	// [EP] Don't spawn fog for facing west spawners offline, if compatflag is on.
	if (( NETWORK_GetState( ) != NETSTATE_SINGLE ) &&
		( p->bDeadSpectator == false ) && ( p->bSpectating == false ) && !(flags & SPF_TEMPPLAYER ) &&
		( !( zacompatflags & ZACOMPATF_SILENT_WEST_SPAWNS ) || mobj->angle != ANGLE_180 ))
	{
		unsigned an = mobj->angle >> ANGLETOFINESHIFT;
		// [BB] Save the pointer.
		AActor *pFog = Spawn ("TeleportFog", mobj->x+20*finecosine[an], mobj->y+20*finesine[an], mobj->z + TELEFOGHEIGHT, ALLOW_REPLACE);
		// [BB] Clients spawn the fog on their own. Giving the fog the NETFL_ALLOWCLIENTSPAWN flag will prevent
		// the server from telling the clients to spawn the fog again during a full update.
		if ( pFog && ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
		{
			pFog->ulNetworkFlags |= NETFL_ALLOWCLIENTSPAWN;
			// [BB] Also remove the netID. Otherwise, the server will not know that 
			// it cannot notify the client about changes to the fog, like when it is
			// destroyed during a map reset.
			pFog->FreeNetID();
		}
	}

	// [BB] Moved the exec.wad MAP01 "fix" up.

	// Tell clients about the respawning player.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// [BB] The clients start their reactiontime later on their end. Try to adjust for this.
		SERVER_AdjustPlayersReactiontime ( playernum );

		pInventory = mobj->FindInventory( RUNTIME_CLASS( APowerInvulnerable ));
		if (( pInventory ) && ( p->bSpectating == false ))
		{
			SERVERCOMMANDS_GivePowerup( playernum, static_cast<APowerup *>( pInventory ));
			SERVERCOMMANDS_PlayerRespawnInvulnerability( playernum );
		}
	}
	// [BC] Do script stuff
	if (!(flags & SPF_TEMPPLAYER))
	{
		if (state == PST_ENTER || state == PST_ENTERNOINVENTORY || (state == PST_LIVE && !savegamerestore))
		{
			// [BC] Don't run enter scripts if we're spectating.
			if ( p->bSpectating == false )
			{
				// [BC] If we're the server, just mark this client as needing to have his enter
				// scripts run when he actually authenticates the level.
				// [BB] Enter scripts of bots have to be called immediately.
				if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
					( p->bIsBot == false ) &&
					( SERVER_GetClient( p - players )->State != CLS_SPAWNED ))
				{
					SERVER_GetClient( p - players )->bRunEnterScripts = true;
				}
				else
					FBehavior::StaticStartTypedScripts (SCRIPT_Enter, p->mo, true);

				// [BB] If there is any stored inventory picked up by unassigned voodoo dolls
				// give it to the entering player.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					COOP_GiveStoredUVDPickupsToPlayer ( static_cast<ULONG> ( p - players ) );
			}
		}
		// [BC] Added PST_REBORNNOINVENTORY.
		else if (state == PST_REBORN || state == PST_REBORNNOINVENTORY)
		{
			//assert (oldactor != NULL);
			// [BC] In Skulltag, it's okay if the player is being reborn without an oldactor,
			// since there's times where we delete a player's body, and respawn him without him
			// having been killed (ex. when a player changes teams).
			if ( oldactor != NULL )
			{
				// before relocating all pointers to the player all sound targets
				// pointing to the old actor have to be NULLed. Otherwise all
				// monsters who last targeted this player will wake up immediately
				// after the player has respawned.
				AActor *th;
				TThinkerIterator<AActor> it;
				while ((th = it.Next()))
				{
					if (th->LastHeard == oldactor) th->LastHeard = NULL;
				}
				for(int i = 0; i < numsectors; i++)
				{
					if (sectors[i].SoundTarget == oldactor) sectors[i].SoundTarget = NULL;
				}

				DObject::StaticPointerSubstitution (oldactor, p->mo);
				// PointerSubstitution() will also affect the bodyque, so undo that now.
				for (int ii=0; ii < BODYQUESIZE; ++ii)
					if (bodyque[ii] == p->mo)
						bodyque[ii] = oldactor;
			}
			// [BB] Don't run respawn scripts if we're spawning a dead spectator body.
			if ( p->bDeadSpectator == false )
				FBehavior::StaticStartTypedScripts (SCRIPT_Respawn, p->mo, true);
		}
	}

	// Incoming players have various stats reset.
	if ( state == PST_ENTER )
	{
		p->ulTime = 0;
		p->ulDeathCount = 0;
		if ( GAMEMODE_GetCurrentFlags() & GMF_USEMAXLIVES )
		{
			PLAYER_SetLivesLeft ( p, GAMEMODE_GetMaxLives() - 1 );
		}
	}
	// If this is a bot, tell him he's been respawned.
	else if (( state == PST_REBORN ) || ( state == PST_REBORNNOINVENTORY ))
	{
		if ( p->pSkullBot )
			p->pSkullBot->PostEvent( BOTEVENT_RESPAWNED );
	}

	// If this is a bot, clear out their path. They won't have a path if they don't have a
	// script, however.
	if (( p->pSkullBot ) && ( p->pSkullBot->m_bHasScript ))
	{
		p->pSkullBot->m_ulPathType = BOTPATHTYPE_NONE;
		ASTAR_ClearPath( p - players );
	}

	// [TP] Set up the player's translation now if we override it.
	// Note: this mostly takes care of offline handling. Clients do this
	// in CLIENT_SpawnPlayer.
	if ( D_ShouldOverridePlayerColors() )
	{
		bool joinedgame = ( p == &players[consoleplayer] )
			&& ( state == PST_ENTER || state == PST_ENTERNOINVENTORY );
		D_UpdatePlayerColors( joinedgame ? MAXPLAYERS : p - players );
	}

	SCOREBOARD_RefreshHUD( );

	// [Spleen] Reset reconciliation buffer when player gets spawned
	UNLAGGED_ResetPlayer( p );

	return mobj;
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
// [RH] position is used to weed out unwanted start spots
AActor *P_SpawnMapThing (FMapThing *mthing, int position)
{
	const PClass *i;
	int mask;
	AActor *mobj;
	fixed_t x, y, z;

	if (mthing->type == 0 || mthing->type == -1)
		return NULL;

	// count deathmatch start positions
	if (mthing->type == 11)
	{
		FPlayerStart start(mthing);
		deathmatchstarts.Push(start);
		return NULL;
	}

	// [BC] Count temporary team starts.
	if ( mthing->type == 5082 )
	{
		FPlayerStart start( mthing );
		TemporaryTeamStarts.Push( start );
		return NULL;
	}

	// [CW] Count team starts.
	for ( ULONG i = 0; i < teams.Size( ); i++ )
	{
		if ( mthing->type == static_cast<int> (teams[i].ulPlayerStartThingNumber) )
		{
			FPlayerStart start( mthing );
			teams[i].TeamStarts.Push( start );
			return NULL;
		}
	}

	// [RC] Catalog possession starts
	if ( mthing->type == 6000 )
	{
		FPlayerStart start( mthing );
		PossessionStarts.Push( start );
		return NULL;
	}

	// [RC] Catalog terminator starts
	if ( mthing->type == 6001 )
	{
		FPlayerStart start( mthing );
		TerminatorStarts.Push( start );
		return NULL;
	}

	// [BC/BB] Count invasion starts.
	if ( INVASION_IsMapThingInvasionSpot( mthing ) )
	{
		FPlayerStart start( mthing );
		GenericInvasionStarts.Push( start );
	}

	// Convert Strife starts to Hexen-style starts
	if (gameinfo.gametype == GAME_Strife && mthing->type >= 118 && mthing->type <= 127)
	{
		mthing->args[0] = mthing->type - 117;
		mthing->type = 1;
	}

	// [RH] Record polyobject-related things
	if (gameinfo.gametype == GAME_Hexen)
	{
		switch (mthing->type)
		{
		case PO_HEX_ANCHOR_TYPE:
			mthing->type = PO_ANCHOR_TYPE;
			break;
		case PO_HEX_SPAWN_TYPE:
			mthing->type = PO_SPAWN_TYPE;
			break;
		case PO_HEX_SPAWNCRUSH_TYPE:
			mthing->type = PO_SPAWNCRUSH_TYPE;
			break;
		}
	}

	if (mthing->type == PO_ANCHOR_TYPE ||
		mthing->type == PO_SPAWN_TYPE ||
		mthing->type == PO_SPAWNCRUSH_TYPE ||
		mthing->type == PO_SPAWNHURT_TYPE)
	{
		polyspawns_t *polyspawn = new polyspawns_t;
		polyspawn->next = polyspawns;
		polyspawn->x = mthing->x;
		polyspawn->y = mthing->y;
		polyspawn->angle = mthing->angle;
		polyspawn->type = mthing->type;
		polyspawns = polyspawn;
		if (mthing->type != PO_ANCHOR_TYPE)
			po_NumPolyobjs++;
		return NULL;
	}

	// check for players specially
	int pnum = -1;

	if (mthing->type <= 4 && mthing->type > 0)
	{
		pnum = mthing->type - 1;
	}
	else
	{
		// [BC] We can't use the value of 32 for MAXPLAYERS here, otherwise it
		// messes with Strife items.
		// [BB] Only apply the restriction if we actually play Strife.
		const int maxPlayerStarts = (gameinfo.gametype == GAME_Strife) ? 8 : MAXPLAYERS;
		if (mthing->type >= gameinfo.player5start && mthing->type < gameinfo.player5start + maxPlayerStarts - 4)
		{
			pnum = mthing->type - gameinfo.player5start + 4;
		}
	}

	if (pnum == -1 || (level.flags & LEVEL_FILTERSTARTS))
	{
		// [BC] These checks are now done later.
/*
		// check for appropriate game type
		if (deathmatch) 
		{
			mask = MTF_DEATHMATCH;
		}
		else if ( NETWORK_GetState( ) != NETSTATE_SINGLE )
		{
			mask = MTF_COOPERATIVE;
		}
		else
		{
			mask = MTF_SINGLE;
		}
		if (!(mthing->flags & mask))
		{
			return NULL;
		}
*/
		// check for apropriate skill level
		mask = G_SkillProperty(SKILLP_SpawnFilter);
		if (!(mthing->SkillFilter & mask))
		{
			return NULL;
		}

		// Check class spawn masks. Now with player classes available
		// this is enabled for all games.
		if ( NETWORK_GetState( ) == NETSTATE_SINGLE )
		{ // Single player
			int spawnmask = players[consoleplayer].GetSpawnClass();
			if (spawnmask != 0 && (mthing->ClassFilter & spawnmask) == 0)
			{ // Not for current class
				return NULL;
			}
		}
		// [BC] Cooperative = !deathmatch && !teamgame, and also always spawn the item
		// in multiplayer.
		else if ((!deathmatch && !teamgame) && ( NETWORK_GetState( ) == NETSTATE_SINGLE ))
		{ // Cooperative
			mask = 0;
			for (int i = 0; i < MAXPLAYERS; i++)
			{
				if (playeringame[i])
				{
					int spawnmask = players[i].GetSpawnClass();
					if (spawnmask != 0)
						mask |= spawnmask;
					else 
						mask = -1;
				}
			}
			if (mask != -1 && (mthing->ClassFilter & mask) == 0)
			{
				return NULL;
			}
		}
	}

	if (pnum != -1)
	{
		// [RH] Only spawn spots that match position.
		if (mthing->args[0] != position)
			return NULL;

		// save spots for respawning in network games
		FPlayerStart start(mthing);
		playerstarts[pnum] = start;
		AllPlayerStarts.Push(start);

		// [BB] To spawn voodoo dolls in multiplayer games, we need to keep track of all starts for every player.
		AllStartsOfPlayer[pnum].Push( start );

		// [BB] Zandronum handles spawning differently.
		// if (!deathmatch && !(level.flags2 & LEVEL2_RANDOMPLAYERSTARTS))
		// [BC] Need to maintain this method so that we get voodoo dolls, which some doomers are used to.
		if ( NETWORK_GetState( ) == NETSTATE_SINGLE && ( deathmatch == false ) && ( teamgame == false ))
			return P_SpawnPlayer(&start, pnum, (level.flags2 & LEVEL2_PRERAISEWEAPON) ? SPF_WEAPONFULLYUP : 0);
		return NULL;
	}

	// [RH] sound sequence overriders
	if (mthing->type >= 1400 && mthing->type < 1410)
	{
		P_PointInSector (mthing->x, mthing->y)->seqType = mthing->type - 1400;
		return NULL;
	}
	else if (mthing->type == 1411)
	{
		int type;

		if (mthing->args[0] == 255)
			type = -1;
		else
			type = mthing->args[0];

		if (type > 63)
		{
			Printf ("Sound sequence %d out of range\n", type);
		}
		else
		{
			P_PointInSector (mthing->x,	mthing->y)->seqType = type;
		}
		return NULL;
	}

	// [RH] Determine if it is an old ambient thing, and if so,
	//		map it to MT_AMBIENT with the proper parameter.
	if (mthing->type >= 14001 && mthing->type <= 14064)
	{
		mthing->args[0] = mthing->type - 14000;
		mthing->type = 14065;
	}
	else if (mthing->type >= 14101 && mthing->type <= 14164)
	{
		mthing->args[0] = mthing->type - 14100;
		mthing->type = 14165;
	}
	// find which type to spawn
	i = DoomEdMap.FindType (mthing->type);

	if (i == NULL)
	{
		// [RH] Don't die if the map tries to spawn an unknown thing
		Printf ("Unknown type %i at (%i, %i)\n",
				 mthing->type,
				 mthing->x>>FRACBITS, mthing->y>>FRACBITS);
		i = PClass::FindClass("Unknown");
	}
	// [RH] If the thing's corresponding sprite has no frames, also map
	//		it to the unknown thing.
	else
	{
		// Handle decorate replacements explicitly here
		// to check for missing frames in the replacement object.
		i = i->GetReplacement();

		const AActor *defaults = GetDefaultByType (i);
		if (defaults->SpawnState == NULL ||
			sprites[defaults->SpawnState->sprite].numframes == 0)
		{
			// We don't load mods for shareware games so we'll just ignore
			// missing actors. Heretic needs this since the shareware includes
			// the retail weapons in Deathmatch.
			if (gameinfo.flags & GI_SHAREWARE)
				return NULL;

			Printf ("%s at (%i, %i) has no frames\n",
					i->TypeName.GetChars(), mthing->x>>FRACBITS, mthing->y>>FRACBITS);
			i = PClass::FindClass("Unknown");
		}
	}

	const AActor *info = GetDefaultByType (i);
/*
	// don't spawn keycards and players in deathmatch
	if (deathmatch && info->flags & MF_NOTDMATCH)
		return NULL;
*/
/*
	// [RH] don't spawn extra weapons in coop if so desired
	if (multiplayer && !deathmatch && (dmflags & DF_NO_COOP_WEAPON_SPAWN))
	{
		if (i->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
		{
			if ((mthing->flags & (MTF_DEATHMATCH|MTF_SINGLE)) == MTF_DEATHMATCH)
				return NULL;
		}
	}
*/
/*
	// don't spawn any monsters if -nomonsters
	if (((level.flags2 & LEVEL2_NOMONSTERS) || (dmflags & DF_NO_MONSTERS)) && info->flags3 & MF3_ISMONSTER )
	{
		return NULL;
	}
	
	// [RH] Other things that shouldn't be spawned depending on dmflags
	if (deathmatch || alwaysapplydmflags)
	{
		if (dmflags & DF_NO_HEALTH)
		{
			if (i->IsDescendantOf (RUNTIME_CLASS(AHealth)))
				return NULL;
			if (i->TypeName == NAME_Berserk)
				return NULL;
			if (i->TypeName == NAME_Megasphere)
				return NULL;
		}
		if (dmflags & DF_NO_ITEMS)
		{
//			if (i->IsDescendantOf (RUNTIME_CLASS(AArtifact)))
//				return;
		}
		if (dmflags & DF_NO_ARMOR)
		{
			if (i->IsDescendantOf (RUNTIME_CLASS(AArmor)))
				return NULL;
			if (i->TypeName == NAME_Megasphere)
				return NULL;
		}
	}
*/
	// [BC] If we're a client, there's no need to spawn map things (unless specified).
	if ( NETWORK_InClientMode() && 
		(( info->ulNetworkFlags & NETFL_ALLOWCLIENTSPAWN ) == false ) &&
		(( info->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) == false ))
	{
		return NULL;
	}

	// [BB] The server doesn't spawn CLIENTSIDEONLY actors.
	if ( ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( info->ulNetworkFlags & NETFL_CLIENTSIDEONLY ) )
	{
		return NULL;
	}

	// spawn it
	x = mthing->x;
	y = mthing->y;

	if (info->flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else if (info->flags2 & MF2_SPAWNFLOAT)
		z = FLOATRANDZ;
	else
		z = ONFLOORZ;

	mobj = AActor::StaticSpawn (i, x, y, z, NO_REPLACE, true);

	if (z == ONFLOORZ)
		mobj->z += mthing->z;
	else if (z == ONCEILINGZ)
		mobj->z -= mthing->z;

	mobj->SpawnPoint[0] = mthing->x;
	mobj->SpawnPoint[1] = mthing->y;
	mobj->SpawnPoint[2] = mthing->z;
	mobj->SpawnAngle = mthing->angle;
	mobj->SpawnFlags = mthing->flags;
	if (mthing->gravity < 0) mobj->gravity = -mthing->gravity;
	else if (mthing->gravity > 0) mobj->gravity = FixedMul(mobj->gravity, mthing->gravity);
	else mobj->flags &= ~MF_NOGRAVITY;

	P_FindFloorCeiling(mobj, FFCF_SAMESECTOR | FFCF_ONLY3DFLOORS | FFCF_3DRESTRICT);

	if (!(mobj->flags2 & MF2_ARGSDEFINED))
	{
		// [RH] Set the thing's special
		mobj->special = mthing->special;
		for(int j=0;j<5;j++) mobj->args[j]=mthing->args[j];
	}

	// [BC] Save the thing's special for resetting the map.
	mobj->SavedSpecial = mobj->special;

	// [Dusk] Save args
	for (int i = 0; i < 5; ++i)
		mobj->SavedArgs[i] = mobj->args[i];

	// [RH] Add ThingID to mobj and link it in with the others
	mobj->tid = mthing->thingid;
	mobj->AddToHash ();

	// [Dusk] Save TID for map resets.
	mobj->SavedTID = mobj->tid;

	mobj->PrevAngle = mobj->angle = (DWORD)((mthing->angle * CONST64(0x100000000)) / 360);

	// Check if this actor's mapthing has a conversation defined
	if (mthing->Conversation > 0)
	{
		// Make sure that this does not partially overwrite the default dialogue settings.
		int root = GetConversation(mthing->Conversation);
		if (root != -1)
		{
			mobj->ConversationRoot = root;
			mobj->Conversation = StrifeDialogues[mobj->ConversationRoot];
		}
	}

	// Set various UDMF options
	if (mthing->alpha != -1)
		mobj->alpha = mthing->alpha;
	if (mthing->RenderStyle != STYLE_Count)
		mobj->RenderStyle = (ERenderStyle)mthing->RenderStyle;
	if (mthing->scaleX)
		mobj->scaleX = FixedMul(mthing->scaleX, mobj->scaleX);
	if (mthing->scaleY)
		mobj->scaleY = FixedMul(mthing->scaleY, mobj->scaleY);
	if (mthing->pitch)
		mobj->pitch = ANGLE_1 * mthing->pitch;
	if (mthing->roll)
		mobj->roll = ANGLE_1 * mthing->roll;
	if (mthing->score)
		mobj->Score = mthing->score;
	if (mthing->fillcolor)
		mobj->fillcolor = mthing->fillcolor;

	mobj->BeginPlay ();
	if (!(mobj->ObjectFlags & OF_EuthanizeMe))
	{
		mobj->LevelSpawned ();
	}

	if (mthing->health > 0)
		mobj->health *= mthing->health;
	else
		mobj->health = -mthing->health;
	if (mthing->health == 0)
		mobj->Die(NULL, NULL);
	else if (mthing->health != 1)
		mobj->StartHealth = mobj->health;
  
	// [BB] Potentially adjust the default flags of this actor.
	GAMEMODE_AdjustActorSpawnFlags ( mobj );

	return mobj;
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
//

// [BC] Added bTellClientToSpawn.
AActor *P_SpawnPuff (AActor *source, const PClass *pufftype, fixed_t x, fixed_t y, fixed_t z, angle_t dir, int updown, int flags, AActor *vict, bool bTellClientToSpawn)
{
	// [CK] If we're a client in this function and we're supposed to be a server
	// telling clients to spawn it, then we will get information later from the
	// server.
	if ( NETWORK_InClientMode() && CLIENT_ShouldPredictPuffs( ) == false )
		return NULL;

	// [CK] The client also should not be doing this puff prediction if it's not
	// for themselves.
	if ( NETWORK_InClientMode() )
	{
		// If these aren't valid or the player is not the console player, don't 
		// predict anything.
		if ( source == NULL || source->player == NULL || source->player - players != consoleplayer )
			return NULL;

		// We want to see if the actor would have NONETID on the actor without
		// spawning it. Therefore we will get the type, and check the flag here.
		AActor *pPuffActor = GetDefaultByType( pufftype );
		if ( pPuffActor == NULL || ( ( pPuffActor->ulNetworkFlags & NETFL_NONETID ) == 0 ) )
			return NULL;
	}

	AActor *puff;
	// [BB] The whole "puff spawning on clients" is pretty awful right now,
	// but currently I don't see how to fix it without increasing net traffic
	// and just made some fixes to make it less buggy.
	// [BC]
	ULONG	ulState = STATE_SPAWN;
	
	if (!(flags & PF_NORANDOMZ))
		z += pr_spawnpuff.Random2 () << 10;

	puff = Spawn (pufftype, x, y, z, ALLOW_REPLACE);
	if (puff == NULL) return NULL;

	// [CK] The puff has been made if we're a client, so any client prediction 
	// of puffs is done, meaning we can exit now.
	if ( NETWORK_InClientMode() )
		return NULL;

	//Moved puff creation and target/master/tracer setting to here. 
	if (puff && vict)
	{
		if (puff->flags7 & MF7_HITTARGET)	puff->target = vict;
		if (puff->flags7 & MF7_HITMASTER)	puff->master = vict;
		if (puff->flags7 & MF7_HITTRACER)	puff->tracer = vict;
	}
	// [BB] If the puff came from a player, set the target of the puff to this player.
	if ( puff && (puff->flags5 & MF5_PUFFGETSOWNER))
		puff->target = source;
	

	if (source != NULL) puff->angle = R_PointToAngle2(x, y, source->x, source->y);

	// [BB] If the clients don't spawn it, make sure it doesn't have a netID.
	if ( bTellClientToSpawn == false )
		puff->FreeNetID();

	// If a puff has a crash state and an actor was not hit,
	// it will enter the crash state. This is used by the StrifeSpark
	// and BlasterPuff.
	FState *crashstate;
	if (!(flags & PF_HITTHING) && (crashstate = puff->FindState(NAME_Crash)) != NULL)
	{
		// [BB] The server needs to tell the state to the clients later.
		ulState = STATE_CRASH;
		// [BB] When spawning the puff, the server won't tell the clients the
		// netID. This needs special handling, otherwise sound won't work.
		// Freeing the ID needs to be done before setting the state!
		puff->FreeNetID();

		puff->SetState (crashstate);
	}
	else if ((flags & PF_HITTHINGBLEED) && (crashstate = puff->FindState(NAME_Death, NAME_Extreme, true)) != NULL)
	{
		puff->SetState (crashstate);
	}
	else if ((flags & PF_MELEERANGE) && puff->MeleeState != NULL)
	{
		// [BB] The server needs to tell the state to the clients later.
		ulState = STATE_MELEE;
		// [BB] When spawning the puff, the server won't tell the clients the
		// netID. This needs special handling, otherwise sound won't work.
		// Freeing the ID needs to be done before setting the state!
		puff->FreeNetID();

		// handle the hard coded state jump of Doom's bullet puff
		// in a more flexible manner.
		puff->SetState (puff->MeleeState);
	}

	// [BC] If we're the server, tell clients to spawn the thing.
	if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
		( bTellClientToSpawn ))
	{
		// If it's translated, or spawning in a state other than its spawn state,
		// treat it as a special case.
		if ( ulState != STATE_SPAWN )
		{
			SERVERCOMMANDS_SpawnPuffNoNetID( puff, ulState, false );
		}
		// In certain other conditions, we need to spawn the puff with a network
		// ID so that things like sounds work.
		else if (( (flags & PF_HITTHING) && puff->SeeSound ) ||
				 ( puff->AttackSound ) || ( ( puff->GetClass()->Meta.GetMetaString (AMETA_Obituary) != NULL ) && ( flags & PF_TEMPORARY ) ) )
		{
			if ( puff->lNetID == -1 )
			{
				puff->lNetID = g_NetIDList.getNewID( );
				g_NetIDList.useID ( puff->lNetID , puff );
			}

			SERVERCOMMANDS_SpawnPuff( puff );
		}
		else
		{
			// [CK] If a player is the source that is firing this, check and see
			// if the attacker is predicting and exclude them (and only them)
			// from getting the predicted puff if the bullet puff has +NONETID.
			if ( source && source->player )
			{
				// [CK] Only spawn for players who are not predicting puffs.
				ULONG activatorPlayerNumber = source->player - players;
				for ( ULONG ulPlayer = 0; ulPlayer < MAXPLAYERS; ulPlayer++ )
				{
					// [CK] Don't send to invalid clients
					if ( SERVER_IsValidClient( ulPlayer ) == false )
						continue;

					// [CK] If the player is the source, and the player fired a
					// puff with +NONETID, and the player wants to predict puffs
					// then we won't send them this command.
					if ( ( activatorPlayerNumber == ulPlayer ) && ( puff->ulNetworkFlags & NETFL_NONETID ) && ( source->player->userinfo.GetClientFlags() & CLIENTFLAGS_CLIENTSIDEPUFFS ) )
						continue;

					// [BB] If the puff has a net ID, it must be spawned with one.
					SERVERCOMMANDS_SpawnPuff( puff, ulPlayer, SVCF_ONLYTHISCLIENT );
				}
			}
			else
			{
				// [CK] It is always sent when fired from a non-player.
				// [BB] If the puff has a net ID, it must be spawned with one.
				SERVERCOMMANDS_SpawnPuff( puff );
			}
		}
	}

	if (!(flags & PF_TEMPORARY))
	{
		if (cl_pufftype && updown != 3 && (puff->flags4 & MF4_ALLOWPARTICLES))
		{
			P_DrawSplash2 (32, x, y, z, dir, updown, 1);
			puff->renderflags |= RF_INVISIBLE;
		}

		if ((flags & PF_HITTHING) && puff->SeeSound)
		{ // Hit thing sound
			S_Sound (puff, CHAN_BODY, puff->SeeSound, 1, ATTN_NORM, bTellClientToSpawn );	// [BC] Inform the clients.
		}
		else if (puff->AttackSound)
		{
			S_Sound (puff, CHAN_BODY, puff->AttackSound, 1, ATTN_NORM, bTellClientToSpawn );	// [BC] Inform the clients.
		}
	}

	return puff;
}



//---------------------------------------------------------------------------
//
// P_SpawnBlood
// 
//---------------------------------------------------------------------------

void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage, AActor *originator)
{
	// [BB] Initialize with NULL.
	AActor *th = NULL;
	PalEntry bloodcolor = originator->GetBloodColor();
	const PClass *bloodcls = originator->GetBloodType();
	
	int bloodtype = cl_bloodtype;
	
	if (bloodcls != NULL && !(GetDefaultByType(bloodcls)->flags4 & MF4_ALLOWPARTICLES))
		bloodtype = 0;

	if (bloodcls != NULL)
	{
		z += pr_spawnblood.Random2 () << 10;
		th = Spawn (bloodcls, x, y, z, NO_REPLACE); // GetBloodType already performed the replacement
		th->velz = FRACUNIT*2;
		th->angle = dir;
		// [NG] Applying PUFFGETSOWNER to the blood will make it target the owner
		if (th->flags5 & MF5_PUFFGETSOWNER) th->target = originator;
		if (gameinfo.gametype & GAME_DoomChex)
		{
			th->tics -= pr_spawnblood() & 3;

			if (th->tics < 1)
				th->tics = 1;
		}
		// colorize the blood
		if (bloodcolor != 0 && !(th->flags2 & MF2_DONTTRANSLATE))
		{
			th->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
		}
		
		// Moved out of the blood actor so that replacing blood is easier
		if (gameinfo.gametype & GAME_DoomStrifeChex)
		{
			if (gameinfo.gametype == GAME_Strife)
			{
				if (damage > 13)
				{
					FState *state = th->FindState(NAME_Spray);
					if (state != NULL)
					{
						th->SetState (state);
						goto statedone;
					}
				}
				else damage += 2;
			}
			int advance = 0;
			if (damage <= 12 && damage >= 9)
			{
				advance = 1;
			}
			else if (damage < 9)
			{
				advance = 2;
			}

			PClass *cls = th->GetClass();

			while (cls != RUNTIME_CLASS(AActor))
			{
				FActorInfo *ai = cls->ActorInfo;
				int checked_advance = advance;
				if (ai->OwnsState(th->SpawnState))
				{
					for (; checked_advance > 0; --checked_advance)
					{
						// [RH] Do not set to a state we do not own.
						if (ai->OwnsState(th->SpawnState + checked_advance))
						{
							th->SetState(th->SpawnState + checked_advance);
							goto statedone;
						}
					}
				}
				cls = cls->ParentClass;
			}
		}
	}

statedone:

	// [BC] If we're the server, tell clients to spawn the blood.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
	{
		// [BB] If the bloodcolor is not the standard one, we have to inform the client 
		// about the correct color.
		if ( ( bloodcolor == 0 ) && ( th != NULL ) )
		{
			// [BC] It's not translated, nor is it spawning in a state other than its
			// spawn state. Therefore, there's no need to treat it as a special case.
			// [BB] This saves bandwidth, but doesn't spawn the blood size based on the damage dealt,
			// so only use this for players with a slow connection.
			SERVERCOMMANDS_SpawnThingNoNetID( th, MAXPLAYERS, SVCF_ONLY_CONNECTIONTYPE_0 );
			SERVERCOMMANDS_SpawnBlood( x, y, z, dir, damage, originator, MAXPLAYERS, SVCF_ONLY_CONNECTIONTYPE_1 );
		}
		else
			SERVERCOMMANDS_SpawnBlood( x, y, z, dir, damage, originator );
	}

	if (!(bloodtype <= 1)) th->renderflags |= RF_INVISIBLE;
	if (bloodtype >= 1)
		P_DrawSplash2 (40, x, y, z, dir, 2, bloodcolor);
}

//---------------------------------------------------------------------------
//
// PROC P_BloodSplatter
//
//---------------------------------------------------------------------------

void P_BloodSplatter (fixed_t x, fixed_t y, fixed_t z, AActor *originator)
{
	PalEntry bloodcolor = originator->GetBloodColor();
	const PClass *bloodcls = originator->GetBloodType(1); 

	int bloodtype = cl_bloodtype;
	
	if (bloodcls != NULL && !(GetDefaultByType(bloodcls)->flags4 & MF4_ALLOWPARTICLES))
		bloodtype = 0;

	if (bloodcls != NULL)
	{
		AActor *mo;

		mo = Spawn(bloodcls, x, y, z, NO_REPLACE); // GetBloodType already performed the replacement
		mo->target = originator;
		mo->velx = pr_splatter.Random2 () << 10;
		mo->vely = pr_splatter.Random2 () << 10;
		mo->velz = 3*FRACUNIT;

		// [BB] This is not fully synced, but handled with SERVERCOMMANDS_SpawnBloodSplatter.
		// In order to prevent the server from printing warnings when the server calls P_ExplodeMissile,
		// we also mark this as SERVERSIDEONLY.
		if ( NETWORK_GetState () == NETSTATE_SERVER )
		{
			mo->ulNetworkFlags |= NETFL_SERVERSIDEONLY;
			mo->FreeNetID ();
		}

		// colorize the blood!
		if (bloodcolor!=0 && !(mo->flags2 & MF2_DONTTRANSLATE)) 
		{
			mo->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
		}

		if (!(bloodtype <= 1)) mo->renderflags |= RF_INVISIBLE;
	}
	if (bloodtype >= 1)
	{
		P_DrawSplash2 (40, x, y, z, R_PointToAngle2 (x, y, originator->x, originator->y), 2, bloodcolor);
	}

	// [BB] Tell the clients to spawn the splatter.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnBloodSplatter( x, y, z, originator, false );
}

//===========================================================================
//
//  P_BloodSplatter2
//
//===========================================================================

void P_BloodSplatter2 (fixed_t x, fixed_t y, fixed_t z, AActor *originator)
{
	PalEntry bloodcolor = originator->GetBloodColor();
	const PClass *bloodcls = originator->GetBloodType(2);

	int bloodtype = cl_bloodtype;
	
	if (bloodcls != NULL && !(GetDefaultByType(bloodcls)->flags4 & MF4_ALLOWPARTICLES))
		bloodtype = 0;

	if (bloodcls != NULL)
	{
		AActor *mo;
		
		x += ((pr_splat()-128)<<11);
		y += ((pr_splat()-128)<<11);

		mo = Spawn (bloodcls, x, y, z, NO_REPLACE); // GetBloodType already performed the replacement
		mo->target = originator;

		// colorize the blood!
		if (bloodcolor != 0 && !(mo->flags2 & MF2_DONTTRANSLATE))
		{
			mo->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
		}

		if (!(bloodtype <= 1)) mo->renderflags |= RF_INVISIBLE;
	}
	if (bloodtype >= 1)
	{
		P_DrawSplash2 (100, x, y, z, R_PointToAngle2 (0, 0, originator->x - x, originator->y - y), 2, bloodcolor);
	}

	// [BB] Tell the clients to spawn the splatter.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SpawnBloodSplatter( x, y, z, originator, true );
}

//---------------------------------------------------------------------------
//
// PROC P_RipperBlood
//
//---------------------------------------------------------------------------

void P_RipperBlood (AActor *mo, AActor *bleeder)
{
	fixed_t x, y, z;
	PalEntry bloodcolor = bleeder->GetBloodColor();
	const PClass *bloodcls = bleeder->GetBloodType();

	x = mo->x + (pr_ripperblood.Random2 () << 12);
	y = mo->y + (pr_ripperblood.Random2 () << 12);
	z = mo->z + (pr_ripperblood.Random2 () << 12);

	int bloodtype = cl_bloodtype;
	
	if (bloodcls != NULL && !(GetDefaultByType(bloodcls)->flags4 & MF4_ALLOWPARTICLES))
		bloodtype = 0;

	if (bloodcls != NULL)
	{
		AActor *th;
		th = Spawn (bloodcls, x, y, z, NO_REPLACE); // GetBloodType already performed the replacement
		// [NG] Applying PUFFGETSOWNER to the blood will make it target the owner
		if (th->flags5 & MF5_PUFFGETSOWNER) th->target = bleeder;
		if (gameinfo.gametype == GAME_Heretic)
			th->flags |= MF_NOGRAVITY;
		th->velx = mo->velx >> 1;
		th->vely = mo->vely >> 1;
		th->tics += pr_ripperblood () & 3;

		// colorize the blood!
		if (bloodcolor!=0 && !(th->flags2 & MF2_DONTTRANSLATE))
		{
			th->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
		}

		if (!(bloodtype <= 1)) th->renderflags |= RF_INVISIBLE;
	}
	if (bloodtype >= 1)
	{
		P_DrawSplash2 (28, x, y, z, 0, 0, bloodcolor);
	}
}

//---------------------------------------------------------------------------
//
// FUNC P_GetThingFloorType
//
//---------------------------------------------------------------------------

int P_GetThingFloorType (AActor *thing)
{
	if (thing->floorpic.isValid())
	{		
		return TerrainTypes[thing->floorpic];
	}
	else
	{
		return TerrainTypes[thing->Sector->GetTexture(sector_t::floor)];
	}
}

//---------------------------------------------------------------------------
//
// FUNC P_HitWater
//
// Returns true if hit liquid and splashed, false if not.
//---------------------------------------------------------------------------

bool P_HitWater (AActor * thing, sector_t * sec, fixed_t x, fixed_t y, fixed_t z, bool checkabove, bool alert)
{
	if (thing->flags3 & MF3_DONTSPLASH)
		return false;

	// [BC] Spectators can't cause splashes.
	if (( thing->player ) &&
		( thing->player->bSpectating ))
	{
		return ( false );
	}

	// [BC] Let the server handle splashes.
	if ( NETWORK_InClientMode() )
	{
		return ( false );
	}
/*
	if (thing->player && (thing->player->cheats & CF_PREDICTING))
		return false;
*/
	AActor *mo = NULL;
	FSplashDef *splash;
	int terrainnum;
	sector_t *hsec = NULL;
	
	if (x == FIXED_MIN) x = thing->x;
	if (y == FIXED_MIN) y = thing->y;
	if (z == FIXED_MIN) z = thing->z;
	// don't splash above the object
	if (checkabove)
	{
		fixed_t compare_z = thing->z + (thing->height >> 1);
		// Missiles are typically small and fast, so they might
		// end up submerged by the move that calls P_HitWater.
		if (thing->flags & MF_MISSILE)
			compare_z -= thing->velz;
		if (z > compare_z) 
			return false;
	}

#if 0 // needs some rethinking before activation

	// This avoids spawning splashes on invisible self referencing sectors.
	// For network consistency do this only in single player though because
	// it is not guaranteed that all players have GL nodes loaded.
	if (!multiplayer && thing->subsector->sector != thing->subsector->render_sector)
	{
		fixed_t zs = thing->subsector->sector->floorplane.ZatPoint(x, y);
		fixed_t zr = thing->subsector->render_sector->floorplane.ZatPoint(x, y);

		if (zs > zr && thing->z >= zs) return false;
	}
#endif

#ifdef _3DFLOORS
	for(unsigned int i=0;i<sec->e->XFloor.ffloors.Size();i++)
	{		
		F3DFloor * rover = sec->e->XFloor.ffloors[i];
		if (!(rover->flags & FF_EXISTS)) continue;
		fixed_t planez = rover->top.plane->ZatPoint(x, y);
		if (z > planez - FRACUNIT/2 && z < planez + FRACUNIT/2)	// allow minor imprecisions
		{
			if (rover->flags & (FF_SOLID|FF_SWIMMABLE) )
			{
				terrainnum = TerrainTypes[*rover->top.texture];
				goto foundone;
			}
		}
		planez = rover->bottom.plane->ZatPoint(x, y);
		if (planez < z && !(planez < thing->floorz)) return false;
	}
#endif
	hsec = sec->GetHeightSec();
	if (hsec == NULL || !(hsec->MoreFlags & SECF_CLIPFAKEPLANES))
	{
		terrainnum = TerrainTypes[sec->GetTexture(sector_t::floor)];
	}
	else
	{
		terrainnum = TerrainTypes[hsec->GetTexture(sector_t::floor)];
	}
#ifdef _3DFLOORS
foundone:
#endif

	int splashnum = Terrains[terrainnum].Splash;
	bool smallsplash = false;
	const secplane_t *plane;

	if (splashnum == -1)
		return Terrains[terrainnum].IsLiquid;

	// don't splash when touching an underwater floor
	if (thing->waterlevel>=1 && z<=thing->floorz) return Terrains[terrainnum].IsLiquid;

	plane = hsec != NULL? &sec->heightsec->floorplane : &sec->floorplane;

	// Don't splash for living things with small vertical velocities.
	// There are levels where the constant splashing from the monsters gets extremely annoying
	if ((thing->flags3&MF3_ISMONSTER || thing->player) && thing->velz >= -6*FRACUNIT)
		return Terrains[terrainnum].IsLiquid;

	splash = &Splashes[splashnum];

	// Small splash for small masses
	if (thing->Mass < 10)
		smallsplash = true;

	if (smallsplash && splash->SmallSplash)
	{
		mo = Spawn (splash->SmallSplash, x, y, z, ALLOW_REPLACE);
		if (mo) mo->floorclip += splash->SmallSplashClip;

		// [BC/BB] Tell clients to spawn the splash.
		if ( mo && ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
			SERVERCOMMANDS_SpawnThing( mo );
	}
	else
	{
		if (splash->SplashChunk)
		{
			mo = Spawn (splash->SplashChunk, x, y, z, ALLOW_REPLACE);
			mo->target = thing;
			if (splash->ChunkXVelShift != 255)
			{
				mo->velx = pr_chunk.Random2() << splash->ChunkXVelShift;
			}
			if (splash->ChunkYVelShift != 255)
			{
				mo->vely = pr_chunk.Random2() << splash->ChunkYVelShift;
			}
			mo->velz = splash->ChunkBaseZVel + (pr_chunk() << splash->ChunkZVelShift);

			// [BC/BB] Tell clients to spawn the splash.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				SERVERCOMMANDS_SpawnThing( mo );
				SERVERCOMMANDS_MoveThing( mo, CM_VELX|CM_VELY|CM_VELZ );
			}
		}
		if (splash->SplashBase)
		{
			mo = Spawn (splash->SplashBase, x, y, z, ALLOW_REPLACE);

			// [BC/BB] Tell clients to spawn the splash.
			if ( mo && ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
				SERVERCOMMANDS_SpawnThing( mo );
		}
		if (thing->player && !splash->NoAlert && alert)
		{
			P_NoiseAlert (thing, thing, true);
		}
	}
	if (mo)
	{
		S_Sound (mo, CHAN_ITEM, smallsplash ?
			splash->SmallSplashSound : splash->NormalSplashSound,
			1, ATTN_IDLE, true );	// [BC] Inform the clients.
	}
	else
	{
		S_Sound (x, y, z, CHAN_ITEM, smallsplash ?
			splash->SmallSplashSound : splash->NormalSplashSound,
			1, ATTN_IDLE);

		// [BC] Tell clients to play the sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SoundPoint( thing->x, thing->y, thing->z, CHAN_ITEM, smallsplash ? S_GetName( splash->SmallSplashSound ) : S_GetName( splash->NormalSplashSound ), 1, ATTN_IDLE );
	}

	// Don't let deep water eat missiles
	return plane == &sec->floorplane ? Terrains[terrainnum].IsLiquid : false;
}

//---------------------------------------------------------------------------
//
// FUNC P_HitFloor
//
// Returns true if hit liquid and splashed, false if not.
//---------------------------------------------------------------------------

bool P_HitFloor (AActor *thing)
{
	const msecnode_t *m;

	// killough 11/98: touchy objects explode on impact
	// Allow very short drops to be safe, so that a touchy can be summoned without exploding.
	if (thing->flags6 & MF6_TOUCHY && ((thing->flags6 & MF6_ARMED) || thing->IsSentient()) && ((thing->velz) < (-5 * FRACUNIT)))
	{
		thing->flags6 &= ~MF6_ARMED; // Disarm
		P_DamageMobj (thing, NULL, NULL, thing->health, NAME_Crush, DMG_FORCED);  // kill object
		return false;
	}

	if (thing->flags3 & MF3_DONTSPLASH)
		return false;

	// don't splash if landing on the edge above water/lava/etc....
	for (m = thing->touching_sectorlist; m; m = m->m_tnext)
	{
		if (thing->z == m->m_sector->floorplane.ZatPoint (thing->x, thing->y))
		{
			break;
		}

#ifdef _3DFLOORS
		// Check 3D floors
		for(unsigned int i=0;i<m->m_sector->e->XFloor.ffloors.Size();i++)
		{		
			F3DFloor * rover = m->m_sector->e->XFloor.ffloors[i];
			if (!(rover->flags & FF_EXISTS)) continue;
			if (rover->flags & (FF_SOLID|FF_SWIMMABLE))
			{
				if (rover->top.plane->ZatPoint(thing->x, thing->y) == thing->z)
				{
					return P_HitWater (thing, m->m_sector);
				}
			}
		}
#endif
	}
	if (m == NULL || m->m_sector->GetHeightSec() != NULL)
	{ 
		return false;
	}

	return P_HitWater (thing, m->m_sector);
}

//---------------------------------------------------------------------------
//
// P_CheckSplash
//
// Checks for splashes caused by explosions
//
//---------------------------------------------------------------------------

void P_CheckSplash(AActor *self, fixed_t distance)
{
	if (self->z <= self->floorz + (distance<<FRACBITS) && self->floorsector == self->Sector && self->Sector->GetHeightSec() == NULL)
	{
		// Explosion splashes never alert monsters. This is because A_Explode has
		// a separate parameter for that so this would get in the way of proper 
		// behavior.
		P_HitWater (self, self->Sector, self->x, self->y, self->floorz, false, false);
	}
}

//---------------------------------------------------------------------------
//
// FUNC P_CheckMissileSpawn
//
// Returns true if the missile is at a valid spawn point, otherwise
// explodes it and returns false.
//
//---------------------------------------------------------------------------
// [WS] Added bExplode.
bool P_CheckMissileSpawn (AActor* th, fixed_t maxdist, bool bExplode)
{
	// [RH] Don't decrement tics if they are already less than 1
	if ((th->flags4 & MF4_RANDOMIZE) && th->tics > 0)
	{
		th->tics -= pr_checkmissilespawn() & 3;
		if (th->tics < 1)
			th->tics = 1;
	}

	if (maxdist > 0)
	{
		// move a little forward so an angle can be computed if it immediately explodes
		TVector3<double> advance(FIXED2DBL(th->velx), FIXED2DBL(th->vely), FIXED2DBL(th->velz));
		double maxsquared = FIXED2DBL(maxdist);
		maxsquared *= maxsquared;

		// Keep halving the advance vector until we get something less than maxdist
		// units away, since we still want to spawn the missile inside the shooter.
		do
		{
			advance *= 0.5f;
		}
		while (TVector2<double>(advance).LengthSquared() >= maxsquared);
		th->x += FLOAT2FIXED(advance.X);
		th->y += FLOAT2FIXED(advance.Y);
		th->z += FLOAT2FIXED(advance.Z);
	}

	FCheckPosition tm(!!(th->flags2 & MF2_RIP));

	// killough 8/12/98: for non-missile objects (e.g. grenades)
	// 
	// [GZ] MBF excludes non-missile objects from the P_TryMove test
	// and subsequent potential P_ExplodeMissile call. That is because
	// in MBF, a projectile is not an actor with the MF_MISSILE flag
	// but an actor with either or both the MF_MISSILE and MF_BOUNCES
	// flags, and a grenade is identified by not having MF_MISSILE.
	// Killough wanted grenades not to explode directly when spawned,
	// therefore they can be fired safely even when humping a wall as
	// they will then just drop on the floor at their shooter's feet.
	//
	// However, ZDoom does allow non-missiles to be shot as well, so
	// Killough's check for non-missiles is inadequate here. So let's
	// replace it by a check for non-missile and MBF bounce type.
	// This should allow MBF behavior where relevant without altering
	// established ZDoom behavior for crazy stuff like a cacodemon cannon.
	bool MBFGrenade = (!(th->flags & MF_MISSILE) || (th->BounceFlags & BOUNCE_MBF));

	// killough 3/15/98: no dropoff (really = don't care for missiles)
	if (!(P_TryMove (th, th->x, th->y, false, NULL, tm, true)))
	{
		// [RH] Don't explode ripping missiles that spawn inside something
		if (th->BlockingMobj == NULL || !(th->flags2 & MF2_RIP) || (th->BlockingMobj->flags5 & MF5_DONTRIP))
		{
			// If this is a monster spawned by A_CustomMissile subtract it from the counter.
			th->ClearCounters();
			// [RH] Don't explode missiles that spawn on top of horizon lines
			if (th->BlockingLine != NULL && th->BlockingLine->special == Line_Horizon)
			{
				th->Destroy ();
			}
			else if (MBFGrenade && th->BlockingLine != NULL)
			{
				P_BounceWall(th);
			}
			else
			{
				// Potentially reward the player who shot this missile with an accuracy/precision medal.
				if ((( th->ulSTFlags & STFL_EXPLODEONDEATH ) == false ) && th->target && th->target->player )
				{
					if ( th->target->player->bStruckPlayer )
						PLAYER_StruckPlayer( th->target->player );
					else
						th->target->player->ulConsecutiveHits = 0;
				}
				// [WS] Can we explode the missile?
				if (bExplode)
					P_ExplodeMissile (th, NULL, th->BlockingMobj);
			}
			return false;
		}
	}
	return true;
}


//---------------------------------------------------------------------------
//
// FUNC P_PlaySpawnSound
//
// Plays a missiles spawn sound. Location depends on the
// MF_SPAWNSOUNDSOURCE flag.
//
//---------------------------------------------------------------------------

void P_PlaySpawnSound(AActor *missile, AActor *spawner)
{
	if (missile->SeeSound != 0)
	{
		if (!(missile->flags & MF_SPAWNSOUNDSOURCE))
		{
			S_Sound (missile, CHAN_VOICE, missile->SeeSound, 1, ATTN_NORM);
		}
		else if (spawner != NULL)
		{
			S_Sound (spawner, CHAN_WEAPON, missile->SeeSound, 1, ATTN_NORM);
		}
		else
		{
			// If there is no spawner use the spawn position.
			// But not in a silenced sector.
			if (!(missile->Sector->Flags & SECF_SILENT))
				S_Sound (missile->x, missile->y, missile->z, CHAN_WEAPON, missile->SeeSound, 1, ATTN_NORM);
		}
	}
}

static fixed_t GetDefaultSpeed(const PClass *type)
{
	if (type == NULL) return 0;
	else if (G_SkillProperty(SKILLP_FastMonsters))
		return type->Meta.GetMetaFixed(AMETA_FastSpeed, GetDefaultByType(type)->Speed);
	else
		return GetDefaultByType(type)->Speed;
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissile
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

AActor *P_SpawnMissile (AActor *source, AActor *dest, const PClass *type, AActor *owner, const bool bSpawnOnClient ) // [BB] Added bSpawnOnClient.
{
	return P_SpawnMissileXYZ (source->x, source->y, source->z + 32*FRACUNIT + source->GetBobOffset(),
		source, dest, type, true, owner, bSpawnOnClient); // [BB] Added bSpawnOnClient.
}

AActor *P_SpawnMissileZ (AActor *source, fixed_t z, AActor *dest, const PClass *type, const bool bSpawnOnClient) // [BB] Added bSpawnOnClient.
{
	return P_SpawnMissileXYZ (source->x, source->y, z, source, dest, type,
		true, NULL, bSpawnOnClient ); // [BB] Added bSpawnOnClient.
}

AActor *P_SpawnMissileXYZ (fixed_t x, fixed_t y, fixed_t z,
	AActor *source, AActor *dest, const PClass *type, bool checkspawn, AActor *owner, const bool bSpawnOnClient ) // [BB] Added bSpawnOnClient.
{
	if (dest == NULL)
	{
		Printf ("P_SpawnMissilyXYZ: Tried to shoot %s from %s with no dest\n",
			type->TypeName.GetChars(), source->GetClass()->TypeName.GetChars());
		return NULL;
	}

	if (z != ONFLOORZ && z != ONCEILINGZ) 
	{
		z -= source->floorclip;
	}

	AActor *th = Spawn (type, x, y, z, ALLOW_REPLACE);
	
	P_PlaySpawnSound(th, source);

	// record missile's originator
	if (owner == NULL) owner = source;
	th->target = owner;

	float speed = (float)(th->Speed);

	// [RH]
	// Hexen calculates the missile velocity based on the source's location.
	// Would it be more useful to base it on the actual position of the
	// missile?
	// Answer: No, because this way, you can set up sets of parallel missiles.

	FVector3 velocity(dest->x - source->x, dest->y - source->y, dest->z - source->z);
	// Floor and ceiling huggers should never have a vertical component to their velocity
	if (th->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER))
	{
		velocity.Z = 0;
	}
	// [RH] Adjust the trajectory if the missile will go over the target's head.
	else if (z - source->z >= dest->height)
	{
		velocity.Z += dest->height - z + source->z;
	}
	velocity.Resize (speed);
	th->velx = (fixed_t)(velocity.X);
	th->vely = (fixed_t)(velocity.Y);
	th->velz = (fixed_t)(velocity.Z);

	// invisible target: rotate velocity vector in 2D
	// [RC] Now monsters can aim at invisible player as if they were fully visible.
	if (dest->flags & MF_SHADOW && !(source->flags6 & MF6_SEEINVISIBLE))
	{
		angle_t an = pr_spawnmissile.Random2 () << 20;
		an >>= ANGLETOFINESHIFT;
		
		fixed_t newx = DMulScale16 (th->velx, finecosine[an], -th->vely, finesine[an]);
		fixed_t newy = DMulScale16 (th->velx, finesine[an], th->vely, finecosine[an]);
		th->velx = newx;
		th->vely = newy;
	}

	th->angle = R_PointToAngle2 (0, 0, th->velx, th->vely);

	if (th->flags4 & MF4_SPECTRAL)
	{
		th->SetFriendPlayer(owner->player);
	}

	// [BB]
	AActor *pMissile = (!checkspawn || P_CheckMissileSpawn (th, source->radius)) ? th : NULL;

	// [BB] If we're the server, tell clients to spawn the missile.
	if ( bSpawnOnClient && ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );

	return pMissile;
}

AActor * P_OldSpawnMissile(AActor * source, AActor * owner, AActor * dest, const PClass *type)
{
	angle_t an;
	fixed_t dist;
	AActor *th = Spawn (type, source->x, source->y, source->z + 4*8*FRACUNIT, ALLOW_REPLACE);

	P_PlaySpawnSound(th, source);
	th->target = owner;		// record missile's originator

	th->angle = an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);
	an >>= ANGLETOFINESHIFT;
	th->velx = FixedMul (th->Speed, finecosine[an]);
	th->vely = FixedMul (th->Speed, finesine[an]);

	dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
	if (th->Speed) dist = dist / th->Speed;

	if (dist < 1)
		dist = 1;

	th->velz = (dest->z - source->z) / dist;

	if (th->flags4 & MF4_SPECTRAL)
	{
		th->SetFriendPlayer(owner->player);
	}

	P_CheckMissileSpawn(th, source->radius);
	return th;
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissileAngle
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

AActor *P_SpawnMissileAngle (AActor *source, const PClass *type,
	angle_t angle, fixed_t velz, bool bSpawnOnClient) // [BB] Added bSpawnOnClient.
{
	return P_SpawnMissileAngleZSpeed (source, source->z + 32*FRACUNIT + source->GetBobOffset(),
		type, angle, velz, GetDefaultSpeed (type),
		NULL, true, bSpawnOnClient ); // [BB] Added bSpawnOnClient.
}

AActor *P_SpawnMissileAngleZ (AActor *source, fixed_t z,
	const PClass *type, angle_t angle, fixed_t velz, bool bSpawnOnClient) // [BB] Added bSpawnOnClient.
{
	return P_SpawnMissileAngleZSpeed (source, z, type, angle, velz,
		GetDefaultSpeed (type),
		NULL, true, bSpawnOnClient ); // [BB] Added bSpawnOnClient.
}

AActor *P_SpawnMissileZAimed (AActor *source, fixed_t z, AActor *dest, const PClass *type, bool bSpawnOnClient ) // [BB] Added bSpawnOnClient.
{
	angle_t an;
	fixed_t dist;
	fixed_t speed;
	fixed_t velz;

	an = source->angle;

	if (dest->flags & MF_SHADOW)
	{
		an += pr_spawnmissile.Random2() << 20;
	}
	dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
	speed = GetDefaultSpeed (type);
	dist /= speed;
	velz = dist != 0 ? (dest->z - source->z)/dist : speed;
	return P_SpawnMissileAngleZSpeed (source, z, type, an, velz, speed, NULL, true, bSpawnOnClient ); // [BB] Added bSpawnOnClient.
}

//---------------------------------------------------------------------------
//
// FUNC P_SpawnMissileAngleSpeed
//
// Returns NULL if the missile exploded immediately, otherwise returns
// a mobj_t pointer to the missile.
//
//---------------------------------------------------------------------------

AActor *P_SpawnMissileAngleSpeed (AActor *source, const PClass *type,
	angle_t angle, fixed_t velz, fixed_t speed)
{
	return P_SpawnMissileAngleZSpeed (source, source->z + 32*FRACUNIT + source->GetBobOffset(),
		type, angle, velz, speed);
}

AActor *P_SpawnMissileAngleZSpeed (AActor *source, fixed_t z,
	const PClass *type, angle_t angle, fixed_t velz, fixed_t speed, AActor *owner, bool checkspawn, const bool bSpawnOnClient ) // [BB] Added bSpawnOnClient.
{
	AActor *mo;

	if (z != ONFLOORZ && z != ONCEILINGZ && source != NULL) 
	{
		z -= source->floorclip;
	}

	mo = Spawn (type, source->x, source->y, z, ALLOW_REPLACE);

	P_PlaySpawnSound(mo, source);
	if (owner == NULL) owner = source;
	mo->target = owner;
	mo->angle = angle;
	angle >>= ANGLETOFINESHIFT;
	mo->velx = FixedMul (speed, finecosine[angle]);
	mo->vely = FixedMul (speed, finesine[angle]);
	mo->velz = velz;

	if (mo->flags4 & MF4_SPECTRAL)
	{
		mo->SetFriendPlayer(owner->player);
	}

	// [BB]
	AActor *pMissile = (!checkspawn || P_CheckMissileSpawn(mo, source->radius)) ? mo : NULL;

	// [BB] If we're the server, tell clients to spawn the missile.
	if ( bSpawnOnClient && ( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pMissile ))
		SERVERCOMMANDS_SpawnMissile( pMissile );

	return pMissile;
}

/*
================
=
= P_SpawnPlayerMissile
=
= Tries to aim at a nearby monster
================
*/

AActor *P_SpawnPlayerMissile (AActor *source, const PClass *type)
{
	return P_SpawnPlayerMissile (source, 0, 0, 0, type, source->angle);
}

// [BC/BB] Added bSpawnSound.
AActor *P_SpawnPlayerMissile (AActor *source, const PClass *type, angle_t angle, bool bSpawnSound )
{
	return P_SpawnPlayerMissile (source, 0, 0, 0, type, angle, NULL, NULL, false, bSpawnSound );
}

// [BC/BB] Added bSpawnSound.
// [BB] Added bSpawnOnClient.
AActor *P_SpawnPlayerMissile (AActor *source, fixed_t x, fixed_t y, fixed_t z,
							  const PClass *type, angle_t angle, AActor **pLineTarget, AActor **pMissileActor,
							  bool nofreeaim, bool bSpawnSound, bool bSpawnOnClient)
{
	static const int angdiff[3] = { -1<<26, 1<<26, 0 };
	angle_t an = angle;
	angle_t pitch;
	AActor *linetarget;
	AActor *defaultobject = GetDefaultByType(type);
	int vrange = nofreeaim ? ANGLE_1*35 : 0;

	if (source == NULL)
	{
		return NULL;
	}
	if (source->player && source->player->ReadyWeapon && (source->player->ReadyWeapon->WeaponFlags & WIF_NOAUTOAIM))
	{
		// Keep exactly the same angle and pitch as the player's own aim
		an = angle;
		pitch = source->pitch;
		linetarget = NULL;
	}
	else // see which target is to be aimed at
	{
		// [XA] If MaxTargetRange is defined in the spawned projectile, use this as the
		//      maximum range for the P_AimLineAttack call later; this allows MaxTargetRange
		//      to function as a "maximum tracer-acquisition range" for seeker missiles.
		fixed_t linetargetrange = defaultobject->maxtargetrange > 0 ? defaultobject->maxtargetrange*64 : 16*64*FRACUNIT;

		int i = 2;
		do
		{
			an = angle + angdiff[i];
			pitch = P_AimLineAttack (source, an, linetargetrange, &linetarget, vrange);
	
			if (source->player != NULL &&
				!nofreeaim &&
				level.IsFreelookAllowed() &&
				source->player->userinfo.GetAimDist() <= ANGLE_1/2)
			{
				break;
			}
		} while (linetarget == NULL && --i >= 0);

		if (linetarget == NULL)
		{
			an = angle;
			if (nofreeaim || !level.IsFreelookAllowed())
			{
				pitch = 0;
			}
		}
	}
	if (pLineTarget) *pLineTarget = linetarget;

	if (z != ONFLOORZ && z != ONCEILINGZ)
	{
		// Doom spawns missiles 4 units lower than hitscan attacks for players.
		z += source->z + (source->height>>1) - source->floorclip;
		if (source->player != NULL)	// Considering this is for player missiles, it better not be NULL.
		{
			z += FixedMul (source->player->mo->AttackZOffset - 4*FRACUNIT, source->player->crouchfactor);
		}
		else
		{
			z += 4*FRACUNIT;
		}
		// Do not fire beneath the floor.
		if (z < source->floorz)
		{
			z = source->floorz;
		}
	}
	AActor *MissileActor = Spawn (type, source->x + x, source->y + y, z, ALLOW_REPLACE);
	if (pMissileActor) *pMissileActor = MissileActor;

	if ( bSpawnSound )
	{
		P_PlaySpawnSound(MissileActor, source);
	}
	MissileActor->target = source;
	MissileActor->angle = an;

	fixed_t vx, vy, vz, speed;

	vx = FixedMul (finecosine[pitch>>ANGLETOFINESHIFT], finecosine[an>>ANGLETOFINESHIFT]);
	vy = FixedMul (finecosine[pitch>>ANGLETOFINESHIFT], finesine[an>>ANGLETOFINESHIFT]);
	vz = -finesine[pitch>>ANGLETOFINESHIFT];
	speed = MissileActor->Speed;

	FVector3 vec(vx, vy, vz);

	if (MissileActor->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER))
	{
		vec.Z = 0;
	}
	vec.Resize(speed);
	MissileActor->velx = (fixed_t)vec.X;
	MissileActor->vely = (fixed_t)vec.Y;
	MissileActor->velz = (fixed_t)vec.Z;

	if (MissileActor->flags4 & MF4_SPECTRAL)
	{
		MissileActor->SetFriendPlayer(source->player);
	}
	// [WS] Redid some logic here for ST to handle when we should spawn a missile
	// as well as exploding the missile and informing the clients.
	bool bValidSpawn = P_CheckMissileSpawn (MissileActor, source->radius, false);

	// [BC/BB] Possibly tell clients to spawn this missile.
	// [WS] If we know the missile is going to explode,
	// we need to spawn the missile for the clients.
	if ( ( bSpawnOnClient || !bValidSpawn ) && ( NETWORK_GetState( ) == NETSTATE_SERVER ) )
		SERVERCOMMANDS_SpawnMissileExact( MissileActor );
	
	if (bValidSpawn)
	{
		return MissileActor;
	}
	// [WS] We are handling the explosion of the missile here instead of in P_CheckMissileSpawn.
	P_ExplodeMissile(MissileActor, NULL, MissileActor->BlockingMobj);
	return NULL;
}

bool AActor::IsTeammate (AActor *other)
{
	// [BL] Function is practically rewritten in Skulltag
	if (!other)
		return false;
	// Allow co-op players to be teammates.
	else if ((( deathmatch == false ) && ( teamgame == false )) && player && other->player)
		return ( true );

	// Can't be an enemy of ourselves!
	if ( this == other )
		return ( true );

	// Teamplay deathmatch, CTF, Skulltag, etc.
	if ( GAMEMODE_GetCurrentFlags() & GMF_PLAYERSONTEAMS )
	{
		int myTeam = DesignatedTeam;
		int otherTeam = other->DesignatedTeam;
		if (player)
		{
			if (!player->bOnTeam)
				return ( false );
			myTeam = player->ulTeam;
		}
		if (other->player)
		{
			if (!other->player->bOnTeam)
				return ( false );
			otherTeam = other->player->ulTeam;
		}

		// If they're not on our team...
		if ( myTeam == TEAM_None || myTeam != otherTeam )
			return ( false );

		// Passed checks. Player is on our team.
		return ( true );
	}

	return ( false );
}

//==========================================================================
//
// AActor :: GetSpecies
//
// Species is defined as the lowest base class that is a monster
// with no non-monster class in between. If the actor specifies an explicit
// species (i.e. not 'None'), that is used. This is virtualized, so special
// monsters can change this behavior if they like.
//
//==========================================================================

FName AActor::GetSpecies()
{
	if (Species != NAME_None)
	{
		return Species;
	}

	const PClass *thistype = GetClass();

	if (GetDefaultByType(thistype)->flags3 & MF3_ISMONSTER)
	{
		while (thistype->ParentClass)
		{
			if (GetDefaultByType(thistype->ParentClass)->flags3 & MF3_ISMONSTER)
				thistype = thistype->ParentClass;
			else 
				break;
		}
	}
	return Species = thistype->TypeName; // [GZ] Speeds up future calls.
}

//==========================================================================
//
// AActor :: IsFriend
//
// Checks if two monsters have to be considered friendly.
//
//==========================================================================

bool AActor::IsFriend (AActor *other)
{
	if (flags & other->flags & MF_FRIENDLY)
	{
		// [BL] Some adjsutments to possibly allow team coop.
		if (deathmatch || teamgame)
			return IsTeammate(other) ||
				(FriendPlayer != 0 && other->FriendPlayer != 0 &&
					players[FriendPlayer-1].mo->IsTeammate(players[other->FriendPlayer-1].mo));

		// [BB] Added teamgame
		return !( deathmatch || teamgame ) ||
			FriendPlayer == other->FriendPlayer ||
			FriendPlayer == 0 ||
			other->FriendPlayer == 0 ||
			players[FriendPlayer-1].mo->IsTeammate(players[other->FriendPlayer-1].mo);
	}
	return false;
}

//==========================================================================
//
// AActor :: IsHostile
//
// Checks if two monsters have to be considered hostile under any circumstances
//
//==========================================================================

bool AActor::IsHostile (AActor *other)
{
	// Both monsters are non-friendlies so hostilities depend on infighting settings
	if (!((flags | other->flags) & MF_FRIENDLY)) return false;

	// Both monsters are friendly and belong to the same player if applicable.
	if (flags & other->flags & MF_FRIENDLY)
	{
		// [BL] Some adjsutments to possibly allow team coop.
		if (deathmatch || teamgame)
			return !IsTeammate(other) &&
				!(FriendPlayer != 0 && other->FriendPlayer != 0 &&
					players[FriendPlayer-1].mo->IsTeammate(players[other->FriendPlayer-1].mo));

		return deathmatch &&
			FriendPlayer != other->FriendPlayer &&
			FriendPlayer !=0 &&
			other->FriendPlayer != 0 &&
			!players[FriendPlayer-1].mo->IsTeammate(players[other->FriendPlayer-1].mo);
	}
	return true;
}

int AActor::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->player && target->player->mo == target && damage < 1000 &&
		(target->player->cheats & CF_GODMODE))
	{
		return -1;
	}
	else
	{
		if (target->player)
		{
			// Only do this for old style poison damage.
			if (PoisonDamage > 0 && PoisonDuration == INT_MIN)
			{
				P_PoisonPlayer (target->player, this, this->target, PoisonDamage);
				damage >>= 1;
			}
		}
	
		return damage;
	}
}

int AActor::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	FState *death;

	if (flags5 & MF5_NODAMAGE)
	{
		return 0;
	}

	// If the actor does not have a corresponding death state, then it does not take damage.
	// Note that DeathState matches every kind of damagetype, so an actor has that, it can
	// be hurt with any type of damage. Exception: Massacre damage always succeeds, because
	// it needs to work.

	// Always kill if there is a regular death state or no death states at all.
	if (FindState (NAME_Death) != NULL || !HasSpecialDeathStates() || damagetype == NAME_Massacre)
	{
		return damage;
	}
	
	if (inflictor && inflictor->DeathType != NAME_None)
		damagetype = inflictor->DeathType;

	if (damagetype == NAME_Ice)
	{
		death = FindState (NAME_Death, NAME_Ice, true);
		if (death == NULL && !deh.NoAutofreeze && !(flags4 & MF4_NOICEDEATH) &&
			(player || (flags3 & MF3_ISMONSTER)))
		{
			death = FindState(NAME_GenericFreezeDeath);
		}
	}
	else
	{
		death = FindState (NAME_Death, damagetype);
	}
	return (death == NULL) ? -1 : damage;
}

int AActor::GibHealth()
{
	return -abs(GetClass()->Meta.GetMetaInt (AMETA_GibHealth, FixedMul(SpawnHealth(), gameinfo.gibfactor)));
}

void AActor::Crash()
{
	// [RC] Weird that this forces the Crash state regardless of flag.
	if(!(flags6 & MF6_DONTCORPSE))
	{
	if (((flags & MF_CORPSE) || (flags6 & MF6_KILLED)) &&
		!(flags3 & MF3_CRASHED) &&
		!(flags & MF_ICECORPSE))
	{
		FState *crashstate = NULL;
		
		if (DamageType != NAME_None)
		{
			if (health < GibHealth())
			{ // Extreme death
				FName labels[] = { NAME_Crash, NAME_Extreme, DamageType };
				crashstate = FindState (3, labels, true);
			}
			if (crashstate == NULL)
			{ // Normal death
				crashstate = FindState(NAME_Crash, DamageType, true);
			}
		}
		if (crashstate == NULL)
		{
			if (health < GibHealth())
			{ // Extreme death
				crashstate = FindState (NAME_Crash, NAME_Extreme);
			}
			else
			{ // Normal death
				crashstate = FindState (NAME_Crash);
			}
		}
		if (crashstate != NULL) SetState(crashstate);
		// Set MF3_CRASHED regardless of the presence of a crash state
		// so this code doesn't have to be executed repeatedly.
		flags3 |= MF3_CRASHED;
	}
	}
}

void AActor::SetIdle()
{
	FState *idle = FindState (NAME_Idle);
	if (idle == NULL) idle = SpawnState;
	SetState(idle);
}

int AActor::SpawnHealth()
{
	int defhealth = StartHealth ? StartHealth : GetDefault()->health;
	if (!(flags3 & MF3_ISMONSTER) || defhealth == 0)
	{
		return defhealth;
	}
	else if (flags & MF_FRIENDLY)
	{
		int adj = FixedMul(defhealth, G_SkillProperty(SKILLP_FriendlyHealth));
		return (adj <= 0) ? 1 : adj;
	}
	else
	{
		int adj = FixedMul(defhealth, G_SkillProperty(SKILLP_MonsterHealth));
		return (adj <= 0) ? 1 : adj;
	}
}

FState *AActor::GetRaiseState()
{
	if (!(flags & MF_CORPSE))
	{
		return NULL;	// not a monster
	}

	if (tics != -1 && // not lying still yet
		!state->GetCanRaise()) // or not ready to be raised yet
	{
		return NULL;
	}

	if (IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{
		return NULL;	// do not resurrect players
	}

	return FindState(NAME_Raise);
}

void AActor::Revive()
{
	AActor *info = GetDefault();
	flags = info->flags;
	flags2 = info->flags2;
	flags3 = info->flags3;
	flags4 = info->flags4;
	flags5 = info->flags5;
	flags6 = info->flags6;
	flags7 = info->flags7;

	// [BC] Apply new ST flags as well.
	// [BB] The STFL_LEVELSPAWNED flag may not be removed by the default flags.
	// Otherwise level spawned actors revived by an Archvile won't be restored
	// during a call of GAME_ResetMap.
	// [WS] We also need this similar treatment for STFL_POSITIONCHANGED.
	const bool actorWasLevelSpawned = !!(ulSTFlags & STFL_LEVELSPAWNED);
	const bool actorHasPositionChanged = !!(ulSTFlags & STFL_POSITIONCHANGED);
	ulSTFlags = info->ulSTFlags;
	if ( actorWasLevelSpawned )
		ulSTFlags |= STFL_LEVELSPAWNED;
	if ( actorHasPositionChanged )
		ulSTFlags |= STFL_POSITIONCHANGED;
	ulNetworkFlags = info->ulNetworkFlags;

	DamageType = info->DamageType;
	health = SpawnHealth();
	target = NULL;
	lastenemy = NULL;

	// [RH] If it's a monster, it gets to count as another kill
	if (CountsAsKill())
	{
		level.total_monsters++;

		// [BB] The number of total monsters was increased, update the invasion monster count accordingly.
		INVASION_UpdateMonsterCount( this, false );
	}
}

FDropItem *AActor::GetDropItems()
{
	unsigned int index = GetClass()->Meta.GetMetaInt (ACMETA_DropItems) - 1;

	if (index < DropItemList.Size())
	{
		return DropItemList[index];
	}
	return NULL;
}

fixed_t AActor::GetGravity() const
{
	if (flags & MF_NOGRAVITY) return 0;
	return fixed_t(level.gravity * Sector->gravity * FIXED2FLOAT(gravity) * 81.92);
}

// killough 11/98:
// Whether an object is "sentient" or not. Used for environmental influences.
// (left precisely the same as MBF even though it doesn't make much sense.)
bool AActor::IsSentient() const
{
	return health > 0 && SeeState != NULL;
}


FSharedStringArena AActor::mStringPropertyData;

const char *AActor::GetTag(const char *def) const
{
	if (Tag != NULL)
	{
		const char *tag = Tag->GetChars();
		if (tag[0] == '$')
		{
			return GStrings(tag + 1);
		}
		else
		{
			return tag;
		}
	}
	else if (def)
	{
		return def;
	}
	else
	{
		return GetClass()->TypeName.GetChars();
	}
}

void AActor::SetTag(const char *def)
{
	if (def == NULL || *def == 0) 
	{
		Tag = NULL;
	}
	else 
	{
		Tag = mStringPropertyData.Alloc(def);
	}
}


void AActor::ClearCounters()
{
	if (CountsAsKill() && health > 0)
	{
		level.total_monsters--;

		// [BB] Since a monster was removed, we also need to correct the number of monsters in invasion mode.
		INVASION_UpdateMonsterCount( this, true );

		// [BB] Inform the clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetMapNumTotalMonsters( );

		flags &= ~MF_COUNTKILL;
	}
	// Same, for items
	if (flags & MF_COUNTITEM)
	{
		level.total_items--;

		// [BB] Inform the clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_SetMapNumTotalItems( );

		flags &= ~MF_COUNTITEM;
	}
	// And finally for secrets
	if (flags5 & MF5_COUNTSECRET)
	{
		level.total_secrets--;
		flags5 &= ~MF5_COUNTSECRET;
	}
}


//----------------------------------------------------------------------------
//
// DropItem handling
//
//----------------------------------------------------------------------------
FDropItemPtrArray DropItemList;

void FreeDropItemChain(FDropItem *chain)
{
	while (chain != NULL)
	{
		FDropItem *next = chain->Next;
		delete chain;
		chain = next;
	}
}

void FDropItemPtrArray::Clear()
{
	for (unsigned int i = 0; i < Size(); ++i)
	{
		FreeDropItemChain ((*this)[i]);
	}
	TArray<FDropItem *>::Clear();
}

int StoreDropItemChain(FDropItem *chain)
{
	return DropItemList.Push (chain) + 1;
}

void PrintMiscActorInfo(AActor *query)
{
	if (query)
	{
		int flagi;
		int querystyle = STYLE_Count;
		for (int style = STYLE_None; style < STYLE_Count; ++style)
		{ // Check for a legacy render style that matches.
			if (LegacyRenderStyles[style] == query->RenderStyle)
			{
				querystyle = style;
				break;
			}
		}
		static const char * renderstyles[]= {"None", "Normal", "Fuzzy", "SoulTrans",
			"OptFuzzy", "Stencil", "Translucent", "Add", "Shaded", "TranslucentStencil"};

		Printf("%s @ %p has the following flags:\n   flags: %x", query->GetTag(), query, query->flags);
		for (flagi = 0; flagi <= 31; flagi++)
			if (query->flags & 1<<flagi) Printf(" %s", FLAG_NAME(1<<flagi, flags));
		Printf("\n   flags2: %x", query->flags2);
		for (flagi = 0; flagi <= 31; flagi++)
			if (query->flags2 & 1<<flagi) Printf(" %s", FLAG_NAME(1<<flagi, flags2));
		Printf("\n   flags3: %x", query->flags3);
		for (flagi = 0; flagi <= 31; flagi++)
			if (query->flags3 & 1<<flagi) Printf(" %s", FLAG_NAME(1<<flagi, flags3));
		Printf("\n   flags4: %x", query->flags4);
		for (flagi = 0; flagi <= 31; flagi++)
			if (query->flags4 & 1<<flagi) Printf(" %s", FLAG_NAME(1<<flagi, flags4));
		Printf("\n   flags5: %x", query->flags5);
		for (flagi = 0; flagi <= 31; flagi++)
			if (query->flags5 & 1<<flagi) Printf(" %s", FLAG_NAME(1<<flagi, flags5));
		Printf("\n   flags6: %x", query->flags6);
		for (flagi = 0; flagi <= 31; flagi++)
			if (query->flags6 & 1<<flagi) Printf(" %s", FLAG_NAME(1<<flagi, flags6));
		Printf("\n   flags7: %x", query->flags7);
		for (flagi = 0; flagi <= 31; flagi++)
			if (query->flags7 & 1<<flagi) Printf(" %s", FLAG_NAME(1<<flagi, flags7));
		Printf("\nBounce flags: %x\nBounce factors: f:%f, w:%f", 
			query->BounceFlags, FIXED2FLOAT(query->bouncefactor), 
			FIXED2FLOAT(query->wallbouncefactor));
		/*for (flagi = 0; flagi < 31; flagi++)
			if (query->BounceFlags & 1<<flagi) Printf(" %s", flagnamesb[flagi]);*/
		Printf("\nRender style = %i:%s, alpha %f\nRender flags: %x", 
			querystyle, (querystyle < STYLE_Count ? renderstyles[querystyle] : "Unknown"),
			FIXED2FLOAT(query->alpha), query->renderflags);
		/*for (flagi = 0; flagi < 31; flagi++)
			if (query->renderflags & 1<<flagi) Printf(" %s", flagnamesr[flagi]);*/
		Printf("\nSpecial+args: %s(%i, %i, %i, %i, %i)\nspecial1: %i, special2: %i.",
			(query->special ? LineSpecialsInfo[query->special]->name : "None"),
			query->args[0], query->args[1], query->args[2], query->args[3], 
			query->args[4],	query->special1, query->special2);
		Printf("\nTID: %d", query->tid);
		Printf("\nCoord= x: %f, y: %f, z:%f, floor:%f, ceiling:%f.",
			FIXED2FLOAT(query->x), FIXED2FLOAT(query->y), FIXED2FLOAT(query->z),
			FIXED2FLOAT(query->floorz), FIXED2FLOAT(query->ceilingz));
		Printf("\nSpeed= %f, velocity= x:%f, y:%f, z:%f, combined:%f.\n",
			FIXED2FLOAT(query->Speed), FIXED2FLOAT(query->velx), FIXED2FLOAT(query->vely), FIXED2FLOAT(query->velz),
			sqrt(pow(FIXED2FLOAT(query->velx), 2) + pow(FIXED2FLOAT(query->vely), 2) + pow(FIXED2FLOAT(query->velz), 2)));
	}
}


// [BC] meh
void P_ResetSpawnCounters( void )
{
	if ( g_lSpawnCount )
	{
		g_lStaleSpawnCount = g_lSpawnCount;
		g_StaleSpawnCycles = g_SpawnCycles;

		g_lSpawnCount = 0;
		g_SpawnCycles.Reset();
	}
}

// [BC]
ADD_STAT( spawns )
{
	FString	Out;

	Out.Format( "Actors spawned: %d in %04.1f ms",
		static_cast<int> (g_lStaleSpawnCount), g_StaleSpawnCycles.TimeMS() );

	return ( Out );
}

#ifdef _DEBUG
// [BC]
#include "c_dispatch.h"
CCMD( dumpcurrentactors )
{
	AActor						*pActor;
	TThinkerIterator<AActor>	Iterator;

	while ( (pActor = Iterator.Next( )))
		Printf( "%s (%d, %d)\n", pActor->GetClass( )->TypeName.GetChars( ), pActor->x >> FRACBITS, pActor->y >> FRACBITS );
}

CCMD( respawnactors )
{
	AActor						*pActor;
	AActor						*pNewActor;
	AActor						*pActorInfo;
	fixed_t						X;
	fixed_t						Y;
	fixed_t						Z;
	TThinkerIterator<AActor>	Iterator;

	while ( (pActor = Iterator.Next( )))
	{
		if (( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideDoomish") ) ||
			( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideSpecial") ) ||
			( pActor->state == RUNTIME_CLASS ( AInventory )->ActorInfo->FindState("HideIndefinitely") ))
		{
//			CLIENT_RestoreSpecialPosition( pActor );
//			CLIENT_RestoreSpecialDoomThing( pActor, true );

			// Get the default information for this actor, so we can determine how to
			// respawn it.
			pActorInfo = pActor->GetDefault( );

			// This item appears to be untouched; no need to respawn it.
			if (( pActor->x == pActor->SpawnPoint[0] ) &&
				( pActor->y == pActor->SpawnPoint[1] ) &&
				( pActor->state == pActor->SpawnState ) &&
				( pActor->health == pActorInfo->health ))
			{
				continue;
			}

			// Determine the Z position to spawn this monster in.
			if ( pActorInfo->flags & MF_SPAWNCEILING )
				Z = ONCEILINGZ;
			else if ( pActorInfo->flags2 & MF2_SPAWNFLOAT )
				Z = FLOATRANDZ;
			else if ( pActorInfo->flags2 & MF2_FLOATBOB )
				Z = pActor->SpawnPoint[2];
			else
				Z = ONFLOORZ;

			// Spawn the new monster.
			X = pActor->SpawnPoint[0];
			Y = pActor->SpawnPoint[1];
			pNewActor = Spawn( RUNTIME_TYPE( pActor ), X, Y, Z, NO_REPLACE );

			if ( Z == ONFLOORZ )
				pNewActor->z += pNewActor->SpawnPoint[2];
			else if ( Z == ONCEILINGZ )
				pNewActor->z -= pNewActor->SpawnPoint[2];

			// Inherit attributes from the old actor.
			pNewActor->SpawnPoint[0] = pActor->SpawnPoint[0];
			pNewActor->SpawnPoint[1] = pActor->SpawnPoint[1];
			pNewActor->SpawnPoint[2] = pActor->SpawnPoint[2];
			pNewActor->SpawnAngle = pActor->SpawnAngle;
			pNewActor->SpawnFlags = pActor->SpawnFlags;
			pNewActor->angle = ANG45 * ( pActor->SpawnAngle / 45 );

			// Just do this stuff for monsters.
			if ( pActor->flags & MF_COUNTKILL )
			{
				if ( pActor->SpawnFlags & MTF_AMBUSH )
					pNewActor->flags |= MF_AMBUSH;

				pNewActor->reactiontime = 18;

				pNewActor->TIDtoHate = pActor->TIDtoHate;
				pNewActor->LastLookActor = pActor->LastLookActor;
				pNewActor->LastLookPlayerNumber = pActor->LastLookPlayerNumber;
				pNewActor->flags3 |= pActor->flags3 & MF3_HUNTPLAYERS;
				pNewActor->flags4 |= pActor->flags4 & MF4_NOHATEPLAYERS;
			}

			pNewActor->flags &= ~MF_DROPPED;
			pNewActor->ulSTFlags |= STFL_LEVELSPAWNED;

			// Remove the old actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DestroyThing( pActor );

			pActor->Destroy( );

			// Tell clients to spawn the new actor.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pNewActor );
		}
	}
}
#endif
