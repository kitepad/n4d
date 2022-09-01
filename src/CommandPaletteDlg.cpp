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
#include "CommandPaletteDlg.h"
#include "CommandHandler.h"
#include "KeyboardShortcutHandler.h"
#include "resource.h"
#include "AppUtils.h"
#include "StringUtils.h"
#include "Theme.h"
#include "OnOutOfScope.h"
#include "ResString.h"
#include <string>
#include <algorithm>
#include <memory>
#include <Commdlg.h>
#include <strsafe.h>
#include <regex>
#include <Richedit.h>

extern HINSTANCE     g_hRes;
static POINT PADDINGS = {16, 4};

CDialogWithFilterableList::CDialogWithFilterableList(void* obj)
    : ICommand(obj), m_pCmd(0)
{
}

void CDialogWithFilterableList::ClearFilterText()
{
    m_pCmd = nullptr;
    SetWindowText(m_hFilter, L"");
    FillResults(false);
    SetFocus(m_hFilter);
}

LRESULT CDialogWithFilterableList::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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

            InitDialog(hwndDlg, IDI_BOWPAD, false);
            CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
            // initialize the controls
            m_hFilter  = GetDlgItem(*this, IDC_FILTER);
            m_hResults = GetDlgItem(*this, IDC_RESULTS);
            
            ResString sFilterCue(g_hRes, GetFilterCUE());
            
            SendMessage(m_hFilter, EM_SETCUEBANNER, 1, reinterpret_cast<LPARAM>(sFilterCue.c_str()));
            SendMessage(m_hFilter, EM_SETMARGINS, (WPARAM)(EC_LEFTMARGIN | EC_RIGHTMARGIN), (LPARAM)MAKELONG(16, 8));
            SetWindowSubclass(m_hFilter, EditSubClassProc, 0, reinterpret_cast<DWORD_PTR>(this));
            SetWindowSubclass(m_hResults, ListViewSubClassProc, 0, reinterpret_cast<DWORD_PTR>(this));

            InitResultsList();
            FillResults(true);
            InvalidateRect(*this, nullptr, true);
        }
            return TRUE;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam), HIWORD(wParam));
        case WM_NOTIFY:
            switch (wParam)
            {
                case IDC_RESULTS:
                    return DoListNotify(reinterpret_cast<LPNMITEMACTIVATE>(lParam));
                default:
                    return FALSE;
            }
        case WM_TIMER:
        {
            FillResults(false);
            KillTimer(*this, 101);
        }
        break;
        case WM_ACTIVATE:
            if (wParam == WA_INACTIVE)
            {
                ShowWindow(*this, SW_HIDE);
            }
            else
            {
                SetFocus(m_hFilter);
            }
            m_pCmd = nullptr;
            break;
        case WM_SIZE:
            ResizeChildren();
            break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hwndDlg, &ps);
            RECT        rc;
            GetClientRect(hwndDlg, &rc);
            HBRUSH hbr = CreateSolidBrush(CTheme::CurrentTheme().itemHover);
            FrameRect(hdc, &rc, hbr);
            rc.top    = 38;
            rc.bottom = 39;

            FillRect(hdc, &rc, hbr); 
            DeleteObject(hbr);
            EndPaint(hwndDlg, &ps);
            break;
        }
        case WM_CTLCOLOREDIT:
        {
            HWND h = (HWND)lParam;
            if (h == m_hFilter)
            {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, CTheme::CurrentTheme().winBack);
                SetTextColor(hdc, CTheme::CurrentTheme().winFore);
                return (INT_PTR)CreateSolidBrush(CTheme::CurrentTheme().winBack);
            }
        }
            break;
    }
    return FALSE;
}

LRESULT CDialogWithFilterableList::DoCommand(int id, int code)
{
    switch (id)
    {
        case IDOK:
        {
            OnOK();
            ShowWindow(*this, SW_HIDE);
        }
        break;
        case IDCANCEL:
            if (!m_pCmd)
                ShowWindow(*this, SW_HIDE);
            else
                FillResults(true);
            m_pCmd = nullptr;
            break;
        case IDC_FILTER:
            if (code == EN_CHANGE)
                SetTimer(*this, 101, 50, nullptr);
            break;
    }
    return 1;
}

