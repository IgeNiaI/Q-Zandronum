/*
** p_things.cpp
** ACS-accessible thing utilities
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#include "doomtype.h"
#include "p_local.h"
#include "info.h"
#include "s_sound.h"
#include "tables.h"
#include "doomstat.h"
#include "m_random.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "a_sharedglobal.h"
#include "gi.h"
#include "templates.h"
#include "g_level.h"
// [BC] New #includes.
#include "a_doomglobal.h"
#include "sv_commands.h"
#include "team.h"
#include "a_keys.h"
#include "invasion.h"
#include "cl_demo.h"
#include "cooperative.h"

// Set of spawnable things for the Thing_Spawn and Thing_Projectile specials.
TMap<int, const PClass *> SpawnableThings;

static FRandom pr_leadtarget ("LeadTarget");

bool P_Thing_Spawn (int tid, AActor *source, int type, angle_t angle, bool fog, int newtid)
{
	int rtn = 0;
	const PClass *kind;
	AActor *spot, *mobj;
	FActorIterator iterator (tid);

	kind = P_GetSpawnableType(type);

	if (kind == NULL)
		return false;

	// Handle decorate replacements.
	kind = kind->GetReplacement();

	if ((GetDefaultByType (kind)->flags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS)))
		return false;

	if (tid == 0)
	{
		spot = source;
	}
	else
	{
		spot = iterator.Next();
	}
	while (spot != NULL)
	{
		mobj = Spawn (kind, spot->x, spot->y, spot->z, ALLOW_REPLACE);

		if (mobj != NULL)
		{
			DWORD oldFlags2 = mobj->flags2;
			mobj->flags2 |= MF2_PASSMOBJ;
			// [BC] Potentially spawn this thing even if it's going to be blocked.
			bool	bSpawn = true;

			// [BC] Don't spawn it if it doesn't have a good place to spawn.
			if ( P_TestMobjLocation( mobj ) == false )
				bSpawn = false;

			// [BC] However, SKULLTAG FLAGS/SKULLS MUST BE RESPAWNED!
			for ( ULONG i = 0; i < teams.Size( ); i++ )
			{
				if (( mobj->GetClass( ) == TEAM_GetItem( i )))
				{
					bSpawn = true;
					break;
				}
			}

			if ( bSpawn )
			{
				rtn++;
				mobj->angle = (angle != ANGLE_MAX ? angle : spot->angle);
				if (fog)
				{
					// [BC]
					AActor	*pFog;

					pFog = Spawn<ATeleportFog> (spot->x, spot->y, spot->z + TELEFOGHEIGHT, ALLOW_REPLACE);

					// [BC] If we're the server, tell clients to spawn the thing.
					if (( NETWORK_GetState( ) == NETSTATE_SERVER ) && ( pFog ))
						SERVERCOMMANDS_SpawnThing( pFog );
				}

				// [BC] Respawned keys in Skulltag CANNOT be dropped items.
				if (( mobj->flags & MF_SPECIAL ) && ( mobj->GetClass( )->IsDescendantOf( RUNTIME_CLASS( AKey )) == false ))
					mobj->flags |= MF_DROPPED;	// Don't respawn
				mobj->tid = newtid;
				mobj->AddToHash ();
				mobj->flags2 = oldFlags2;

				// [BC] Spawn the actor to clients.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				{
					SERVERCOMMANDS_SpawnThing( mobj );

					// Check and see if it's important that the client know the angle of the object.
					if ( mobj->angle != 0 )
						SERVERCOMMANDS_SetThingAngle( mobj );
				}
			}
			else
			{
				// If this is a monster, subtract it from the total monster
				// count, because it already added to it during spawning.
				mobj->ClearCounters();
				mobj->Destroy ();
			}
		}
		spot = iterator.Next();
	}

	return rtn != 0;
}

// [BC] Added
// [RH] Fixed

bool P_MoveThing(AActor *source, fixed_t x, fixed_t y, fixed_t z, bool fog)
{
	fixed_t oldx, oldy, oldz;
	// [BC]
	AActor	*pFog;

	oldx = source->x;
	oldy = source->y;
	oldz = source->z;

	source->SetOrigin (x, y, z);
	if (P_TestMobjLocation (source))
	{
		if (fog)
		{
			pFog = Spawn<ATeleportFog> (x, y, z + TELEFOGHEIGHT, ALLOW_REPLACE);

			// [BC] If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );

			pFog = Spawn<ATeleportFog> (oldx, oldy, oldz + TELEFOGHEIGHT, ALLOW_REPLACE);

			// [BC] If we're the server, tell clients to spawn the fog.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_SpawnThing( pFog );
		}
		source->PrevX = x;
		source->PrevY = y;
		source->PrevZ = z;
		if (source == players[consoleplayer].camera)
		{
			R_ResetViewInterpolation();
		}

		ULONG ulFlags = 0;
		if ( oldx != source->x )
			ulFlags |= CM_X;
		if ( oldy != source->y )
			ulFlags |= CM_Y;
		if ( oldz != source->z )
			ulFlags |= CM_Z;

		// [BC] If we're the server, tell clients to move the object.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_MoveThing( source, ulFlags );

		return true;
	}
	else
	{
		source->SetOrigin (oldx, oldy, oldz);
		return false;
	}
}

bool P_Thing_Move (int tid, AActor *source, int mapspot, bool fog)
{
	AActor *target;

	if (tid != 0)
	{
		FActorIterator iterator1(tid);
		source = iterator1.Next();
	}
	FActorIterator iterator2 (mapspot);
	target = iterator2.Next ();

	if (source != NULL && target != NULL)
	{
		return P_MoveThing(source, target->x, target->y, target->z, fog);
	}
	return false;
}

bool P_Thing_Projectile (int tid, AActor *source, int type, const char *type_name, angle_t angle,
	fixed_t speed, fixed_t vspeed, int dest, AActor *forcedest, int gravity, int newtid,
	bool leadTarget)
{
	int rtn = 0;
	const PClass *kind;
	AActor *spot, *mobj, *targ = forcedest;
	FActorIterator iterator (tid);
	double fspeed = speed;
	int defflags3;
	// [BC]
	bool	bMissileExplode;

	if (type_name == NULL)
	{
		kind = P_GetSpawnableType(type);
	}
	else
	{
		kind = PClass::FindClass(type_name);
	}
	if (kind == NULL || kind->ActorInfo == NULL)
	{
		return false;
	}

	// Handle decorate replacements.
	kind = kind->GetReplacement();

	defflags3 = GetDefaultByType (kind)->flags3;
	if ((defflags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS)))
		return false;

	if (tid == 0)
	{
		spot = source;
	}
	else
	{
		spot = iterator.Next();
	}
	while (spot != NULL)
	{
		FActorIterator tit (dest);

		if (dest == 0 || (targ = tit.Next()))
		{
			do
			{
				fixed_t z = spot->z;
				if (defflags3 & MF3_FLOORHUGGER)
				{
					z = ONFLOORZ;
				}
				else if (defflags3 & MF3_CEILINGHUGGER)
				{
					z = ONCEILINGZ;
				}
				else if (z != ONFLOORZ)
				{
					z -= spot->floorclip;
				}
				mobj = Spawn (kind, spot->x, spot->y, z, ALLOW_REPLACE);

				if (mobj)
				{
					mobj->tid = newtid;
					mobj->AddToHash ();
					P_PlaySpawnSound(mobj, spot);
					if (gravity)
					{
						mobj->flags &= ~MF_NOGRAVITY;
						if (!(mobj->flags3 & MF3_ISMONSTER) && gravity == 1)
						{
							mobj->gravity = FRACUNIT/8;
						}
					}
					else
					{
						mobj->flags |= MF_NOGRAVITY;
					}
					mobj->target = spot;

					if (targ != NULL)
					{
						fixed_t spot[3] = { targ->x, targ->y, targ->z+targ->height/2 };
						FVector3 aim(float(spot[0] - mobj->x), float(spot[1] - mobj->y), float(spot[2] - mobj->z));

						if (leadTarget && speed > 0 && (targ->velx | targ->vely | targ->velz))
						{
							// Aiming at the target's position some time in the future
							// is basically just an application of the law of sines:
							//     a/sin(A) = b/sin(B)
							// Thanks to all those on the notgod phorum for helping me
							// with the math. I don't think I would have thought of using
							// trig alone had I been left to solve it by myself.

							FVector3 tvel(targ->velx, targ->vely, targ->velz);
							if (!(targ->flags & MF_NOGRAVITY) && targ->waterlevel < 3)
							{ // If the target is subject to gravity and not underwater,
							  // assume that it isn't moving vertically. Thanks to gravity,
							  // even if we did consider the vertical component of the target's
							  // velocity, we would still miss more often than not.
								tvel.Z = 0.0;
								if ((targ->velx | targ->vely) == 0)
								{
									goto nolead;
								}
							}
							double dist = aim.Length();
							double targspeed = tvel.Length();
							double ydotx = -aim | tvel;
							double a = acos (clamp (ydotx / targspeed / dist, -1.0, 1.0));
							double multiplier = double(pr_leadtarget.Random2())*0.1/255+1.1;
							double sinb = -clamp (targspeed*multiplier * sin(a) / fspeed, -1.0, 1.0);

							// Use the cross product of two of the triangle's sides to get a
							// rotation vector.
							FVector3 rv(tvel ^ aim);
							// The vector must be normalized.
							rv.MakeUnit();
							// Now combine the rotation vector with angle b to get a rotation matrix.
							FMatrix3x3 rm(rv, cos(asin(sinb)), sinb);
							// And multiply the original aim vector with the matrix to get a
							// new aim vector that leads the target.
							FVector3 aimvec = rm * aim;
							// And make the projectile follow that vector at the desired speed.
							double aimscale = fspeed / dist;
							mobj->velx = fixed_t (aimvec[0] * aimscale);
							mobj->vely = fixed_t (aimvec[1] * aimscale);
							mobj->velz = fixed_t (aimvec[2] * aimscale);
							mobj->angle = R_PointToAngle2 (0, 0, mobj->velx, mobj->vely);
						}
						else
						{
nolead:						mobj->angle = R_PointToAngle2 (mobj->x, mobj->y, targ->x, targ->y);
							aim.Resize (fspeed);
							mobj->velx = fixed_t(aim[0]);
							mobj->vely = fixed_t(aim[1]);
							mobj->velz = fixed_t(aim[2]);
						}
						if (mobj->flags2 & MF2_SEEKERMISSILE)
						{
							mobj->tracer = targ;
						}
					}
					else
					{
						mobj->angle = angle;
						mobj->velx = FixedMul (speed, finecosine[angle>>ANGLETOFINESHIFT]);
						mobj->vely = FixedMul (speed, finesine[angle>>ANGLETOFINESHIFT]);
						mobj->velz = vspeed;
					}
					// Set the missile's speed to reflect the speed it was spawned at.
					if (mobj->flags & MF_MISSILE)
					{
						mobj->Speed = fixed_t (sqrt (double(mobj->velx)*mobj->velx + double(mobj->vely)*mobj->vely + double(mobj->velz)*mobj->velz));
					}
					// Hugger missiles don't have any vertical velocity
					if (mobj->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER))
					{
						mobj->velz = 0;
					}
					if (mobj->flags & MF_SPECIAL)
					{
						mobj->flags |= MF_DROPPED;
					}
					bMissileExplode = false;
					if (mobj->flags & MF_MISSILE)
					{
						if (P_CheckMissileSpawn (mobj, spot->radius))
						{
							rtn = true;
						}
						else
							bMissileExplode = true;
					}
					else if (!P_TestMobjLocation (mobj))
					{
						// If this is a monster, subtract it from the total monster
						// count, because it already added to it during spawning.
						mobj->ClearCounters();
						mobj->Destroy ();
					}
					else
					{
						// It spawned fine.
						rtn = 1;
					}

					// [BC] Spawn this actor to clients. It must be spawned as a missile because
					// it can potentially have velocity, etc. 
					if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					{
						SERVERCOMMANDS_SpawnMissile( mobj );

						// Determine which flags we need to update.
						if ( mobj->flags != mobj->GetDefault( )->flags )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS );
						if ( mobj->flags2 != mobj->GetDefault( )->flags2 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS2 );
						if ( mobj->flags3 != mobj->GetDefault( )->flags3 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS3 );
						if ( mobj->flags4 != mobj->GetDefault( )->flags4 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS4 );
						if ( mobj->flags5 != mobj->GetDefault( )->flags5 )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGS5 );
						if ( mobj->ulSTFlags != mobj->GetDefault( )->ulSTFlags )
							SERVERCOMMANDS_SetThingFlags( mobj, FLAGSET_FLAGSST );
						// [BB] If necessary, also adjust gravity.
						if ( mobj->gravity != mobj->GetDefault( )->gravity )
							SERVERCOMMANDS_SetThingGravity( mobj );

						// For missiles that exploded when P_CheckMissileSpawn() was
						// called, we need to tell clients to explode the missile since
						// it wasn't actually spawned at that point.
						if ( bMissileExplode )
							SERVERCOMMANDS_MissileExplode( mobj, NULL );
					}
				}
			} while (dest != 0 && (targ = tit.Next()));
		}
		spot = iterator.Next();
	}

	return rtn != 0;
}

int P_Thing_Damage (int tid, AActor *whofor0, int amount, FName type)
{
	FActorIterator iterator (tid);
	int count = 0;
	AActor *actor;

	actor = (tid == 0 ? whofor0 : iterator.Next());
	while (actor)
	{
		AActor *next = tid == 0 ? NULL : iterator.Next ();
		if (actor->flags & MF_SHOOTABLE)
		{
			if (amount > 0)
			{
				P_DamageMobj (actor, NULL, whofor0, amount, type);
			}
			else if (actor->health < actor->SpawnHealth())
			{
				actor->health -= amount;
				if (actor->health > actor->SpawnHealth())
				{
					actor->health = actor->SpawnHealth();
				}
				if (actor->player != NULL)
				{
					actor->player->health = actor->health;
				}
			}
			count++;
		}
		actor = next;
	}
	return count;
}

void P_RemoveThing(AActor * actor)
{
	// Don't remove live players.
	if (actor->player == NULL || actor != actor->player->mo)
	{
		// [BC] DESTROY!
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DestroyThing( actor );

		// be friendly to the level statistics. ;)
		actor->ClearCounters();

		// [BB] Only destroy the actor if it's not needed for a map reset. Otherwise just hide it.
		actor->HideOrDestroyIfSafe ();
	}
}

// [BB] Added byClient: If the server instructs the client to raise
// a thing with SERVERCOMMANDS_SetThingState, the client has to ignore the
// P_CheckPosition check. For example this is relevant if an Archvile raised
// the thing.
// [EP] Ignore also the checks in AActor::GetRaiseState (in particular the
// 'tics != -1' one, because the client might get the wrong value).
bool P_Thing_Raise(AActor *thing, bool byClient)
{
	FState * RaiseState = byClient ? thing->FindState(NAME_Raise) : thing->GetRaiseState();	// [EP]
	if (RaiseState == NULL)
	{
		return true;	// monster doesn't have a raise state
	}
	
	AActor *info = thing->GetDefault ();

	thing->velx = thing->vely = 0;

	// [RH] Check against real height and radius
	fixed_t oldheight = thing->height;
	fixed_t oldradius = thing->radius;
	int oldflags = thing->flags;

	thing->flags |= MF_SOLID;
	thing->height = info->height;	// [RH] Use real height
	thing->radius = info->radius;	// [RH] Use real radius
	if (!P_CheckPosition (thing, thing->x, thing->y) && !byClient)
	{
		thing->flags = oldflags;
		thing->radius = oldradius;
		thing->height = oldheight;
		return false;
	}


	S_Sound (thing, CHAN_BODY, "vile/raise", 1, ATTN_IDLE);

	thing->Revive();

	// [BC] If we're the server, tell clients to put the thing into its raise state.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_SetThingState( thing, STATE_RAISE );

	thing->SetState (RaiseState);
	return true;
}

bool P_Thing_CanRaise(AActor *thing)
{
	FState * RaiseState = thing->GetRaiseState();
	if (RaiseState == NULL)
	{
		return false;
	}
	
	AActor *info = thing->GetDefault();

	// Check against real height and radius
	int oldflags = thing->flags;
	fixed_t oldheight = thing->height;
	fixed_t oldradius = thing->radius;

	thing->flags |= MF_SOLID;
	thing->height = info->height;
	thing->radius = info->radius;

	bool check = P_CheckPosition (thing, thing->x, thing->y);

	// Restore checked properties
	thing->flags = oldflags;
	thing->radius = oldradius;
	thing->height = oldheight;

	if (!check)
	{
		return false;
	}

	return true;
}

void P_Thing_SetVelocity(AActor *actor, fixed_t vx, fixed_t vy, fixed_t vz, bool add, bool setbob)
{
	if (actor != NULL)
	{
		if (!add)
		{
			actor->velx = actor->vely = actor->velz = 0;
			if (actor->player != NULL) actor->player->velx = actor->player->vely = 0;
		}
		actor->velx += vx;
		actor->vely += vy;
		actor->velz += vz;
		if (setbob && actor->player != NULL)
		{
			actor->player->velx += vx;
			actor->player->vely += vy;
		}
		// [Dusk] Update velocity
		SERVER_UpdateThingVelocity( actor, true );
	}
}

const PClass *P_GetSpawnableType(int spawnnum)
{
	if (spawnnum < 0)
	{ // A named arg from a UDMF map
		FName spawnname = FName(ENamedName(-spawnnum));
		if (spawnname.IsValidName())
		{
			return PClass::FindClass(spawnname);
		}
	}
	else
	{ // A numbered arg from a Hexen or UDMF map
		const PClass **type = SpawnableThings.CheckKey(spawnnum);
		if (type != NULL)
		{
			return *type;
		}
	}
	return NULL;
}

typedef TMap<int, const PClass *>::Pair SpawnablePair;

static int STACK_ARGS SpawnableSort(const void *a, const void *b)
{
	return (*((SpawnablePair **)a))->Key - (*((SpawnablePair **)b))->Key;
}

CCMD (dumpspawnables)
{
	TMapIterator<int, const PClass *> it(SpawnableThings);
	SpawnablePair *pair, **allpairs;
	int i = 0;

	// Sort into numerical order, since their arrangement in the map can
	// be in an unspecified order.
	allpairs = new TMap<int, const PClass *>::Pair *[SpawnableThings.CountUsed()];
	while (it.NextPair(pair))
	{
		allpairs[i++] = pair;
	}
	qsort(allpairs, i, sizeof(*allpairs), SpawnableSort);
	for (int j = 0; j < i; ++j)
	{
		pair = allpairs[j];
		Printf ("%d %s\n", pair->Key, pair->Value->TypeName.GetChars());
	}
	delete[] allpairs;
}

