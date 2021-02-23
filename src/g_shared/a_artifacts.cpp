#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gi.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "a_artifacts.h"
#include "sbar.h"
#include "d_player.h"
#include "m_random.h"
#include "v_video.h"
#include "templates.h"
#include "a_morph.h"
#include "g_level.h"
#include "doomstat.h"
#include "v_palette.h"
#include "farchive.h"
#include "r_data/colormaps.h"
// New #includes for ST.
#include "cl_demo.h"
#include "cl_main.h"
#include "network.h"
#include "g_game.h"
#include "deathmatch.h"
#include "possession.h"
#include "sv_commands.h"

static FRandom pr_torch ("Torch");

/* Those are no longer needed, except maybe as reference?
 * They're not used anywhere in the code anymore, except
 * MAULATORTICS as redefined in a_minotaur.cpp...
#define	INVULNTICS (30*TICRATE)
#define	INVISTICS (60*TICRATE)
#define	INFRATICS (120*TICRATE)
#define	IRONTICS (60*TICRATE)
#define WPNLEV2TICS (40*TICRATE)
#define FLIGHTTICS (60*TICRATE)
#define SPEEDTICS (45*TICRATE)
#define MAULATORTICS (25*TICRATE)
#define	TIMEFREEZE_TICS	( 12 * TICRATE )
*/

// [BC] New Skulltag power duration defines.
#define	TRANSLUCENCY_TICS		( 45 * TICRATE )

IMPLEMENT_CLASS (APowerup)

// Powerup-Giver -------------------------------------------------------------

//===========================================================================
//
// APowerupGiver :: Use
//
//===========================================================================

bool APowerupGiver::Use (bool pickup)
{
	if (PowerupType == NULL) return true;	// item is useless

	APowerup *power = static_cast<APowerup *> (Spawn (PowerupType, 0, 0, 0, NO_REPLACE));

	if (EffectTics != 0)
	{
		power->EffectTics = EffectTics;
	}
	if (BlendColor != 0)
	{
		power->BlendColor = BlendColor;
	}
	if (Mode != NAME_None)
	{
		power->Mode = Mode;
	}
	if (Strength != 0)
	{
		power->Strength = Strength;
	}

	// [TP]
	ModifyPowerup( power );

	power->ItemFlags |= ItemFlags & (IF_ALWAYSPICKUP|IF_ADDITIVETIME);
	if (power->CallTryPickup (Owner))
	{
		PowerupGranted( power ); // [TP]
		return true;
	}
	power->GoAwayAndDie ();
	return false;
}

//===========================================================================
//
// APowerupGiver :: Serialize
//
//===========================================================================

void APowerupGiver::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PowerupType;
	arc << EffectTics << BlendColor << Mode;
	arc << Strength;
}

// Powerup -------------------------------------------------------------------

//===========================================================================
//
// APowerup :: Tick
//
//===========================================================================

void APowerup::Tick ()
{
	// Powerups cannot exist outside an inventory
	if (Owner == NULL)
	{
		Destroy ();
	}
	if (EffectTics > 0 && --EffectTics == 0)
	{
		Destroy ();
	}
}

//===========================================================================
//
// APowerup :: Serialize
//
//===========================================================================

void APowerup::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << EffectTics << BlendColor << Mode;
	arc << Strength;
}

//===========================================================================
//
// APowerup :: GetBlend
//
//===========================================================================

PalEntry APowerup::GetBlend ()
{
	if (EffectTics <= BLINKTHRESHOLD && !(EffectTics & 8))
		return 0;

	if (IsSpecialColormap(BlendColor)) return 0;
	return BlendColor;
}

//===========================================================================
//
// APowerup :: InitEffect
//
//===========================================================================

void APowerup::InitEffect ()
{
}

//===========================================================================
//
// APowerup :: DoEffect
//
//===========================================================================

void APowerup::DoEffect ()
{
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}

	if (EffectTics > 0)
	{
		int Colormap = GetSpecialColormap(BlendColor);

		if (Colormap != NOFIXEDCOLORMAP)
		{
			if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
			{
				Owner->player->fixedcolormap = Colormap;
				// [BC] Apply the colormap to the player's body, also.
				Owner->lFixedColormap = Colormap;
			}
			else if (Owner->player->fixedcolormap == Colormap)	
			{
				// only unset if the fixed colormap comes from this item
				Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
				// [BB] Also unset the colormap of the player's body.
				Owner->lFixedColormap = NOFIXEDCOLORMAP;
			}
		}
	}
}

//===========================================================================
//
// APowerup :: EndEffect
//
//===========================================================================

void APowerup::EndEffect ()
{
	int colormap = GetSpecialColormap(BlendColor);

	if (colormap != NOFIXEDCOLORMAP && Owner && Owner->player && Owner->player->fixedcolormap == colormap)
	{ // only unset if the fixed colormap comes from this item
		Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
		// [BB] Also unset the colormap of the player's body.
		Owner->lFixedColormap = NOFIXEDCOLORMAP;
	}
}

//===========================================================================
//
// APowerup :: Destroy
//
//===========================================================================

void APowerup::Destroy ()
{
	EndEffect ();
	Super::Destroy ();
}

//===========================================================================
//
// APowerup :: DrawPowerup
//
//===========================================================================

bool APowerup::DrawPowerup (int x, int y)
{
	// [TP] Do not draw this powerup if it's the current rune.
	if ( IsActiveRune() )
		return false;

	if (!Icon.isValid())
	{
		return false;
	}
	if (EffectTics > BLINKTHRESHOLD || !(EffectTics & 16))
	{
		FTexture *pic = TexMan(Icon);
		screen->DrawTexture (pic, x, y,
			DTA_HUDRules, HUD_Normal,
//			DTA_TopOffset, pic->GetHeight()/2,
//			DTA_LeftOffset, pic->GetWidth()/2,
			TAG_DONE);
	}
	return true;
}

//===========================================================================
//
// APowerup :: HandlePickup
//
//===========================================================================

bool APowerup::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		APowerup *power = static_cast<APowerup*>(item);
		if (power->EffectTics == 0)
		{
			power->ItemFlags |= IF_PICKUPGOOD;
			return true;
		}
		// Color gets transferred if the new item has an effect.

		// Increase the effect's duration.
		if (power->ItemFlags & IF_ADDITIVETIME) 
		{
			EffectTics += power->EffectTics;
			BlendColor = power->BlendColor;
		}
		// If it's not blinking yet, you can't replenish the power unless the
		// powerup is required to be picked up.
		else if (EffectTics > BLINKTHRESHOLD && !(power->ItemFlags & IF_ALWAYSPICKUP))
		{
			return true;
		}
		// Reset the effect duration.
		else if (power->EffectTics > EffectTics)
		{
			EffectTics = power->EffectTics;
			BlendColor = power->BlendColor;
		}
		power->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// APowerup :: CreateCopy
//
//===========================================================================