void CDialogWithFilterableList::InitResultsList() const
{
    SetWindowTheme(m_hResults, L"Explorer", nullptr);
    ListView_SetItemCountEx(m_hResults, 0, 0);

    auto hListHeader = ListView_GetHeader(m_hResults);
    int  c           = Header_GetItemCount(hListHeader) - 1;
    while (c >= 0)
        ListView_DeleteColumn(m_hResults, c--);

    DWORD exStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT;
    ListView_SetExtendedListViewStyle(m_hResults, exStyle);
    RECT rc;
    GetClientRect(m_hResults, &rc);

    LVCOLUMN lvc{};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_IMAGE;
    lvc.fmt  = LVCFMT_LEFT | LVCF_IMAGE;
    ListView_InsertColumn(m_hResults, 0, &lvc);
}

LRESULT CDialogWithFilterableList::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    switch (lpNMItemActivate->hdr.code)
    {
        case LVN_GETINFOTIP:
        {
            LPNMLVGETINFOTIP tip       = reinterpret_cast<LPNMLVGETINFOTIPW>(lpNMItemActivate);
            int              itemIndex = static_cast<size_t>(tip->iItem);
            if (itemIndex < 0 || itemIndex >= static_cast<int>(m_results.size()))
            {
                assert(false);
                return 0;
            }
            const auto& data = m_results[itemIndex].text1;
            wcscpy_s(tip->pszText, tip->cchTextMax, data.c_str());
        }
        break;
        case LVN_GETDISPINFO:
            // Note: the listview is Owner Draw which means you draw it,
            // but you still need to tell the list what exactly the text is
            // that you intend to draw or things like clicking the header to
            // re-size won't work.
            return GetListItemDispInfo(reinterpret_cast<NMLVDISPINFO*>(lpNMItemActivate));

        case NM_RETURN:
        case NM_DBLCLK:
            // execute the selected command
            if (lpNMItemActivate->iItem >= 0 && lpNMItemActivate->iItem < static_cast<int>(m_results.size()))
            {
                SendMessage(*this, WM_COMMAND, MAKEWPARAM(IDOK, 1), 0);
            }
            break;
        default:
            break;
    }
    InvalidateRect(m_hResults, nullptr, true);

    return 0;
}

LRESULT CDialogWithFilterableList::GetListItemDispInfo(NMLVDISPINFO* pDispInfo)
{
    if ((pDispInfo->item.mask & LVIF_TEXT) != 0)
    {
        if (pDispInfo->item.pszText == nullptr)
            return 0;
        pDispInfo->item.pszText[0] = 0;
        int itemIndex              = pDispInfo->item.iItem;
        if (itemIndex >= static_cast<int>(m_results.size()))
            return 0;

        std::wstring sTemp;
        const auto&  item = m_results[itemIndex];
        switch (pDispInfo->item.iSubItem)
        {
            case 0:
                StringCchCopy(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, item.text1.c_str());
                break;
            case 1:
                StringCchCopy(pDispInfo->item.pszText, pDispInfo->item.cchTextMax, item.text3.c_str());
                break;

            default:
                break;
        }
    }
    return 0;
}

bool CDialogWithFilterableList::IsFiltered(std::wstring sFilterText, CListItem item)
{
    return sFilterText.empty() || StrStrIW(item.text1.c_str(), sFilterText.c_str());
}

void CDialogWithFilterableList::FillResults(bool force)
{
    static std::wstring lastFilterText;
    auto                filterText  = GetDlgItemText(IDC_FILTER);
    std::wstring        sFilterText = filterText.get();
    if (force || lastFilterText.empty() || _wcsicmp(sFilterText.c_str(), lastFilterText.c_str()))
    {
        m_results.clear();
 
        for (int i = 0; i < m_allResults.size(); i++)
        {
            CListItem cmd = m_allResults[i];
            if (IsFiltered(sFilterText,cmd))
            {
                m_results.push_back(cmd);
            }
        }
        lastFilterText = sFilterText;
        ListView_SetItemCountEx(m_hResults, m_results.size(), 0);
        RECT rc;
        GetClientRect(m_hResults, &rc);
        ListView_SetColumnWidth(m_hResults, 0, rc.right - rc.left);
    }
}

