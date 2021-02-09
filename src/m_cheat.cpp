// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Cheat sequence checking.
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <math.h>

#include "m_cheat.h"
#include "d_player.h"
#include "doomstat.h"
#include "gstrings.h"
#include "p_local.h"
#include "a_strifeglobal.h"
#include "gi.h"
#include "p_enemy.h"
#include "sbar.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "w_wad.h"
#include "a_keys.h"
#include "templates.h"
#include "c_console.h"
#include "r_data/r_translate.h"
#include "g_level.h"
#include "d_net.h"
#include "d_dehacked.h"
#include "gi.h"
#include "farchive.h"
// [BB] New #includes.
#include "deathmatch.h"
#include "announcer.h"
#include "team.h"
#include "gamemode.h"
#include "cl_commands.h"
#include "cl_demo.h"

// [RH] Actually handle the cheat. The cheat code in st_stuff.c now just
// writes some bytes to the network data stream, and the network code
// later calls us.

void cht_DoCheat (player_t *player, int cheat)
{
	static const char * BeholdPowers[9] =
	{
		"PowerInvulnerable",
		"PowerStrength",
		"PowerInvisibility",
		"PowerIronFeet",
		"MapRevealer",
		"PowerLightAmp",
		"PowerShadow",
		"PowerMask",
		"PowerTargeter",
	};
	const PClass *type;
	AInventory *item;
	const char *msg = "";
	char msgbuild[32];
	int i;

	switch (cheat)
	{
	case CHT_IDDQD:
		if (!(player->cheats & CF_GODMODE) && player->playerstate == PST_LIVE)
		{
			if (player->mo)
				player->mo->health = deh.GodHealth;

			player->health = deh.GodHealth;
		}
		// fall through
	case CHT_GOD:
		player->cheats ^= CF_GODMODE;
		if (player->cheats & CF_GODMODE)
			msg = GStrings("STSTR_DQDON");
		else
			msg = GStrings("STSTR_DQDOFF");
		ST_SetNeedRefresh();
		break;

	case CHT_BUDDHA:
		player->cheats ^= CF_BUDDHA;
		if (player->cheats & CF_BUDDHA)
			msg = GStrings("TXT_BUDDHAON");
		else
			msg = GStrings("TXT_BUDDHAOFF");
		break;

	case CHT_NOCLIP:
		player->cheats ^= CF_NOCLIP;
		if (player->cheats & CF_NOCLIP)
			msg = GStrings("STSTR_NCON");
		else
			msg = GStrings("STSTR_NCOFF");
		break;

	case CHT_NOCLIP2:
		player->cheats ^= CF_NOCLIP2;
		if (player->cheats & CF_NOCLIP2)
		{
			player->cheats |= CF_NOCLIP;
			msg = GStrings("STSTR_NC2ON");
		}
		else
		{
			player->cheats &= ~CF_NOCLIP;
			msg = GStrings("STSTR_NCOFF");
		}
		break;

	case CHT_NOVELOCITY:
		player->cheats ^= CF_NOVELOCITY;
		if (player->cheats & CF_NOVELOCITY)
			msg = GStrings("TXT_LEADBOOTSON");
		else
			msg = GStrings("TXT_LEADBOOTSOFF");
		break;

	case CHT_FLY:
		if (player->mo != NULL)
		{
			player->cheats ^= CF_FLY;
			if (player->cheats & CF_FLY)
			{
				player->mo->flags |= MF_NOGRAVITY;
				player->mo->flags2 |= MF2_FLY;
				msg = GStrings("TXT_LIGHTER");
			}
			else
			{
				player->mo->flags &= ~MF_NOGRAVITY;
				player->mo->flags2 &= ~MF2_FLY;
				msg = GStrings("TXT_GRAVITY");
			}
		}
		break;

	case CHT_MORPH:
		msg = cht_Morph (player, PClass::FindClass (gameinfo.gametype == GAME_Heretic ? NAME_ChickenPlayer : NAME_PigPlayer), true);
		break;

	case CHT_NOTARGET:
		player->cheats ^= CF_NOTARGET;
		if (player->cheats & CF_NOTARGET)
			msg = "notarget ON";
		else
			msg = "notarget OFF";
		break;

	case CHT_ANUBIS:
		player->cheats ^= CF_FRIGHTENING;
		if (player->cheats & CF_FRIGHTENING)
			msg = "\"Quake with fear!\"";
		else
			msg = "No more ogre armor";
		break;

	case CHT_CHASECAM:
		player->cheats ^= CF_CHASECAM;
		if (player->cheats & CF_CHASECAM)
			msg = "chasecam ON";
		else
			msg = "chasecam OFF";
		R_ResetViewInterpolation ();
		break;

	case CHT_CHAINSAW:
		if (player->mo != NULL && player->health >= 0)
		{
			type = PClass::FindClass ("Chainsaw");
			if (player->mo->FindInventory (type) == NULL)
			{
				player->mo->GiveInventoryType (type);
			}
			msg = GStrings("STSTR_CHOPPERS");
		}
		// [RH] The original cheat also set powers[pw_invulnerability] to true.
		// Since this is a timer and not a boolean, it effectively turned off
		// the invulnerability powerup, although it looks like it was meant to
		// turn it on.
		break;

	case CHT_POWER:
		if (player->mo != NULL && player->health >= 0)
		{
			item = player->mo->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2), true);
			if (item != NULL)
			{
				item->Destroy ();
				msg = GStrings("TXT_CHEATPOWEROFF");
			}
			else
			{
				player->mo->GiveInventoryType (RUNTIME_CLASS(APowerWeaponLevel2));
				msg = GStrings("TXT_CHEATPOWERON");
			}
		}
		break;

	case CHT_IDKFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		cht_Give (player, "keys");
		cht_Give (player, "armor");
		msg = GStrings("STSTR_KFAADDED");
		break;

	case CHT_IDFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		cht_Give (player, "armor");
		msg = GStrings("STSTR_FAADDED");
		break;

	case CHT_BEHOLDV:
	case CHT_BEHOLDS:
	case CHT_BEHOLDI:
	case CHT_BEHOLDR:
	case CHT_BEHOLDA:
	case CHT_BEHOLDL:
	case CHT_PUMPUPI:
	case CHT_PUMPUPM:
	case CHT_PUMPUPT:
		i = cheat - CHT_BEHOLDV;

		if (i == 4)
		{
			level.flags2 ^= LEVEL2_ALLMAP;
		}
		else if (player->mo != NULL && player->health >= 0)
		{
			item = player->mo->FindInventory (BeholdPowers[i]);
			if (item == NULL)
			{
				if (i != 0)
				{
					cht_Give(player, BeholdPowers[i]);
					if (cheat == CHT_BEHOLDS)
					{
						P_GiveBody (player->mo, -100);
					}
				}
				else
				{
					// Let's give the item here so that the power doesn't need colormap information.
					cht_Give(player, "InvulnerabilitySphere");
				}
			}
			else
			{
				item->Destroy ();
			}
		}
		msg = GStrings("STSTR_BEHOLDX");
		break;

	case CHT_MASSACRE:
		{
			int killcount = P_Massacre ();
			// killough 3/22/98: make more intelligent about plural
			// Ty 03/27/98 - string(s) *not* externalized
			mysnprintf (msgbuild, countof(msgbuild), "%d Monster%s Killed", killcount, killcount==1 ? "" : "s");
			msg = msgbuild;
		}
		break;

	case CHT_HEALTH:
		if (player->mo != NULL && player->playerstate == PST_LIVE)
		{
			player->health = player->mo->health = player->mo->GetDefault()->health;
			msg = GStrings("TXT_CHEATHEALTH");
		}
		break;

	case CHT_KEYS:
		cht_Give (player, "keys");
		msg = GStrings("TXT_CHEATKEYS");
		break;

	// [GRB]
	case CHT_RESSURECT:
		if (player->playerstate != PST_LIVE && player->mo != NULL)
		{
			if (player->mo->IsKindOf(RUNTIME_CLASS(APlayerChunk)))
			{
				Printf("Unable to resurrect. Player is no longer connected to its body.\n");
			}
			else
			{
				player->playerstate = PST_LIVE;
				player->health = player->mo->health = player->mo->GetDefault()->health;
				player->viewheight = ((APlayerPawn *)player->mo->GetDefault())->ViewHeight;
				player->mo->flags = player->mo->GetDefault()->flags;
				player->mo->flags2 = player->mo->GetDefault()->flags2;
				player->mo->flags3 = player->mo->GetDefault()->flags3;
				player->mo->flags4 = player->mo->GetDefault()->flags4;
				player->mo->flags5 = player->mo->GetDefault()->flags5;
				player->mo->flags6 = player->mo->GetDefault()->flags6;
				player->mo->flags7 = player->mo->GetDefault()->flags7;
				player->mo->flags8 = player->mo->GetDefault()->flags8;
				player->mo->mvFlags = player->mo->GetDefault()->mvFlags;
				player->mo->renderflags &= ~RF_INVISIBLE;
				player->mo->height = player->mo->GetDefault()->height;
				player->mo->radius = player->mo->GetDefault()->radius;
				player->mo->special1 = 0;	// required for the Hexen fighter's fist attack. 
											// This gets set by AActor::Die as flag for the wimpy death and must be reset here.
				player->mo->SetState (player->mo->SpawnState);
				if (!(player->mo->flags2 & MF2_DONTTRANSLATE))
				{
					player->mo->Translation = TRANSLATION(TRANSLATION_Players, BYTE(player-players));
				}
				player->mo->DamageType = NAME_None;
//				player->mo->GiveDefaultInventory();
				if (player->ReadyWeapon != NULL)
				{
					P_SetPsprite(player, ps_weapon, player->ReadyWeapon->GetUpState());
				}

				if (player->morphTics > 0)
				{
					P_UndoPlayerMorph(player, player);
				}

			}
		}
		break;

	case CHT_GIMMIEA:
		cht_Give (player, "ArtiInvulnerability");
		msg = "Valador's Ring of Invunerability";
		break;

	case CHT_GIMMIEB:
		cht_Give (player, "ArtiInvisibility");
		msg = "Shadowsphere";
		break;

	case CHT_GIMMIEC:
		cht_Give (player, "ArtiHealth");
		msg = "Quartz Flask";
		break;

	case CHT_GIMMIED:
		cht_Give (player, "ArtiSuperHealth");
		msg = "Mystic Urn";
		break;

	case CHT_GIMMIEE:
		cht_Give (player, "ArtiTomeOfPower");
		msg = "Tyketto's Tome of Power";
		break;

	case CHT_GIMMIEF:
		cht_Give (player, "ArtiTorch");
		msg = "Torch";
		break;

	case CHT_GIMMIEG:
		cht_Give (player, "ArtiTimeBomb");
		msg = "Delmintalintar's Time Bomb of the Ancients";
		break;

	case CHT_GIMMIEH:
		cht_Give (player, "ArtiEgg");
		msg = "Torpol's Morph Ovum";
		break;

	case CHT_GIMMIEI:
		cht_Give (player, "ArtiFly");
		msg = "Inhilicon's Wings of Wrath";
		break;

	case CHT_GIMMIEJ:
		cht_Give (player, "ArtiTeleport");
		msg = "Darchala's Chaos Device";
		break;

	case CHT_GIMMIEZ:
		for (int i=0;i<16;i++)
		{
			cht_Give (player, "artifacts");
		}
		msg = "All artifacts!";
		break;

	case CHT_TAKEWEAPS:
		if (player->morphTics || player->mo == NULL || player->mo->health <= 0)
		{
			return;
		}
		{
			// Take away all weapons that are either non-wimpy or use ammo.
			AInventory **invp = &player->mo->Inventory, **lastinvp;
			for (item = *invp; item != NULL; item = *invp)
			{
				lastinvp = invp;
				invp = &(*invp)->Inventory;
				if (item->IsKindOf (RUNTIME_CLASS(AWeapon)))
				{
					AWeapon *weap = static_cast<AWeapon *> (item);
					if (!(weap->WeaponFlags & WIF_WIMPY_WEAPON) ||
						weap->AmmoType1 != NULL)
					{
						item->Destroy ();
						invp = lastinvp;
					}
				}
			}
		}
		msg = GStrings("TXT_CHEATIDKFA");
		break;

	case CHT_NOWUDIE:
		cht_Suicide (player);
		msg = GStrings("TXT_CHEATIDDQD");
		break;

	case CHT_ALLARTI:
		for (int i=0;i<25;i++)
		{
			cht_Give (player, "artifacts");
		}
		msg = GStrings("TXT_CHEATARTIFACTS3");
		break;

	case CHT_PUZZLE:
		cht_Give (player, "puzzlepieces");
		msg = GStrings("TXT_CHEATARTIFACTS3");
		break;

	case CHT_MDK:
		if (player->mo == NULL)
		{
			Printf ("What do you want to kill outside of a game?\n");
		}
		// else if (!deathmatch)
		{
			// Don't allow this in deathmatch even with cheats enabled, because it's
			// a very very cheap kill.
			// [Dusk] <jino> and summoning 5000 bfg balls isn't?
			P_LineAttack (player->mo, player->mo->angle, PLAYERMISSILERANGE,
				P_AimLineAttack (player->mo, player->mo->angle, PLAYERMISSILERANGE), TELEFRAG_DAMAGE,
				NAME_MDK, NAME_BulletPuff);
		}
		break;

	case CHT_DONNYTRUMP:
		cht_Give (player, "HealthTraining");
		msg = GStrings("TXT_MIDASTOUCH");
		break;

	case CHT_LEGO:
		if (player->mo != NULL && player->health >= 0)
		{
			int oldpieces = ASigil::GiveSigilPiece (player->mo);
			item = player->mo->FindInventory (RUNTIME_CLASS(ASigil));

			if (item != NULL)
			{
				if (oldpieces == 5)
				{
					item->Destroy ();
				}
				else
				{
					player->PendingWeapon = static_cast<AWeapon *> (item);
				}
			}
		}
		break;

	case CHT_PUMPUPH:
		cht_Give (player, "MedPatch");
		cht_Give (player, "MedicalKit");
		cht_Give (player, "SurgeryKit");
		msg = GStrings("TXT_GOTSTUFF");
		break;

	case CHT_PUMPUPP:
		cht_Give (player, "AmmoSatchel");
		msg = GStrings("TXT_GOTSTUFF");
		break;

	case CHT_PUMPUPS:
		cht_Give (player, "UpgradeStamina", 10);
		cht_Give (player, "UpgradeAccuracy");
		msg = GStrings("TXT_GOTSTUFF");
		break;

	case CHT_CLEARFROZENPROPS:
		player->cheats &= ~(CF_FROZEN|CF_TOTALLYFROZEN);
		msg = "Frozen player properties turned off";
		break;

