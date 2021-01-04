/*
** a_movingcamera.cpp
** Cameras that move and related neat stuff
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "doomstat.h"
#include "farchive.h"
// [BB] New #includes.
#include "a_movingcamera.h"
#include "cl_demo.h"
#include "cl_main.h"
#include "network.h"
#include "sv_commands.h"

/*
== InterpolationPoint: node along a camera's path
==
== args[0] = pitch
== args[1] = time (in octics) to get here from previous node
== args[2] = time (in octics) to stay here before moving to next node
== args[3] = low byte of next node's tid
== args[4] = high byte of next node's tid
*/

// [BB] Moved to a_movingcamera.h and commented out linewise to ensure merge conflicts
// if the code is changed in ZDoom.
//class AInterpolationPoint : public AActor
//{
//	DECLARE_CLASS (AInterpolationPoint, AActor)
//	HAS_OBJECT_POINTERS
//public:
//	void BeginPlay ();
//	void HandleSpawnFlags ();
//	void Tick () {}		// Nodes do no thinking
//	AInterpolationPoint *ScanForLoop ();
//	void FormChain ();
//
//	void Serialize (FArchive &arc);
//
//	TObjPtr<AInterpolationPoint> Next;
//};

IMPLEMENT_POINTY_CLASS (AInterpolationPoint)
 DECLARE_POINTER (Next)
END_POINTERS

void AInterpolationPoint::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Next;
}

void AInterpolationPoint::BeginPlay ()
{
	Super::BeginPlay ();
	Next = NULL;
}

void AInterpolationPoint::HandleSpawnFlags ()
{
	// Spawn flags mean nothing to an interpolation point
}

void AInterpolationPoint::FormChain ()
{
	if (flags & MF_AMBUSH)
		return;

	flags |= MF_AMBUSH;

	TActorIterator<AInterpolationPoint> iterator (args[3] + 256 * args[4]);
	Next = iterator.Next ();

	if (Next == this)	// Don't link to self
		Next = iterator.Next ();

	if (Next == NULL && (args[3] | args[4]))
		Printf ("Can't find target for camera node %d\n", tid);

	pitch = (signed int)((char)args[0]) * ANGLE_1;
	if (pitch <= -ANGLE_90)
		pitch = -ANGLE_90 + ANGLE_1;
	else if (pitch >= ANGLE_90)
		pitch = ANGLE_90 - ANGLE_1;

	if (Next != NULL)
		Next->FormChain ();
}

// Return the node (if any) where a path loops, relative to this one.
AInterpolationPoint *AInterpolationPoint::ScanForLoop ()
{
	AInterpolationPoint *node = this;
	while (node->Next && node->Next != this && node->special1 == 0)
	{
		node->special1 = 1;
		node = node->Next;
	}
	return node->Next == this ? node : NULL;
}

/*
== InterpolationSpecial: Holds a special to execute when a
==  PathFollower reaches an InterpolationPoint of the same TID.
*/

class AInterpolationSpecial : public AActor
{
	DECLARE_CLASS (AInterpolationSpecial, AActor)
public:
	void Tick () {}		// Does absolutely nothing itself
};

IMPLEMENT_CLASS (AInterpolationSpecial)

/*
== PathFollower: something that follows a camera path
==		Base class for some moving cameras
==
== args[0] = low byte of first node in path's tid
== args[1] = high byte of first node's tid
== args[2] = bit 0 = follow a linear path (rather than curved)
==			 bit 1 = adjust angle
==			 bit 2 = adjust pitch
==			 bit 3 = aim in direction of motion
==
== Also uses:
==	target = first node in path
==	lastenemy = node prior to first node (if looped)
*/

// [BB] Moved to a_movingcamera.h and commented out linewise to ensure merge conflicts
// if the code is changed in ZDoom.
//class APathFollower : public AActor
//{
//	DECLARE_CLASS (APathFollower, AActor)
//	HAS_OBJECT_POINTERS
//public:
//	void BeginPlay ();
//	void PostBeginPlay ();
//	void Tick ();
//	void Activate (AActor *activator);
//	void Deactivate (AActor *activator);
//protected:
//	float Splerp (float p1, float p2, float p3, float p4);
//	float Lerp (float p1, float p2);
//	virtual bool Interpolate ();
//	virtual void NewNode ();
//
//	void Serialize (FArchive &arc);
//
//	bool bActive, bJustStepped;
//	TObjPtr<AInterpolationPoint> PrevNode, CurrNode;
//	float Time;		// Runs from 0.0 to 1.0 between CurrNode and CurrNode->Next
//	int HoldTime;
//
//	bool bPostBeginPlayCalled;
//	bool bActivateCalledBeforePostBeginPlay;
//};

