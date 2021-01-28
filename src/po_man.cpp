
//**************************************************************************
//**
//** PO_MAN.C : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: po_man.c,v $
//** $Revision: 1.22 $
//** $Date: 95/09/28 18:20:56 $
//** $Author: cjr $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "i_system.h"
#include "w_wad.h"
#include "m_swap.h"
#include "m_bbox.h"
#include "tables.h"
#include "s_sndseq.h"
#include "a_sharedglobal.h"
#include "p_3dmidtex.h"
#include "p_lnspec.h"
#include "r_data/r_interpolate.h"
#include "g_level.h"
#include "po_man.h"
#include "p_setup.h"
#include "vectors.h"
#include "farchive.h"
// [BC] New #includes.
#include "cl_demo.h"
#include "network.h"
#include "version.h"
#include "sv_commands.h"
#include "cl_main.h"

// MACROS ------------------------------------------------------------------

#define PO_MAXPOLYSEGS 64

// TYPES -------------------------------------------------------------------

inline vertex_t *side_t::V1() const
{
	return this == linedef->sidedef[0]? linedef->v1 : linedef->v2;
}

inline vertex_t *side_t::V2() const
{
	return this == linedef->sidedef[0]? linedef->v2 : linedef->v1;
}


FArchive &operator<< (FArchive &arc, FPolyObj *&poly)
{
	return arc.SerializePointer (polyobjs, (BYTE **)&poly, sizeof(FPolyObj));
}

FArchive &operator<< (FArchive &arc, const FPolyObj *&poly)
{
	return arc.SerializePointer (polyobjs, (BYTE **)&poly, sizeof(FPolyObj));
}

inline FArchive &operator<< (FArchive &arc, podoortype_t &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (podoortype_t)val;
	return arc;
}

class FPolyMirrorIterator
{
	FPolyObj *CurPoly;
	int UsedPolys[100];	// tracks mirrored polyobjects we've seen
	int NumUsedPolys;

public:
	FPolyMirrorIterator(FPolyObj *poly);
	FPolyObj *NextMirror();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void PO_Init (void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// [BC]
FPolyObj *GetPolyobjByIndex( ULONG ulPolyIdx );
static void RotatePt (int an, fixed_t *x, fixed_t *y, fixed_t startSpotX,
	fixed_t startSpotY);
static void InitBlockMap (void);
static void IterFindPolySides (FPolyObj *po, side_t *side);
static void SpawnPolyobj (int index, int tag, int type);
static void TranslateToStartSpot (int tag, int originX, int originY);
static FPolyNode *NewPolyNode();
static void ReleaseAllPolyNodes();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern seg_t *segs;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

polyblock_t **PolyBlockMap;
FPolyObj *polyobjs; // list of all poly-objects on the level
int po_NumPolyobjs;
polyspawns_t *polyspawns; // [RH] Let P_SpawnMapThings() find our thingies for us

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<SDWORD> KnownPolySides;
static FPolyNode *FreePolyNodes;

// CODE --------------------------------------------------------------------



//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_POINTY_CLASS (DPolyAction)
	DECLARE_POINTER(m_Interpolation)
END_POINTERS

DPolyAction::DPolyAction ()
{
}

void DPolyAction::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_PolyObj << m_Speed << m_Dist << m_Interpolation;
}

DPolyAction::DPolyAction (int polyNum)
{
	m_PolyObj = polyNum;
	m_Speed = 0;
	m_Dist = 0;
	SetInterpolation ();
}

void DPolyAction::Destroy()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly->specialdata == this)
	{
		poly->specialdata = NULL;
	}

	StopInterpolation();
	Super::Destroy();
}

void DPolyAction::Stop()
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	SN_StopSequence(poly);
	Destroy();
}

player_t* DPolyAction::GetLastInstigator()
{
	return m_LastInstigator;
}

void DPolyAction::SetLastInstigator(player_t* Player)
{
	m_LastInstigator = Player;
}

void DPolyAction::SetInterpolation ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	m_Interpolation = poly->SetInterpolation();
}

void DPolyAction::StopInterpolation ()
{
	if (m_Interpolation != NULL)
	{
		m_Interpolation->DelRef();
		m_Interpolation = NULL;
	}
}

void DPolyAction::SetSpeed (LONG lSpeed)
{
	m_Speed = lSpeed;
}

LONG DPolyAction::GetDist ()
{
	return ( m_Dist );
}

void DPolyAction::SetDist (LONG lDist)
{
	m_Dist = lDist;
}