/* [BB] Skulltag doesn't use this.
	case CHT_FREEZE:
		bglobal.changefreeze ^= 1;
		if (bglobal.freeze ^ bglobal.changefreeze)
		{
			msg = GStrings("TXT_FREEZEON");
		}
		else
		{
			msg = GStrings("TXT_FREEZEOFF");
		}
		break;
*/
	}

	if (!*msg)              // [SO] Don't print blank lines!
		return;

	if( ( cheat != CHT_CHASECAM )
		|| ( !( GAMEMODE_GetCurrentFlags() & GMF_COOPERATIVE )
			&& ( player->bSpectating == false ) && !(dmflags2 & DF2_CHASECAM))){
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVER_Printf( "%s is a cheater: %s\n", player->userinfo.GetName(), msg );
		else if ( player == &players[consoleplayer] || CLIENTDEMO_IsFreeSpectatorPlayer( player ) )
			Printf ("%s\n", msg);
		// [BB] The server already ensures that all clients see the cheater message.
		else if ( NETWORK_GetState( ) != NETSTATE_CLIENT )
			Printf ("%s is a cheater: %s\n", player->userinfo.GetName(), msg);
	}
}

const char *cht_Morph (player_t *player, const PClass *morphclass, bool quickundo)
{
	if (player->mo == NULL)
	{
		return "";
	}
	PClass *oldclass = player->mo->GetClass();

	// Set the standard morph style for the current game
	int style = MORPH_UNDOBYTOMEOFPOWER;
	if (gameinfo.gametype == GAME_Hexen) style |= MORPH_UNDOBYCHAOSDEVICE;

	if (player->morphTics)
	{
		if (P_UndoPlayerMorph (player, player))
		{
			if (!quickundo && oldclass != morphclass && P_MorphPlayer (player, player, morphclass, 0, style))
			{
				return GStrings("TXT_STRANGER");
			}
			return GStrings("TXT_NOTSTRANGE");
		}
	}
	else if (P_MorphPlayer (player, player, morphclass, 0, style))
	{
		return GStrings("TXT_STRANGE");
	}
	return "";
}

