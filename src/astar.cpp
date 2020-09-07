//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004-2005 Brad Carney
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
// Date created:  6/7/04
//
//
// Filename: astar.cpp
//
// Description: 
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "astar.h"
#include "doomstat.h"
#include "gi.h"
#include "m_random.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "p_trace.h"
#include "i_system.h"
#include "stats.h"
#include "botpath.h"
#include "doomerrors.h"

//*****************************************************************************
//	VARIABLES

static	LONG			g_lNumHorizontalNodes;
static	LONG			g_lNumVerticalNodes;
static	LONG			g_lMapXMin;
static	LONG			g_lMapYMin;
static	LONG			g_lMapXMax;
static	LONG			g_lMapYMax;
static	LONG			g_lNodeListSize;
static	LONG			g_lNumSearchedNodes;
static	cycle_t			g_PathingCycles;
static	ASTARNODE_t		*g_aMasterNodeList = NULL;
static	ULONG			g_ulPriorityQueuePosition[MAX_PATHS];
static	ASTARNODE_t		**g_apOpenListPriorityQueue[MAX_PATHS];
static	ASTARPATH_t		g_aPaths[MAX_PATHS];
static	LONG			g_lCurrentPathIdx;
static	FRandom			g_RandomRoamSeed( "RoamSeed" );
static	bool			g_bIsInitialized;

//*****************************************************************************
//	PROTOTYPES

static	bool			astar_PathNextNode( ASTARPATH_t *pPath );
static	ASTARNODE_t		*astar_GetNodeFromPoint( POS_t Point );
static	LONG			astar_GetCostToGoalEstimate( ASTARPATH_t *pPath, ASTARNODE_t *pNode );
static	void			astar_PushNodeToStack( ASTARNODE_t *pNode, ASTARPATH_t *pPath );
static	bool			astar_PullNodeFromOpenList( ASTARPATH_t *pPath );
static	void			astar_ProcessNextPathNode( ASTARPATH_t *pPath, ASTARNODE_t *pNode, LONG lAddedCost, LONG lDirection );
static	ASTARNODE_t		*astar_GetNode( LONG lXNodeIdx, LONG lYNodeIdx );
static	void			astar_InsertToPriorityQueue( ASTARNODE_t *pNode );
static	ASTARNODE_t		*astar_PopFromPriorityQueue( void );
static	void			astar_FixUpPriorityQueue( ULONG ulPosition );
static	void			astar_FixDownPriorityQueue( ULONG ulStartPosition, ULONG ulEndPosition );
static	bool			astar_IsPriorityQueueEmpty( void );
static	void			astar_Exchange( ASTARNODE_t **pNode1, ASTARNODE_t **pNode2 );
static	ULONG			astar_GetTotalCost( ASTARNODE_t *pNode );

//*****************************************************************************
//	FUNCTIONS

void ASTAR_Construct( void )
{
	ULONG	ulIdx;

	for ( ulIdx = 0; ulIdx < MAX_PATHS; ulIdx++ )
		g_apOpenListPriorityQueue[ulIdx] = NULL;

	g_bIsInitialized = false;
}

//*****************************************************************************
//
void ASTAR_BuildNodes( void )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	g_lMapXMax = INT_MIN;
	g_lMapYMax = INT_MIN;

	for ( ulIdx = 0; ulIdx < (ULONG)numvertexes; ulIdx++ )
	{
		if ( vertexes[ulIdx].x > g_lMapXMax )
			g_lMapXMax = vertexes[ulIdx].x;
    
		if ( vertexes[ulIdx].y > g_lMapYMax )
			g_lMapYMax = vertexes[ulIdx].y;
	}

	g_lMapXMin = bmaporgx;
	g_lMapYMin = bmaporgy;

//	g_lNumHorizontalNodes = ((( g_lMapXMax - g_lMapXMin ) / FRACUNIT ) / ASTAR_NODE_SIZE );
//	g_lNumVerticalNodes = ((( g_lMapYMax - g_lMapYMin ) / FRACUNIT ) / ASTAR_NODE_SIZE );
	g_lNumHorizontalNodes = ( g_lMapXMax >> ASTAR_NODE_SHIFT ) - ( g_lMapXMin >> ASTAR_NODE_SHIFT ) + 1;
	g_lNumVerticalNodes = ( g_lMapYMax >> ASTAR_NODE_SHIFT ) - ( g_lMapYMin >> ASTAR_NODE_SHIFT ) + 1;

	g_lNodeListSize = ( g_lNumHorizontalNodes * g_lNumVerticalNodes );

	// Allocate a bunch of nodes for the master node list. The size of the master node list
	// is the maximum number of nodes per search: length * width.
	// [BB] This is certainly more memory than we want to assign for the bots...
	if ( sizeof ( ASTARNODE_t ) * g_lNodeListSize > INT_MAX )
	{
		Printf ( "Unable to allocate bot nodes. Disabling bots on this map.\n");
		g_bIsInitialized = true;
		return;
	}
	g_aMasterNodeList = new ASTARNODE_t[g_lNodeListSize];

	for ( ulIdx = 0; ulIdx < (ULONG)g_lNumHorizontalNodes; ulIdx++ )
	{
		for ( ulIdx2 = 0; ulIdx2 < (ULONG)g_lNumVerticalNodes; ulIdx2++ )
		{
			g_aMasterNodeList[( ulIdx * g_lNumVerticalNodes ) + ulIdx2].lXNodeIdx = ulIdx;
			g_aMasterNodeList[( ulIdx * g_lNumVerticalNodes ) + ulIdx2].lYNodeIdx = ulIdx2;
			g_aMasterNodeList[( ulIdx * g_lNumVerticalNodes ) + ulIdx2].Position = ASTAR_GetPositionFromIndex( ulIdx, ulIdx2 );
		}
	}

	for ( ulIdx = 0; ulIdx < (ULONG)g_lNodeListSize; ulIdx++ )
	{
		for ( ulIdx2 = 0; ulIdx2 < MAX_PATHS; ulIdx2++ )
		{
			g_aMasterNodeList[ulIdx].bOnClosed[ulIdx2] = false;
			g_aMasterNodeList[ulIdx].bOnOpen[ulIdx2] = false;
			g_aMasterNodeList[ulIdx].lCostFromStart[ulIdx2] = 0;
			g_aMasterNodeList[ulIdx].lTotalCost[ulIdx2] = 0;
			g_aMasterNodeList[ulIdx].pParent[ulIdx2] = NULL;
		}
	}

	for ( ulIdx = 0; ulIdx < MAX_PATHS; ulIdx++ )
	{
		g_aPaths[ulIdx].bInGoalNode = false;
		g_aPaths[ulIdx].lStackPos = 0;
		g_aPaths[ulIdx].pActor = NULL;
		g_aPaths[ulIdx].paVisualizations = (AActor **)M_Malloc( sizeof(AActor *) * g_lNodeListSize );
		for ( ulIdx2 = 0; ulIdx2 < (ULONG)g_lNodeListSize; ulIdx2++ )
			g_aPaths[ulIdx].paVisualizations[ulIdx2] = NULL;
		g_aPaths[ulIdx].pCurrentNode = NULL;
		g_aPaths[ulIdx].pStartNode = NULL;
		g_aPaths[ulIdx].pGoalNode = NULL;
		for ( ulIdx2 = 0; ulIdx2 < MAX_NODES_IN_PATH; ulIdx2++ )
			g_aPaths[ulIdx].pNodeStack[ulIdx2] = NULL;
		g_aPaths[ulIdx].ulFlags = 0;
		g_aPaths[ulIdx].ulNextStep = 0;
		g_aPaths[ulIdx].ulNumSearchedNodes = 0;

		g_ulPriorityQueuePosition[ulIdx] = 0;
		g_apOpenListPriorityQueue[ulIdx] = (ASTARNODE_t **)M_Malloc( sizeof(ASTARNODE_t *) * ( g_lNodeListSize + 1 ));
		for ( ulIdx2 = 0; ulIdx2 < (ULONG)g_lNodeListSize; ulIdx2++ )
			g_apOpenListPriorityQueue[ulIdx][ulIdx2] = NULL;
	}

	g_lNumSearchedNodes = 0;

	g_lCurrentPathIdx = -1;

	g_bIsInitialized = true;
}

