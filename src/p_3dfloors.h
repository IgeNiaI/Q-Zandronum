#ifndef __SECTORE_H
#define __SECTORE_H


#define CenterSpot(sec) (vertex_t*)&(sec)->soundorg[0]

#define _3DFLOORS

// 3D floor flags. Most are the same as in Legacy but I added some for EDGE's and Vavoom's features as well.
typedef enum
{
  FF_EXISTS            = 0x1,    //MAKE SURE IT'S VALID
  FF_SOLID             = 0x2,    //Does it clip things?
  FF_RENDERSIDES       = 0x4,    //Render the sides?
  FF_RENDERPLANES      = 0x8,    //Render the floor/ceiling?
  FF_RENDERALL         = 0xC,    //Render everything?
  FF_SWIMMABLE         = 0x10,   //Can we swim?
  FF_NOSHADE           = 0x20,   //Does it mess with the lighting?
  FF_BOTHPLANES        = 0x200,  //Render both planes all the time?
  FF_TRANSLUCENT       = 0x800,  //See through!
  FF_FOG               = 0x1000, //Fog "brush"?
  FF_INVERTPLANES      = 0x2000, //Reverse the plane visibility rules?
  FF_ALLSIDES          = 0x4000, //Render inside and outside sides?
  FF_INVERTSIDES       = 0x8000, //Only render inside sides?
  FF_DOUBLESHADOW      = 0x10000,//Make two lightlist entries to reset light?
  FF_UPPERTEXTURE	   = 0x20000,
  FF_LOWERTEXTURE      = 0x40000,
  FF_THINFLOOR		   = 0x80000,	// EDGE
  FF_SCROLLY           = 0x100000,  // EDGE - not yet implemented!!!
  FF_FIX			   = 0x200000,  // use floor of model sector as floor and floor of real sector as ceiling
  FF_INVERTSECTOR	   = 0x400000,	// swap meaning of sector planes
  FF_DYNAMIC		   = 0x800000,	// created by partitioning another 3D-floor due to overlap
  FF_CLIPPED		   = 0x1000000,	// split into several dynamic ffloors
  FF_SEETHROUGH        = 0x2000000,
  FF_SHOOTTHROUGH      = 0x4000000,
  FF_FADEWALLS         = 0x8000000,	// Applies real fog to walls and doesn't blend the view		
  FF_ADDITIVETRANS	   = 0x10000000, // Render this floor with additive translucency
  FF_FLOOD			   = 0x20000000, // extends towards the next lowest flooding or solid 3D floor or the bottom of the sector
  FF_THISINSIDE		   = 0x40000000, // hack for software 3D with FF_BOTHPLANES
} ffloortype_e;

// This is for the purpose of Sector_SetContents:
#ifdef _MSC_VER
enum : unsigned int // MSVC is apparently the only compiler that supports this syntax
#else
enum
#endif
{
	VC_EMPTY = 0, // Here's the original values of the color shifts in Vavoom, and in ARGB:
	VC_WATER	 = 0x80825032,	// 130, 80, 50, 128		-> 80.82.50.32 (was 0x101080)
	VC_LAVA		 = 0x96FF5000,	// 255, 80, 0, 150		-> 96.FF.50.00 (was 0xf0f010)
	VC_NUKAGE	 = 0x9632FF32,	// 50, 255, 50, 150		-> 96.32.FF.32 (was 0x108010)
	VC_SLIME	 = 0x96001905,	// 0, 25, 5, 150		-> 96.00.19.05 (was 0x287020)
	VC_HELLSLIME = 0x96FF5000,	// 255, 80, 0, 150		-> 96.FF.50.00 (wasn't covered)
	VC_BLOOD	 = 0x96A00A0A,	// 160, 16, 16, 150		-> 96.A0.0A.0A (was 0x801010)
	VC_SLUDGE	 = 0x9680A080,	// 128, 160, 128, 150	-> 96.80.A0.80 (wasn't covered)
	VC_HAZARD	 = 0x8080A080,	// 128, 160, 128, 128	-> 80.80.A0.80 (wasn't covered)
	VC_BOOMWATER = 0x80004FA5,	// Boom WATERMAP:		-> 80.00.4F.A5 (wasn't covered)
	VC_ALPHAMASK = 0xFF000000,
	VC_COLORMASK = 0x00FFFFFF,
};

