// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020-2021 - Stefan Kueng
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
#include "CmdGotoLine.h"

#include "ScintillaWnd.h"
#include "StringUtils.h"
#include "Theme.h"
#include "ResString.h"
#include "MainWindow.h"

LRESULT CCmdGotoLine::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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

        UpdateLineInfo();
        SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_HIDEWINDOW);
        SetFocus(GetDlgItem(hwndDlg, IDC_LINEINFO));
        return FALSE;
    }
    case WM_ACTIVATE:
    {
        if (wParam == WA_INACTIVE)
        {
            ShowWindow(*this, SW_HIDE);
        }
        else
        {
            UpdateLineInfo();
            SetFocus(hwndDlg);
        }

        return TRUE;
    }
    case WM_COMMAND:
        return DoCommand(LOWORD(wParam), HIWORD(wParam));
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC         hdc = BeginPaint(hwndDlg, &ps);
        RECT        rc;
        GetClientRect(hwndDlg, &rc);
        FrameRect(hdc, &rc, GetSysColorBrush(COLOR_BTNSHADOW));
        EndPaint(hwndDlg, &ps);
        return 0;
    }
    default:
        break;
    }
    return FALSE;
}
void CCmdGotoLine::UpdateLineInfo()
{
    auto      last = Scintilla().LineFromPosition(Scintilla().Length()) + 1;
    ResString lineFormat(g_hRes, IDS_GOTOLINEINFO);
    auto lineInfo = CStringUtils::Format(lineFormat, last);
    SetDlgItemText(*this, IDC_LINEINFO, lineInfo.c_str());

    auto      line = GetCurrentLineNumber() + 1;
    std::wstring sLine = CStringUtils::Format(L"%Id", line);
    SetDlgItemText(*this, IDC_LINE, sLine.c_str());
    SendDlgItemMessage(*this, IDC_LINE, EM_SETSEL, 0, -1);
}

LRESULT CCmdGotoLine::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
    case IDCANCEL:
        ShowWindow(*this, SW_HIDE);
        return FALSE;
    case IDOK:
    {
        auto sLine = GetDlgItemText(IDC_LINE);
        intptr_t line = _wtol(sLine.get());
        intptr_t last = Scintilla().LineFromPosition(Scintilla().Length()) + 1;

        line = line > last ? last : line;

        GotoLine(line >= 1 ? line - 1 : 0);
        return FALSE;
    }

    default:
        break;
    }
    return 1;
}

bool CCmdGotoLine::Execute()
{
    ShowModeless(g_hRes, IDD_GOTOLINE, *m_pMainWindow);

    constexpr UINT flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS;
    //Adjust dialog position
    int   dx = GetWidthToLeft(STATUSBAR_CUR_POS); // STATUSBAR_CUR_POS = 7
    int   dw = GetWidthToLeft(STATUSBAR_CUR_POS + 1) - dx;
    RECT  rcw;
    GetWindowRect(GetStatusbarWnd(), &rcw);
    POINT pt(rcw.left + dx, rcw.top);
    SetWindowPos(*this, nullptr, pt.x, pt.y, dw, 30, flags | SWP_SHOWWINDOW);
    return true;
}
