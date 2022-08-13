// This file is part of BowPad.
//
// Copyright (C) 2013, 2015-2017, 2020-2021 - Stefan Kueng
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
#include "main.h"
#include "AboutDlg.h"
#include "AppUtils.h"
#include "Theme.h"
#include "ResString.h"
#include <string>
#include <Commdlg.h>


CAboutDlg::CAboutDlg(HWND /*hParent*/)
    //: m_hParent(hParent)
    //, m_hHiddenWnd(nullptr)
    //, m_scrollView(0)
{
    //m_scrollView.RegisterWindowClass();
}

CAboutDlg::~CAboutDlg()
{
}


LRESULT CAboutDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_BOWPAD);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            // initialize the controls
            m_link.ConvertStaticToHyperlink(hwndDlg, IDC_WEBLINK, L"http://tools.stefankueng.com");
            SetDlgItemText(hwndDlg, IDC_VERSIONLABEL, L"N4D (Notepad for Developer) 1.0.0 (64-bit)");
        }
            return TRUE;
        case WM_COMMAND:
            EndDialog(*this, LOWORD(wParam));
            return TRUE;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hwndDlg, &ps);
            RECT        rc;
            GetClientRect(hwndDlg, &rc);
            HBRUSH hbr = CreateSolidBrush(CTheme::CurrentTheme().itemHover);
            FrameRect(hdc, &ps.rcPaint, hbr);
            DeleteObject(hbr);
            EndPaint(hwndDlg, &ps);
            return FALSE;
        }

        default:
            return FALSE;
    }
}

//LRESULT CAboutDlg::DoCommand(int id)
//{
//    switch (id)
//    {
//        case IDOK:
//        case IDCANCEL:
//            EndDialog(*this, id);
//            break;
//    }
//    return 1;
//}