AInventory *APowerup::CreateCopy (AActor *other)
{
	// Get the effective effect time.
	EffectTics = abs (EffectTics);
	// Abuse the Owner field to tell the
	// InitEffect method who started it;
	// this should be cleared afterwards,
	// as this powerup instance is not
	// properly attached to anything yet.
	Owner = other;
	// Actually activate the powerup.
	InitEffect ();
	// Clear the Owner field, unless it was
	// changed by the activation, for example,
	// if this instance is a morph powerup;
	// the flag tells the caller that the
	// ownership has changed so that they
	// can properly handle the situation.
	if (!(ItemFlags & IF_CREATECOPYMOVED))
	{
		Owner = NULL;
	}
	// All done.
	return this;
}

//===========================================================================
//
// APowerup :: CreateTossable
//
// Powerups are never droppable, even without IF_UNDROPPABLE set.
//
//===========================================================================

AInventory *APowerup::CreateTossable ()
{
	return NULL;
}

//===========================================================================
//
// APowerup :: OwnerDied
//
// Powerups don't last beyond death.
//
//===========================================================================

void APowerup::OwnerDied ()
{
	Destroy ();
}

//===========================================================================
//
// [TP] APowerup :: IsActiveRune
//
// Is this powerup its owner's current rune?
//
//===========================================================================

bool APowerup::IsActiveRune()
{
	return ( Owner != NULL ) && ( Owner->Rune == this );
}

// Invulnerability Powerup ---------------------------------------------------

IMPLEMENT_CLASS (APowerInvulnerable)

//===========================================================================
//
// APowerInvulnerable :: InitEffect
//
//===========================================================================

void APowerInvulnerable::InitEffect ()
{
	Super::InitEffect();
	Owner->effects &= ~FX_RESPAWNINVUL;
	Owner->flags2 |= MF2_INVULNERABLE;
	if (Mode == NAME_None)
	{
		Mode = (ENamedName)RUNTIME_TYPE(Owner)->Meta.GetMetaInt(APMETA_InvulMode);
	}
	if (Mode == NAME_Reflective)
	{
		Owner->flags2 |= MF2_REFLECTIVE;
	}
}

//===========================================================================
//
// APowerInvulnerable :: DoEffect
//
//===========================================================================

void APowerInvulnerable::DoEffect ()
{
	Super::DoEffect ();

	if (Owner == NULL)
	{
		return;
	}

	if (Mode == NAME_Ghost)
	{
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Translucent;
			if (!(level.time & 7) && Owner->alpha > 0 && Owner->alpha < OPAQUE)
			{
				if (Owner->alpha == HX_SHADOW)
				{
					Owner->alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->alpha = 0;
					Owner->flags2 |= MF2_NONSHOOTABLE;
				}
			}
			if (!(level.time & 31))
			{
				if (Owner->alpha == 0)
				{
					Owner->flags2 &= ~MF2_NONSHOOTABLE;
					Owner->alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->alpha = HX_SHADOW;
				}
			}
		}
		else
		{
			Owner->flags2 &= ~MF2_NONSHOOTABLE;
		}
	}
}

//===========================================================================
//
// APowerInvulnerable :: EndEffect
//
//===========================================================================

void APowerInvulnerable::EndEffect ()
{
	Super::EndEffect();

	if (Owner == NULL)
	{
		return;
	}

	Owner->flags2 &= ~MF2_INVULNERABLE;
	Owner->effects &= ~FX_RESPAWNINVUL;
	if ( Owner->effects & FX_VISIBILITYFLICKER )
	{
		Owner->effects &= ~FX_VISIBILITYFLICKER;
		// [BC] If the owner is a spectating player, don't make him visible!
		if (( Owner->player == NULL ) || ( Owner->player->bSpectating == false ))
		{
			// [BB] Restore the default alpha value and set RenderStyle accordingly.
			Owner->alpha = Owner->GetDefault()->alpha;
			if ( Owner->alpha == OPAQUE )
				Owner->RenderStyle = STYLE_Normal;
			else
				Owner->RenderStyle = STYLE_Translucent;
		}
		else
			Owner->RenderStyle = STYLE_None;
	}
	if (Mode == NAME_Ghost)
	{
		Owner->flags2 &= ~MF2_NONSHOOTABLE;
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Normal;
			Owner->alpha = OPAQUE;
		}
	}
	else if (Mode == NAME_Reflective)
	{
		Owner->flags2 &= ~MF2_REFLECTIVE;
	}

	if (Owner->player != NULL)
	{
		Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
		// [BB] Additionally clear lFixedColormap.
		Owner->lFixedColormap = NOFIXEDCOLORMAP;
	}
}

//===========================================================================
//
// APowerInvulnerable :: AlterWeaponSprite
//
//===========================================================================

int APowerInvulnerable::AlterWeaponSprite (visstyle_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);
	if (Owner != NULL)
	{
		if (Mode == NAME_Ghost && !(Owner->flags & MF_SHADOW))
		{
			fixed_t wp_alpha = MIN<fixed_t>(FRACUNIT/4 + Owner->alpha*3/4, FRACUNIT);
			if (wp_alpha != FIXED_MAX) vis->alpha = wp_alpha;
		}
	}
	return changed;
}

// Strength (aka Berserk) Powerup --------------------------------------------

IMPLEMENT_CLASS (APowerStrength)

//===========================================================================
//
// APowerStrength :: HandlePickup
//
//===========================================================================

bool APowerStrength::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{ // Setting EffectTics to 0 will force Powerup's HandlePickup()
	  // method to reset the tic count so you get the red flash again.
		EffectTics = 0;
	}
	return Super::HandlePickup (item);
}

//===========================================================================
//
// APowerStrength :: InitEffect
//
//===========================================================================

void APowerStrength::InitEffect ()
{
	Super::InitEffect();
}

//===========================================================================
//
// APowerStrength :: DoEffect
//
//===========================================================================

void APowerStrength::Tick ()
{
	// Strength counts up to diminish the fade.
	assert(EffectTics < (INT_MAX - 1)); // I can't see a game lasting nearly two years, but...
	EffectTics += 2;
	Super::Tick();
}

//===========================================================================
//
// APowerStrength :: GetBlend
//
//===========================================================================

PalEntry APowerStrength::GetBlend ()
{
	// slowly fade the berserk out
	int cnt = 12 - (EffectTics >> 6);

	if (cnt > 0)
	{
		cnt = (cnt + 7) >> 3;
		return PalEntry (BlendColor.a*cnt*255/9,
			BlendColor.r, BlendColor.g, BlendColor.b);
	}
	return 0;
}

// Invisibility Powerup ------------------------------------------------------

IMPLEMENT_CLASS (APowerInvisibility)

// Invisibility flag combos
#define INVISIBILITY_FLAGS1	(MF_SHADOW)
#define INVISIBILITY_FLAGS3	(MF3_GHOST)
#define INVISIBILITY_FLAGS5	(MF5_CANTSEEK)

//===========================================================================
//
// APowerInvisibility :: InitEffect
//
//===========================================================================

