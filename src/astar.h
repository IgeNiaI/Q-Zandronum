//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2004 Brad Carney
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
// Filename: astar.h
//
// Description: 
//
//-----------------------------------------------------------------------------

#ifndef __ASTAR_H__
#define __ASTAR_H__

#include "actor.h"
#include "doomtype.h"

//*****************************************************************************
//	DEFINES

// Size of each astar node.
#define	ASTAR_NODE_SIZE			64

#define	ASTAR_NODE_SHIFT		22

#define	MAX_PATHS				( MAXPLAYERS * 2 )

#define	MAX_NODES_IN_PATH		128

// Maximum number of nodes that can be pathed in a tick.
#define	MAX_NODES_TO_SEARCH		1//256

// The path has been initialized.
#define	PF_INITIALIZED			1

// The path has been completed.
#define	PF_COMPLETE				2

// The pathing algorith successfully created a path from start to goal.
#define	PF_SUCCESS				4

#define	ASTAR_FRAME_INOPEN		0
#define	ASTAR_FRAME_OFFOPEN		1
#define	ASTAR_FRAME_INCLOSED	2
#define	ASTAR_FRAME_ONPATH		3

// Possible next steps in the pathing process.
#define	ASTAR_NS_PULLFROMOPENLIST		0
#define	ASTAR_NS_LOOKABOVE				1
#define	ASTAR_NS_LOOKABOVEFORWARD		2
#define	ASTAR_NS_LOOKFORWARD			3
#define	ASTAR_NS_LOOKBELOWFORWARD		4
#define	ASTAR_NS_LOOKBELOW				5
#define	ASTAR_NS_LOOKBELOWBACK			6
#define	ASTAR_NS_LOOKBACK				7
#define	ASTAR_NS_LOOKABOVEBACK			8

//*****************************************************************************
//	STRUCTURES

typedef struct ASTARNODE_s
{
	// The XY Node indicies this node is in.
	LONG				lXNodeIdx;
	LONG				lYNodeIdx;

	// The XY coordinates of the center of this node.
	POS_t				Position;

	// Parent of this node.
	struct ASTARNODE_s		*pParent[MAX_PATHS];

	// Cost of getting from the start node to this node.
	LONG				lCostFromStart[MAX_PATHS];

	// lCostFromStart (g, or "gone") + h, or "heuristic".
	LONG				lTotalCost[MAX_PATHS];

	// Is this node on the open list?
	bool				bOnOpen[MAX_PATHS];

	// Is this node on the closed list?
	bool				bOnClosed[MAX_PATHS];

	// Direction this node.
	LONG				lDirection[MAX_PATHS];

} ASTARNODE_t;

//*****************************************************************************
typedef struct
{
	// Goal node of the next location.
	ASTARNODE_t		*pNode;

	// Is the node returned the goal node?
	bool			bIsGoal;

	// Return flags for this return struct.
	ULONG			ulFlags;

	// The total cost to get to the goal node.
	LONG			lTotalCost;

} ASTARRETURNSTRUCT_t;

//*****************************************************************************
typedef struct
{
	// Flags for this path (initialized, complete, successful, etc.)
	ULONG			ulFlags;

	// The list of all the nodes to follow in this path.
	ASTARNODE_t		*pNodeStack[MAX_NODES_IN_PATH];

	// Current position in the node stack.
	LONG			lStackPos;

	// Has the goal node been reached?
	bool			bInGoalNode;

	// The next step in the pathing process.
	ULONG			ulNextStep;

	// The current node being used in the pathing process.
	ASTARNODE_t		*pCurrentNode;

	// The starting node in this path. The actor doing the pathing lies in this node.
	ASTARNODE_t		*pStartNode;

	// The goal node in this path.
	ASTARNODE_t		*pGoalNode;

	// Actor this path belongs to.
	AActor			*pActor;

	// How many nodes have been searched?
	ULONG			ulNumSearchedNodes;

	// Dynamic array of visualizations for this path.
	AActor			**paVisualizations;

} ASTARPATH_t;

//*****************************************************************************
//	PROTOTYPES

void				ASTAR_Construct( void );
void				ASTAR_BuildNodes( void );
void				ASTAR_ClearNodes( void );
bool				ASTAR_IsInitialized( void );
ASTARRETURNSTRUCT_t	ASTAR_Path( ULONG ulIdx, POS_t GoalPoint, float fMaxSearchNodes, LONG lGiveUpLimit );
POS_t				ASTAR_GetPosition( ASTARNODE_t *pNode );
POS_t				ASTAR_GetPositionFromIndex( LONG lXIdx, LONG lYIdx );
void				ASTAR_ClearVisualizations( void );
void				ASTAR_ShowCosts( POS_t Position );
void				ASTAR_ClearPath( LONG lPathIdx );
void				ASTAR_SelectRandomMapLocation( POS_t *pPos, fixed_t X, fixed_t Y );

#endif	// __ASTAR_H__