void GiveSpawner (player_t *player, const PClass *type, int amount)
{
	if (player->mo == NULL || player->health <= 0)
	{
		return;
	}

	AInventory *item = static_cast<AInventory *>
		(Spawn (type, player->mo->x, player->mo->y, player->mo->z, NO_REPLACE));
	if (item != NULL)
	{
		if (amount > 0)
		{
			if (type->IsDescendantOf (RUNTIME_CLASS(ABasicArmorPickup)))
			{
				if (static_cast<ABasicArmorPickup*>(item)->SaveAmount != 0)
				{
					static_cast<ABasicArmorPickup*>(item)->SaveAmount *= amount;
				}
				else
				{
					static_cast<ABasicArmorPickup*>(item)->SaveAmount *= amount;
				}
			}
			else if (type->IsDescendantOf (RUNTIME_CLASS(ABasicArmorBonus)))
			{
				static_cast<ABasicArmorBonus*>(item)->SaveAmount *= amount;
				// [BB]
				static_cast<ABasicArmorBonus*>(item)->BonusCount *= amount;

			}
			else
			{
				item->Amount = MIN (amount, item->MaxAmount);
			}
		}
		item->ClearCounters();
		if (!item->CallTryPickup (player->mo))
		{
			item->Destroy ();
		}
		else
		{
			// [BB] This construction is more or less a hack, but at least the give cheats are now working.
			SERVER_GiveInventoryToPlayer( player, item );
			// [BC] Play the announcer sound.
			if ( players[consoleplayer].camera == players[consoleplayer].mo && cl_announcepickups )
				ANNOUNCER_PlayEntry( cl_announcer, item->PickupAnnouncerEntry( ));
		}
	}
}

