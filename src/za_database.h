//-----------------------------------------------------------------------------
//
// Zandronum Source
// Copyright (C) 2014 Benjamin Berkels
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
// Filename: za_database.h
//
//-----------------------------------------------------------------------------

#include "c_cvars.h"
#include "c_dispatch.h"
#include <utility>

//*****************************************************************************
//	PROTOTYPES

void	DATABASE_Construct ( void );
void	DATABASE_Destruct ( void );
void	DATABASE_Init ( void );
bool	DATABASE_IsAvailable ( const char *CallingFunction = NULL );
void	DATABASE_SetMaxPageCount ( const unsigned int MaxPageCount );
void	DATABASE_BeginTransaction ( void );
void	DATABASE_EndTransaction ( void );
void	DATABASE_CreateTable ( );
void	DATABASE_ClearTable ( );
void	DATABASE_DeleteTable ( );
void	DATABASE_DumpTable ( );
void	DATABASE_DumpNamespace ( const char *Namespace );
void	DATABASE_AddEntry ( const char *Namespace, const char *EntryName, const char *EntryValue );
void	DATABASE_SetEntry ( const char *Namespace, const char *EntryName, const char *EntryValue );
FString	DATABASE_GetEntry ( const char *Namespace, const char *EntryName );
bool	DATABASE_EntryExists ( const char *Namespace, const char *EntryName );
void	DATABASE_DeleteEntry ( const char *Namespace, const char *EntryName );
void	DATABASE_SaveSetEntry ( const char *Namespace, const char *EntryName, const char *EntryValue );
void	DATABASE_SaveSetEntryInt ( const char *Namespace, const char *EntryName, int EntryValue );
FString	DATABASE_SaveGetEntry ( const char *Namespace, const char *EntryName );
void	DATABASE_SaveIncrementEntryInt ( const char *Namespace, const char *EntryName, int Increment );
int		DATABASE_GetEntryRank ( const char *Namespace, const char *EntryName, const bool Descending );
int		DATABASE_GetSortedEntries ( const char *Namespace, const int N, const int Offset, const bool Descending, TArray<std::pair<FString, FString> > &Entries );
int		DATABASE_GetEntries ( const char *Namespace, TArray<std::pair<FString, FString> > &Entries );
