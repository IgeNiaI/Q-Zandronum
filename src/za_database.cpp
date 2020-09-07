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
// Filename: za_database.cpp
//
//-----------------------------------------------------------------------------

#include "za_database.h"
#include "i_system.h"
#include "g_game.h"
#include "p_acs.h"
#include <sqlite3.h>

//*****************************************************************************
//	DEFINES

#define TABLENAME "Zandronum"

#define TIMEQUERY "SELECT (julianday('now') - 2440587.5)*86400.0"

//*****************************************************************************
//	VARIABLES

// [BB] Handle to our database.
sqlite3 *g_db = NULL;

// [BB] Filename for the database.
CUSTOM_CVAR( String, databasefile, ":memory:", CVAR_ARCHIVE|CVAR_NOSETBYACS )
{
	DATABASE_Init ( );
}

// [BB] The max page count allows to limit the size of the database.
CUSTOM_CVAR( Int, database_maxpagecount, 32768, CVAR_ARCHIVE|CVAR_NOSETBYACS )
{
	if ( DATABASE_IsAvailable() )
		DATABASE_SetMaxPageCount ( self );
}

//*****************************************************************************
//	PROTOTYPES

/**
 * \brief Handles the preparation, binding and execution of an SQLite command.
 *
 * \author Benjamin Berkels
 */
class DataBaseCommand
{
	sqlite3_stmt *_stmt;
public:
	DataBaseCommand ( const char *Command ) : _stmt ( NULL )
	{
		int error = sqlite3_prepare ( g_db, Command, -1, &_stmt, NULL );
		if ( error != SQLITE_OK )
			Printf ( "Could not prepare statement. Error: %s\n", sqlite3_errmsg ( g_db ) );
	}

	~DataBaseCommand ( )
	{
		finalize();
	}

	void bindString ( const int Index, const char *String )
	{
		int error = sqlite3_bind_text ( _stmt, Index, String, -1, SQLITE_STATIC );
		if ( error != SQLITE_OK )
			Printf ( "Could not bind text. Error: %s\n", sqlite3_errmsg ( g_db ) );
	}

	void bindInt ( const int Index, const int IntValue )
	{
		int error = sqlite3_bind_int ( _stmt, Index, IntValue );
		if ( error != SQLITE_OK )
			Printf ( "Could not bind integer. Error: %s\n", sqlite3_errmsg ( g_db ) );
	}

	void finalize ( )
	{
		if ( _stmt != NULL )
		{
			sqlite3_finalize ( _stmt );
			_stmt = NULL;
		}
	}

	bool step ( )
	{
		const int result = sqlite3_step ( _stmt );
		if ( ( result != SQLITE_ROW ) && ( result != SQLITE_DONE ) )
		{
			Printf ( "Could not step statement. Error: %s\n", sqlite3_errmsg ( g_db ) );
			finalize ( );
		}

		return ( result == SQLITE_ROW );
	}

	void exec ( )
	{
		const int result = sqlite3_step ( _stmt );
		if ( result == SQLITE_ROW )
			Printf ( "Executing statement did not finish, sqlite3_step() has another row ready.\n" );
		else if ( result != SQLITE_DONE )
			Printf ( "Could not execute statement. Error: %s\n", sqlite3_errmsg ( g_db ) );

		finalize();
	}

	const unsigned char *getText ( const int ColumnIndex )
	{
		return sqlite3_column_text ( _stmt, ColumnIndex );
	}

	int getInteger ( const int ColumnIndex )
	{
		return sqlite3_column_int ( _stmt, ColumnIndex );
	}

	void iterateAndGetReturnedEntries ( TArray<std::pair<FString, FString> > &Entries )
	{
		while ( step( ) )
		{
			std::pair<FString, FString> value;
			value.first.Format ( "%s", getText(1) );
			value.second.Format ( "%s", getText(2) );
			Entries.Push ( value );
		}
		finalize();
	}
};

//*****************************************************************************
//	FUNCTIONS

void database_ClearHandle ( void )
{
	if ( g_db != NULL )
	{
		sqlite3_close ( g_db );
		g_db = NULL;
	}
}

//*****************************************************************************
//
void database_ExecuteCommand ( const char *Command, int (*Callback)(void*,int,char**,char**) = NULL, void *Data = NULL )
{
	int error = sqlite3_exec ( g_db, Command, Callback, Data, 0);
	if ( error != SQLITE_OK )
		Printf ( "Error: %s\n", sqlite3_errmsg ( g_db ) );
}

//*****************************************************************************
//

void DATABASE_Construct( void )
{
	// [BB] At least we should close the database in case it is open.
	atterm( DATABASE_Destruct );
}