void cht_Give (player_t *player, const char *name, int amount)
{
	enum { ALL_NO, ALL_YES, ALL_YESYES } giveall;
	int i;
	const PClass *type;

	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVER_Printf( "%s is a cheater: give %s\n", player->userinfo.GetName(), name );
	else if (player != &players[consoleplayer])
		Printf ("%s is a cheater: give %s\n", player->userinfo.GetName(), name);

	if (player->mo == NULL || player->health <= 0)
	{
		return;
	}

	giveall = ALL_NO;
	if (stricmp (name, "all") == 0)
	{
		giveall = ALL_YES;
	}
	else if (stricmp (name, "everything") == 0)
	{
		giveall = ALL_YESYES;
	}

	if (stricmp (name, "health") == 0)
	{
		if (amount > 0)
		{
			if (player->mo)
			{
				player->mo->health += amount;
	  			player->health = player->mo->health;
			}
			else
			{
				player->health += amount;
			}
		}
		else
		{
			if (player->mo != NULL)
			{
				player->health = player->mo->health = player->mo->GetMaxHealth();
			}
			else
			{
				player->health = deh.GodHealth;
			}
		}
		// [BB]: The server has to inform the clients that this player's health has changed.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			ULONG playerIdx = static_cast<ULONG> ( player - players );
			SERVERCOMMANDS_SetPlayerHealth( playerIdx );
		}
	}

	if (giveall || stricmp (name, "backpack") == 0)
	{
		// Select the correct type of backpack based on the game
		type = PClass::FindClass(gameinfo.backpacktype);
		if (type != NULL)
		{
			GiveSpawner (player, type, 1);
		}

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "ammo") == 0)
	{
		// Find every unique type of ammo. Give it to the player if
		// he doesn't have it already, and set each to its maximum.
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			const PClass *type = PClass::m_Types[i];

			if (type->ParentClass == RUNTIME_CLASS(AAmmo))
			{
				AInventory *ammo = player->mo->FindInventory (type);
				if (ammo == NULL)
				{
					ammo = static_cast<AInventory *>(Spawn (type, 0, 0, 0, NO_REPLACE));
					ammo->AttachToOwner (player->mo);
					ammo->Amount = ammo->MaxAmount;
				}
				else if (ammo->Amount < ammo->MaxAmount)
				{
					ammo->Amount = ammo->MaxAmount;
				}
				// [BB] This construction is more or less a hack, but at least the give cheats are now working.
				SERVER_GiveInventoryToPlayer( player, ammo );
			}
		}

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "armor") == 0)
	{
		if (gameinfo.gametype != GAME_Hexen)
		{
			ABasicArmorPickup *armor = Spawn<ABasicArmorPickup> (0,0,0, NO_REPLACE);
			armor->SaveAmount = 100*deh.BlueAC;
			armor->SavePercent = gameinfo.Armor2Percent > 0? gameinfo.Armor2Percent : FRACUNIT/2;
			if (!armor->CallTryPickup (player->mo))
			{
				armor->Destroy ();
			}
			else
			{
				// [BB] This construction is more or less a hack, but at least the give cheats are now working.
				SERVER_GiveInventoryToPlayer( player, armor );
			}
		}
		else
		{
			for (i = 0; i < 4; ++i)
			{
				AHexenArmor *armor = Spawn<AHexenArmor> (0,0,0, NO_REPLACE);
				armor->health = i;
				armor->Amount = 0;
				if (!armor->CallTryPickup (player->mo))
				{
					armor->Destroy ();
				}
				else
				{
					// [BB] This construction is more or less a hack, but at least the give cheats are now working.
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVER_GiveInventoryToPlayer( player, armor );
						SERVERCOMMANDS_SyncHexenArmorSlots ( static_cast<ULONG> ( player - players ) );
					}
				}
			}
		}

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "keys") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			if (PClass::m_Types[i]->IsDescendantOf (RUNTIME_CLASS(AKey)))
			{
				AKey *key = (AKey *)GetDefaultByType (PClass::m_Types[i]);
				if (key->KeyNumber != 0)
				{
					key = static_cast<AKey *>(Spawn (PClass::m_Types[i], 0,0,0, NO_REPLACE));
					if (!key->CallTryPickup (player->mo))
					{
						key->Destroy ();
					}
					else
					{
						// [BB] This construction is more or less a hack, but at least the give cheats are now working.
						SERVER_GiveInventoryToPlayer( player, key );
					}
				}
			}
		}
		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "weapons") == 0 || stricmp (name, "stdweapons") == 0)
	{
		// [BB] Don't give the ST weapons if this it true. Useful if you want
		// to start a game in the middle of a Doom coop megawad for example.
		bool stdweapons = (stricmp (name, "stdweapons") == 0);

		AWeapon *savedpending = player->PendingWeapon;
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];
			// Don't give replaced weapons unless the replacement was done by Dehacked.
			if (type != RUNTIME_CLASS(AWeapon) &&
				type->IsDescendantOf (RUNTIME_CLASS(AWeapon)) &&
				(type->GetReplacement() == type ||
				 type->GetReplacement()->IsDescendantOf(RUNTIME_CLASS(ADehackedPickup))))

			{
				// Give the weapon only if it belongs to the current game or
				// is in a weapon slot. 
				if (type->ActorInfo->GameFilter == GAME_Any || 
					(type->ActorInfo->GameFilter & gameinfo.gametype) ||	
					player->weapons.LocateWeapon(type, NULL, NULL))
				{
				if (stdweapons)
				{
					const char *WeaponName = type->TypeName.GetChars();
					if ( !stricmp (WeaponName, "Railgun")
					     || !stricmp (WeaponName, "Minigun")
					     || !stricmp (WeaponName, "GrenadeLauncher")
					     || !stricmp (WeaponName, "Bfg10K") )
						continue;
				}

					AWeapon *def = (AWeapon*)GetDefaultByType (type);
					if (giveall == ALL_YESYES || !(def->WeaponFlags & WIF_CHEATNOTWEAPON))
					{
						GiveSpawner (player, type, 1);
					}
				}
			}
		}
		player->PendingWeapon = savedpending;

		// [BB] If we're the server, also tell the client to restore the original weapon.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			// [BB] We need to make sure that the client has its old weapon up instantly.
			// Since there is no net command for this and I don't want to add another net command
			// just for this cheat, we use a workaround here.
			const bool playerHasInstantWeapSwitch = !!(player->cheats & CF_INSTANTWEAPSWITCH);
			const ULONG ulPlayer = static_cast<ULONG>( player - players );
			if ( playerHasInstantWeapSwitch == false )
			{
				player->cheats |= CF_INSTANTWEAPSWITCH;
				SERVERCOMMANDS_SetPlayerCheats( ulPlayer );
			}
			SERVERCOMMANDS_WeaponChange( ulPlayer );
			if ( playerHasInstantWeapSwitch == false )
			{
				player->cheats &= ~CF_INSTANTWEAPSWITCH;
				SERVERCOMMANDS_SetPlayerCheats( ulPlayer );
			}
		}

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "artifacts") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];
			if (type->IsDescendantOf (RUNTIME_CLASS(AInventory)))
			{
				AInventory *def = (AInventory*)GetDefaultByType (type);
				if (def->Icon.isValid() && def->MaxAmount > 1 &&
					!type->IsDescendantOf (RUNTIME_CLASS(APuzzleItem)) &&
					!type->IsDescendantOf (RUNTIME_CLASS(APowerup)) &&
					!type->IsDescendantOf (RUNTIME_CLASS(AArmor)))
				{
					// Do not give replaced items unless using "give everything"
					if (giveall == ALL_YESYES || type->GetReplacement() == type)
					{
						GiveSpawner (player, type, amount <= 0 ? def->MaxAmount : amount);
					}
				}
			}
		}
		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "puzzlepieces") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];
			if (type->IsDescendantOf (RUNTIME_CLASS(APuzzleItem)))
			{
				AInventory *def = (AInventory*)GetDefaultByType (type);
				if (def->Icon.isValid())
				{
					// Do not give replaced items unless using "give everything"
					if (giveall == ALL_YESYES || type->GetReplacement() == type)
					{
						GiveSpawner (player, type, amount <= 0 ? def->MaxAmount : amount);
					}
				}
			}
		}
		if (!giveall)
			return;
	}

	if (giveall)
		return;

	type = PClass::FindClass (name);
	if (type == NULL || !type->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		if (player == &players[consoleplayer])
			Printf ("Unknown item \"%s\"\n", name);
	}
	else
	{
		GiveSpawner (player, type, amount);
	}
	return;
}

