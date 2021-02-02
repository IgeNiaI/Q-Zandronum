/*
** p_effect.cpp
** Particle effects
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
** If particles used real sprites instead of blocks, they could be much
** more useful.
*/

#include "doomtype.h"
#include "doomstat.h"
#include "i_system.h"
#include "c_cvars.h"
#include "actor.h"
#include "m_argv.h"
#include "p_effect.h"
#include "p_local.h"
#include "g_level.h"
#include "v_video.h"
#include "m_random.h"
#include "r_defs.h"
#include "s_sound.h"
#include "templates.h"
#include "gi.h"
#include "v_palette.h"
#include "colormatcher.h"
// [BB] New #includes.
#include "deathmatch.h"
#include "network.h"
#include "team.h" // [CK]
#include "doomdata.h"
#include "v_palette.h"

// [CK] Prototypes
static void MakeFountain (fixed_t x, fixed_t y, fixed_t z, fixed_t radius, fixed_t height, int color1, int color2);

CVAR (Int, cl_rockettrails, 1, CVAR_ARCHIVE);
CVAR (Int, cl_grenadetrails, 1, CVAR_ARCHIVE);
CVAR (Int, cl_respawninvuleffect, 1, CVAR_ARCHIVE);
CVAR (Bool, cl_showspawns, false, CVAR_ARCHIVE); // [CK] Particle fountains at spawns
CVAR (Bool, r_rail_smartspiral, 0, CVAR_ARCHIVE);
CVAR (Int, r_rail_spiralsparsity, 1, CVAR_ARCHIVE);
CVAR (Int, r_rail_trailsparsity, 1, CVAR_ARCHIVE);
CVAR (Bool, r_particles, true, 0);

FRandom pr_railtrail("RailTrail");

#define FADEFROMTTL(a)	(255/(a))

// [RH] particle globals
WORD			NumParticles;
WORD			ActiveParticles;
WORD			InactiveParticles;
particle_t		*Particles;
TArray<WORD>	ParticlesInSubsec;

static int grey1, grey2, grey3, grey4, red, red2, red3, red4, green, blue, yellow, black,
		   red1, green1, blue1, yellow1, yellow2, yellow3, purple, purple1, purple2, purple3, white,
		   rblue1, rblue2, rblue3, rblue4, orange, yorange, dred,  dred2,
		   dred3, dred4, grey5, grey6, maroon1, maroon2, blood1, blood2, gold1, gold2, cyan1, cyan2, green2;


static const struct ColorList {
	int *color;
	BYTE r, g, b;
} Colors[] = {
	{&grey1,	85,  85,  85 },
	{&grey2,	171, 171, 171},
	{&grey3,	50,  50,  50 },
	{&grey4,	210, 210, 210},
	{&grey5,	128, 128, 128},
	{&grey6,	139, 139, 139},
	{&red,		255, 0,   0  },  
	{&red1,		255, 127, 127},
	{&red2,		227, 0,   0  },
	{&red3,		255, 31,  31 },
	{&red4,		203, 0,   0  },
	{&green,	0,   200, 0  },  
	{&green1,	127, 255, 127},
	{&green2,	71,  131, 58 },
	{&blue,		0,   0,   255},
	{&blue1,	127, 127, 255},
	{&yellow,	255, 255, 0  },  
	{&yellow1,	255, 255, 180},
	{&yellow2,	255, 255, 35 },  
	{&yellow3,	255, 255, 71 },  
	{&black,	0,   0,   0  },  
	{&purple,	120, 0,   160},
	{&purple1,	200, 30,  255},
	{&purple2,	207, 0,   207},
	{&purple3,	255, 0,   255},
	{&white, 	255, 255, 255},
	{&rblue1,	81,  81,  255},
	{&rblue2,	0,   0,   227},
	{&rblue3,	0,   0,   130},
	{&rblue4,	0,   0,   80 },
	{&orange,	255, 120, 0  },
	{&yorange,	255, 170, 0  },
	{&dred,		91,  3,   3  },
	{&dred2,	127, 3,   3  },
	{&dred3,	227, 0,	  0  },
	{&dred4,	255, 31,  31 },
	{&maroon1,	154, 49,  49 },
	{&maroon2,	125, 24,  24 },
	{&gold1,	204, 168, 62 },
	{&gold2,	186, 139, 44 },
	{&cyan1,	0,   255, 255},
	{&cyan2,	81,  255, 255},	
	{NULL, 0, 0, 0 }
};

