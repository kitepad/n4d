﻿// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2020-2021 - Stefan Kueng
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// See <http://www.gnu.org/licenses/> for a copy of the full license text
//
#include "stdafx.h"
#include "Theme.h"
#include "AppUtils.h"
#include "GDIHelpers.h"
#include "DarkModeHelper.h"
#include "DPIAware.h"
#include "SmartHandle.h"
#include <Uxtheme.h>
#include <vssym32.h>
#include <richedit.h>
#pragma warning(push)
#pragma warning(disable : 4458) // declaration of 'xxx' hides class member
#include <gdiplus.h>
#pragma warning(pop)
#include <algorithm>

//constexpr COLORREF darkBkColor           = 0x26211c;
//constexpr COLORREF darkTextColor         = 0xDDDDDD;
constexpr COLORREF darkDisabledTextColor = 0x808080;

constexpr auto SubclassID = 1234;

HBRUSH CTheme::m_sBackBrush = nullptr;

static int  GetStateFromBtnState(LONG_PTR dwStyle, BOOL bHot, BOOL bFocus, LRESULT dwCheckState, int iPartId, BOOL bHasMouseCapture);
static void GetRoundRectPath(Gdiplus::GraphicsPath* pPath, const Gdiplus::Rect& r, int dia);
static void DrawRect(LPRECT prc, HDC hdcPaint, Gdiplus::DashStyle dashStyle, Gdiplus::Color clr, Gdiplus::REAL width);
static void DrawFocusRect(LPRECT prcFocus, HDC hdcPaint);
static void PaintControl(HWND hWnd, HDC hdc, RECT* prc, bool bDrawBorder);
static BOOL DetermineGlowSize(int* piSize, LPCWSTR pszClassIdList = nullptr);
static BOOL GetEditBorderColor(HWND hWnd, COLORREF* pClr);

static THEME darkTheme  = {RGB(0x10, 0x10, 0x10), RGB(0xEF, 0xEF, 0xEF), RGB(0x10, 0x10, 0x10), 
    RGB(0xA6, 0xAB, 0xAF), RGB(0x32, 0x32, 0x32), RGB(0xFF, 0xFF, 0xFF), RGB(0xEF, 0xEF, 0xEF), 
    RGB(0x6C, 0x71, 0x76), RGB(0x30, 0x60, 0xE8), RGB(0x32, 0x32, 0x32), L"Tahoma", 13, 32, 36};
static THEME lightTheme = {RGB(0xF0, 0xF0, 0xF0), RGB(0x10, 0x10, 0x10), RGB(0xF0, 0xF0, 0xF0), 
    RGB(0x10, 0x10, 0x10), RGB(0x80, 0x80, 0x80), RGB(0xDF, 0xDF, 0xDF), RGB(0x40, 0x40, 0x40), 
    RGB(0x8F, 0x8F, 0x8F), RGB(0x40, 0x40, 0xF0), RGB(0xAD, 0xAD, 0xAD), L"Tahoma", 13, 32, 36};

THEME CTheme::CurrentTheme()
{
    return Instance().m_dark ? darkTheme : lightTheme;
}

CTheme::CTheme()
    : m_bLoaded(false)
    , m_isHighContrastMode(false)
    , m_isHighContrastModeDark(false)
    , m_dark(false)
    , m_lastThemeChangeCallbackId(0)
{
}

CTheme::~CTheme()
{
}

CTheme& CTheme::Instance()
{
    static CTheme instance;
    if (!instance.m_bLoaded)
        instance.Load();

    return instance;
}

void CTheme::Load()
{
    CSimpleIni themeIni;

    DWORD       resLen  = 0;
    const char* resData = CAppUtils::GetResourceData(L"config", IDR_DARKTHEME, resLen);
    if (resData != nullptr)
    {
        themeIni.LoadFile(resData, resLen);
    }

    CSimpleIni::TNamesDepend colors;
    themeIni.GetAllKeys(L"SubstColors", colors);

    for (const auto& it : colors)
    {
        COLORREF     clr1;
        std::wstring s  = it;
        bool         ok = GDIHelpers::HexStringToCOLORREF(s, &clr1);
        //APPVERIFY(ok);

        COLORREF clr2;
        s  = themeIni.GetValue(L"SubstColors", it, L"");
        ok = GDIHelpers::HexStringToCOLORREF(s, &clr2);
        //APPVERIFY(ok);

        m_colorMap[clr1] = clr2;
    }

    OnSysColorChanged();

    m_bLoaded = true;
}

void CTheme::OnSysColorChanged()
{
    m_isHighContrastModeDark = false;
    m_isHighContrastMode     = false;
    HIGHCONTRAST hc          = {sizeof(HIGHCONTRAST)};
    SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRAST), &hc, FALSE);
    if ((hc.dwFlags & HCF_HIGHCONTRASTON) != 0)
    {
        m_isHighContrastMode = true;
        // check if the high contrast mode is dark
        float h1, h2, s1, s2, l1, l2;
        GDIHelpers::RGBtoHSL(::GetSysColor(COLOR_WINDOWTEXT), h1, s1, l1);
        GDIHelpers::RGBtoHSL(::GetSysColor(COLOR_WINDOW), h2, s2, l2);
        m_isHighContrastModeDark = l2 < l1;
    }
    m_dark = GetInt64(DEFAULTS_SECTION, L"DarkTheme") != 0 && !IsHighContrastMode();
}

bool CTheme::IsHighContrastMode() const
{
    return m_isHighContrastMode;
}

bool CTheme::IsHighContrastModeDark() const
{
    return m_isHighContrastModeDark;
}

COLORREF CTheme::GetThemeColor(COLORREF clr, bool fixed /*= false*/) const
{
    if (m_dark || (fixed && m_isHighContrastModeDark))
    {
        auto cIt = m_colorMap.find(clr);
        if (cIt != m_colorMap.end())
            return cIt->second;

        float h, s, l;
        GDIHelpers::RGBtoHSL(clr, h, s, l);
        l = 100.0f - l;
        if (!m_isHighContrastModeDark)
        {
            // to avoid too much contrast, prevent
            // too dark and too bright colors.
            // this is because in dark mode, contrast is
            // much more visible.
            l = std::clamp(l, 5.0f, 90.0f);
        }
        return GDIHelpers::HSLtoRGB(h, s, l);
    }

    return clr;
}