//*****************************************************************************
//
void ASTAR_ClearNodes( void )
{
	ULONG	ulIdx;

	delete[] g_aMasterNodeList;
	g_aMasterNodeList = NULL;

	for ( ulIdx = 0; ulIdx < MAX_PATHS; ulIdx++ )
	{
		M_Free( g_apOpenListPriorityQueue[ulIdx] );
		g_apOpenListPriorityQueue[ulIdx] = NULL;

		M_Free( g_aPaths[ulIdx].paVisualizations );
		g_aPaths[ulIdx].paVisualizations = NULL;
	}

	g_lNodeListSize = 0;
	g_bIsInitialized = false;
}

//*****************************************************************************
//
bool ASTAR_IsInitialized( void )
{
	return ( g_bIsInitialized );
}

//*****************************************************************************
//
ASTARRETURNSTRUCT_t ASTAR_Path( ULONG ulPathIdx, POS_t GoalPoint, float fMaxSearchNodes, LONG lGiveUpLimit )
{
//	ASTARNODE_t				*pStartNode;
	ASTARRETURNSTRUCT_t		ReturnVal;
	POS_t					StartPoint;
	ASTARPATH_t				*pPath;

	g_lCurrentPathIdx = ulPathIdx;

	pPath = &g_aPaths[ulPathIdx];
	pPath->pActor = players[ulPathIdx % MAXPLAYERS].mo;

	StartPoint.x = pPath->pActor->x;
	StartPoint.y = pPath->pActor->y;

	pPath->pGoalNode = astar_GetNodeFromPoint( GoalPoint );
	if ( pPath->pGoalNode == NULL )
	{
		Printf( "WARNING! Cannot path to location: (%d, %d)\n", GoalPoint.x / FRACUNIT, GoalPoint.y / FRACUNIT );
		ReturnVal.bIsGoal = false;
		ReturnVal.pNode = NULL;
		ReturnVal.ulFlags = 0;
		ReturnVal.lTotalCost = 0;

		return ( ReturnVal );
	}

	pPath->pStartNode = astar_GetNodeFromPoint( StartPoint );
	if ( pPath->pStartNode == NULL )
	{
		// [BB] The bot spawned outside the map. This really should NOT happen.
		// But if it does, we have to do something about it, otherwise ST will
		// crash. So we kill the bot and hope he respawns inside the level.
		Printf( "WARNING! Cannot find start point: (%d, %d)\n", StartPoint.x / FRACUNIT, StartPoint.y / FRACUNIT );
		ReturnVal.bIsGoal = false;
		ReturnVal.pNode = NULL;
		ReturnVal.ulFlags = 0;
		ReturnVal.lTotalCost = 0;

		pPath->pActor->Die(NULL, NULL);
		return ( ReturnVal );
	}

//	ASTAR_ClearPath( pPath );

	// Has the path has already been built? If so, simply return the next node on the path.
	if ( pPath->ulFlags & PF_COMPLETE )
	{
//		POS_t			CurPos;
		POS_t			DestPos;
//		sector_t		*pSector;
//		fixed_t			vx;
//		fixed_t			vy;
//		fixed_t			vz;
//		fixed_t			Angle;
//		fixed_t			Pitch;
//		FTraceResults	TraceResults;
		ULONG			ulResults;

//		CurPos.x = pPath->pActor->x;
//		CurPos.y = pPath->pActor->y;
//		CurPos.z = pPath->pActor->z;

		if ( pPath->lStackPos <= 0 )
			I_Error( "ASTAR_Path: Bot pathing stack position went below 0!" );

		DestPos = pPath->pNodeStack[pPath->lStackPos - 1]->Position;
//		pSector = pPath->pActor->Sector;

//		Angle = R_PointToAngle2( CurPos.x, CurPos.y, DestPos.x, DestPos.y ) >> ANGLETOFINESHIFT;
//		Pitch = 0;

//		vx = FixedMul( finecosine[Pitch], finecosine[Angle] );
//		vy = FixedMul( finecosine[Pitch], finesine[Angle] );
//		vz = finesine[Pitch];

		// If we for some reason cannot reach the next node in our goal, we need to repath.
/*
		if ( Trace( CurPos.x,
			CurPos.y,
			pSector->floorplane.ZatPoint( CurPos.x, CurPos.y ) + gameinfo.StepHeight,
			pSector,
			vx,
			vy,
			vz,
			ASTAR_NODE_SIZE * FRACUNIT,
			MF_SOLID|MF_SHOOTABLE,
			0,
			pPath->pActor,
			TraceResults ))
*/
		ulResults = BOTPATH_TryWalk( pPath->pActor, pPath->pActor->x, pPath->pActor->y, pPath->pActor->z, DestPos.x, DestPos.y );
		if ( ulResults & BOTPATH_OBSTRUCTED )
		{
			// If this is a roaming path, just pick another roam location. Otherwise, try to
			// salvage it.
			if ( pPath->pActor->player->pSkullBot->m_ulPathType == BOTPATHTYPE_ROAM )
			{
				pPath->pActor->player->pSkullBot->m_ulPathType = BOTPATHTYPE_NONE;

				ReturnVal.bIsGoal = false;
				ReturnVal.lTotalCost = 0;
				ReturnVal.pNode = NULL;
				ReturnVal.ulFlags = PF_COMPLETE;

				return ( ReturnVal );
			}

			ASTAR_ClearPath( g_lCurrentPathIdx );

			// Retain a few things.
			pPath->pActor = players[ulPathIdx % MAXPLAYERS].mo;
			pPath->pGoalNode = astar_GetNodeFromPoint( GoalPoint );
			pPath->pStartNode = astar_GetNodeFromPoint( StartPoint );
		}
		else
		{
			// If we've reached the node we've been heading to, it's time to pop a new node
			// off the stack.
			if ( pPath->pStartNode == pPath->pNodeStack[pPath->lStackPos - 1] )
			{
				// If there is no new node to pop, this must be the goal node.
				if ( pPath->lStackPos == 1 )
				{
					ReturnVal.pNode = pPath->pNodeStack[pPath->lStackPos - 1];
					ReturnVal.bIsGoal = true;
				}
				else
				{
					pPath->lStackPos--;
					ReturnVal.pNode = pPath->pNodeStack[pPath->lStackPos - 1];
					ReturnVal.bIsGoal = false;
				}
			}
			else
			{
				ReturnVal.pNode = pPath->pNodeStack[pPath->lStackPos - 1];
				ReturnVal.bIsGoal = false;
			}

			ReturnVal.ulFlags = pPath->ulFlags;
			ReturnVal.lTotalCost = pPath->pGoalNode->lTotalCost[g_lCurrentPathIdx];

//			unclock( g_PathingCycles );
			return ( ReturnVal );
		}
	}

	g_PathingCycles.Reset();

	g_PathingCycles.Clock();

	// Begin the pathing process.
	g_lNumSearchedNodes = 0;

	// If the path has not been initialized, we need to set some things up.
	if (( pPath->ulFlags & PF_INITIALIZED ) == false )
	{
		// First, check if the object is too high off the ground. If it is, we can't get to it.
		if ( pPath->pActor->player->pSkullBot->m_ulPathType == BOTPATHTYPE_ITEM )
		{
			subsector_t	*pSubSector;

			pSubSector = R_PointInSubsector( GoalPoint.x, GoalPoint.y );
			if (( GoalPoint.z - pSubSector->sector->floorplane.ZatPoint( GoalPoint.x, GoalPoint.y )) > (( 36 * FRACUNIT ) + pPath->pActor->height ))
			{
				ReturnVal.bIsGoal = false;
				ReturnVal.lTotalCost = 0;
				ReturnVal.pNode = NULL;
				ReturnVal.ulFlags = PF_COMPLETE;

				return ( ReturnVal );
			}
		}

		// Estimate the total cost to the goal from this node.
		pPath->pStartNode->lCostFromStart[g_lCurrentPathIdx] = 0;
		pPath->pStartNode->lTotalCost[g_lCurrentPathIdx] = pPath->pStartNode->lCostFromStart[g_lCurrentPathIdx] + astar_GetCostToGoalEstimate( pPath, pPath->pStartNode );

		// The start node does not have a parent.
		pPath->pStartNode->pParent[g_lCurrentPathIdx] = NULL;

		// Put this node on the open list.
		pPath->pStartNode->bOnOpen[g_lCurrentPathIdx] = true;
		astar_InsertToPriorityQueue( pPath->pStartNode );

		pPath->pStartNode->bOnClosed[g_lCurrentPathIdx] = false;
		pPath->pStartNode->lDirection[g_lCurrentPathIdx] = 0;
//		pPath->pStartNode->ulFlags[g_lCurrentPathIdx] = 0;

		// The first thing to do in our pathing algorithm is pull a node from the open list.
		pPath->ulNextStep = ASTAR_NS_PULLFROMOPENLIST;

		// All done!
		pPath->ulFlags |= PF_INITIALIZED;

		// The VERY first thing we can do is test to see if there's a straight path betwen the
		// bot and his goal.
		if (( BOTPATH_TryWalk( pPath->pActor, pPath->pActor->x, pPath->pActor->y, pPath->pActor->z, GoalPoint.x, GoalPoint.y ) & (BOTPATH_OBSTRUCTED|BOTPATH_DAMAGINGSECTOR)) == false )
		{
			pPath->ulFlags |= PF_COMPLETE|PF_SUCCESS;
			astar_PushNodeToStack( pPath->pGoalNode, pPath );

			ReturnVal.bIsGoal = true;
			ReturnVal.lTotalCost = P_AproxDistance( pPath->pActor->x - GoalPoint.x, pPath->pActor->y - GoalPoint.y );
			ReturnVal.pNode = pPath->pGoalNode;
			ReturnVal.ulFlags = pPath->ulFlags;

			g_PathingCycles.Unclock();
			return ( ReturnVal );
		}
	}

	if (( fMaxSearchNodes > 0 ) && ( fMaxSearchNodes < 1 ))
	{
		if (( gametic % (LONG)( 1.0f / fMaxSearchNodes )) == 0 )
			astar_PathNextNode( pPath );
	}
	else
	{
		while ( astar_PathNextNode( pPath ) == false )
		{
			if (( fMaxSearchNodes > 0 ) && ( g_lNumSearchedNodes >= fMaxSearchNodes ))
				break;

			if (( lGiveUpLimit > 0 ) && ( pPath->ulNumSearchedNodes >= (ULONG)lGiveUpLimit ))
			{
				// We've exceeded the give up limit. So, label the path as complete.
				pPath->ulFlags |= PF_COMPLETE;
				break;
			}
		}
	}

	ReturnVal.ulFlags = pPath->ulFlags;
	if ( pPath->ulFlags & PF_COMPLETE )
	{
		 if ( pPath->ulFlags & PF_SUCCESS )
		 {
			// We have not yet completed a path to the goal.
			ReturnVal.pNode = pPath->pNodeStack[pPath->lStackPos - 1];
			ReturnVal.bIsGoal = false;
			ReturnVal.lTotalCost = pPath->pGoalNode->lTotalCost[g_lCurrentPathIdx];
		 }
		 // Were not able to find a path.
		 else
		 {
			ReturnVal.pNode = NULL;
			ReturnVal.bIsGoal = false;
			ReturnVal.lTotalCost = 0;
		 }
	}
	else
	{
		// We have not yet completed a path to the goal.
		ReturnVal.pNode = NULL;
		ReturnVal.bIsGoal = false;
		ReturnVal.lTotalCost = 0;
	}

	g_PathingCycles.Unclock();
	return ( ReturnVal );
}

