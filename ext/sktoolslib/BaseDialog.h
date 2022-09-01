// sktoolslib - common files for SK tools

// Copyright (C) 2012-2013, 2016, 2020-2021 - Stefan Kueng

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
#include "resource.h"
#include "AeroGlass.h"
#include <memory>

/**
 * A base window class.
 * Provides separate window message handlers for every window object based on
 * this class.
 */
class CDialog
{
public:
    static constexpr int  TITLE_HEIGHT = 40;

    static void InitScrollInfo(HWND hwnd, int bar, HWND scrollView)
    {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        const SIZE sz = {rc.right - rc.left, rc.bottom - rc.top};

        SCROLLINFO si = {};
        si.cbSize     = sizeof(SCROLLINFO);
        si.fMask      = SIF_PAGE | SIF_POS | SIF_RANGE;
        si.nPos = si.nMin = 1;

        si.nMax  = SB_HORZ == bar ? sz.cx : sz.cy;
        si.nPage = SB_HORZ == bar ? sz.cx : sz.cy;
        SetScrollInfo(hwnd, bar, &si, FALSE);

        EnumChildWindows(
        hwnd, [](HWND h, LPARAM lp) 
            {
                HWND newParent = (HWND)(lp);
                HWND oldParent   = GetParent(newParent);
                
                if (!(h == GetDlgItem(oldParent, IDOK) || h == GetDlgItem(oldParent, IDCANCEL)))
                {
                    SetParent(h, newParent);
                    RECT rc;
                    GetWindowRect(h, &rc);
                    OffsetRect(&rc, 0, -TITLE_HEIGHT);
                    POINT pt(rc.left, rc.top);
                    ScreenToClient(newParent, &pt);
                    MoveWindow(h, pt.x, pt.y,rc.right - rc.left, rc.bottom - rc.top, TRUE);
                }
                return TRUE;
            }, (LPARAM)scrollView);
         
    }

    static int GetScrollPos(HWND hwnd, int bar, UINT code)
    {
        SCROLLINFO si = {};
        si.cbSize     = sizeof(SCROLLINFO);
        si.fMask      = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
        GetScrollInfo(hwnd, bar, &si);

        const int minPos = si.nMin;
        const int maxPos = si.nMax - (si.nPage - 1);

        int result = -1;

        switch (code)
        {
            case SB_LINEUP /*SB_LINELEFT*/:
                result = max(si.nPos - 1, minPos);
                break;

            case SB_LINEDOWN /*SB_LINERIGHT*/:
                result = min(si.nPos + 1, maxPos);
                break;

            case SB_PAGEUP /*SB_PAGELEFT*/:
                result = max(si.nPos - (int)si.nPage, minPos);
                break;

            case SB_PAGEDOWN /*SB_PAGERIGHT*/:
                result = min(si.nPos + (int)si.nPage, maxPos);
                break;

            case SB_THUMBPOSITION:
                // do nothing
                break;

            case SB_THUMBTRACK:
                result = si.nTrackPos;
                break;

            case SB_TOP /*SB_LEFT*/:
                result = minPos;
                break;

            case SB_BOTTOM /*SB_RIGHT*/:
                result = maxPos;
                break;

            case SB_ENDSCROLL:
                // do nothing
                break;
        }

        return result;
    }

    static void ScrollClient(HWND hwnd, int bar, int pos)
    {
        static int s_prevx = 1;
        static int s_prevy = 1;

        int cx = 0;
        int cy = 0;

        int& delta = (bar == SB_HORZ ? cx : cy);
        int& prev  = (bar == SB_HORZ ? s_prevx : s_prevy);

        delta = prev - pos;
        prev  = pos;

        if (cx || cy)
        {
            //ScrollWindow(hwnd, cx, cy, NULL, NULL);
            //InvalidateRect(hwnd, NULL, true);
            ScrollWindowEx(hwnd, cx, cy, nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE | SW_SCROLLCHILDREN);
            //RECT rc; 
            //GetClientRect(hwnd, &rc);
            //rc.top = rc.top + TITLE_HEIGHT;
            //InvalidateRect(hwnd, NULL, TRUE);
        }
    }

    static void HandleScroll(HWND hwnd,int bar, WPARAM wParam, HWND clientHandle)
    {
        const int scrollPos = GetScrollPos(hwnd, bar, LOWORD(wParam));

        if (scrollPos == -1)
            return;

        SetScrollPos(hwnd, bar, scrollPos, TRUE);
        ScrollClient(clientHandle, bar, scrollPos);
        //ScrollClient(GetDlgItem(hwnd, IDC_SCROLLABLECONTAINER), bar, scrollPos);
    }

