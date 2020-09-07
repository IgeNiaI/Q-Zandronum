
#include "doomtype.h"
#include "w_wad.h"
#include "i_system.h"
#include "s_sound.h"
#include "c_console.h"
#include "d_player.h"
#include "d_netinf.h"
#include "gi.h"
#include "colormatcher.h"
#include "c_dispatch.h"
#include "r_defs.h"
#include "v_text.h"
#include "r_data/sprites.h"
#include "r_data/voxels.h"
#include "textures/textures.h"

void gl_InitModels();

// variables used to look up
//	and range check thing_t sprites patches
TArray<spritedef_t> sprites;
TArray<spriteframe_t> SpriteFrames;
DWORD			NumStdSprites;		// The first x sprites that don't belong to skins.

struct spriteframewithrotate : public spriteframe_t
{
	int rotate;
}
sprtemp[MAX_SPRITE_FRAMES];
int 			maxframe;
char*			spritename;

// [RH] skin globals
// [BL] Changed to TArray
TArray<FPlayerSkin> skins;
BYTE			OtherGameSkinRemap[256];
PalEntry		OtherGameSkinPalette[256];



//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
// [RH] Removed checks for coexistance of rotation 0 with other
//		rotations and made it look more like BOOM's version.
//
static bool R_InstallSpriteLump (FTextureID lump, unsigned frame, char rot, bool flipped)
{
	unsigned rotation;

	if (rot >= '0' && rot <= '9')
	{
		rotation = rot - '0';
	}
	else if (rot >= 'A')
	{
		rotation = rot - 'A' + 10;
	}
	else
	{
		rotation = 17;
	}

	if (frame >= MAX_SPRITE_FRAMES || rotation > 16)
	{
		Printf (TEXTCOLOR_RED"R_InstallSpriteLump: Bad frame characters in lump %s\n", TexMan[lump]->Name);
		return false;
	}

	if ((int)frame > maxframe)
		maxframe = frame;

	if (rotation == 0)
	{
		// the lump should be used for all rotations
        // false=0, true=1, but array initialised to -1
        // allows doom to have a "no value set yet" boolean value!
		int r;

		for (r = 14; r >= 0; r -= 2)
		{
			if (!sprtemp[frame].Texture[r].isValid())
			{
				sprtemp[frame].Texture[r] = lump;
				if (flipped)
				{
					sprtemp[frame].Flip |= 1 << r;
				}
				sprtemp[frame].rotate = false;
			}
		}
	}
	else
	{
		if (rotation <= 8)
		{
			rotation = (rotation - 1) * 2;
		}
		else
		{
			rotation = (rotation - 9) * 2 + 1;
		}

		if (!sprtemp[frame].Texture[rotation].isValid())
		{
			// the lump is only used for one rotation
			sprtemp[frame].Texture[rotation] = lump;
			if (flipped)
			{
				sprtemp[frame].Flip |= 1 << rotation;
			}
			sprtemp[frame].rotate = true;
		}
	}
	return true;
}


// [RH] Seperated out of R_InitSpriteDefs()
static void R_InstallSprite (int num)
{
	int frame;
	int framestart;
	int rot;
//	int undefinedFix;

	if (maxframe == -1)
	{
		sprites[num].numframes = 0;
		return;
	}

	maxframe++;

	// [RH] If any frames are undefined, but there are some defined frames, map
	// them to the first defined frame. This is a fix for Doom Raider, which actually
	// worked with ZDoom 2.0.47, because of a bug here. It does not define frames A,
	// B, or C for the sprite PSBG, but because I had sprtemp[].rotate defined as a
	// bool, this code never detected that it was not actually present. After switching
	// to the unified texture system, this caused it to crash while loading the wad.

// [RH] Let undefined frames actually be blank because LWM uses this in at least
// one of her wads.
//	for (frame = 0; frame < maxframe && sprtemp[frame].rotate == -1; ++frame)
//	{ }
//
//	undefinedFix = frame;

	for (frame = 0; frame < maxframe; ++frame)
	{
		switch (sprtemp[frame].rotate)
		{
		case -1:
			// no rotations were found for that frame at all
			//I_FatalError ("R_InstallSprite: No patches found for %s frame %c", sprites[num].name, frame+'A');
			break;
			
		case 0:
			// only the first rotation is needed
			for (rot = 1; rot < 16; ++rot)
			{
				sprtemp[frame].Texture[rot] = sprtemp[frame].Texture[0];
			}
			// If the frame is flipped, they all should be
			if (sprtemp[frame].Flip & 1)
			{
				sprtemp[frame].Flip = 0xFFFF;
			}
			break;
					
		case 1:
			// must have all 8 frame pairs
			for (rot = 0; rot < 8; ++rot)
			{
				if (!sprtemp[frame].Texture[rot*2+1].isValid())
				{
					sprtemp[frame].Texture[rot*2+1] = sprtemp[frame].Texture[rot*2];
					if (sprtemp[frame].Flip & (1 << (rot*2)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2+1);
					}
				}
				if (!sprtemp[frame].Texture[rot*2].isValid())
				{
					sprtemp[frame].Texture[rot*2] = sprtemp[frame].Texture[rot*2+1];
					if (sprtemp[frame].Flip & (1 << (rot*2+1)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2);
					}
				}

			}
			for (rot = 0; rot < 16; ++rot)
			{
				if (!sprtemp[frame].Texture[rot].isValid())
					I_FatalError ("R_InstallSprite: Sprite %s frame %c is missing rotations",
									sprites[num].name, frame+'A');
			}
			break;
		}
	}

	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == -1)
		{
			memset (&sprtemp[frame].Texture, 0, sizeof(sprtemp[0].Texture));
			sprtemp[frame].Flip = 0;
			sprtemp[frame].rotate = 0;
		}
	}
	
	// allocate space for the frames present and copy sprtemp to it
	sprites[num].numframes = maxframe;
	sprites[num].spriteframes = WORD(framestart = SpriteFrames.Reserve (maxframe));
	for (frame = 0; frame < maxframe; ++frame)
	{
		memcpy (SpriteFrames[framestart+frame].Texture, sprtemp[frame].Texture, sizeof(sprtemp[frame].Texture));
		SpriteFrames[framestart+frame].Flip = sprtemp[frame].Flip;
		SpriteFrames[framestart+frame].Voxel = sprtemp[frame].Voxel;
	}

	// Let the textures know about the rotations
	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == 1)
		{
			for (int rot = 0; rot < 16; ++rot)
			{
				TexMan[sprtemp[frame].Texture[rot]]->Rotations = framestart + frame;
			}
		}
	}
}


