﻿// sktoolslib - common files for SK tools

// Copyright (C) 2017, 2020-2021 Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#pragma once
#include "BaseWindow.h"
#include "AnimationManager.h"
#include <string>
#include <vector>
#include <functional>

/**
 * defines a status bar part with all the properties it can have.
 *
 * The text is a custom rich text format. The '%' char serves as a
 * command token. To actually write a '%' you need to put '%%' in
 * the string.
 *
 * the commands available are:
 * %i : changes the font to italic
 * %b : changes the font to bold
 * %cRRGGBB : changes the color of the font to the RGB value specified in HEX
 * %r : resets the text to default (font and color)
 * example:
 * "normal text %i italic %b italic bold %r%b only bold %cFF0000 red text"
 */
class CRichStatusBarItem
{
public:
    /// text to show for the part.
    std::wstring text      = L"";
    bool         useEditor = false;
    /// text to show if the width of the part is smaller than requested.
    /// if not set, the default text is used and cropped
    ///std::wstring shortText;
    /// 0 : left align
    /// 1 : center
    /// 2 : right align
    int align = 0;
    /// text for the tooltip of the part
    std::wstring tooltip = L"";
    /// icon to show. When the text is left-aligned or centered, the icon is shown to the left,
    /// when the text is right-aligned, the icon is shown right of the text.
    /// note: the icon will be shown with the same width and height.
    HICON icon = nullptr;
    /// the requested width of the part, in pixels. If set to 0, the width is calculated
    /// at runtime from the text and icon. A negative value is used as padding to the
    /// calculated width.
    int width = 0;
    /// if \b fixedWidth is set to true, then this sets the width for the short text
    ///int shortWidth;
    /// determines whether the part can be resized with the main window
    bool fixedWidth = false;
    /// if set to true, the part indicates when the mouse pointer hovers over it.
    /// can be used to indicate that a click/right-click does something
    bool hoverActive = false;
    /// icon to show if the width is too small for text.
    /// if not set, the text is shown cropped
    HICON collapsedIcon = nullptr;
    int  fontIdx = 0;
    bool  isChecked = false;
    int   type = 0; //0, text; 1, font icon; 2, image icon
};

/**
 * internal struct. do not use!
 */
struct PartWidths
{
    int  defaultWidth    = 0;
    bool fixed           = false;
    int  calculatedWidth = 0;
};

/**
 * a custom status bar control
 * Note: you have to define the message WM_STATUSBAR_MSG somewhere for this to work.
 * E.g.:
 * #define WM_STATUSBAR_MSG (WM_APP + 1)
 * it needs to be an WM_APP message!
 */
class CRichStatusBar : public CWindow
{
public:
    CRichStatusBar(HINSTANCE hInst);
    virtual ~CRichStatusBar();

    bool Init(HWND hParent, bool drawGrip);

    /// Sets/Updates or inserts a part.
    /// \param index the index to update. If the index does not exist, the function returns false.
    ///              if the index is set to -1, the part is inserted at the end.
    /// \param item  the item to set/insert
    /// \param redraw redraws the status bar after setting/inserting the part. Set to false while initializing.
    /// \param replace if true, the index must exist or be set to -1. if set to false,
    ///               the item is inserted before the index
    bool SetPart(int index, const CRichStatusBarItem& item, bool redraw, bool replace = true);
    bool SetPart(int fontIdx, int index, const std::wstring& text, const std::wstring& shortText, const std::wstring& tooltip, int width, int shortWidth, int align = 0, bool fixedWidth = false, bool hover = false, HICON icon = nullptr, HICON collapsedIcon = nullptr);
    /// returns the recommended height of the status bar
    int GetHeight() const { return m_height; }
    /// calculates the widths of all parts and updates the status bar.
    /// call this after changing parts or inserting new ones
    void CalcWidths();
    int GetWidthToLeft(int idx); // client coordinate
    /// sets a callback function that takes a COLORREF and returns a (modified) COLORREF.
    /// useful if you want the color to change depending on a selected theme.
    //void SetHandlerFunc(std::function<COLORREF(const COLORREF&)> themeColor) { m_themeColorFunc = themeColor; }
    /// returns the index of the part at the specified client coordinates
    int GetPartIndexAt(const POINT& pt) const;
    /// returns a plain string without the formatting chars
    static std::wstring GetPlainString(const std::wstring& text);

    void SetPartState(int idx, bool isChecked) { m_parts[idx].isChecked = isChecked; }

protected:
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    void             CalcRequestedWidths(int index);
    void             DrawRichText(int fontIdx, HDC hdc, const std::wstring& text, RECT& rect, UINT flags);

private:
    std::vector<CRichStatusBarItem>  m_parts;
    std::vector<PartWidths>          m_partWidths;
    HFONT                            m_fonts[5];
    HWND                             m_tooltip;
    int                              m_hoverPart;
    int                              m_height;
    bool                             m_drawGrip;
    std::vector<AnimationVariable>   m_animVars;
};
