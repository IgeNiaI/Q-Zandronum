//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2014 Teemu Piippo
// Copyright (C) 2014 Zandronum Development Team
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
// Filename: pwo.h
//
//-----------------------------------------------------------------------------

#pragma once
#include "g_shared/a_pickups.h"

class PWOWeaponInfo
{
public:
	PWOWeaponInfo ( const PClass* type );
	
	void BumpReferenceCount() { ReferenceCount++; }
	const PClass* GetClass() const { return WeaponClass; }
	FName GetIniName() const { return IniName; }
	const char* GetName() const { return Name; }
	unsigned int GetReferenceCount() const { return ReferenceCount; }
	float* GetWeightVar() { return &Weight; }
	int GetWeight() { return static_cast<int>( Weight ); }
	bool IsShared() const { return ReferenceCount != 1; }

	bool operator== ( const PClass* type ) const
	{
		return WeaponClass == type->ActorInfo->GetReplacee()->Class;
	}

	static PWOWeaponInfo* FindInfo ( const PClass* type );

private:
	PWOWeaponInfo( const PWOWeaponInfo& ) {}

	const PClass* WeaponClass;
	unsigned int ReferenceCount;
	float Weight;
	FString Name;
	FName IniName;
};

// -------------------------------------------------------------------------------------------------
//
void PWO_Init();

void PWO_ArchivePreferences ( FConfigFile* config );
void PWO_FillMenu ( struct FOptionMenuDescriptor& descriptor );
TArray<PWOWeaponInfo*> PWO_GetWeaponsForClass ( const class FPlayerClass* playerclass, bool uniqueOnly = false );
bool PWO_IsActive ( player_t* player );
void PWO_SetWeaponWeight ( const char* name, int weight );
bool PWO_ShouldSwitch ( AWeapon* currentWeapon, AWeapon* testWeapon );