int CTheme::RegisterThemeChangeCallback(ThemeChangeCallback&& cb)
{
    ++m_lastThemeChangeCallbackId;
    int nextThemeCallBackId = m_lastThemeChangeCallbackId;
    m_themeChangeCallbacks.emplace(nextThemeCallBackId, std::move(cb));
    return nextThemeCallBackId;
}

bool CTheme::RemoveRegisteredCallback(int id)
{
    auto foundIt = m_themeChangeCallbacks.find(id);
    if (foundIt != m_themeChangeCallbacks.end())
    {
        m_themeChangeCallbacks.erase(foundIt);
        return true;
    }
    return false;
}

void CTheme::SetDarkTheme(bool b /*= true*/)
{
    if (IsHighContrastMode())
        return;
    m_dark = b;
    SetInt64(DEFAULTS_SECTION, L"DarktTeme", b ? 1 : 0);
    for (auto& [id, callBack] : m_themeChangeCallbacks)
        callBack();
}

bool CTheme::SetThemeForDialog(HWND hWnd, bool bDark)
{
    DarkModeHelper::Instance().AllowDarkModeForWindow(hWnd, bDark);
    if (bDark && !DarkModeHelper::Instance().CanHaveDarkMode())
        return false;
    if (bDark)
    {
        SetWindowSubclass(hWnd, MainSubclassProc, SubclassID, reinterpret_cast<DWORD_PTR>(&m_sBackBrush));
    }
    else
    {
        RemoveWindowSubclass(hWnd, MainSubclassProc, SubclassID);
    }
    EnumThreadWindows(GetCurrentThreadId(), AdjustThemeForChildrenProc, bDark ? TRUE : FALSE);
    EnumChildWindows(hWnd, AdjustThemeForChildrenProc, bDark ? TRUE : FALSE);
    DarkModeHelper::Instance().RefreshTitleBarThemeColor(hWnd, bDark);
    ::RedrawWindow(hWnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW);
    return true;
}