// [geNia] This should never be called.
void DPolyAction::RecordUnlagged (LONG lTick)
{
	Printf("WARNING: DPolyAction::RecordUnlagged was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

// [geNia] This should never be called.
void DPolyAction::ReconcileUnlagged (LONG lTick)
{
	Printf("WARNING: DPolyAction::ReconcileUnlagged was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

// [geNia] This should never be called.
void DPolyAction::RestoreUnlagged ()
{
	Printf("WARNING: DPolyAction::RestoreUnlagged was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

// [geNia] This should never be called.
void DPolyAction::RecordPredict (LONG lTick)
{
	Printf("WARNING: DPolyAction::RecordPredict was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

// [geNia] This should never be called.
void DPolyAction::RestorePredict (LONG lTick)
{
	Printf("WARNING: DPolyAction::RestorePredict was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

LONG DPolyAction::GetPolyObj ()
{
	return ( m_PolyObj );
}

// [WS] This should never be called.
void DPolyAction::UpdateToClient(ULONG ulClient)
{
	Printf("WARNING: DPolyAction::UpdateToClient was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

void DPolyAction::Predict()
{
	Printf("WARNING: DPolyAction::Predict was called. This should never happen! Please report this at the %s bug tracker!\n", GAMENAME);
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS (DRotatePoly)

DRotatePoly::DRotatePoly ()
{
}

DRotatePoly::DRotatePoly (int polyNum)
	: Super (polyNum)
{
	m_LastInstigator = NULL;
	
	if ( NETWORK_GetState() == NETSTATE_SERVER )
	{
		FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
		if (poly == NULL)
			return;

		m_restoreAngle = poly->angle;
	}
}

// [geNia]
void DRotatePoly::RecordUnlagged (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	m_unlaggedAngle[lTick] = poly->angle;
}

// [geNia]
void DRotatePoly::ReconcileUnlagged (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	m_restoreAngle = poly->angle;
	poly->RotatePolyobj(m_unlaggedAngle[lTick] - poly->angle);
}

// [geNia]
void DRotatePoly::RestoreUnlagged ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	poly->RotatePolyobj(m_restoreAngle - poly->angle);
}

// [geNia]
void DRotatePoly::RecordPredict (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	m_predictAngle[lTick] = poly->angle;
}

// [geNia]
void DRotatePoly::RestorePredict (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	poly->RotatePolyobj(m_predictAngle[lTick] - poly->angle);
}

// [BC]
void DRotatePoly::UpdateToClient(ULONG ulClient)
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	if (poly == NULL)
		return;

	SERVERCOMMANDS_DoRotatePoly( this, ulClient, SVCF_ONLYTHISCLIENT );
}

void DRotatePoly::Predict()
{
	// Use a version of gametic that's appropriate for both the current game and demos.
	ULONG TicsToPredict = gametic - CLIENTDEMO_GetGameticOffset( );

	// [geNia] This would mean that a negative amount of prediction tics is needed, so something is wrong.
	// So far it looks like the "lagging at connect / map start" prevented this from happening before.
	if ( CLIENT_GetLastConsolePlayerUpdateTick() > TicsToPredict)
		return;

	// How many ticks of prediction do we need?
	TicsToPredict = TicsToPredict - CLIENT_GetLastConsolePlayerUpdateTick( );

	while (TicsToPredict)
	{
		Tick();
		TicsToPredict--;
	}
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS (DMovePoly)

DMovePoly::DMovePoly ()
{
}

void DMovePoly::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Angle << m_xSpeed << m_ySpeed;
}

DMovePoly::DMovePoly (int polyNum)
	: Super (polyNum)
{
	m_Angle = 0;
	m_xSpeed = 0;
	m_ySpeed = 0;
	m_LastInstigator = NULL;

	if ( NETWORK_GetState() == NETSTATE_SERVER )
	{
		FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
		if (poly == NULL)
			return;

		m_restoreX = poly->StartSpot.x;
		m_restoreY = poly->StartSpot.y;
	}
}

void DMovePoly::UpdateToClient(ULONG ulClient)
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	if (poly == NULL)
		return;

	SERVERCOMMANDS_DoMovePoly( this, ulClient, SVCF_ONLYTHISCLIENT );
}

void DMovePoly::Predict()
{
	// Use a version of gametic that's appropriate for both the current game and demos.
	ULONG TicsToPredict = gametic - CLIENTDEMO_GetGameticOffset( );

	// [geNia] This would mean that a negative amount of prediction tics is needed, so something is wrong.
	// So far it looks like the "lagging at connect / map start" prevented this from happening before.
	if ( CLIENT_GetLastConsolePlayerUpdateTick() > TicsToPredict)
		return;

	// How many ticks of prediction do we need?
	TicsToPredict = TicsToPredict - CLIENT_GetLastConsolePlayerUpdateTick( );

	while (TicsToPredict)
	{
		Tick();
		TicsToPredict--;
	}
}

// [geNia]
void DMovePoly::RecordUnlagged (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	m_unlaggedX[lTick] = poly->StartSpot.x;
	m_unlaggedY[lTick] = poly->StartSpot.y;
}

// [geNia]
void DMovePoly::ReconcileUnlagged (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	m_restoreX = poly->StartSpot.x;
	m_restoreY = poly->StartSpot.y;
	poly->MovePolyobj(m_unlaggedX[lTick] - poly->StartSpot.x, m_unlaggedY[lTick] - poly->StartSpot.y, true);
}

// [geNia]
void DMovePoly::RestoreUnlagged ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	poly->MovePolyobj(m_restoreX - poly->StartSpot.x, m_restoreY - poly->StartSpot.y, true);
}

// [geNia]
void DMovePoly::RecordPredict (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	m_predictX[lTick] = poly->StartSpot.x;
	m_predictY[lTick] = poly->StartSpot.y;
}

// [geNia]
void DMovePoly::RestorePredict (LONG lTick)
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	poly->MovePolyobj(m_predictX[lTick] - poly->StartSpot.x, m_predictY[lTick] - poly->StartSpot.y, true);
}

fixed_t DMovePoly::GetXSpeed ()
{
	return ( m_xSpeed );
}

void DMovePoly::SetXSpeed (fixed_t lSpeed)
{
	m_xSpeed = lSpeed;
}

fixed_t DMovePoly::GetYSpeed ()
{
	return ( m_ySpeed );
}

void DMovePoly::SetYSpeed (fixed_t lSpeed)
{
	m_ySpeed = lSpeed;
}

angle_t DMovePoly::GetAngle ()
{
	return ( m_Angle );
}

void DMovePoly::SetAngle (angle_t lAngle)
{
	m_Angle = lAngle;
}

//==========================================================================
//
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DMovePolyTo)

DMovePolyTo::DMovePolyTo()
{
}

void DMovePolyTo::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << m_xSpeed << m_ySpeed << m_xTarget << m_yTarget;
}

DMovePolyTo::DMovePolyTo(int polyNum)
	: Super(polyNum)
{
	m_xSpeed = 0;
	m_ySpeed = 0;
	m_xTarget = 0;
	m_yTarget = 0;
	m_LastInstigator = NULL;
	for (int i = 0; i < UNLAGGEDTICS; i++)
		RecordUnlagged(i);
}

void DMovePolyTo::UpdateToClient(ULONG ulClient)
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	if (poly == NULL)
		return;

	SERVERCOMMANDS_DoMovePolyTo( this, ulClient, SVCF_ONLYTHISCLIENT );
}

void DMovePolyTo::Predict()
{
	// Use a version of gametic that's appropriate for both the current game and demos.
	ULONG TicsToPredict = gametic - CLIENTDEMO_GetGameticOffset( );

	// [geNia] This would mean that a negative amount of prediction tics is needed, so something is wrong.
	// So far it looks like the "lagging at connect / map start" prevented this from happening before.
	if ( CLIENT_GetLastConsolePlayerUpdateTick() > TicsToPredict)
		return;

	// How many ticks of prediction do we need?
	TicsToPredict = TicsToPredict - CLIENT_GetLastConsolePlayerUpdateTick( );

	while (TicsToPredict)
	{
		Tick();
		TicsToPredict--;
	}
}

fixed_t DMovePolyTo::GetXTarget ()
{
	return ( m_xTarget );
}

void DMovePolyTo::SetXTarget ( fixed_t lTarget )
{
	m_xTarget = lTarget;
}

fixed_t DMovePolyTo::GetYTarget ()
{
	return ( m_yTarget );
}

void DMovePolyTo::SetYTarget ( fixed_t lTarget )
{
	m_yTarget = lTarget;
}

fixed_t DMovePolyTo::GetXSpeed ()
{
	return ( m_xSpeed );
}

void DMovePolyTo::SetXSpeed ( fixed_t lSpeed )
{
	m_xSpeed = lSpeed;
}

fixed_t DMovePolyTo::GetYSpeed ()
{
	return ( m_ySpeed );
}

void DMovePolyTo::SetYSpeed ( fixed_t lSpeed )
{
	m_ySpeed = lSpeed;
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS (DPolyDoor)

DPolyDoor::DPolyDoor ()
{
}

void DPolyDoor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Direction << m_TotalDist << m_Tics << m_WaitTics << m_Type << m_Close;
}

DPolyDoor::DPolyDoor (int polyNum, podoortype_t type)
	: Super (polyNum), m_Type (type)
{
	m_Direction = 0;
	m_TotalDist = 0;
	m_Tics = 0;
	m_WaitTics = 0;
	m_Close = false;
}

void DPolyDoor::UpdateToClient(ULONG ulClient)
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	if (poly == NULL)
		return;

	SERVERCOMMANDS_DoPolyDoor( this, ulClient, SVCF_ONLYTHISCLIENT );
}

void DPolyDoor::Predict()
{
	// Use a version of gametic that's appropriate for both the current game and demos.
	ULONG TicsToPredict = gametic - CLIENTDEMO_GetGameticOffset( );

	// [geNia] This would mean that a negative amount of prediction tics is needed, so something is wrong.
	// So far it looks like the "lagging at connect / map start" prevented this from happening before.
	if ( CLIENT_GetLastConsolePlayerUpdateTick() > TicsToPredict)
		return;

	// How many ticks of prediction do we need?
	TicsToPredict = TicsToPredict - CLIENT_GetLastConsolePlayerUpdateTick( );

	while (TicsToPredict)
	{
		Tick();
		TicsToPredict--;
	}
}

podoortype_t DPolyDoor::GetType()
{
	return ( m_Type );
}

fixed_t DPolyDoor::GetX ()
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	if (poly == NULL)
		return 0;

	return ( poly->StartSpot.x );
}

fixed_t DPolyDoor::GetY ()
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	if (poly == NULL)
		return 0;

	return ( poly->StartSpot.y );
}

LONG DPolyDoor::GetTics ()
{
	return ( m_Tics );
}

void DPolyDoor::SetTics (LONG lTics)
{
	m_Tics = lTics;
}

LONG DPolyDoor::GetWaitTics ()
{
	return ( m_WaitTics );
}

void DPolyDoor::SetWaitTics (LONG lWaitTics)
{
	m_WaitTics = lWaitTics;
}

LONG DPolyDoor::GetDirection ()
{
	return ( m_Direction );
}

void DPolyDoor::SetDirection (LONG lDirection)
{
	m_Direction = lDirection;
}

LONG DPolyDoor::GetTotalDist ()
{
	return ( m_TotalDist );
}

void DPolyDoor::SetTotalDist (LONG lDist)
{
	m_TotalDist = lDist;
}

bool DPolyDoor::GetClose ()
{
	return ( m_Close );
}

void DPolyDoor::SetClose (bool bClose)
{
	m_Close = bClose;
}


// ===== Polyobj Event Code =====

//==========================================================================
//
// T_RotatePoly
//
//==========================================================================

void DRotatePoly::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL)
		return;

	// Don't let non-perpetual polyobjs overshoot their targets.
	if (m_Dist != -1 && (unsigned int)m_Dist < (unsigned int)abs(m_Speed))
	{
		m_Speed = m_Speed < 0 ? -m_Dist : m_Dist;
	}

	if (poly->RotatePolyobj (m_Speed))
	{
		if (m_Dist == -1)
		{ // perpetual polyobj
			return;
		}
		m_Dist -= abs(m_Speed);
		if (m_Dist == 0)
		{
			// [BC] Now that our destination has been reached, tell clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoRotatePoly( this );

			SN_StopSequence (poly);
			Destroy ();
		}
	}
	else if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		// [WS] The poly object is blocked, tell clients the rotation!
	{
		SERVERCOMMANDS_DoRotatePoly( this );
	}
}

//==========================================================================
//
// EV_RotatePoly
//
//==========================================================================

bool EV_RotatePoly (player_t *instigator, line_t *line, int polyNum, int speed, int byteAngle,
					int direction, bool overRide)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	DRotatePoly *pe;
	FPolyObj *poly;

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_RotatePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != NULL)
	{
		pe = NULL;

		if (poly->specialdata != NULL && !overRide)
		{ // poly is already in motion
			if ( poly->specialdata->IsKindOf(RUNTIME_CLASS(DRotatePoly)) )
				pe = (DRotatePoly*) poly->specialdata;
			else
				return false;
		}

		if ( pe == NULL )
		{
			pe = new DRotatePoly(poly->tag);
			poly->specialdata = pe;
		}

		pe->m_LastInstigator = instigator;
		if (byteAngle != 0)
		{
			if (byteAngle == 255)
			{
				pe->m_Dist = ~0;
			}
			else
			{
				pe->m_Dist = byteAngle*(ANGLE_90/64); // Angle
			}
		}
		else
		{
			pe->m_Dist = ANGLE_MAX-1;
		}
		pe->m_Speed = speed*direction*(ANGLE_90/(64<<3));
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		direction = -direction;	// Reverse the direction

		// [BC] If we're the server, tell clients to create the rotate poly.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoRotatePoly( pe );
	}
	return pe != NULL;	// Return true if something started moving.
}

