/*
#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "gi.h"
#include "doomstat.h"
*/

#define HEAL_RADIUS_DIST	255*FRACUNIT

static FRandom pr_healradius ("HealRadius");

// Healing Radius Artifact --------------------------------------------------

class AArtiHealingRadius : public AInventory
{
	DECLARE_CLASS (AArtiHealingRadius, AInventory)
public:
	bool Use (bool pickup);
};

IMPLEMENT_CLASS (AArtiHealingRadius)

bool AArtiHealingRadius::Use (bool pickup)
{
	// [Dusk] If we got here as the client, this function returned  true on the server.
	// Thus, it shall return true here as well. Furthermore, the client shouldn't
	// actually execute anything here, since the effects include manipulating health/armor
	// and stuff like that. Therefore, just return true.
	if ( NETWORK_InClientMode() )
		return true;

	bool effective = false;
	int mode = Owner->GetClass()->Meta.GetMetaInt(APMETA_HealingRadius);

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] &&
			players[i].mo != NULL &&
			players[i].mo->health > 0 &&
			P_AproxDistance (players[i].mo->x - Owner->x, players[i].mo->y - Owner->y) <= HEAL_RADIUS_DIST)
		{
			// Q: Is it worth it to make this selectable as a player property?
			// A: Probably not - but it sure doesn't hurt.
			bool gotsome=false;
			switch (mode)
			{
			case NAME_Armor:
				for (int j = 0; j < 4; ++j)
				{
					AHexenArmor *armor = Spawn<AHexenArmor> (0,0,0, NO_REPLACE);
					armor->health = j;
					armor->Amount = 1;
					if (!armor->CallTryPickup (players[i].mo))
					{
						armor->Destroy ();
					}
					else
					{
						gotsome = true;

						// [Dusk] As the server, sync the new armor values
						if ( NETWORK_GetState () == NETSTATE_SERVER )
							SERVERCOMMANDS_SyncHexenArmorSlots( i );
					}
				}
				break;

			case NAME_Mana:
			{
				int amount = 50 + (pr_healradius() % 50);

				if (players[i].mo->GiveAmmo (PClass::FindClass(NAME_Mana1), amount) ||
					players[i].mo->GiveAmmo (PClass::FindClass(NAME_Mana2), amount))
				{
					gotsome = true;

					// [Dusk] As the server, sync the new ammo counts.
					if ( NETWORK_GetState () == NETSTATE_SERVER )
						SERVERCOMMANDS_SyncPlayerAmmoAmount( i );
				}
				break;
			}

			default:
			//case NAME_Health:
				gotsome = P_GiveBody (players[i].mo, 50 + (pr_healradius()%50));

				// [Dusk] As the server, sync the new health count
				if ( NETWORK_GetState () == NETSTATE_SERVER && gotsome )
					SERVERCOMMANDS_SetPlayerHealth( i );
				break;
			}
			if (gotsome)
			{
				S_Sound (players[i].mo, CHAN_AUTO, "MysticIncant", 1, ATTN_NORM, true);	// [TP] Inform the clients.
				effective=true;
			}
		}
	}
	return effective;

}