//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//	(4 chars exactly) to be used.
// Builds the sprite rotation matrices to account
//	for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//	a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//	letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs () 
{
	struct Hasher
	{
		int Head, Next;
	} *hashes;
	struct VHasher
	{
		int Head, Next, Name, Spin;
		char Frame;
	} *vhashes;
	unsigned int i, j, smax, vmax;
	DWORD intname;

	// Create a hash table to speed up the process
	smax = TexMan.NumTextures();
	hashes = new Hasher[smax];
	clearbuf(hashes, sizeof(Hasher)*smax/4, -1);
	for (i = 0; i < smax; ++i)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if (tex->UseType == FTexture::TEX_Sprite && strlen(tex->Name) >= 6)
		{
			size_t bucket = tex->dwName % smax;
			hashes[i].Next = hashes[bucket].Head;
			hashes[bucket].Head = i;
		}
	}

	// Repeat, for voxels
	vmax = Wads.GetNumLumps();
	vhashes = new VHasher[vmax];
	clearbuf(vhashes, sizeof(VHasher)*vmax/4, -1);
	for (i = 0; i < vmax; ++i)
	{
		if (Wads.GetLumpNamespace(i) == ns_voxels)
		{
			char name[9];
			size_t namelen;
			int spin;
			int sign;

			Wads.GetLumpName(name, i);
			name[8] = 0;
			namelen = strlen(name);
			if (namelen < 4)
			{ // name is too short
				continue;
			}
			if (name[4] != '\0' && name[4] != ' ' && (name[4] < 'A' || name[4] >= 'A' + MAX_SPRITE_FRAMES))
			{ // frame char is invalid
				continue;
			}
			spin = 0;
			sign = 2;	// 2 to convert from deg/halfsec to deg/sec
			j = 5;
			if (j < namelen && name[j] == '-')
			{ // a minus sign is okay, but only before any digits
				j++;
				sign = -2;
			}
			for (; j < namelen; ++j)
			{ // the remainder to the end of the name must be digits
				if (name[j] >= '0' && name[j] <= '9')
				{
					spin = spin * 10 + name[j] - '0';
				}
				else
				{
					break;
				}
			}
			if (j < namelen)
			{ // the spin part is invalid
				continue;
			}
			memcpy(&vhashes[i].Name, name, 4);
			vhashes[i].Frame = name[4];
			vhashes[i].Spin = spin * sign;
			size_t bucket = vhashes[i].Name % vmax;
			vhashes[i].Next = vhashes[bucket].Head;
			vhashes[bucket].Head = i;
		}
	}

	// scan all the lump names for each of the names, noting the highest frame letter.
	for (i = 0; i < sprites.Size(); ++i)
	{
		memset (sprtemp, 0xFF, sizeof(sprtemp));
		for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
		{
			sprtemp[j].Flip = 0;
			sprtemp[j].Voxel = NULL;
		}
				
		maxframe = -1;
		intname = sprites[i].dwName;

		// scan the lumps, filling in the frames for whatever is found
		int hash = hashes[intname % smax].Head;
		while (hash != -1)
		{
			FTexture *tex = TexMan[hash];
			if (tex->dwName == intname)
			{
				bool res = R_InstallSpriteLump (FTextureID(hash), tex->Name[4] - 'A', tex->Name[5], false);

				if (tex->Name[6] && res)
					R_InstallSpriteLump (FTextureID(hash), tex->Name[6] - 'A', tex->Name[7], true);
			}
			hash = hashes[hash].Next;
		}

		// repeat, for voxels
		hash = vhashes[intname % vmax].Head;
		while (hash != -1)
		{
			VHasher *vh = &vhashes[hash];
			if (vh->Name == (int)intname)
			{
				FVoxelDef *voxdef = R_LoadVoxelDef(hash, vh->Spin);
				if (voxdef != NULL)
				{
					if (vh->Frame == ' ' || vh->Frame == '\0')
					{ // voxel applies to every sprite frame
						for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
						{
							if (sprtemp[j].Voxel == NULL)
							{
								sprtemp[j].Voxel = voxdef;
							}
						}
						maxframe = MAX_SPRITE_FRAMES-1;
					}
					else
					{ // voxel applies to a specific frame
						j = vh->Frame - 'A';
						sprtemp[j].Voxel = voxdef;
						maxframe = MAX<int>(maxframe, j);
					}
				}
			}
			hash = vh->Next;
		}
		
		R_InstallSprite ((int)i);
	}

	delete[] hashes;
	delete[] vhashes;
}