#ifdef _3DFLOORS


struct secplane_t;
struct FDynamicColormap;


struct F3DFloor
{
	struct planeref
	{
		secplane_t *	plane;
		const FTextureID *	texture;
		const fixed_t *		texheight;
		sector_t *		model;
		int				isceiling;
		int				vindex;
		int				iindex;
	};

	planeref			bottom;
	planeref			top;

	short				*toplightlevel;
	
	fixed_t				delta;
	
	int					flags;
	line_t*				master;
	
	sector_t *			model;
	sector_t *			target;
	
	int					lastlight;
	int					alpha;

	// kg3D - for software
	short	*floorclip;
	short	*ceilingclip;
	int	validcount;

	FDynamicColormap *GetColormap();
	void UpdateColormap(FDynamicColormap *&map);
	PalEntry GetBlend();
};



struct lightlist_t
{
	secplane_t				plane;
	short *					p_lightlevel;
	FDynamicColormap *		extra_colormap;
	PalEntry				blend;
	int						flags;
	F3DFloor*				lightsource;
	F3DFloor*				caster;
};



class player_s;
void P_PlayerOnSpecial3DFloor(player_t* player);

void P_Get3DFloorAndCeiling(AActor * thing, sector_t * sector, fixed_t * floorz, fixed_t * ceilingz, int * floorpic);
bool P_CheckFor3DFloorHit(AActor * mo);
bool P_CheckFor3DCeilingHit(AActor * mo);
void P_CheckFor3DSectorEnter(AActor * mo);
void P_Recalculate3DFloors(sector_t *);
void P_RecalculateAttached3DFloors(sector_t * sec);
void P_RecalculateLights(sector_t *sector);
void P_RecalculateAttachedLights(sector_t *sector);

lightlist_t * P_GetPlaneLight(sector_t * , secplane_t * plane, bool underside);
void P_Spawn3DFloors( void );

struct FLineOpening;

void P_LineOpening_XFloors (FLineOpening &open, AActor * thing, const line_t *linedef, 
							fixed_t x, fixed_t y, fixed_t refx, fixed_t refy, bool restrict);

secplane_t P_FindFloorPlane(sector_t * sector, fixed_t x, fixed_t y, fixed_t z);
int	P_Find3DFloor(sector_t * sec, fixed_t x, fixed_t y, fixed_t z, bool above, bool floor, fixed_t &cmpz);
							
#else

// Dummy definitions for disabled 3D floor code

struct F3DFloor
{
	int dummy;
};

struct lightlist_t
{
	int dummy;
};

class player_s;
inline void P_PlayerOnSpecial3DFloor(player_t* player) {}

inline void P_Get3DFloorAndCeiling(AActor * thing, sector_t * sector, fixed_t * floorz, fixed_t * ceilingz, int * floorpic) {}
inline bool P_CheckFor3DFloorHit(AActor * mo) { return false; }
inline bool P_CheckFor3DCeilingHit(AActor * mo) { return false; }
inline void P_CheckFor3DSectorEnter(AActor * mo) {}
inline void P_Recalculate3DFloors(sector_t *) {}
inline void P_RecalculateAttached3DFloors(sector_t * sec) {}
inline lightlist_t * P_GetPlaneLight(sector_t * , secplane_t * plane, bool underside) { return NULL; }
inline void P_Spawn3DFloors( void ) {}

struct FLineOpening;

inline void P_LineOpening_XFloors (FLineOpening &open, AActor * thing, const line_t *linedef, 
							fixed_t x, fixed_t y, fixed_t refx, fixed_t refy, bool restrict) {}

//secplane_t P_FindFloorPlane(sector_t * sector, fixed_t x, fixed_t y, fixed_t z){return sector->floorplane;}

#endif


#endif