void APowerInvisibility::InitEffect ()
{
	Super::InitEffect();
	// This used to call CommonInit(), which used to contain all the code that's repeated every
	// tic, plus the following code that needs to happen once and only once.
	// The CommonInit() code has been moved to DoEffect(), so this now ends with a call to DoEffect(),
	// and DoEffect() no longer needs to call InitEffect(). CommonInit() has been removed for being redundant.
	if (Owner != NULL)
	{
		flags &= ~(Owner->flags  & INVISIBILITY_FLAGS1);
		Owner->flags  |= flags & INVISIBILITY_FLAGS1;
		flags3 &= ~(Owner->flags3 & INVISIBILITY_FLAGS3);
		Owner->flags3 |= flags3 & INVISIBILITY_FLAGS3;
		flags5 &= ~(Owner->flags5 & INVISIBILITY_FLAGS5);
		Owner->flags5 |= flags5 & INVISIBILITY_FLAGS5;

		DoEffect();
	}
}

//===========================================================================
//
// APowerInvisibility :: DoEffect
//
//===========================================================================
void APowerInvisibility::DoEffect ()
{
	Super::DoEffect();
	// Due to potential interference with other PowerInvisibility items
	// the effect has to be refreshed each tic.
	fixed_t ts = (Strength/100) * (special1 + 1); if (ts > FRACUNIT) ts = FRACUNIT;
	Owner->alpha = clamp<fixed_t>((OPAQUE - ts), 0, OPAQUE);
	switch (Mode)
	{
	case (NAME_Fuzzy):
		Owner->RenderStyle = STYLE_OptFuzzy;
		break;
	case (NAME_Opaque):
		Owner->RenderStyle = STYLE_Normal;
		break;
	case (NAME_Additive):
		Owner->RenderStyle = STYLE_Add;
		break;
	case (NAME_Stencil):
		Owner->RenderStyle = STYLE_Stencil;
		break;
	case (NAME_None):
	case (NAME_Cumulative):
	case (NAME_Translucent):
		Owner->RenderStyle = STYLE_Translucent;
		break;
	default: // Something's wrong
		Owner->RenderStyle = STYLE_Normal;
		Owner->alpha = OPAQUE;
		break;
	}
}

//===========================================================================
//
// APowerInvisibility :: EndEffect
//
//===========================================================================

void APowerInvisibility::EndEffect ()
{
	Super::EndEffect();
	if (Owner != NULL)
	{
		Owner->flags  &= ~(flags  & INVISIBILITY_FLAGS1);
		Owner->flags3 &= ~(flags3 & INVISIBILITY_FLAGS3);
		Owner->flags5 &= ~(flags5 & INVISIBILITY_FLAGS5);

		// [BC] If the owner is a spectating player, don't make him visible!
		if (( Owner->player == NULL ) || ( Owner->player->bSpectating == false ))
			Owner->RenderStyle = STYLE_Normal;
		else
			Owner->RenderStyle = STYLE_None;
		Owner->alpha = OPAQUE;

		// Check whether there are other invisibility items and refresh their effect.
		// If this isn't done there will be one incorrectly drawn frame when this
		// item expires.
		AInventory *item = Owner->Inventory;
		while (item != NULL)
		{
			if (item->IsKindOf(RUNTIME_CLASS(APowerInvisibility)) && item != this)
			{
				static_cast<APowerInvisibility*>(item)->DoEffect();
			}
			item = item->Inventory;
		}
	}
}

//===========================================================================
//
// APowerInvisibility :: AlterWeaponSprite
//
//===========================================================================

int APowerInvisibility::AlterWeaponSprite (visstyle_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);
	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		vis->alpha = OPAQUE;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		fixed_t ts = (Strength/100) * (special1 + 1); if (ts > FRACUNIT) ts = FRACUNIT;
		vis->alpha = clamp<fixed_t>((OPAQUE - ts), 0, OPAQUE);
		switch (Mode)
		{
		case (NAME_Fuzzy):
			vis->RenderStyle = STYLE_OptFuzzy;
			break;
		case (NAME_Opaque):
			vis->RenderStyle = STYLE_Normal;
			break;
		case (NAME_Additive):
			vis->RenderStyle = STYLE_Add;
			break;
		case (NAME_Stencil):
			vis->RenderStyle = STYLE_Stencil;
			break;
		case (NAME_None):
		case (NAME_Cumulative):
		case (NAME_Translucent):
		default:
			vis->RenderStyle = STYLE_Translucent;
			break;
		}
	}
	// Handling of Strife-like cumulative invisibility powerups, the weapon itself shouldn't become invisible
	if ((vis->alpha < TRANSLUC25 && special1 > 0) || (vis->alpha == 0))
	{
		vis->alpha = clamp<fixed_t>((OPAQUE - (Strength/100)), 0, OPAQUE);
		vis->colormap = SpecialColormaps[INVERSECOLORMAP].Colormap;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

//===========================================================================
//
// APowerInvisibility :: HandlePickup
//
// If the player already has the first stage of a cumulative powerup, getting 
// it again increases the player's alpha. (But shouldn't this be in Use()?)
//
//===========================================================================

bool APowerInvisibility::HandlePickup (AInventory *item)
{
	if (Mode == NAME_Cumulative && ((Strength * special1) < FRACUNIT) && item->GetClass() == GetClass())
	{
		APowerup *power = static_cast<APowerup *>(item);
		if (power->EffectTics == 0)
		{
			power->ItemFlags |= IF_PICKUPGOOD;
			return true;
		}
		// Only increase the EffectTics, not decrease it.
		// Color also gets transferred only when the new item has an effect.
		if (power->EffectTics > EffectTics)
		{
			EffectTics = power->EffectTics;
			BlendColor = power->BlendColor;
		}
		special1++;	// increases power
		power->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	return Super::HandlePickup (item);
}

// Ironfeet Powerup ----------------------------------------------------------

IMPLEMENT_CLASS (APowerIronFeet)

//===========================================================================
//
// APowerIronFeet :: AbsorbDamage
//
//===========================================================================

void APowerIronFeet::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Drowning)
	{
		newdamage = 0;
	}
	else if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// APowerIronFeet :: DoEffect
//
//===========================================================================

void APowerIronFeet::DoEffect ()
{
	// [EP] Be careful with the dummy player
	if (Owner->player != NULL && Owner->player->mo != NULL)
	{
		Owner->player->mo->ResetAirSupply ();
	}
}


// Strife Environment Suit Powerup -------------------------------------------

IMPLEMENT_CLASS (APowerMask)

//===========================================================================
//
// APowerMask :: AbsorbDamage
//
//===========================================================================

void APowerMask::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Fire)
	{
		newdamage = 0;
	}
	else
	{
		Super::AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// APowerMask :: DoEffect
//
//===========================================================================

void APowerMask::DoEffect ()
{
	Super::DoEffect ();
	if (!(level.time & 0x3f))
	{
		S_Sound (Owner, CHAN_AUTO, "misc/mask", 1, ATTN_STATIC);
	}
}

// Light-Amp Powerup ---------------------------------------------------------

IMPLEMENT_CLASS (APowerLightAmp)

//===========================================================================
//
// APowerLightAmp :: DoEffect
//
//===========================================================================

void APowerLightAmp::DoEffect ()
{
	Super::DoEffect ();

	if (Owner->player != NULL && Owner->player->fixedcolormap < NUMCOLORMAPS)
	{
		if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
		{	
			Owner->player->fixedlightlevel = 1;
		}
		else
		{
			Owner->player->fixedlightlevel = -1;
		}
	}
}

//===========================================================================
//
// APowerLightAmp :: EndEffect
//
//===========================================================================

void APowerLightAmp::EndEffect ()
{
	Super::EndEffect();
	if (Owner != NULL && Owner->player != NULL && Owner->player->fixedcolormap < NUMCOLORMAPS)
	{
		Owner->player->fixedlightlevel = -1;
	}
}

// Torch Powerup -------------------------------------------------------------

IMPLEMENT_CLASS (APowerTorch)

//===========================================================================
//
// APowerTorch :: Serialize
//
//===========================================================================

void APowerTorch::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << NewTorch << NewTorchDelta;
}