// [Dusk] Lookup table based on the old rainbow trail code
static int g_rainbowParticleColorIndex = 0;
static int* g_rainbowParticleColors[] =
{
	&red,
	&orange,
	&yellow,
	&green,
	&blue,
	&purple,
	&purple3,
};

inline particle_t *NewParticle (void)
{
	particle_t *result = NULL;
	if (InactiveParticles != NO_PARTICLE)
	{
		result = Particles + InactiveParticles;
		InactiveParticles = result->tnext;
		result->tnext = ActiveParticles;
		ActiveParticles = WORD(result - Particles);
	}
	return result;
}

//
// [RH] Particle functions
//
void P_InitParticles ();
void P_DeinitParticles ();

// [BC] Allow the maximum number of particles to be specified by a cvar (so people
// with lots of nice hardware can have lots of particles!).
CUSTOM_CVAR( Int, r_maxparticles, 4000, CVAR_ARCHIVE )
{
	if ( self == 0 )
		self = 4000;
	else if ( self < 100 )
		self = 100;

	if ( gamestate != GS_STARTUP )
	{
		P_DeinitParticles( );
		P_InitParticles( );
	}
}

void P_InitParticles ()
{
	const char *i;

	if ((i = Args->CheckValue ("-numparticles")))
		NumParticles = atoi (i);
	// [BC] Use r_maxparticles now.
	else
		NumParticles = r_maxparticles;

	// This should be good, but eh...
	NumParticles = clamp<WORD>(NumParticles, 100, 65535);

	P_DeinitParticles();
	Particles = new particle_t[NumParticles];
	P_ClearParticles ();
	atterm (P_DeinitParticles);
}

void P_DeinitParticles()
{
	if (Particles != NULL)
	{
		delete[] Particles;
		Particles = NULL;
	}
}

void P_ClearParticles ()
{
	int i;

	memset (Particles, 0, NumParticles * sizeof(particle_t));
	ActiveParticles = NO_PARTICLE;
	InactiveParticles = 0;
	for (i = 0; i < NumParticles-1; i++)
		Particles[i].tnext = i + 1;
	Particles[i].tnext = NO_PARTICLE;
}

// Group particles by subsectors. Because particles are always
// in motion, there is little benefit to caching this information
// from one frame to the next.

void P_FindParticleSubsectors ()
{
	if (ParticlesInSubsec.Size() < (size_t)numsubsectors)
	{
		ParticlesInSubsec.Reserve (numsubsectors - ParticlesInSubsec.Size());
	}

	clearbufshort (&ParticlesInSubsec[0], numsubsectors, NO_PARTICLE);

	if (!r_particles)
	{
		return;
	}
	for (WORD i = ActiveParticles; i != NO_PARTICLE; i = Particles[i].tnext)
	{
		subsector_t *ssec = R_PointInSubsector (Particles[i].x, Particles[i].y);
		int ssnum = int(ssec-subsectors);
		Particles[i].subsector = ssec;
		Particles[i].snext = ParticlesInSubsec[ssnum];
		ParticlesInSubsec[ssnum] = i;
	}
}

static TMap<int, int> ColorSaver;

static uint32 ParticleColor(int rgb)
{
	int *val;
	int stuff;

	val = ColorSaver.CheckKey(rgb);
	if (val != NULL)
	{
		return *val;
	}
	stuff = rgb | (ColorMatcher.Pick(RPART(rgb), GPART(rgb), BPART(rgb)) << 24);
	ColorSaver[rgb] = stuff;
	return stuff;
}

