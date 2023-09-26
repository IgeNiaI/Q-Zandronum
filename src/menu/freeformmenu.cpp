/*
** freeformmenu.cpp
** Handler class for the freeform menus and associated items
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

#include <float.h>
#include "gstrings.h"
#include "v_palette.h"
#include "v_video.h"
#include "v_text.h"
#include "d_gui.h"
#include "d_event.h"
#include "c_dispatch.h"
#include "c_bind.h"
#include "gi.h"
#include "deathmatch.h"
#include "team.h"
#include "menu/menu.h"
#include "menu/freeformmenuitems.h"

EXTERN_CVAR(Int, m_show_backbutton)

IMPLEMENT_CLASS(DFreeformMenu)

//=============================================================================
//
//
//
//=============================================================================

DFreeformMenu::DFreeformMenu(DMenu* parent, FFreeformMenuDescriptor* desc)
	: DMenu(parent)
{
	CanScrollUp = false;
	CanScrollDown = false;
	mFocusControl = NULL;
	Init(parent, desc);
}

//=============================================================================
//
//
//
//=============================================================================

void DFreeformMenu::Init(DMenu* parent, FFreeformMenuDescriptor* desc)
{
	mParentMenu = parent;
	GC::WriteBarrier(this, parent);
	mDesc = desc;
	if (mDesc != NULL)
	{
		if (mDesc->mDefaultSelection == -2) mDesc->mSelectedItem = -2;
		if (mDesc->mSelectedItem == -1) mDesc->mSelectedItem = FirstSelectable();
		if (mDesc->mResetScroll) mDesc->mScrollPos = 0;
		
		int ytop = mDesc->mTopPadding;
		if (ytop < 0)
			ytop = -ytop;
		int pagesize = screen->GetHeight() / CleanYfac_1 - ytop;

		if (mDesc->mHeightOverride > 0)
		{
			LowestScroll = mDesc->mHeightOverride;
		}
		else
		{
			int x, y;
			LowestScroll = 0;
			for (unsigned i = 0; i < mDesc->mItems.Size(); i++)
			{
				if (!mDesc->mItems[i]->IsStatic()) // you can't scroll static items
				{
					mDesc->mItems[i]->CalcDrawScreenPos(x, y, mDesc->mItems[i]->GetWidth(), mDesc->mItems[i]->GetHeight(), false);
					y /= CleanYfac_1;
					y += mDesc->mItems[i]->GetMouseAreaHeight();
					if (y > LowestScroll)
						LowestScroll = y;
				}
			}
		}

		if (LowestScroll < pagesize)
		{
			if (mDesc->mCenter)
				CenteredOffset = (pagesize - LowestScroll) / 2;
			else
				CenteredOffset = 0;
			LowestScroll = 0;
		}
		else
		{
			CenteredOffset = 0;
			LowestScroll -= pagesize;
		}

		CanScrollUp = (mDesc->mScrollPos > 0);
		CanScrollDown = (mDesc->mScrollPos < LowestScroll);
	}
}

//=============================================================================
//
//
//
//=============================================================================

int DFreeformMenu::FirstSelectable()
{
	if (mDesc != NULL)
	{
		unsigned int topLeftItem = -1;
		// Find the most top left item
		for (unsigned int i = 0; i < mDesc->mItems.Size(); i++)
		{
			int topLeftX = 0, topLeftY = 0;
			if (mDesc->mItems[i]->IsVisible() && mDesc->mItems[i]->Selectable())
			{
				if (topLeftItem == -1)
				{
					topLeftItem = i;
					mDesc->mItems[i]->CalcDrawScreenPos(topLeftX, topLeftY, mDesc->mItems[i]->GetWidth(), mDesc->mItems[i]->GetHeight(), false);
				}
				else
				{
					int x, y;
					mDesc->mItems[i]->CalcDrawScreenPos(x, y, mDesc->mItems[i]->GetWidth(), mDesc->mItems[i]->GetHeight(), false);
					if ((y < topLeftY) || (y == topLeftY && x < topLeftX))
					{
						topLeftItem = i;
						topLeftX = x;
						topLeftY = y;
					}
				}
			}
		}

		return topLeftItem;
	}
	return -1;
}

//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* DFreeformMenu::GetItem(FName name)
{
	for (unsigned i = 0; i < mDesc->mItems.Size(); i++)
	{
		FName nm = mDesc->mItems[i]->GetAction(NULL);
		if (nm == name) return mDesc->mItems[i];
	}
	return NULL;
}

//=============================================================================
//
//
//
//=============================================================================

bool DFreeformMenu::Responder(event_t* ev)
{
	if (ev->type == EV_GUI_Event)
	{
		if (ev->subtype == EV_GUI_WheelUp)
		{
			if (CanScrollUp)
			{
				mDesc->mScrollPos -= mDesc->mScrollSpeed;
				CanScrollDown = true;
				if (mDesc->mScrollPos < 0)
				{
					mDesc->mScrollPos = 0;
					CanScrollUp = false;
				}
			}
			return true;
		}
		else if (ev->subtype == EV_GUI_WheelDown)
		{
			if (CanScrollDown)
			{
				mDesc->mScrollPos += mDesc->mScrollSpeed;
				CanScrollUp = true;
				if (mDesc->mScrollPos > LowestScroll)
				{
					mDesc->mScrollPos = LowestScroll;
					CanScrollDown = false;
				}
			}
			return true;
		}
	}
	return Super::Responder(ev);
}

//=============================================================================
//
//
//
//=============================================================================

bool DFreeformMenu::MenuEvent(int mkey, bool fromcontroller)
{
	int startedAt = mDesc->mSelectedItem;

	switch (mkey)
	{
	case MKEY_Up:
	case MKEY_Down:
	case MKEY_Left:
	case MKEY_Right:
	{
		if (mDesc->mSelectedItem <= -1)
		{
			mDesc->mSelectedItem = FirstSelectable();
			break;
		}

		if (mDesc->mSelectedItem >= 0 &&
			mDesc->mItems[mDesc->mSelectedItem]->MenuEvent(mkey, fromcontroller)) return true;
		
		int yOffset = mDesc->mTopPadding;
		if (yOffset < 0)
			yOffset = -yOffset;
		yOffset += CenteredOffset;

		unsigned int nearestItem = mDesc->mSelectedItem;
		int selectedX, selectedY;
		mDesc->mItems[mDesc->mSelectedItem]->CalcDrawScreenPos(selectedX, selectedY,
			mDesc->mItems[mDesc->mSelectedItem]->GetWidth(),
			mDesc->mItems[mDesc->mSelectedItem]->GetHeight(), false);
		selectedX += mDesc->mItems[mDesc->mSelectedItem]->GetWidth() * CleanXfac_1 / 2;
		selectedY += mDesc->mItems[mDesc->mSelectedItem]->GetHeight() * CleanYfac_1 / 2;

		selectedY += (mDesc->mItems[mDesc->mSelectedItem]->IsStatic() ? 0 : yOffset) * CleanYfac_1;

		int nearestDist = INT_MAX;
		for (unsigned int i = 0; i < mDesc->mItems.Size(); i++)
		{
			if (i != mDesc->mSelectedItem && mDesc->mItems[i]->IsVisible() && mDesc->mItems[i]->Selectable()) // item must be selectable
			{
				int x, y;
				mDesc->mItems[i]->CalcDrawScreenPos(x, y, mDesc->mItems[i]->GetWidth(), mDesc->mItems[i]->GetHeight(), false);
				x += mDesc->mItems[i]->GetWidth() * CleanXfac_1 / 2;
				y += mDesc->mItems[i]->GetHeight() * CleanYfac_1 / 2;

				y += (mDesc->mItems[i]->IsStatic() ? 0 : yOffset) * CleanYfac_1;

				int xDiff = x - selectedX;
				int yDiff = y - selectedY;
				bool directionCheck = false;
				switch (mkey)
				{
				case MKEY_Up:		directionCheck = yDiff < 0;	break;
				case MKEY_Down:		directionCheck = yDiff > 0;	break;
				case MKEY_Left:		directionCheck = xDiff < 0;	break;
				case MKEY_Right:	directionCheck = xDiff > 0;	break;
				}

				if (directionCheck) // item must be in correct direction
				{
					int dist = (int)sqrt(xDiff * xDiff + yDiff * yDiff);
					if (dist < nearestDist) // item must be closest
					{
						nearestDist = dist;
						nearestItem = i;
					}
				}
			}
		}

		mDesc->mSelectedItem = nearestItem;

		if (mDesc->mItems[mDesc->mSelectedItem]->GetY() < mDesc->mScrollPos)
		{
			mDesc->mScrollPos = mDesc->mItems[mDesc->mSelectedItem]->GetY();
			if (mDesc->mScrollPos < 0)
				mDesc->mScrollPos = 0;
		}
		else 
		{
			int ytop = mDesc->mTopPadding;
			if (ytop < 0)
				ytop = -ytop;
			int pagesize = screen->GetHeight() / CleanYfac_1 - ytop;

			int x, y;
			mDesc->mItems[mDesc->mSelectedItem]->CalcDrawScreenPos(x, y,
				mDesc->mItems[mDesc->mSelectedItem]->GetWidth(), mDesc->mItems[mDesc->mSelectedItem]->GetHeight(), false);
			y /= CleanYfac_1;
			y += mDesc->mItems[mDesc->mSelectedItem]->GetMouseAreaHeight();
			if (y - pagesize > mDesc->mScrollPos)
			{
				mDesc->mScrollPos = y - pagesize;
				if (mDesc->mScrollPos > LowestScroll)
					mDesc->mScrollPos = LowestScroll;
			}
		}
		break;
	}
	case MKEY_PageUp:
		if (CanScrollUp)
		{
			int ytop = mDesc->mTopPadding;
			if (ytop < 0)
				ytop = -ytop;
			ytop *= CleanYfac_1;
			int pagesize = (screen->GetHeight() - ytop) / CleanYfac_1;

			mDesc->mScrollPos -= pagesize;
			CanScrollDown = true;
			if (mDesc->mScrollPos < 0)
			{
				mDesc->mScrollPos = 0;
				CanScrollUp = false;
			}
		}
		break;

	case MKEY_PageDown:
		if (CanScrollDown)
		{
			int ytop = mDesc->mTopPadding;
			if (ytop < 0)
				ytop = -ytop;
			ytop *= CleanYfac_1;
			int pagesize = (screen->GetHeight() - ytop) / CleanYfac_1;

			mDesc->mScrollPos += pagesize;
			CanScrollUp = true;
			if (mDesc->mScrollPos > LowestScroll)
			{
				mDesc->mScrollPos = LowestScroll;
				CanScrollDown = false;
			}
		}
		break;

	case MKEY_Enter:
		if (mDesc->mSelectedItem >= 0 && mDesc->mItems[mDesc->mSelectedItem]->Activate())
		{
			return true;
		}
		// fall through
	default:
		if (mDesc->mSelectedItem >= 0 &&
			mDesc->mItems[mDesc->mSelectedItem]->MenuEvent(mkey, fromcontroller)) return true;
		return Super::MenuEvent(mkey, fromcontroller);
	}

	if (mDesc->mSelectedItem != startedAt)
	{
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
	}
	return true;
}

//=============================================================================
//
//
//
//=============================================================================

bool DFreeformMenu::MouseEvent(int type, int x, int y)
{
	if (mFocusControl)
	{
		mFocusControl->MouseEvent(type, x, y);
		return true;
	}
	else
	{
		int startedAt = mDesc->mSelectedItem;

		int yOffset = mDesc->mTopPadding;
		if (yOffset < 0)
			yOffset = -yOffset;
		yOffset += CenteredOffset;

		unsigned i;
		// going from last to first, because visually last items are drawn on top of first
		for (i = mDesc->mItems.Size(); i-- > 0;)
		{
			if (mDesc->mItems[i]->IsVisible()
				&& mDesc->mItems[i]->Selectable()
				&& mDesc->mItems[i]->CheckCoordinate(
					x,
					y - (mDesc->mItems[i]->IsStatic() ? 0 : yOffset - mDesc->mScrollPos) * CleanYfac_1)
				)
			{
				mDesc->mSelectedItem = i;
				mDesc->mItems[i]->MouseEvent(type, x, y);

				if (mDesc->mSelectedItem != startedAt && !S_GetSoundPaused())
				{
					S_Sound(CHAN_VOICE | CHAN_UI, "menu/hover", snd_menuvolume, ATTN_NONE);
				}

				return true;
			}
		}
	}
	mDesc->mSelectedItem = -2;
	return Super::MouseEvent(type, x, y);
}

//=============================================================================
//
//
//
//=============================================================================

bool DFreeformMenu::MouseEventBack(int type, int x, int y)
{
	if (mDesc->mShowBackButton)
	{
		return Super::MouseEventBack(type, x, y); // super class only checkf the back button click
	}

	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void DFreeformMenu::Ticker()
{
	Super::Ticker();
	for (unsigned i = 0; i < mDesc->mItems.Size(); i++)
	{
		mDesc->mItems[i]->Ticker();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DFreeformMenu::Drawer()
{
	int ytop = mDesc->mTopPadding;

	if (ytop <= 0)
		ytop = -ytop;
	ytop *= CleanYfac_1;

	for (unsigned i = 0; i < mDesc->mItems.Size(); i++)
	{
		if (mDesc->mItems[i]->IsVisible())
		{
			bool isSelected = mDesc->mSelectedItem == (int)i;
			mDesc->mItems[i]->Draw(mDesc, ytop - mDesc->mScrollPos * CleanYfac_1 + CenteredOffset * CleanYfac_1, isSelected);
		}
	}

	if (CanScrollUp)
	{
		if (mDesc->mShowBackButton && m_show_backbutton == 0 && TexMan.CheckForTexture(gameinfo.mBackButton, FTexture::TEX_MiscPatch).isValid())
		{
			FTexture* texBack = TexMan(gameinfo.mBackButton);
			if (ytop < texBack->GetScaledHeight() * CleanYfac_1)
				ytop = texBack->GetScaledHeight() * CleanYfac_1;
		}
		ytop += 4 * CleanYfac_1;
		if (TexMan.CheckForTexture(gameinfo.mUpArrow, FTexture::TEX_MiscPatch).isValid())
		{
			FTexture* texArrow = TexMan(gameinfo.mUpArrow);
			screen->DrawTexture(texArrow, 3 * CleanXfac_1, ytop, DTA_CleanNoMove_1, true, TAG_DONE);
		}
		else
		{
			M_DrawConText(CR_ORANGE, 3 * CleanXfac_1, ytop, "\x1a");
		}
	}
	if (CanScrollDown)
	{
		int ybot = 8 * CleanYfac_1;
		if (mDesc->mShowBackButton && m_show_backbutton == 2 && TexMan.CheckForTexture(gameinfo.mBackButton, FTexture::TEX_MiscPatch).isValid())
		{
			FTexture* texBack = TexMan(gameinfo.mBackButton);
			if (ybot < texBack->GetScaledHeight() * CleanYfac_1)
				ybot = texBack->GetScaledHeight() * CleanYfac_1;
		}
		if (TexMan.CheckForTexture(gameinfo.mDownArrow, FTexture::TEX_MiscPatch).isValid())
		{
			FTexture* texArrow = TexMan(gameinfo.mDownArrow);
			ybot += texArrow->GetScaledHeight() * CleanYfac_1;
			screen->DrawTexture(texArrow, 3 * CleanXfac_1, screen->GetHeight() - ybot, DTA_CleanNoMove_1, true, TAG_DONE);
		}
		else
		{
			ybot += 4 * CleanYfac_1;
			M_DrawConText(CR_ORANGE, 3 * CleanXfac_1, screen->GetHeight() - ybot, "\x1b");
		}
	}

	if (mDesc->mShowBackButton)
	{
		Super::Drawer(); // super class only draws the back button
	}
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuDescriptor::GetItem(FName name)
{
	for (unsigned i = 0; i < mItems.Size(); i++)
	{
		FName nm = mItems[i]->GetAction(NULL);
		if (nm == name) return mItems[i];
	}
	return NULL;
}


//=============================================================================
//
//
//
//=============================================================================

bool FFreeformMenuDescriptor::DeleteItem(FName name)
{
	for (unsigned i = 0; i < mItems.Size(); i++)
	{
		FName nm = mItems[i]->GetAction(NULL);
		if (nm == name) {
			mItems.Delete(i);
			return true;
		}
	}
	return false;
}


//=============================================================================
//
// base class for menu items
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItem::Clone()
{
	return NULL; // you are not supposed to clone base class
}

bool FFreeformMenuItem::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItem* theOther = static_cast<FFreeformMenuItem*>(other);
	if (theOther != NULL)
	{
		theOther->SetStatic(mIsStatic);
		theOther->SetVisibleOnTitlemap(mIsVisibleOnTitlemap);
		theOther->SetVisibleInSingleplayer(mIsVisibleInSingleplayer);
		theOther->SetVisibleInBotplay(mIsVisibleInBotplay);
		theOther->SetVisibleInMultiplayer(mIsVisibleInMultiplayer);
		theOther->SetX(mXpos);
		theOther->SetY(mYpos);
		theOther->SetWidth(mWidth);
		theOther->SetHeight(mHeight);
		theOther->SetMouseArea(mMouseAreaWidth, mMouseAreaHeight);
		theOther->SetPadding(mXPadding, mYPadding);
		theOther->SetGravity(mGravity);
		theOther->SetAnchor(mAnchor);
		theOther->SetAlpha(mAlpha);
		theOther->SetAction(mAction);
		theOther->SetVisibilityCVar(mVisibilityCVar != NULL ? mVisibilityCVar->GetName() : NULL, mVisibilityCVarMin, mVisibilityCVarMax);
		return true;
	}

	return false;
}

void FFreeformMenuItem::Draw(FFreeformMenuDescriptor* desc, int yoffset, bool selected)
{
}

bool FFreeformMenuItem::CheckCoordinate(int x, int y)
{
	int width = mMouseAreaWidth >= 0 ? mMouseAreaWidth : GetWidth();
	int height = mMouseAreaHeight >= 0 ? mMouseAreaHeight : GetHeight();

	int xLeft, yTop;
	CalcDrawScreenPos(xLeft, yTop, width, height, false);
	width *= CleanXfac_1;
	height *= CleanYfac_1;

	return x >= xLeft && x < xLeft + width
		&& y >= yTop  && y < yTop + height;
}

void FFreeformMenuItem::CalcDrawScreenPos(int &outX, int &outY, int width, int height, bool withPadding)
{
	outX = mXpos * CleanXfac_1;
	outY = mYpos * CleanYfac_1;
	
	// Adjust for gravity
	if (mGravity & GRAV_RIGHT)
	{	outX += screen->GetWidth();			}
	else if (mGravity & GRAV_CENTER_HORIZONTAL)
	{	outX += screen->GetWidth() / 2;		}

	if (mGravity & GRAV_BOTTOM)
	{	outY += screen->GetHeight();		}
	else if (mGravity & GRAV_CENTER_VERTICAL)
	{	outY += screen->GetHeight() / 2;	}

	// Adjust for anchor
	if (mAnchor & GRAV_RIGHT)
	{	outX -= width * CleanXfac_1;		}
	else if (mAnchor & GRAV_CENTER_HORIZONTAL)
	{	outX -= width * CleanXfac_1 / 2;	}

	if (mAnchor & GRAV_BOTTOM)
	{	outY -= height * CleanYfac_1;		}
	else if (mAnchor & GRAV_CENTER_VERTICAL)
	{	outY -= height * CleanYfac_1 / 2;	}

	// Adjust for padding
	if (withPadding)
	{
		outX += mXPadding * CleanXfac_1;
		outY += mYPadding * CleanYfac_1;
	}
}



bool FFreeformMenuItem::IsEnabled()
{
	return mEnabled;
}

bool FFreeformMenuItem::Selectable()
{
	return false;
}

bool FFreeformMenuItem::MouseEvent(int type, int x, int y)
{
	if (Selectable())
	{
		if (type == DMenu::MOUSE_Release)
			return DMenu::CurrentMenu->MenuEvent(MKEY_Enter, true);
		else if (type == DMenu::MOUSE_Release2)
			return DMenu::CurrentMenu->MenuEvent(MKEY_Clear, true);
	}
	return false;
}


//=============================================================================
//
//
//
//=============================================================================

bool FFreeformMenuItem::IsVisible() const
{
	bool result = true;

	// Titlemap check
	if (!mIsVisibleOnTitlemap && gamestate == GS_TITLELEVEL)
		return false;

	// Singleplayer check
	if (!mIsVisibleInSingleplayer && (gamestate != GS_TITLELEVEL) && (NETWORK_GetState() == NETSTATE_SINGLE || NETWORK_GetState() == NETSTATE_SINGLE_MULTIPLAYER))
		return false;
	
	// Botplay check
	if (!mIsVisibleInBotplay && (gamestate != GS_TITLELEVEL) && NETWORK_GetState() == NETSTATE_SINGLE_MULTIPLAYER && (deathmatch || teamgame))
		return false;

	// Multiplayer check
	if (!mIsVisibleInMultiplayer && (gamestate != GS_TITLELEVEL) && NETWORK_GetState( ) == NETSTATE_CLIENT)
		return false;

	// CVar check
	if (mVisibilityCVar != NULL)
	{
		int cvarValue = mVisibilityCVar->GetGenericRep(CVAR_Int).Int;
		return (cvarValue >= mVisibilityCVarMin) && (cvarValue <= mVisibilityCVarMax);
	}

	return true;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemLabel::Clone()
{
	FFreeformMenuItemLabel* clone = new FFreeformMenuItemLabel();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuItemLabel::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemLabel* theOther = static_cast<FFreeformMenuItemLabel*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItem::CopyTo(other);
		theOther->SetLabel(mLabel);
		theOther->SetFont(mFont);
		theOther->SetFontColor(mFontColor);
		theOther->SetBackground(mBackground);
		return true;
	}

	return false;
}

void FFreeformMenuItemLabel::Draw(FFreeformMenuDescriptor* desc, int yoffset, bool selected)
{
	int width = GetWidth();
	int height = GetHeight();

	if (mBackground.isValid())
	{
		FTexture* tex = TexMan(mBackground);
		int x, y;
		width = GetWidth() >= 0 ? GetWidth() : tex->GetScaledWidth();
		height = GetHeight() >= 0 ? GetHeight() : tex->GetScaledHeight();
		CalcDrawScreenPos(x, y, width, height, false);
		if (!mIsStatic)
			y += yoffset;
		screen->DrawTexture(tex, x, y,
			DTA_CleanNoMove_1, true,
			DTA_DestWidth, width * CleanXfac_1,
			DTA_DestHeight, height * CleanYfac_1,
			DTA_Alpha, mAlpha,
			TAG_DONE);
	}

	if (mLabel != NULL && strcmp(mLabel, "") != 0)
	{
		const char* label = mLabel;
		if (*label == '$') label = GStrings(label + 1);
		if ( mMaxLabelWidth > 0 && mFont->StringWidth( label ) > mMaxLabelWidth )
		{
			// Break it onto multiple lines, if necessary.
			const BYTE *bytes = reinterpret_cast<const BYTE*>( label );
			FBrokenLines *lines = V_BreakLines( mFont, mMaxLabelWidth, bytes );
			int messageY = 0;

			for ( int i = 0; lines[i].Width != -1; ++i )
			{
				int x, y;
				int textwidth = mFont->StringWidth( lines[i].Text );
				int textheight = mFont->GetHeight();
				CalcDrawScreenPos(x, y, textwidth, textheight, true);

				if (!mIsStatic)
					y += yoffset;
				screen->DrawText(mFont, mFontColor, x, y + messageY, lines[i].Text,
					DTA_CleanNoMove_1, true,
					DTA_Alpha, mAlpha,
					TAG_DONE);
				messageY += mFont->GetHeight() * CleanYfac_1;
			}

			V_FreeBrokenLines( lines );
		}
		else
		{
			int clipleft, clipright, cliptop, clipbottom;
			CalcTextClip(clipleft, clipright, cliptop, clipbottom, width, height, mIsStatic ? 0 : yoffset);

			int x, y;
			int textwidth = mFont->StringWidth(label);
			int textheight = mFont->GetHeight();
			CalcDrawScreenPos(x, y, textwidth, textheight, true);
			if (!mIsStatic)
				y += yoffset;
			screen->DrawText(mFont, mFontColor, x, y, label,
				DTA_CleanNoMove_1, true,
				DTA_ClipLeft, clipleft,
				DTA_ClipRight, clipright,
				DTA_ClipTop, cliptop,
				DTA_ClipBottom, clipbottom,
				DTA_Alpha, mAlpha,
				TAG_DONE);
		}
	}
}

void FFreeformMenuItemLabel::CalcTextClip(int &left, int &right, int &top, int &bottom, int width, int height, int yoffset)
{
	if (width > 0 || height > 0)
	{
		int x, y;
		CalcDrawScreenPos(x, y, width, height, false);
		left = x;
		top = y + yoffset;
		right = x + width * CleanXfac_1;
		bottom = y + height * CleanYfac_1 + yoffset;
	}
	else
	{
		left = 0;
		top = 0;
		right = screen->GetWidth();
		bottom = screen->GetHeight();
	}
}

//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemActionableBase::Clone()
{
	return NULL; // you are not supposed to clone base class
}

bool FFreeformMenuItemActionableBase::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemActionableBase* theOther = static_cast<FFreeformMenuItemActionableBase*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemLabel::CopyTo(other);
		theOther->SetSelectedFontColor(mSelectedFontColor);
		theOther->SetSelectedBackground(mSelectedBackground);
		theOther->SetSelectedForeground(mSelectedForeground, mForegroundWidth, mForegroundHeight);
		theOther->SetGrayCheck(mGrayCheck != NULL ? mGrayCheck->GetName() : NULL, mGrayCheckMin, mGrayCheckMax);
		theOther->SetSelectable(mSelectable);
		return true;
	}

	return false;
}

bool FFreeformMenuItemActionableBase::Selectable()
{
	return mSelectable && IsVisible() && IsEnabled();
}

void FFreeformMenuItemActionableBase::Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected)
{
	int overlay = IsEnabled() ? 0 : MAKEARGB(96, 48, 0, 0);

	FTexture* tex;

	int width = GetWidth();
	int height = GetHeight();
	if (mBackground.isValid())
	{
		if (selected && mSelectedBackground.isValid())
			tex = TexMan(mSelectedBackground);
		else
			tex = TexMan(mBackground);
		int x, y;
		if (width < 0) width = tex->GetScaledWidth();
		if (height < 0) height = tex->GetScaledHeight();
		CalcDrawScreenPos(x, y, width, height, false);
		if (!mIsStatic)
			y += yoffset;
		screen->DrawTexture(tex, x, y,
			DTA_CleanNoMove_1, true,
			DTA_DestWidth, width * CleanXfac_1,
			DTA_DestHeight, height * CleanYfac_1,
			DTA_ColorOverlay, overlay,
			DTA_Alpha, mAlpha,
			TAG_DONE);
	}

	if (width < 0) width = 0;
	if (height < 0) height = 0;

	int clipleft, clipright, cliptop, clipbottom;
	CalcTextClip(clipleft, clipright, cliptop, clipbottom, width, height, mIsStatic ? 0 : yoffset);

	if (selected && mSelectedForeground.isValid())
	{
		tex = TexMan(mSelectedForeground);
		int x, y;
		int foregroundWidth = mForegroundWidth >= 0 ? mForegroundWidth : tex->GetScaledWidth();
		int foregroundHeight = mForegroundHeight >= 0 ? mForegroundHeight : tex->GetScaledHeight();
		CalcDrawScreenPos(x, y, foregroundWidth, foregroundHeight, false);
		if (mAnchor & GRAV_LEFT)
		{	x -= (foregroundWidth - width) * CleanXfac_1 / 2;		}
		else if (mAnchor & GRAV_RIGHT)
		{	x += (foregroundWidth - width) * CleanXfac_1 / 2;		}

		if (mAnchor & GRAV_TOP)
		{	y -= (foregroundHeight - height) * CleanYfac_1 / 2;	}
		else if (mAnchor & GRAV_BOTTOM)
		{	y += (foregroundHeight - height) * CleanYfac_1 / 2;	}
		
		if (!mIsStatic)
			y += yoffset;
		screen->DrawTexture(tex, x, y,
			DTA_CleanNoMove_1, true,
			DTA_DestWidth, foregroundWidth * CleanXfac_1,
			DTA_DestHeight, foregroundHeight * CleanYfac_1,
			DTA_ColorOverlay, overlay,
			DTA_Alpha, mAlpha,
			TAG_DONE);
	}

	if (mLabel != NULL && strcmp(mLabel, "") != 0)
	{
		const char* label = mLabel;
		if (*label == '$') label = GStrings(label + 1);

		if ( mMaxLabelWidth > 0 && mFont->StringWidth( label ) > mMaxLabelWidth )
		{
			// Break it onto multiple lines, if necessary.
			const BYTE *bytes = reinterpret_cast<const BYTE*>( label );
			FBrokenLines *lines = V_BreakLines( mFont, mMaxLabelWidth, bytes );
			int messageY = 0;

			for ( int i = 0; lines[i].Width != -1; ++i )
			{
				int x, y;
				int textwidth = mFont->StringWidth( lines[i].Text );
				int textheight = mFont->GetHeight();
				CalcDrawScreenPos(x, y, textwidth, textheight, true);

				if (!mIsStatic)
					y += yoffset;
				screen->DrawText(mFont, selected ? mSelectedFontColor : mFontColor, x, y + messageY, lines[i].Text,
					DTA_CleanNoMove_1, true,
					DTA_ClipLeft, clipleft,
					DTA_ClipRight, clipright,
					DTA_ClipTop, cliptop,
					DTA_ClipBottom, clipbottom,
					DTA_Alpha, mAlpha,
					TAG_DONE);
				messageY += mFont->GetHeight() * CleanYfac_1;
			}

			V_FreeBrokenLines( lines );
		}
		else
		{
			int x, y;
			int textwidth = mFont->StringWidth(label);
			int textheight = mFont->GetHeight();
			CalcDrawScreenPos(x, y, textwidth, textheight, true);
			if (!mIsStatic)
				y += yoffset;
			screen->DrawText(mFont, selected ? mSelectedFontColor : mFontColor, x, y, label,
				DTA_CleanNoMove_1, true,
				DTA_ClipLeft, clipleft,
				DTA_ClipRight, clipright,
				DTA_ClipTop, cliptop,
				DTA_ClipBottom, clipbottom,
				DTA_Alpha, mAlpha,
				TAG_DONE);
		}
	}
}

bool FFreeformMenuItemActionableBase::IsEnabled()
{
	int cvarValue = mGrayCheck != NULL ? mGrayCheck->GetGenericRep(CVAR_Int).Int : 1;
	return mEnabled && (cvarValue >= mGrayCheckMin) && (cvarValue <= mGrayCheckMax);
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemSubmenu::Clone()
{
	FFreeformMenuItemSubmenu* clone = new FFreeformMenuItemSubmenu();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuItemSubmenu::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemSubmenu* theOther = static_cast<FFreeformMenuItemSubmenu*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemActionableBase::CopyTo(other);
		theOther->SetParam(mParam);
		return true;
	}

	return false;
}

bool FFreeformMenuItemSubmenu::Activate()
{
	S_Sound(CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
	M_SetMenu(mAction, mParam);
	return true;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemCommand::Clone()
{
	FFreeformMenuItemCommand* clone = new FFreeformMenuItemCommand();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuItemCommand::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemCommand* theOther = static_cast<FFreeformMenuItemCommand*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemActionableBase::CopyTo(other);
		theOther->SetSafe(mIsSafe);
		return true;
	}

	return false;
}

bool FFreeformMenuItemCommand::MenuEvent(int mkey, bool fromcontroller)
{
	if (mkey == MKEY_MBYes)
	{
		C_DoCommand(mAction);
		return true;
	}
	return FFreeformMenuItemActionableBase::MenuEvent(mkey, fromcontroller);
}

bool FFreeformMenuItemCommand::Activate()
{
	if (mIsSafe)
	{
		M_StartMessage("Do you really want to do this?", 0);
	}
	else
	{
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		C_DoCommand(mAction);
	}
	return true;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemOptionBase::Clone()
{
	return NULL; // you are not supposed to clone base class
}

bool FFreeformMenuItemOptionBase::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemOptionBase* theOther = static_cast<FFreeformMenuItemOptionBase*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemActionableBase::CopyTo(other);
		theOther->SetValues(mValues);
		theOther->SetUnknownValueText(mUnknownValueText);
		theOther->SetBackgroundValues(mBackgroundValues);
		theOther->SetUnknownBackgroundTexture(mUnknownBackgroundTexture);
		return true;
	}

	return false;
}

void FFreeformMenuItemOptionBase::Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected)
{
	int Selection = GetSelection();
	FOptionValues **opt = OptionValues.CheckKey(mValues);
	if (Selection < 0 || opt == NULL || *opt == NULL)
		ReplaceString(&mLabel, mUnknownValueText);
	else
		ReplaceString(&mLabel, (*opt)->mValues[Selection].Text);

	FOptionValues** optback = OptionValues.CheckKey(mBackgroundValues);
	if (Selection < 0 || optback == NULL || *optback == NULL)
	{
		mBackground = mUnknownBackgroundTexture;
	}
	else
	{
		mBackground = TexMan.CheckForTexture((*optback)->mValues[Selection].Text, FTexture::TEX_MiscPatch);
		if (mBackground.GetIndex() == -1 || mBackground.GetIndex() == 0)
			mBackground = mUnknownBackgroundTexture;
	}

	FFreeformMenuItemActionableBase::Draw(desc, yoffset, selected);
}

bool FFreeformMenuItemOptionBase::SetString(int i, const char* newtext)
{
	if (i == OP_VALUES)
	{
		FOptionValues** opt = OptionValues.CheckKey(newtext);
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

bool FFreeformMenuItemOptionBase::MenuEvent(int mkey, bool fromcontroller)
{
	FOptionValues** opt = OptionValues.CheckKey(mValues);
	if (opt != NULL && *opt != NULL && (*opt)->mValues.Size() > 0)
	{
		int Selection = GetSelection();
		if (mkey == MKEY_Clear)
		{
			if (Selection == -1) Selection = 0;
			else if (--Selection < 0) Selection = (*opt)->mValues.Size() - 1;
		}
		else if (mkey == MKEY_Enter)
		{
			if (++Selection >= (int)(*opt)->mValues.Size()) Selection = 0;
		}
		else
		{
			return FFreeformMenuItem::MenuEvent(mkey, fromcontroller);
		}
		SetSelection(Selection);
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
	}
	return true;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemOption::Clone()
{
	FFreeformMenuItemOption* clone = new FFreeformMenuItemOption();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuItemOption::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemOption* theOther = static_cast<FFreeformMenuItemOption*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemOptionBase::CopyTo(other);
		theOther->SetCVar(mCVar != NULL ? mCVar->GetName() : NULL);
		return true;
	}

	return false;
}

bool FFreeformMenuItemOption::IsOptionSelectable(double)
{
	return true;
}

int FFreeformMenuItemOption::GetSelection()
{
	int Selection = -1;
	FOptionValues** opt = OptionValues.CheckKey(mValues);
	if (opt != NULL && *opt != NULL && mCVar != NULL && (*opt)->mValues.Size() > 0)
	{
		if ((*opt)->mValues[0].TextValue.IsEmpty())
		{
			UCVarValue cv = mCVar->GetGenericRep(CVAR_Float);
			for (unsigned i = 0; i < (*opt)->mValues.Size(); i++)
			{
				// [TP]
				if (IsOptionSelectable((*opt)->mValues[i].Value) == false)
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
			for (unsigned i = 0; i < (*opt)->mValues.Size(); i++)
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

void FFreeformMenuItemOption::SetSelection(int Selection)
{
	UCVarValue value;
	FOptionValues** opt = OptionValues.CheckKey(mValues);
	if (opt != NULL && *opt != NULL && mCVar != NULL && (*opt)->mValues.Size() > 0)
	{
		if ((*opt)->mValues[0].TextValue.IsEmpty())
		{
			value.Float = (float)(*opt)->mValues[Selection].Value;
			mCVar->SetGenericRep(value, CVAR_Float);
		}
		else
		{
			value.String = (*opt)->mValues[Selection].TextValue.LockBuffer();
			mCVar->SetGenericRep(value, CVAR_String);
			(*opt)->mValues[Selection].TextValue.UnlockBuffer();
		}
	}
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemControl::Clone()
{
	FFreeformMenuItemControl* clone = new FFreeformMenuItemControl();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuItemControl::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemControl* theOther = static_cast<FFreeformMenuItemControl*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemActionableBase::CopyTo(other);
		return true;
	}

	return false;
}

void FFreeformMenuItemControl::Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected)
{
	char description[64];
	int Key1, Key2;

	mBindings->GetKeysForCommand(mAction, &Key1, &Key2);
	C_NameKeys (description, Key1, Key2);
	if (description[0])
		ReplaceString(&mLabel, description);
	else
		ReplaceString(&mLabel, "---");

	FFreeformMenuItemActionableBase::Draw(desc, yoffset, selected);
}

bool FFreeformMenuItemControl::MenuEvent(int mkey, bool fromcontroller)
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

bool FFreeformMenuItemControl::Activate()
{
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
	mWaiting = true;
	DMenu *input = new DEnterKey(DMenu::CurrentMenu, &mInput);
	M_ActivateMenu(input);
	return true;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuItemColorPicker::Clone()
{
	FFreeformMenuItemColorPicker* clone = new FFreeformMenuItemColorPicker();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuItemColorPicker::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuItemColorPicker* theOther = static_cast<FFreeformMenuItemColorPicker*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemActionableBase::CopyTo(other);
		theOther->SetCVar(mCVar != NULL ? mCVar->GetName() : NULL);
		return true;
	}

	return false;
}

void FFreeformMenuItemColorPicker::Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected)
{
	if (mCVar != NULL)
	{
		int x, y;
		int width = GetWidth();
		int height = GetHeight();
		CalcDrawScreenPos(x, y, width, height, false);
		if (!mIsStatic)
			y += yoffset;

		screen->Clear (x, y, x + width * CleanXfac_1, y + height * CleanYfac_1,
			-1, (uint32)*mCVar | 0xff000000);

		FFreeformMenuItemActionableBase::Draw(desc, yoffset, selected);
	}
}

bool FFreeformMenuItemColorPicker::SetValue(int i, int v)
{
	if (i == CPF_RESET && mCVar != NULL)
	{
		mCVar->ResetToDefault();
		return true;
	}
	return false;
}

bool FFreeformMenuItemColorPicker::Activate()
{
	if (mCVar != NULL)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		DMenu *picker = StartPickerMenu(DMenu::CurrentMenu, NULL, mCVar);
		if (picker != NULL)
		{
			M_ActivateMenu(picker);
			return true;
		}
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuFieldBase::Clone()
{
	return NULL; // you are not supposed to clone base class
}

bool FFreeformMenuFieldBase::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuFieldBase* theOther = static_cast<FFreeformMenuFieldBase*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemActionableBase::CopyTo(other);
		theOther->SetCVar(mCVar != NULL ? mCVar->GetName() : NULL);
		return true;
	}

	return false;
}

const char* FFreeformMenuFieldBase::GetCVarString()
{
	if (mCVar == NULL)
		return "";

	return mCVar->GetGenericRep(CVAR_String).String;
}

FString FFreeformMenuFieldBase::Represent()
{
	return GetCVarString();
}

void FFreeformMenuFieldBase::Draw(FFreeformMenuDescriptor *desc, int yoffset, bool selected)
{
	ReplaceString(&mLabel, Represent().GetChars());
	FFreeformMenuItemActionableBase::Draw(desc, yoffset, selected);
}

bool FFreeformMenuFieldBase::GetString(int i, char* s, int len)
{
	if (i == 0)
	{
		strncpy(s, GetCVarString(), len);
		s[len - 1] = '\0';
		return true;
	}

	return false;
}

bool FFreeformMenuFieldBase::SetString(int i, const char* s)
{
	if (i == 0)
	{
		if (mCVar)
		{
			UCVarValue vval;
			vval.String = s;
			mCVar->SetGenericRep(vval, CVAR_String);
		}

		return true;
	}

	return false;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuTextField::Clone()
{
	FFreeformMenuTextField* clone = new FFreeformMenuTextField();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuTextField::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuTextField* theOther = static_cast<FFreeformMenuTextField*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuFieldBase::CopyTo(other);
		return true;
	}

	return false;
}

FString FFreeformMenuTextField::Represent()
{
	FString text = mEntering ? mEditName : GetCVarString();

	if (mEntering)
		text += (gameinfo.gametype & GAME_DoomStrifeChex) ? '_' : '[';

	return text;
}

bool FFreeformMenuTextField::MenuEvent(int mkey, bool fromcontroller)
{
	if (mkey == MKEY_Enter)
	{
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		strcpy(mEditName, GetCVarString());
		mEntering = true;
		DMenu* input = new DTextEnterMenu(DMenu::CurrentMenu, mEditName, sizeof mEditName, 2, fromcontroller);
		M_ActivateMenu(input);
		return true;
	}
	else if (mkey == MKEY_Input)
	{
		if (mCVar)
		{
			UCVarValue vval;
			vval.String = mEditName;
			mCVar->SetGenericRep(vval, CVAR_String);
		}

		mEntering = false;
		return true;
	}
	else if (mkey == MKEY_Abort)
	{
		mEntering = false;
		return true;
	}

	return FFreeformMenuItem::MenuEvent(mkey, fromcontroller);
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuNumberField::Clone()
{
	FFreeformMenuNumberField* clone = new FFreeformMenuNumberField();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuNumberField::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuNumberField* theOther = static_cast<FFreeformMenuNumberField*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItemActionableBase::CopyTo(other);
		theOther->SetRange(mMinimum, mMaximum, mStep);
		return true;
	}

	return false;
}

bool FFreeformMenuNumberField::MenuEvent(int mkey, bool fromcontroller)
{
	if (mCVar)
	{
		float value = mCVar->GetGenericRep(CVAR_Float).Float;

		if (mkey == MKEY_Clear)
		{
			value -= mStep;

			if (value < mMinimum)
				value = mMaximum;
		}
		else if (mkey == MKEY_Enter)
		{
			value += mStep;

			if (value > mMaximum)
				value = mMinimum;
		}
		else
			return FFreeformMenuItem::MenuEvent(mkey, fromcontroller);

		UCVarValue vval;
		vval.Float = value;
		mCVar->SetGenericRep(vval, CVAR_Float);
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
	}

	return true;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuCustomOptionField::Clone()
{
	return NULL; // you are not supposed to clone base class
}

bool FFreeformMenuCustomOptionField::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuCustomOptionField* theOther = static_cast<FFreeformMenuCustomOptionField*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuFieldBase::CopyTo(other);
		return true;
	}

	return false;
}

// [TP] Seeks the value forwards or backwards. Returns true if it was possible to find a new
// player. Can return false if there are no players (not in a game)
bool FFreeformMenuCustomOptionField::Seek ( bool forward )
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

bool FFreeformMenuCustomOptionField::MenuEvent ( int mkey, bool fromcontroller )
{
	if ( mkey == MKEY_Left || mkey == MKEY_Right || mkey == MKEY_Enter )
	{
		if ( Seek( mkey != MKEY_Left ))
			S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE );

		return true;
	}

	return FFreeformMenuItem::MenuEvent( mkey, fromcontroller );
}

FString FFreeformMenuCustomOptionField::Represent()
{
	if ( mCVar )
	{
		int i = mCVar->GetGenericRep( CVAR_Int ).Int;

		if ( IsValid( i ))
			return RepresentOption( i );
	}

	return "Unknown";
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuPlayerField::Clone()
{
	FFreeformMenuPlayerField* clone = new FFreeformMenuPlayerField();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuPlayerField::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuPlayerField* theOther = static_cast<FFreeformMenuPlayerField*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuCustomOptionField::CopyTo(other);
		return true;
	}

	return false;
}

bool FFreeformMenuPlayerField::IsValid ( int player )
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

FString FFreeformMenuPlayerField::RepresentOption ( int playerId )
{
	return players[playerId].userinfo.GetName();
}

int FFreeformMenuPlayerField::Maximum()
{
	return MAXPLAYERS;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuTeamField::Clone()
{
	FFreeformMenuTeamField* clone = new FFreeformMenuTeamField();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuTeamField::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuTeamField* theOther = static_cast<FFreeformMenuTeamField*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuCustomOptionField::CopyTo(other);
		return true;
	}

	return false;
}

bool FFreeformMenuTeamField::IsValid ( int teamId )
{
	return TEAM_CheckIfValid( teamId ) || ( unsigned ( teamId ) == teams.Size() );
}

FString FFreeformMenuTeamField::RepresentOption ( int teamId )
{
	if ( static_cast<unsigned>( teamId ) >= teams.Size() )
		return "Random";

	FString result = "\\c";
	result += teams[teamId].TextColor;
	result += teams[teamId].Name;
	V_ColorizeString( result );
	return result;
}

int FFreeformMenuTeamField::Maximum()
{
	return teams.Size() + 1;
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuPlayerClassField::Clone()
{
	FFreeformMenuPlayerClassField* clone = new FFreeformMenuPlayerClassField();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuPlayerClassField::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuPlayerClassField* theOther = static_cast<FFreeformMenuPlayerClassField*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuCustomOptionField::CopyTo(other);
		return true;
	}

	return false;
}

bool FFreeformMenuPlayerClassField::IsValid ( int classId )
{
	// [BB] The random class is always a valid choice.
	if ( classId == ( Maximum() - 1 ) )
		return true;

	// [EP] Temporary spots require no other limitation.
	if ( !TEAM_ShouldJoinTeam() )
		return true;

	return TEAM_IsClassAllowedForTeam( classId, menu_jointeamidx );
}

int FFreeformMenuPlayerClassField::Maximum()
{
	return PlayerClasses.Size() + 1;
}

FString FFreeformMenuPlayerClassField::RepresentOption ( int classId )
{
	if ( classId == ( Maximum() - 1 ) )
		return "Random";
	else
		return GetPrintableDisplayName( PlayerClasses[classId].Type );
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuSliderBase::Clone()
{
	return NULL; // you are not supposed to clone base class
}

bool FFreeformMenuSliderBase::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuSliderBase* theOther = static_cast<FFreeformMenuSliderBase*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuItem::CopyTo(other);
		theOther->SetRange(mMinimum, mMaximum, mStep);
		theOther->SetGrayCheck(mGrayCheck != NULL ? mGrayCheck->GetName() : NULL, mGrayCheckMin, mGrayCheckMax);
		theOther->SetTextures(mBackground, mSlider, mSliderSelected, mSliderWidth, mSliderHeight);
		theOther->SetSelectable(mSelectable);
		return true;
	}

	return false;
}


bool FFreeformMenuSliderBase::Selectable()
{
	return mSelectable && IsVisible() && IsEnabled();
}

bool FFreeformMenuSliderBase::IsEnabled()
{
	int cvarValue = mGrayCheck != NULL ? mGrayCheck->GetGenericRep(CVAR_Int).Int : 1;
	return mEnabled && (cvarValue >= mGrayCheckMin) && (cvarValue <= mGrayCheckMax);
}

//=============================================================================
void FFreeformMenuSliderBase::Draw(FFreeformMenuDescriptor* desc, int yoffset, bool selected)
{
	int overlay = IsEnabled() ? 0 : MAKEARGB(96, 48, 0, 0);

	float range = mMaximum - mMinimum;
	float ccur = clamp(GetSliderValue(), mMinimum, mMaximum) - mMinimum;

	if (mBackground.isValid() && mSlider.isValid())
	{
		FTexture* tex = TexMan(mBackground);
		FTexture* slider;
		if (selected && mSliderSelected.GetIndex() != -1 && mSliderSelected.GetIndex() != 0)
			slider = TexMan(mSliderSelected);
		else
			slider = TexMan(mSlider);

		int x, y;
		int width = GetWidth() >= 0 ? GetWidth() : tex->GetScaledWidth();
		int height = GetHeight() >= 0 ? GetHeight() : tex->GetScaledHeight();
		CalcDrawScreenPos(x, y, width, height, false);
		if (!mIsStatic)
			y += yoffset;
		screen->DrawTexture(tex, x, y,
			DTA_CleanNoMove_1, true,
			DTA_DestWidth, width * CleanXfac_1,
			DTA_DestHeight, height * CleanYfac_1,
			DTA_ColorOverlay, overlay,
			DTA_Alpha, mAlpha,
			TAG_DONE);

		x += (int) ((width - mSliderWidth) * CleanXfac_1 * ccur / range);
		y -= (mSliderHeight - height) * CleanYfac_1 / 2;
		screen->DrawTexture(slider, x, y,
			DTA_CleanNoMove_1, true,
			DTA_DestWidth, mSliderWidth * CleanXfac_1,
			DTA_DestHeight, mSliderHeight * CleanYfac_1,
			DTA_ColorOverlay, overlay,
			DTA_Alpha, mAlpha,
			TAG_DONE);
	}
	else
	{
		int x, y;
		CalcDrawScreenPos(x, y, ConFont->StringWidth(mSliderText), ConFont->GetHeight(), false);

		if (!mIsStatic)
			y += yoffset;

		screen->DrawText(ConFont, CR_WHITE, x, y, mSliderText,
			DTA_CellX, 8 * CleanXfac_1,
			DTA_CellY, 8 * CleanYfac_1,
			DTA_ColorOverlay, overlay,
			DTA_Alpha, mAlpha,
			TAG_DONE);
		screen->DrawText(ConFont, CR_ORANGE, x + int((5 + ((ccur * 78) / range)) * CleanXfac_1), y, "\x13",
			DTA_CellX, 8 * CleanXfac_1,
			DTA_CellY, 8 * CleanYfac_1,
			DTA_ColorOverlay, overlay,
			DTA_Alpha, mAlpha,
			TAG_DONE);
	}
}

//=============================================================================
bool FFreeformMenuSliderBase::MenuEvent (int mkey, bool fromcontroller)
{
	float value = GetSliderValue();

	if (mkey == MKEY_Clear)
	{
		value -= mStep;
	}
	else if (mkey == MKEY_Enter)
	{
		value += mStep;
	}
	else
	{
		return FFreeformMenuItem::MenuEvent(mkey, fromcontroller);
	}
	SetSliderValue(clamp(value, mMinimum, mMaximum));
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
	return true;
}

bool FFreeformMenuSliderBase::MouseEvent(int type, int x, int y)
{
	DFreeformMenu *lm = static_cast<DFreeformMenu*>(DMenu::CurrentMenu);
	if (type != DMenu::MOUSE_Click)
	{
		if (!lm->CheckFocus(this)) return false;
	}
	if (type == DMenu::MOUSE_Release)
	{
		lm->ReleaseFocus();
	}

	int slide_left;
	int slide_right;

	if (mBackground.isValid() && mSlider.isValid())
	{
		FTexture* tex = TexMan(mBackground);
		int y; // unused, needed as parameter for CalDrawScreenPos
		int width = GetWidth() >= 0 ? GetWidth() : tex->GetScaledWidth();
		int height = GetHeight() >= 0 ? GetHeight() : tex->GetScaledHeight();
		CalcDrawScreenPos(slide_left, y, width, height, false);
		slide_left += mSliderWidth * CleanXfac_1 / 2;
		slide_right = slide_left + width * CleanXfac_1 - mSliderWidth * CleanXfac_1;
	}
	else
	{
		int y; // unused, needed as parameter for CalDrawScreenPos
		CalcDrawScreenPos(slide_left, y, ConFont->StringWidth(mSliderText), ConFont->GetHeight(), false);
		slide_left += 8 * CleanXfac_1;
		slide_right = slide_left + 10 * 8 * CleanXfac_1;	// 12 char cells with 8 pixels each.
	}

	x = clamp(x, slide_left, slide_right);
	float v = mMinimum + ((x - slide_left) * (mMaximum - mMinimum)) / (slide_right - slide_left);
	if (v != GetSliderValue())
	{
		SetSliderValue(round(v / mStep) * mStep);
		//S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
	}
	if (type == DMenu::MOUSE_Click)
	{
		lm->SetFocus(this);
	}
	return true;
}

bool FFreeformMenuSliderBase::SetValue( int i, int value )
{
	if ( i == SLIDER_MAXIMUM )
	{
		mMaximum = (float) value;
		return true;
	}
	else
	{
		return false;
	}
}


//=============================================================================
//
//
//
//=============================================================================

FFreeformMenuItem* FFreeformMenuSliderCVar::Clone()
{
	FFreeformMenuSliderCVar* clone = new FFreeformMenuSliderCVar();
	CopyTo(clone);
	return clone;
}

bool FFreeformMenuSliderCVar::CopyTo(FFreeformMenuItem* other)
{
	FFreeformMenuSliderCVar* theOther = static_cast<FFreeformMenuSliderCVar*>(other);
	if (theOther != NULL)
	{
		FFreeformMenuSliderBase::CopyTo(other);
		theOther->SetCVar(mCVar != NULL ? mCVar->GetName() : NULL);
		return true;
	}

	return false;
}

float FFreeformMenuSliderCVar::GetSliderValue()
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

void FFreeformMenuSliderCVar::SetSliderValue(float val)
{
	if (mCVar != NULL)
	{
		UCVarValue value;
		value.Float = (float)val;
		mCVar->SetGenericRep(value, CVAR_Float);
	}
}

bool FFreeformMenuSliderCVar::IsServerInfo() const
{
	return mCVar && mCVar->IsServerInfo();
}