//===========================================================================
//
// APowerTorch :: DoEffect
//
//===========================================================================

void APowerTorch::DoEffect ()
{
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}

	if (EffectTics <= BLINKTHRESHOLD || Owner->player->fixedcolormap >= NUMCOLORMAPS)
	{
		Super::DoEffect ();
	}
	else 
	{
		APowerup::DoEffect ();

		if (!(level.time & 16) && Owner->player != NULL)
		{
			if (NewTorch != 0)
			{
				if (Owner->player->fixedlightlevel + NewTorchDelta > 7
					|| Owner->player->fixedlightlevel + NewTorchDelta < 0
					|| NewTorch == Owner->player->fixedlightlevel)
				{
					NewTorch = 0;
				}
				else
				{
					Owner->player->fixedlightlevel += NewTorchDelta;
				}
			}
			else
			{
				NewTorch = (pr_torch() & 7) + 1;
				NewTorchDelta = (NewTorch == Owner->player->fixedlightlevel) ?
					0 : ((NewTorch > Owner->player->fixedlightlevel) ? 1 : -1);
			}
		}
	}
}

// Flight (aka Wings of Wrath) powerup ---------------------------------------

IMPLEMENT_CLASS (APowerFlight)

//===========================================================================
//
// APowerFlight :: Serialize
//
//===========================================================================

void APowerFlight::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << HitCenterFrame;
}

//===========================================================================
//
// APowerFlight :: InitEffect
//
//===========================================================================

void APowerFlight::InitEffect ()
{
	Super::InitEffect();
	Owner->flags2 |= MF2_FLY;
	Owner->flags |= MF_NOGRAVITY;
	if (Owner->z <= Owner->floorz)
	{
		Owner->velz = 4*FRACUNIT;	// thrust the player in the air a bit
	}
	if (Owner->velz <= -35*FRACUNIT)
	{ // stop falling scream
		S_StopSound (Owner, CHAN_VOICE);
	}
}

//===========================================================================
//
// APowerFlight :: DoEffect
//
//===========================================================================

void APowerFlight::Tick ()
{
	// The Wings of Wrath only expire in multiplayer and non-hub games
	if (( NETWORK_GetState( ) == NETSTATE_SINGLE ) && (level.flags2 & LEVEL2_INFINITE_FLIGHT))
	{
		assert(EffectTics < INT_MAX); // I can't see a game lasting nearly two years, but...
		EffectTics++;
	}

	Super::Tick ();

//	Owner->flags |= MF_NOGRAVITY;
//	Owner->flags2 |= MF2_FLY;
}

//===========================================================================
//
// APowerFlight :: EndEffect
//
//===========================================================================

void APowerFlight::EndEffect ()
{
	Super::EndEffect();
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}
	if (!(Owner->player->cheats & CF_FLY))
	{
		Owner->flags2 &= ~MF2_FLY;
		Owner->flags &= ~MF_NOGRAVITY;
	}
//	BorderTopRefresh = screen->GetPageCount (); //make sure the sprite's cleared out
}

//===========================================================================
//
// APowerFlight :: DrawPowerup
//
//===========================================================================