BOOL CTheme::AdjustThemeForChildrenProc(HWND hwnd, LPARAM lParam)
{
    DarkModeHelper::Instance().AllowDarkModeForWindow(hwnd, static_cast<BOOL>(lParam));
    wchar_t szWndClassName[MAX_PATH] = {0};
    GetClassName(hwnd, szWndClassName, _countof(szWndClassName));
    if (wcscmp(szWndClassName, WC_EDIT) == 0)
        return TRUE;

    if (lParam)
    {
        if ((wcscmp(szWndClassName, WC_LISTVIEW) == 0) || (wcscmp(szWndClassName, WC_LISTBOX) == 0))
        {
            // theme "Explorer" also gets the scrollbars with dark mode, but the hover color
            // is the blueish from the bright mode.
            // theme "ItemsView" has the hover color the same as the windows explorer (grayish),
            // but then the scrollbars are not drawn for dark mode.
            // theme "DarkMode_Explorer" doesn't paint a hover color at all.
            //
            // Also, the group headers are not affected in dark mode and therefore the group texts are
            // hardly visible.
            //
            // so use "Explorer" for now. The downside of the bluish hover color isn't that bad,
            // except in situations where both a treeview and a listview are on the same dialog
            // at the same time - then the difference is unfortunately very
            // noticeable...
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            auto header = ListView_GetHeader(hwnd);
            DarkModeHelper::Instance().AllowDarkModeForWindow(header, static_cast<BOOL>(lParam));
            SetWindowTheme(header, L"Explorer", nullptr);
            ListView_SetTextColor(hwnd, CurrentTheme().winFore); // darkTextColor);
            ListView_SetTextBkColor(hwnd, CurrentTheme().winBack); // darkBkColor);
            ListView_SetBkColor(hwnd, CurrentTheme().winBack);     // darkBkColor);
            auto hTT = ListView_GetToolTips(hwnd);
            if (hTT)
            {
                DarkModeHelper::Instance().AllowDarkModeForWindow(hTT, static_cast<BOOL>(lParam));
                SetWindowTheme(hTT, L"Explorer", nullptr);
            }
            SetWindowSubclass(hwnd, ListViewSubclassProc, SubclassID, reinterpret_cast<DWORD_PTR>(&m_sBackBrush));
        }
        else if (wcscmp(szWndClassName, WC_HEADER) == 0)
        {
            SetWindowTheme(hwnd, L"ItemsView", nullptr);
        }
        else if (wcscmp(szWndClassName, WC_BUTTON) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            auto style = GetWindowLongPtr(hwnd, GWL_STYLE) & 0x0F;
            if ((style & BS_GROUPBOX) == BS_GROUPBOX)
            {
                SetWindowSubclass(hwnd, ButtonSubclassProc, SubclassID, reinterpret_cast<DWORD_PTR>(&m_sBackBrush));
            }
            else if (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX || style == BS_3STATE || style == BS_AUTO3STATE || style == BS_RADIOBUTTON || style == BS_AUTORADIOBUTTON)
            {
                SetWindowSubclass(hwnd, ButtonSubclassProc, SubclassID, reinterpret_cast<DWORD_PTR>(&m_sBackBrush));
            }
        }
        else if (wcscmp(szWndClassName, WC_STATIC) == 0)
        {
            SetWindowTheme(hwnd, L"", L"");
        }
        else if (wcscmp(szWndClassName, L"SysDateTimePick32") == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
        }
        else if ((wcscmp(szWndClassName, WC_COMBOBOXEX) == 0) ||
                 (wcscmp(szWndClassName, L"ComboLBox") == 0) ||
                 (wcscmp(szWndClassName, WC_COMBOBOX) == 0))
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            HWND hCombo = hwnd;
            if (wcscmp(szWndClassName, WC_COMBOBOXEX) == 0)
            {
                SendMessage(hwnd, CBEM_SETWINDOWTHEME, 0, reinterpret_cast<LPARAM>(L"Explorer"));
                hCombo = reinterpret_cast<HWND>(SendMessage(hwnd, CBEM_GETCOMBOCONTROL, 0, 0));
            }
            if (hCombo)
            {
                SetWindowSubclass(hCombo, ComboBoxSubclassProc, SubclassID, reinterpret_cast<DWORD_PTR>(&m_sBackBrush));
                COMBOBOXINFO info = {0};
                info.cbSize       = sizeof(COMBOBOXINFO);
                if (SendMessage(hCombo, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(&info)))
                {
                    DarkModeHelper::Instance().AllowDarkModeForWindow(info.hwndList, static_cast<BOOL>(lParam));
                    DarkModeHelper::Instance().AllowDarkModeForWindow(info.hwndItem, static_cast<BOOL>(lParam));
                    DarkModeHelper::Instance().AllowDarkModeForWindow(info.hwndCombo, static_cast<BOOL>(lParam));

                    SetWindowTheme(info.hwndList, L"Explorer", nullptr);
                    SetWindowTheme(info.hwndItem, L"Explorer", nullptr);
                    SetWindowTheme(info.hwndCombo, L"CFD", nullptr);
                }
            }
        }
        else if (wcscmp(szWndClassName, WC_TREEVIEW) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            TreeView_SetTextColor(hwnd, CurrentTheme().winFore); // darkTextColor);
            TreeView_SetBkColor(hwnd, CurrentTheme().winBack);   // darkBkColor);
            auto hTT = TreeView_GetToolTips(hwnd);
            if (hTT)
            {
                DarkModeHelper::Instance().AllowDarkModeForWindow(hTT, static_cast<BOOL>(lParam));
                SetWindowTheme(hTT, L"Explorer", nullptr);
            }
        }
        else if (wcsncmp(szWndClassName, L"RICHEDIT", 8) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            CHARFORMAT2 format = {0};
            format.cbSize      = sizeof(CHARFORMAT2);
            format.dwMask      = CFM_COLOR | CFM_BACKCOLOR;
            format.crTextColor = CTheme::CurrentTheme().winFore;//darkTextColor;
            format.crBackColor = CTheme::CurrentTheme().winBack;//darkBkColor;
            SendMessage(hwnd, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
            SendMessage(hwnd, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(format.crBackColor));
        }
        else if (wcscmp(szWndClassName, PROGRESS_CLASS) == 0)
        {
            SetWindowTheme(hwnd, L"", L"");
            SendMessage(hwnd, PBM_SETBKCOLOR, 0, static_cast<LPARAM>(CurrentTheme().winBack));
            SendMessage(hwnd, PBM_SETBARCOLOR, 0, static_cast<LPARAM>(RGB(50, 50, 180)));
        }
        else if (wcscmp(szWndClassName, L"Auto-Suggest Dropdown") == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            SetWindowSubclass(hwnd, AutoSuggestSubclassProc, SubclassID, reinterpret_cast<DWORD_PTR>(&m_sBackBrush));
            EnumChildWindows(hwnd, AdjustThemeForChildrenProc, lParam);
        }
        else if (wcscmp(szWndClassName, TOOLTIPS_CLASSW) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
        }
        else if (FAILED(SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr)))
            SetWindowTheme(hwnd, L"Explorer", nullptr);
    }
    else
    {
        if (wcscmp(szWndClassName, WC_LISTVIEW) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            CAutoThemeData hTheme = OpenThemeData(nullptr, L"ItemsView");
            if (hTheme)
            {
                COLORREF color;
                if (SUCCEEDED(::GetThemeColor(hTheme, 0, 0, TMT_TEXTCOLOR, &color)))
                {
                    ListView_SetTextColor(hwnd, color);
                }
                if (SUCCEEDED(::GetThemeColor(hTheme, 0, 0, TMT_FILLCOLOR, &color)))
                {
                    ListView_SetTextBkColor(hwnd, color);
                    ListView_SetBkColor(hwnd, color);
                }
            }
            auto hTT = ListView_GetToolTips(hwnd);
            if (hTT)
            {
                DarkModeHelper::Instance().AllowDarkModeForWindow(hTT, static_cast<BOOL>(lParam));
                SetWindowTheme(hTT, L"Explorer", nullptr);
            }
            RemoveWindowSubclass(hwnd, ListViewSubclassProc, SubclassID);
        }
        else if (wcscmp(szWndClassName, WC_BUTTON) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            RemoveWindowSubclass(hwnd, ButtonSubclassProc, SubclassID);
        }
        else if ((wcscmp(szWndClassName, WC_COMBOBOXEX) == 0) ||
                 (wcscmp(szWndClassName, WC_COMBOBOX) == 0))
        {
            SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
            HWND hCombo = hwnd;
            if (wcscmp(szWndClassName, WC_COMBOBOXEX) == 0)
            {
                SendMessage(hwnd, CBEM_SETWINDOWTHEME, 0, reinterpret_cast<LPARAM>(L"DarkMode_Explorer"));
                hCombo = reinterpret_cast<HWND>(SendMessage(hwnd, CBEM_GETCOMBOCONTROL, 0, 0));
            }
            if (hCombo)
            {
                COMBOBOXINFO info = {0};
                info.cbSize       = sizeof(COMBOBOXINFO);
                if (SendMessage(hCombo, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(&info)))
                {
                    DarkModeHelper::Instance().AllowDarkModeForWindow(info.hwndList, static_cast<BOOL>(lParam));
                    DarkModeHelper::Instance().AllowDarkModeForWindow(info.hwndItem, static_cast<BOOL>(lParam));
                    DarkModeHelper::Instance().AllowDarkModeForWindow(info.hwndCombo, static_cast<BOOL>(lParam));

                    SetWindowTheme(info.hwndList, L"Explorer", nullptr);
                    SetWindowTheme(info.hwndItem, L"Explorer", nullptr);
                    SetWindowTheme(info.hwndCombo, L"Explorer", nullptr);

                    CAutoThemeData hTheme = OpenThemeData(nullptr, L"ItemsView");
                    if (hTheme)
                    {
                        COLORREF color;
                        if (SUCCEEDED(::GetThemeColor(hTheme, 0, 0, TMT_TEXTCOLOR, &color)))
                        {
                            ListView_SetTextColor(info.hwndList, color);
                        }
                        if (SUCCEEDED(::GetThemeColor(hTheme, 0, 0, TMT_FILLCOLOR, &color)))
                        {
                            ListView_SetTextBkColor(info.hwndList, color);
                            ListView_SetBkColor(info.hwndList, color);
                        }
                    }

                    RemoveWindowSubclass(info.hwndList, ListViewSubclassProc, SubclassID);
                }
                RemoveWindowSubclass(hCombo, ComboBoxSubclassProc, SubclassID);
            }
        }
        else if (wcscmp(szWndClassName, WC_TREEVIEW) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            CAutoThemeData hTheme = OpenThemeData(nullptr, L"ItemsView");
            if (hTheme)
            {
                COLORREF color;
                if (SUCCEEDED(::GetThemeColor(hTheme, 0, 0, TMT_TEXTCOLOR, &color)))
                {
                    TreeView_SetTextColor(hwnd, color);
                }
                if (SUCCEEDED(::GetThemeColor(hTheme, 0, 0, TMT_FILLCOLOR, &color)))
                {
                    TreeView_SetBkColor(hwnd, color);
                }
            }
            auto hTT = TreeView_GetToolTips(hwnd);
            if (hTT)
            {
                DarkModeHelper::Instance().AllowDarkModeForWindow(hTT, static_cast<BOOL>(lParam));
                SetWindowTheme(hTT, L"Explorer", nullptr);
            }
        }
        else if (wcsncmp(szWndClassName, L"RICHEDIT", 8) == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            CHARFORMAT2 format = {0};
            format.cbSize      = sizeof(CHARFORMAT2);
            format.dwMask      = CFM_COLOR | CFM_BACKCOLOR;
            format.crTextColor = CTheme::CurrentTheme().winFore;//CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT));
            format.crBackColor = CTheme::CurrentTheme().winBack;//CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOW));
            SendMessage(hwnd, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
            SendMessage(hwnd, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(format.crBackColor));
        }
        else if (wcscmp(szWndClassName, PROGRESS_CLASS) == 0)
        {
            SetWindowTheme(hwnd, nullptr, nullptr);
        }
        else if (wcscmp(szWndClassName, PROGRESS_CLASS) == 0)
        {
            SetWindowTheme(hwnd, nullptr, nullptr);
        }
        else if (wcscmp(szWndClassName, L"Auto-Suggest Dropdown") == 0)
        {
            SetWindowTheme(hwnd, L"Explorer", nullptr);
            RemoveWindowSubclass(hwnd, AutoSuggestSubclassProc, SubclassID);
            EnumChildWindows(hwnd, AdjustThemeForChildrenProc, lParam);
        }
        else
            SetWindowTheme(hwnd, L"Explorer", nullptr);
    }
    return TRUE;
}