IMPLEMENT_POINTY_CLASS (APathFollower)
 DECLARE_POINTER (PrevNode)
 DECLARE_POINTER (CurrNode)
END_POINTERS

void APathFollower::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << bActive << bJustStepped << PrevNode << CurrNode << Time << HoldTime;
	// [BB] Zandronum specific stuff.
	arc << bPostBeginPlayCalled << bActivateCalledBeforePostBeginPlay << lServerPrevNodeId << lServerCurrNodeId << fServerTime;
}

// Interpolate between p2 and p3 along a Catmull-Rom spline
// http://research.microsoft.com/~hollasch/cgindex/curves/catmull-rom.html
float APathFollower::Splerp (float p1, float p2, float p3, float p4)
{
	float t = Time;
	float res = 2*p2;
	res += (p3 - p1) * Time;
	t *= Time;
	res += (2*p1 - 5*p2 + 4*p3 - p4) * t;
	t *= Time;
	res += (3*p2 - 3*p3 + p4 - p1) * t;
	return 0.5f * res;
}

// Linearly interpolate between p1 and p2
float APathFollower::Lerp (float p1, float p2)
{
	return p1 + Time * (p2 - p1);
}

void APathFollower::BeginPlay ()
{
	Super::BeginPlay ();
	PrevNode = CurrNode = NULL;
	bActive = false;
	// [BB] Init custom Zandronum stuff.
	bPostBeginPlayCalled = false;
	bActivateCalledBeforePostBeginPlay = false;
	lServerPrevNodeId = lServerCurrNodeId = -1;
	fServerTime = 0;
}

void APathFollower::PostBeginPlay ()
{
	bPostBeginPlayCalled = true;

	// Find first node of path
	TActorIterator<AInterpolationPoint> iterator (args[0] + 256 * args[1]);
	AInterpolationPoint *node = iterator.Next ();
	AInterpolationPoint *prevnode;

	target = node;

	if (node == NULL)
	{
		Printf ("PathFollower %d: Can't find interpolation pt %d\n",
			tid, args[0] + 256 * args[1]);
		return;
	}

	// Verify the path has enough nodes
	node->FormChain ();
	if (args[2] & 1)
	{	// linear path; need 2 nodes
		if (node->Next == NULL)
		{
			Printf ("PathFollower %d: Path needs at least 2 nodes\n", tid);
			return;
		}
		lastenemy = NULL;
	}
	else
	{	// spline path; need 4 nodes
		if (node->Next == NULL ||
			node->Next->Next == NULL ||
			node->Next->Next->Next == NULL)
		{
			Printf ("PathFollower %d: Path needs at least 4 nodes\n", tid);
			return;
		}
		// If the first node is in a loop, we can start there.
		// Otherwise, we need to start at the second node in the path.
		prevnode = node->ScanForLoop ();
		if (prevnode == NULL || prevnode->Next != node)
		{
			lastenemy = target;
			target = node->Next;
		}
		else
		{
			lastenemy = prevnode;
		}
	}

	// [BB] ATTENTION: Ugly hack here! Find a cleaner way to do this!
	// APathFollower::Activate does not work if it's called before APathFollower::PostBeginPlay.
	// In the client/server mode it's possible, that the functions are called in this order
	// on the client, so apply a workaround for this here. If Activate has been called before
	// PostBeginPlay we have to call Actiate again here.
	if (( bActivateCalledBeforePostBeginPlay ) && ( this->IsKindOf ( PClass::FindClass("ActorMover" ) ) == false ) &&
		NETWORK_InClientMode() )
	{
		APathFollower::Activate (NULL);
		bActivateCalledBeforePostBeginPlay = false;
	}
}

void APathFollower::Deactivate (AActor *activator)
{
	bActive = false;
}