static uint32 ParticleColor(int r, int g, int b)
{
	return ParticleColor(MAKERGB(r, g, b));
}

void P_InitEffects ()
{
	const struct ColorList *color = Colors;

	P_InitParticles();
	while (color->color)
	{
		*(color->color) = ParticleColor(color->r, color->g, color->b);
		color++;
	}

	int kind = gameinfo.defaultbloodparticlecolor;
	blood1 = ParticleColor(kind);
	blood2 = ParticleColor(RPART(kind)/3, GPART(kind)/3, BPART(kind)/3);
}


void P_ThinkParticles ()
{
	int i;
	particle_t *particle, *prev;

	i = ActiveParticles;
	prev = NULL;
	while (i != NO_PARTICLE)
	{
		BYTE oldtrans;

		particle = Particles + i;
		i = particle->tnext;
		oldtrans = particle->trans;
		particle->trans -= particle->fade;
		if (oldtrans < particle->trans || --particle->ttl == 0)
		{ // The particle has expired, so free it
			memset (particle, 0, sizeof(particle_t));
			if (prev)
				prev->tnext = i;
			else
				ActiveParticles = i;
			particle->tnext = InactiveParticles;
			InactiveParticles = (int)(particle - Particles);
			continue;
		}
		particle->x += particle->velx;
		particle->y += particle->vely;
		particle->z += particle->velz;
		particle->velx += particle->accx;
		particle->vely += particle->accy;
		particle->velz += particle->accz;
		prev = particle;
	}
}

// [CK] Refactored code to generate a fountain.
static void GenerateShowSpawnFountain ( FPlayerStart &ts, const int color, const int pnum )
{
	sector_t* sector = P_PointInSector( ts.x, ts.y );

	// Do not spawn particles if the sector is null
	if ( sector != NULL )
	{
		// Only draw the fountain if it's potentially visible
		int rejectnum = pnum + int(sector - sectors);
		if (rejectmatrix == NULL || !(rejectmatrix[rejectnum>>3] & (1 << (rejectnum & 7))))
		{
			// [TP] Take useplayerstartz into account here
			fixed_t floorZ = sector->floorplane.ZatPoint( ts.x, ts.y );
			// [RK] Prevent the fountains from being shifted incorrectly; add the floor and offset.
			fixed_t z = ( level.flags & LEVEL_USEPLAYERSTARTZ ) ? ( floorZ + ts.z ) : floorZ;
			MakeFountain( ts.x, ts.y, z, 16 << FRACBITS, 0, color, color );
		}
	}
}

//
// P_RunEffects
//
// Run effects on all actors in the world
//
void P_RunEffects ()
{
	if (players[consoleplayer].camera == NULL) return;

	int	pnum = int(players[consoleplayer].camera->Sector - sectors) * numsectors;

	AActor *actor;
	TThinkerIterator<AActor> iterator;

	while ( (actor = iterator.Next ()) )
	{
		if (actor->effects)
		{
			// Only run the effect if the actor is potentially visible
			int rnum = pnum + int(actor->Sector - sectors);
			if (rejectmatrix == NULL || !(rejectmatrix[rnum>>3] & (1 << (rnum & 7))))
				P_RunEffect (actor, actor->effects);
		}
	}

	// [CK] If the client wants to show particles at spawns, do so.
	// Teams get special handling due to colors.
	// [BB] DM and Coop starts need different treatment, too.
	if ( cl_showspawns ) 
	{
		if ( teamgame ) 
		{
			for ( ULONG t = 0; t < teams.Size(); t++ )
			{
				const int r = RPART( teams[t].lPlayerColor );
				const int g = GPART( teams[t].lPlayerColor );
				const int b = BPART( teams[t].lPlayerColor );
				const int color = (MAKERGB( r, g, b ) | (ColorMatcher.Pick( r, g, b ) << 24));
				for ( ULONG i = 0; i < teams[t].TeamStarts.Size( ); i++ )
				{
					GenerateShowSpawnFountain( teams[t].TeamStarts[i], color, pnum );
				}
			}
		}
		else if ( deathmatch )
		{
			for ( ULONG i = 0; i < deathmatchstarts.Size( ); i++)
			{
				GenerateShowSpawnFountain( deathmatchstarts[i], grey4, pnum );
			}
		}
		else
		{
			for ( ULONG i = 0; i < MAXPLAYERS; i++)
			{
				if ( playerstarts[i].type != 0 )
					GenerateShowSpawnFountain( playerstarts[i], grey4, pnum );
			}
		}
	}
}