//*****************************************************************************
//

void DATABASE_Destruct( void )
{
	database_ClearHandle ( );
}

//*****************************************************************************
//
void DATABASE_Init ( void )
{
	// [BB] Make sure no database is open.
	database_ClearHandle ( );

	const char *dbFileName = databasefile.GetGenericRep( CVAR_String ).String;

	// [BB] If the user doesn't specify a name for the database, don't open one.
	if ( strlen ( dbFileName ) == 0 )
		return;

	const int error = sqlite3_open ( dbFileName, &g_db );
	if ( error )
	{
		Printf ( "Can't open database \"%s\": %s\n", dbFileName, sqlite3_errmsg ( g_db ) );
		database_ClearHandle ( );
		return;
	}
	else if ( strcmp ( dbFileName, ":memory:" ) == 0 )
		Printf ( PRINT_BOLD, "Using in-memory database. The database will not be saved on exit.\n" );
	else
		Printf ( "Opening database \"%s\" succeeded.\n", dbFileName );

	// [BB] Make sure we have a table.
	DATABASE_CreateTable ( );

	// [BB] Now that the database is ready, we can set the max page count.
	DATABASE_SetMaxPageCount ( database_maxpagecount );
}

//*****************************************************************************
//
bool DATABASE_IsAvailable ( const char *CallingFunction )
{
	const bool available = ( g_db != NULL );
	if ( !available && CallingFunction )
		Printf ( "%s error: No database.\n", CallingFunction );

	return available;
}

//*****************************************************************************
//
void DATABASE_SetMaxPageCount ( const unsigned int MaxPageCount )
{
	FString commandString;
	// [BB] Binding MaxPageCount to the query doesn't seem to work, so
	// we'll have to use this workaround.
	commandString.Format ( "PRAGMA max_page_count=%d", MaxPageCount );
	database_ExecuteCommand ( commandString.GetChars() );
}

//*****************************************************************************
//
void DATABASE_BeginTransaction ( void )
{
	if ( DATABASE_IsAvailable ( "DATABASE_BeginTransaction" ) == false )
		return;

	database_ExecuteCommand ( "BEGIN TRANSACTION" );
}

//*****************************************************************************
//
void DATABASE_EndTransaction ( void )
{
	if ( DATABASE_IsAvailable ( "DATABASE_EndTransaction" ) == false )
		return;

	database_ExecuteCommand ( "END TRANSACTION" );
}

//*****************************************************************************
//
void DATABASE_CreateTable ( )
{
	if ( DATABASE_IsAvailable ( "DATABASE_CreateTable" ) == false )
		return;

	database_ExecuteCommand ( "CREATE TABLE if not exists " TABLENAME "(Namespace text, KeyName text, Value text, Timestamp text, PRIMARY KEY (Namespace, KeyName))" );
}

//*****************************************************************************
//
void DATABASE_ClearTable ( )
{
	if ( DATABASE_IsAvailable ( "DATABASE_ClearTable" ) == false )
		return;

	database_ExecuteCommand ( "DELETE FROM " TABLENAME );
}

//*****************************************************************************
//
void DATABASE_DeleteTable ( )
{
	if ( DATABASE_IsAvailable ( "DATABASE_DeleteTable" ) == false )
		return;

	database_ExecuteCommand ( "DROP TABLE " TABLENAME );
}

//*****************************************************************************
//

static int database_DumpTableCallback ( void * /*Data*/, int Argc, char **Argv, char **ColName )
{
   for ( int i = 0; i < Argc; ++i )
      Printf ( "%s = %s\n", ColName[i], Argv[i] ? Argv[i] : "NULL" );
   Printf("\n");
   return 0;
}

//*****************************************************************************
//
void DATABASE_DumpTable ( )
{
	if ( DATABASE_IsAvailable ( "DATABASE_DumpTable" ) == false )
		return;

	Printf ( "Dumping table \"%s\"\n", TABLENAME );
	database_ExecuteCommand ( "SELECT * from " TABLENAME, database_DumpTableCallback );
}

//*****************************************************************************
//
void DATABASE_EnableWAL ( )
{
	if ( DATABASE_IsAvailable ( "DATABASE_EnableWAL" ) == false )
		return;

	database_ExecuteCommand ( "PRAGMA journal_mode=WAL" );
}

//*****************************************************************************
//
void DATABASE_DisableWAL ( )
{
	if ( DATABASE_IsAvailable ( "DATABASE_DisableWAL" ) == false )
		return;

	database_ExecuteCommand ( "PRAGMA journal_mode=DELETE" );
}