void APathFollower::Activate (AActor *activator)
{
	// [BB] Hack.
	if ( !bPostBeginPlayCalled )
		bActivateCalledBeforePostBeginPlay = true;

	// [BB] Added !bActivateCalledBeforePostBeginPlay check.
	if (!bActive && !bActivateCalledBeforePostBeginPlay)
	{
		CurrNode = barrier_cast<AInterpolationPoint *>(target);
		PrevNode = barrier_cast<AInterpolationPoint *>(lastenemy);

		// [BB] Possibly init as instructed by the server.
		if ( NETWORK_InClientMode() )
		{
			if ( lServerCurrNodeId != -1 )
			{
				CurrNode = static_cast<AInterpolationPoint *> ( CLIENT_FindThingByNetID ( lServerCurrNodeId ) );
				lServerCurrNodeId = -1;
			}
			if ( lServerPrevNodeId != -1 )
			{
				PrevNode = static_cast<AInterpolationPoint *> ( CLIENT_FindThingByNetID ( lServerPrevNodeId ) );
				lServerPrevNodeId = -1;
			}
		}

		if (CurrNode != NULL)
		{
			NewNode ();
			SetOrigin (CurrNode->x, CurrNode->y, CurrNode->z);
			Time = 0.f;
			HoldTime = 0;
			bJustStepped = true;
			bActive = true;
		}

		// [BB] Possibly init Time as instructed by the server.
		if ( NETWORK_InClientMode() && fServerTime )
		{
			Time = fServerTime;
			fServerTime = 0;
		}
	}
	// If we're the server, tell clients to activate this actor.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		SERVERCOMMANDS_ThingActivate( this, activator );
}

void APathFollower::Tick ()
{
	if (!bActive)
		return;

	if (bJustStepped)
	{
		bJustStepped = false;
		if (CurrNode->args[2])
		{
			HoldTime = gametic + CurrNode->args[2] * TICRATE / 8;
			x = CurrNode->x;
			y = CurrNode->y;
			z = CurrNode->z;
		}
	}

	if (HoldTime > gametic)
		return;

	// Splines must have a previous node.
	if (PrevNode == NULL && !(args[2] & 1))
	{
		bActive = false;
		return;
	}

	// All paths must have a current node.
	if (CurrNode->Next == NULL)
	{
		bActive = false;
		return;
	}

	if (Interpolate ())
	{
		Time += 8.f / ((float)CurrNode->args[1] * (float)TICRATE);
		if (Time > 1.f)
		{
			Time -= 1.f;
			bJustStepped = true;
			PrevNode = CurrNode;
			CurrNode = CurrNode->Next;
			if (CurrNode != NULL)
				NewNode ();
			if (CurrNode == NULL || CurrNode->Next == NULL)
				Deactivate (this);
			if ((args[2] & 1) == 0 && CurrNode->Next->Next == NULL)
				Deactivate (this);
		}
	}
}

void APathFollower::NewNode ()
{
	TActorIterator<AInterpolationSpecial> iterator (CurrNode->tid);
	AInterpolationSpecial *spec;

	while ( (spec = iterator.Next ()) )
	{
		P_ExecuteSpecial(spec->special, NULL, NULL, false, false,
			spec->args[0], spec->args[1], spec->args[2], spec->args[3], spec->args[4]);
	}
}

