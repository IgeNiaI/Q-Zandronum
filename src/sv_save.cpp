//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2003-2005 Brad Carney
// Copyright (C) 2007-2012 Skulltag Development Team
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
// 3. Neither the name of the Skulltag Development Team nor the names of its
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
// File created:  3/26/05
//
//
// Filename: sv_save.cpp
//
// Description: Saves players' scores when they leave the server, and restores it when they return.
//
//-----------------------------------------------------------------------------

#include "network.h"
#include "sv_save.h"
#include "v_text.h"

//*****************************************************************************
//	VARIABLES

// Global list of saved information.
static	PLAYERSAVEDINFO_t	g_SavedPlayerInfo[MAXPLAYERS];

//*****************************************************************************
//	PROTOTYPES

void	server_save_UpdateSlotWithInfo( ULONG ulSlot, PLAYERSAVEDINFO_t *pInfo );

//*****************************************************************************
//	FUNCTIONS

void SERVER_SAVE_Construct( void )
{
	// Initialzed the saved player info list.
	SERVER_SAVE_ClearList( );
}

//*****************************************************************************
//
PLAYERSAVEDINFO_t *SERVER_SAVE_GetSavedInfo( const char *pszPlayerName, NETADDRESS_s Address )
{
	ULONG	ulIdx;
	char	szPlayerName[128];

	sprintf( szPlayerName, "%s", pszPlayerName );
	V_RemoveColorCodes( szPlayerName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( g_SavedPlayerInfo[ulIdx].bInitialized == false )
			continue;

		if (( stricmp( szPlayerName, g_SavedPlayerInfo[ulIdx].szName ) == 0 ) &&
			Address.Compare( g_SavedPlayerInfo[ulIdx].Address ))
		{
			return ( &g_SavedPlayerInfo[ulIdx] );
		}
	}

	return ( NULL );
}

//*****************************************************************************
//
void SERVER_SAVE_ClearList( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		g_SavedPlayerInfo[ulIdx].Address.abIP[0] = 0;
		g_SavedPlayerInfo[ulIdx].Address.abIP[1] = 0;
		g_SavedPlayerInfo[ulIdx].Address.abIP[2] = 0;
		g_SavedPlayerInfo[ulIdx].Address.abIP[3] = 0;
		g_SavedPlayerInfo[ulIdx].Address.usPort = 0;
		g_SavedPlayerInfo[ulIdx].bInitialized = false;
		g_SavedPlayerInfo[ulIdx].lFragCount = 0;
		g_SavedPlayerInfo[ulIdx].lPointCount = 0;
		g_SavedPlayerInfo[ulIdx].lWinCount = 0;
		g_SavedPlayerInfo[ulIdx].szName[0] = 0;
		g_SavedPlayerInfo[ulIdx].ulTime = 0;
	}
}

//*****************************************************************************
//
void SERVER_SAVE_SaveInfo( PLAYERSAVEDINFO_t *pInfo )
{
	ULONG	ulIdx;
	char	szPlayerName[128];

	sprintf( szPlayerName, "%s", pInfo->szName );
	V_RemoveColorCodes( szPlayerName );

	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( g_SavedPlayerInfo[ulIdx].bInitialized )
		{
			// If this slot matches the player we're trying to save, just update it.
			if (( stricmp( szPlayerName, g_SavedPlayerInfo[ulIdx].szName ) == 0 ) &&
				pInfo->Address.Compare( g_SavedPlayerInfo[ulIdx].Address ))
			{
				server_save_UpdateSlotWithInfo( ulIdx, pInfo );
				return;
			}

			continue;
		}

		server_save_UpdateSlotWithInfo( ulIdx, pInfo );
		return;
	}
}

//*****************************************************************************
//*****************************************************************************
//
void server_save_UpdateSlotWithInfo( ULONG ulSlot, PLAYERSAVEDINFO_t *pInfo )
{
	if ( ulSlot >= MAXPLAYERS )
		return;

	g_SavedPlayerInfo[ulSlot].bInitialized		= true;
	g_SavedPlayerInfo[ulSlot].Address			= pInfo->Address;
	g_SavedPlayerInfo[ulSlot].lFragCount		= pInfo->lFragCount;
	g_SavedPlayerInfo[ulSlot].lPointCount		= pInfo->lPointCount;
	g_SavedPlayerInfo[ulSlot].lWinCount			= pInfo->lWinCount;
	g_SavedPlayerInfo[ulSlot].ulTime			= pInfo->ulTime;
	sprintf( g_SavedPlayerInfo[ulSlot].szName, "%s", pInfo->szName );

	V_RemoveColorCodes( g_SavedPlayerInfo[ulSlot].szName );
}
