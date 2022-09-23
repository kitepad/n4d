// This file is part of BowPad.
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
#include "IniSettings.h"
#include "Theme.h"
#include "ResString.h"
#include "MainWindow.h"

CCmdWhiteSpace::CCmdWhiteSpace(void* obj)
    : ICommand(obj)
{
}

bool CCmdWhiteSpace::Execute()
{
    bool bShown = Scintilla().ViewWS() != Scintilla::WhiteSpace::Invisible;
    Scintilla().SetViewWS(bShown ? Scintilla::WhiteSpace::Invisible : Scintilla::WhiteSpace::VisibleAlways);
    SetInt64(DEFAULTS_SECTION, L"Whitespace", static_cast<int>(Scintilla().ViewWS()));
    if (bShown || ((GetKeyState(VK_SHIFT) & 0x8000) == 0))
        Scintilla().SetViewEOL(false);
    else
        Scintilla().SetViewEOL(true);

    return true;
}

void CCmdWhiteSpace::AfterInit()
{
    auto ws = static_cast<Scintilla::WhiteSpace>(GetInt64(DEFAULTS_SECTION, L"Whitespace", 0));
    Scintilla().SetViewWS(ws);
}

void CCmdTabSize::AfterInit()
{
    int ve = static_cast<int>(GetInt64(DEFAULTS_SECTION, L"TabWidth", 4));
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
        SetInt64(DEFAULTS_SECTION, L"UseTabs", Scintilla().UseTabs());
        UpdateStatusBar(false);
        return true;
    }
    return false;
}


LRESULT CCmdTabSize::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            int          val   = static_cast<int>(GetInt64(DEFAULTS_SECTION, L"TabWidth", 4));
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
            //HBRUSH hbr = CreateSolidBrush(CTheme::CurrentTheme().itemHover);
            rc.bottom -= 1;
            //FrameRect(hdc, &rc, hbr);
            FrameRect(hdc, &rc, GetSysColorBrush(COLOR_BTNSHADOW));
            //DeleteObject(hbr);
            EndPaint(hwndDlg, &ps);
            return 0;
        }
        default:
            break;
    }
    return FALSE;
}

LRESULT CCmdTabSize::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDOK:
        {
            auto sLine = GetDlgItemText(IDC_TABWIDTH);
            int w = _wtol(sLine.get());
            Scintilla().SetTabWidth(w);
            SetInt64(DEFAULTS_SECTION, L"TabWidth", w);
            UpdateStatusBar(false);
            EndDialog(*this, id);
            break;
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
    ShowModeless(g_hRes, IDD_SETTABWIDTH, GetHwnd(), TRUE);

    constexpr UINT flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS;
    // Adjust dialog position
    int  dx = GetWidthToLeft(STATUSBAR_TABSPACE); // STATUSBAR_CUR_POS = 7
    int  dw = GetWidthToLeft(STATUSBAR_TABSPACE + 1) - dx;
    RECT rcw;
    GetWindowRect(GetStatusbarWnd(), &rcw);
    POINT pt(rcw.left + dx, rcw.top);
    SetWindowPos(*this, nullptr, pt.x, pt.y, dw, 30, flags | SWP_SHOWWINDOW);
    return true;
}