//==========================================================================
//
// R_ExtendSpriteFrames
//
// Extends a sprite so that it can hold the desired frame.
//
//==========================================================================

static void R_ExtendSpriteFrames(spritedef_t &spr, int frame)
{
	unsigned int i, newstart;

	if (spr.numframes >= ++frame)
	{ // The sprite already has enough frames, so do nothing.
		return;
	}

	if (spr.numframes == 0 || (spr.spriteframes + spr.numframes == SpriteFrames.Size()))
	{ // Sprite's frames are at the end of the array, or it has no frames
	  // at all, so we can tack the new frames directly on to the end
	  // of the SpriteFrames array.
		newstart = SpriteFrames.Reserve(frame - spr.numframes);
		if (spr.numframes == 0)
		{
			spr.spriteframes = WORD(newstart);
		}
	}
	else
	{ // We need to allocate space for all the sprite's frames and copy
	  // the existing ones over to the new space. The old space will be
	  // lost.
		newstart = SpriteFrames.Reserve(frame);
		for (i = 0; i < spr.numframes; ++i)
		{
			SpriteFrames[newstart + i] = SpriteFrames[spr.spriteframes + i];
		}
		spr.spriteframes = WORD(newstart);
		newstart += i;
	}
	// Initialize all new frames to 0.
	memset(&SpriteFrames[newstart], 0, sizeof(spriteframe_t)*(frame - spr.numframes));
	spr.numframes = frame;
}

//==========================================================================
//
// VOX_AddVoxel
//
// Sets a voxel for a single sprite frame.
//
//==========================================================================

void VOX_AddVoxel(int sprnum, int frame, FVoxelDef *def)
{
	R_ExtendSpriteFrames(sprites[sprnum], frame);
	SpriteFrames[sprites[sprnum].spriteframes + frame].Voxel = def;
}



// [RH]
// R_InitSkins
// Reads in everything applicable to a skin. The skins should have already
// been counted and had their identifiers assigned to namespaces.
//
#define NUMSKINSOUNDS 18
static const char *skinsoundnames[NUMSKINSOUNDS][2] =
{ // The *painXXX sounds must be the first four
	{ "dsplpain",	"*pain100" },
	{ "dsplpain",	"*pain75" },
	{ "dsplpain",	"*pain50" },
	{ "dsplpain",	"*pain25" },
	{ "dsplpain",	"*poison" },

	{ "dsoof",		"*grunt" },
	{ "dsoof",		"*land" },

	{ "dspldeth",	"*death" },
	{ "dspldeth",	"*wimpydeath" },

	{ "dspdiehi",	"*xdeath" },
	{ "dspdiehi",	"*crazydeath" },

	{ "dsnoway",	"*usefail" },
	{ "dsnoway",	"*puzzfail" },

	{ "dsslop",		"*gibbed" },
	{ "dsslop",		"*splat" },

	{ "dspunch",	"*fist" },
	{ "dsjump",		"*jump" },
	{ "dstaunt",	"*taunt" },
};

/*
static int STACK_ARGS skinsorter (const void *a, const void *b)
{
	return stricmp (((FPlayerSkin *)a)->name, ((FPlayerSkin *)b)->name);
}
*/

