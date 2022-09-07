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

namespace
{
std::unique_ptr<CGotoLineDlg>    g_pGotoLineDlg;
} // namespace

LRESULT CGotoLineDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0,0,0,0, SWP_HIDEWINDOW);
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
                //SetDlgItemText(hwndDlg, IDC_LINEINFO, lineInfo.c_str());
                //std::wstring sLine = CStringUtils::Format(L"%Id", line);
                //SetDlgItemText(hwndDlg, IDC_LINE, sLine.c_str());
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
void CGotoLineDlg::UpdateLineInfo()
{
    auto      last       = Scintilla().LineFromPosition(Scintilla().Length()) + 1;
    ResString lineFormat(g_hRes, IDS_GOTOLINEINFO);
    auto lineInfo = CStringUtils::Format(lineFormat, last);
    SetDlgItemText(*this, IDC_LINEINFO, lineInfo.c_str());

    auto      line  = GetCurrentLineNumber() + 1;
    std::wstring sLine = CStringUtils::Format(L"%Id", line);
    SetDlgItemText(*this, IDC_LINE, sLine.c_str());
    SendDlgItemMessage(*this, IDC_LINE, EM_SETSEL, 0, -1);
}

LRESULT CGotoLineDlg::DoCommand(int id, int /*msg*/)
{
    switch (id)
    {
        case IDCANCEL:
            ShowWindow(*this, SW_HIDE);
            return FALSE;
        case IDOK:
        {
            auto sLine = GetDlgItemText(IDC_LINE);
            intptr_t line       = _wtol(sLine.get());
            intptr_t last  = Scintilla().LineFromPosition(Scintilla().Length()) + 1;
            
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
    if (g_pGotoLineDlg == nullptr)
        g_pGotoLineDlg = std::make_unique<CGotoLineDlg>(m_pMainWindow);
    
    g_pGotoLineDlg->ShowModeless(g_hRes, IDD_GOTOLINE, GetHwnd(), FALSE);
    
    constexpr UINT flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS;
    RECT           rc;
    GetClientRect(GetHwnd(), &rc);
    
    //Adjust dialog position
    int   dx = GetWidthToLeft(STATUSBAR_CUR_POS); // STATUSBAR_CUR_POS = 7
    int   dw = GetWidthToLeft(STATUSBAR_CUR_POS + 1) - dx;
    RECT  rcw;
    GetWindowRect(GetStatusbarWnd(), &rcw);
    POINT pt(rcw.left + dx, rcw.top);
    //ClientToScreen(GetHwnd(), &pt);
    //pt.y = rcw.top;
    SetWindowPos(*g_pGotoLineDlg, nullptr, pt.x, pt.y, dw, 30, flags | SWP_HIDEWINDOW);

    //Adjust control position
    HWND lines  = GetDlgItem(*g_pGotoLineDlg, IDC_LINEINFO);
    HWND line = GetDlgItem(*g_pGotoLineDlg, IDC_LINE);
    HWND close = GetDlgItem(*g_pGotoLineDlg, IDCANCEL);

    GetWindowRect(close, &rc);
    int bw = rc.right - rc.left;
    int bh = rc.bottom - rc.top;
    
    int lineHeight = GetStatusBarHeight() - 6; 
    int lineWidth  = (dw - 16 - bw - 8) / 2;
    GetClientRect(*g_pGotoLineDlg, &rc);
    int top = (rc.bottom - rc.top - bh) / 2;
    MoveWindow(close, rc.right - bw - 8, top, bw, bh, FALSE);
    top = (rc.bottom - rc.top - lineHeight) / 2;
    MoveWindow(line, rc.left + 8, top, lineWidth, lineHeight, FALSE);
    MoveWindow(lines, rc.left + 8 + lineWidth, top, lineWidth, lineHeight, FALSE);
    
    SetWindowPos(*g_pGotoLineDlg, nullptr, pt.x, pt.y, dw, 30, flags | SWP_SHOWWINDOW);
    SetFocus(*g_pGotoLineDlg);

    return true;
}

void gotoLineClose()
{
    if (g_pGotoLineDlg != nullptr && IsWindowVisible(*g_pGotoLineDlg))
        SendMessage(*g_pGotoLineDlg, WM_COMMAND, IDCANCEL, 0);
}