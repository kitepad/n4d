﻿// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017, 2020-2021 - Stefan Kueng
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
#include "CmdWhiteSpace.h"
#include "main.h"
#include "IniSettings.h"
#include "Theme.h"
#include "ResString.h"
#include "MainWindow.h"

namespace
{
std::unique_ptr<CSetTabSizeDlg> g_pTabSizeDlg;
} // namespace

CCmdWhiteSpace::CCmdWhiteSpace(void* obj)
    : ICommand(obj)
{
}

bool CCmdWhiteSpace::Execute()
{
    bool bShown = Scintilla().ViewWS() != Scintilla::WhiteSpace::Invisible;
    Scintilla().SetViewWS(bShown ? Scintilla::WhiteSpace::Invisible : Scintilla::WhiteSpace::VisibleAlways);
    CIniSettings::Instance().SetInt64(L"View", L"whitespace", static_cast<int>(Scintilla().ViewWS()));
    if (bShown || ((GetKeyState(VK_SHIFT) & 0x8000) == 0))
        Scintilla().SetViewEOL(false);
    else
        Scintilla().SetViewEOL(true);
    // InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
    return true;
}

void CCmdWhiteSpace::AfterInit()
{
    auto ws = static_cast<Scintilla::WhiteSpace>(CIniSettings::Instance().GetInt64(L"View", L"whitespace", 0));
    Scintilla().SetViewWS(ws);
    // InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
}

CCmdTabSize::CCmdTabSize(void* obj)
    : ICommand(obj)
{
}

void CCmdTabSize::AfterInit()
{
    int   ve         = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"tabwidth", 4));
    Scintilla().SetTabWidth(ve);
    UpdateStatusBar(false);
}

// each document stores its own setting of tabs/spaces.
// the 'use tabs' command toggles both the setting of the
// current document as well as the global default.
// When a document is opened the first time, it's setting
// is set to 'default' and stays that way until the user
// executes this command.
// While the document has its setting set to 'default', it will
// always use the global default setting or the settings set
// by an editorconfig file.
//
// to think about:
// * only toggle the current doc settings, have the global settings toggled via a settings dialog or a separate dropdown button
// * allow to configure the tab/space setting with file extension masks, e.g. space for all *.cpp files but tabs for all *.py files
CCmdUseTabs::CCmdUseTabs(void* obj)
    : ICommand(obj)
{
}

bool CCmdUseTabs::Execute()
{
    if (HasActiveDocument())
    {
        auto& doc = GetModActiveDocument();
        if (doc.m_tabSpace == TabSpace::Default)
        {
            Scintilla().SetUseTabs(Scintilla().UseTabs() ? 0 : 1);
        }
        else
        {
            Scintilla().SetUseTabs(doc.m_tabSpace == TabSpace::Tabs ? 0 : 1);
        }
        doc.m_tabSpace = Scintilla().UseTabs() ? TabSpace::Tabs : TabSpace::Spaces;
        CIniSettings::Instance().SetInt64(L"View", L"usetabs", Scintilla().UseTabs());
        // InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
        UpdateStatusBar(false);
        return true;
    }
    return false;
}


LRESULT CSetTabSizeDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            CTheme::Instance().RegisterThemeChangeCallback(
                [this]() {
                    CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
                });

            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            int val = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"tabwidth", 4));
            std::wstring sLine = CStringUtils::Format(L"%lld", val);
            SetDlgItemText(hwndDlg, IDC_TABWIDTH, sLine.c_str());
            SendDlgItemMessage(hwndDlg, IDC_TABWIDTH, EM_SETSEL, 0, -1);
        }
            return FALSE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_ACTIVATE:
        {
            if (wParam == WA_INACTIVE)
            {
                ShowWindow(*this, SW_HIDE);
            }
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hwndDlg, &ps);
            RECT        rc;
            GetClientRect(hwndDlg, &rc);
            HBRUSH hbr = CreateSolidBrush(CTheme::CurrentTheme().itemHover);
            rc.bottom += 1;
            FrameRect(hdc, &rc, hbr);
            DeleteObject(hbr);
            EndPaint(hwndDlg, &ps);
            return 0;
        }
        default:
            break;
    }
    return FALSE;
}

LRESULT CSetTabSizeDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDOK:
        {
            auto sLine = GetDlgItemText(IDC_TABWIDTH);
            int w = _wtol(sLine.get());
            Scintilla().SetTabWidth(w);
            CIniSettings::Instance().SetInt64(L"View", L"tabwidth", w);
            UpdateStatusBar(false);
        }
        case IDCANCEL:
            EndDialog(*this, id);
            break;
        default:
            break;
    }
    return 1;
}

bool CCmdTabSize::Execute()
{
    if (g_pTabSizeDlg == nullptr)
        g_pTabSizeDlg = std::make_unique<CSetTabSizeDlg>(m_pMainWindow);

    g_pTabSizeDlg->ShowModeless(g_hRes, IDD_SETTABWIDTH, GetHwnd(), FALSE);

    constexpr UINT flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS;
    RECT           rc;
    GetClientRect(GetHwnd(), &rc);

    // Adjust dialog position
    int  dx = GetWidthToLeft(STATUSBAR_TABSPACE); // STATUSBAR_CUR_POS = 7
    int  dw = GetWidthToLeft(STATUSBAR_TABSPACE + 1) - dx;
    RECT rcw;
    GetWindowRect(GetStatusbarWnd(), &rcw);
    POINT pt(rcw.left + dx, rcw.top);
    SetWindowPos(*g_pTabSizeDlg, nullptr, pt.x, pt.y, dw, 30, flags | SWP_HIDEWINDOW);

    // Adjust control position
    HWND edit = GetDlgItem(*g_pTabSizeDlg, IDC_TABWIDTH);
    GetClientRect(*g_pTabSizeDlg, &rc);
    MoveWindow(edit, rc.left + 1, rc.top + 4, rc.right - rc.left - 2, rc.bottom - rc.top - 2, FALSE);

    SetWindowPos(*g_pTabSizeDlg, nullptr, pt.x, pt.y, dw, 30, flags | SWP_SHOWWINDOW);
    SetFocus(*g_pTabSizeDlg);
    return true;
}