//*****************************************************************************
//
void DATABASE_DumpNamespace ( const char *Namespace )
{
	if ( DATABASE_IsAvailable ( "DATABASE_DumpNamespace" ) == false )
		return;

	Printf ( "Dumping namespace \"%s\"\n", Namespace );
	DataBaseCommand cmd ( "SELECT * from " TABLENAME " WHERE Namespace=?1" );
	cmd.bindString ( 1, Namespace );
	while ( cmd.step( ) )
		Printf ( "%s %s %s\n", cmd.getText(1), cmd.getText(2), cmd.getText(3) );
	cmd.finalize();
}

//*****************************************************************************
//
void DATABASE_AddEntry ( const char *Namespace, const char *EntryName, const char *EntryValue )
{
	if ( DATABASE_IsAvailable ( "DATABASE_AddEntry" ) == false )
		return;

	DataBaseCommand cmd ( "INSERT INTO " TABLENAME " VALUES(?1,?2,?3,(" TIMEQUERY "))" );
	cmd.bindString ( 1, Namespace );
	cmd.bindString ( 2, EntryName );
	cmd.bindString ( 3, EntryValue );
	cmd.exec ( );
}

//*****************************************************************************
//
void DATABASE_SetEntry ( const char *Namespace, const char *EntryName, const char *EntryValue )
{
	if ( DATABASE_IsAvailable ( "DATABASE_SetEntry" ) == false )
		return;

	DataBaseCommand cmd ( "UPDATE " TABLENAME " SET Value=?3,Timestamp=(" TIMEQUERY ") WHERE Namespace=?1 AND KeyName=?2" );
	cmd.bindString ( 1, Namespace );
	cmd.bindString ( 2, EntryName );
	cmd.bindString ( 3, EntryValue );
	cmd.exec ( );
}

//*****************************************************************************
//
FString DATABASE_GetEntry ( const char *Namespace, const char *EntryName )
{
	if ( DATABASE_IsAvailable ( "DATABASE_GetEntry" ) == false )
		return "";

	DataBaseCommand cmd ( "SELECT * FROM " TABLENAME " WHERE Namespace=?1 AND KeyName=?2" );
	cmd.bindString ( 1, Namespace );
	cmd.bindString ( 2, EntryName );
	cmd.step( );
	FString value;
	value.AppendFormat ( "%s", cmd.getText(2) );
	// [BB] We assume that the query will return exactly one row. So we finish stepping now.
	cmd.exec( );
	return value;
}

//*****************************************************************************
//
bool DATABASE_EntryExists ( const char *Namespace, const char *EntryName )
{
	if ( DATABASE_IsAvailable ( "DATABASE_GetEntry" ) == false )
		return "";

	DataBaseCommand cmd ( "SELECT * FROM " TABLENAME " WHERE Namespace=?1 AND KeyName=?2" );
	cmd.bindString ( 1, Namespace );
	cmd.bindString ( 2, EntryName );
	// [BB] The destructor of DataBaseCommand calls finalize.
	return ( cmd.step( ) );
}

//*****************************************************************************
//
void DATABASE_DeleteEntry ( const char *Namespace, const char *EntryName )
{
	if ( DATABASE_IsAvailable ( "DATABASE_DeleteEntry" ) == false )
		return;

	DataBaseCommand cmd ( "DELETE FROM " TABLENAME " WHERE Namespace=?1 AND KeyName=?2" );
	cmd.bindString ( 1, Namespace );
	cmd.bindString ( 2, EntryName );
	cmd.exec ( );
}

//*****************************************************************************
//
void DATABASE_SaveSetEntry ( const char *Namespace, const char *EntryName, const char *EntryValue )
{
	if ( DATABASE_IsAvailable ( "DATABASE_SaveSetEntry" ) == false )
		return;

	if ( DATABASE_EntryExists ( Namespace, EntryName ) )
	{
		// [BB] Setting an entry to the empty string deletes the entry.
		if ( EntryValue && ( strlen ( EntryValue ) > 0 ) )
			DATABASE_SetEntry ( Namespace, EntryName, EntryValue );
		else
			DATABASE_DeleteEntry ( Namespace, EntryName );
	}
	// [BB] Don't store empty string entries.
	else if ( EntryValue && ( strlen ( EntryValue ) > 0 ) )
		DATABASE_AddEntry ( Namespace, EntryName, EntryValue );
}

//*****************************************************************************
//
void DATABASE_SaveSetEntryInt ( const char *Namespace, const char *EntryName, int EntryValue )
{
	FString entryValue;
	entryValue.AppendFormat ( "%d", EntryValue );
	DATABASE_SaveSetEntry ( Namespace, EntryName, entryValue.GetChars() );
}