//*****************************************************************************
//
POS_t ASTAR_GetPosition( ASTARNODE_t *pNode )
{
	POS_t	Position;

//	pNode->lXNodeIdx << ASTAR_NODE_SHIFT + (( g_lMapXMin >> ASTAR_NODE_SHIFT ) << ASTAR_NODE_SHIFT )
//	Position.x = ( pNode->lXNodeIdx << ASTAR_NODE_SHIFT ) + (((( g_lMapXMin / FRACUNIT ) / 64 ) * 64 ) * FRACUNIT ) + ( 32 << FRACBITS );
//	Position.y = ( pNode->lYNodeIdx << ASTAR_NODE_SHIFT ) + (((( g_lMapYMin / FRACUNIT ) / 64 ) * 64 ) * FRACUNIT ) + ( 32 << FRACBITS );
	Position.x = ( pNode->lXNodeIdx << ASTAR_NODE_SHIFT ) + (( g_lMapXMin >> ASTAR_NODE_SHIFT ) << ASTAR_NODE_SHIFT ) + ( 32 << FRACBITS );
	Position.y = ( pNode->lYNodeIdx << ASTAR_NODE_SHIFT ) + (( g_lMapYMin >> ASTAR_NODE_SHIFT ) << ASTAR_NODE_SHIFT ) + ( 32 << FRACBITS );
	Position.z = 0;

	return ( Position );
}