bool APathFollower::Interpolate ()
{
	fixed_t dx = 0, dy = 0, dz = 0;

	if ((args[2] & 8) && Time > 0.f)
	{
		dx = x;
		dy = y;
		dz = z;
	}

	if (CurrNode->Next==NULL) return false;

	UnlinkFromWorld ();
	if (args[2] & 1)
	{	// linear
		x = FLOAT2FIXED(Lerp (FIXED2FLOAT(CurrNode->x), FIXED2FLOAT(CurrNode->Next->x)));
		y = FLOAT2FIXED(Lerp (FIXED2FLOAT(CurrNode->y), FIXED2FLOAT(CurrNode->Next->y)));
		z = FLOAT2FIXED(Lerp (FIXED2FLOAT(CurrNode->z), FIXED2FLOAT(CurrNode->Next->z)));
	}
	else
	{	// spline
		if (CurrNode->Next->Next==NULL) return false;

		x = FLOAT2FIXED(Splerp (FIXED2FLOAT(PrevNode->x), FIXED2FLOAT(CurrNode->x),
								FIXED2FLOAT(CurrNode->Next->x), FIXED2FLOAT(CurrNode->Next->Next->x)));
		y = FLOAT2FIXED(Splerp (FIXED2FLOAT(PrevNode->y), FIXED2FLOAT(CurrNode->y),
								FIXED2FLOAT(CurrNode->Next->y), FIXED2FLOAT(CurrNode->Next->Next->y)));
		z = FLOAT2FIXED(Splerp (FIXED2FLOAT(PrevNode->z), FIXED2FLOAT(CurrNode->z),
								FIXED2FLOAT(CurrNode->Next->z), FIXED2FLOAT(CurrNode->Next->Next->z)));
	}
	LinkToWorld ();

	if (args[2] & 6)
	{
		if (args[2] & 8)
		{
			if (args[2] & 1)
			{ // linear
				dx = CurrNode->Next->x - CurrNode->x;
				dy = CurrNode->Next->y - CurrNode->y;
				dz = CurrNode->Next->z - CurrNode->z;
			}
			else if (Time > 0.f)
			{ // spline
				dx = x - dx;
				dy = y - dy;
				dz = z - dz;
			}
			else
			{
				int realarg = args[2];
				args[2] &= ~(2|4|8);
				Time += 0.1f;
				dx = x;
				dy = y;
				dz = z;
				Interpolate ();
				Time -= 0.1f;
				args[2] = realarg;
				dx = x - dx;
				dy = y - dy;
				dz = z - dz;
				x -= dx;
				y -= dy;
				z -= dz;
			}
			if (args[2] & 2)
			{ // adjust yaw
				angle = R_PointToAngle2 (0, 0, dx, dy);
			}
			if (args[2] & 4)
			{ // adjust pitch; use floats for precision
				float fdx = FIXED2FLOAT(dx);
				float fdy = FIXED2FLOAT(dy);
				float fdz = FIXED2FLOAT(-dz);
				float dist = (float)sqrt (fdx*fdx + fdy*fdy);
				float ang = dist != 0.f ? (float)atan2 (fdz, dist) : 0;
				pitch = (angle_t)(ang * 2147483648.f / PI);
			}
		}
		else
		{
			if (args[2] & 2)
			{ // interpolate angle
				float angle1 = (float)CurrNode->angle;
				float angle2 = (float)CurrNode->Next->angle;
				if (angle2 - angle1 <= -2147483648.f)
				{
					float lerped = Lerp (angle1, angle2 + 4294967296.f);
					if (lerped >= 4294967296.f)
					{
						angle = (angle_t)(lerped - 4294967296.f);
					}
					else
					{
						angle = (angle_t)lerped;
					}
				}
				else if (angle2 - angle1 >= 2147483648.f)
				{
					float lerped = Lerp (angle1, angle2 - 4294967296.f);
					if (lerped < 0.f)
					{
						angle = (angle_t)(lerped + 4294967296.f);
					}
					else
					{
						angle = (angle_t)lerped;
					}
				}
				else
				{
					angle = (angle_t)Lerp (angle1, angle2);
				}
			}
			if (args[2] & 1)
			{ // linear
				if (args[2] & 4)
				{ // interpolate pitch
					pitch = FLOAT2FIXED(Lerp (FIXED2FLOAT(CurrNode->pitch), FIXED2FLOAT(CurrNode->Next->pitch)));
				}
			}
			else
			{ // spline
				if (args[2] & 4)
				{ // interpolate pitch
					pitch = FLOAT2FIXED(Splerp (FIXED2FLOAT(PrevNode->pitch), FIXED2FLOAT(CurrNode->pitch),
						FIXED2FLOAT(CurrNode->Next->pitch), FIXED2FLOAT(CurrNode->Next->Next->pitch)));
				}
			}
		}
	}

	return true;
}

// [BB]
bool APathFollower::IsActive () const
{
	return ( bActive );
}

/*
== ActorMover: Moves any actor along a camera path
==
== Same as PathFollower, except
== args[2], bit 7: make nonsolid
== args[3] = tid of thing to move
==
== also uses:
==	tracer = thing to move
*/

class AActorMover : public APathFollower
{
	DECLARE_CLASS (AActorMover, APathFollower)
public:
	void BeginPlay();
	void PostBeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
protected:
	bool Interpolate ();
};

IMPLEMENT_CLASS (AActorMover)

void AActorMover::BeginPlay()
{
	// [BB] The Zandronum specific members of APathFollower must be initialized.
	Super::BeginPlay ();

	ChangeStatNum(STAT_ACTORMOVER);
}

