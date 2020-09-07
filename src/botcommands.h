//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2002 Brad Carney
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
// Date created:  5/18/04
//
//
// Filename: botcommands.h
//
// Description: Contains bot structures and prototypes
//
//-----------------------------------------------------------------------------

#ifndef __BOTCOMMANDS_H__
#define __BOTCOMMANDS_H__

#include "bots.h"

//*****************************************************************************
//  DEFINES

#define	SETENEMY_LASTSEEN		0
#define	SETENEMY_LASTSHOTBY		1

// Different results for pathing commands.
#define	PATH_UNREACHABLE			-1
#define	PATH_INCOMPLETE				0
#define	PATH_COMPLETE				1
#define	PATH_REACHEDGOAL			2

// This is the size of the return string for the bot command functions.
#define	BOTCMD_RETURNSTRING_SIZE	256

//*****************************************************************************
typedef enum
{
	BOTCMD_CHANGESTATE,									// Basic botcmd utility functions.
	BOTCMD_DELAY,
	BOTCMD_RAND,
	BOTCMD_STRINGSAREEQUAL,
	BOTCMD_LOOKFORPOWERUPS,								// Search functions.
	BOTCMD_LOOKFORWEAPONS,
	BOTCMD_LOOKFORAMMO,
	BOTCMD_LOOKFORBASEHEALTH,
	BOTCMD_LOOKFORBASEARMOR,
	BOTCMD_LOOKFORSUPERHEALTH,
	BOTCMD_LOOKFORSUPERARMOR,				/* 10 */
	BOTCMD_LOOKFORPLAYERENEMIES,
	BOTCMD_GETCLOSESTPLAYERENEMY,
	BOTCMD_MOVELEFT,									// Movement functions.
	BOTCMD_MOVERIGHT,
	BOTCMD_MOVEFORWARD,
	BOTCMD_MOVEBACKWARDS,
	BOTCMD_STOPMOVEMENT,
	BOTCMD_STOPFORWARDMOVEMENT,
	BOTCMD_STOPSIDEWAYSMOVEMENT,
	BOTCMD_CHECKTERRAIN,					/* 20 */
	BOTCMD_PATHTOGOAL,									// Pathing functions.
	BOTCMD_PATHTOLASTKNOWNENEMYPOSITION,
	BOTCMD_PATHTOLASTHEARDSOUND,
	BOTCMD_ROAM,
	BOTCMD_GETPATHINGCOSTTOITEM,
	BOTCMD_GETDISTANCETOITEM,
	BOTCMD_GETITEMNAME,
	BOTCMD_ISITEMVISIBLE,
	BOTCMD_SETGOAL,
	BOTCMD_BEGINAIMINGATENEMY,				/* 30 */	// Aiming functions.
	BOTCMD_STOPAIMINGATENEMY,
	BOTCMD_TURN,
	BOTCMD_GETCURRENTANGLE,
	BOTCMD_SETENEMY,									// Enemy functions.
	BOTCMD_CLEARENEMY,
	BOTCMD_ISENEMYALIVE,
	BOTCMD_ISENEMYVISIBLE,
	BOTCMD_GETDISTANCETOENEMY,
	BOTCMD_GETPLAYERDAMAGEDBY,
	BOTCMD_GETENEMYINVULNERABILITYTICKS,	/* 40 */
	BOTCMD_FIREWEAPON,									// Weapon functions.
	BOTCMD_BEGINFIRINGWEAPON,
	BOTCMD_STOPFIRINGWEAPON,
	BOTCMD_GETCURRENTWEAPON,
	BOTCMD_CHANGEWEAPON,
	BOTCMD_GETWEAPONFROMITEM,
	BOTCMD_ISWEAPONOWNED,
	BOTCMD_ISFAVORITEWEAPON,
	BOTCMD_SAY,											// Chat functions.
	BOTCMD_SAYFROMFILE,						/* 50 */
	BOTCMD_SAYFROMCHATFILE,
	BOTCMD_BEGINCHATTING,
	BOTCMD_STOPCHATTING,
	BOTCMD_CHATSECTIONEXISTS,
	BOTCMD_CHATSECTIONEXISTSINFILE,
	BOTCMD_GETLASTCHATSTRING,
	BOTCMD_GETLASTCHATPLAYER,
	BOTCMD_GETCHATFREQUENCY,
	BOTCMD_JUMP,										// Jumping functions.
	BOTCMD_BEGINJUMPING,					/* 60 */
	BOTCMD_STOPJUMPING,
	BOTCMD_TAUNT,										// Other action functions.
	BOTCMD_RESPAWN,
	BOTCMD_TRYTOJOINGAME,
	BOTCMD_ISDEAD,										// Information about self functions.
	BOTCMD_ISSPECTATING,
	BOTCMD_GETHEALTH,
	BOTCMD_GETARMOR,
	BOTCMD_GETBASEHEALTH,
	BOTCMD_GETBASEARMOR,					/* 70 */
	BOTCMD_GETBOTSKILL,									// Botskill functions.
	BOTCMD_GETACCURACY,
	BOTCMD_GETINTELLECT,
	BOTCMD_GETANTICIPATION,
	BOTCMD_GETEVADE,
	BOTCMD_GETREACTIONTIME,
	BOTCMD_GETPERCEPTION,
	BOTCMD_SETSKILLINCREASE,							// Botskill modifying functions functions.
	BOTCMD_ISSKILLINCREASED,
	BOTCMD_SETSKILLDECREASE,				/* 80 */
	BOTCMD_ISSKILLDECREASED,
	BOTCMD_GETGAMEMODE,									// Other functions.
	BOTCMD_GETSPREAD,
	BOTCMD_GETLASTJOINEDPLAYER,
	BOTCMD_GETPLAYERNAME,
	BOTCMD_GETRECEIVEDMEDAL,
	BOTCMD_ACS_EXECUTE,
	BOTCMD_GETFAVORITEWEAPON,
	BOTCMD_SAYFROMLUMP,
	BOTCMD_SAYFROMCHATLUMP,					/* 90 */
	BOTCMD_CHATSECTIONEXISTSINLUMP,
	BOTCMD_CHATSECTIONEXISTSINCHATLUMP,

	NUM_BOTCMDS

} BOTCMD_e;