bool APowerFlight::DrawPowerup (int x, int y)
{
	// [TP] Do not draw this powerup if it's the current rune.
	if ( IsActiveRune() )
		return false;

	// If this item got a valid icon use that instead of the default spinning wings.
	if (Icon.isValid())
	{
		return Super::DrawPowerup(x, y);
	}

	if (EffectTics > BLINKTHRESHOLD || !(EffectTics & 16))
	{
		FTextureID picnum = TexMan.CheckForTexture ("SPFLY0", FTexture::TEX_MiscPatch);
		int frame = (level.time/3) & 15;

		if (!picnum.isValid())
		{
			return false;
		}
		if (Owner->flags & MF_NOGRAVITY)
		{
			if (HitCenterFrame && (frame != 15 && frame != 0))
			{
				screen->DrawTexture (TexMan[picnum+15], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
			}
			else
			{
				screen->DrawTexture (TexMan[picnum+frame], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = false;
			}
		}
		else
		{
			if (!HitCenterFrame && (frame != 15 && frame != 0))
			{
				screen->DrawTexture (TexMan[picnum+frame], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = false;
			}
			else
			{
				screen->DrawTexture (TexMan[picnum+15], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = true;
			}
		}
	}
	return true;
}

// Weapon Level 2 (aka Tome of Power) Powerup --------------------------------

IMPLEMENT_CLASS (APowerWeaponLevel2)

//===========================================================================
//
// APowerWeaponLevel2 :: InitEffect
//
//===========================================================================

void APowerWeaponLevel2::InitEffect ()
{
	AWeapon *weapon, *sister;

	Super::InitEffect();

	if (Owner->player == NULL)
		return;

	weapon = Owner->player->ReadyWeapon;

	if (weapon == NULL)
		return;

	sister = weapon->SisterWeapon;

	if (sister == NULL)
		return;

	if (!(sister->WeaponFlags & WIF_POWERED_UP))
		return;

	assert (sister->SisterWeapon == weapon);

	Owner->player->ReadyWeapon = sister;

	if (weapon->GetReadyState() != sister->GetReadyState())
	{
		P_SetPsprite (Owner->player, ps_weapon, sister->GetReadyState());
	}
}

//===========================================================================
//
// APowerWeaponLevel2 :: EndEffect
//
//===========================================================================

void APowerWeaponLevel2::EndEffect ()
{
	player_t *player = Owner != NULL ? Owner->player : NULL;

	Super::EndEffect();
	if (player != NULL)
	{
		if (player->ReadyWeapon != NULL &&
			player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP)
		{
			player->ReadyWeapon->EndPowerup ();
		}
		if (player->PendingWeapon != NULL && player->PendingWeapon != WP_NOCHANGE &&
			player->PendingWeapon->WeaponFlags & WIF_POWERED_UP &&
			player->PendingWeapon->SisterWeapon != NULL)
		{
			player->PendingWeapon = player->PendingWeapon->SisterWeapon;
		}
	}
}

// Player Speed Trail (used by the Speed Powerup) ----------------------------

class APlayerSpeedTrail : public AActor
{
	DECLARE_CLASS (APlayerSpeedTrail, AActor)
public:
	void Tick ();
};

IMPLEMENT_CLASS (APlayerSpeedTrail)

//===========================================================================
//
// APlayerSpeedTrail :: Tick
//
//===========================================================================

void APlayerSpeedTrail::Tick ()
{
	const int fade = OPAQUE*6/10/8;
	if (alpha <= fade)
	{
		Destroy ();
	}
	else
	{
		alpha -= fade;
	}
}

// Speed Powerup -------------------------------------------------------------

IMPLEMENT_CLASS (APowerSpeed)

//===========================================================================
//
// APowerSpeed :: Serialize
//
//===========================================================================

void APowerSpeed::Serialize(FArchive &arc)
{
	Super::Serialize (arc);
	if (SaveVersion < 4146)
	{
		SpeedFlags = 0;
	}
	else
	{
		arc << SpeedFlags;
	}
}

//===========================================================================
//
// APowerSpeed :: GetSpeedFactor
//
//===========================================================================

fixed_t APowerSpeed ::GetSpeedFactor ()
{
	if (Inventory != NULL)
		return FixedMul(Speed, Inventory->GetSpeedFactor());
	else
		return Speed;
}

//===========================================================================
//
// APowerSpeed :: DoEffect
//
//===========================================================================

void APowerSpeed::DoEffect ()
{
	Super::DoEffect ();
	
	if (Owner == NULL || Owner->player == NULL)
		return;

	if ( CLIENT_PREDICT_IsPredicting( ))
		return;

	if (SpeedFlags & PSF_NOTRAIL)
		return;

	if (level.time & 1)
		return;

	// Check if another speed item is present to avoid multiple drawing of the speed trail.
	// Only the last PowerSpeed without PSF_NOTRAIL set will actually draw the trail.
	for (AInventory *item = Inventory; item != NULL; item = item->Inventory)
	{
		if (item->IsKindOf(RUNTIME_CLASS(APowerSpeed)) &&
			!(static_cast<APowerSpeed *>(item)->SpeedFlags & PSF_NOTRAIL))
		{
			return;
		}
	}

	if (P_AproxDistance (Owner->velx, Owner->vely) <= 12*FRACUNIT)
		return;
	// [BC] Skulltag powerups, such as the turbosphere, require to move at least SOME to
	// create a speed trail.
	else if (( Owner->velx == 0 ) &&
			 ( Owner->vely == 0 ))
	{
		return;
	}

	AActor *speedMo = Spawn<APlayerSpeedTrail> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
	if (speedMo)
	{
		speedMo->angle = Owner->angle;
		speedMo->Translation = Owner->Translation;
		speedMo->target = Owner;
		speedMo->sprite = Owner->sprite;
		speedMo->frame = Owner->frame;
		speedMo->floorclip = Owner->floorclip;

		// [BC] Also get the scale from the owner.
		speedMo->scaleX = Owner->scaleX;
		speedMo->scaleY = Owner->scaleY;

		// [BB] If the owner is a player, we also have to take into account the scaling of the skin.
		if ( Owner->player )
		{
			const int skinidx = Owner->player->userinfo.GetSkin();

			if (0 != skinidx && !(Owner->flags4 & MF4_NOSKIN))
			{
				// Apply skin's scale to actor's scale, it will be lost otherwise
				speedMo->scaleX = Scale(speedMo->scaleX, skins[skinidx].ScaleX, Owner->scaleX);
				speedMo->scaleY = Scale(speedMo->scaleY, skins[skinidx].ScaleY, Owner->scaleY);
			}
		}

		if (Owner == players[consoleplayer].camera &&
			!(Owner->player->cheats & CF_CHASECAM))
		{
			speedMo->renderflags |= RF_INVISIBLE;
		}
	}
}

// Minotaur (aka Dark Servant) powerup ---------------------------------------

IMPLEMENT_CLASS (APowerMinotaur)

// Targeter powerup ---------------------------------------------------------

IMPLEMENT_CLASS (APowerTargeter)

void APowerTargeter::Travelled ()
{
	InitEffect ();
}

void APowerTargeter::InitEffect ()
{
	player_t *player;

	Super::InitEffect();

	if ((player = Owner->player) == NULL)
		return;

	FState *state = FindState("Targeter");

	if (state != NULL)
	{
		P_SetPsprite (player, ps_targetcenter, state + 0);
		P_SetPsprite (player, ps_targetleft, state + 1);
		P_SetPsprite (player, ps_targetright, state + 2);
	}

	player->psprites[ps_targetcenter].sx = (160-3)*FRACUNIT;
	player->psprites[ps_targetcenter].sy =
		player->psprites[ps_targetleft].sy =
		player->psprites[ps_targetright].sy = (100-3)*FRACUNIT;
	PositionAccuracy ();
}

void APowerTargeter::DoEffect ()
{
	Super::DoEffect ();

	if (Owner != NULL && Owner->player != NULL)
	{
		player_t *player = Owner->player;

		PositionAccuracy ();
		if (EffectTics < 5*TICRATE)
		{
			FState *state = FindState("Targeter");

			if (state != NULL)
			{
				if (EffectTics & 32)
				{
					P_SetPsprite (player, ps_targetright, NULL);
					P_SetPsprite (player, ps_targetleft, state+1);
				}
				else if (EffectTics & 16)
				{
					P_SetPsprite (player, ps_targetright, state+2);
					P_SetPsprite (player, ps_targetleft, NULL);
				}
			}
		}
	}
}

void APowerTargeter::EndEffect ()
{
	Super::EndEffect();
	if (Owner != NULL && Owner->player != NULL)
	{
		P_SetPsprite (Owner->player, ps_targetcenter, NULL);
		P_SetPsprite (Owner->player, ps_targetleft, NULL);
		P_SetPsprite (Owner->player, ps_targetright, NULL);
	}
}

void APowerTargeter::PositionAccuracy ()
{
	player_t *player = Owner->player;

	if (player != NULL)
	{
		player->psprites[ps_targetleft].sx = (160-3)*FRACUNIT - ((100 - player->mo->accuracy) << FRACBITS);
		player->psprites[ps_targetright].sx = (160-3)*FRACUNIT + ((100 - player->mo->accuracy) << FRACBITS);
	}
}

// Frightener Powerup --------------------------------

IMPLEMENT_CLASS (APowerFrightener)

//===========================================================================
//
// APowerFrightener :: InitEffect
//
//===========================================================================

void APowerFrightener::InitEffect ()
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats |= CF_FRIGHTENING;
}

//===========================================================================
//
// APowerFrightener :: EndEffect
//
//===========================================================================

void APowerFrightener::EndEffect ()
{
	Super::EndEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats &= ~CF_FRIGHTENING;
}

// Scanner powerup ----------------------------------------------------------

IMPLEMENT_CLASS (APowerScanner)
// [BC] Start of new Skulltag powerup types.
// Time freezer powerup -----------------------------------------------------

IMPLEMENT_CLASS( APowerTimeFreezer)

//===========================================================================
//
// APowerTimeFreezer :: InitEffect
//
//===========================================================================

void APowerTimeFreezer::InitEffect()
{
	int freezemask;

	Super::InitEffect();

	if (Owner == NULL || Owner->player == NULL)
		return;

	// When this powerup is in effect, pause the music.
	S_PauseSound(false, false);

	// Give the player and his teammates the power to move when time is frozen.
	freezemask = 1 << (Owner->player - players);
	Owner->player->timefreezer |= freezemask;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] &&
			players[i].mo != NULL &&
			players[i].mo->IsTeammate(Owner)
		   )
		{
			players[i].timefreezer |= freezemask;
		}
	}

	// [RH] The effect ends one tic after the counter hits zero, so make
	// sure we start at an odd count.
	EffectTics += !(EffectTics & 1);
	if ((EffectTics & 1) == 0)
	{
		EffectTics++;
	}
	// Make sure the effect starts and ends on an even tic.
	if ((level.time & 1) == 0)
	{
		level.flags2 |= LEVEL2_FROZEN;
	}
	else
	{
		EffectTics++;
	}
}