//*****************************************************************************
//
POS_t ASTAR_GetPositionFromIndex( LONG lXIdx, LONG lYIdx )
{
	POS_t	Position;

	Position.x = ( lXIdx << ASTAR_NODE_SHIFT ) + (( g_lMapXMin >> ASTAR_NODE_SHIFT ) << ASTAR_NODE_SHIFT ) + ( 32 << FRACBITS );
	Position.y = ( lYIdx << ASTAR_NODE_SHIFT ) + (( g_lMapYMin >> ASTAR_NODE_SHIFT ) << ASTAR_NODE_SHIFT ) + ( 32 << FRACBITS );
	Position.z = 0;

	return ( Position );
}

//*****************************************************************************
//
void ASTAR_ClearVisualizations( void )
{
	ULONG	ulIdx;
	ULONG	ulIdx2;

	for ( ulIdx = 0; ulIdx < MAX_PATHS; ulIdx++ )
	{
		for ( ulIdx2 = 0; ulIdx2 < (ULONG)g_lNodeListSize; ulIdx2++ )
		{
			if ( g_aPaths[ulIdx].paVisualizations[ulIdx2] != NULL )
			{
				g_aPaths[ulIdx].paVisualizations[ulIdx2]->Destroy( );
				g_aPaths[ulIdx].paVisualizations[ulIdx2] = NULL;
			}
		}
	}
}

//*****************************************************************************
//
void ASTAR_ShowCosts( POS_t Position )
{
	ASTARNODE_t	*pNode;

	pNode = astar_GetNodeFromPoint( Position );

	if ( pNode )
	{
		Printf( "(%d, %d) (%s)\n", static_cast<int> (pNode->lXNodeIdx), static_cast<int> (pNode->lYNodeIdx), pNode->lDirection[1] == 0 ? "N" :
			pNode->lDirection[1] == 1 ? "NE" : 
			pNode->lDirection[1] == 2 ? "E" : 
			pNode->lDirection[1] == 3 ? "SE" : 
			pNode->lDirection[1] == 4 ? "S" : 
			pNode->lDirection[1] == 5 ? "SW" : 
			pNode->lDirection[1] == 6 ? "W" : 
			pNode->lDirection[1] == 7 ? "NW" : "UNKNOWN"
		);
		Printf( "From start (g): %d\n", static_cast<int> (pNode->lCostFromStart[1]) );
		Printf( "From goal (h): %d\n", static_cast<int> (pNode->lTotalCost[1] - pNode->lCostFromStart[1]) );
		Printf( "Total (f): %d\n", static_cast<int> (pNode->lTotalCost[1]) );
	}
}

//*****************************************************************************
//
void ASTAR_ClearPath( LONG lPathIdx )
{
	ULONG	ulIdx;

	g_ulPriorityQueuePosition[lPathIdx] = 0;

	g_aPaths[lPathIdx].bInGoalNode = false;
	g_aPaths[lPathIdx].lStackPos = 0;
	g_aPaths[lPathIdx].pActor = NULL;
	for ( ulIdx = 0; ulIdx < (ULONG)g_lNodeListSize; ulIdx++ )
	{
		if ( g_aPaths[lPathIdx].paVisualizations[ulIdx] != NULL )
		{
			g_aPaths[lPathIdx].paVisualizations[ulIdx]->Destroy( );
			g_aPaths[lPathIdx].paVisualizations[ulIdx] = NULL;
		}

		g_apOpenListPriorityQueue[lPathIdx][ulIdx] = NULL;

		g_aMasterNodeList[ulIdx].bOnClosed[lPathIdx] = false;
		g_aMasterNodeList[ulIdx].bOnOpen[lPathIdx] = false;
		g_aMasterNodeList[ulIdx].lCostFromStart[lPathIdx] = 0;
		g_aMasterNodeList[ulIdx].lTotalCost[lPathIdx] = 0;
		g_aMasterNodeList[ulIdx].pParent[lPathIdx] = NULL;
	}
	g_aPaths[lPathIdx].pCurrentNode = NULL;
	g_aPaths[lPathIdx].pStartNode = NULL;
	g_aPaths[lPathIdx].pGoalNode = NULL;
	for ( ulIdx = 0; ulIdx < MAX_NODES_IN_PATH; ulIdx++ )
		g_aPaths[lPathIdx].pNodeStack[ulIdx] = NULL;
	g_aPaths[lPathIdx].ulFlags = 0;
	g_aPaths[lPathIdx].ulNextStep = 0;
	g_aPaths[lPathIdx].ulNumSearchedNodes = 0;
}