static void R_CreateSkin();
void R_InitSkins (void)
{
	FSoundID playersoundrefs[NUMSKINSOUNDS];
	spritedef_t temp;
	int sndlumps[NUMSKINSOUNDS];
	char key[65];
	DWORD intname, crouchname;
	int i;
	int j, k, base;
	int lastlump;
	int aliasid;
	bool remove;
	const PClass *basetype, *transtype;
	int s_skin = 1; // (0 = skininfo, 1 = s_skin, 2 = s_skin non-changeable)
	bool lumpSkininfo = false; // are we parsing the SKININFO lumps

	key[sizeof(key)-1] = 0;
	i = PlayerClasses.Size () - 1;
	lastlump = 0;

	for (j = 0; j < NUMSKINSOUNDS; ++j)
	{
		playersoundrefs[j] = skinsoundnames[j][1];
	}

	//[BL] We need to parse two different lumps.
	while (true)
	{
		if ( !lumpSkininfo && (base = Wads.FindLump ("S_SKIN", &lastlump, true)) == -1)
		{
			lastlump = 0;
			lumpSkininfo = true;
			continue;
		}
		else if( lumpSkininfo && (base = Wads.FindLump( "SKININFO", &lastlump, true )) == -1)
		{
			break;
		}

		//[BL] removed some restrictions since Skulltag doesn't need them.
		//     Also, default to S_SKIN format.
		s_skin = 1;

		R_CreateSkin();
		i++;

		FScanner sc(base);

		// Data is stored as "key = data".
		while (sc.GetString ())
		{
			// [BB] The original SKININFO parser ate everything before the starting bracket.
			// To retain compatibility with existing wads, we need to keep this behavior.
			if ( lumpSkininfo )
			{
				// Parse until we find a starting bracket.
				while ( sc.String[0] != '{' )
				{
					if ( !sc.GetString( ) )
						break;
				}
			}

			if(sc.String[0] == '{')
			{
				if(s_skin == 1)
					s_skin = 0; // Change to SKININFO
				else if(s_skin == 0)
				{
					R_CreateSkin();
					i++; // new skin
				}
				if(s_skin != 2) // If this is at S_SKIN unchangeable then no nothing else get a new string.
					sc.GetString();
			}
			for (j = 0; j < NUMSKINSOUNDS; j++)
				sndlumps[j] = -1;
			skins[i].namespc = Wads.GetLumpNamespace (base);
			intname = 0;
			crouchname = 0;

			remove = false;
			basetype = NULL;
			transtype = NULL;

			if(s_skin == 1)
				s_skin = 2; // If it doesn't find a '{' permanently use S_SKIN format.

			//[BL] We'll now go until we hit a '}' in SKININFO format
			do
			{
				if(s_skin == 0 && sc.String[0] == '}')
					break;

				strncpy (key, sc.String, sizeof(key)-1);
				if (!sc.GetString() || sc.String[0] != '=')
				{
					Printf (PRINT_BOLD, "Bad format for skin %d: %s\n", (int)i, key);
					// [BB] If there was a problem parsing the skin, remove it. Otherwise bad things may happen.
					remove = true;
					break;
				}
				sc.GetString ();
				if (0 == stricmp (key, "name"))
				{
					// [BC] MAX_SKIN_NAME.
					strncpy (skins[i].name, sc.String, MAX_SKIN_NAME);
					for (j = 0; j < i; j++)
					{
						if (stricmp (skins[i].name, skins[j].name) == 0)
						{
							mysnprintf (skins[i].name, countof(skins[i].name), "skin%d", (int)i);
							Printf (PRINT_BOLD, "Skin %s duplicated as %s\n",
								skins[j].name, skins[i].name);
							break;
						}
					}
				}
				else if (0 == stricmp (key, "sprite"))
				{
					for (j = 3; j >= 0; j--)
						sc.String[j] = toupper (sc.String[j]);
					intname = *((DWORD *)sc.String);
				}
				else if (0 == stricmp (key, "crouchsprite"))
				{
					for (j = 3; j >= 0; j--)
						sc.String[j] = toupper (sc.String[j]);
					crouchname = *((DWORD *)sc.String);
				}
				else if (0 == stricmp (key, "face"))
				{
					for (j = 2; j >= 0; j--)
						skins[i].face[j] = toupper (sc.String[j]);
					skins[i].face[3] = '\0';
				}
				else if (0 == stricmp (key, "gender"))
				{
					skins[i].gender = D_GenderToInt (sc.String);
				}
				else if (0 == stricmp (key, "scale"))
				{
					skins[i].ScaleX = clamp<fixed_t> (FLOAT2FIXED(atof (sc.String)), 1, 256*FRACUNIT);
					skins[i].ScaleY = skins[i].ScaleX;
				}
				else if (0 == stricmp (key, "game"))
				{
					if (gameinfo.gametype == GAME_Heretic)
						basetype = PClass::FindClass (NAME_HereticPlayer);
					else if (gameinfo.gametype == GAME_Strife)
						basetype = PClass::FindClass (NAME_StrifePlayer);
					else
						basetype = PClass::FindClass (NAME_DoomPlayer);

					transtype = basetype;

					if (stricmp (sc.String, "heretic") == 0)
					{
						if (gameinfo.gametype & GAME_DoomChex)
						{
							transtype = PClass::FindClass (NAME_HereticPlayer);
							skins[i].othergame = true;
						}
						else if (gameinfo.gametype != GAME_Heretic)
						{
							remove = true;
						}
					}
					else if (stricmp (sc.String, "strife") == 0)
					{
						if (gameinfo.gametype != GAME_Strife)
						{
							remove = true;
						}
					}
					else
					{
						if (gameinfo.gametype == GAME_Heretic)
						{
							transtype = PClass::FindClass (NAME_DoomPlayer);
							skins[i].othergame = true;
						}
						else if (!(gameinfo.gametype & GAME_DoomChex))
						{
							remove = true;
						}
					}

					if (remove)
						break;
				}
				else if (0 == stricmp (key, "class"))
				{ // [GRB] Define the skin for a specific player class
					int pclass = D_PlayerClassToInt (sc.String);

					if (pclass < 0)
					{
						remove = true;
						break;
					}

					basetype = transtype = PlayerClasses[pclass].Type;
				}
				// [BL] Skulltag additions
				else if (0 == stricmp (key, "hidden"))
				{
					if (( stricmp(sc.String, "true") == 0 ) || ( stricmp(sc.String, "yes") == 0 ))
						skins[i].bRevealed = true;
					else if (( stricmp(sc.String, "false") == 0 ) || ( stricmp(sc.String, "no") == 0 ))
						skins[i].bRevealed = false;
				}
				else if (0 == stricmp (key, "cheat"))
				{
					if (( stricmp(sc.String, "true") == 0 ) || ( stricmp(sc.String, "yes") == 0 ))
						skins[i].bCheat = true;
					else if (( stricmp(sc.String, "false") == 0 ) || ( stricmp(sc.String, "no") == 0 ))
						skins[i].bCheat = false;
				}
				else if (0 == stricmp( key, "color" ))
				{
					sprintf( skins[i].szColor, "%s", sc.String );
				}
				else if (key[0] == '*')
				{ // Player sound replacment (ZDoom extension)
					int lump = Wads.CheckNumForName (sc.String, skins[i].namespc);
					if (lump == -1)
					{
						lump = Wads.CheckNumForFullName (sc.String, true, ns_sounds);
					}
					if (lump != -1)
					{
						if (stricmp (key, "*pain") == 0)
						{ // Replace all pain sounds in one go
							aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
								playersoundrefs[0], lump, true);
							for (int l = 3; l > 0; --l)
							{
								S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
									playersoundrefs[l], aliasid, true);
							}
						}
						else
						{
							int sndref = S_FindSoundNoHash (key);
							if (sndref != 0)
							{
								S_AddPlayerSound (skins[i].name, skins[i].gender, sndref, lump, true);
							}
						}
					}
				}
				else
				{
					for (j = 0; j < NUMSKINSOUNDS; j++)
					{
						if (stricmp (key, skinsoundnames[j][0]) == 0)
						{
							sndlumps[j] = Wads.CheckNumForName (sc.String, skins[i].namespc);
							if (sndlumps[j] == -1)
							{ // [BL] no replacement, search all wads?
								sndlumps[j] = Wads.CheckNumForName (sc.String);
							}
							if (sndlumps[j] == -1)
							{ // Replacement not found, try finding it in the global namespace
								sndlumps[j] = Wads.CheckNumForFullName (sc.String, true, ns_sounds);
							}
						}
					}
					//if (j == 8)
					//	Printf ("Funny info for skin %i: %s = %s\n", i, key, sc.String);
				}
			}
			while(sc.GetString());

			// [GRB] Assume Doom skin by default
			if (!remove && basetype == NULL)
			{
				if (gameinfo.gametype & GAME_DoomChex)
				{
					basetype = transtype = PClass::FindClass (NAME_DoomPlayer);
				}
				else if (gameinfo.gametype == GAME_Heretic)
				{
					basetype = PClass::FindClass (NAME_HereticPlayer);
					transtype = PClass::FindClass (NAME_DoomPlayer);
					skins[i].othergame = true;
				}
				else
				{
					remove = true;
				}
			}

			if (!remove)
			{
				skins[i].range0start = transtype->Meta.GetMetaInt (APMETA_ColorRange) & 0xff;
				skins[i].range0end = transtype->Meta.GetMetaInt (APMETA_ColorRange) >> 8;
			
				remove = true;
				for (j = 0; j < (int)PlayerClasses.Size (); j++)
				{
					const PClass *type = PlayerClasses[j].Type;
			
					if (type->IsDescendantOf (basetype) &&
						GetDefaultByType (type)->SpawnState->sprite == GetDefaultByType (basetype)->SpawnState->sprite &&
						type->Meta.GetMetaInt (APMETA_ColorRange) == basetype->Meta.GetMetaInt (APMETA_ColorRange))
					{
						PlayerClasses[j].Skins.Push ((int)i);
						remove = false;
					}
				}
			}
			
			if (!remove)
			{
				if (skins[i].name[0] == 0)
					mysnprintf (skins[i].name, countof(skins[i].name), "skin%d", (int)i);

				// Now collect the sprite frames for this skin. If the sprite name was not
				// specified, use whatever immediately follows the specifier lump.
				// [BL] S_SKIN only
				if (intname == 0 && s_skin != 0)
				{
					char name[9];
					Wads.GetLumpName (name, base+1);
					memcpy(&intname, name, 4);
				}

				int basens = Wads.GetLumpNamespace(base);

				for(int spr = 0; spr<2; spr++)
				{
					memset (sprtemp, 0xFFFF, sizeof(sprtemp));
					for (k = 0; k < MAX_SPRITE_FRAMES; ++k)
					{
						sprtemp[k].Flip = 0;
					sprtemp[k].Voxel = NULL;
					}
					maxframe = -1;

					if (spr == 1)
					{
						if (crouchname !=0 && crouchname != intname)
						{
							intname = crouchname;
						}
						else
						{
							skins[i].crouchsprite = -1;
							break;
						}
					}

					if (intname == 0)
						continue;

					// [BL] From the SKININFO parser
					// Loop through all the lumps searching for frames for this skin.
					for ( k = 0; static_cast<signed> (k) < Wads.GetNumLumps( ); k++ )
					{
						// Only process skin entries from the wad the SKININFO lump is in.
						// NOTE: If this isn't done, Skulltag doesn't work with hr.wad.
						if ( Wads.GetLumpFile( base ) != Wads.GetLumpFile( k ))
							continue;

						char lname[9];
						DWORD lnameint;
						Wads.GetLumpName( lname, k );
						memcpy(&lnameint, lname, 4);
						if (lnameint == intname)
						{
							FTextureID picnum = TexMan.CreateTexture(k, FTexture::TEX_SkinSprite);
							if (!picnum.isValid())
								continue;

							bool res = R_InstallSpriteLump (picnum, lname[4] - 'A', lname[5], false);

							if (lname[6] && res)
								R_InstallSpriteLump (picnum, lname[6] - 'A', lname[7], true);
						}
					}

					if (spr == 0 && maxframe <= 0)
					{
						Printf (PRINT_BOLD, "Skin %s (#%d) has no frames. Removing.\n", skins[i].name, (int)i);
						remove = true;
						break;
					}

					// [BB] S_SKIN only, unless we don't have a proper intname.
					if ( ( s_skin != 0 ) || ( intname == 0 ) )
						Wads.GetLumpName (temp.name, base+1);
					else
						memcpy(temp.name, &intname, 4);
					temp.name[4] = 0;
					int sprno = (int)sprites.Push (temp);
					if (spr==0)	skins[i].sprite = sprno;
					else skins[i].crouchsprite = sprno;
					R_InstallSprite (sprno);
				}
			}

			if (remove)
			{
				skins.Delete(i);
				i--;
				continue;
			}

			// Register any sounds this skin provides
			aliasid = 0;
			for (j = 0; j < NUMSKINSOUNDS; j++)
			{
				if (sndlumps[j] != -1)
				{
					if (j == 0 || sndlumps[j] != sndlumps[j-1])
					{
						aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
							playersoundrefs[j], sndlumps[j], true);
					}
					else
					{
						S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
							playersoundrefs[j], aliasid, true);
					}
				}
			}

			// Make sure face prefix is a full 3 chars
			if (skins[i].face[1] == 0 || skins[i].face[2] == 0)
			{
				skins[i].face[0] = 0;
			}
		}
	}

	if (skins.Size() > PlayerClasses.Size ())
	{ // The sound table may have changed, so rehash it.
		S_HashSounds ();
		S_ShrinkPlayerSoundLists ();
	}
}