    static void HandleScrollOnSize(HWND hwnd, WPARAM wParam, LPARAM lParam, HWND clientHandle)
    {
        UINT state = static_cast<UINT>(wParam);
        int  cx    = LOWORD(lParam);
        int  cy    = HIWORD(lParam);

        if (state != SIZE_RESTORED && state != SIZE_MAXIMIZED)
            return;

        SCROLLINFO si = {};
        si.cbSize     = sizeof(SCROLLINFO);

        const int bar[]  = {SB_HORZ, SB_VERT};
        const int page[] = {cx, cy};

        for (size_t i = 0; i < ARRAYSIZE(bar); ++i)
        {
            si.fMask = SIF_PAGE;
            si.nPage = page[i];
            SetScrollInfo(hwnd, bar[i], &si, TRUE);

            si.fMask = SIF_RANGE | SIF_POS;
            GetScrollInfo(hwnd, bar[i], &si);

            const int maxScrollPos = si.nMax - (page[i] - 1);

            // Scroll client only if scroll bar is visible and window's
            // content is fully scrolled toward right and/or bottom side.
            // Also, update window's content on maximize.
            const bool needToScroll =
                (si.nPos != si.nMin && si.nPos == maxScrollPos) ||
                (state == SIZE_MAXIMIZED);

            if (needToScroll)
            {
                ScrollClient(clientHandle, bar[i], si.nPos);
                //ScrollClient(GetDlgItem(hwnd, IDC_SCROLLABLECONTAINER), bar[i], si.nPos);
            }
        }
    }

public:
    CDialog()
        : hResource(nullptr)
        , m_hwnd(nullptr)
        , m_bPseudoModal(false)
        , m_bPseudoEnded(false)
        , m_iPseudoRet(0)
        , m_hToolTips(nullptr)
    {
        m_margins = {};
    }
    virtual ~CDialog() = default;

    INT_PTR     DoModal(HINSTANCE hInstance, int resID, HWND hWndParent);
    INT_PTR     DoModal(HINSTANCE hInstance, LPCDLGTEMPLATE pDlgTemplate, HWND hWndParent);
    INT_PTR     DoModal(HINSTANCE hInstance, int resID, HWND hWndParent, UINT idAccel);
    void        ShowModeless(HINSTANCE hInstance, int resID, HWND hWndParent, bool show = true);
    void        ShowModeless(HINSTANCE hInstance, LPCDLGTEMPLATE pDlgTemplate, HWND hWndParent, bool show = true);
    static BOOL IsDialogMessage(LPMSG lpMsg);
    HWND        Create(HINSTANCE hInstance, int resID, HWND hWndParent);
    BOOL        EndDialog(HWND hDlg, INT_PTR nResult);
    void        AddToolTip(UINT ctrlID, LPCWSTR text);
    void        AddToolTip(HWND hWnd, LPCWSTR text) const;
    bool        IsCursorOverWindowBorder();
    static void RefreshCursor();
    void        ShowEditBalloon(UINT nId, LPCWSTR title, LPCWSTR text, int icon = TTI_ERROR);
    /**
     * Sets the transparency of the window.
     * \remark note that this also sets the WS_EX_LAYERED style!
     */
    void SetTransparency(BYTE alpha, COLORREF color = 0xFF000000);

    /**
     * Wrapper around the CWnd::EnableWindow() method, but
     * makes sure that a control that has the focus is not disabled
     * before the focus is passed on to the next control.
     */
    bool                       DialogEnableWindow(UINT nID, bool bEnable);
    void                       OnCompositionChanged();
    void                       ExtendFrameIntoClientArea(UINT leftControl, UINT topControl, UINT rightControl, UINT botomControl);
    int                        GetDlgItemTextLength(UINT nId);
    std::unique_ptr<wchar_t[]> GetDlgItemText(UINT nId);

    virtual LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
    virtual bool             PreTranslateMessage(MSG* pMsg);

    operator HWND() { return m_hwnd; }
    operator HWND() const { return m_hwnd; }

    HWND GetToolTipHWND() const { return m_hToolTips; }

protected:
    HINSTANCE   hResource;
    HWND        m_hwnd;
    CDwmApiImpl m_dwm;
    MARGINS     m_margins;

    void InitDialog(HWND hwndDlg, UINT iconID, bool bPosition = true);
    /**
    * Adjusts the size of a checkbox or radio button control.
    * Since we always make the size of those bigger than 'necessary'
    * for making sure that translated strings can fit in those too,
    * this method can reduce the size of those controls again to only
    * fit the text.
    */
    RECT AdjustControlSize(UINT nID);

    // the real message handler
    static INT_PTR CALLBACK stDlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // returns a pointer the dialog (stored as the WindowLong)
    static CDialog* GetObjectFromWindow(HWND hWnd)
    {
        return reinterpret_cast<CDialog*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

private:
    bool    m_bPseudoModal;
    bool    m_bPseudoEnded;
    INT_PTR m_iPseudoRet;
    HWND    m_hToolTips;
};
