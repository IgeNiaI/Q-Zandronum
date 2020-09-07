#include "info.h"
#include "a_pickups.h"
#include "doomstat.h"
#include "d_player.h"
#include "p_local.h"
#include "gi.h"
#include "gstrings.h"
#include "s_sound.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "p_effect.h"
#include "cl_demo.h"
#include "a_doomglobal.h"
#include "announcer.h"
#include "farchive.h"

// [BC] Random Powerup ------------------------------------------------------

#define	RPF_MEGASPHERE				1
#define	RPF_SOULSPHERE				2
#define	RPF_GUARDSPHERE				4
#define	RPF_PARTIALINVISIBILITY		8
#define	RPF_TIMEFREEZESPHERE		16
#define	RPF_INVISIBILITY			32
#define	RPF_DOOMSPHERE				64
#define	RPF_TURBOSPHERE				128
#define	RPF_ALL						( RPF_MEGASPHERE|RPF_SOULSPHERE|RPF_GUARDSPHERE| \
									  RPF_PARTIALINVISIBILITY|RPF_TIMEFREEZESPHERE|RPF_INVISIBILITY | \
									  RPF_DOOMSPHERE|RPF_TURBOSPHERE )

class ARandomPowerup : public AInventory
{
	DECLARE_CLASS (ARandomPowerup, AInventory)
public:
	virtual bool	Use (bool pickup);
	void			Serialize( FArchive &arc );

	void	PostBeginPlay( )
	{
		Super::PostBeginPlay( );

		ulPowerupFlags = args[0];
		if ( ulPowerupFlags == 0 )
			ulPowerupFlags |= RPF_ALL;
	}

	bool	IsFrameAllowed( ULONG ulFrame )
	{
		// No time freeze spheres in multiplayer games.
		if (( ulFrame == 4 ) && ( NETWORK_GetState( ) != NETSTATE_SINGLE ))
			return ( false );

		return !!( ulPowerupFlags & ( 1 << ulFrame ));
	}

	// [EP] TODO: remove the 'ul' prefix from this variable, it isn't ULONG anymore
	unsigned int ulCurrentFrame;

protected:
	const char *PickupMessage ();

	// [EP] TODO: remove the 'ul' prefix from this variable, it isn't ULONG anymore
	unsigned int ulPowerupFlags;
};

DEFINE_ACTION_FUNCTION(AActor, A_RandomPowerupFrame)
{
	ULONG	ulFrame;
	ARandomPowerup	*pRandomPowerup;

	pRandomPowerup = static_cast<ARandomPowerup *>( self );

	ulFrame = (ULONG)( self->state - self->SpawnState );
	if ( pRandomPowerup->IsFrameAllowed( ulFrame ) == false )
		self->SetState( self->state->NextState );
	else
		pRandomPowerup->ulCurrentFrame = ulFrame;
}

IMPLEMENT_CLASS (ARandomPowerup)

bool ARandomPowerup::Use (bool pickup)
{
	AInventory		*pItem;
	const PClass	*pType = NULL;
	bool			bReturnValue = false;

	if ( NETWORK_InClientMode() )
	{
		return ( false );
	}

	switch ( ulCurrentFrame )
	{
	// Megasphere.
	case 0:

		pType = PClass::FindClass( "Megasphere" );
		break;
	// Soulsphere.
	case 1:

		pType = PClass::FindClass( "Soulsphere" );
		break;
	// Guardsphere.
	case 2:

		pType = PClass::FindClass( "Guardsphere" );
		break;
	// Partial invisibility.
	case 3:

		pType = PClass::FindClass( "Blursphere" );
		break;
	// Time freeze.
	case 4:

		pType = PClass::FindClass( "TimeFreezeSphere" );
		break;
	// Invisibility.
	case 5:

		pType = PClass::FindClass( "InvisibilitySphere" );
		break;
	// Doomsphere.
	case 6:

		pType = PClass::FindClass( "Doomsphere" );
		break;
	// Turbosphere.
	case 7:

		pType = PClass::FindClass( "Turbosphere" );
		break;
	}

	pItem = static_cast<AInventory *>( Spawn( pType, Owner->x, Owner->y, Owner->z, ALLOW_REPLACE ));
	if ( pItem != NULL )
	{
		bReturnValue = pItem->CallTryPickup( Owner );
		if ( bReturnValue )
		{
			// [BC] If the item has an announcer sound, play it.
			if ( Owner->CheckLocalView( consoleplayer ) && cl_announcepickups )
				ANNOUNCER_PlayEntry( cl_announcer, pItem->PickupAnnouncerEntry( ));

			// [BC] Tell the client that he successfully picked up the item.
			if (( NETWORK_GetState( ) == NETSTATE_SERVER ) &&
				( Owner->player ) &&
				// [BB] Special handling for RandomPowerup, formerly done with NETFL_SPECIALPICKUP.
				( this->GetClass( )->IsDescendantOf( PClass::FindClass( "RandomPowerup" ) ) ))
			{
				SERVERCOMMANDS_GiveInventory( ULONG( Owner->player - players ), pItem );

				if (( ItemFlags & IF_QUIET ) == false )
					SERVERCOMMANDS_DoInventoryPickup( ULONG( Owner->player - players ), pItem->GetClass( )->TypeName.GetChars( ), pItem->PickupMessage( ));
			}
		}

		if (( pItem->ObjectFlags & OF_EuthanizeMe ) == false )
			pItem->Destroy( );
	}

	return ( bReturnValue );
}

void ARandomPowerup::Serialize( FArchive &arc )
{
	Super::Serialize( arc );
	arc << ulCurrentFrame << ulPowerupFlags;
}

const char *ARandomPowerup::PickupMessage( )
{
	if ( NETWORK_InClientMode() )
	{
		return ( NULL );
	}

	switch ( ulCurrentFrame )
	{
	case 0:

		return GStrings( "GOTMSPHERE" );
	case 1:

		return GStrings( "GOTSUPER" );
	case 2:

		return GStrings( "PICKUP_GUARDSPHERE" );
	case 3:

		return GStrings( "GOTINVIS" );
	case 4:

		return GStrings( "PICKUP_TIMEFREEZER" );
	case 5:

		return GStrings( "PICKUP_INVISIBILITY" );
	case 6:

		return GStrings( "PICKUP_DOOMSPHERE" );
	case 7:

		return GStrings( "PICKUP_TURBOSPHERE" );
	}

	return ( "Random powerup error" );
}