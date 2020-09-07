//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2014-2015 Teemu Piippo
// Copyright (C) 2014-2015 Zandronum Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Zandronum Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: pwo.cpp
//
//-----------------------------------------------------------------------------

// These directives must come before #include "optionmenuitems.h" line.
#define NO_IMP
#include <float.h>
#include "menu/menu.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "v_palette.h"
#include "d_event.h"
#include "c_bind.h"
#include "gi.h"
#include "menu/optionmenuitems.h"

#include "g_shared/pwo.h"
#include "d_player.h"
#include "gameconfigfile.h"
#include "network.h"

CVAR ( Bool, pwo_switchonsameweight, true, CVAR_ARCHIVE )
CVAR ( Bool, pwo_switchonunknown, false, CVAR_ARCHIVE )

static TDeletingArray<PWOWeaponInfo*> WeaponInfo;
static TMap<const PClass*, PWOWeaponInfo*> WeaponInfoPerClass;
bool ParsingPWO;

// =================================================================================================
//
// PWO_Init
//
// Initializes PWO data.
//
// =================================================================================================

void PWO_Init()
{
	Printf( "PWO_Init: Initializing preferred weapon order.\n" );

	ParsingPWO = true;

	// Collect weapons and store class references. PWO_GetWeaponsForClass will initialize weapons
	// before returning them.
	for ( unsigned int i = 0; i < PlayerClasses.Size(); ++i )
	{
		TArray<PWOWeaponInfo*> weapons = PWO_GetWeaponsForClass( &PlayerClasses[i], false );

		for ( unsigned int j = 0; j < weapons.Size(); ++j )
			weapons[j]->BumpReferenceCount();
	}

	// Read preferences from the ini. This cannot be done when the ini loads because that
	// happens before WADs are loaded.
	GameConfig->ReadPWO( GameNames[gameinfo.gametype] );

	ParsingPWO = false;
}

// =================================================================================================
//
//
//
//
// =================================================================================================

PWOWeaponInfo::PWOWeaponInfo ( const PClass* type ) :
	WeaponClass ( type ),
	ReferenceCount ( 0 ),
	Weight ( 0.0f ),
	Name ( GetDefaultByType( type )->GetTag( type->TypeName )),
	// The PWO of the weapon is stored by the name of the replacee of the weapon. This way,
	// when playing a mod that replaces a stock weapon, the stock weapon and the mod-replaced
	// weapon will share PWO values.
	//
	// Technically this could be extended further so that weapons replaced by a random spawner
	// have their PWO shared with the weapon replaced with. But this whole thing is assuming that
	// weapons replaced directly by a mod are similar enough so that they can share PWO, but weapons
	// replaced by random spawner such are usually distinct enough that one wants separate entries
	// for each weapon.
	IniName ( type->ActorInfo->GetReplacee()->Class->TypeName ) {}

// =================================================================================================
//
// PWO_FindInfoForWeapon
//
// Finds PWO info for a weapon. Returns NULL if not found.
//
// =================================================================================================

PWOWeaponInfo* PWOWeaponInfo::FindInfo ( const PClass* type )
{
	PWOWeaponInfo** infop = WeaponInfoPerClass.CheckKey( type );
	return infop ? *infop : NULL;
}

// =================================================================================================
//
// PWO_AddWeapon
//
// Creates PWO info for the given weapon.
//
// =================================================================================================

static PWOWeaponInfo* PWO_AddWeapon ( const PClass* type )
{
	assert ( WeaponInfoPerClass.CheckKey( type ) == NULL );
	WeaponInfo.Push( new PWOWeaponInfo( type ) );
	WeaponInfoPerClass[type] = WeaponInfo.Last();
	return WeaponInfo.Last();
}

// =================================================================================================
//
// PWO_SetWeaponWeight
//
// Sets the weight of the given weapon name to the given weight.
//
// =================================================================================================

void PWO_SetWeaponWeight ( const char* name, int weight )
{
	const PClass* type = PClass::FindClass( name );

	if ( type != NULL )
	{
		type = type->ActorInfo->GetReplacement()->Class;
		weight = clamp( weight, -10, 10 );
		PWOWeaponInfo* info = PWOWeaponInfo::FindInfo( type );
	
		if ( info != NULL )
			*info->GetWeightVar() = static_cast<float>( weight );
	}
}

// =================================================================================================
//
// PWO_ArchivePreferences
//
// Writes preferences to ini.
//
// =================================================================================================

void PWO_ArchivePreferences ( FConfigFile* config )
{
	for ( unsigned int i = 0; i < WeaponInfo.Size(); ++i )
	{
		PWOWeaponInfo* info = WeaponInfo[i];

		if ( info->GetWeight() != 0 )
		{
			FString valueString;
			valueString.Format( "%d", info->GetWeight() );
			config->SetValueForKey( info->GetIniName(), valueString );
		}
	}
}