//==========================================================================
//
// T_MovePoly
//
//==========================================================================

void DMovePoly::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly == NULL)
		return;

	if (poly->MovePolyobj (m_xSpeed, m_ySpeed))
	{
		int absSpeed = abs (m_Speed);
		m_Dist -= absSpeed;
		if (m_Dist <= 0)
		{
			SN_StopSequence (poly);
			Destroy ();

			// [BC] Now that our destination has been reached, tell clients.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoMovePoly( this );
		}
		else if (m_Dist < absSpeed)
		{
			m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
			m_xSpeed = FixedMul (m_Speed, finecosine[m_Angle]);
			m_ySpeed = FixedMul (m_Speed, finesine[m_Angle]);
		}
	}
	else if ( NETWORK_GetState ( ) == NETSTATE_SERVER )
	// [WS] The poly object is blocked, tell clients the position!
	{
		SERVERCOMMANDS_DoMovePoly( this );
	}
}

//==========================================================================
//
// EV_MovePoly
//
//==========================================================================

bool EV_MovePoly (player_t *instigator, line_t *line, int polyNum, int speed, angle_t angle,
				  fixed_t dist, bool overRide)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	DMovePoly *pe;
	FPolyObj *poly;
	angle_t an = angle;

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_MovePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != NULL)
	{
		pe = NULL;

		if (poly->specialdata != NULL && !overRide)
		{ // poly is already in motion
			if ( poly->specialdata->IsKindOf(RUNTIME_CLASS(DMovePoly)) )
				pe = (DMovePoly*) poly->specialdata;
			else
				return false;
		}

		if ( pe == NULL )
		{
			pe = new DMovePoly(poly->tag);
			poly->specialdata = pe;
		}

		if (pe->m_Dist > 0 )
			return false;

		pe->m_LastInstigator = instigator;
		pe->m_Dist = dist; // Distance
		pe->m_Speed = speed;
		pe->m_Angle = an >> ANGLETOFINESHIFT;
		pe->m_xSpeed = FixedMul (pe->m_Speed, finecosine[pe->m_Angle]);
		pe->m_ySpeed = FixedMul (pe->m_Speed, finesine[pe->m_Angle]);
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

		// [BC] If we're the server, tell clients to create the move poly.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoMovePoly( pe );

		// Do not interpolate very fast moving polyobjects. The minimum tic count is
		// 3 instead of 2, because the moving crate effect in Massmouth 2, Hostitality
		// that this fixes isn't quite fast enough to move the crate back to its start
		// in just 1 tic.
		if (dist/speed <= 2)
		{
			pe->StopInterpolation ();
		}

		an = an + ANGLE_180;	// Reverse the angle.
	}
	return pe != NULL;	// Return true if something started moving.
}

//==========================================================================
//
// DMovePolyTo :: Tick
//
//==========================================================================

void DMovePolyTo::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly == NULL)
		return;

	if (poly->MovePolyobj (m_xSpeed, m_ySpeed))
	{
		int absSpeed = abs (m_Speed);
		m_Dist -= absSpeed;
		if (m_Dist <= 0)
		{
			SN_StopSequence (poly);
			Destroy ();

			// [BC] If we're the server, tell clients to create the move poly.
			if ( NETWORK_GetState( ) == NETSTATE_SERVER )
				SERVERCOMMANDS_DoMovePolyTo( this );
		}
		else if (m_Dist < absSpeed)
		{
			m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
			m_xSpeed = m_xTarget - poly->StartSpot.x;
			m_ySpeed = m_yTarget - poly->StartSpot.y;
		}
	}
	else if ( NETWORK_GetState ( ) == NETSTATE_SERVER )
	// [WS] The poly object is blocked, tell clients the position!
	{
		SERVERCOMMANDS_DoMovePolyTo( this );
	}
}

//==========================================================================
//
// EV_MovePolyTo
//
//==========================================================================

bool EV_MovePolyTo(player_t *instigator, line_t *line, int polyNum, int speed, fixed_t targx, fixed_t targy, bool overRide)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	DMovePolyTo *pe;
	FPolyObj *poly;
	TVector2<double> dist;
	double distlen;

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_MovePolyTo: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	dist.X = targx - poly->StartSpot.x;
	dist.Y = targy - poly->StartSpot.y;
	distlen = dist.MakeUnit();
	while ((poly = it.NextMirror()) != NULL)
	{
		pe = NULL;

		if (poly->specialdata != NULL && !overRide)
		{ // poly is already in motion
			if ( poly->specialdata->IsKindOf(RUNTIME_CLASS(DMovePolyTo)) )
				pe = (DMovePolyTo*) poly->specialdata;
			else
				return false;
		}

		if ( pe == NULL )
		{
			pe = new DMovePolyTo(poly->tag);
			poly->specialdata = pe;
		}

		if ( pe->m_Dist > 0)
			return false;

		pe->m_LastInstigator = instigator;
		pe->m_Dist = xs_RoundToInt(distlen);
		pe->m_Speed = speed;
		pe->m_xSpeed = xs_RoundToInt(speed * dist.X);
		pe->m_ySpeed = xs_RoundToInt(speed * dist.Y);
		pe->m_xTarget = xs_RoundToInt(poly->StartSpot.x + distlen * dist.X);
		pe->m_yTarget = xs_RoundToInt(poly->StartSpot.y + distlen * dist.Y);
		if ((pe->m_Dist / pe->m_Speed) <= 2)
		{
			pe->StopInterpolation();
		}
		dist = -dist;	// reverse the direction

		// [BC] If we're the server, tell clients to create the move poly.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoMovePolyTo( pe );
	}
	return pe != NULL; // Return true if something started moving.
}

//==========================================================================
//
// T_PolyDoor
//
//==========================================================================

void DPolyDoor::Tick ()
{
	int absSpeed;
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly == NULL)
		return;

	if (m_Tics)
	{
		if (!--m_Tics)
		{
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, m_Close);
		}
		return;
	}
	switch (m_Type)
	{
	case PODOOR_SLIDE:
		if (m_Dist <= 0 || poly->MovePolyobj (m_xSpeed, m_ySpeed))
		{
			// [WS] Inform clients that the door is closing.
			if ( ( m_TotalDist == m_Dist && m_Close ) || poly->bBlocked )
			{
				// [EP] The door is not blocked anymore.
				poly->bBlocked = false;
			}

			absSpeed = abs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (poly);
				if (!m_Close)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Direction = (ANGLE_MAX>>ANGLETOFINESHIFT) - m_Direction;
					m_xSpeed = -m_xSpeed;
					m_ySpeed = -m_ySpeed;
				}
				else
				{
					Destroy ();
				}

				// [BC] If we're the server, tell clients to update the poly door.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoPolyDoor( this );
			}
		}
		else
		{
			if (poly->crush || !m_Close)
			{
				// continue moving if the poly is a crusher, or is opening
				if ( ( m_Dist > 0 ) && ( poly->bBlocked == false ) )
				{
					poly->bBlocked = true;
				}
				return;
			}
			else
			{ // open back up
				m_Dist = m_TotalDist - m_Dist;
				m_Direction = (ANGLE_MAX>>ANGLETOFINESHIFT) - m_Direction;
				m_xSpeed = -m_xSpeed;
				m_ySpeed = -m_ySpeed;
				m_Close = false;
				SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

				// [WS] Tell clients to update the speed and position.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoPolyDoor( this );
			}
		}
		break;

	case PODOOR_SWING:
		if (poly->RotatePolyobj (m_Speed))
		{
			// [WS] Inform clients that the door is closing.
			if ( ( ( m_TotalDist == m_Dist ) && m_Close ) || poly->bBlocked )
			{
				// [BB] The door is not blocked anymore.
				poly->bBlocked = false;
			}

			absSpeed = abs (m_Speed);
			if (m_Dist == -1)
			{ // perpetual polyobj
				return;
			}
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (poly);

				if (!m_Close)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Speed = -m_Speed;
				}
				else
				{
					Destroy ();
				}

				// [BC] If we're the server, tell clients to update the poly door.
				if ( NETWORK_GetState() == NETSTATE_SERVER )
					SERVERCOMMANDS_DoPolyDoor( this );
			}
		}
		else
		{
			if(poly->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				// [BB] Something just blocked the movement, inform the clients.
				if ( ( m_Dist > 0 ) && ( poly->bBlocked == false ) )
				{
					poly->bBlocked = true;
				}
				return;
			}
			else
			{ // open back up and rewait
				m_Dist = m_TotalDist - m_Dist;
				m_Speed = -m_Speed;
				m_Close = false;
				SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

				// [WS] Tell clients to update the speed and rotation.
				if ( NETWORK_GetState( ) == NETSTATE_SERVER )
					SERVERCOMMANDS_DoPolyDoor( this );
			}
		}			
		break;

	default:
		break;
	}
}

//==========================================================================
//
// EV_OpenPolyDoor
//
//==========================================================================