//
// JitterParticle
//
// Creates a particle with "jitter"
//
particle_t *JitterParticle (int ttl)
{
	return JitterParticle (ttl, 1.0);
}
// [XA] Added "drift speed" multiplier setting for enhanced railgun stuffs.
particle_t *JitterParticle (int ttl, float drift)
{
	particle_t *particle = NewParticle ();

	if (particle) {
		fixed_t *val = &particle->velx;
		int i;

		// Set initial velocities
		for (i = 3; i; i--, val++)
			*val = (int)((FRACUNIT/4096) * (M_Random () - 128) * drift);
		// Set initial accelerations
		for (i = 3; i; i--, val++)
			*val = (int)((FRACUNIT/16384) * (M_Random () - 128) * drift);

		particle->trans = 255;	// fully opaque
		particle->ttl = ttl;
		particle->fade = FADEFROMTTL(ttl);
	}
	return particle;
}

// [CK] Refactored fountain that allows custom spawners to be used
static void MakeFountain (fixed_t x, fixed_t y, fixed_t z, fixed_t radius, fixed_t height, int color1, int color2)
{
	particle_t *particle;

	if (!(level.time & 1))
		return;

	particle = JitterParticle (51);

	if (particle)
	{
		angle_t an = M_Random()<<(24-ANGLETOFINESHIFT);
		fixed_t out = FixedMul (radius, M_Random()<<8);

		particle->x = x + FixedMul (out, finecosine[an]);
		particle->y = y + FixedMul (out, finesine[an]);
		particle->z = z + height + FRACUNIT;
		if (out < radius/8)
			particle->velz += FRACUNIT*10/3;
		else
			particle->velz += FRACUNIT*3;
		particle->accz -= FRACUNIT/11;
		if (M_Random() < 30) {
			particle->size = 4;
			particle->color = color2;
		} else {
			particle->size = 6;
			particle->color = color1;
		}
	}
}

static void MakeFountain (AActor *actor, int color1, int color2)
{
	if ( actor == NULL )
		return;

	// [CK] We now use a general function
	MakeFountain(actor->x, actor->y, actor->z, actor->radius, actor->height, color1, color2);
}

