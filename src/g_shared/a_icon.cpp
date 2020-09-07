#include "a_sharedglobal.h"
#include "actor.h"
#include "info.h"
#include "d_player.h"
#include "farchive.h"

IMPLEMENT_CLASS( AFloatyIcon )

void AFloatyIcon::Serialize( FArchive &arc )
{
	Super::Serialize( arc );

	arc << bTeamItemFloatyIcon;
}

void AFloatyIcon::BeginPlay( )
{
	Super::BeginPlay( );
}

void AFloatyIcon::Tick( )
{
	Super::Tick( );

	// Delete stray icons. This shouldn't ever happen.
	if ( !tracer || !tracer->player || tracer->player->pIcon != this )
	{
		Destroy( );
		return;
	}

	// Make the icon float directly above the player's head.
	SetOrigin( tracer->x, tracer->y, tracer->z + tracer->height + ( 4 * FRACUNIT ));

	this->alpha = OPAQUE;
	this->RenderStyle = STYLE_Normal;

	if ( this->lTick )
	{
		this->lTick--;
		if ( this->lTick )
		{
			// If the icon is fading out, ramp down the alpha.
			if ( this->lTick <= TICRATE )
			{
				this->alpha = (LONG)( OPAQUE * (float)( (float)lTick / (float)TICRATE ));
				this->RenderStyle = STYLE_Translucent;
			}
		}
		else
		{
			Destroy( );
			return;
		}
	}

	// If the tracer has some type of visibility affect, apply it to the icon.
	if ( !(tracer->RenderStyle == LegacyRenderStyles[STYLE_Normal]) || tracer->alpha != OPAQUE )
	{
		this->RenderStyle = tracer->RenderStyle;
		this->alpha = tracer->alpha;
	}
}

void AFloatyIcon::SetTracer( AActor *pTracer )
{
	tracer = pTracer;

	// [BB] This only seems to happen if a player has an invalid body. Nevertheless, we should avoid a crash here. 
	if ( tracer == NULL )
		return;

	// Make the icon float directly above the tracer's head.
	SetOrigin( tracer->x, tracer->y, tracer->z + tracer->height + ( 4 * FRACUNIT ));

	// If the tracer has some type of visibility affect, apply it to the icon.
	if ( !(tracer->RenderStyle == LegacyRenderStyles[STYLE_Normal]) || tracer->alpha != OPAQUE )
	{
		this->RenderStyle = tracer->RenderStyle;
		this->alpha = tracer->alpha;
	}
}