// [RH] Find a skin by name
int R_FindSkin (const char *name, int pclass)
{
	if (stricmp ("base", name) == 0)
	{
		return pclass;
	}

	for (unsigned i = PlayerClasses.Size(); i < skins.Size(); i++)
	{
		// [BC] Changed from 16 to MAX_SKIN_NAME.
		if (strnicmp (skins[i].name, name, MAX_SKIN_NAME) == 0)
		{
			if (PlayerClasses[pclass].CheckSkin (i))
				return i;
			else
				return pclass;
		}
	}
	return pclass;
}

// [RH] List the names of all installed skins
CCMD (skins)
{
	int i;
	ULONG	ulNumSkins;
	ULONG	ulNumHiddenSkins;

	ulNumSkins = 0;
	ulNumHiddenSkins = 0;
	for (i = PlayerClasses.Size ()-1; i < (int)skins.Size(); i++)
	{
		if ( skins[i].bRevealed )
		{
			Printf ("% 3d %s\n", static_cast<unsigned int> (ulNumSkins), skins[i].name);
			ulNumSkins++;
		}
		else
			ulNumHiddenSkins++;
	}

	if ( ulNumHiddenSkins == 0 )
		Printf( "\n%d skins; All hidden skins unlocked!\n", (int)skins.Size() );
	else
		Printf( "\n%d skins; %d remain%s hidden.\n", (int)skins.Size(), static_cast<unsigned int> (ulNumHiddenSkins), ulNumHiddenSkins == 1 ? "s" : "" );
}

