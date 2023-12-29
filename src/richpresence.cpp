/*
** richpresence.cpp
**
** handles rich presence for Discord. does nothing but transmit the appid,
** game, and current level.
**
**---------------------------------------------------------------------------
** Copyright 2022 Rachael Alexanderson
** Copyright 2017 Discord
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef NO_DRPC

#ifdef WIN32
#include <windows.h>
#define USE_WINDOWS_DWORD
#endif

#include <time.h>
#include <string.h>

#include "discord.h"
#include "gstrings.h"
#include "version.h"
#include "network.h"
#include "gamemode.h"

static int64_t StartTime = 0;
static bool didInit = false;
static discord::Core *core;

extern int client_curplayers;
extern int client_maxplayers;

void I_UpdateDiscordPresence(bool SendPresence)
{
	const char* curappid = DEFAULT_DISCORD_APP_ID;

	const char* appid = DoomStartupInfo.DiscordAppId.GetChars();
	if (appid && appid[0] != '\0')
		curappid = appid;

	if (!didInit && !SendPresence)
		return;	// we haven't initted, there's nothing to do here, just exit

	if (!didInit)
	{
		auto result = discord::Core::Create(strtoll(curappid, NULL, 10), DiscordCreateFlags_NoRequireDiscord, &core);
		if (!core) {
			DPrintf("Failed to instantiate discord core! (err %d)\n", static_cast<int>(result));
			return;
		}

		didInit = true;
	}

	if (SendPresence)
	{
		bool multiplayer = NETWORK_GetState() == NETSTATE_CLIENT;

		FString mapnamelower = level.mapname;
		mapnamelower.ToLower();

		char* gamemode = GAMEMODE_GetName(GAMEMODE_GetCurrentMode());
		// Rename singleplayer cooperative to "single player" to avoid confusing other people. Other gamemodes stay unchanged.
		if (!multiplayer && stricmp(gamemode, "Cooperative") == 0)
			gamemode = "Singleplayer";

        discord::Activity activity{};
        activity.SetDetails(gamemode);
        activity.SetState(level.LevelName.GetChars());
        activity.GetAssets().SetSmallImage("game-image");
        activity.GetAssets().SetSmallText("game-image");
        activity.GetAssets().SetLargeImage(mapnamelower);
        activity.GetAssets().SetLargeText(level.mapname);
        activity.GetParty().GetSize().SetCurrentSize(multiplayer ? client_curplayers : 0);
        activity.GetParty().GetSize().SetMaxSize(multiplayer ? client_curplayers > 0 ? client_maxplayers : 0 : 0);
        //activity.GetParty().SetId("party id");
		activity.GetParty().SetPrivacy(multiplayer ? discord::ActivityPartyPrivacy::Public : discord::ActivityPartyPrivacy::Private);
        activity.SetType(discord::ActivityType::Playing);
        core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
			DPrintf((result == discord::Result::Ok) ? "Succeeded" : "Failed");
			DPrintf(" updating activity!\n");
        });
	}
	else
	{
        core->ActivityManager().ClearActivity([](discord::Result result) {
			DPrintf((result == discord::Result::Ok) ? "Succeeded" : "Failed");
			DPrintf(" clearing activity!\n");
        });
	}
}

void I_RunDiscordCallbacks()
{
	if (core)
		core->RunCallbacks();
}

#else
void I_UpdateDiscordPresence(bool SendPresence)
{
}
void I_RunDiscordCallbacks()
{
}
#endif