void P_RunEffect (AActor *actor, int effects)
{
	angle_t moveangle;
	
	// 512 is the limit below which R_PointToAngle2 does no longer returns usable values.
	if (abs(actor->velx) > 512 || abs(actor->vely) > 512)
	{
		moveangle = R_PointToAngle2(0,0,actor->velx,actor->vely);
	}
	else
	{
		moveangle = actor->angle;
	}

	particle_t *particle;
	int i;

	if ((effects & FX_ROCKET) && (cl_rockettrails & 1))
	{
		// Rocket trail

		fixed_t backx = actor->x - FixedMul (finecosine[(moveangle)>>ANGLETOFINESHIFT], actor->radius*2);
		fixed_t backy = actor->y - FixedMul (finesine[(moveangle)>>ANGLETOFINESHIFT], actor->radius*2);
		fixed_t backz = actor->z - (actor->height>>3) * (actor->velz>>16) + (2*actor->height)/3;

		angle_t an = (moveangle + ANG90) >> ANGLETOFINESHIFT;
		int speed;

		particle = JitterParticle (3 + (M_Random() & 31));
		if (particle) {
			fixed_t pathdist = M_Random()<<8;
			particle->x = backx - FixedMul(actor->velx, pathdist);
			particle->y = backy - FixedMul(actor->vely, pathdist);
			particle->z = backz - FixedMul(actor->velz, pathdist);
			speed = (M_Random () - 128) * (FRACUNIT/200);
			particle->velx += FixedMul (speed, finecosine[an]);
			particle->vely += FixedMul (speed, finesine[an]);
			particle->velz -= FRACUNIT/36;
			particle->accz -= FRACUNIT/20;
			particle->color = yellow;
			particle->size = 2;
		}
		for (i = 6; i; i--) {
			particle_t *particle = JitterParticle (3 + (M_Random() & 31));
			if (particle) {
				fixed_t pathdist = M_Random()<<8;
				particle->x = backx - FixedMul(actor->velx, pathdist);
				particle->y = backy - FixedMul(actor->vely, pathdist);
				particle->z = backz - FixedMul(actor->velz, pathdist) + (M_Random() << 10);
				speed = (M_Random () - 128) * (FRACUNIT/200);
				particle->velx += FixedMul (speed, finecosine[an]);
				particle->vely += FixedMul (speed, finesine[an]);
				particle->velz += FRACUNIT/80;
				particle->accz += FRACUNIT/40;
				if (M_Random () & 7)
					particle->color = grey2;
				else
					particle->color = grey1;
				particle->size = 3;
			} else
				break;
		}
	}
	if ((effects & FX_GRENADE) && (cl_rockettrails & 1))
	{
		// Grenade trail

		P_DrawSplash2 (6,
			actor->x - FixedMul (finecosine[(moveangle)>>ANGLETOFINESHIFT], actor->radius*2),
			actor->y - FixedMul (finesine[(moveangle)>>ANGLETOFINESHIFT], actor->radius*2),
			actor->z - (actor->height>>3) * (actor->velz>>16) + (2*actor->height)/3,
			moveangle + ANG180, 2, 2);
	}
	if (effects & FX_FOUNTAINMASK)
	{
		// Particle fountain

		static const int *fountainColors[16] = 
			{ &black,	&black,
			  &red,		&red1,
			  &green,	&green1,
			  &blue,	&blue1,
			  &yellow,	&yellow1,
			  &purple,	&purple1,
			  &black,	&grey3,
			  &grey4,	&white
			};
		int color = (effects & FX_FOUNTAINMASK) >> 15;
		MakeFountain (actor, *fountainColors[color], *fountainColors[color+1]);
	}
	if (effects & FX_RESPAWNINVUL)
	{
		// Respawn protection

		static const int *protectColors[2] = { &yellow1, &white };

		for (i = 3; i > 0; i--)
		{
			particle = JitterParticle (16);
			if (particle != NULL)
			{
				angle_t ang = M_Random () << (32-ANGLETOFINESHIFT-8);
				particle->x = actor->x + FixedMul (actor->radius, finecosine[ang]);
				particle->y = actor->y + FixedMul (actor->radius, finesine[ang]);
				particle->color = *protectColors[M_Random() & 1];
				particle->z = actor->z;
				particle->velz = FRACUNIT;
				particle->accz = M_Random () << 7;
				particle->size = 1;
				if (M_Random () < 128)
				{ // make particle fall from top of actor
					particle->z += actor->height;
					particle->velz = -particle->velz;
					particle->accz = -particle->accz;
				}
			}
		}
	}
}

