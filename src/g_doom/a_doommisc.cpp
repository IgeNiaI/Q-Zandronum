#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "m_random.h"
#include "gi.h"
#include "doomstat.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "a_specialspot.h"
#include "templates.h"
#include "m_bbox.h"
#include "farchive.h"
// [BC] New #includes.
#include "cl_demo.h"
#include "gamemode.h"
#include "deathmatch.h"
#include "team.h"
#include "unlagged.h"

// Include all the other Doom stuff here to reduce compile time
#include "a_arachnotron.cpp"
#include "a_archvile.cpp"
#include "a_bossbrain.cpp"
#include "a_bruiser.cpp"
#include "a_cacodemon.cpp"
#include "a_cyberdemon.cpp"
#include "a_demon.cpp"
#include "a_doomimp.cpp"
#include "a_doomweaps.cpp"
#include "a_fatso.cpp"
#include "a_keen.cpp"
#include "a_lostsoul.cpp"
#include "a_painelemental.cpp"
#include "a_possessed.cpp"
#include "a_revenant.cpp"
#include "a_spidermaster.cpp"
#include "a_scriptedmarine.cpp"

// The barrel of green goop ------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BarrelDestroy)
{
	// [BC] Just always destroy it in client mode.
	if ( NETWORK_InClientMode() )
	{
		self->Destroy( );
		return;
	}

	if (dmflags2 & DF2_BARRELS_RESPAWN)
	{
		self->height = self->GetDefault()->height;
		self->renderflags |= RF_INVISIBLE;
		self->flags &= ~MF_SOLID;

		// [BB] The clients destroy their local version of the actor, we have to keep this in mind.
		// For instance to prevent this from being spawned on newly connecting clients.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			self->ulNetworkFlags |= NETFL_DESTROYED_ON_CLIENT;
	}
	else
	{
		// [BB] Only destroy the actor if it's not needed for a map reset. Otherwise just hide it.
		self->HideOrDestroyIfSafe ();
	}
}