//*****************************************************************************
//
void ASTAR_SelectRandomMapLocation( POS_t *pPos, fixed_t X, fixed_t Y )
{
	LONG	lXIdx;
	LONG	lYIdx;
	LONG	lXNode;
	LONG	lYNode;

	lXNode = ( X >> ASTAR_NODE_SHIFT ) - ( g_lMapXMin >> ASTAR_NODE_SHIFT );
	lYNode = ( Y >> ASTAR_NODE_SHIFT ) - ( g_lMapYMin >> ASTAR_NODE_SHIFT );

	if ( g_lNumHorizontalNodes <= 3 )
	{
		do
		{
			lXIdx = g_RandomRoamSeed ( g_lNumHorizontalNodes );
		} while ( labs( lXNode - lXIdx ) > 8 );
	}
	else
	{
		do
		{
			lXIdx = g_RandomRoamSeed ( g_lNumHorizontalNodes );
		} while (( labs( lXNode - lXIdx ) > 8 ) || ( labs( lXNode - lXIdx ) < 2 ));
	}

	if ( g_lNumVerticalNodes <= 3 )
	{
		do
		{
			lYIdx = g_RandomRoamSeed ( g_lNumVerticalNodes );
		} while ( labs( lYNode - lYIdx ) > 8 );
	}
	else
	{
		do
		{
			lYIdx = g_RandomRoamSeed ( g_lNumVerticalNodes );
		} while (( labs( lYNode - lYIdx ) > 8 ) || ( labs( lYNode - lYIdx ) < 2 ));
	}

	*pPos = ASTAR_GetPositionFromIndex( lXIdx, lYIdx );
}

//*****************************************************************************
//*****************************************************************************
//
static bool astar_PathNextNode( ASTARPATH_t *pPath )
{
	ASTARNODE_t		*pNewNode;
	LONG			lAddedCost;

	g_lNumSearchedNodes++;
	pPath->ulNumSearchedNodes++;

	// First, determine the next step.
	switch ( pPath->ulNextStep )
	{
	case ASTAR_NS_PULLFROMOPENLIST:

		if ( astar_PullNodeFromOpenList( pPath ))
			return ( true );

		pPath->ulNextStep = ASTAR_NS_LOOKABOVE;
		break;
	case ASTAR_NS_LOOKABOVE:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx, pPath->pCurrentNode->lYNodeIdx + 1 );
		lAddedCost = 64;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 0 );

		pPath->ulNextStep = ASTAR_NS_LOOKABOVEFORWARD;
		break;
	case ASTAR_NS_LOOKABOVEFORWARD:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx + 1, pPath->pCurrentNode->lYNodeIdx + 1 );
		lAddedCost = 91;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 1 );

		pPath->ulNextStep = ASTAR_NS_LOOKFORWARD;
		break;
	case ASTAR_NS_LOOKFORWARD:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx + 1, pPath->pCurrentNode->lYNodeIdx );
		lAddedCost = 64;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 2 );

		pPath->ulNextStep = ASTAR_NS_LOOKBELOWFORWARD;
		break;
	case ASTAR_NS_LOOKBELOWFORWARD:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx + 1, pPath->pCurrentNode->lYNodeIdx - 1 );
		lAddedCost = 91;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 3 );

		pPath->ulNextStep = ASTAR_NS_LOOKBELOW;
		break;
	case ASTAR_NS_LOOKBELOW:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx, pPath->pCurrentNode->lYNodeIdx - 1 );
		lAddedCost = 64;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 4 );

		pPath->ulNextStep = ASTAR_NS_LOOKBELOWBACK;
		break;
	case ASTAR_NS_LOOKBELOWBACK:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx - 1, pPath->pCurrentNode->lYNodeIdx - 1 );
		lAddedCost = 91;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 5 );

		pPath->ulNextStep = ASTAR_NS_LOOKBACK;
		break;
	case ASTAR_NS_LOOKBACK:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx - 1, pPath->pCurrentNode->lYNodeIdx );
		lAddedCost = 64;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 6 );

		pPath->ulNextStep = ASTAR_NS_LOOKABOVEBACK;
		break;
	case ASTAR_NS_LOOKABOVEBACK:

		pNewNode = astar_GetNode( pPath->pCurrentNode->lXNodeIdx - 1, pPath->pCurrentNode->lYNodeIdx - 1 );
		lAddedCost = 91;

		astar_ProcessNextPathNode( pPath, pNewNode, lAddedCost, 7 );

		// Now that we've checked all the adjacent nodes, add the parent node to the closed list.
		if ( pPath->pCurrentNode->bOnClosed[g_lCurrentPathIdx] == false )
		{
			pPath->pCurrentNode->bOnClosed[g_lCurrentPathIdx] = true;

			if ( botdebug_shownodes )
			{
				if ( pPath->paVisualizations[( pPath->pCurrentNode->lXNodeIdx * g_lNumVerticalNodes ) + pPath->pCurrentNode->lYNodeIdx] == NULL )
					pPath->paVisualizations[( pPath->pCurrentNode->lXNodeIdx * g_lNumVerticalNodes ) + pPath->pCurrentNode->lYNodeIdx] = Spawn( PClass::FindClass( "PathNode" ), pPath->pCurrentNode->Position.x, pPath->pCurrentNode->Position.y, ONFLOORZ, NO_REPLACE );

				AActor * pPathNode = pPath->paVisualizations[( pPath->pCurrentNode->lXNodeIdx * g_lNumVerticalNodes ) + pPath->pCurrentNode->lYNodeIdx];
				pPathNode->SetState( pPathNode->SpawnState + ASTAR_FRAME_INCLOSED );
			}
		}


		pPath->ulNextStep = ASTAR_NS_PULLFROMOPENLIST;
		break;
	}

	// We haven't finished creating the path, so return false.
	return ( false );
}