void P_DrawSplash (int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int kind)
{
	int color1, color2;

	switch (kind)
	{
	case 1:		// Spark
		color1 = orange;
		color2 = yorange;
		break;
	default:
		return;
	}

	for (; count; count--)
	{
		particle_t *p = JitterParticle (10);
		angle_t an;

		if (!p)
			break;

		p->size = 2;
		p->color = M_Random() & 0x80 ? color1 : color2;
		p->velz -= M_Random () * 512;
		p->accz -= FRACUNIT/8;
		p->accx += (M_Random () - 128) * 8;
		p->accy += (M_Random () - 128) * 8;
		p->z = z - M_Random () * 1024;
		an = (angle + (M_Random() << 21)) >> ANGLETOFINESHIFT;
		p->x = x + (M_Random () & 15)*finecosine[an];
		p->y = y + (M_Random () & 15)*finesine[an];
	}
}

void P_DrawSplash2 (int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int updown, int kind)
{
	int color1, color2, zvel, zspread, zadd;

	switch (kind)
	{
	case 0:		// Blood
		color1 = blood1;
		color2 = blood2;
		break;
	case 1:		// Gunshot
		color1 = grey3;
		color2 = grey5;
		break;
	case 2:		// Smoke
		color1 = grey3;
		color2 = grey1;
		break;
	default:	// colorized blood
		color1 = ParticleColor(kind);
		color2 = ParticleColor(RPART(kind)/3, GPART(kind)/3, BPART(kind)/3);
		break;
	}

	zvel = -128;
	zspread = updown ? -6000 : 6000;
	zadd = (updown == 2) ? -128 : 0;

	for (; count; count--)
	{
		particle_t *p = NewParticle ();
		angle_t an;

		if (!p)
			break;

		p->ttl = 12;
		p->fade = FADEFROMTTL(12);
		p->trans = 255;
		p->size = 4;
		p->color = M_Random() & 0x80 ? color1 : color2;
		p->velz = M_Random () * zvel;
		p->accz = -FRACUNIT/22;
		if (kind) {
			an = (angle + ((M_Random() - 128) << 23)) >> ANGLETOFINESHIFT;
			p->velx = (M_Random () * finecosine[an]) >> 11;
			p->vely = (M_Random () * finesine[an]) >> 11;
			p->accx = p->velx >> 4;
			p->accy = p->vely >> 4;
		}
		p->z = z + (M_Random () + zadd - 128) * zspread;
		an = (angle + ((M_Random() - 128) << 22)) >> ANGLETOFINESHIFT;
		p->x = x + ((M_Random () & 31)-15)*finecosine[an];
		p->y = y + ((M_Random () & 31)-15)*finesine[an];
	}
}

// [Dusk]
static int P_RainbowParticleColor( )
{
	int index = g_rainbowParticleColorIndex++ % countof( g_rainbowParticleColors );
	return *( g_rainbowParticleColors[index] );
}