LRESULT CTheme::ListViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPNMHDR>(lParam)->code == NM_CUSTOMDRAW)
            {
                LPNMCUSTOMDRAW nmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
                switch (nmcd->dwDrawStage)
                {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
                    case CDDS_ITEMPREPAINT:
                    {
                        SetTextColor(nmcd->hdc, CurrentTheme().winFore); // darkTextColor);
                        SetBkMode(nmcd->hdc, TRANSPARENT);
                        return CDRF_DODEFAULT;
                    }
                    default:
                        break;
                }
            }
        }
        break;
        case WM_DESTROY:
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ListViewSubclassProc, SubclassID);
            break;
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CTheme::ComboBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSCROLLBAR:
        {
            auto hbrBkgnd = reinterpret_cast<HBRUSH*>(dwRefData);
            HDC  hdc      = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CurrentTheme().winFore); // darkTextColor);
            SetBkColor(hdc, CurrentTheme().winBack);   // darkBkColor);
            if (!*hbrBkgnd)
                *hbrBkgnd = CreateSolidBrush(CurrentTheme().winBack); // darkBkColor);
            return reinterpret_cast<LRESULT>(*hbrBkgnd);
        }
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT pDis           = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            HDC              hDC            = pDis->hDC;
            RECT             rc             = pDis->rcItem;
            wchar_t          itemText[1024] = {0};

            COMBOBOXEXITEM cbi = {0};
            cbi.mask           = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY | CBEIF_INDENT;
            cbi.iItem          = pDis->itemID;
            cbi.cchTextMax     = _countof(itemText);
            cbi.pszText        = itemText;

            auto cwnd = GetParent(hWnd);

            if (SendMessage(cwnd, CBEM_GETITEM, 0, reinterpret_cast<LPARAM>(&cbi)))
            {
                rc.left += (cbi.iIndent * 10);
                auto img = (pDis->itemState & LVIS_SELECTED) ? cbi.iSelectedImage : cbi.iImage;
                if (pDis->itemState & LVIS_FOCUSED)
                {
                    ::SetBkColor(hDC, darkDisabledTextColor);
                }
                else
                {
                    ::SetBkColor(hDC, CurrentTheme().winBack); // darkBkColor);
                }
                ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);

                if (img)
                {
                    auto imglist = reinterpret_cast<HIMAGELIST>(SendMessage(cwnd, CBEM_GETIMAGELIST, 0, 0));
                    if (imglist)
                    {
                        int iconX(0), iconY(0);
                        ImageList_GetIconSize(imglist, &iconX, &iconY);
                        ImageList_Draw(imglist, img, hDC, rc.left, rc.top, ILD_TRANSPARENT | INDEXTOOVERLAYMASK(cbi.iOverlay));
                        rc.left += (iconX + 2);
                    }
                }

                SetTextColor(pDis->hDC, CurrentTheme().winFore); // darkTextColor);
                SetBkMode(hDC, TRANSPARENT);
                DrawText(hDC, cbi.pszText, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
                return TRUE;
            }
        }
        break;
        case WM_DESTROY:
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ComboBoxSubclassProc, SubclassID);
            break;
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CTheme::MainSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSCROLLBAR:
        {
            auto hbrBkgnd = reinterpret_cast<HBRUSH*>(dwRefData);
            HDC  hdc      = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CurrentTheme().winFore); // darkTextColor);
            SetBkColor(hdc, CurrentTheme().winBack); //  darkBkColor);
            if (!*hbrBkgnd)
                *hbrBkgnd = CreateSolidBrush(CurrentTheme().winBack); // darkBkColor);
            return reinterpret_cast<LRESULT>(*hbrBkgnd);
        }
        case WM_DESTROY:
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, MainSubclassProc, SubclassID);
            break;
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CTheme::AutoSuggestSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT pDis          = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            HDC              hDC           = pDis->hDC;
            RECT             rc            = pDis->rcItem;
            wchar_t          itemText[256] = {};
            // get the text from sub-items
            ListView_GetItemText(pDis->hwndItem, pDis->itemID, 0, itemText, _countof(itemText));

            if (pDis->itemState & LVIS_FOCUSED)
                ::SetBkColor(hDC, darkDisabledTextColor);
            else
                ::SetBkColor(hDC, CurrentTheme().winBack); // darkBkColor);
            ::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);

            SetTextColor(pDis->hDC, CurrentTheme().winFore); // darkTextColor);
            SetBkMode(hDC, TRANSPARENT);
            DrawText(hDC, itemText, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        }
            return TRUE;
        case WM_DESTROY:
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, AutoSuggestSubclassProc, SubclassID);
            break;
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