//===========================================================================
//
// APowerTimeFreezer :: DoEffect
//
//===========================================================================

void APowerTimeFreezer::DoEffect()
{
	Super::DoEffect();
	// [RH] Do not change LEVEL_FROZEN on odd tics, or the Revenant's tracer
	// will get thrown off.
	// [ED850] Don't change it if the player is predicted either.
	// [BB] Adapted to Zandronums's prediction.
	if (level.time & 1 /*|| (Owner != NULL && Owner->player != NULL && Owner->player->cheats & CF_PREDICTING)*/)
	{
		return;
	}
	// [RH] The "blinking" can't check against EffectTics exactly or it will
	// never happen, because InitEffect ensures that EffectTics will always
	// be odd when level.time is even.
	if ( EffectTics > 4*32 
		|| (( EffectTics > 3*32 && EffectTics <= 4*32 ) && ((EffectTics + 1) & 15) != 0 )
		|| (( EffectTics > 2*32 && EffectTics <= 3*32 ) && ((EffectTics + 1) & 7) != 0 )
		|| (( EffectTics >   32 && EffectTics <= 2*32 ) && ((EffectTics + 1) & 3) != 0 )
		|| (( EffectTics >    0 && EffectTics <= 1*32 ) && ((EffectTics + 1) & 1) != 0 ))
		level.flags2 |= LEVEL2_FROZEN;
	else
		level.flags2 &= ~LEVEL2_FROZEN;
}

//===========================================================================
//
// APowerTimeFreezer :: EndEffect
//
//===========================================================================

void APowerTimeFreezer::EndEffect()
{
	int	i;

	Super::EndEffect();

	// If there is an owner, remove the timefreeze flag corresponding to
	// her from all players.
	if (Owner != NULL && Owner->player != NULL)
	{
		int freezemask = ~(1 << (Owner->player - players));
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			players[i].timefreezer &= freezemask;
		}
	}

	// Are there any players who still have timefreezer bits set?
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].timefreezer != 0)
		{
			break;
		}
	}

	if (i == MAXPLAYERS)
	{
		// No, so allow other actors to move about freely once again.
		level.flags2 &= ~LEVEL2_FROZEN;

		// Also, turn the music back on.
		S_ResumeSound(false);
	}
}

// Damage powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerDamage)

//===========================================================================
//
// APowerDamage :: InitEffect
//
//===========================================================================

void APowerDamage::InitEffect( )
{
	Super::InitEffect();

	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_Sound(Owner, 5, SeeSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerDamage :: EndEffect
//
//===========================================================================

void APowerDamage::EndEffect( )
{
	Super::EndEffect();
	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_Sound(Owner, 5, DeathSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerDamage :: ModifyDamage
//
//===========================================================================

void APowerDamage::ModifyDamage(int damage, FName damageType, int &newdamage, bool passive)
{
	static const fixed_t def = 4*FRACUNIT;
	if (!passive && damage > 0)
	{
		const fixed_t * pdf = NULL;
		DmgFactors * df = GetClass()->ActorInfo->DamageFactors;
		if (df != NULL && df->CountUsed() != 0)
		{
			pdf = df->CheckFactor(damageType);
		}
		else
		{
			pdf = &def;
		}
		if (pdf != NULL)
		{
			damage = newdamage = FixedMul(damage, *pdf);
			if (*pdf > 0 && damage == 0) damage = newdamage = 1;	// don't allow zero damage as result of an underflow
			if (Owner != NULL && *pdf > FRACUNIT) S_Sound(Owner, 5, ActiveSound, 1.0f, ATTN_NONE);
		}
	}
	if (Inventory != NULL) Inventory->ModifyDamage(damage, damageType, newdamage, passive);
}

// Quarter damage powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerProtection)

#define PROTECTION_FLAGS3	(MF3_NORADIUSDMG | MF3_DONTMORPH | MF3_DONTSQUASH | MF3_DONTBLAST | MF3_NOTELEOTHER)
#define PROTECTION_FLAGS5	(MF5_NOPAIN | MF5_DONTRIP)

//===========================================================================
//
// APowerProtection :: InitEffect
//
//===========================================================================

void APowerProtection::InitEffect( )
{
	Super::InitEffect();

	if (Owner != NULL)
	{
		S_Sound(Owner, CHAN_AUTO, SeeSound, 1.0f, ATTN_NONE);

		// Transfer various protection flags if owner does not already have them.
		// If the owner already has the flag, clear it from the powerup.
		// If the powerup still has a flag set, add it to the owner.
		flags3 &= ~(Owner->flags3 & PROTECTION_FLAGS3);
		Owner->flags3 |= flags3 & PROTECTION_FLAGS3;

		flags5 &= ~(Owner->flags5 & PROTECTION_FLAGS5);
		Owner->flags5 |= flags5 & PROTECTION_FLAGS5;
	}
}

//===========================================================================
//
// APowerProtection :: EndEffect
//
//===========================================================================

void APowerProtection::EndEffect( )
{
	Super::EndEffect();
	if (Owner != NULL)
	{
		S_Sound(Owner, CHAN_AUTO, DeathSound, 1.0f, ATTN_NONE);
		Owner->flags3 &= ~(flags3 & PROTECTION_FLAGS3);
		Owner->flags5 &= ~(flags5 & PROTECTION_FLAGS5);
	}
}

//===========================================================================
//
// APowerProtection :: AbsorbDamage
//
//===========================================================================

void APowerProtection::ModifyDamage(int damage, FName damageType, int &newdamage, bool passive)
{
	static const fixed_t def = FRACUNIT/4;
	if (passive && damage > 0)
	{
		const fixed_t *pdf = NULL;
		DmgFactors *df = GetClass()->ActorInfo->DamageFactors;
		if (df != NULL && df->CountUsed() != 0)
		{
			pdf = df->CheckFactor(damageType);
		}
		else pdf = &def;

		if (pdf != NULL)
		{
			damage = newdamage = FixedMul(damage, *pdf);
			if (Owner != NULL && *pdf < FRACUNIT) S_Sound(Owner, CHAN_AUTO, ActiveSound, 1.0f, ATTN_NONE, true);	// [EP] Inform the clients.
		}
	}
	if (Inventory != NULL)
	{
		Inventory->ModifyDamage(damage, damageType, newdamage, passive);
	}
}

// Drain rune -------------------------------------------------------

IMPLEMENT_CLASS(APowerDrain)

//===========================================================================
//
// ARuneDrain :: InitEffect
//
//===========================================================================

void APowerDrain::InitEffect( )
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player the power to drain life from opponents when he damages them.
	Owner->player->cheats |= CF_DRAIN;
}

//===========================================================================
//
// ARuneDrain :: EndEffect
//
//===========================================================================

void APowerDrain::EndEffect( )
{
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the drain power.
		Owner->player->cheats &= ~CF_DRAIN;
	}
}


// Regeneration rune -------------------------------------------------------

IMPLEMENT_CLASS(APowerRegeneration)

//===========================================================================
//
// APowerRegeneration :: DoEffect
//
//===========================================================================

void APowerRegeneration::DoEffect()
{
	// [BB] This is server side.
	if ( NETWORK_InClientMode() )
		return;

	if (Owner != NULL && Owner->health > 0 && (level.time & 31) == 0)
	{
		if (P_GiveBody(Owner, Strength/FRACUNIT))
		{
			// [BC] If we're the server, send out the health change.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			{
				if ( Owner->player )
					SERVERCOMMANDS_SetPlayerHealth( Owner->player - players );
			}

			S_Sound(Owner, CHAN_ITEM, "*regenerate", 1, ATTN_NORM, true );	// [BC] Inform the clients.
		}
	}
}

// High jump rune -------------------------------------------------------

IMPLEMENT_CLASS(APowerHighJump)

//===========================================================================
//
// ARuneHighJump :: InitEffect
//
//===========================================================================

void APowerHighJump::InitEffect( )
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player the power to jump much higher.
	Owner->player->cheats |= CF_HIGHJUMP;
}