void P_DrawRailTrail (AActor *source, const FVector3 &start, const FVector3 &end, int color1, int color2, float maxdiff, int flags, const PClass *spawnclass, angle_t angle, int duration, float sparsity, float drift)
{
	// [BC] The server has no need to draw a railgun trail.
	if ( NETWORK_GetState( ) == NETSTATE_SERVER )
		return;

	double length, lengthsquared;
	int steps, i;
	FAngle deg;
	FVector3 step, dir, pos, extend;
	bool fullbright;

	dir = end - start;
	lengthsquared = dir | dir;
	length = sqrt(lengthsquared);
	steps = xs_FloorToInt(length / 3);
	fullbright = !!(flags & RAF_FULLBRIGHT);

	if (steps)
	{
		if (!(flags & RAF_SILENT))
		{
			FSoundID sound;
			
			// Allow other sounds than 'weapons/railgf'!
			if (!source->player) sound = source->AttackSound;
			else if (source->player->ReadyWeapon) sound = source->player->ReadyWeapon->AttackSound;
			else sound = 0;
			if (!sound) sound = "weapons/railgf";

			// The railgun's sound is special. It gets played from the
			// point on the slug's trail that is closest to the hearing player.
			AActor *mo = players[consoleplayer].camera;

			// [BB] The camera can be NULL on the client, so provide a fall back.
			if ( mo == NULL )
				mo = source;

			FVector3 point;
			double r;
			float dirz;

			if (abs(mo->x - FLOAT2FIXED(start.X)) < 20 * FRACUNIT
				&& (mo->y - FLOAT2FIXED(start.Y)) < 20 * FRACUNIT)
			{ // This player (probably) fired the railgun
				S_Sound (mo, CHAN_WEAPON, sound, 1, ATTN_NORM);
			}
			else
			{

				// Only consider sound in 2D (for now, anyway)
				// [BB] You have to divide by lengthsquared here, not multiply with it.

				r = ((start.Y - FIXED2FLOAT(mo->y)) * (-dir.Y) - (start.X - FIXED2FLOAT(mo->x)) * (dir.X)) / lengthsquared;
				r = clamp<double>(r, 0., 1.);

				dirz = dir.Z;
				dir.Z = 0;
				point = start + r * dir;
				dir.Z = dirz;

				S_Sound (FLOAT2FIXED(point.X), FLOAT2FIXED(point.Y), viewz,
					CHAN_WEAPON, sound, 1, ATTN_NORM);
			}
		}
	}
	else
	{
		// line is 0 length, so nothing to do
		return;
	}

	dir /= length;

	//Calculate PerpendicularVector (extend, dir):
	double minelem = 1;
	int epos;
	for (epos = 0, i = 0; i < 3; ++i)
	{
		if (fabs(dir[i]) < minelem)
		{
			epos = i;
			minelem = fabs(dir[i]);
		}
	}
	FVector3 tempvec(0,0,0);
	tempvec[epos] = 1;
	extend = tempvec - (dir | tempvec) * dir;
	//

	extend *= 3;
	step = dir * 3;

	// Create the outer spiral.
	if (color1 != -1 && (!r_rail_smartspiral || color2 == -1) && r_rail_spiralsparsity > 0 && (spawnclass == NULL))
	{
		FVector3 spiral_step = step * r_rail_spiralsparsity * sparsity;
		int spiral_steps = (int)(steps * r_rail_spiralsparsity / sparsity);
		
		// [BC] If color1 is -2, then we want a rainbow trail.
		if ( color1 != -2 )
			color1 = color1 == 0 ? -1 : ParticleColor(color1);

		pos = start;
		deg = FAngle(270);
		for (i = spiral_steps; i; i--)
		{
			particle_t *p = NewParticle ();
			FVector3 tempvec;

			if (!p)
				return;

			int spiralduration = (duration == 0) ? 35 : duration;

			p->trans = 255;
			p->ttl = duration;
			p->fade = FADEFROMTTL(spiralduration);
			p->size = 3;
			p->bright = fullbright;

			tempvec = FMatrix3x3(dir, deg) * extend;
			p->velx = FLOAT2FIXED(tempvec.X * drift)>>4;
			p->vely = FLOAT2FIXED(tempvec.Y * drift)>>4;
			p->velz = FLOAT2FIXED(tempvec.Z * drift)>>4;
			tempvec += pos;
			p->x = FLOAT2FIXED(tempvec.X);
			p->y = FLOAT2FIXED(tempvec.Y);
			p->z = FLOAT2FIXED(tempvec.Z);
			pos += spiral_step;
			deg += FAngle(r_rail_spiralsparsity * 14);

			if (color1 == -1)
			{
				int rand = M_Random();

				if (rand < 155)
					p->color = rblue2;
				else if (rand < 188)
					p->color = rblue1;
				else if (rand < 222)
					p->color = rblue3;
				else
					p->color = rblue4;
			}
			// [BC] Handle the rainbow trail.
			// [Dusk] Refactored.
			else if ( color1 == -2 )
			{
				p->color = P_RainbowParticleColor();
			}
			else
			{
				p->color = color1;
			}
		}
	}

	// Create the inner trail.
	if (color2 != -1 && r_rail_trailsparsity > 0 && spawnclass == NULL)
	{
		FVector3 trail_step = step * r_rail_trailsparsity * sparsity;
		int trail_steps = xs_FloorToInt(steps * r_rail_trailsparsity / sparsity);

		// [BC] 
		static LONG	s_lParticleColor = 0;

		// [BC] If color1 is -2, then we want a rainbow trail.
		if ( color2 != -2 )
			color2 = color2 == 0 ? -1 : ParticleColor(color2);

		FVector3 diff(0, 0, 0);

		pos = start;
		for (i = trail_steps; i; i--)
		{
			// [XA] inner trail uses a different default duration (33).
			int innerduration = (duration == 0) ? 33 : duration;
			particle_t *p = JitterParticle (innerduration, drift);

			if (!p)
				return;

			if (maxdiff > 0)
			{
				int rnd = M_Random ();
				if (rnd & 1)
					diff.X = clamp<float> (diff.X + ((rnd & 8) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 2)
					diff.Y = clamp<float> (diff.Y + ((rnd & 16) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 4)
					diff.Z = clamp<float> (diff.Z + ((rnd & 32) ? 1 : -1), -maxdiff, maxdiff);
			}

			FVector3 postmp = pos + diff;

			p->size = 2;
			p->x = FLOAT2FIXED(postmp.X);
			p->y = FLOAT2FIXED(postmp.Y);
			p->z = FLOAT2FIXED(postmp.Z);
			if (color1 != -1)
				p->accz -= FRACUNIT/4096;
			pos += trail_step;

			p->bright = fullbright;

			if (color2 == -1)
			{
				int rand = M_Random();

				if (rand < 85)
					p->color = grey4;
				else if (rand < 170)
					p->color = grey2;
				else
					p->color = grey1;
			}
			// [BC] Handle the rainbow trail.
			// [Dusk] Refactored.
			else if ( color1 == -2 )
			{
				p->color = P_RainbowParticleColor();
			}
			else 
			{
				p->color = color2;
			}
		}
	}
	// create actors
	if (spawnclass != NULL)
	{
		if (sparsity < 1)
			sparsity = 32;

		FVector3 trail_step = (step / 3) * sparsity;
		int trail_steps = (int)((steps * 3) / sparsity);
		FVector3 diff(0, 0, 0);

		pos = start;
		for (i = trail_steps; i; i--)
		{
			if (maxdiff > 0)
			{
				int rnd = pr_railtrail();
				if (rnd & 1)
					diff.X = clamp<float> (diff.X + ((rnd & 8) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 2)
					diff.Y = clamp<float> (diff.Y + ((rnd & 16) ? 1 : -1), -maxdiff, maxdiff);
				if (rnd & 4)
					diff.Z = clamp<float> (diff.Z + ((rnd & 32) ? 1 : -1), -maxdiff, maxdiff);
			}			
			FVector3 postmp = pos + diff;

			AActor *thing = Spawn (spawnclass, FLOAT2FIXED(postmp.X), FLOAT2FIXED(postmp.Y), FLOAT2FIXED(postmp.Z), ALLOW_REPLACE);
			if (thing) {
				thing->angle = angle;
				thing->target = source;
			}
			pos += trail_step;
		}
	}
}

void P_DisconnectEffect (AActor *actor)
{
	int i;

	if (actor == NULL)
		return;

	for (i = 64; i; i--)
	{
		particle_t *p = JitterParticle (TICRATE*2);

		if (!p)
			break;

		p->x = actor->x + ((M_Random()-128)<<9) * (actor->radius>>FRACBITS);
		p->y = actor->y + ((M_Random()-128)<<9) * (actor->radius>>FRACBITS);
		p->z = actor->z + (M_Random()<<8) * (actor->height>>FRACBITS);
		p->accz -= FRACUNIT/4096;
		p->color = M_Random() < 128 ? maroon1 : maroon2;
		p->size = 4;
	}
}