#ifndef RECTWIDTH
#    define RECTWIDTH(rc) ((rc).right - (rc).left)
#endif

#ifndef RECTHEIGHT
#    define RECTHEIGHT(rc) ((rc).bottom - (rc).top)
#endif

LRESULT CTheme::ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg)
    {
        case WM_SETTEXT:
        case WM_ENABLE:
        case WM_STYLECHANGED:
        {
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            InvalidateRgn(hWnd, nullptr, FALSE);
            return res;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hWnd, &ps);
            if (hdc)
            {
                LONG_PTR dwStyle       = GetWindowLongPtr(hWnd, GWL_STYLE);
                LONG_PTR dwButtonStyle = LOWORD(dwStyle);
                LONG_PTR dwButtonType  = dwButtonStyle & 0xF;
                RECT     rcClient;
                GetClientRect(hWnd, &rcClient);

                if ((dwButtonType & BS_GROUPBOX) == BS_GROUPBOX)
                {
                    CAutoThemeData hTheme = OpenThemeData(hWnd, L"Button");
                    if (hTheme)
                    {
                        BP_PAINTPARAMS params = {sizeof(BP_PAINTPARAMS)};
                        params.dwFlags        = BPPF_ERASE;

                        RECT rcExclusion  = rcClient;
                        params.prcExclude = &rcExclusion;

                        // We have to calculate the exclusion rect and therefore
                        // calculate the font height. We select the control's font
                        // into the DC and fake a drawing operation:
                        HFONT hFontOld = reinterpret_cast<HFONT>(SendMessage(hWnd, WM_GETFONT, 0L, NULL));
                        if (hFontOld)
                            hFontOld = static_cast<HFONT>(SelectObject(hdc, hFontOld));

                        RECT  rcDraw  = rcClient;
                        DWORD dwFlags = DT_SINGLELINE;

                        // we use uppercase A to determine the height of text, so we
                        // can draw the upper line of the groupbox:
                        DrawTextW(hdc, L"A", -1, &rcDraw, dwFlags | DT_CALCRECT);

                        if (hFontOld)
                        {
                            SelectObject(hdc, hFontOld);
                            hFontOld = nullptr;
                        }

                        rcExclusion.left += 2;
                        rcExclusion.top += RECTHEIGHT(rcDraw);
                        rcExclusion.right -= 2;
                        rcExclusion.bottom -= 2;

                        HDC          hdcPaint       = nullptr;
                        HPAINTBUFFER hBufferedPaint = BeginBufferedPaint(hdc, &rcClient, BPBF_TOPDOWNDIB,
                                                                         &params, &hdcPaint);
                        if (hdcPaint)
                        {
                            // now we again retrieve the font, but this time we select it into
                            // the buffered DC:
                            hFontOld = reinterpret_cast<HFONT>(SendMessage(hWnd, WM_GETFONT, 0L, NULL));
                            if (hFontOld)
                                hFontOld = static_cast<HFONT>(SelectObject(hdcPaint, hFontOld));

                            ::SetBkColor(hdcPaint, CurrentTheme().winBack); // darkBkColor);
                            ::ExtTextOut(hdcPaint, 0, 0, ETO_OPAQUE, &rcClient, nullptr, 0, nullptr);

                            BufferedPaintSetAlpha(hBufferedPaint, &ps.rcPaint, 0x00);

                            DTTOPTS dttOpts   = {sizeof(DTTOPTS)};
                            dttOpts.dwFlags   = DTT_COMPOSITED | DTT_GLOWSIZE;
                            dttOpts.crText    = CurrentTheme().winFore; //darkTextColor;
                            dttOpts.iGlowSize = 12; // Default value

                            DetermineGlowSize(&dttOpts.iGlowSize);

                            COLORREF cr = CurrentTheme().winBack; //darkBkColor;
                            GetEditBorderColor(hWnd, &cr);
                            if (CTheme::Instance().IsDarkTheme())
                                cr = CurrentTheme().itemHover;
                            cr |= 0xff000000;

                            auto                  myPen      = std::make_unique<Gdiplus::Pen>(Gdiplus::Color(cr), 1.0f);
                            auto                  myGraphics = std::make_unique<Gdiplus::Graphics>(hdcPaint);
                            int                   iY         = RECTHEIGHT(rcDraw) / 2;
                            Gdiplus::Rect         rr         = Gdiplus::Rect(rcClient.left, rcClient.top + iY,
                                                             RECTWIDTH(rcClient), RECTHEIGHT(rcClient) - iY - 1);
                            Gdiplus::GraphicsPath path;
                            GetRoundRectPath(&path, rr, 5);
                            myGraphics->DrawPath(myPen.get(), &path);
                            myGraphics.reset();
                            myPen.reset();

                            int iLen = GetWindowTextLength(hWnd);

                            if (iLen)
                            {
                                iLen += 5; // 1 for terminating zero, 4 for DT_MODIFYSTRING
                                LPWSTR szText = static_cast<LPWSTR>(LocalAlloc(LPTR, sizeof(WCHAR) * iLen));
                                if (szText)
                                {
                                    iLen = GetWindowTextW(hWnd, szText, iLen);
                                    if (iLen)
                                    {
                                        int iX = RECTWIDTH(rcDraw);
                                        rcDraw = rcClient;
                                        rcDraw.left += iX;
                                        DrawTextW(hdcPaint, szText, -1, &rcDraw, dwFlags | DT_CALCRECT);
                                        ::SetBkColor(hdcPaint, CurrentTheme().winBack); // darkBkColor);
                                        ::ExtTextOut(hdcPaint, 0, 0, ETO_OPAQUE, &rcDraw, nullptr, 0, nullptr);
                                        rcDraw.left++;
                                        rcDraw.right++;

                                        SetBkMode(hdcPaint, TRANSPARENT);
                                        SetTextColor(hdcPaint, CurrentTheme().winFore); // darkTextColor);
                                        DrawText(hdcPaint, szText, -1, &rcDraw, dwFlags);
                                    }

                                    LocalFree(szText);
                                }
                            }

                            if (hFontOld)
                            {
                                SelectObject(hdcPaint, hFontOld);
                                hFontOld = nullptr;
                            }

                            EndBufferedPaint(hBufferedPaint, TRUE);
                        }
                    }
                }

                else if (dwButtonType == BS_CHECKBOX || dwButtonType == BS_AUTOCHECKBOX ||
                         dwButtonType == BS_3STATE || dwButtonType == BS_AUTO3STATE || dwButtonType == BS_RADIOBUTTON || dwButtonType == BS_AUTORADIOBUTTON)
                {
                    CAutoThemeData hTheme = OpenThemeData(hWnd, L"Button");
                    if (hTheme)
                    {
                        HDC            hdcPaint     = nullptr;
                        BP_PAINTPARAMS params       = {sizeof(BP_PAINTPARAMS)};
                        params.dwFlags              = BPPF_ERASE;
                        HPAINTBUFFER hBufferedPaint = BeginBufferedPaint(hdc, &rcClient, BPBF_TOPDOWNDIB, &params, &hdcPaint);
                        if (hdcPaint && hBufferedPaint)
                        {
                            ::SetBkColor(hdcPaint, CurrentTheme().winBack); // darkBkColor);
                            ::ExtTextOut(hdcPaint, 0, 0, ETO_OPAQUE, &rcClient, nullptr, 0, nullptr);

                            BufferedPaintSetAlpha(hBufferedPaint, &ps.rcPaint, 0x00);

                            LRESULT dwCheckState = SendMessage(hWnd, BM_GETCHECK, 0, NULL);
                            POINT   pt;
                            RECT    rc;
                            GetWindowRect(hWnd, &rc);
                            GetCursorPos(&pt);
                            BOOL bHot   = PtInRect(&rc, pt);
                            BOOL bFocus = GetFocus() == hWnd;

                            int iPartId = BP_CHECKBOX;
                            if (dwButtonType == BS_RADIOBUTTON || dwButtonType == BS_AUTORADIOBUTTON)
                                iPartId = BP_RADIOBUTTON;

                            int iState = GetStateFromBtnState(dwStyle, bHot, bFocus, dwCheckState, iPartId, FALSE);

                            int bmWidth = static_cast<int>(ceil(13.0 * CDPIAware::Instance().GetDPI(hWnd) / 96.0));

                            UINT uiHalfWidth = (RECTWIDTH(rcClient) - bmWidth) / 2;

                            // we have to use the whole client area, otherwise we get only partially
                            // drawn areas:
                            RECT rcPaint = rcClient;

                            if (dwButtonStyle & BS_LEFTTEXT)
                            {
                                rcPaint.left += uiHalfWidth;
                                rcPaint.right += uiHalfWidth;
                            }
                            else
                            {
                                rcPaint.left -= uiHalfWidth;
                                rcPaint.right -= uiHalfWidth;
                            }

                            // we assume that bmWidth is both the horizontal and the vertical
                            // dimension of the control bitmap and that it is square. bm.bmHeight
                            // seems to be the height of a striped bitmap because it is an absurdly
                            // high dimension value
                            if ((dwButtonStyle & BS_VCENTER) == BS_VCENTER) /// BS_VCENTER is BS_TOP|BS_BOTTOM
                            {
                                int h          = RECTHEIGHT(rcPaint);
                                rcPaint.top    = (h - bmWidth) / 2;
                                rcPaint.bottom = rcPaint.top + bmWidth;
                            }
                            else if (dwButtonStyle & BS_TOP)
                            {
                                rcPaint.bottom = rcPaint.top + bmWidth;
                            }
                            else if (dwButtonStyle & BS_BOTTOM)
                            {
                                rcPaint.top = rcPaint.bottom - bmWidth;
                            }
                            else // default: center the m_checkbox/radiobutton vertically
                            {
                                int h          = RECTHEIGHT(rcPaint);
                                rcPaint.top    = (h - bmWidth) / 2;
                                rcPaint.bottom = rcPaint.top + bmWidth;
                            }

                            DrawThemeBackground(hTheme, hdcPaint, iPartId, iState, &rcPaint, nullptr);
                            rcPaint = rcClient;

                            GetThemeBackgroundContentRect(hTheme, hdcPaint, iPartId, iState, &rcPaint, &rc);

                            if (dwButtonStyle & BS_LEFTTEXT)
                                rc.right -= bmWidth + 2 * GetSystemMetrics(SM_CXEDGE);
                            else
                                rc.left += bmWidth + 2 * GetSystemMetrics(SM_CXEDGE);

                            DTTOPTS dttOpts   = {sizeof(DTTOPTS)};
                            dttOpts.dwFlags   = DTT_COMPOSITED | DTT_GLOWSIZE;
                            dttOpts.crText    = CurrentTheme().winFore; //darkTextColor;
                            dttOpts.iGlowSize = 12; // Default value

                            DetermineGlowSize(&dttOpts.iGlowSize);

                            HFONT hFontOld = reinterpret_cast<HFONT>(SendMessage(hWnd, WM_GETFONT, 0L, NULL));
                            if (hFontOld)
                                hFontOld = static_cast<HFONT>(SelectObject(hdcPaint, hFontOld));
                            int iLen = GetWindowTextLength(hWnd);

                            if (iLen)
                            {
                                iLen += 5; // 1 for terminating zero, 4 for DT_MODIFYSTRING
                                LPWSTR szText = static_cast<LPWSTR>(LocalAlloc(LPTR, sizeof(WCHAR) * iLen));
                                if (szText)
                                {
                                    iLen = GetWindowTextW(hWnd, szText, iLen);
                                    if (iLen)
                                    {
                                        DWORD dwFlags = DT_SINGLELINE /*|DT_VCENTER*/;
                                        if (dwButtonStyle & BS_MULTILINE)
                                        {
                                            dwFlags |= DT_WORDBREAK;
                                            dwFlags &= ~(DT_SINGLELINE | DT_VCENTER);
                                        }

                                        if ((dwButtonStyle & BS_CENTER) == BS_CENTER) /// BS_CENTER is BS_LEFT|BS_RIGHT
                                            dwFlags |= DT_CENTER;
                                        else if (dwButtonStyle & BS_LEFT)
                                            dwFlags |= DT_LEFT;
                                        else if (dwButtonStyle & BS_RIGHT)
                                            dwFlags |= DT_RIGHT;

                                        if ((dwButtonStyle & BS_VCENTER) == BS_VCENTER) /// BS_VCENTER is BS_TOP|BS_BOTTOM
                                            dwFlags |= DT_VCENTER;
                                        else if (dwButtonStyle & BS_TOP)
                                            dwFlags |= DT_TOP;
                                        else if (dwButtonStyle & BS_BOTTOM)
                                            dwFlags |= DT_BOTTOM;
                                        else
                                            dwFlags |= DT_VCENTER;

                                        if ((dwButtonStyle & BS_MULTILINE) && (dwFlags & DT_VCENTER))
                                        {
                                            // the DT_VCENTER flag only works for DT_SINGLELINE, so
                                            // we have to center the text ourselves here
                                            RECT rcDummy = rc;
                                            int  height  = DrawText(hdcPaint, szText, -1, &rcDummy, dwFlags | DT_WORDBREAK | DT_CALCRECT);
                                            int  centerY = rc.top + (RECTHEIGHT(rc) / 2);
                                            rc.top       = centerY - height / 2;
                                            rc.bottom    = centerY + height / 2;
                                        }
                                        SetBkMode(hdcPaint, TRANSPARENT);
                                        if (dwStyle & WS_DISABLED)
                                            SetTextColor(hdcPaint, darkDisabledTextColor);
                                        else
                                            SetTextColor(hdcPaint, CurrentTheme().winFore); // darkTextColor);
                                        DrawText(hdcPaint, szText, -1, &rc, dwFlags);

                                        // draw the focus rectangle if necessary:
                                        if (bFocus)
                                        {
                                            RECT rcDraw = rc;

                                            DrawTextW(hdcPaint, szText, -1, &rcDraw, dwFlags | DT_CALCRECT);
                                            if (dwFlags & DT_SINGLELINE)
                                            {
                                                dwFlags &= ~DT_VCENTER;
                                                RECT rcDrawTop;
                                                DrawTextW(hdcPaint, szText, -1, &rcDrawTop, dwFlags | DT_CALCRECT);
                                                rcDraw.top = rcDraw.bottom - RECTHEIGHT(rcDrawTop);
                                            }

                                            if (dwFlags & DT_RIGHT)
                                            {
                                                int iWidth   = RECTWIDTH(rcDraw);
                                                rcDraw.right = rc.right;
                                                rcDraw.left  = rcDraw.right - iWidth;
                                            }

                                            RECT rcFocus;
                                            IntersectRect(&rcFocus, &rc, &rcDraw);

                                            DrawFocusRect(&rcFocus, hdcPaint);
                                        }
                                    }
                                    LocalFree(szText);
                                }
                            }

                            if (hFontOld)
                            {
                                SelectObject(hdcPaint, hFontOld);
                                hFontOld = nullptr;
                            }

                            EndBufferedPaint(hBufferedPaint, TRUE);
                        }
                    }
                }
                else if (BS_PUSHBUTTON == dwButtonType || BS_DEFPUSHBUTTON == dwButtonType)
                {
                    // push buttons are drawn properly in dark mode without us doing anything
                    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
                }
                else
                    PaintControl(hWnd, hdc, &ps.rcPaint, false);
            }

            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, ButtonSubclassProc, SubclassID);
            break;
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

