#ifndef __RES_SPRITES_H
#define __RES_SPRITES_H

// [BC] This is the maximum length a skin name can be.
#define	MAX_SKIN_NAME					24

#define MAX_SPRITE_FRAMES	29		// [RH] Macro-ized as in BOOM.

//
// Sprites are patches with a special naming convention so they can be
// recognized by R_InitSprites. The base name is NNNNFx or NNNNFxFx, with
// x indicating the rotation, x = 0, 1-7. The sprite and frame specified
// by a thing_t is range checked at run time.
// A sprite is a patch_t that is assumed to represent a three dimensional
// object and may have multiple rotations pre drawn. Horizontal flipping
// is used to save space, thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used for all views: NNNNF0
//
struct spriteframe_t
{
	struct FVoxelDef *Voxel;// voxel to use for this frame
	FTextureID Texture[16];	// texture to use for view angles 0-15
	WORD Flip;				// flip (1 = flip) to use for view angles 0-15.
};

//
// A sprite definition:
//	a number of animation frames.
//

struct spritedef_t
{
	union
	{
		char name[5];
		DWORD dwName;
	};
	BYTE numframes;
	WORD spriteframes;
};

extern TArray<spriteframe_t> SpriteFrames;


//
// [RH] Internal "skin" definition.
//
class FPlayerSkin
{
public:
	// [BC] Changed to MAX_SKIN_NAME.
	char		name[MAX_SKIN_NAME+1];	// MAX_SKIN_NAME chars + NULL
	char		face[4];	// 3 chars ([MH] + NULL so can use as a C string)
	BYTE		gender;		// This skin's gender (not really used)
	BYTE		range0start;
	BYTE		range0end;
	bool		othergame;	// [GRB]
	fixed_t		ScaleX;
	fixed_t		ScaleY;
	int			sprite;
	int			crouchsprite;
	int			namespc;	// namespace for this skin

	// [BC] New skin properties for Skulltag.
	// Default color used for this skin.
	char		szColor[16];
	// Can this skin be selected from the menu?
	bool		bRevealed;

	// Is this skin hidden by default?
	bool		bRevealedByDefault;

	// Is this skin a cheat skin?
	bool		bCheat;
	// [BC] End of new skin properties.
};

// [BL] Use a TArray instead of trying to manage this manually
extern TArray<FPlayerSkin> skins;		// [RH]

extern BYTE				OtherGameSkinRemap[256];
extern PalEntry			OtherGameSkinPalette[256];

void R_InitSprites ();
void R_DeinitSpriteData ();

#endif