void AActorMover::PostBeginPlay ()
{
	Super::PostBeginPlay ();

	TActorIterator<AActor> iterator (args[3]);
	tracer = iterator.Next ();

	if (tracer == NULL)
	{
		Printf ("ActorMover %d: Can't find target %d\n", tid, args[3]);
	}
	else
	{
		special1 = tracer->flags;
		special2 = tracer->flags2;
	}

	// [BB] ATTENTION: Ugly hack here! Find a cleaner way to do this!
	// AActorMover::Activate does not work if it's called before AActorMover::PostBeginPlay.
	// In the client/server mode it's possible, that the functions are called in this order
	// on the client, so apply a workaround for this here. If Activate has been called before
	// PostBeginPlay we have to call Actiate again here.
	if (( bActivateCalledBeforePostBeginPlay ) &&
		NETWORK_InClientMode() )
	{
		bActivateCalledBeforePostBeginPlay = false;
		Activate (NULL);
	}
}

bool AActorMover::Interpolate ()
{
	if (tracer == NULL)
		return true;

	if (Super::Interpolate ())
	{
		fixed_t savedz = tracer->z;
		tracer->z = z;
		if (!P_TryMove (tracer, x, y, true))
		{
			tracer->z = savedz;
			return false;
		}

		if (args[2] & 2)
			tracer->angle = angle;
		if (args[2] & 4)
			tracer->pitch = pitch;

		return true;
	}
	return false;
}

void AActorMover::Activate (AActor *activator)
{
	// [BB] Hack.
	if ( !bPostBeginPlayCalled )
		bActivateCalledBeforePostBeginPlay = true;

	if (tracer == NULL || bActive)
		return;

	Super::Activate (activator);
	special1 = tracer->flags;
	special2 = tracer->flags2;
	tracer->flags |= MF_NOGRAVITY;
	if (args[2] & 128)
	{
		tracer->UnlinkFromWorld ();
		tracer->flags |= MF_NOBLOCKMAP;
		tracer->flags &= ~MF_SOLID;
		tracer->LinkToWorld ();
	}
	if (tracer->flags3 & MF3_ISMONSTER)
	{
		tracer->flags2 |= MF2_INVULNERABLE | MF2_DORMANT;
	}
	// Don't let the renderer interpolate between the actor's
	// old position and its new position.
	Interpolate ();
	tracer->PrevX = tracer->x;
	tracer->PrevY = tracer->y;
	tracer->PrevZ = tracer->z;
	tracer->PrevAngle = tracer->angle;
	tracer->PrevPitch = tracer->pitch;
}

void AActorMover::Deactivate (AActor *activator)
{
	if (bActive)
	{
		Super::Deactivate (activator);
		if (tracer != NULL)
		{
			tracer->UnlinkFromWorld ();
			tracer->flags = special1;
			tracer->LinkToWorld ();
			tracer->flags2 = special2;
		}
	}
}

/*
== MovingCamera: Moves any actor along a camera path
==
== Same as PathFollower, except
== args[3] = tid of thing to look at (0 if none)
==
== Also uses:
==	tracer = thing to look at
*/

class AMovingCamera : public APathFollower
{
	DECLARE_CLASS (AMovingCamera, APathFollower)
	HAS_OBJECT_POINTERS
public:
	void PostBeginPlay ();

	void Serialize (FArchive &arc);
protected:
	bool Interpolate ();

	TObjPtr<AActor> Activator;
};

IMPLEMENT_POINTY_CLASS (AMovingCamera)
 DECLARE_POINTER (Activator)
END_POINTERS

void AMovingCamera::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Activator;
}

void AMovingCamera::PostBeginPlay ()
{
	Super::PostBeginPlay ();

	Activator = NULL;
	if (args[3] != 0)
	{
		TActorIterator<AActor> iterator (args[3]);
		tracer = iterator.Next ();
		if (tracer == NULL)
		{
			Printf ("MovingCamera %d: Can't find thing %d\n", tid, args[3]);
		}
	}
}

bool AMovingCamera::Interpolate ()
{
	if (tracer == NULL)
		return Super::Interpolate ();

	if (Super::Interpolate ())
	{
		angle = R_PointToAngle2 (x, y, tracer->x, tracer->y);

		if (args[2] & 4)
		{ // Also aim camera's pitch; use floats for precision
			float dx = FIXED2FLOAT(x - tracer->x);
			float dy = FIXED2FLOAT(y - tracer->y);
			float dz = FIXED2FLOAT(z - tracer->z - tracer->height/2);
			float dist = (float)sqrt (dx*dx + dy*dy);
			float ang = dist != 0.f ? (float)atan2 (dz, dist) : 0;
			pitch = (angle_t)(ang * 2147483648.f / PI);
		}

		return true;
	}
	return false;
}