//===========================================================================
//
// ARuneHighJump :: EndEffect
//
//===========================================================================

void APowerHighJump::EndEffect( )
{
	Super::EndEffect();
	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the high jump power.
		Owner->player->cheats &= ~CF_HIGHJUMP;
	}
}

// Double firing speed rune ---------------------------------------------

IMPLEMENT_CLASS(APowerDoubleFiringSpeed)

//===========================================================================
//
// APowerDoubleFiringSpeed :: InitEffect
//
//===========================================================================

void APowerDoubleFiringSpeed::InitEffect( )
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player the power to shoot twice as fast.
	Owner->player->cheats |= CF_DOUBLEFIRINGSPEED;
}

//===========================================================================
//
// APowerDoubleFiringSpeed :: EndEffect
//
//===========================================================================

void APowerDoubleFiringSpeed::EndEffect( )
{
	Super::EndEffect();
	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the shooting twice as fast power.
		Owner->player->cheats &= ~CF_DOUBLEFIRINGSPEED;
	}
}

// Morph powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerMorph)

//===========================================================================
//
// APowerMorph :: Serialize
//
//===========================================================================

void APowerMorph::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PlayerClass << MorphStyle << MorphFlash << UnMorphFlash;
	arc << Player;
}

//===========================================================================
//
// APowerMorph :: InitEffect
//
//===========================================================================

void APowerMorph::InitEffect( )
{
	Super::InitEffect();

	if (Owner != NULL && Owner->player != NULL && PlayerClass != NAME_None)
	{
		player_t *realplayer = Owner->player;	// Remember the identity of the player
		const PClass *morph_flash = PClass::FindClass (MorphFlash);
		const PClass *unmorph_flash = PClass::FindClass (UnMorphFlash);
		const PClass *player_class = PClass::FindClass (PlayerClass);
		if (P_MorphPlayer(realplayer, realplayer, player_class, -1/*INDEFINITELY*/, MorphStyle, morph_flash, unmorph_flash))
		{
			Owner = realplayer->mo;				// Replace the new owner in our owner; safe because we are not attached to anything yet
			ItemFlags |= IF_CREATECOPYMOVED;	// Let the caller know the "real" owner has changed (to the morphed actor)
			Player = realplayer;				// Store the player identity (morphing clears the unmorphed actor's "player" field)
		}
		else // morph failed - give the caller an opportunity to fail the pickup completely
		{
			ItemFlags |= IF_INITEFFECTFAILED;	// Let the caller know that the activation failed (can fail the pickup if appropriate)
		}
	}
}

//===========================================================================
//
// APowerMorph :: EndEffect
//
//===========================================================================

void APowerMorph::EndEffect( )
{
	Super::EndEffect();

	// Abort if owner already destroyed
	if (Owner == NULL)
	{
		assert(Player == NULL);
		return;
	}
	
	// Abort if owner already unmorphed
	if (Player == NULL)
	{
		return;
	}

	// Abort if owner is dead; their Die() method will
	// take care of any required unmorphing on death.
	if (Player->health <= 0)
	{
		return;
	}

	// Unmorph if possible
	if (!bNoCallUndoMorph)
	{
		int savedMorphTics = Player->morphTics;
		P_UndoPlayerMorph (Player, Player);

		// Abort if unmorph failed; in that case,
		// set the usual retry timer and return.
		if (Player != NULL && Player->morphTics)
		{
			// Transfer retry timeout
			// to the powerup's timer.
			EffectTics = Player->morphTics;
			// Reload negative morph tics;
			// use actual value; it may
			// be in use for animation.
			Player->morphTics = savedMorphTics;
			// Try again some time later
			return;
		}
	}
	// Unmorph suceeded
	Player = NULL;
}

//===========================================================================
//
// AReturningPowerupGiver
//
// [BB] Base class for the Terminator and PossessionStone that handles
// the automatic return of these objects to a spawn point.
//===========================================================================

CVAR( Int, sv_artifactreturntime, 30, CVAR_SERVERINFO );

class AReturningPowerupGiver : public APowerupGiver
{
	DECLARE_CLASS (AReturningPowerupGiver, APowerupGiver)

	int ReturnTics;
public:
	virtual void BeginPlay () {
		Super::BeginPlay ();
		ReturnTics = sv_artifactreturntime * TICRATE;
	}
	virtual void Tick () {
		Super::Tick ();
		if ( NETWORK_InClientMode() )
			return;

		if ( ReturnTics > 0 )
		{
			ReturnTics--;
			if ( ReturnTics == 0 )
			{
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DestroyThing ( this );
				
				Destroy();
				if ( this->IsKindOf ( PClass::FindClass( "Terminator" ) ) )
					GAME_SpawnTerminatorArtifact( );
				else if ( this->IsKindOf ( PClass::FindClass( "PossessionStone" ) ) )
					GAME_SpawnPossessionArtifact( );
			}
		}
	}
};

IMPLEMENT_CLASS (AReturningPowerupGiver)

// Possession artifact powerup -------------------------------------------------

IMPLEMENT_CLASS( APowerPossessionArtifact )

//===========================================================================
//
// APowerPossessionArtifact :: InitEffect
//
//===========================================================================