//*****************************************************************************
//
static void R_CreateSkin()
{
	FPlayerSkin skin;
	memset(&skin, 0, sizeof(FPlayerSkin));

	const PClass *type = PlayerClasses[0].Type;
	skin.range0start = type->Meta.GetMetaInt (APMETA_ColorRange) & 255;
	skin.range0end = type->Meta.GetMetaInt (APMETA_ColorRange) >> 8;
	skin.ScaleX = GetDefaultByType (type)->scaleX;
	skin.ScaleY = GetDefaultByType (type)->scaleY;

	// [BC/BB] We need to initialize the default sprite, because when we create a skin
	// using SKININFO, we don't necessarily specify a sprite.
	skin.sprite = GetDefaultByType (type)->SpawnState->sprite;
	// [BL] Hidden skins
	skin.bRevealed = true;
	skin.bRevealedByDefault = true;

	skins.Push(skin);
}

// [BB] Helper code for the effective skin sprite width/height check.
static bool R_IsCharUsuableAsSpriteRotation ( const char rot )
{
	unsigned rotation = 17;

	if (rot >= '0' && rot <= '9')
		rotation = rot - '0';
	else if (rot >= 'A')
		rotation = rot - 'A' + 10;

	return ( rotation <= 16 );
}

static void R_CreateSkinTranslation (const char *palname)
{
	FMemLump lump = Wads.ReadLump (palname);
	const BYTE *otherPal = (BYTE *)lump.GetMem();
 
	for (int i = 0; i < 256; ++i)
	{
		OtherGameSkinRemap[i] = ColorMatcher.Pick (otherPal[0], otherPal[1], otherPal[2]);
		OtherGameSkinPalette[i] = PalEntry(otherPal[0], otherPal[1], otherPal[2]);
		otherPal += 3;
	}
}


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites ()
{
	// [BB] Zandronum's skin handling doesn't need this 'lump'.
	int /*lump,*/ lastlump;
	unsigned int i, j;

	// [RH] Create a standard translation to map skins between Heretic and Doom
	if (gameinfo.gametype == GAME_DoomChex)
	{
		R_CreateSkinTranslation ("SPALHTIC");
	}
	else
	{
		R_CreateSkinTranslation ("SPALDOOM");
	}

	lastlump = 0;

	SpriteFrames.Clear();

	// [RH] Do some preliminary setup
	skins.Clear(); // [BB] Adapted to Zandronum's skin handling
	for (i = 0; i < PlayerClasses.Size (); i++)
	{
		R_CreateSkin();
	}

	R_InitSpriteDefs ();
	R_InitVoxels();		// [RH] Parse VOXELDEF
	NumStdSprites = sprites.Size();
	R_InitSkins ();		// [RH] Finish loading skin data

	// [RH] Set up base skin
	// [GRB] Each player class has its own base skin
	for (i = 0; i < PlayerClasses.Size (); i++)
	{
		const PClass *basetype = PlayerClasses[i].Type;
		const char *pclassface = basetype->Meta.GetMetaString (APMETA_Face);

		strcpy (skins[i].name, "Base");
		if (pclassface == NULL || strcmp(pclassface, "None") == 0)
		{
			skins[i].face[0] = 'S';
			skins[i].face[1] = 'T';
			skins[i].face[2] = 'F';
			skins[i].face[3] = '\0';
		}
		else
		{
			strcpy(skins[i].face, pclassface);
		}
		skins[i].range0start = basetype->Meta.GetMetaInt (APMETA_ColorRange) & 255;
		skins[i].range0end = basetype->Meta.GetMetaInt (APMETA_ColorRange) >> 8;
		skins[i].ScaleX = GetDefaultByType (basetype)->scaleX;
		skins[i].ScaleY = GetDefaultByType (basetype)->scaleY;
		skins[i].sprite = GetDefaultByType (basetype)->SpawnState->sprite;
		skins[i].namespc = ns_global;

		PlayerClasses[i].Skins.Push (i);

		if (memcmp (sprites[skins[i].sprite].name, "PLAY", 4) == 0)
		{
			for (j = 0; j < sprites.Size (); j++)
			{
				if (memcmp (sprites[j].name, deh.PlayerSprite, 4) == 0)
				{
					skins[i].sprite = (int)j;
					break;
				}
			}
		}
	}

	// [BB] Check if any of the skin sprites are ridiculously big to prevent
	// abusing the possibility to replace the skin sprites.
	for ( unsigned int skinIdx = 0; skinIdx < skins.Size(); skinIdx++ )
	{
		// [BB] If the skin doesn't have a name, it's removed and doesn't need to be checked.
		// Removed skins for example are Doom skins in a Hexen games.
		if ( skins[skinIdx].name[0] == 0 )
			continue;

		int maxwidth = 0, maxheight = 0;
		char	szTempLumpName[9];
		szTempLumpName[8]=0;
		FString maxwidthSprite, maxheightSprite;

		// [BB] Loop through all the lumps searching for sprites of this skin.
		// This may look very inefficient, but since this is only called once on
		// startup it's ok.
		for ( ULONG ulIdx = 0; static_cast<signed> (ulIdx) < Wads.GetNumLumps( ); ulIdx++ )
		{
			Wads.GetLumpName( szTempLumpName, ulIdx );
			if ( ( strnicmp ( szTempLumpName, sprites[skins[skinIdx].sprite].name, 4 ) == 0 )
			     // [BB] Only check lumps that possibly can be used as sprite frames.
			     && ( static_cast<unsigned> ( szTempLumpName[4] - 'A' ) < MAX_SPRITE_FRAMES )
			     // [BB] No need to check Death/XDeath frames.
			     && ( szTempLumpName[4] < 'H' ) )
			{
				// [BB] Check if the lump can be used as sprite. If not, no need to check it.
				if ( R_IsCharUsuableAsSpriteRotation ( szTempLumpName[5] ) == false )
					continue;

				if ( szTempLumpName[6] )
				{
					// [BB] Only check lumps that possibly can be used as sprite frames.
					if ( static_cast<unsigned> ( szTempLumpName[6] - 'A' ) >= MAX_SPRITE_FRAMES )
						continue;

					// [BB] No need to check Death/XDeath frames.
					if ( szTempLumpName[6] >= 'H' )
						continue;

					if ( R_IsCharUsuableAsSpriteRotation ( szTempLumpName[7] ) == false )
						continue;
				}

				FTextureID texnum = TexMan.CheckForTexture (szTempLumpName, FTexture::TEX_Sprite);
				FTexture *tex = (texnum.Exists()) ? TexMan[ texnum ] : NULL;
				if ( tex )
				{
					if ( tex->GetScaledHeight() > maxheight )
					{
						maxheight = tex->GetScaledHeight();
						maxheightSprite = szTempLumpName;
					}
					if ( tex->GetScaledWidth() > maxwidth )
					{
						maxwidth = tex->GetScaledWidth();
						maxwidthSprite = szTempLumpName;
					}
				}
			}
		}

		int classSkinIdx = -1;

		// [BB] Find the player class this skin belongs to.
		if ( !skins[skinIdx].othergame )
		{
			for ( unsigned int pcIdx = 0; pcIdx < PlayerClasses.Size(); pcIdx++ )
			{
				if ( classSkinIdx != -1 )
					break;

				for ( unsigned int pcSkinIdx = 0; pcSkinIdx < PlayerClasses[pcIdx].Skins.Size(); pcSkinIdx++ )
				{
					if ( PlayerClasses[pcIdx].Skins[pcSkinIdx] == static_cast<int> (skinIdx) )
					{
						classSkinIdx = pcIdx;
						break;
					}
				}
			}
			// [BB] The skin doesn't seem to belong to any of the the available player classes, so just check it against the standard player class.
			if ( classSkinIdx == -1 )
				classSkinIdx = 0;
		}
		else
		{
			// [BB] The skin doesn't belong to this game, so just check it against the standard player class.
			classSkinIdx = 0;
		}

		// [TP] How big can the skin be?
		const FMetaTable& meta = PlayerClasses[classSkinIdx].Type->Meta;
		fixed_t maxwidthfactor = meta.GetMetaFixed( APMETA_MaxSkinWidthFactor );
		fixed_t maxheightfactor = meta.GetMetaFixed( APMETA_MaxSkinHeightFactor );

		// [TP] If either of the size factors are 0, we can just skip this.
		if (( maxwidthfactor == 0 ) || ( maxheightfactor == 0 ))
			continue;

		AActor* def = GetDefaultByType( PlayerClasses[classSkinIdx].Type );
		fixed_t maxAllowedHeight = FixedMul( maxheightfactor, def->height );
		// [BB] 2*radius is approximately the actor width.
		fixed_t maxAllowedWidth = FixedMul( maxwidthfactor, 2 * def->radius );
		FPlayerSkin& skin = skins[skinIdx];

		// [BB] If a skin sprite violates the limits, just downsize it.
		bool sizeLimitsExceeded = false;
		// [BB] Compare the maximal sprite height/width to the height/radius of the player class this skin belongs to.
		// Massmouth is very big, so we have to be pretty lenient here with the checks.
		// [TP] Also, allow 1px lee-way so that we won't complain about scale being off by a
		// fraction of a pixel (would cause messages such as "52px, max is 52px").
		if ( maxheight * skin.ScaleY > maxAllowedHeight + FRACUNIT )
		{
			sizeLimitsExceeded = true;
			Printf ( "\\cgSprite %s of skin %s is too tall (%dpx, max is %dpx). Downsizing.\n",
				maxheightSprite.GetChars(),
				skin.name,
				( maxheight * skin.ScaleY ) >> FRACBITS,
				maxAllowedHeight >> FRACBITS );
			const fixed_t oldScaleY = skin.ScaleY;
			skin.ScaleY = maxAllowedHeight / maxheight;
			// [BB] Preserve the aspect ration of the sprites.
			skin.ScaleX = static_cast<fixed_t> ( skin.ScaleX * ( FIXED2FLOAT( skin.ScaleY ) / FIXED2FLOAT( oldScaleY ) ) );
		}

		if ( maxwidth * skin.ScaleX > maxAllowedWidth + FRACUNIT )
		{
			sizeLimitsExceeded = true;
			Printf ( "\\cgSprite %s of skin %s is too wide (%dpx, max is %dpx). Downsizing.\n",
				maxwidthSprite.GetChars(),
				skin.name,
				( maxwidth * skin.ScaleX ) >> FRACBITS,
				( maxAllowedWidth ) >> FRACBITS );
			const fixed_t oldScaleX = skin.ScaleX;
			skin.ScaleX = maxAllowedWidth / maxwidth;
			// [BB] Preserve the aspect ration of the sprites.
			skin.ScaleY = static_cast<fixed_t> ( skin.ScaleY * ( FIXED2FLOAT( skin.ScaleX ) / FIXED2FLOAT( oldScaleX ) ) );
		}

		// [BB] Don't allow the base skin sprites of the player classes to exceed the limits.
		if ( sizeLimitsExceeded && ( skinIdx < PlayerClasses.Size () ) )
		{
			I_FatalError ( "The base skin sprite of player class %s exceeds the limits!\n", PlayerClasses[skinIdx].Type->TypeName.GetChars() );
		}
	}

	// [RH] Sort the skins, but leave base as skin 0
	//qsort (&skins[PlayerClasses.Size ()], skins.Size()-PlayerClasses.Size (), sizeof(FPlayerSkin), skinsorter);

	gl_InitModels();
}