bool EV_OpenPolyDoor (player_t *instigator, line_t *line, int polyNum, int speed, angle_t angle,
					  int delay, int distance, podoortype_t type)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	DPolyDoor *pd;
	FPolyObj *poly;
	int swingdir = 1;	// ADD:  PODOOR_SWINGL, PODOOR_SWINGR

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_OpenPolyDoor: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != NULL)
	{
		pd = NULL;

		if (poly->specialdata != NULL)
		{ // poly is already moving
			if ( poly->specialdata->IsKindOf(RUNTIME_CLASS(DPolyDoor)) )
				pd = (DPolyDoor*) poly->specialdata;
			else
				return false;
		}

		if ( pd == NULL )
		{
			pd = new DPolyDoor(poly->tag, type);
			poly->specialdata = pd;
		}

		if ( pd->m_Dist > 0 )
			return false;

		pd->m_LastInstigator = instigator;
		if (type == PODOOR_SLIDE)
		{
			pd->m_WaitTics = delay;
			pd->m_Speed = speed;
			pd->m_Dist = pd->m_TotalDist = distance; // Distance
			pd->m_Direction = angle >> ANGLETOFINESHIFT;
			pd->m_xSpeed = FixedMul (pd->m_Speed, finecosine[pd->m_Direction]);
			pd->m_ySpeed = FixedMul (pd->m_Speed, finesine[pd->m_Direction]);
			pd->m_Close = false;
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			angle += ANGLE_180;	// reverse the angle
		}
		else if (type == PODOOR_SWING)
		{
			pd->m_WaitTics = delay;
			pd->m_Direction = swingdir; 
			pd->m_Speed = (speed*pd->m_Direction*(ANGLE_90/64))>>3;
			pd->m_Dist = pd->m_TotalDist = angle;
			pd->m_Close = false;
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			swingdir = -swingdir;	// reverse the direction
		}

		// [BC] Tell clients to create the poly door, and play a sound.
		if ( NETWORK_GetState( ) == NETSTATE_SERVER )
			SERVERCOMMANDS_DoPolyDoor( pd );

	}
	return pd != NULL;	// Return true if something started moving.
}

//==========================================================================
//
// EV_StopPoly
//
//==========================================================================

bool EV_StopPoly(player_t *instigator, int polynum)
{
	if (CLIENT_PREDICT_IsPredicting())
		return false;

	FPolyObj *poly;

	if (NULL != (poly = PO_GetPolyobj(polynum)))
	{
		if (poly->specialdata != NULL)
		{
			poly->specialdata->SetLastInstigator(instigator);
			poly->specialdata->Stop();
		}
		return true;
	}
	return false;
}

// ===== Higher Level Poly Interface code =====

//==========================================================================
//
// PO_GetPolyobj
//
//==========================================================================

FPolyObj *PO_GetPolyobj (int polyNum)
{
	int i;

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].tag == polyNum)
		{
			return &polyobjs[i];
		}
	}
	return NULL;
}


//==========================================================================
//
// [BC] GetPolyobjByIndex
//
//==========================================================================

FPolyObj *GetPolyobjByIndex( ULONG ulPolyIdx )
{
	if ( ulPolyIdx >= (ULONG)po_NumPolyobjs )
		return ( NULL );

	return ( &polyobjs[ulPolyIdx] );
}

//==========================================================================
//
// 
//
//==========================================================================

FPolyObj::FPolyObj()
{
	StartSpot.x = StartSpot.y = 0;
	angle = 0;
	tag = 0;
	memset(bbox, 0, sizeof(bbox));
	validcount = 0;
	crush = 0;
	bHurtOnTouch = false;
	seqType = 0;
	size = 0;
	subsectorlinks = NULL;
	specialdata = NULL;
	interpolation = NULL;
}

//==========================================================================
//
// GetPolyobjMirror
//
//==========================================================================

int FPolyObj::GetMirror()
{
	return MirrorNum;
}

//==========================================================================
//
// ThrustMobj
//
//==========================================================================

void FPolyObj::ThrustMobj (AActor *actor, side_t *side)
{
	int thrustAngle;
	int thrustX;
	int thrustY;
	DPolyAction *pe;

	int force;

	if (!(actor->flags&MF_SHOOTABLE) && !actor->player)
	{
		return;
	}
	vertex_t *v1 = side->V1();
	vertex_t *v2 = side->V2();
	thrustAngle = (R_PointToAngle2 (v1->x, v1->y, v2->x, v2->y) - ANGLE_90) >> ANGLETOFINESHIFT;

	pe = static_cast<DPolyAction *>(specialdata);
	if (pe)
	{
		if (pe->IsKindOf (RUNTIME_CLASS (DRotatePoly)))
		{
			force = pe->GetSpeed() >> 8;
		}
		else
		{
			force = pe->GetSpeed() >> 3;
		}
		if (force < FRACUNIT)
		{
			force = FRACUNIT;
		}
		else if (force > 4*FRACUNIT)
		{
			force = 4*FRACUNIT;
		}
	}
	else
	{
		force = FRACUNIT;
	}

	thrustX = FixedMul (force, finecosine[thrustAngle]);
	thrustY = FixedMul (force, finesine[thrustAngle]);
	actor->velx += thrustX;
	actor->vely += thrustY;
	if (crush && ( NETWORK_InClientMode() == false ))
	{
		if (bHurtOnTouch || !P_CheckMove (actor, actor->x + thrustX, actor->y + thrustY))
		{
			int newdam = P_DamageMobj (actor, NULL, NULL, crush, NAME_Crush);
			P_TraceBleed (newdam > 0 ? newdam : crush, actor);
		}
	}
	if (level.flags2 & LEVEL2_POLYGRIND) actor->Grind(false); // crush corpses that get caught in a polyobject's way
}

//==========================================================================
//
// UpdateSegBBox
//
//==========================================================================

void FPolyObj::UpdateBBox ()
{
	for(unsigned i=0;i<Linedefs.Size(); i++)
	{
		line_t *line = Linedefs[i];

		if (line->v1->x < line->v2->x)
		{
			line->bbox[BOXLEFT] = line->v1->x;
			line->bbox[BOXRIGHT] = line->v2->x;
		}
		else
		{
			line->bbox[BOXLEFT] = line->v2->x;
			line->bbox[BOXRIGHT] = line->v1->x;
		}
		if (line->v1->y < line->v2->y)
		{
			line->bbox[BOXBOTTOM] = line->v1->y;
			line->bbox[BOXTOP] = line->v2->y;
		}
		else
		{
			line->bbox[BOXBOTTOM] = line->v2->y;
			line->bbox[BOXTOP] = line->v1->y;
		}

		// Update the line's slopetype
		line->dx = line->v2->x - line->v1->x;
		line->dy = line->v2->y - line->v1->y;
		if (!line->dx)
		{
			line->slopetype = ST_VERTICAL;
		}
		else if (!line->dy)
		{
			line->slopetype = ST_HORIZONTAL;
		}
		else
		{
			line->slopetype = ((line->dy ^ line->dx) >= 0) ? ST_POSITIVE : ST_NEGATIVE;
		}
	}
	CalcCenter();
}

void FPolyObj::CalcCenter()
{
	SQWORD cx = 0, cy = 0;
	for(unsigned i=0;i<Vertices.Size(); i++)
	{
		cx += Vertices[i]->x;
		cy += Vertices[i]->y;
	}
	CenterSpot.x = (fixed_t)(cx / Vertices.Size());
	CenterSpot.y = (fixed_t)(cy / Vertices.Size());
}

//==========================================================================
//
// PO_MovePolyobj
//
//==========================================================================

bool FPolyObj::MovePolyobj (int x, int y, bool force)
{
	FBoundingBox oldbounds = Bounds;
	UnLinkPolyobj ();
	DoMovePolyobj (x, y);

	if (!force)
	{
		bool blocked = false;

		for(unsigned i=0;i < Sidedefs.Size(); i++)
		{
			if (CheckMobjBlocking(Sidedefs[i]))
			{
				blocked = true;
			}
		}
		if (blocked)
		{
			DoMovePolyobj (-x, -y);
			LinkPolyobj();
			return false;
		}
	}
	StartSpot.x += x;
	StartSpot.y += y;
	CenterSpot.x += x;
	CenterSpot.y += y;
	LinkPolyobj ();
	ClearSubsectorLinks();
	RecalcActorFloorCeil(Bounds | oldbounds);

	// [BC/BB] Polyobject has moved.
	bMoved = true;

	return true;
}

//==========================================================================
//
// DoMovePolyobj
//
//==========================================================================

void FPolyObj::DoMovePolyobj (int x, int y)
{
	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		Vertices[i]->x += x;
		Vertices[i]->y += y;
		PrevPts[i].x += x;
		PrevPts[i].y += y;
	}
	for (unsigned i = 0; i < Linedefs.Size(); i++)
	{
		Linedefs[i]->bbox[BOXTOP] += y;
		Linedefs[i]->bbox[BOXBOTTOM] += y;
		Linedefs[i]->bbox[BOXLEFT] += x;
		Linedefs[i]->bbox[BOXRIGHT] += x;
	}
}