void APowerPossessionArtifact::InitEffect( )
{
	Super::InitEffect();

	// Flag the player as carrying the possession artifact.
	Owner->player->cheats2 |= CF2_POSSESSIONARTIFACT;

	// Tell the possession module that the artifact has been picked up.
	if (( possession || teampossession ) &&
		( NETWORK_InClientMode() == false ))
	{
		POSSESSION_ArtifactPickedUp( Owner->player, sv_possessionholdtime * TICRATE );
	}
}

//===========================================================================
//
// APowerPossessionArtifact :: DoEffect
//
//===========================================================================

void APowerPossessionArtifact::DoEffect( )
{
	Super::DoEffect ();
	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// When the player carries the possession artifact, the player's weapon
	// must always be lowered.
	P_SetPsprite( Owner->player, ps_weapon, NULL );
}

//===========================================================================
//
// APowerPossessionArtifact :: EndEffect
//
//===========================================================================

void APowerPossessionArtifact::EndEffect( )
{
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the possession artifact flag.
	Owner->player->cheats2 &= ~CF2_POSSESSIONARTIFACT;
}

// Terminator artifact powerup -------------------------------------------------

IMPLEMENT_CLASS( APowerTerminatorArtifact )

//===========================================================================
//
// APowerTerminatorArtifact :: InitEffect
//
//===========================================================================

void APowerTerminatorArtifact::InitEffect( )
{
	Super::InitEffect();

	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
		return;

	// Flag the player as carrying the terminator artifact.
	Owner->player->cheats2 |= CF2_TERMINATORARTIFACT;

	// Also, give the player a megasphere as part of the bonus.
	// [BB] The server handles giving the megasphere.
	if ( NETWORK_InClientMode() == false )
	{
		AInventory *pGivenInventory = Owner->GiveInventoryType( PClass::FindClass( "Megasphere" ));
		if ( pGivenInventory && (NETWORK_GetState( ) == NETSTATE_SERVER) )
			SERVERCOMMANDS_GiveInventory( ULONG( Owner->player - players ), pGivenInventory );
	}
}

//===========================================================================
//
// APowerTerminatorArtifact :: DoEffect
//
//===========================================================================

void APowerTerminatorArtifact::DoEffect( )
{
	Super::DoEffect ();
}

//===========================================================================
//
// APowerTerminatorArtifact :: EndEffect
//
//===========================================================================

void APowerTerminatorArtifact::EndEffect( )
{
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the terminator artifact flag.
	Owner->player->cheats2 &= ~CF2_TERMINATORARTIFACT;
}

//===========================================================================
//
// APowerTerminatorArtifact :: ModifyDamage
//
//===========================================================================

void APowerTerminatorArtifact::ModifyDamage( int damage, FName damageType, int &newdamage, bool passive )
{
	if (( passive == false ) &&
		( damage > 0 ))
	{
		damage = newdamage = damage * 4;
	}

	// Go onto the next item.
	if ( Inventory != NULL )
		Inventory->ModifyDamage( damage, damageType, newdamage, passive );
}

// Translucency Powerup (Skulltag's version of invisibility) ----------------

IMPLEMENT_CLASS( APowerTranslucency )

//===========================================================================
//
// APowerTranslucency :: InitEffect
//
//===========================================================================

EXTERN_CVAR (Bool, r_drawtrans)
void APowerTranslucency::InitEffect( )
{
	Super::InitEffect();

	// [BC] If r_drawtrans is false, then just give the same effect as partial invisibility.
	if ( r_drawtrans == false )
	{
		// [BB] Check if the CommonInit removal needs any changes here.
		//CommonInit();
		Owner->alpha = FRACUNIT/5;
		Owner->RenderStyle = STYLE_OptFuzzy;
	}
	else
	{
		Owner->alpha = ( FRACUNIT / 10 );
		Owner->RenderStyle = STYLE_Translucent;
	}
}

// Rune-Giver -------------------------------------------------------------

//===========================================================================
//
// ARuneGiver :: PowerupGranted
//
//===========================================================================

void ARuneGiver::PowerupGranted ( APowerup* power )
{
	// If the owner already has a rune, remove that now.
	if ( Owner->Rune != NULL )
		Owner->Rune->Destroy();

	Owner->Rune = power;
}

//===========================================================================
//
// ARuneGiver :: ModifyPowerup
//
//===========================================================================

void ARuneGiver::ModifyPowerup ( APowerup* power )
{
	power->EffectTics = ( INT_MAX - 1 );
	power->ItemFlags |= IF_PERSISTENTPOWER;
	power->ItemFlags &= ~IF_ALWAYSPICKUP;
	power->Icon = Icon;
}

// Prosperity rune -------------------------------------------------------

IMPLEMENT_CLASS( APowerProsperity )

//===========================================================================
//
// APowerProsperity :: InitEffect
//
//===========================================================================

void APowerProsperity::InitEffect( )
{
	Super::InitEffect();

	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Give the player the power to pickup base health artifacts past 100%.
	Owner->player->cheats |= CF_PROSPERITY;
}

//===========================================================================
//
// APowerProsperity :: EndEffect
//
//===========================================================================

void APowerProsperity::EndEffect( )
{
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the prosperity power.
	Owner->player->cheats &= ~CF_PROSPERITY;
}

// Reflection rune -------------------------------------------------------

IMPLEMENT_CLASS( APowerReflection )

//===========================================================================
//
// APowerReflection :: InitEffect
//
//===========================================================================

void APowerReflection::InitEffect()
{
	Super::InitEffect();

	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Give the player the power to reflect damage back at their attacker.
	Owner->player->cheats |= CF_REFLECTION;
}

//===========================================================================
//
// APowerReflection :: EndEffect
//
//===========================================================================

void APowerReflection::EndEffect()
{
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the reflection power.
	Owner->player->cheats &= ~CF_REFLECTION;
}

// Spread rune -------------------------------------------------------

IMPLEMENT_CLASS( APowerSpread )

//===========================================================================
//
// APowerSpread :: InitEffect
//
//===========================================================================

void APowerSpread::InitEffect( )
{
	Super::InitEffect();

	if ( Owner && Owner->player )
		Owner->player->cheats2 |= CF2_SPREAD;
}

//===========================================================================
//
// APowerSpread :: EndEffect
//
//===========================================================================

void APowerSpread::EndEffect( )
{
	Super::EndEffect();

	if ( Owner && Owner->player )
		Owner->player->cheats2 &= ~CF2_SPREAD;
}

// Infinite Ammo Powerup -----------------------------------------------------

IMPLEMENT_CLASS(APowerInfiniteAmmo)

//===========================================================================
//
// APowerInfiniteAmmo :: InitEffect
//
//===========================================================================

void APowerInfiniteAmmo::InitEffect( )
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player infinite ammo
	Owner->player->cheats |= CF_INFINITEAMMO;
}

//===========================================================================
//
// APowerInfiniteAmmo :: EndEffect
//
//===========================================================================

void APowerInfiniteAmmo::EndEffect( )
{
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the limitless ammo
		Owner->player->cheats &= ~CF_INFINITEAMMO;
	}
}

