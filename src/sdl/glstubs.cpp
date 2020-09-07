#include "r_defs.h"
#include "r_state.h"
#include "v_video.h"

#include "gl/textures/gl_texture.h"

#ifdef NO_GL
// [BB] Having to declare dummy versions of all the dynamic lights is pretty awful...
class ADynamicLight : public AActor
{
	DECLARE_CLASS (ADynamicLight, AActor)
};

class AVavoomLight : public ADynamicLight
{
   DECLARE_CLASS (AVavoomLight, ADynamicLight)
};

class AVavoomLightWhite : public AVavoomLight
{
   DECLARE_CLASS (AVavoomLightWhite, AVavoomLight)
};

class AVavoomLightColor : public AVavoomLight
{
   DECLARE_CLASS (AVavoomLightColor, AVavoomLight)
};

IMPLEMENT_CLASS (ADynamicLight)
IMPLEMENT_CLASS (AVavoomLight)
IMPLEMENT_CLASS (AVavoomLightWhite)
IMPLEMENT_CLASS (AVavoomLightColor)

DEFINE_CLASS_PROPERTY(type, S, DynamicLight)
{
	PROP_STRING_PARM(str, 0);
}

CVAR (Float, vid_brightness, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, vid_contrast, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

void FTexture::UncacheGL()
{
}

void gl_CleanLevelData()
{
}

void gl_PreprocessLevel()
{
}

void gl_ParseDefs()
{
}

void gl_InitModels()
{
}

void StartGLMenu (void)
{
}

void FTexture::PrecacheGL()
{
}

FTexture::MiscGLInfo::MiscGLInfo() throw ()
{
}

FTexture::MiscGLInfo::~MiscGLInfo()
{
}

void AddStateLight(FState *, const char *)
{
}

size_t AActor::PropagateMark()
{
	return Super::PropagateMark();
}

#endif