int GetStateFromBtnState(LONG_PTR dwStyle, BOOL bHot, BOOL bFocus, LRESULT dwCheckState, int iPartId, BOOL bHasMouseCapture)
{
    int iState = 0;
    switch (iPartId)
    {
        case BP_PUSHBUTTON:
            iState = PBS_NORMAL;
            if (dwStyle & WS_DISABLED)
                iState = PBS_DISABLED;
            else
            {
                if (dwStyle & BS_DEFPUSHBUTTON)
                    iState = PBS_DEFAULTED;

                if (bHasMouseCapture && bHot)
                    iState = PBS_PRESSED;
                else if (bHasMouseCapture || bHot)
                    iState = PBS_HOT;
            }
            break;
        case BP_GROUPBOX:
            iState = (dwStyle & WS_DISABLED) ? GBS_DISABLED : GBS_NORMAL;
            break;

        case BP_RADIOBUTTON:
            iState = RBS_UNCHECKEDNORMAL;
            switch (dwCheckState)
            {
                case BST_CHECKED:
                    if (dwStyle & WS_DISABLED)
                        iState = RBS_CHECKEDDISABLED;
                    else if (bFocus)
                        iState = RBS_CHECKEDPRESSED;
                    else if (bHot)
                        iState = RBS_CHECKEDHOT;
                    else
                        iState = RBS_CHECKEDNORMAL;
                    break;
                case BST_UNCHECKED:
                    if (dwStyle & WS_DISABLED)
                        iState = RBS_UNCHECKEDDISABLED;
                    else if (bFocus)
                        iState = RBS_UNCHECKEDPRESSED;
                    else if (bHot)
                        iState = RBS_UNCHECKEDHOT;
                    else
                        iState = RBS_UNCHECKEDNORMAL;
                    break;
                default:
                    break;
            }
            break;

        case BP_CHECKBOX:
            switch (dwCheckState)
            {
                case BST_CHECKED:
                    if (dwStyle & WS_DISABLED)
                        iState = CBS_CHECKEDDISABLED;
                    else if (bFocus)
                        iState = CBS_CHECKEDPRESSED;
                    else if (bHot)
                        iState = CBS_CHECKEDHOT;
                    else
                        iState = CBS_CHECKEDNORMAL;
                    break;
                case BST_INDETERMINATE:
                    if (dwStyle & WS_DISABLED)
                        iState = CBS_MIXEDDISABLED;
                    else if (bFocus)
                        iState = CBS_MIXEDPRESSED;
                    else if (bHot)
                        iState = CBS_MIXEDHOT;
                    else
                        iState = CBS_MIXEDNORMAL;
                    break;
                case BST_UNCHECKED:
                    if (dwStyle & WS_DISABLED)
                        iState = CBS_UNCHECKEDDISABLED;
                    else if (bFocus)
                        iState = CBS_UNCHECKEDPRESSED;
                    else if (bHot)
                        iState = CBS_UNCHECKEDHOT;
                    else
                        iState = CBS_UNCHECKEDNORMAL;
                    break;
                default:
                    break;
            }
            break;
        default:
            //ASSERT(0);
            break;
    }

    return iState;
}