LRESULT CALLBACK CDialogWithFilterableList::EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    CDialogWithFilterableList* pThis = reinterpret_cast<CDialogWithFilterableList*>(dwRefData);

    switch (uMsg)
    {
        case WM_DESTROY:
        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hWnd, EditSubClassProc, uIdSubclass);
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_DOWN)
            {
                auto i = ListView_GetSelectionMark(pThis->m_hResults);
                ListView_SetItemState(pThis->m_hResults, i, 0, LVIS_SELECTED);
                ListView_SetSelectionMark(pThis->m_hResults, i + 1);
                ListView_SetItemState(pThis->m_hResults, i + 1, LVIS_SELECTED, LVIS_SELECTED);
                ListView_EnsureVisible(pThis->m_hResults, i + 1, false);
                return 0;
            }
            if (wParam == VK_UP)
            {
                auto i = ListView_GetSelectionMark(pThis->m_hResults);
                if (i > 0)
                {
                    ListView_SetItemState(pThis->m_hResults, i, 0, LVIS_SELECTED);
                    ListView_SetSelectionMark(pThis->m_hResults, i - 1);
                    ListView_SetItemState(pThis->m_hResults, i - 1, LVIS_SELECTED, LVIS_SELECTED);
                    ListView_EnsureVisible(pThis->m_hResults, i - 1, false);
                }
                return 0;
            }
            if (wParam == VK_RETURN)
            {
                auto i = ListView_GetSelectionMark(pThis->m_hResults);
                if (i >= 0)
                {
                    SendMessage(*pThis, WM_COMMAND, MAKEWPARAM(IDOK, 1), 0);
                }
                return 0;
            }
            else

                break;
        default:
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CDialogWithFilterableList::ListViewSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    CDialogWithFilterableList* pThis = reinterpret_cast<CDialogWithFilterableList*>(dwRefData);
    switch (uMsg)
    {
        case WM_PAINT:
        {
            COLORREF back    = CTheme::CurrentTheme().winBack;
            COLORREF selBack = CTheme::CurrentTheme().itemHover; // RGB(200, 200, 200); // CTheme::CurrentTheme().selBack;

            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            int  width   = ps.rcPaint.right - ps.rcPaint.left;
            int  height  = ps.rcPaint.bottom - ps.rcPaint.top;
            auto memDC   = ::CreateCompatibleDC(ps.hdc);
            auto hBitmap = CreateCompatibleBitmap(ps.hdc, width, height);
            SelectObject(memDC, hBitmap);

            SetBkMode(memDC, TRANSPARENT);
            SetBkColor(memDC, back);
            SetTextColor(memDC, CTheme::CurrentTheme().winFore);
            HBRUSH hbrBack = CreateSolidBrush(back);
            FillRect(memDC, &ps.rcPaint, hbrBack);
            HBRUSH hbrSelBack = CreateSolidBrush(selBack);
            for (int i = 0; i < pThis->m_results.size(); i++)
            {
                RECT rc = {};
                ListView_GetItemRect(hWnd, i, &rc, LVIR_BOUNDS);

                BOOL isSelected = ListView_GetItemState(hWnd, i, LVIS_SELECTED) & LVIS_SELECTED;

                FillRect(memDC, &rc, isSelected ? hbrSelBack : hbrBack);
                SetBkColor(memDC, isSelected ? selBack : back);
                rc.top += PADDINGS.y;
                rc.left += PADDINGS.x;
                pThis->DrawItemText(memDC, &rc,i);
            }
            BitBlt(ps.hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            DeleteObject(hbrBack);
            DeleteObject(hbrSelBack);
            ReleaseDC(hWnd, memDC);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hWnd, ListViewSubClassProc, uIdSubclass);
            break;
        }
        default:
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void CDialogWithFilterableList::ResizeChildren()
{
    RECT rc;
    GetClientRect(*this, &rc);
    MoveWindow(m_hFilter, 1, 8, rc.right - rc.left - 2, 28, false);
    MoveWindow(m_hResults, 1, 40, rc.right - rc.left - 2, rc.bottom - rc.top - 41, false);

    GetClientRect(m_hResults, &rc);
    ListView_SetColumnWidth(m_hResults, 0, rc.right - rc.left);
}

void CCommandPaletteDlg::DrawItemText(HDC hdc, LPRECT rc,int idx)
{
    DrawText(hdc, m_results[idx].text2.c_str(), -1, rc, DT_LEFT | DT_VCENTER);
    rc->left -= PADDINGS.x;
    rc->right -= PADDINGS.x; 
    DrawText(hdc, m_results[idx].text3.c_str(), -1, rc, DT_RIGHT | DT_VCENTER);
}

CCommandPaletteDlg::CCommandPaletteDlg(void* obj)
    : CDialogWithFilterableList(obj)
{
    const auto& commands = CCommandHandler::Instance().GetCommands();
    for (const auto& [cmdId, pCommand] : commands)
    {
        CListItem data = {};
        data.uintVal       = cmdId;
        data.text1     = CCommandHandler::Instance().GetCommandLabel(cmdId);
        data.text2 = CCommandHandler::Instance().GetCommandDescription(cmdId);
        data.text2.erase(0, data.text2.find_first_not_of(L" "));
        data.text2.erase(data.text2.find_last_not_of(L" ") + 1);
        data.text3 = CCommandHandler::Instance().GetShortCutStringForCommand(static_cast<WORD>(cmdId));
        if (!data.text1.empty())
            m_allResults.push_back(data);
    }
    const auto& noDelCommands = CCommandHandler::Instance().GetNoDeleteCommands();
    for (const auto& [cmdId, pCommand] : noDelCommands)
    {
        CListItem data;
        data.uintVal       = cmdId;
        data.text1     = CCommandHandler::Instance().GetCommandLabel(cmdId);
        data.text2 = CCommandHandler::Instance().GetCommandDescription(cmdId);
        data.text2.erase(0, data.text2.find_first_not_of(L" "));
        data.text2.erase(data.text2.find_last_not_of(L" ") + 1);
        data.text3 = CCommandHandler::Instance().GetShortCutStringForCommand(static_cast<WORD>(cmdId));
        if (!data.text1.empty())
            m_allResults.push_back(data);
    }

    std::sort(m_allResults.begin(), m_allResults.end(),
              [](const CListItem& a, const CListItem& b) -> bool {
                  return StrCmpLogicalW(a.text2.c_str(), b.text2.c_str()) < 0;
              });
}


UINT CCommandPaletteDlg::GetFilterCUE()
{
    return IDS_COMMANDPALETTE_FILTERCUE;
}

void CCommandPaletteDlg::OnOK()
{
    auto i = ListView_GetSelectionMark(m_hResults);
    if (i >= 0)
    {
        const auto& data    = m_results[i];
        m_pCmd = nullptr;
        SendMessage(GetParent(*this), WM_COMMAND, MAKEWPARAM(data.uintVal, 1), 0);
        ShowWindow(*this, SW_HIDE);
    }
}

 LRESULT CCommandPaletteDlg::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
     switch (lpNMItemActivate->hdr.code)
     {
         case LVN_GETINFOTIP:
         {
             LPNMLVGETINFOTIP tip       = reinterpret_cast<LPNMLVGETINFOTIPW>(lpNMItemActivate);
             int              itemIndex = static_cast<size_t>(tip->iItem);
             if (itemIndex < 0 || itemIndex >= static_cast<int>(m_results.size()))
             {
                 assert(false);
                 return 0;
             }
             const auto& data = m_results[itemIndex];
             if (data.text2.empty())
             {
                 _snwprintf_s(tip->pszText, tip->cchTextMax, _TRUNCATE, L"%s\t%s",
                              data.text1.c_str(), data.text3.c_str());
             }
             else
             {
                 wcscpy_s(tip->pszText, tip->cchTextMax, data.text2.c_str());
             }
         }
         break;
         case LVN_GETDISPINFO:
             // Note: the listview is Owner Draw which means you draw it,
             // but you still need to tell the list what exactly the text is
             // that you intend to draw or things like clicking the header to
             // re-size won't work.
             return GetListItemDispInfo(reinterpret_cast<NMLVDISPINFO*>(lpNMItemActivate));

         case NM_RETURN:
         case NM_DBLCLK:
             // execute the selected command
             if (lpNMItemActivate->iItem >= 0 && lpNMItemActivate->iItem < static_cast<int>(m_results.size()))
             {
                 SendMessage(*this, WM_COMMAND, MAKEWPARAM(IDOK, 1), 0);
             }
             break;
         default:
             break;
     }
     InvalidateRect(m_hResults, nullptr, true);

     return 0;
 }

 bool CCommandPaletteDlg::IsFiltered(std::wstring sFilterText, CListItem item)
 {
     return sFilterText.empty() || StrStrIW(item.text1.c_str(), sFilterText.c_str()) || StrStrIW(item.text2.c_str(), sFilterText.c_str());
 }