//*****************************************************************************
typedef enum
{
	RETURNVAL_VOID,
	RETURNVAL_INT,
	RETURNVAL_BOOLEAN,
	RETURNVAL_STRING,

} RETURNVAL_e;

//*****************************************************************************
//	STRUCTURES

typedef struct
{
	// Text name of this command.
	const char	*pszName;

	// Function that corresponds to this bot command.
	void		(*pvFunction)( CSkullBot *pBot );

	// Number of args required for this command (excluding strings).
	LONG		lNumArgs;

	// Number of args string arguments required for this command.
	LONG		lNumStringArgs;

	// Type of return value of this function.
	RETURNVAL_e	ReturnType;

} BOTCMD_s;

//*****************************************************************************
typedef struct
{
	// The name of this section.
	char	szName[64];

	// Each entry in the section. Max of 64.
	char	szEntry[64][1024];

	// Number of entries in this section.
	LONG	lNumEntries;

} CHATFILESECTION_t;

//*****************************************************************************
class	CChatFile
{
public:
	CChatFile( );
	~CChatFile( );

	// Load a chat file of the specified name.
	bool		LoadChatFile( char *pszFileName );

	// Parse the chat file to build the sections, and their entries.
	void		ParseChatFile( void *pvFile );

	// Load a chat file of the specified name.
	bool		LoadChatLump( char *pszLumpName );

	// Parse the chat file to build the sections, and their entries.
	void		ParseChatLump( char *pszLumpName );

	// Add a new section to the chat file.
	CHATFILESECTION_t	*AddSection( char *pszName );

	// Add a new entry to the specified section.
	void		AddEntry( CHATFILESECTION_t *pSection, char *pszName );

	// Find the index of a section with the specified name.
	LONG		FindSection( char *pszName );

	// Return a random entry within the specified section.
	char		*ChooseRandomEntry( char *pszSection );

	// Reads a line from the given file.
	char		*ReadLine( char *pszString, LONG lSize, void *pvFile );

	//*************************************************************************
	CHATFILESECTION_t	Sections[64];

};

//*****************************************************************************
//  PROTOTYPES

// Standard API
void		BOTCMD_Construct( void );
void		BOTCMD_RunCommand( BOTCMD_e Command, CSkullBot *pBot );
void		BOTCMD_SetLastChatString( const char *pszString );
void		BOTCMD_SetLastChatPlayer( const char *pszString );
void		BOTCMD_SetLastJoinedPlayer( const char *pszString );
void		BOTCMD_DoChatStringSubstitutions( CSkullBot *pBot, const char *pszInString, char *pszOutString );
bool		BOTCMD_IgnoreItem( CSkullBot *pBot, LONG lIdx, bool bVisibilityCheck );

#endif	// __BOTCOMMANDS_H__