//==========================================================================
//
// RotatePt
//
//==========================================================================

static void RotatePt (int an, fixed_t *x, fixed_t *y, fixed_t startSpotX, fixed_t startSpotY)
{
	fixed_t tr_x = *x;
	fixed_t tr_y = *y;

	*x = (DMulScale16 (tr_x, finecosine[an], -tr_y, finesine[an]) & 0xFFFFFE00) + startSpotX;
	*y = (DMulScale16 (tr_x, finesine[an], tr_y, finecosine[an]) & 0xFFFFFE00) + startSpotY;
}

//==========================================================================
//
// PO_RotatePolyobj
//
//==========================================================================

bool FPolyObj::RotatePolyobj (angle_t angle)
{
	int an;
	bool blocked;
	FBoundingBox oldbounds = Bounds;

	an = (this->angle+angle)>>ANGLETOFINESHIFT;

	UnLinkPolyobj();

	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		PrevPts[i].x = Vertices[i]->x;
		PrevPts[i].y = Vertices[i]->y;
		Vertices[i]->x = OriginalPts[i].x;
		Vertices[i]->y = OriginalPts[i].y;
		RotatePt(an, &Vertices[i]->x, &Vertices[i]->y, StartSpot.x,	StartSpot.y);
	}
	blocked = false;
	validcount++;
	UpdateBBox();

	for(unsigned i=0;i < Sidedefs.Size(); i++)
	{
		if (CheckMobjBlocking(Sidedefs[i]))
		{
			blocked = true;
		}
	}
	if (blocked)
	{
		for(unsigned i=0;i < Vertices.Size(); i++)
		{
			Vertices[i]->x = PrevPts[i].x;
			Vertices[i]->y = PrevPts[i].y;
		}
		UpdateBBox();
		LinkPolyobj();
		return false;
	}
	this->angle += angle;
	LinkPolyobj();
	ClearSubsectorLinks();
	RecalcActorFloorCeil(Bounds | oldbounds);

	// [BC] Polyobject has rotated.
	bRotated = true;

	return true;
}

//==========================================================================
//
// UnLinkPolyobj
//
//==========================================================================

void FPolyObj::UnLinkPolyobj ()
{
	polyblock_t *link;
	int i, j;
	int index;

	// remove the polyobj from each blockmap section
	for(j = bbox[BOXBOTTOM]; j <= bbox[BOXTOP]; j++)
	{
		index = j*bmapwidth;
		for(i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight)
			{
				link = PolyBlockMap[index+i];
				while(link != NULL && link->polyobj != this)
				{
					link = link->next;
				}
				if(link == NULL)
				{ // polyobj not located in the link cell
					continue;
				}
				link->polyobj = NULL;
			}
		}
	}
}

//==========================================================================
//
// CheckMobjBlocking
//
//==========================================================================

bool FPolyObj::CheckMobjBlocking (side_t *sd)
{
	static TArray<AActor *> checker;
	FBlockNode *block;
	AActor *mobj;
	int i, j, k;
	int left, right, top, bottom;
	line_t *ld;
	bool blocked;
	bool performBlockingThrust;

	ld = sd->linedef;

	top = GetSafeBlockY(ld->bbox[BOXTOP]-bmaporgy);
	bottom = GetSafeBlockY(ld->bbox[BOXBOTTOM]-bmaporgy);
	left = GetSafeBlockX(ld->bbox[BOXLEFT]-bmaporgx);
	right = GetSafeBlockX(ld->bbox[BOXRIGHT]-bmaporgx);

	blocked = false;
	checker.Clear();

	bottom = bottom < 0 ? 0 : bottom;
	bottom = bottom >= bmapheight ? bmapheight-1 : bottom;
	top = top < 0 ? 0 : top;
	top = top >= bmapheight  ? bmapheight-1 : top;
	left = left < 0 ? 0 : left;
	left = left >= bmapwidth ? bmapwidth-1 : left;
	right = right < 0 ? 0 : right;
	right = right >= bmapwidth ?  bmapwidth-1 : right;

	for (j = bottom*bmapwidth; j <= top*bmapwidth; j += bmapwidth)
	{
		for (i = left; i <= right; i++)
		{
			for (block = blocklinks[j+i]; block != NULL; block = block->NextActor)
			{
				mobj = block->Me;
				for (k = (int)checker.Size()-1; k >= 0; --k)
				{
					if (checker[k] == mobj)
					{
						break;
					}
				}
				if (k < 0)
				{
					checker.Push (mobj);
					if ((mobj->flags&MF_SOLID) && !(mobj->flags&MF_NOCLIP))
					{
						FLineOpening open;
						open.top = INT_MAX;
						open.bottom = -INT_MAX;
						// [TN] Check wether this actor gets blocked by the line.
						if (ld->backsector != NULL &&
							!(ld->flags & (ML_BLOCKING|ML_BLOCKEVERYTHING))
							&& !(ld->flags & ML_BLOCK_PLAYERS && mobj->player) 
							&& !(ld->flags & ML_BLOCKMONSTERS && mobj->flags3 & MF3_ISMONSTER)
							&& !((mobj->flags & MF_FLOAT) && (ld->flags & ML_BLOCK_FLOATERS))
							&& (!(ld->flags & ML_3DMIDTEX) ||
								(!P_LineOpening_3dMidtex(mobj, ld, open) &&
									(mobj->z + mobj->height < open.top)
								) || (open.abovemidtex && mobj->z > mobj->floorz))
							)
						{
							// [BL] We can't just continue here since we must
							// determine if the line's backsector is going to
							// be blocked.
							performBlockingThrust = false;
						}
						else
						{
							performBlockingThrust = true;
						}

						FBoundingBox box(mobj->x, mobj->y, mobj->radius);

						if (box.Right() <= ld->bbox[BOXLEFT]
							|| box.Left() >= ld->bbox[BOXRIGHT]
							|| box.Top() <= ld->bbox[BOXBOTTOM]
							|| box.Bottom() >= ld->bbox[BOXTOP])
						{
							continue;
						}
						if (box.BoxOnLineSide(ld) != -1)
						{
							continue;
						}
						// We have a two-sided linedef so we should only check one side
						// so that the thrust from both sides doesn't cancel each other out.
						// Best use the one facing the player and ignore the back side.
						if (ld->sidedef[1] != NULL)
						{
							int side = P_PointOnLineSide(mobj->x, mobj->y, ld);
							if (ld->sidedef[side] != sd)
							{
								continue;
							}
							// [BL] See if we hit below the floor/ceiling of the poly.
							else if(!performBlockingThrust && (
									mobj->z < ld->sidedef[!side]->sector->GetSecPlane(sector_t::floor).ZatPoint(mobj->x, mobj->y) ||
									mobj->z + mobj->height > ld->sidedef[!side]->sector->GetSecPlane(sector_t::ceiling).ZatPoint(mobj->x, mobj->y)
								))
							{
								performBlockingThrust = true;
							}
						}

						if(performBlockingThrust)
						{
							ThrustMobj (mobj, sd);
							blocked = true;
						}
						else
							continue;
					}
				}
			}
		}
	}
	return blocked;
}

//==========================================================================
//
// LinkPolyobj
//
//==========================================================================

void FPolyObj::LinkPolyobj ()
{
	polyblock_t **link;
	polyblock_t *tempLink;

	// calculate the polyobj bbox
	Bounds.ClearBox();
	for(unsigned i = 0; i < Sidedefs.Size(); i++)
	{
		vertex_t *vt;
		
		vt = Sidedefs[i]->linedef->v1;
		Bounds.AddToBox(vt->x, vt->y);
		vt = Sidedefs[i]->linedef->v2;
		Bounds.AddToBox(vt->x, vt->y);
	}
	bbox[BOXRIGHT] = GetSafeBlockX(Bounds.Right() - bmaporgx);
	bbox[BOXLEFT] = GetSafeBlockX(Bounds.Left() - bmaporgx);
	bbox[BOXTOP] = GetSafeBlockY(Bounds.Top() - bmaporgy);
	bbox[BOXBOTTOM] = GetSafeBlockY(Bounds.Bottom() - bmaporgy);
	// add the polyobj to each blockmap section
	for(int j = bbox[BOXBOTTOM]*bmapwidth; j <= bbox[BOXTOP]*bmapwidth;
		j += bmapwidth)
	{
		for(int i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight*bmapwidth)
			{
				link = &PolyBlockMap[j+i];
				if(!(*link))
				{ // Create a new link at the current block cell
					*link = new polyblock_t;
					(*link)->next = NULL;
					(*link)->prev = NULL;
					(*link)->polyobj = this;
					continue;
				}
				else
				{
					tempLink = *link;
					while(tempLink->next != NULL && tempLink->polyobj != NULL)
					{
						tempLink = tempLink->next;
					}
				}
				if(tempLink->polyobj == NULL)
				{
					tempLink->polyobj = this;
					continue;
				}
				else
				{
					tempLink->next = new polyblock_t;
					tempLink->next->next = NULL;
					tempLink->next->prev = tempLink;
					tempLink->next->polyobj = this;
				}
			}
			// else, don't link the polyobj, since it's off the map
		}
	}
}