void GetRoundRectPath(Gdiplus::GraphicsPath* pPath, const Gdiplus::Rect& r, int dia)
{
    // diameter can't exceed width or height
    if (dia > r.Width)
        dia = r.Width;
    if (dia > r.Height)
        dia = r.Height;

    // define a corner
    Gdiplus::Rect corner(r.X, r.Y, dia, dia);

    // begin path
    pPath->Reset();
    pPath->StartFigure();

    // top left
    pPath->AddArc(corner, 180, 90);

    // top right
    corner.X += (r.Width - dia - 1);
    pPath->AddArc(corner, 270, 90);

    // bottom right
    corner.Y += (r.Height - dia - 1);
    pPath->AddArc(corner, 0, 90);

    // bottom left
    corner.X -= (r.Width - dia - 1);
    pPath->AddArc(corner, 90, 90);

    // end path
    pPath->CloseFigure();
}

void DrawRect(LPRECT prc, HDC hdcPaint, Gdiplus::DashStyle dashStyle, Gdiplus::Color clr, Gdiplus::REAL width)
{
    auto myPen = std::make_unique<Gdiplus::Pen>(clr, width);
    myPen->SetDashStyle(dashStyle);
    auto myGraphics = std::make_unique<Gdiplus::Graphics>(hdcPaint);

    myGraphics->DrawRectangle(myPen.get(), static_cast<INT>(prc->left), static_cast<INT>(prc->top),
                              static_cast<INT>(prc->right - 1 - prc->left), static_cast<INT>(prc->bottom - 1 - prc->top));
}