//*****************************************************************************
//
static ASTARNODE_t *astar_GetNodeFromPoint( POS_t Point )
{
	LONG	lXNode;
	LONG	lYNode;

//	lXNode = ( Point.x - g_lMapXMin ) >> ASTAR_NODE_SHIFT;
//	lYNode = ( Point.y - g_lMapYMin ) >> ASTAR_NODE_SHIFT;
	lXNode = ( Point.x >> ASTAR_NODE_SHIFT ) - ( g_lMapXMin >> ASTAR_NODE_SHIFT );
	lYNode = ( Point.y >> ASTAR_NODE_SHIFT ) - ( g_lMapYMin >> ASTAR_NODE_SHIFT );

	if (( lXNode >= g_lNumHorizontalNodes ) || ( lXNode < 0 ) ||
		( lYNode >= g_lNumVerticalNodes ) || ( lYNode < 0 ))
	{
		return ( NULL );
	}

	return ( astar_GetNode( lXNode, lYNode ));
}

//*****************************************************************************
//
static LONG astar_GetCostToGoalEstimate( ASTARPATH_t *pPath, ASTARNODE_t *pNode )
{
	return ( P_AproxDistance( pNode->Position.x - pPath->pGoalNode->Position.x, pNode->Position.y - pPath->pGoalNode->Position.y ) / FRACUNIT );
/*
	POS_t	PosGoal;
	POS_t	PosNode;

	PosNode = ASTAR_GetPosition( pNode );
	PosGoal = ASTAR_GetPosition( pPath->pGoalNode );

	return ( P_AproxDistance( PosNode.x - PosGoal.x, PosNode.y - PosGoal.y ));

//	return (( labs( pPath->pGoalNode->lXNodeIdx - pNode->lXNodeIdx ) + ( labs( pPath->pGoalNode->lYNodeIdx - pNode->lYNodeIdx ))) * 64 );

	LONG	lXDiff;
	LONG	lYDiff;

	lXDiff = (( pPath->pGoalNode->lXNodeIdx - pNode->lXNodeIdx ) * 64 ) * (( pPath->pGoalNode->lXNodeIdx - pNode->lXNodeIdx ) * 64 );
	lYDiff = (( pPath->pGoalNode->lYNodeIdx - pNode->lYNodeIdx ) * 64 ) * (( pPath->pGoalNode->lYNodeIdx - pNode->lYNodeIdx ) * 64 );

//	lXDiff *= lXDiff;
//	lYDiff *= lYDiff;

	// Fixme: THIS IS EXTREMELY EXPENSIVE! Revise.
	return ( (LONG)sqrt( lXDiff + lYDiff ));

	return ( (LONG)sqrt(((( pPath->pGoalNode->lXNodeIdx - pNode->lXNodeIdx ) * 64 ) ^ 2 ) + ((( pPath->pGoalNode->lYNodeIdx - pNode->lYNodeIdx ) * 64 ) ^ 2 )));

//	return (( labs( pPath->pGoalNode->lXNodeIdx - pNode->lXNodeIdx ) + labs( pPath->pGoalNode->lYNodeIdx - pNode->lYNodeIdx )) * 15 );

	return (( labs( pPath->pGoalNode->lXNodeIdx - pNode->lXNodeIdx ) * labs( pPath->pGoalNode->lXNodeIdx - pNode->lXNodeIdx )) + 
			( labs( pPath->pGoalNode->lYNodeIdx - pNode->lYNodeIdx ) * labs( pPath->pGoalNode->lYNodeIdx - pNode->lYNodeIdx )));
*/
}

//*****************************************************************************
//
static void astar_PushNodeToStack( ASTARNODE_t *pNode, ASTARPATH_t *pPath )
{
	if ( pPath->lStackPos == MAX_NODES_IN_PATH )
		I_Error( "astar_PushNodeToStack: Number of nodes in path exceeded!" );

	pPath->pNodeStack[pPath->lStackPos++] = pNode;
}