//===========================================================================
//
// FPolyObj :: RecalcActorFloorCeil
//
// For each actor within the bounding box, recalculate its floorz, ceilingz,
// and related values.
//
//===========================================================================

void FPolyObj::RecalcActorFloorCeil(FBoundingBox bounds) const
{
	FBlockThingsIterator it(bounds);
	AActor *actor;

	while ((actor = it.Next()) != NULL)
	{
		P_FindFloorCeiling(actor);
	}
}

//===========================================================================
//
// PO_ClosestPoint
//
// Given a point (x,y), returns the point (ox,oy) on the polyobject's walls
// that is nearest to (x,y). Also returns the seg this point came from.
//
//===========================================================================

void FPolyObj::ClosestPoint(fixed_t fx, fixed_t fy, fixed_t &ox, fixed_t &oy, side_t **side) const
{
	unsigned int i;
	double x = fx, y = fy;
	double bestdist = HUGE_VAL;
	double bestx = 0, besty = 0;
	side_t *bestline = NULL;

	for (i = 0; i < Sidedefs.Size(); ++i)
	{
		vertex_t *v1 = Sidedefs[i]->V1();
		vertex_t *v2 = Sidedefs[i]->V2();
		double a = v2->x - v1->x;
		double b = v2->y - v1->y;
		double den = a*a + b*b;
		double ix, iy, dist;

		if (den == 0)
		{ // Line is actually a point!
			ix = v1->x;
			iy = v1->y;
		}
		else
		{
			double num = (x - v1->x) * a + (y - v1->y) * b;
			double u = num / den;
			if (u <= 0)
			{
				ix = v1->x;
				iy = v1->y;
			}
			else if (u >= 1)
			{
				ix = v2->x;
				iy = v2->y;
			}
			else
			{
				ix = v1->x + u * a;
				iy = v1->y + u * b;
			}
		}
		a = (ix - x);
		b = (iy - y);
		dist = a*a + b*b;
		if (dist < bestdist)
		{
			bestdist = dist;
			bestx = ix;
			besty = iy;
			bestline = Sidedefs[i];
		}
	}
	ox = fixed_t(bestx);
	oy = fixed_t(besty);
	if (side != NULL)
	{
		*side = bestline;
	}
}

//==========================================================================
//
// InitBlockMap
//
//==========================================================================

static void InitBlockMap (void)
{
	int i;

	PolyBlockMap = new polyblock_t *[bmapwidth*bmapheight];
	memset (PolyBlockMap, 0, bmapwidth*bmapheight*sizeof(polyblock_t *));

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		polyobjs[i].LinkPolyobj();
	}
}

//==========================================================================
//
// InitSideLists [RH]
//
// Group sides by vertex and collect side that are known to belong to a
// polyobject so that they can be initialized fast.
//==========================================================================

static void InitSideLists ()
{
	for (int i = 0; i < numsides; ++i)
	{
		if (sides[i].linedef != NULL &&
			(sides[i].linedef->special == Polyobj_StartLine ||
			 sides[i].linedef->special == Polyobj_ExplicitLine))
		{
			KnownPolySides.Push (i);
		}
	}
}

//==========================================================================
//
// KillSideLists [RH]
//
//==========================================================================

static void KillSideLists ()
{
	KnownPolySides.Clear ();
	KnownPolySides.ShrinkToFit ();
}

//==========================================================================
//
// AddPolyVert
//
// Helper function for IterFindPolySides()
//
//==========================================================================

static void AddPolyVert(TArray<DWORD> &vnum, DWORD vert)
{
	for (unsigned int i = vnum.Size() - 1; i-- != 0; )
	{
		if (vnum[i] == vert)
		{ // Already in the set. No need to add it.
			return;
		}
	}
	vnum.Push(vert);
}

//==========================================================================
//
// IterFindPolySides
//
// Beginning with the first vertex of the starting side, for each vertex
// in vnum, add all the sides that use it as a first vertex to the polyobj,
// and add all their second vertices to vnum. This continues until there
// are no new vertices in vnum.
//
//==========================================================================

static void IterFindPolySides (FPolyObj *po, side_t *side)
{
	static TArray<DWORD> vnum;
	unsigned int vnumat;

	assert(sidetemp != NULL);

	vnum.Clear();
	vnum.Push(DWORD(side->V1() - vertexes));
	vnumat = 0;

	while (vnum.Size() != vnumat)
	{
		DWORD sidenum = sidetemp[vnum[vnumat++]].b.first;
		while (sidenum != NO_SIDE)
		{
			po->Sidedefs.Push(&sides[sidenum]);
			AddPolyVert(vnum, DWORD(sides[sidenum].V2() - vertexes));
			sidenum = sidetemp[sidenum].b.next;
		}
	}
}


//==========================================================================
//
// SpawnPolyobj
//
//==========================================================================

static void SpawnPolyobj (int index, int tag, int type)
{
	unsigned int ii;
	int i;
	int j;
	FPolyObj *po = &polyobjs[index];

	for (ii = 0; ii < KnownPolySides.Size(); ++ii)
	{
		i = KnownPolySides[ii];
		if (i < 0)
		{
			continue;
		}

		side_t *sd = &sides[i];
		
		if (sd->linedef->special == Polyobj_StartLine &&
			sd->linedef->args[0] == tag)
		{
			if (po->Sidedefs.Size() > 0)
			{
				I_Error ("SpawnPolyobj: Polyobj %d already spawned.\n", tag);
			}
			sd->linedef->special = 0;
			sd->linedef->args[0] = 0;
			IterFindPolySides(&polyobjs[index], sd);
			po->MirrorNum = sd->linedef->args[1];
			po->crush = (type != PO_SPAWN_TYPE) ? 3 : 0;
			po->bHurtOnTouch = (type == PO_SPAWNHURT_TYPE);
			po->tag = tag;
			po->seqType = sd->linedef->args[2];
			if (po->seqType < 0 || po->seqType > 63)
			{
				po->seqType = 0;
			}
			// [BB] Init Zandronum stuff.
			polyobjs[index].bMoved = false;
			polyobjs[index].bRotated = false;
			polyobjs[index].bBlocked = false;
			break;
		}
	}
	if (po->Sidedefs.Size() == 0)
	{ 
		// didn't find a polyobj through PO_LINE_START
		TArray<side_t *> polySideList;
		unsigned int psIndexOld;
		for (j = 1; j < PO_MAXPOLYSEGS; j++)
		{
			psIndexOld = po->Sidedefs.Size();
			for (ii = 0; ii < KnownPolySides.Size(); ++ii)
			{
				i = KnownPolySides[ii];

				if (i >= 0 &&
					sides[i].linedef->special == Polyobj_ExplicitLine &&
					sides[i].linedef->args[0] == tag)
				{
					if (!sides[i].linedef->args[1])
					{
						I_Error ("SpawnPolyobj: Explicit line missing order number (probably %d) in poly %d.\n",
							j+1, tag);
					}
					if (sides[i].linedef->args[1] == j)
					{
						po->Sidedefs.Push (&sides[i]);
					}
				}
			}
			// Clear out any specials for these segs...we cannot clear them out
			// 	in the above loop, since we aren't guaranteed one seg per linedef.
			for (ii = 0; ii < KnownPolySides.Size(); ++ii)
			{
				i = KnownPolySides[ii];
				if (i >= 0 &&
					sides[i].linedef->special == Polyobj_ExplicitLine &&
					sides[i].linedef->args[0] == tag && sides[i].linedef->args[1] == j)
				{
					sides[i].linedef->special = 0;
					sides[i].linedef->args[0] = 0;
					KnownPolySides[ii] = -1;
				}
			}
			if (po->Sidedefs.Size() == psIndexOld)
			{ // Check if an explicit line order has been skipped.
			  // A line has been skipped if there are any more explicit
			  // lines with the current tag value. [RH] Can this actually happen?
				for (ii = 0; ii < KnownPolySides.Size(); ++ii)
				{
					i = KnownPolySides[ii];
					if (i >= 0 &&
						sides[i].linedef->special == Polyobj_ExplicitLine &&
						sides[i].linedef->args[0] == tag)
					{
						I_Error ("SpawnPolyobj: Missing explicit line %d for poly %d\n",
							j, tag);
					}
				}
			}
		}
		if (po->Sidedefs.Size() > 0)
		{
			po->crush = (type != PO_SPAWN_TYPE) ? 3 : 0;
			po->bHurtOnTouch = (type == PO_SPAWNHURT_TYPE);
			po->tag = tag;
			po->seqType = po->Sidedefs[0]->linedef->args[3];
			po->MirrorNum = po->Sidedefs[0]->linedef->args[2];

			// [BB] Init Zandronum stuff
			po->bMoved = false;
			po->bRotated = false;
			po->bBlocked = false;
		}
		else
			I_Error ("SpawnPolyobj: Poly %d does not exist\n", tag);
	}

	validcount++;	
	for(unsigned int i=0; i<po->Sidedefs.Size(); i++)
	{
		line_t *l = po->Sidedefs[i]->linedef;

		if (l->validcount != validcount)
		{
			l->validcount = validcount;
			po->Linedefs.Push(l);

			vertex_t *v = l->v1;
			int j;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

			v = l->v2;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

		}
	}
	po->Sidedefs.ShrinkToFit();
	po->Linedefs.ShrinkToFit();
	po->Vertices.ShrinkToFit();
}