void DrawFocusRect(LPRECT prcFocus, HDC hdcPaint)
{
    DrawRect(prcFocus, hdcPaint, Gdiplus::DashStyleDot, Gdiplus::Color(0xFF, 0, 0, 0), 1.0);
}

void PaintControl(HWND hWnd, HDC hdc, RECT* prc, bool bDrawBorder)
{
    HDC hdcPaint = nullptr;

    if (bDrawBorder)
        InflateRect(prc, 1, 1);
    HPAINTBUFFER hBufferedPaint = BeginBufferedPaint(hdc, prc, BPBF_TOPDOWNDIB, nullptr, &hdcPaint);
    if (hdcPaint && hBufferedPaint)
    {
        RECT rc;
        GetWindowRect(hWnd, &rc);

        PatBlt(hdcPaint, 0, 0, RECTWIDTH(rc), RECTHEIGHT(rc), BLACKNESS);
        BufferedPaintSetAlpha(hBufferedPaint, &rc, 0x00);

        ///
        /// first blit white so list ctrls don't look ugly:
        ///
        PatBlt(hdcPaint, 0, 0, RECTWIDTH(rc), RECTHEIGHT(rc), WHITENESS);

        if (bDrawBorder)
            InflateRect(prc, -1, -1);
        // Tell the control to paint itself in our memory buffer
        SendMessage(hWnd, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(hdcPaint), PRF_CLIENT | PRF_ERASEBKGND | PRF_NONCLIENT | PRF_CHECKVISIBLE);

        if (bDrawBorder)
        {
            InflateRect(prc, 1, 1);
            FrameRect(hdcPaint, prc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        }

        // don't make a possible border opaque, only the inner part of the control
        InflateRect(prc, -2, -2);
        // Make every pixel opaque
        BufferedPaintSetAlpha(hBufferedPaint, prc, 255);
        EndBufferedPaint(hBufferedPaint, TRUE);
    }
}

BOOL DetermineGlowSize(int* piSize, LPCWSTR pszClassIdList /*= NULL*/)
{
    if (!piSize)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!pszClassIdList)
        pszClassIdList = L"CompositedWindow::Window";

    HTHEME hThemeWindow = OpenThemeData(nullptr, pszClassIdList);
    if (hThemeWindow != nullptr)
    {
        SUCCEEDED(GetThemeInt(hThemeWindow, 0, 0, TMT_TEXTGLOWSIZE, piSize));
        SUCCEEDED(CloseThemeData(hThemeWindow));
        return TRUE;
    }

    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
}

BOOL GetEditBorderColor(HWND hWnd, COLORREF* pClr)
{
    //ASSERT(pClr);

    HTHEME hTheme = OpenThemeData(hWnd, L"Edit");
    if (hTheme)
    {
        ::GetThemeColor(hTheme, EP_BACKGROUNDWITHBORDER, EBWBS_NORMAL, TMT_BORDERCOLOR, pClr);
        
        CloseThemeData(hTheme);
        return TRUE;
    }

    return FALSE;
}