void cht_Take (player_t *player, const char *name, int amount)
{
	bool takeall;
	const PClass *type;

	if (player->mo == NULL || player->health <= 0)
	{
		return;
	}

	takeall = (stricmp (name, "all") == 0);

	if (!takeall && stricmp (name, "health") == 0)
	{
		if (player->mo->health - amount <= 0
			|| player->health - amount <= 0
			|| amount == 0)
		{

			cht_Suicide (player);

			if (player == &players[consoleplayer])
				C_HideConsole ();

			return;
		}

		if (amount > 0)
		{
			if (player->mo)
			{
				player->mo->health -= amount;
	  			player->health = player->mo->health;
			}
			else
			{
				player->health -= amount;
			}

			// [TP]
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SetPlayerHealth( player - players );
		}

		if (!takeall)
			return;
	}

	if (takeall || stricmp (name, "backpack") == 0)
	{
		// Take away all types of backpacks the player might own.
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			const PClass *type = PClass::m_Types[i];

			if (type->IsDescendantOf(RUNTIME_CLASS (ABackpackItem)))
			{
				AInventory *pack = player->mo->FindInventory (type);

				// [TP]
				if ( pack && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
					SERVERCOMMANDS_TakeInventory( player - players, type, 0 );

				if (pack) pack->Destroy();
			}
		}

		if (!takeall)
			return;
	}

	if (takeall || stricmp (name, "ammo") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			const PClass *type = PClass::m_Types[i];

			if (type->ParentClass == RUNTIME_CLASS (AAmmo))
			{
				AInventory *ammo = player->mo->FindInventory (type);

				// [TP]
				if ( ammo && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
					SERVERCOMMANDS_TakeInventory( player - players, type, 0 );

				if (ammo)
					ammo->Amount = 0;
			}
		}

		if (!takeall)
			return;
	}

	if (takeall || stricmp (name, "armor") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];

			if (type->IsDescendantOf (RUNTIME_CLASS (AArmor)))
			{
				AActor *armor = player->mo->FindInventory (type);

				if (armor)
					armor->Destroy ();
			}
		}

		// [TP]
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		{
			SERVERCOMMANDS_SetPlayerArmor( player - players );
			SERVERCOMMANDS_SyncHexenArmorSlots( player - players );
		}

		if (!takeall)
			return;
	}

	if (takeall || stricmp (name, "keys") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];

			if (type->IsDescendantOf (RUNTIME_CLASS (AKey)))
			{
				AActor *key = player->mo->FindInventory (type);

				// [TP]
				if ( key && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
					SERVERCOMMANDS_TakeInventory( player - players, type, 0 );

				if (key)
					key->Destroy ();
			}
		}

		if (!takeall)
			return;
	}

	if (takeall || stricmp (name, "weapons") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];

			if (type != RUNTIME_CLASS(AWeapon) &&
				type->IsDescendantOf (RUNTIME_CLASS (AWeapon)))
			{
				AActor *weapon = player->mo->FindInventory (type);

				// [TP]
				if ( weapon && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
					SERVERCOMMANDS_TakeInventory( player - players, type, 0 );

				if (weapon)
					weapon->Destroy ();

				player->ReadyWeapon = NULL;
				player->PendingWeapon = WP_NOCHANGE;
				player->psprites[ps_weapon].state = NULL;
				player->psprites[ps_flash].state = NULL;
			}
		}

		if (!takeall)
			return;
	}

	if (takeall || stricmp (name, "artifacts") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];

			if (type->IsDescendantOf (RUNTIME_CLASS (AInventory)))
			{
				if (!type->IsDescendantOf (RUNTIME_CLASS (APuzzleItem)) &&
					!type->IsDescendantOf (RUNTIME_CLASS (APowerup)) &&
					!type->IsDescendantOf (RUNTIME_CLASS (AArmor)) &&
					!type->IsDescendantOf (RUNTIME_CLASS (AWeapon)) &&
					!type->IsDescendantOf (RUNTIME_CLASS (AKey)))
				{
					AActor *artifact = player->mo->FindInventory (type);

					// [TP]
					if ( artifact && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
						SERVERCOMMANDS_TakeInventory( player - players, type, 0 );

					if (artifact)
						artifact->Destroy ();
				}
			}
		}

		if (!takeall)
			return;
	}

	if (takeall || stricmp (name, "puzzlepieces") == 0)
	{
		for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
		{
			type = PClass::m_Types[i];

			if (type->IsDescendantOf (RUNTIME_CLASS (APuzzleItem)))
			{
				AActor *puzzlepiece = player->mo->FindInventory (type);

				// [TP]
				if ( puzzlepiece && ( NETWORK_GetState( ) == NETSTATE_SERVER ))
					SERVERCOMMANDS_TakeInventory( player - players, type, 0 );

				if (puzzlepiece)
					puzzlepiece->Destroy ();
			}
		}

		if (!takeall)
			return;
	}

	if (takeall)
		return;

	type = PClass::FindClass (name);
	if (type == NULL || !type->IsDescendantOf (RUNTIME_CLASS (AInventory)))
	{
		if (player == &players[consoleplayer])
			Printf ("Unknown item \"%s\"\n", name);
	}
	else
	{
		AInventory *inventory = player->mo->FindInventory (type);

		if (inventory != NULL)
		{
			inventory->Amount -= amount ? amount : 1;

			// [TP]
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_TakeInventory( player - players, type, inventory->Amount );

			if (inventory->Amount <= 0)
			{
				if (inventory->ItemFlags & IF_KEEPDEPLETED)
				{
					inventory->Amount = 0;
				}
				else
				{
					inventory->Destroy ();
				}
			}
		}
	}
	return;
}