// =================================================================================================
//
// PWO_GetWeaponsForClass
//
// Gets all the weapons that PWO is considered for the given player class.
//
// If uniqueOnly is true, only weapons unique to this player class are given (i.e. weapons
// that this function would return for multiple player classes are left out).
//
// If PWO info does not exist for a weapon in the player's slots, it is created here.
//
// =================================================================================================

TArray<PWOWeaponInfo*> PWO_GetWeaponsForClass ( const FPlayerClass* playerclass, bool uniqueOnly )
{
	TArray<PWOWeaponInfo*> result;
	FWeaponSlots weaponSlots;
	weaponSlots.StandardSetup( playerclass->Type );
	weaponSlots.LocalSetup( playerclass->Type );

	for ( int slot = 0; slot < NUM_WEAPON_SLOTS; ++slot )
	for ( int rank = 0; rank < weaponSlots.Slots[slot].Size(); ++rank )
	{
		const PClass* type = weaponSlots.Slots[slot].GetWeapon( rank );

		if ( type != type->ActorInfo->GetReplacement()->Class )
			continue; // replaced by something else

		PWOWeaponInfo* info = PWOWeaponInfo::FindInfo( type );

		if ( info == NULL )
			info = PWO_AddWeapon( type );

		if ( uniqueOnly && info->IsShared() )
			continue;

		result.Push( info );
	}

	return result;
}

// =================================================================================================
//
// PWO_AddWeaponToMenu
//
// Adds one weapon slider to the menu
//
// =================================================================================================

static void PWO_AddWeaponToMenu ( FOptionMenuDescriptor& descriptor, PWOWeaponInfo* weapon )
{
	descriptor.mItems.Push( new FOptionMenuSliderVar( weapon->GetName(), weapon->GetWeightVar(),
		-10.0, 10.0, 1.0, 1 ));
}

// =================================================================================================
//
// PWO_FillMenu
//
// Fills the given menu descriptor with PWO-related items.
//
// =================================================================================================

void PWO_FillMenu ( FOptionMenuDescriptor& descriptor )
{
	TArray<FOptionMenuItem*>& items = descriptor.mItems;

	items.Push( new FOptionMenuItemStaticText( " ", true ));
	items.Push( new FOptionMenuItemStaticText( "PREFERRED WEAPON ORDER", true ));
	items.Push( new FOptionMenuItemOption( "Switch with same weight", "pwo_switchonsameweight", "YesNo", NULL, false ));
	items.Push( new FOptionMenuItemOption( "Switch to unknown weapons", "pwo_switchonunknown", "YesNo", NULL, false ));

	for ( unsigned int i = 0; i < WeaponInfo.Size(); ++i )
	{
		PWOWeaponInfo* info = WeaponInfo[i];

		if ( info->IsShared() )
			PWO_AddWeaponToMenu( descriptor, info );
	}

	// Add player-class specific weapons
	for ( unsigned int classIndex = 0; classIndex < PlayerClasses.Size(); ++classIndex )
	{
		TArray<PWOWeaponInfo*> weapons = PWO_GetWeaponsForClass( &PlayerClasses[classIndex], true );

		if ( weapons.Size() == 0 )
			continue; // no fitting weapons

		// Add a separator
		items.Push( new FOptionMenuItemStaticText( " ", true ));

		// Add a label for the class name, however it doesn't make sense to have labels
		// for classes when there's one class around.
		if ( PlayerClasses.Size() > 1 )
		{
			const char* label = GetPrintableDisplayName( PlayerClasses[classIndex].Type );
			items.Push( new FOptionMenuItemStaticText( label, true ));
		}

		// Add weapons
		for ( unsigned int i = 0; i < weapons.Size(); ++i )
			PWO_AddWeaponToMenu( descriptor, weapons[i] );
	}
}

// =================================================================================================
//
// PWO_IsActive
//
// Does PWO take effect for the given player?
//
// =================================================================================================

bool PWO_IsActive ( player_t* player )
{
	return ( NETWORK_GetState() != NETSTATE_SERVER )
		&& ( player->userinfo.GetSwitchOnPickup() == 3 )
		&& ( player == &players[consoleplayer] );
}

// =================================================================================================
//
// PWO_ShouldSwitch
//
// Should the player switch from the compareWeapon to the pendingWeapon?
//
// =================================================================================================

bool PWO_ShouldSwitch ( AWeapon* currentWeapon, AWeapon* testWeapon )
{
	if ( currentWeapon == NULL )
		return true;

	PWOWeaponInfo* compareInfo = PWOWeaponInfo::FindInfo( currentWeapon->GetClass() );
	PWOWeaponInfo* pendingInfo = PWOWeaponInfo::FindInfo( testWeapon->GetClass() );

	if ( compareInfo == NULL || pendingInfo == NULL )
		return pwo_switchonunknown;

	if ( pendingInfo->GetWeight() == compareInfo->GetWeight() )
		return pwo_switchonsameweight;

	return pendingInfo->GetWeight() > compareInfo->GetWeight();
}