//==========================================================================
//
// TranslateToStartSpot
//
//==========================================================================

static void TranslateToStartSpot (int tag, int originX, int originY)
{
	FPolyObj *po;
	int deltaX;
	int deltaY;

	po = NULL;
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].tag == tag)
		{
			po = &polyobjs[i];
			break;
		}
	}
	if (po == NULL)
	{ // didn't match the tag with a polyobj tag
		I_Error("TranslateToStartSpot: Unable to match polyobj tag: %d\n", tag);
	}
	if (po->Sidedefs.Size() == 0)
	{
		I_Error ("TranslateToStartSpot: Anchor point located without a StartSpot point: %d\n", tag);
	}
	po->OriginalPts.Resize(po->Sidedefs.Size());
	po->PrevPts.Resize(po->Sidedefs.Size());
	deltaX = originX - po->StartSpot.x;
	deltaY = originY - po->StartSpot.y;

	for (unsigned i = 0; i < po->Sidedefs.Size(); i++)
	{
		po->Sidedefs[i]->Flags |= WALLF_POLYOBJ;
	}
	for (unsigned i = 0; i < po->Linedefs.Size(); i++)
	{
		po->Linedefs[i]->bbox[BOXTOP] -= deltaY;
		po->Linedefs[i]->bbox[BOXBOTTOM] -= deltaY;
		po->Linedefs[i]->bbox[BOXLEFT] -= deltaX;
		po->Linedefs[i]->bbox[BOXRIGHT] -= deltaX;
	}
	for (unsigned i = 0; i < po->Vertices.Size(); i++)
	{
		po->Vertices[i]->x -= deltaX;
		po->Vertices[i]->y -= deltaY;
		po->OriginalPts[i].x = po->Vertices[i]->x - po->StartSpot.x;
		po->OriginalPts[i].y = po->Vertices[i]->y - po->StartSpot.y;
	}
	po->CalcCenter();
	// For compatibility purposes
	po->CenterSubsector = R_PointInSubsector(po->CenterSpot.x, po->CenterSpot.y);
}

//==========================================================================
//
// PO_Init
//
//==========================================================================

void PO_Init (void)
{
	// [RH] Hexen found the polyobject-related things by reloading the map's
	//		THINGS lump here and scanning through it. I have P_SpawnMapThing()
	//		record those things instead, so that in here we simply need to
	//		look at the polyspawns list.
	polyspawns_t *polyspawn, **prev;
	int polyIndex;

	// [RH] Make this faster
	InitSideLists ();

	polyobjs = new FPolyObj[po_NumPolyobjs];

	polyIndex = 0; // index polyobj number
	// Find the startSpot points, and spawn each polyobj
	for (polyspawn = polyspawns, prev = &polyspawns; polyspawn;)
	{
		// 9301 (3001) = no crush, 9302 (3002) = crushing, 9303 = hurting touch
		if (polyspawn->type == PO_SPAWN_TYPE ||
			polyspawn->type == PO_SPAWNCRUSH_TYPE ||
			polyspawn->type == PO_SPAWNHURT_TYPE)
		{ 
			// Polyobj StartSpot Pt.
			polyobjs[polyIndex].StartSpot.x = polyspawn->x;
			polyobjs[polyIndex].StartSpot.y = polyspawn->y;
			// [BB] Save the original start spot, so that we can reset to it in GAME_ResetMap.
			polyobjs[polyIndex].SavedStartSpot[0] = polyobjs[polyIndex].StartSpot.x;
			polyobjs[polyIndex].SavedStartSpot[1] = polyobjs[polyIndex].StartSpot.y;
			SpawnPolyobj(polyIndex, polyspawn->angle, polyspawn->type);
			polyIndex++;
			*prev = polyspawn->next;
			delete polyspawn;
			polyspawn = *prev;
		} 
		else 
		{
			prev = &polyspawn->next;
			polyspawn = polyspawn->next;
		}
	}
	for (polyspawn = polyspawns; polyspawn;)
	{
		polyspawns_t *next = polyspawn->next;
		if (polyspawn->type == PO_ANCHOR_TYPE)
		{ 
			// Polyobj Anchor Pt.
			TranslateToStartSpot (polyspawn->angle, polyspawn->x, polyspawn->y);
		}
		delete polyspawn;
		polyspawn = next;
	}
	polyspawns = NULL;

	// check for a startspot without an anchor point
	for (polyIndex = 0; polyIndex < po_NumPolyobjs; polyIndex++)
	{
		if (polyobjs[polyIndex].OriginalPts.Size() == 0)
		{
			I_Error ("PO_Init: StartSpot located without an Anchor point: %d\n",
				polyobjs[polyIndex].tag);
		}
	}
	InitBlockMap();

	// [RH] Don't need the seg lists anymore
	KillSideLists ();

	for(int i=0;i<numnodes;i++)
	{
		node_t *no = &nodes[i];
		double fdx = (double)no->dx;
		double fdy = (double)no->dy;
		no->len = (float)sqrt(fdx * fdx + fdy * fdy);
	}

	// mark all subsectors which have a seg belonging to a polyobj
	// These ones should not be rendered on the textured automap.
	for (int i = 0; i < numsubsectors; i++)
	{
		subsector_t *ss = &subsectors[i];
		for(DWORD j=0;j<ss->numlines; j++)
		{
			if (ss->firstline[j].sidedef != NULL &&
				ss->firstline[j].sidedef->Flags & WALLF_POLYOBJ)
			{
				ss->flags |= SSECF_POLYORG;
				break;
			}
		}
	}

}

//==========================================================================
//
// PO_Busy
//
//==========================================================================

bool PO_Busy (int polyobj)
{
	FPolyObj *poly;

	poly = PO_GetPolyobj (polyobj);
	return (poly != NULL && poly->specialdata != NULL);
}



//==========================================================================
//
//
//
//==========================================================================

void FPolyObj::ClearSubsectorLinks()
{
	while (subsectorlinks != NULL)
	{
		assert(subsectorlinks->state == 1337);

		FPolyNode *next = subsectorlinks->snext;

		if (subsectorlinks->pnext != NULL)
		{
			assert(subsectorlinks->pnext->state == 1337);
			subsectorlinks->pnext->pprev = subsectorlinks->pprev;
		}

		if (subsectorlinks->pprev != NULL)
		{
			assert(subsectorlinks->pprev->state == 1337);
			subsectorlinks->pprev->pnext = subsectorlinks->pnext;
		}
		else
		{
			subsectorlinks->subsector->polys = subsectorlinks->pnext;
		}

		if (subsectorlinks->subsector->BSP != NULL)
		{
			subsectorlinks->subsector->BSP->bDirty = true;
		}

		subsectorlinks->state = -1;
		delete subsectorlinks;
		subsectorlinks = next;
	}
	subsectorlinks = NULL;
}

void FPolyObj::ClearAllSubsectorLinks()
{
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		polyobjs[i].ClearSubsectorLinks();
	}
	ReleaseAllPolyNodes();
}

//==========================================================================
//
// GetIntersection
//
// adapted from P_InterceptVector
//
//==========================================================================

static bool GetIntersection(FPolySeg *seg, node_t *bsp, FPolyVertex *v)
{
	double frac;
	double num;
	double den;

	double v2x = (double)seg->v1.x;
	double v2y = (double)seg->v1.y;
	double v2dx = (double)(seg->v2.x - seg->v1.x);
	double v2dy = (double)(seg->v2.y - seg->v1.y);
	double v1x = (double)bsp->x;
	double v1y = (double)bsp->y;
	double v1dx = (double)bsp->dx;
	double v1dy = (double)bsp->dy;
		
	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return false;		// parallel
	
	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	frac = num / den;

	if (frac < 0. || frac > 1.) return false;

	v->x = xs_RoundToInt(v2x + frac * v2dx);
	v->y = xs_RoundToInt(v2y + frac * v2dy);
	return true;
}

//==========================================================================
//
// PartitionDistance
//
// Determine the distance of a vertex to a node's partition line.
//
//==========================================================================