//*****************************************************************************
//
static bool astar_PullNodeFromOpenList( ASTARPATH_t *pPath )
{
	// If there aren't any nodes left in the open list, we're done.
	if ( astar_IsPriorityQueueEmpty( ))
	{
		pPath->ulFlags |= PF_COMPLETE;
		return ( true );
	}

	// Get the lowest cost node from the open stack.
	pPath->pCurrentNode = astar_PopFromPriorityQueue( );
	pPath->pCurrentNode->bOnOpen[g_lCurrentPathIdx] = false;

	if ( botdebug_shownodes )
	{
		if ( pPath->paVisualizations[( pPath->pCurrentNode->lXNodeIdx * g_lNumVerticalNodes ) + pPath->pCurrentNode->lYNodeIdx] == NULL )
			pPath->paVisualizations[( pPath->pCurrentNode->lXNodeIdx * g_lNumVerticalNodes ) + pPath->pCurrentNode->lYNodeIdx] = Spawn( PClass::FindClass( "PathNode" ), pPath->pCurrentNode->Position.x, pPath->pCurrentNode->Position.y, ONFLOORZ, NO_REPLACE );

		AActor * pPathNode = pPath->paVisualizations[( pPath->pCurrentNode->lXNodeIdx * g_lNumVerticalNodes ) + pPath->pCurrentNode->lYNodeIdx];
		pPathNode->SetState( pPathNode->SpawnState + ASTAR_FRAME_OFFOPEN );
	}

	// If this node is the goal node, we've found the goal node. Now we can construct a path
	// back to the goal node.
	if ( pPath->pCurrentNode == pPath->pGoalNode )
	{
		ASTARNODE_t	*pNextNode;
		bool		bPushNextNode = true;

		// Construct path.
		pNextNode = pPath->pGoalNode;
		while ( pNextNode->pParent[g_lCurrentPathIdx] && pNextNode->pParent[g_lCurrentPathIdx]->pParent[g_lCurrentPathIdx] )
		{
			if (( bPushNextNode ) || ( pNextNode->lDirection[g_lCurrentPathIdx] != pNextNode->pParent[g_lCurrentPathIdx]->lDirection[g_lCurrentPathIdx] ))
			{
				astar_PushNodeToStack( pNextNode, pPath );

				if ( botdebug_shownodes )
				{
					if ( pPath->paVisualizations[( pNextNode->lXNodeIdx * g_lNumVerticalNodes ) + pNextNode->lYNodeIdx] == NULL )
						pPath->paVisualizations[( pNextNode->lXNodeIdx * g_lNumVerticalNodes ) + pNextNode->lYNodeIdx] = Spawn( PClass::FindClass( "PathNode" ), pNextNode->Position.x, pNextNode->Position.y, ONFLOORZ, NO_REPLACE );

					AActor * pPathNode = pPath->paVisualizations[( pNextNode->lXNodeIdx * g_lNumVerticalNodes ) + pNextNode->lYNodeIdx];
					pPathNode ->SetState( pPathNode->SpawnState + ASTAR_FRAME_ONPATH );
				}
			}

			if (( pNextNode == pPath->pGoalNode ) || ( pNextNode->lDirection[g_lCurrentPathIdx] != pNextNode->pParent[g_lCurrentPathIdx]->lDirection[g_lCurrentPathIdx] ))
				bPushNextNode = true;
			else
				bPushNextNode = false;

			pNextNode = pNextNode->pParent[g_lCurrentPathIdx];
		}

		// If there's 1 or less nodes in the path, just push the goal node.
		if ( pPath->lStackPos == 0 )
			astar_PushNodeToStack( pPath->pGoalNode, pPath );
		else if ( pPath->lStackPos > 1 )
		{
			ASTARNODE_t		*pNode;
			ASTARNODE_t		*pNecessaryNodeList[MAX_NODES_IN_PATH];
			LONG			lListPos;
			LONG			lStackPos;
			POS_t			GoalPos;
			POS_t			NodePos;

			lListPos = 0;
			lStackPos = 1;
			pNode = pPath->pNodeStack[pPath->lStackPos - lStackPos];
			GoalPos = ASTAR_GetPositionFromIndex( pPath->pGoalNode->lXNodeIdx, pPath->pGoalNode->lYNodeIdx );
			while (( pNode != pPath->pGoalNode ) && ( pNode != pPath->pGoalNode->pParent[g_lCurrentPathIdx] ))
			{
				NodePos = ASTAR_GetPositionFromIndex( pNode->lXNodeIdx, pNode->lYNodeIdx );
				pNecessaryNodeList[lListPos++] = pNode;

				if ( BOTPATH_TryWalk( pPath->pActor, NodePos.x, NodePos.y, pPath->pActor->z, GoalPos.x, GoalPos.y ) & (BOTPATH_OBSTRUCTED|BOTPATH_DAMAGINGSECTOR) )
				{
					lStackPos++;
					pNode = pPath->pNodeStack[pPath->lStackPos - lStackPos];
				}
				else
				{
					ULONG	ulIdx;

					for ( ulIdx = 0; ulIdx < MAX_NODES_IN_PATH; ulIdx++ )
						pPath->pNodeStack[ulIdx] = NULL;

					pPath->lStackPos = 0;
					astar_PushNodeToStack( pPath->pGoalNode, pPath );

					for ( ulIdx = 0; ulIdx < (ULONG)lListPos; ulIdx++ )
						astar_PushNodeToStack( pNecessaryNodeList[ulIdx], pPath );

					break;
				}
			}
		}

		pPath->ulFlags |= PF_COMPLETE|PF_SUCCESS;
		return ( true );
	}

	return ( false );
}

//*****************************************************************************
//
static void astar_ProcessNextPathNode( ASTARPATH_t *pPath, ASTARNODE_t *pNode, LONG lAddedCost, LONG lDirection )
{
	LONG	lNewCost;

	if ( pNode == NULL )
		return;

	// This node is on the closed list. Don't do anything with it.
	if ( pNode->bOnClosed[g_lCurrentPathIdx] )
		return;

	// Issue a small penalty for changing directions.
	if ( lDirection != pPath->pCurrentNode->lDirection[g_lCurrentPathIdx] )
		lAddedCost = (LONG)( lAddedCost * 1.5 );

	// Check if it's possible to get to this new node.
	{
		POS_t			CurPos;
		POS_t			DestPos;
		sector_t		*pSector;
//		fixed_t			vx;
//		fixed_t			vy;
//		fixed_t			vz;
//		fixed_t			Angle;
//		fixed_t			Pitch;
//		FTraceResults	TraceResults;
		ULONG			ulResults;
		fixed_t			Distance;

		if ( pPath->pCurrentNode == pPath->pStartNode )
		{
			CurPos.x = pPath->pActor->x;
			CurPos.y = pPath->pActor->y;
		}
		else
			CurPos = pPath->pCurrentNode->Position;

		DestPos = pNode->Position;
		pSector = R_PointInSubsector( CurPos.x, CurPos.y )->sector;

//		Angle = R_PointToAngle2( CurPos.x, CurPos.y, DestPos.x, DestPos.y ) >> ANGLETOFINESHIFT;
//		Pitch = 0;
	
//		vx = FixedMul( finecosine[Pitch], finecosine[Angle/* >> ANGLETOFINESHIFT*/] );
//		vy = FixedMul( finecosine[Pitch], finesine[Angle/* >> ANGLETOFINESHIFT*/] );
//		vz = finesine[Pitch];

		// THIS NEEDS TO CHANGE IF ASTAR_NODE_SIZE EVER CHANGES!!!
		if (( lDirection == 0 ) ||
			( lDirection == 2 ) ||
			( lDirection == 4 ) ||
			( lDirection == 6 ))
		{
			Distance = 64 * FRACUNIT;
		}
		else
		{
			Distance = 91 * FRACUNIT;
		}
/*
		if ( Trace( CurPos.x,
			CurPos.y,
			pSector->floorplane.ZatPoint( CurPos.x, CurPos.y ) + gameinfo.StepHeight,
			pSector,
			vx,
			vy,
			vz,
			Distance,
			MF_SOLID|MF_SHOOTABLE,
			0,
			pPath->pActor,
			TraceResults ))
		{
			return;
		}
*/
		ulResults = BOTPATH_TryWalk( pPath->pActor, CurPos.x, CurPos.y, pSector->floorplane.ZatPoint( CurPos.x, CurPos.y ), DestPos.x, DestPos.y );
		if (( ulResults & BOTPATH_OBSTRUCTED ) || (( pPath->pActor->player->pSkullBot->m_ulPathType == BOTPATHTYPE_ROAM ) && ( ulResults & BOTPATH_DAMAGINGSECTOR )))
			return;

		// If this sector is a damaging sector, make it more costly to go through here.
		switch ( pSector->special )
		{
		case dDamage_End:

			break;
		case dDamage_Hellslime:

			lAddedCost += 32;
			break;
		case dDamage_SuperHellslime:
		case dLight_Strobe_Hurt:

			lAddedCost += 64;
			break;
		case dDamage_Nukage:
		case dDamage_LavaWimpy:
		case dScroll_EastLavaDamage:

			lAddedCost += 16;
			break;
		case dDamage_LavaHefty:

			lAddedCost += 24;
			break;
		default:

			break;
		}
	}

	lNewCost = pPath->pCurrentNode->lCostFromStart[g_lCurrentPathIdx] + lAddedCost;// + astar_TraverseCost( pPath->pCurrentNode, pNode );

	// If this node is already in the open list, and this path to the node isn't any better,
	// don't do anything.
	if (( pNode->bOnOpen[g_lCurrentPathIdx] ) && ( lNewCost >= pNode->lCostFromStart[g_lCurrentPathIdx] ))
	{
		return;
	}
	// Store the new or improved information.
	else
	{
		pNode->pParent[g_lCurrentPathIdx] = pPath->pCurrentNode;
		pNode->lDirection[g_lCurrentPathIdx] = lDirection;
		if ( pPath->pCurrentNode == pNode )
			I_Error( "astar_ProcessNextPathNode: Parent node same as child node!" );
		pNode->lCostFromStart[g_lCurrentPathIdx] = lNewCost;
		pNode->lTotalCost[g_lCurrentPathIdx] = pNode->lCostFromStart[g_lCurrentPathIdx] + astar_GetCostToGoalEstimate( pPath, pNode );

		if ( pNode->bOnOpen[g_lCurrentPathIdx] == false )
		{
			pNode->bOnOpen[g_lCurrentPathIdx] = true;
			astar_InsertToPriorityQueue( pNode );

			if ( botdebug_shownodes )
			{
				if ( pPath->paVisualizations[( pNode->lXNodeIdx * g_lNumVerticalNodes ) + pNode->lYNodeIdx] == NULL )
					pPath->paVisualizations[( pNode->lXNodeIdx * g_lNumVerticalNodes ) + pNode->lYNodeIdx] = Spawn( PClass::FindClass( "PathNode" ), pNode->Position.x, pNode->Position.y, ONFLOORZ, NO_REPLACE );

				AActor *pPathNode = pPath->paVisualizations[( pNode->lXNodeIdx * g_lNumVerticalNodes ) + pNode->lYNodeIdx];
				pPathNode->SetState( pPathNode->SpawnState + ASTAR_FRAME_INOPEN );
			}
		}
	}
}