void R_DeinitSpriteData()
{
	// Free skins
	// [BB]
	skins.Clear();
}

// [BC] Allow clients to decide whether or not they want skins enabled.
CUSTOM_CVAR( Int, cl_skins, 1, CVAR_ARCHIVE )
{
	LONG	lSkin;
	ULONG	ulIdx;

	// Loop through all the players and set their sprite according to the value of cl_skins.
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if (( playeringame[ulIdx] == false ) || ( players[ulIdx].mo == NULL ))
			continue;

		// If cl_skins == 0, then the user wishes to disable all skins.
		if ( self <= 0 )
		{
			lSkin = R_FindSkin( "base", players[ulIdx].CurrentPlayerClass );

			// Make sure the player doesn't change sprites when his state changes.
			players[ulIdx].mo->flags4 |= MF4_NOSKIN;
		}
		// If cl_skins >= 2, then the user wants to disable cheat skins, but allow all others.
		else if ( self >= 2 )
		{
			if ( skins[players[ulIdx].userinfo.GetSkin()].bCheat )
			{
				lSkin = R_FindSkin( "base", players[ulIdx].CurrentPlayerClass );

				// Make sure the player doesn't change sprites when his state changes.
				players[ulIdx].mo->flags4 |= MF4_NOSKIN;
			}
			else
			{
				lSkin = players[ulIdx].userinfo.GetSkin();

				if (( players[ulIdx].mo->GetDefault( )->flags4 & MF4_NOSKIN ) == false )
					players[ulIdx].mo->flags4 &= ~MF4_NOSKIN;
			}
		}
		// If cl_skins == 1, allow all skins to be used.
		else
		{
			lSkin = players[ulIdx].userinfo.GetSkin();

			if (( players[ulIdx].mo->GetDefault( )->flags4 & MF4_NOSKIN ) == false )
				players[ulIdx].mo->flags4 &= ~MF4_NOSKIN;
		}

		// If the skin is valid, set the player's sprite to the skin's sprite, and adjust
		// the player's scale accordingly.
		if (( lSkin >= 0 ) && ( static_cast<unsigned> (lSkin) < skins.Size() ))
		{
			players[ulIdx].mo->sprite = skins[lSkin].sprite;
			players[ulIdx].mo->scaleX = skins[lSkin].ScaleX;
			players[ulIdx].mo->scaleY = skins[lSkin].ScaleY;
/*
			// Make sure the player doesn't change sprites when his state changes.
			if ( lSkin == R_FindSkin( "base", players[ulIdx].CurrentPlayerClass ))
				players[ulIdx].mo->flags4 |= MF4_NOSKIN;
			else
			{
				if (( players[ulIdx].mo->GetDefault( )->flags4 & MF4_NOSKIN ) == false )
					players[ulIdx].mo->flags4 &= ~MF4_NOSKIN;
			}
*/
		}
	}
}