static double PartitionDistance(FPolyVertex *vt, node_t *node)
{	
	return fabs(double(-node->dy) * (vt->x - node->x) + double(node->dx) * (vt->y - node->y)) / node->len;
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(fixed_t child[4], fixed_t parent[4])
{
	if (child[BOXTOP] > parent[BOXTOP])
	{
		parent[BOXTOP] = child[BOXTOP];
	}
	if (child[BOXBOTTOM] < parent[BOXBOTTOM])
	{
		parent[BOXBOTTOM] = child[BOXBOTTOM];
	}
	if (child[BOXLEFT] < parent[BOXLEFT])
	{
		parent[BOXLEFT] = child[BOXLEFT];
	}
	if (child[BOXRIGHT] > parent[BOXRIGHT])
	{
		parent[BOXRIGHT] = child[BOXRIGHT];
	}
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(FPolyVertex *v, fixed_t bbox[4])
{
	if (v->x < bbox[BOXLEFT])
	{
		bbox[BOXLEFT] = v->x;
	}
	if (v->x > bbox[BOXRIGHT])
	{
		bbox[BOXRIGHT] = v->x;
	}
	if (v->y < bbox[BOXBOTTOM])
	{
		bbox[BOXBOTTOM] = v->y;
	}
	if (v->y > bbox[BOXTOP])
	{
		bbox[BOXTOP] = v->y;
	}
}

//==========================================================================
//
// SplitPoly
//
//==========================================================================

static void SplitPoly(FPolyNode *pnode, void *node, fixed_t bbox[4])
{
	static TArray<FPolySeg> lists[2];
	static const double POLY_EPSILON = 0.3125;

	if (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		int centerside = R_PointOnSide(pnode->poly->CenterSpot.x, pnode->poly->CenterSpot.y, bsp);

		lists[0].Clear();
		lists[1].Clear();
		for(unsigned i=0;i<pnode->segs.Size(); i++)
		{
			FPolySeg *seg = &pnode->segs[i];

			// Parts of the following code were taken from Eternity and are
			// being used with permission.

			// get distance of vertices from partition line
			// If the distance is too small, we may decide to
			// change our idea of sidedness.
			double dist_v1 = PartitionDistance(&seg->v1, bsp);
			double dist_v2 = PartitionDistance(&seg->v2, bsp);

			// If the distances are less than epsilon, consider the points as being
			// on the same side as the polyobj origin. Why? People like to build
			// polyobject doors flush with their door tracks. This breaks using the
			// usual assumptions.
			

			// Addition to Eternity code: We must also check any seg with only one
			// vertex inside the epsilon threshold. If not, these lines will get split but
			// adjoining ones with both vertices inside the threshold won't thus messing up
			// the order in which they get drawn.

			if(dist_v1 <= POLY_EPSILON)
			{
				if (dist_v2 <= POLY_EPSILON)
				{
					lists[centerside].Push(*seg);
				}
				else
				{
					int side = R_PointOnSide(seg->v2.x, seg->v2.y, bsp);
					lists[side].Push(*seg);
				}
			}
			else if (dist_v2 <= POLY_EPSILON)
			{
				int side = R_PointOnSide(seg->v1.x, seg->v1.y, bsp);
				lists[side].Push(*seg);
			}
			else 
			{
				int side1 = R_PointOnSide(seg->v1.x, seg->v1.y, bsp);
				int side2 = R_PointOnSide(seg->v2.x, seg->v2.y, bsp);

				if(side1 != side2)
				{
					// if the partition line crosses this seg, we must split it.

					FPolyVertex vert;

					if (GetIntersection(seg, bsp, &vert))
					{
						lists[0].Push(*seg);
						lists[1].Push(*seg);
						lists[side1].Last().v2 = vert;
						lists[side2].Last().v1 = vert;
					}
					else
					{
						// should never happen
						lists[side1].Push(*seg);
					}
				}
				else 
				{
					// both points on the same side.
					lists[side1].Push(*seg);
				}
			}
		}
		if (lists[1].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);
			AddToBBox(bsp->bbox[0], bbox);
		}
		else if (lists[0].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[1], bsp->bbox[1]);
			AddToBBox(bsp->bbox[1], bbox);
		}
		else
		{
			// create the new node 
			FPolyNode *newnode = NewPolyNode();
			newnode->poly = pnode->poly;
			newnode->segs = lists[1];

			// set segs for original node
			pnode->segs = lists[0];
		
			// recurse back side
			SplitPoly(newnode, bsp->children[1], bsp->bbox[1]);
			
			// recurse front side
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);

			AddToBBox(bsp->bbox[0], bbox);
			AddToBBox(bsp->bbox[1], bbox);
		}
	}
	else
	{
		// we reached a subsector so we can link the node with this subsector
		subsector_t *sub = (subsector_t *)((BYTE *)node - 1);

		// Link node to subsector
		pnode->pnext = sub->polys;
		if (pnode->pnext != NULL) 
		{
			assert(pnode->pnext->state == 1337);
			pnode->pnext->pprev = pnode;
		}
		pnode->pprev = NULL;
		sub->polys = pnode;

		// link node to polyobject
		pnode->snext = pnode->poly->subsectorlinks;
		pnode->poly->subsectorlinks = pnode;
		pnode->subsector = sub;

		// calculate bounding box for this polynode
		assert(pnode->segs.Size() != 0);
		fixed_t subbbox[4] = { FIXED_MIN, FIXED_MAX, FIXED_MAX, FIXED_MIN };

		for (unsigned i = 0; i < pnode->segs.Size(); ++i)
		{
			AddToBBox(&pnode->segs[i].v1, subbbox);
			AddToBBox(&pnode->segs[i].v2, subbbox);
		}
		// Potentially expand the parent node's bounding box to contain these bits of polyobject.
		AddToBBox(subbbox, bbox);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FPolyObj::CreateSubsectorLinks()
{
	FPolyNode *node = NewPolyNode();
	// Even though we don't care about it, we need to initialize this
	// bounding box to something so that Valgrind won't complain about it
	// when SplitPoly modifies it.
	fixed_t dummybbox[4] = { 0 };

	node->poly = this;
	node->segs.Resize(Sidedefs.Size());

	for(unsigned i=0; i<Sidedefs.Size(); i++)
	{
		FPolySeg *seg = &node->segs[i];
		side_t *side = Sidedefs[i];

		seg->v1 = side->V1();
		seg->v2 = side->V2();
		seg->wall = side;
	}
	if (!(i_compatflags & COMPATF_POLYOBJ))
	{
		SplitPoly(node, nodes + numnodes - 1, dummybbox);
	}
	else
	{
		subsector_t *sub = CenterSubsector;

		// Link node to subsector
		node->pnext = sub->polys;
		if (node->pnext != NULL) 
		{
			assert(node->pnext->state == 1337);
			node->pnext->pprev = node;
		}
		node->pprev = NULL;
		sub->polys = node;

		// link node to polyobject
		node->snext = node->poly->subsectorlinks;
		node->poly->subsectorlinks = node;
		node->subsector = sub;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void PO_LinkToSubsectors()
{
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].subsectorlinks == NULL)
		{
			polyobjs[i].CreateSubsectorLinks();
		}
	}
}

//==========================================================================
//
// NewPolyNode
//
//==========================================================================

static FPolyNode *NewPolyNode()
{
	FPolyNode *node;

	if (FreePolyNodes != NULL)
	{
		node = FreePolyNodes;
		FreePolyNodes = node->pnext;
	}
	else
	{
		node = new FPolyNode;
	}
	node->state = 1337;
	node->poly = NULL;
	node->pnext = NULL;
	node->pprev = NULL;
	node->subsector = NULL;
	node->snext = NULL;
	return node;
}

//==========================================================================
//
// FreePolyNode
//
//==========================================================================

void FreePolyNode(FPolyNode *node)
{
	node->segs.Clear();
	node->pnext = FreePolyNodes;
	FreePolyNodes = node;
}

//==========================================================================
//
// ReleaseAllPolyNodes
//
//==========================================================================

void ReleaseAllPolyNodes()
{
	FPolyNode *node, *next;

	for (node = FreePolyNodes; node != NULL; node = next)
	{
		next = node->pnext;
		delete node;
	}
}

//==========================================================================
//
// FPolyMirrorIterator Constructor
//
// This class is used to avoid infinitely looping on cyclical chains of
// mirrored polyobjects.
//
//==========================================================================

FPolyMirrorIterator::FPolyMirrorIterator(FPolyObj *poly)
{
	CurPoly = poly;
	if (poly != NULL)
	{
		UsedPolys[0] = poly->tag;
		NumUsedPolys = 1;
	}
	else
	{
		NumUsedPolys = 0;
	}
}

//==========================================================================
//
// FPolyMirrorIterator :: NextMirror
//
// Returns the polyobject that mirrors the current one, or NULL if there
// is no mirroring polyobject, or there is a mirroring polyobject but it was
// already returned.
//
//==========================================================================

FPolyObj *FPolyMirrorIterator::NextMirror()
{
	FPolyObj *poly = CurPoly, *nextpoly;

	if (poly == NULL)
	{
		return NULL;
	}

	// Do the work to decide which polyobject to return the next time this
	// function is called.
	int mirror = poly->GetMirror(), i;
	nextpoly = NULL;

	// Is there a mirror and we have room to remember it?
	if (mirror != 0 && NumUsedPolys != countof(UsedPolys))
	{
		// Has this polyobject been returned already?
		for (i = 0; i < NumUsedPolys; ++i)
		{
			if (UsedPolys[i] == mirror)
			{
				break;	// Yes, it has been returned.
			}
		}
		if (i == NumUsedPolys)
		{ // No, it has not been returned.
			UsedPolys[NumUsedPolys++] = mirror;
			nextpoly = PO_GetPolyobj(mirror);
			if (nextpoly == NULL)
			{
				Printf("Invalid mirror polyobj num %d for polyobj num %d\n", mirror, UsedPolys[i - 1]);
			}
		}
	}
	CurPoly = nextpoly;
	return poly;
}