//*****************************************************************************
//
FString DATABASE_SaveGetEntry ( const char *Namespace, const char *EntryName )
{
	if ( DATABASE_IsAvailable ( "DATABASE_SaveGetEntry" ) == false )
		return "";

	if ( DATABASE_EntryExists ( Namespace, EntryName ) )
		return DATABASE_GetEntry ( Namespace, EntryName );
	else
		return "";
}

//*****************************************************************************
//
void DATABASE_SaveIncrementEntryInt ( const char *Namespace, const char *EntryName, int Increment )
{
	if ( DATABASE_IsAvailable ( "DATABASE_SaveIncrementEntryInt" ) == false )
		return;

	FString newVal;
	if ( DATABASE_EntryExists ( Namespace, EntryName ) )
	{
		// [BB] Get the old value and set the incremented value in a single query.
		DataBaseCommand cmd ( "UPDATE " TABLENAME " SET Value=(SELECT CAST(Value AS INTEGER) FROM " TABLENAME " WHERE Namespace=?1 AND KeyName=?2)+?3,Timestamp=(" TIMEQUERY ") WHERE Namespace=?1 AND KeyName=?2" );
		cmd.bindString ( 1, Namespace );
		cmd.bindString ( 2, EntryName );
		cmd.bindInt ( 3, Increment );
		cmd.exec ( );
	}
	else
	{
		newVal.AppendFormat ( "%d", Increment );
		DATABASE_AddEntry ( Namespace, EntryName, newVal.GetChars() );
	}
}

//*****************************************************************************
//
int DATABASE_GetEntryRank ( const char *Namespace, const char *EntryName, const bool Descending )
{
	if ( DATABASE_IsAvailable ( "DATABASE_GetEntryRank" ) == false )
		return -1;

	if ( DATABASE_EntryExists ( Namespace, EntryName ) )
	{
		// [BB] To get the rank of a certain entry, we get the value of the entry,
		// count how many values are lower (or higher) than the value and return
		// the count + 1.
		FString commandString;
		commandString.Format ( "SELECT COUNT(*) from " TABLENAME " WHERE Namespace=?1 AND CAST(Value AS INTEGER)" );
		commandString += Descending ? ">" : "<";
		commandString += ( "(SELECT CAST(Value AS INTEGER) FROM " TABLENAME " WHERE Namespace=?2 AND KeyName=?3)" );
		DataBaseCommand cmd ( commandString.GetChars() );
		cmd.bindString ( 1, Namespace );
		cmd.bindString ( 2, Namespace );
		cmd.bindString ( 3, EntryName );
		cmd.step( );
		const int res = cmd.getInteger(0) + 1 ;
		return res;
	}
	else
	{
		return -1;
	}
}

//*****************************************************************************
//
int DATABASE_GetSortedEntries ( const char *Namespace, const int N, const int Offset, const bool Descending, TArray<std::pair<FString, FString> > &Entries )
{
	if ( DATABASE_IsAvailable ( "DATABASE_GetSortedEntryAsPair" ) == false )
	{
		Entries.Clear();
		return 0;
	}

	FString commandString;
	commandString.Format ( "SELECT * from " TABLENAME " WHERE Namespace=?1 ORDER BY CAST(Value AS INTEGER) " );
	commandString += Descending ? "DESC" : "ASC";
	commandString += " LIMIT ?2 OFFSET ?3";
	DataBaseCommand cmd ( commandString.GetChars() );
	cmd.bindString ( 1, Namespace );
	cmd.bindInt ( 2, N );
	cmd.bindInt ( 3, Offset );
	cmd.iterateAndGetReturnedEntries ( Entries );
	return Entries.Size();
}

//*****************************************************************************
//
int DATABASE_GetEntries ( const char *Namespace, TArray<std::pair<FString, FString> > &Entries )
{
	if ( DATABASE_IsAvailable ( "DATABASE_GetEntries" ) == false )
	{
		Entries.Clear();
		return 0;
	}

	DataBaseCommand cmd ( "SELECT * from " TABLENAME " WHERE Namespace=?1" );
	cmd.bindString ( 1, Namespace );
	cmd.iterateAndGetReturnedEntries ( Entries );
	return Entries.Size();
}

//*****************************************************************************
//	CONSOLE COMMANDS

CCMD ( dumpdb )
{
	DATABASE_DumpTable();
}

CCMD ( db_enable_wal )
{
	// [BB] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	DATABASE_EnableWAL();
}

CCMD ( db_disable_wal )
{
	// [BB] This function may not be used by ConsoleCommand.
	if ( ACS_IsCalledFromConsoleCommand( ))
		return;

	DATABASE_DisableWAL();
}