class DSuicider : public DThinker
{
	DECLARE_CLASS(DSuicider, DThinker)
	HAS_OBJECT_POINTERS;
public:
	TObjPtr<APlayerPawn> Pawn;

	void Tick()
	{
		// [BB] Added safety check.
		if ( Pawn )
		{
			Pawn->flags |= MF_SHOOTABLE;
			Pawn->flags2 &= ~MF2_INVULNERABLE;
			// Store the player's current damage factor, to restore it later.
			fixed_t plyrdmgfact = Pawn->DamageFactor;
			Pawn->DamageFactor = 65536;
			P_DamageMobj (Pawn, Pawn, Pawn, TELEFRAG_DAMAGE, NAME_Suicide);
			Pawn->DamageFactor = plyrdmgfact;
			if (Pawn->health <= 0)
			{
				Pawn->flags &= ~MF_SHOOTABLE;
			}
		}
		Destroy();
	}
	// You'll probably never be able to catch this in a save game, but
	// just in case, add a proper serializer.
	void Serialize(FArchive &arc)
	{ 
		Super::Serialize(arc);
		arc << Pawn;
	}
};

IMPLEMENT_POINTY_CLASS(DSuicider)
 DECLARE_POINTER(Pawn)
END_POINTERS

void cht_Suicide (player_t *plyr)
{
	// If this cheat was initiated by the suicide ccmd, and this is a single
	// player game, the CHT_SUICIDE will be processed before the tic is run,
	// so the console has not gone up yet. Use a temporary thinker to delay
	// the suicide until the game ticks so that death noises can be heard on
	// the initial tick.
	if (plyr->mo != NULL)
	{
		DSuicider *suicide = new DSuicider;
		suicide->Pawn = plyr->mo;
		GC::WriteBarrier(suicide, suicide->Pawn);
	}
}


CCMD (mdk)
{
	if (CheckCheatmode ())
		return;

	// [Dusk] Online handling for mdk
	if ( NETWORK_GetState() == NETSTATE_CLIENT )
	{
		CLIENTCOMMANDS_GenericCheat( CHT_MDK );
		return;
	}

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_MDK);
}
