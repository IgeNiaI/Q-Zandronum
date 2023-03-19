/*
** freeformmenuitems.h
** Control items for freeform menus
**
**---------------------------------------------------------------------------
** Copyright 2023 Evghenii Olenciuc
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

#include "gstrings.h"


void M_DrawConText (int color, int x, int y, const char *str);

//=============================================================================
//
// Draws text and images
//
//=============================================================================

class FFreeformMenuItemLabel : public FFreeformMenuItem
{
protected:
	const char *mLabel;
	FFont* mFont;
	int mMaxLabelWidth;
	EColorRange mFontColor;
	FTextureID mBackground;
public:
	FFreeformMenuItemLabel() : FFreeformMenuItem(NAME_None)
	{
		mLabel = NULL;
		mFont = SmallFont;
		mMaxLabelWidth = 0;
		mFontColor = OptionSettings.mFontColorMore;
		mBackground = FNullTextureID();
	}

	~FFreeformMenuItemLabel() {};

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetLabel(const char *label) {
		mLabel = copystring(label);
	}
	void SetFont(FFont* font) { if (font != NULL) mFont = font; }
	void SetFontColor(EColorRange fontColor) { mFontColor = fontColor; }
	void SetMaxLabelWidth(int width) { mMaxLabelWidth = width; }
	void SetBackground(FTextureID texture) { mBackground = texture; }

	void Draw(FFreeformMenuDescriptor* desc, int yoffset, bool selected);
	void CalcTextClip(int &left, int &right, int &top, int &bottom, int width, int height, int yoffset);
};

//=============================================================================
//
// Base class for actionable items
//
//=============================================================================

class FFreeformMenuItemActionableBase : public FFreeformMenuItemLabel
{
protected:
	EColorRange mSelectedFontColor;
	FTextureID mSelectedBackground;
	int mForegroundWidth, mForegroundHeight;
	FTextureID mSelectedForeground;
	FBaseCVar* mGrayCheck;
	int mGrayCheckMin, mGrayCheckMax;
	bool mSelectable;
public:
	FFreeformMenuItemActionableBase() : FFreeformMenuItemLabel()
	{
		mSelectedFontColor = OptionSettings.mFontColorSelection;
		mSelectedBackground = FNullTextureID();
		mForegroundWidth = 0;
		mForegroundHeight = 0;
		mSelectedForeground = FNullTextureID();
		mGrayCheck = NULL;
		mGrayCheckMin = 1;
		mGrayCheckMax = INT_MAX;
		mSelectable = true;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetSelectedFontColor(EColorRange fontColor) { mSelectedFontColor = fontColor; }
	void SetSelectedBackground(FTextureID texture) { mSelectedBackground = texture; }
	void SetSelectedForeground(FTextureID texture, int width, int height)
	{ 
		mSelectedForeground = texture;
		mForegroundWidth = width;
		mForegroundHeight = height;
	}
	void SetGrayCheck(const char* graycheck, int min, int max)
	{
		mGrayCheck = FindCVar(graycheck, NULL);
		mGrayCheckMin = min;
		mGrayCheckMax = max;
		if (mGrayCheckMax < mGrayCheckMin)
			swapvalues(mGrayCheckMin, mGrayCheckMax);
	}
	void SetSelectable(bool selectable) { mSelectable = selectable; }

	void Draw(FFreeformMenuDescriptor* desc, int yoffset, bool selected);
	bool Selectable();
	bool IsEnabled();
};

//=============================================================================
//
// Opens a menu when activated
//
//=============================================================================

class FFreeformMenuItemSubmenu : public FFreeformMenuItemActionableBase
{
protected:
	int mParam;
public:
	FFreeformMenuItemSubmenu() : FFreeformMenuItemActionableBase()
	{
		mParam = 0;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetParam(int param) { mParam = param; }

	bool Activate();

};

//=============================================================================
//
// Executes a CCMD when activated
//
//=============================================================================

class FFreeformMenuItemCommand : public FFreeformMenuItemActionableBase
{
	bool mIsSafe;
public:
	FFreeformMenuItemCommand() : FFreeformMenuItemActionableBase()
	{
		mIsSafe = false;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	bool MenuEvent(int mkey, bool fromcontroller);
	bool Activate();

	void SetSafe(bool safe) { mIsSafe = safe; }
};


//=============================================================================
//
// Base class for option lists
//
//=============================================================================

class FFreeformMenuItemOptionBase : public FFreeformMenuItemActionableBase
{
protected:
	// action is a CVAR
	FName mValues;	// Entry in OptionValues table
	const char *mUnknownValueText;
	FName mBackgroundValues;	// Entry in OptionValues table
	FTextureID mUnknownBackgroundTexture;
public:

	enum
	{
		OP_VALUES = 0x11001
	};

	FFreeformMenuItemOptionBase() : FFreeformMenuItemActionableBase()
	{
		mValues = NAME_None;
		mUnknownValueText = "Unknown";
		mBackgroundValues = NAME_None;
		mUnknownBackgroundTexture = FNullTextureID();
	}

	~FFreeformMenuItemOptionBase() {};

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetValues(FName values) { mValues = values; }
	void SetUnknownValueText(const char *text) {
		mUnknownValueText = copystring(text);
	}
	void SetBackgroundValues(FName values) { mBackgroundValues = values; }
	void SetUnknownBackgroundTexture(FTextureID texture) { mUnknownBackgroundTexture = texture; }

	void Draw(FFreeformMenuDescriptor* desc, int yoffset, bool selected);
	bool SetString(int i, const char* newtext);
	bool MenuEvent(int mkey, bool fromcontroller);

	//=============================================================================
	virtual int GetSelection() = 0;
	virtual void SetSelection(int Selection) = 0;
};


//=============================================================================
//
// Change a CVAR, action is the CVAR name
//
//=============================================================================

class FFreeformMenuItemOption : public FFreeformMenuItemOptionBase
{
	// action is a CVAR
	FBaseCVar *mCVar;
public:

	FFreeformMenuItemOption() : FFreeformMenuItemOptionBase()
	{
		mCVar = NULL;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	bool IsOptionSelectable(double);

	bool IsServerInfo() const
	{
		return mCVar && mCVar->IsServerInfo();
	}

	//=============================================================================
	int GetSelection();
	void SetSelection(int Selection);
	void SetCVar(FName cvar) { mCVar = FindCVar(cvar, NULL); mAction = cvar; } // Need to set mAction for AddFreeformMenu to work

};

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class FFreeformMenuItemControl : public FFreeformMenuItemActionableBase
{
	FKeyBindings *mBindings;
	int mInput;
	bool mWaiting;
public:

	FFreeformMenuItemControl() : FFreeformMenuItemActionableBase()
	{
		mBindings = NULL;
		mInput = 0;
		mWaiting = false;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetBindings(FKeyBindings *bindings) { mBindings = bindings; }

	void Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected);
	bool MenuEvent(int mkey, bool fromcontroller);
	bool Activate();
};

//=============================================================================
//
// // Edit a color, Action is the CCMD to bind
//
//=============================================================================

class FFreeformMenuItemColorPicker : public FFreeformMenuItemActionableBase
{
	FColorCVar *mCVar;
public:

	enum
	{
		CPF_RESET = 0x20001,
	};

	FFreeformMenuItemColorPicker() : FFreeformMenuItemActionableBase()
	{
		mCVar = NULL;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);
	
	void SetCVar(FName cvar)
	{ 
		FBaseCVar *cv = FindCVar(cvar, NULL);
		if (cv != NULL && cv->GetRealType() == CVAR_Color)
		{
			mCVar = (FColorCVar*)cv;
		}
		else mCVar = NULL;
		mAction = cvar; // Need to set mAction for AddFreeformMenu to work
	}

	//=============================================================================
	void Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected);
	bool SetValue(int i, int v);
	bool Activate();

	bool IsServerInfo() const
	{
		return mCVar && mCVar->IsServerInfo();
	}
};

//=============================================================================
//
// Base class for input fields
//
//=============================================================================

class FFreeformMenuFieldBase : public FFreeformMenuItemActionableBase
{
protected:
	// Action is a CVar in this class and derivatives.
	FBaseCVar* mCVar;
public:
	FFreeformMenuFieldBase() : FFreeformMenuItemActionableBase() {
		mCVar = NULL;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetCVar(FName cvar) { mCVar = FindCVar(cvar, NULL); mAction = cvar; } // Need to set mAction for AddFreeformMenu to work

	const char* GetCVarString();
	virtual FString Represent();
	void Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected);

	//=============================================================================
	bool GetString(int i, char* s, int len);
	bool SetString(int i, const char* s);

	bool IsServerInfo() const
	{
		return mCVar && mCVar->IsServerInfo();
	}

};

//=============================================================================
//
// A text input field widget, for use with string CVars.
//
//=============================================================================

class FFreeformMenuTextField : public FFreeformMenuFieldBase
{
	bool mEntering;
	char mEditName[128];
public:
	FFreeformMenuTextField() : FFreeformMenuFieldBase() {
		mEntering = false;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	FString Represent();
	bool MenuEvent(int mkey, bool fromcontroller);
};

//=============================================================================
//
// A numeric input field widget, for use with number CVars where sliders are inappropriate (i.e.
// where the user is interested in the exact value specifically)
//
//=============================================================================

class FFreeformMenuNumberField : public FFreeformMenuFieldBase
{
	float mMinimum;
	float mMaximum;
	float mStep;
public:
	FFreeformMenuNumberField() : FFreeformMenuFieldBase()
	{
		mMinimum = 0;
		mMaximum = 100;
		mStep = 1;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetRange(float min, float max, float step)
	{
		mMinimum = min;
		mMaximum = max;
		mStep = step;

		if (mMaximum <= mMinimum)
			swapvalues(mMinimum, mMaximum);

		if (mStep <= 0)
			mStep = 1;
	}
	bool MenuEvent(int mkey, bool fromcontroller);
};

//=============================================================================
//
// Base class for more specialized option fields.
//
//=============================================================================

class FFreeformMenuCustomOptionField : public FFreeformMenuFieldBase
{
public:
	FFreeformMenuCustomOptionField() : FFreeformMenuFieldBase ()
	{
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	virtual bool IsValid ( int ) = 0;
	virtual FString RepresentOption ( int ) = 0;
	virtual int Maximum() = 0;

	// [TP] Seeks the value forwards or backwards. Returns true if it was possible to find a new
	// player. Can return false if there are no players (not in a game)
	bool Seek ( bool forward );
	bool MenuEvent ( int mkey, bool fromcontroller );
	FString Represent();
};

//=============================================================================
//
// Selects a player. Cannot be a derivative of the Option widgets because it
// needs to be able to skip non-existing players.
//
//=============================================================================

class FFreeformMenuPlayerField : public FFreeformMenuCustomOptionField
{
	bool mAllowBots;
	bool mAllowSelf;

public:
	FFreeformMenuPlayerField () : FFreeformMenuCustomOptionField ()
	{
		mAllowBots = false;
		mAllowSelf = false;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetAllowBots(bool allow) { mAllowBots = allow; }
	void SetAllowSelf(bool allow) { mAllowSelf = allow; }

	bool IsValid ( int player );
	FString RepresentOption ( int playerId );
	int Maximum();
};

//=============================================================================
//
// Selects a team (similar reasoning as with FOptionMenuPlayerField)
//
//=============================================================================

class FFreeformMenuTeamField : public FFreeformMenuCustomOptionField
{
public:
	FFreeformMenuTeamField() : FFreeformMenuCustomOptionField()
	{
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	bool IsValid ( int teamId );
	FString RepresentOption ( int teamId );
	int Maximum();
};

//=============================================================================
//
// Selects a player class.
//
//=============================================================================

EXTERN_CVAR( Int, menu_jointeamidx )

class FFreeformMenuPlayerClassField : public FFreeformMenuCustomOptionField
{
public:
	FFreeformMenuPlayerClassField() : FFreeformMenuCustomOptionField()
	{
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	bool IsValid ( int classId );
	int Maximum();
	FString RepresentOption ( int classId );
};

//=============================================================================
//
// Allows to slide a value
//
//=============================================================================

class FFreeformMenuSliderBase : public FFreeformMenuItem
{
	float mMinimum;
	float mMaximum;
	float mStep;
	bool mSelectable;
	FString mSliderText;
	FTextureID mBackground;
	int mSliderWidth, mSliderHeight;
	FTextureID mSlider;
	FTextureID mSliderSelected;
	FBaseCVar* mGrayCheck;
	int mGrayCheckMin, mGrayCheckMax;

public:
	FFreeformMenuSliderBase() : FFreeformMenuItem(NAME_None)
	{
		mMinimum = 0;
		mMaximum = 100;
		mStep = 1;
		mSelectable = true;
		mSliderText = "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12";

		// width and height for ConFont based slider
		mWidth = ConFont->StringWidth(mSliderText);
		mHeight = ConFont->GetHeight();
		mBackground = FNullTextureID();
		mSliderWidth = 0;
		mSliderHeight = 0;
		mSlider = FNullTextureID();
		mSliderSelected = FNullTextureID();
		
		mGrayCheck = NULL;
		mGrayCheckMin = 1;
		mGrayCheckMax = INT_MAX;
	}

	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);
	
	void SetGrayCheck(const char* graycheck, int min, int max)
	{
		mGrayCheck = FindCVar(graycheck, NULL);
		mGrayCheckMin = min;
		mGrayCheckMax = max;
		if (mGrayCheckMax < mGrayCheckMin)
			swapvalues(mGrayCheckMin, mGrayCheckMax);
	}
	void SetRange(float min, float max, float step)
	{
		mMinimum = min;
		mMaximum = max;
		mStep = step;

		if (mMaximum <= mMinimum)
			swapvalues(mMinimum, mMaximum);

		if (mStep <= 0)
			mStep = 1;
	}
	void SetTextures(FTextureID texture, FTextureID slider, FTextureID sliderSelected, int width, int height)
	{ 
		mBackground = texture;
		mSlider = slider;
		mSliderSelected = sliderSelected;
		mSliderWidth = width;
		mSliderHeight = height;
	}
	void SetSelectable(bool selectable) { mSelectable = selectable; }
	bool Selectable();
	bool IsEnabled();
	virtual float GetSliderValue() = 0;
	virtual void SetSliderValue(float val) = 0;

	enum { SLIDER_MAXIMUM = 0x40001 };
	
	void Draw(FFreeformMenuDescriptor* desc, int yoffset, bool selected);
	bool MenuEvent (int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	bool SetValue( int i, int value );
};

//=============================================================================
//
//
//
//=============================================================================

class FFreeformMenuSliderCVar : public FFreeformMenuSliderBase
{
	FBaseCVar *mCVar;
public:
	FFreeformMenuSliderCVar() : FFreeformMenuSliderBase()
	{
		mCVar = NULL;
	}
	
	virtual FFreeformMenuItem* Clone();
	virtual bool CopyTo(FFreeformMenuItem* other);

	void SetCVar(FName cvar) { mCVar = FindCVar(cvar, NULL); mAction = cvar; } // Need to set mAction for AddFreeformMenu to work

	float GetSliderValue();
	void SetSliderValue(float val);
	bool IsServerInfo() const;
};
