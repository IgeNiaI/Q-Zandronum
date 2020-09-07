/*
** optionmenuitems.h
** Control items for option menus
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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


void M_DrawConText (int color, int x, int y, const char *str);
void M_SetVideoMode();



//=============================================================================
//
// opens a submenu, action is a submenu name
//
//=============================================================================

class FOptionMenuItemSubmenu : public FOptionMenuItem
{
	int mParam;
public:
	FOptionMenuItemSubmenu(const char *label, const char *menu, int param = 0)
		: FOptionMenuItem(label, menu)
	{
		mParam = param;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColorMore);
		return indent;
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		M_SetMenu(mAction, mParam);
		return true;
	}
};


//=============================================================================
//
// Executes a CCMD, action is a CCMD name
//
//=============================================================================

class FOptionMenuItemCommand : public FOptionMenuItemSubmenu
{
public:
	FOptionMenuItemCommand(const char *label, const char *menu)
		: FOptionMenuItemSubmenu(label, menu)
	{
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		C_DoCommand(mAction);
		return true;
	}

};

//=============================================================================
//
// Executes a CCMD after confirmation, action is a CCMD name
//
//=============================================================================

class FOptionMenuItemSafeCommand : public FOptionMenuItemCommand
{
	// action is a CCMD
public:
	FOptionMenuItemSafeCommand(const char *label, const char *menu)
		: FOptionMenuItemCommand(label, menu)
	{
	}

	bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_MBYes)
		{
			C_DoCommand(mAction);
			return true;
		}
		return FOptionMenuItemCommand::MenuEvent(mkey, fromcontroller);
	}

	bool Activate()
	{
		M_StartMessage("Do you really want to do this?", 0);
		return true;
	}
};

//=============================================================================
//
// Base class for option lists
//
//=============================================================================

class FOptionMenuItemOptionBase : public FOptionMenuItem
{
protected:
	// action is a CVAR
	FName mValues;	// Entry in OptionValues table
	FBaseCVar *mGrayCheck;
	int mCenter;
public:

	enum
	{
		OP_VALUES = 0x11001
	};

	FOptionMenuItemOptionBase(const char *label, const char *menu, const char *values, const char *graycheck, int center)
		: FOptionMenuItem(label, menu)
	{
		mValues = values;
		mGrayCheck = (FBoolCVar*)FindCVar(graycheck, NULL);
		mCenter = center;
	}

	bool SetString(int i, const char *newtext)
	{
		if (i == OP_VALUES) 
		{
			FOptionValues **opt = OptionValues.CheckKey(newtext);
			mValues = newtext;
			if (opt != NULL && *opt != NULL) 
			{
				int s = GetSelection();
				if (s >= (int)(*opt)->mValues.Size()) s = 0;
				SetSelection(s);	// readjust the CVAR if its value is outside the range now
				return true;
			}
		}
		return false;
	}



	//=============================================================================
	virtual int GetSelection() = 0;
	virtual void SetSelection(int Selection) = 0;

	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		bool grayed = mGrayCheck != NULL && !(mGrayCheck->GetGenericRep(CVAR_Bool).Bool);

		// [TP] Also gray it if we cannot edit this cvar for some other reason.
		if ( IsDisabled() )
			grayed = true;

		if (mCenter)
		{
			indent = (screen->GetWidth() / 2);
		}
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor, grayed);

		int overlay = grayed? MAKEARGB(96,48,0,0) : 0;
		const char *text;
		int Selection = GetSelection();
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (Selection < 0 || opt == NULL || *opt == NULL)
		{
			text = "Unknown";
		}
		else
		{
			text = (*opt)->mValues[Selection].Text;
		}
		screen->DrawText (SmallFont, OptionSettings.mFontColorValue, indent + CURSORSPACE, y, 
			text, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
		return indent;
	}

	//=============================================================================
	bool MenuEvent (int mkey, bool fromcontroller)
	{
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (opt != NULL && *opt != NULL && (*opt)->mValues.Size() > 0)
		{
			int Selection = GetSelection();
			if (mkey == MKEY_Left)
			{
				if (Selection == -1) Selection = 0;
				else if (--Selection < 0) Selection = (*opt)->mValues.Size()-1;
			}
			else if (mkey == MKEY_Right || mkey == MKEY_Enter)
			{
				if (++Selection >= (int)(*opt)->mValues.Size()) Selection = 0;
			}
			else
			{
				return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
			}
			SetSelection(Selection);
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
		}
		return true;
	}

	bool Selectable()
	{
		// [TP] We also need to check the base class's Selectable
		return !(mGrayCheck != NULL && !(mGrayCheck->GetGenericRep(CVAR_Bool).Bool))
			&& FOptionMenuItem::Selectable();
	}
};

//=============================================================================
//
// Change a CVAR, action is the CVAR name
//
//=============================================================================

class FOptionMenuItemOption : public FOptionMenuItemOptionBase
{
	// action is a CVAR
	FBaseCVar *mCVar;
public:

	FOptionMenuItemOption(const char *label, const char *menu, const char *values, const char *graycheck, int center)
		: FOptionMenuItemOptionBase(label, menu, values, graycheck, center)
	{
		mCVar = FindCVar(mAction, NULL);
	}

	// [TP] Can the give item be selected?
	virtual bool IsOptionSelectable ( double )
	{
		return true;
	}

	//=============================================================================
	int GetSelection()
	{
		int Selection = -1;
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (opt != NULL && *opt != NULL && mCVar != NULL && (*opt)->mValues.Size() > 0)
		{
			if ((*opt)->mValues[0].TextValue.IsEmpty())
			{
				UCVarValue cv = mCVar->GetGenericRep(CVAR_Float);
				for(unsigned i = 0; i < (*opt)->mValues.Size(); i++)
				{ 
					// [TP]
					if ( IsOptionSelectable(( *opt )->mValues[i].Value ) == false )
						continue;

					if (fabs(cv.Float - (*opt)->mValues[i].Value) < FLT_EPSILON)
					{
						Selection = i;
						break;
					}
				}
			}
			else
			{
				UCVarValue cv = mCVar->GetGenericRep(CVAR_String);
				for(unsigned i = 0; i < (*opt)->mValues.Size(); i++)
				{
					if ((*opt)->mValues[i].TextValue.CompareNoCase(cv.String) == 0)
					{
						Selection = i;
						break;
					}
				}
			}
		}
		return Selection;
	}

	void SetSelection(int Selection)
	{
		UCVarValue value;
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (opt != NULL && *opt != NULL && mCVar != NULL && (*opt)->mValues.Size() > 0)
		{
			if ((*opt)->mValues[0].TextValue.IsEmpty())
			{
				value.Float = (float)(*opt)->mValues[Selection].Value;
				mCVar->SetGenericRep (value, CVAR_Float);
			}
			else
			{
				value.String = (*opt)->mValues[Selection].TextValue.LockBuffer();
				mCVar->SetGenericRep (value, CVAR_String);
				(*opt)->mValues[Selection].TextValue.UnlockBuffer();
			}
		}
	}

	// [TP]
	bool IsServerInfo() const
	{
		return mCVar && mCVar->IsServerInfo();
	}
};

//=============================================================================
//
// This class is used to capture the key to be used as the new key binding
// for a control item
//
//=============================================================================

class DEnterKey : public DMenu
{
	DECLARE_CLASS(DEnterKey, DMenu)

	int *pKey;

public:
	DEnterKey(DMenu *parent, int *keyptr)
	: DMenu(parent)
	{
		pKey = keyptr;
		SetMenuMessage(1);
		menuactive = MENU_WaitKey;	// There should be a better way to disable GUI capture...
	}

	bool TranslateKeyboardEvents()
	{
		return false; 
	}

	void SetMenuMessage(int which)
	{
		if (mParentMenu->IsKindOf(RUNTIME_CLASS(DOptionMenu)))
		{
			DOptionMenu *m = barrier_cast<DOptionMenu*>(mParentMenu);
			FListMenuItem *it = m->GetItem(NAME_Controlmessage);
			if (it != NULL)
			{
				it->SetValue(0, which);
			}
		}
	}

	bool Responder(event_t *ev)
	{
		if (ev->type == EV_KeyDown)
		{
			*pKey = ev->data1;
			menuactive = MENU_On;
			SetMenuMessage(0);
			Close();
			mParentMenu->MenuEvent((ev->data1 == KEY_ESCAPE)? MKEY_Abort : MKEY_Input, 0);
			return true;
		}
		return false;
	}

	void Drawer()
	{
		mParentMenu->Drawer();
	}
};

#ifndef NO_IMP
IMPLEMENT_ABSTRACT_CLASS(DEnterKey)
#endif

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class FOptionMenuItemControl : public FOptionMenuItem
{
	FKeyBindings *mBindings;
	int mInput;
	bool mWaiting;
public:

	FOptionMenuItemControl(const char *label, const char *menu, FKeyBindings *bindings)
		: FOptionMenuItem(label, menu)
	{
		mBindings = bindings;
		mWaiting = false;
	}


	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, mWaiting? OptionSettings.mFontColorHighlight: 
			(selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor));

		char description[64];
		int Key1, Key2;

		mBindings->GetKeysForCommand(mAction, &Key1, &Key2);
		C_NameKeys (description, Key1, Key2);
		if (description[0])
		{
			M_DrawConText(CR_WHITE, indent + CURSORSPACE, y + (OptionSettings.mLinespacing-8)*CleanYfac_1, description);
		}
		else
		{
			screen->DrawText(SmallFont, CR_BLACK, indent + CURSORSPACE, y + (OptionSettings.mLinespacing-8)*CleanYfac_1, "---",
				DTA_CleanNoMove_1, true, TAG_DONE);
		}
		return indent;
	}

	//=============================================================================
	bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_Input)
		{
			mWaiting = false;
			mBindings->SetBind(mInput, mAction);
			return true;
		}
		else if (mkey == MKEY_Clear)
		{
			mBindings->UnbindACommand(mAction);
			return true;
		}
		else if (mkey == MKEY_Abort)
		{
			mWaiting = false;
			return true;
		}
		return false;
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		mWaiting = true;
		DMenu *input = new DEnterKey(DMenu::CurrentMenu, &mInput);
		M_ActivateMenu(input);
		return true;
	}
};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuItemStaticText : public FOptionMenuItem
{
	EColorRange mColor;
public:
	FOptionMenuItemStaticText(const char *label, bool header)
		: FOptionMenuItem(label, NAME_None, true)
	{
		mColor = header? OptionSettings.mFontColorHeader : OptionSettings.mFontColor;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, mColor);
		return -1;
	}

	bool Selectable()
	{
		return false;
	}

};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuItemStaticTextSwitchable : public FOptionMenuItem
{
	EColorRange mColor;
	FString mAltText;
	int mCurrent;

public:
	FOptionMenuItemStaticTextSwitchable(const char *label, const char *label2, FName action, bool header)
		: FOptionMenuItem(label, action, true)
	{
		mColor = header? OptionSettings.mFontColorHeader : OptionSettings.mFontColor;
		mAltText = label2;
		mCurrent = 0;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		const char *txt = mCurrent? (const char*)mAltText : mLabel;
		int w = SmallFont->StringWidth(txt) * CleanXfac_1;
		int x = (screen->GetWidth() - w) / 2;
		screen->DrawText (SmallFont, mColor, x, y, txt, DTA_CleanNoMove_1, true, TAG_DONE);
		return -1;
	}

	bool SetValue(int i, int val)
	{
		if (i == 0) 
		{
			mCurrent = val;
			return true;
		}
		return false;
	}

	bool SetString(int i, const char *newtext)
	{
		if (i == 0) 
		{
			mAltText = newtext;
			return true;
		}
		return false;
	}

	bool Selectable()
	{
		return false;
	}
};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuSliderBase : public FOptionMenuItem
{
	// action is a CVAR
	double mMin, mMax, mStep;
	int mShowValue;
	int mDrawX;
	int mSliderShort;

public:
	// [TP] Now takes menu as well
	FOptionMenuSliderBase(const char *label, double min, double max, double step, int showval, FName menu = NAME_None)
		: FOptionMenuItem(label, menu)
	{
		mMin = min;
		mMax = max;
		mStep = step;
		mShowValue = showval;
		mDrawX = 0;
		mSliderShort = 0;
	}

	virtual double GetSliderValue() = 0;
	virtual void SetSliderValue(double val) = 0;

	//=============================================================================
	//
	// Draw a slider. Set fracdigits negative to not display the current value numerically.
	//
	//=============================================================================

	void DrawSlider (int x, int y, double min, double max, double cur, int fracdigits, int indent)
	{
		char textbuf[16];
		double range;
		int maxlen = 0;
		int right = x + (12*8 + 4) * CleanXfac_1;
		int cy = y + (OptionSettings.mLinespacing-8)*CleanYfac_1;

		range = max - min;
		double ccur = clamp(cur, min, max) - min;

		if (fracdigits >= 0)
		{
			mysnprintf(textbuf, countof(textbuf), "%.*f", fracdigits, max);
			maxlen = SmallFont->StringWidth(textbuf) * CleanXfac_1;
		}

		mSliderShort = right + maxlen > screen->GetWidth();

		if (!mSliderShort)
		{
			M_DrawConText(CR_WHITE, x, cy, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12");
			M_DrawConText(CR_ORANGE, x + int((5 + ((ccur * 78) / range)) * CleanXfac_1), cy, "\x13");
		}
		else
		{
			// On 320x200 we need a shorter slider
			M_DrawConText(CR_WHITE, x, cy, "\x10\x11\x11\x11\x11\x11\x12");
			M_DrawConText(CR_ORANGE, x + int((5 + ((ccur * 38) / range)) * CleanXfac_1), cy, "\x13");
			right -= 5*8*CleanXfac_1;
		}

		if (fracdigits >= 0 && right + maxlen <= screen->GetWidth())
		{
			mysnprintf(textbuf, countof(textbuf), "%.*f", fracdigits, cur);
			screen->DrawText(SmallFont, CR_DARKGRAY, right, y, textbuf, DTA_CleanNoMove_1, true, TAG_DONE);
		}
	}


	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		// [TP] Support for graying
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor, Selectable() == false );
		mDrawX = indent + CURSORSPACE;
		DrawSlider (mDrawX, y, mMin, mMax, GetSliderValue(), mShowValue, indent);
		return indent;
	}

	//=============================================================================
	bool MenuEvent (int mkey, bool fromcontroller)
	{
		double value = GetSliderValue();

		if (mkey == MKEY_Left)
		{
			value -= mStep;
		}
		else if (mkey == MKEY_Right)
		{
			value += mStep;
		}
		else
		{
			return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
		}
		SetSliderValue(clamp(value, mMin, mMax));
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
		return true;
	}

	bool MouseEvent(int type, int x, int y)
	{
		DOptionMenu *lm = static_cast<DOptionMenu*>(DMenu::CurrentMenu);
		if (type != DMenu::MOUSE_Click)
		{
			if (!lm->CheckFocus(this)) return false;
		}
		if (type == DMenu::MOUSE_Release)
		{
			lm->ReleaseFocus();
		}

		int slide_left = mDrawX+8*CleanXfac_1;
		int slide_right = slide_left + (10*8*CleanXfac_1 >> mSliderShort);	// 12 char cells with 8 pixels each.

		if (type == DMenu::MOUSE_Click)
		{
			if (x < slide_left || x >= slide_right) return true;
		}

		x = clamp(x, slide_left, slide_right);
		double v = mMin + ((x - slide_left) * (mMax - mMin)) / (slide_right - slide_left);
		if (v != GetSliderValue())
		{
			SetSliderValue(v);
			//S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
		}
		if (type == DMenu::MOUSE_Click)
		{
			lm->SetFocus(this);
		}
		return true;
	}

	// [TP]
	enum { SLIDER_MAXIMUM = 0x40001 };
	bool SetValue( int i, int value )
	{
		if ( i == SLIDER_MAXIMUM )
		{
			mMax = value;
			return true;
		}
		else
		{
			return false;
		}
	}
};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuSliderCVar : public FOptionMenuSliderBase
{
	FBaseCVar *mCVar;
public:
	FOptionMenuSliderCVar(const char *label, const char *menu, double min, double max, double step, int showval)
		: FOptionMenuSliderBase(label, min, max, step, showval, menu) // [TP] Added menu parameter
	{
		mCVar = FindCVar(menu, NULL);
	}

	double GetSliderValue()
	{
		if (mCVar != NULL)
		{
			return mCVar->GetGenericRep(CVAR_Float).Float;
		}
		else
		{
			return 0;
		}
	}

	void SetSliderValue(double val)
	{
		if (mCVar != NULL)
		{
			UCVarValue value;
			value.Float = (float)val;
			mCVar->SetGenericRep(value, CVAR_Float);
		}
	}

	// [TP]
	bool IsServerInfo() const
	{
		return mCVar && mCVar->IsServerInfo();
	}
};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuSliderVar : public FOptionMenuSliderBase
{
	float *mPVal;
public:

	FOptionMenuSliderVar(const char *label, float *pVal, double min, double max, double step, int showval)
		: FOptionMenuSliderBase(label, min, max, step, showval)
	{
		mPVal = pVal;
	}

	double GetSliderValue()
	{
		return *mPVal;
	}

	void SetSliderValue(double val)
	{
		*mPVal = (float)val;
	}
};

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class FOptionMenuItemColorPicker : public FOptionMenuItem
{
	FColorCVar *mCVar;
public:

	enum
	{
		CPF_RESET = 0x20001,
	};

	FOptionMenuItemColorPicker(const char *label, const char *menu)
		: FOptionMenuItem(label, menu)
	{
		FBaseCVar *cv = FindCVar(menu, NULL);
		if (cv != NULL && cv->GetRealType() == CVAR_Color)
		{
			mCVar = (FColorCVar*)cv;
		}
		else mCVar = NULL;
	}

	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor);

		if (mCVar != NULL)
		{
			int box_x = indent + CURSORSPACE;
			int box_y = y + CleanYfac_1;
			screen->Clear (box_x, box_y, box_x + 32*CleanXfac_1, box_y + OptionSettings.mLinespacing*CleanYfac_1,
				-1, (uint32)*mCVar | 0xff000000);
		}
		return indent;
	}

	bool SetValue(int i, int v)
	{
		if (i == CPF_RESET && mCVar != NULL)
		{
			mCVar->ResetToDefault();
			return true;
		}
		return false;
	}

	bool Activate()
	{
		if (mCVar != NULL)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
			DMenu *picker = StartPickerMenu(DMenu::CurrentMenu, mLabel, mCVar);
			if (picker != NULL)
			{
				M_ActivateMenu(picker);
				return true;
			}
		}
		return false;
	}

	// [TP]
	bool IsServerInfo() const
	{
		return mCVar && mCVar->IsServerInfo();
	}
};

class FOptionMenuScreenResolutionLine : public FOptionMenuItem
{
	FString mResTexts[3];
	int mSelection;
	int mHighlight;
	int mMaxValid;
public:

	enum
	{
		SRL_INDEX = 0x30000,
		SRL_SELECTION = 0x30003,
		SRL_HIGHLIGHT = 0x30004,
	};

	FOptionMenuScreenResolutionLine(const char *action)
		: FOptionMenuItem("", action)
	{
		mSelection = 0;
		mHighlight = -1;
	}

	bool SetValue(int i, int v)
	{
		if (i == SRL_SELECTION)
		{
			mSelection = v;
			return true;
		}
		else if (i == SRL_HIGHLIGHT)
		{
			mHighlight = v;
			return true;
		}
		return false;
	}

	bool GetValue(int i, int *v)
	{
		if (i == SRL_SELECTION)
		{
			*v = mSelection;
			return true;
		}
		return false;
	}

	bool SetString(int i, const char *newtext)
	{
		if (i >= SRL_INDEX && i <= SRL_INDEX+2) 
		{
			mResTexts[i-SRL_INDEX] = newtext;
			if (mResTexts[0].IsEmpty()) mMaxValid = -1;
			else if (mResTexts[1].IsEmpty()) mMaxValid = 0;
			else if (mResTexts[2].IsEmpty()) mMaxValid = 1;
			else mMaxValid = 2;
			return true;
		}
		return false;
	}

	bool GetString(int i, char *s, int len)
	{
		if (i >= SRL_INDEX && i <= SRL_INDEX+2) 
		{
			strncpy(s, mResTexts[i-SRL_INDEX], len-1);
			s[len-1] = 0;
			return true;
		}
		return false;
	}

	bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_Left)
		{
			if (--mSelection < 0) mSelection = mMaxValid;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
			return true;
		}
		else if (mkey == MKEY_Right)
		{
			if (++mSelection > mMaxValid) mSelection = 0;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
			return true;
		}
		else 
		{
			return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
		}
		return false;
	}

	bool MouseEvent(int type, int x, int y)
	{
		int colwidth = screen->GetWidth() / 3;
		mSelection = x / colwidth;
		return FOptionMenuItem::MouseEvent(type, x, y);
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		M_SetVideoMode();
		return true;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		int colwidth = screen->GetWidth() / 3;
		EColorRange color;

		for (int x = 0; x < 3; x++)
		{
			if (selected && mSelection == x)
				color = OptionSettings.mFontColorSelection;
			else if (x == mHighlight)
				color = OptionSettings.mFontColorHighlight;
			else
				color = OptionSettings.mFontColorValue;

			screen->DrawText (SmallFont, color, colwidth * x + 20 * CleanXfac_1, y, mResTexts[x], DTA_CleanNoMove_1, true, TAG_DONE);
		}
		return colwidth * mSelection + 20 * CleanXfac_1 - CURSORSPACE;
	}

	bool Selectable()
	{
		return mMaxValid >= 0;
	}
};

// -------------------------------------------------------------------------------------------------
// [TP] Begin Zandronum-specific widgets
//

#include "team.h"
#include "v_text.h"

//=============================================================================
//
// [TP] FOptionMenuFieldBase
//
// Base class for input fields
//
//=============================================================================

class FOptionMenuFieldBase : public FOptionMenuItem
{
public:
	FOptionMenuFieldBase ( const char* label, const char* menu, const char* graycheck ) :
		FOptionMenuItem ( label, menu ),
		mCVar ( FindCVar( mAction, NULL )),
		mGrayCheck (( graycheck && strlen( graycheck )) ? FindCVar( graycheck, NULL ) : NULL ) {}

	const char* GetCVarString()
	{
		if ( mCVar == NULL )
			return "";

		return mCVar->GetGenericRep( CVAR_String ).String;
	}

	virtual FString Represent()
	{
		return GetCVarString();
	}

	int Draw ( FOptionMenuDescriptor*, int y, int indent, bool selected )
	{
		bool grayed = ( Selectable() == false );
		drawLabel( indent, y, selected ? OptionSettings.mFontColorSelection : OptionSettings.mFontColor, grayed );
		int overlay = grayed? MAKEARGB( 96, 48, 0, 0 ) : 0;

		screen->DrawText( SmallFont, OptionSettings.mFontColorValue, indent + CURSORSPACE, y,
			Represent().GetChars(), DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE );
		return indent;
	}

	bool GetString ( int i, char* s, int len )
	{
		if ( i == 0 )
		{
			strncpy( s, GetCVarString(), len );
			s[len - 1] = '\0';
			return true;
		}

		return false;
	}

	bool SetString ( int i, const char* s )
	{
		if ( i == 0 )
		{
			if ( mCVar )
			{
				UCVarValue vval;
				vval.String = s;
				mCVar->SetGenericRep( vval, CVAR_String );
			}

			return true;
		}

		return false;
	}

	bool IsServerInfo() const
	{
		return mCVar && mCVar->IsServerInfo();
	}

	bool Selectable()
	{
		return ( mGrayCheck == NULL || mGrayCheck->GetGenericRep( CVAR_Bool ).Bool )
			&& FOptionMenuItem::Selectable();
	}

protected:
	// Action is a CVar in this class and derivatives.
	FBaseCVar* mCVar;
	FBaseCVar* mGrayCheck;
};

//=============================================================================
//
// [TP] FOptionMenuTextField
//
// A text input field widget, for use with string CVars.
//
//=============================================================================

class FOptionMenuTextField : public FOptionMenuFieldBase
{
public:
	FOptionMenuTextField ( const char *label, const char* menu, const char* graycheck ) :
		FOptionMenuFieldBase ( label, menu, graycheck ),
		mEntering ( false ) {}

	FString Represent()
	{
		FString text = mEntering ? mEditName : GetCVarString();

		if ( mEntering )
			text += ( gameinfo.gametype & GAME_DoomStrifeChex ) ? '_' : '[';

		return text;
	}

	bool MenuEvent ( int mkey, bool fromcontroller )
	{
		if ( mkey == MKEY_Enter )
		{
			S_Sound( CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE );
			strcpy( mEditName, GetCVarString() );
			mEntering = true;
			DMenu* input = new DTextEnterMenu ( DMenu::CurrentMenu, mEditName, sizeof mEditName, 2, fromcontroller );
			M_ActivateMenu( input );
			return true;
		}
		else if ( mkey == MKEY_Input )
		{
			if ( mCVar )
			{
				UCVarValue vval;
				vval.String = mEditName;
				mCVar->SetGenericRep( vval, CVAR_String );
			}

			mEntering = false;
			return true;
		}
		else if ( mkey == MKEY_Abort )
		{
			mEntering = false;
			return true;
		}

		return FOptionMenuItem::MenuEvent( mkey, fromcontroller );
	}

private:
	bool mEntering;
	char mEditName[128];
};

//=============================================================================
//
// [TP] FOptionMenuNumberField
//
// A numeric input field widget, for use with number CVars where sliders are inappropriate (i.e.
// where the user is interested in the exact value specifically)
//
//=============================================================================

class FOptionMenuNumberField : public FOptionMenuFieldBase
{
public:
	FOptionMenuNumberField ( const char *label, const char* menu, float minimum, float maximum,
		float step, const char* graycheck )
		: FOptionMenuFieldBase ( label, menu, graycheck ),
		mMinimum ( minimum ),
		mMaximum ( maximum ),
		mStep ( step )
	{
		if ( mMaximum <= mMinimum )
			swapvalues( mMinimum, mMaximum );

		if ( mStep <= 0 )
			mStep = 1;
	}

	bool MenuEvent ( int mkey, bool fromcontroller )
	{
		if ( mCVar )
		{
			float value = mCVar->GetGenericRep( CVAR_Float ).Float;

			if ( mkey == MKEY_Left )
			{
				value -= mStep;

				if ( value < mMinimum )
					value = mMaximum;
			}
			else if ( mkey == MKEY_Right || mkey == MKEY_Enter )
			{
				value += mStep;

				if ( value > mMaximum )
					value = mMinimum;
			}
			else
				return FOptionMenuItem::MenuEvent( mkey, fromcontroller );

			UCVarValue vval;
			vval.Float = value;
			mCVar->SetGenericRep( vval, CVAR_Float );
			S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE );
		}

		return true;
	}

private:
	float mMinimum;
	float mMaximum;
	float mStep;
};

//=============================================================================
//
// [TP] FOptionMenuCustomOptionField
//
// Base class for more specialized option fields.
//
//=============================================================================

class FOptionMenuCustomOptionField : public FOptionMenuFieldBase
{
public:
	FOptionMenuCustomOptionField ( const char* label, const char* menu ) :
		FOptionMenuFieldBase ( label, menu, NULL ) {}

	virtual bool IsValid ( int ) = 0;
	virtual FString RepresentOption ( int ) = 0;
	virtual int Maximum() = 0;

	// [TP] Seeks the value forwards or backwards. Returns true if it was possible to find a new
	// player. Can return false if there are no players (not in a game)
	bool Seek ( bool forward )
	{
		if ( mCVar )
		{
			int step = forward ? 1 : -1;
			int const startValue = mCVar->GetGenericRep( CVAR_Int ).Int;
			int newValue = startValue;
			int const end = Maximum();

			do
			{
				newValue += step;

				if ( newValue < 0 )
					newValue = end - 1;

				if ( newValue >= end )
					newValue = 0;
			}
			while (( IsValid( newValue ) == false ) && ( newValue != startValue ));

			if ( newValue == startValue )
				return false;

			UCVarValue vval;
			vval.Int = newValue;
			mCVar->SetGenericRep( vval, CVAR_Int );
		}

		return true;
	}

	bool MenuEvent ( int mkey, bool fromcontroller )
	{
		if ( mkey == MKEY_Left || mkey == MKEY_Right || mkey == MKEY_Enter )
		{
			if ( Seek( mkey != MKEY_Left ))
				S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE );

			return true;
		}

		return FOptionMenuItem::MenuEvent( mkey, fromcontroller );
	}

	FString Represent()
	{
		if ( mCVar )
		{
			int i = mCVar->GetGenericRep( CVAR_Int ).Int;

			if ( IsValid( i ))
				return RepresentOption( i );
		}

		return "Unknown";
	}
};

//=============================================================================
//
// [TP] FOptionMenuPlayerField
//
// Selects a player. Cannot be a derivative of the Option widgets because it
// needs to be able to skip non-existing players.
//
//=============================================================================

class FOptionMenuPlayerField : public FOptionMenuCustomOptionField
{
	bool mAllowBots;
	bool mAllowSelf;

public:
	FOptionMenuPlayerField ( const char* label, const char* menu, bool allowBots, bool allowSelf ) :
		FOptionMenuCustomOptionField ( label, menu ),
		mAllowBots ( allowBots ),
		mAllowSelf ( allowSelf ) {}

	bool IsValid ( int player )
	{
		if (( player < 0 )
			|| ( player >= MAXPLAYERS )
			|| ( playeringame[player] == false ))
		{
			return false;
		}

		if (( mAllowSelf == false ) && ( player == consoleplayer ))
			return false;

		return mAllowBots || ( players[player].bIsBot == false );
	}

	FString RepresentOption ( int playerId )
	{
		return players[playerId].userinfo.GetName();
	}

	int Maximum()
	{
		return MAXPLAYERS;
	}
};

//=============================================================================
//
// [TP] FOptionMenuTeamField
//
// Selects a team (similar reasoning as with FOptionMenuPlayerField)
//
//=============================================================================

class FOptionMenuTeamField : public FOptionMenuCustomOptionField
{
public:
	FOptionMenuTeamField()
		: FOptionMenuCustomOptionField ( "Team", "menu_jointeamidx" ) {}

	bool IsValid ( int teamId )
	{
		return TEAM_CheckIfValid( teamId ) || ( unsigned ( teamId ) == teams.Size() );
	}

	FString RepresentOption ( int teamId )
	{
		if ( static_cast<unsigned>( teamId ) >= teams.Size() )
			return "Random";

		FString result = "\\c";
		result += teams[teamId].TextColor;
		result += teams[teamId].Name;
		V_ColorizeString( result );
		return result;
	}

	int Maximum()
	{
		return teams.Size() + 1;
	}
};

//=============================================================================
//
// [TP] FOptionMenuPlayerClassField
//
// Selects a player class.
//
//=============================================================================

EXTERN_CVAR( Int, menu_jointeamidx )

class FOptionMenuPlayerClassField : public FOptionMenuCustomOptionField
{
public:
	FOptionMenuPlayerClassField()
		: FOptionMenuCustomOptionField ( "Class", "menu_joinclassidx" ) {}

	bool IsValid ( int classId )
	{
		// [BB] The random class is always a valid choice.
		if ( classId == ( Maximum() - 1 ) )
			return true;

		// [EP] Temporary spots require no other limitation.
		if ( !TEAM_ShouldJoinTeam() )
			return true;

		return TEAM_IsClassAllowedForTeam( classId, menu_jointeamidx );
	}

	int Maximum()
	{
		return PlayerClasses.Size() + 1;
	}

	FString RepresentOption ( int classId )
	{
		if ( classId == ( Maximum() - 1 ) )
			return "Random";
		else
			return GetPrintableDisplayName( PlayerClasses[classId].Type );
	}
};

//=============================================================================
//
// [BB] FOptionMenuServerBrowserLine
//
//=============================================================================

class FOptionMenuServerBrowserLine : public FOptionMenuItem
{
	int mSlotNum;
public:

	FOptionMenuServerBrowserLine( const int slotNum )
		: FOptionMenuItem ( "" )
	{
		mSlotNum = slotNum;
	}

	bool Activate();
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected);
	bool Selectable();
	bool MenuEvent (int mkey, bool fromcontroller);
};

//
// [TP] End of Zandronum-specific option menu widgets
// -------------------------------------------------------------------------------------------------