//*****************************************************************************
//
static ASTARNODE_t *astar_GetNode( LONG lXNodeIdx, LONG lYNodeIdx )
{
	if (( lXNodeIdx >= g_lNumHorizontalNodes ) || ( lXNodeIdx < 0 ) ||
		( lYNodeIdx >= g_lNumVerticalNodes ) || ( lYNodeIdx < 0 ))
	{
		return ( NULL );
	}

	return ( &g_aMasterNodeList[( lXNodeIdx * g_lNumVerticalNodes ) + lYNodeIdx] );
}

//*****************************************************************************
//
static void astar_InsertToPriorityQueue( ASTARNODE_t *pNode )
{
	g_apOpenListPriorityQueue[g_lCurrentPathIdx][++g_ulPriorityQueuePosition[g_lCurrentPathIdx]] = pNode;

	// Resort the priority queue.
	astar_FixUpPriorityQueue( g_ulPriorityQueuePosition[g_lCurrentPathIdx] );
}

//*****************************************************************************
//
static ASTARNODE_t *astar_PopFromPriorityQueue( void )
{
	astar_Exchange( &g_apOpenListPriorityQueue[g_lCurrentPathIdx][g_ulPriorityQueuePosition[g_lCurrentPathIdx]], &g_apOpenListPriorityQueue[g_lCurrentPathIdx][1] );
	astar_FixDownPriorityQueue( 1, g_ulPriorityQueuePosition[g_lCurrentPathIdx] - 1 );

	return ( g_apOpenListPriorityQueue[g_lCurrentPathIdx][g_ulPriorityQueuePosition[g_lCurrentPathIdx]--] );
}

//*****************************************************************************
//
static void astar_FixUpPriorityQueue( ULONG ulPosition )
{
	while (( ulPosition > 1 ) &&
		( astar_GetTotalCost( g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulPosition] ) < astar_GetTotalCost( g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulPosition / 2] )))
	{
		astar_Exchange( &g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulPosition], &g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulPosition / 2] );
		ulPosition /= 2;
	}
}

//*****************************************************************************
//
static void astar_FixDownPriorityQueue( ULONG ulStartPosition, ULONG ulEndPosition )
{
	ULONG		ulChild;
	ASTARNODE_t	*pTempNode;

	pTempNode = g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulStartPosition];
	while (( ulStartPosition * 2 ) < ulEndPosition )
	{
		ulChild = ulStartPosition * 2;

		// If there is a right child and it is bigger than the left child, move it.
		if (( ulChild < ulEndPosition ) && ( astar_GetTotalCost( g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulChild + 1] ) < astar_GetTotalCost( g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulChild] )))
			ulChild++;

		// Move child up?
		if ( astar_GetTotalCost( g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulChild] ) < astar_GetTotalCost( pTempNode ))
			g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulStartPosition] = g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulChild];
		else
			break;

		ulStartPosition = ulChild;
	}

	g_apOpenListPriorityQueue[g_lCurrentPathIdx][ulStartPosition] = pTempNode;
}

//*****************************************************************************
//
static bool astar_IsPriorityQueueEmpty( void )
{
	return ( g_ulPriorityQueuePosition[g_lCurrentPathIdx] == 0 );
}

//*****************************************************************************
//
static void astar_Exchange( ASTARNODE_t **pNode1, ASTARNODE_t **pNode2 )
{
	ASTARNODE_t	*pNode3;

	pNode3 = *pNode1;
	*pNode1 = *pNode2;
	*pNode2 = pNode3;
}

//*****************************************************************************
//
static ULONG astar_GetTotalCost( ASTARNODE_t *pNode )
{
	return ( pNode == NULL ? 0 : pNode->lTotalCost[g_lCurrentPathIdx] );
}

//*****************************************************************************
//	STATISTICS

ADD_STAT( pathing )
{
	FString	Out;

	Out.Format( "Pathing cycles = %04.1f ms (%3d nodes pathed)", 
		g_PathingCycles.TimeMS(),
		static_cast<int> (g_lNumSearchedNodes)
		);

	return ( Out );
}
