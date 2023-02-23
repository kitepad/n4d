// This file is part of BowPad.
//
// Copyright (C) 2013-2021 - Stefan Kueng
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
#include "MainWindow.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "TempFile.h"
#include "CommandHandler.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "Theme.h"
#include "PreserveChdir.h"
#include "CmdLineParser.h"
#include "SysInfo.h"
#include "ClipboardHelper.h"
#include "DirFileEnum.h"
#include "LexStyles.h"
#include "OnOutOfScope.h"
#include "CustomTooltip.h"
#include "GDIHelpers.h"
#include "DarkModeHelper.h"
#include "KeyboardShortcutHandler.h"
#include "DPIAware.h"
#include "Monitor.h"
#include "ResString.h"
#include "../ext/tinyexpr/tinyexpr.h"

#include <memory>
#include <cassert>
#include <type_traits>
#include <future>
#include <regex>
#include <Shobjidl.h>
#include <codecvt>
#include <uxtheme.h>
#include <vssym32.h>
#include <gdiplus.h>

using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

#define ColorFrom(crBase) (Gdiplus::Color(255, GetRValue(crBase), GetGValue(crBase), GetBValue(crBase)))
#define FontFrom(theme) (Gdiplus::Font(theme.fontName.c_str(), (theme.fontSize - 1) * 1.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint))
#define HeightOf(rect)         (rect.bottom - rect.top)
#define WidthOf(rect)          (rect.right - rect.left)

namespace
{
    constexpr char   URL_REG_EXPR[]      = {"\\b[A-Za-z+]{3,9}://[A-Za-z0-9_\\-+~.:?&@=/%#,;{}()[\\]|*!\\\\]+\\b"};
    constexpr size_t URL_REG_EXPR_LENGTH = _countof(URL_REG_EXPR) - 1;
    constexpr int TIMER_UPDATECHECK = 101;
    constexpr int TIMER_SELCHANGE   = 102;
    constexpr int TIMER_CHECKLINES  = 103;
    constexpr int TIMER_DWELLEND    = 104;
    constexpr int RECENTS_LENGTH    = 10;

    ResponseToOutsideModifiedFile responseToOutsideModifiedFile      = ResponseToOutsideModifiedFile::Reload;
    BOOL                          responseToOutsideModifiedFileDoAll = FALSE;
    bool                          doModifiedAll                      = FALSE;

    bool               doCloseAll         = false;
    BOOL               closeAllDoAll      = FALSE;
    ResponseToCloseTab responseToCloseTab = ResponseToCloseTab::CloseWithoutSaving;
} // namespace

inline void GetRoundRectPath(Gdiplus::GraphicsPath* pPath, Rect r, int dia)
{
    // diameter can't exceed width or height
    if (dia > r.Width)
        dia = r.Width;
    if (dia > r.Height)
        dia = r.Height;

    // define a corner
    Rect Corner(r.X, r.Y, dia, dia);

    // begin path
    pPath->Reset();

    // top left
    pPath->AddArc(Corner, 180, 90);

    // tweak needed for radius of 10 (dia of 20)
    if (dia == 20)
    {
        Corner.Width += 1;
        Corner.Height += 1;
        r.Width -= 1;
        r.Height -= 1;
    }

    // top right
    Corner.X += (r.Width - dia - 1);
    pPath->AddArc(Corner, 270, 90);

    int  gap = (dia % 2 == 0) ? 0 : 1;
    Rect rect(r.X, r.Y + dia / 2 + gap, r.Width, r.Height - dia / 2 - gap);
    pPath->AddRectangle(rect);
    
    // end path
    pPath->CloseFigure();
}

inline bool IsHexDigitString(const char* str)
{
    for (int i = 0; str[i]; ++i)
    {
        if (!isxdigit(str[i]))
            return false;
    }
    return true;
}

inline BOOL IsLeftButtonDown()
{
    SHORT state;
    if (::GetSystemMetrics(SM_SWAPBUTTON))
        // Mouse buttons are swapped.
        state = ::GetAsyncKeyState(VK_RBUTTON);
    else
        // Mouse buttons are not swapped.
        state = ::GetAsyncKeyState(VK_LBUTTON);

    // Returns true if the left mouse button is down.
    return (state & 0x8000);
}

static void CenterRectInRect(RECT* toCenter, const RECT* outerRect)
{
    int toWidth     = toCenter->right - toCenter->left;
    int toHeight    = toCenter->bottom - toCenter->top;
    int outerWidth  = outerRect->right - outerRect->left;
    int outerHeight = outerRect->bottom - outerRect->top;

    int paddingX = (outerWidth - toWidth) / 2;
    int paddingY = (outerHeight - toHeight) / 2;

    toCenter->left   = outerRect->left + paddingX;
    toCenter->top    = outerRect->top + paddingY;
    toCenter->right  = toCenter->left + toWidth;
    toCenter->bottom = toCenter->top + toHeight;
}

static bool ShowFileSaveDialog(HWND hParentWnd, const std::wstring& title, const std::wstring fileExt, UINT extIndex, std::wstring& path)
{
    PreserveChdir      keepCwd;
    IFileSaveDialogPtr pfd;

    HRESULT hr = pfd.CreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    // Set the dialog options
    DWORD dwOptions;
    hr = pfd->GetOptions(&dwOptions);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    hr = pfd->SetOptions(dwOptions | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    hr = pfd->SetTitle(title.c_str());
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    hr = pfd->SetFileTypes(static_cast<UINT>(CLexStyles::Instance().GetFilterSpecCount()), CLexStyles::Instance().GetFilterSpecData());
    if (CAppUtils::FailedShowMessage(hr))
        return false;

    if (extIndex > 0)
    {
        hr = pfd->SetFileTypeIndex(extIndex + 1);
        if (CAppUtils::FailedShowMessage(hr))
            return false;
    }
    if (!fileExt.empty())
    {
        hr = pfd->SetDefaultExtension(fileExt.c_str());
        if (CAppUtils::FailedShowMessage(hr))
            return false;
    }

    // set the default folder to the folder of the current tab
    if (!path.empty())
    {
        std::wstring folder = CPathUtils::GetParentDirectory(path);
        if (folder.empty())
            folder = CPathUtils::GetCWD();
        auto modDir = CPathUtils::GetLongPathname(CPathUtils::GetModuleDir());
        if (_wcsicmp(folder.c_str(), modDir.c_str()) != 0)
        {
            std::wstring  filename     = CPathUtils::GetFileName(path);
            IShellItemPtr psiDefFolder = nullptr;
            hr                         = SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&psiDefFolder));
            if (!CAppUtils::FailedShowMessage(hr))
            {
                hr = pfd->SetFolder(psiDefFolder);
                if (CAppUtils::FailedShowMessage(hr))
                    return false;
                if (!filename.empty())
                {
                    hr = pfd->SetFileName(filename.c_str());
                    if (CAppUtils::FailedShowMessage(hr))
                        return false;
                }
            }
        }
    }

    // Show the save file dialog
    hr = pfd->Show(hParentWnd);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    IShellItemPtr psiResult = nullptr;
    hr                      = pfd->GetResult(&psiResult);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    PWSTR pszPath = nullptr;
    hr            = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    if (CAppUtils::FailedShowMessage(hr))
        return false;
    path = (pszPath ? pszPath : L"");
    CoTaskMemFree(pszPath);
    return true;
}

static void UpdateMenu(HMENU menu)
{
    int menuCount = GetMenuItemCount(menu);

    for (int i = 0; i < menuCount; i++)
    {
        MENUITEMINFO mii;
        // Get a handle to the Character menu.
        mii.cbSize     = sizeof(MENUITEMINFO);
        mii.dwTypeData = nullptr;
        mii.fMask      = MIIM_FTYPE | MIIM_SUBMENU | MIIM_STRING;
        GetMenuItemInfo(menu, i, TRUE, &mii);

        if (mii.fType == MFT_SEPARATOR)
            continue;

        if (!mii.hSubMenu)
        {
            int          cmd     = GetMenuItemID(menu, i);
            std::wstring strItem = CCommandHandler::Instance().GetCommandLabel(cmd);
            if (strItem.empty())
            {
                int  size      = static_cast<size_t>(mii.cch) + 1;
                auto buf       = std::make_unique<WCHAR[]>(size);
                mii.dwTypeData = buf.get();
                mii.cch        = size;
                GetMenuItemInfo(menu, i, TRUE, &mii);
                strItem.append(buf.get());
            }

            strItem.append(L"\t");
            strItem.append(CCommandHandler::Instance().GetShortCutStringForCommand(cmd));
            mii.fMask = MIIM_STRING;
            mii.cch   = static_cast<int>(strItem.size()) + 1;
            ModifyMenu(menu, i, MF_STRING | MF_BYPOSITION, cmd, strItem.c_str());
            continue;
        }

        UpdateMenu(mii.hSubMenu);
    }
}

CAboutDlg::CAboutDlg(HWND /*hParent*/)
{
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
        InitDialog(hwndDlg, 0);// IDI_BOWPAD);
        CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
        // initialize the controls
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

CMainWindow::CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx /* = nullptr*/)
    : CWindow(hInst, wcx)
    , m_statusBar(hInst)
    , m_editor(hInst)
    , m_fileTree(hInst)
    , m_progressBar(hInst)
    , m_custToolTip(hResource)
    , m_treeWidth(0)
    , m_bDragging(false)
    , m_oldPt{0, 0}
    , m_fileTreeVisible(false)
    , m_initLine(0)
    , m_insertionIndex(-1)
    , m_inMenuLoop(false)
    , m_scratchEditor(hResource)
    , m_blockCount(0)
    , m_autoCompleter(&m_editor)
    , m_dwellStartPos(-1)
    , m_bBlockAutoIndent(false)
    , m_lastCheckedLine(0)
    , m_newCount(0)
{
    m_fileTreeVisible = GetInt64(DEFAULTS_SECTION,L"FileTreeVisible", 0) != 0;
    m_scratchEditor.InitScratch(g_hRes);

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);
}

extern void findReplaceFinish();
extern void findReplaceClose();
extern void findReplaceResize();

CMainWindow::~CMainWindow()
{
    GdiplusShutdown(m_gdiplusToken);
}

void CMainWindow::UpdateTitlebarRects()
{
    int           h1        = CTheme::CurrentTheme().titleHeight;
    int           h2        = CTheme::CurrentTheme().tabHeight;
    constexpr int iconWidth = 12;
    RECT          rc;
    GetClientRect(*this, &rc);
    const int right = rc.right - 2;
    const int left  = rc.left + 2;
    const int vsPos = rc.top + h1;

    m_allRects.total    = {rc.left, rc.top, rc.right, vsPos + h2};
    m_allRects.system   = {left, rc.top, left + h1 + iconWidth, vsPos};
    m_allRects.close    = {right - h1 - iconWidth, rc.top, right, vsPos};
    m_allRects.maximize = {m_allRects.close.left - h1 - iconWidth, rc.top, m_allRects.close.left, vsPos};
    m_allRects.minimize = {m_allRects.maximize.left - h1 - iconWidth, rc.top, m_allRects.maximize.left, vsPos};
    RECT rcqbar;
    GetWindowRect(m_quickbar, &rcqbar);
    m_allRects.text = { m_allRects.system.right, rc.top, m_allRects.minimize.left - WidthOf(rcqbar), vsPos};
    m_allRects.leftRoller = { left, vsPos,  left + h2 / 2, vsPos + h2 };
    m_allRects.showMore = { right - h2, vsPos, right, vsPos + h2 };
    m_allRects.rightRoller = { left + h2 / 2, vsPos, left + h2, vsPos + h2 };
    m_allRects.tabs = { left + h2, vsPos, right - h2, vsPos + h2 };

}

TitlebarRects CMainWindow::GetTitlebarRects(bool flag) // TRUE : force to invalidate all rects
{
    if (flag)
        UpdateTitlebarRects();

    
    return m_allRects;
}

void CMainWindow::About() const
{
    CAboutDlg dlg(*this);
    dlg.DoModal(g_hRes, IDD_ABOUTBOX, *this);
}

std::wstring CMainWindow::GetAppName()
{
    auto title = LoadResourceWString(g_hRes, IDS_APP_TITLE);
    return title;
}

std::wstring CMainWindow::GetWindowClassName() const
{
    auto className = LoadResourceWString(hResource, IDC_BOWPAD);
    className += CAppUtils::GetSessionID();
    return className;
}

std::wstring CMainWindow::GetNewTabName()
{
    // Tab's start at 1, m_mewCount left at value of last ticket used.
    int          newCount = ++m_newCount;
    ResString    newRes(g_hRes, IDS_NEW_TABTITLE);
    std::wstring tabName = CStringUtils::Format(newRes, newCount);
    return tabName;
}

HWND CMainWindow::FindAppMainWindow(HWND hStartWnd, bool* isThisInstance) const
{
    std::wstring myClassName = GetWindowClassName();
    while (hStartWnd)
    {
        wchar_t classname[257]; // docs for WNDCLASS state that a class name is max 256 chars.
        GetClassName(hStartWnd, classname, _countof(classname));
        if (myClassName.compare(classname) == 0)
            break;
        hStartWnd = GetParent(hStartWnd);
    }
    if (isThisInstance)
        *isThisInstance = hStartWnd != nullptr && hStartWnd == *this;
    return hStartWnd;
}

static HBRUSH darkHBR = CreateSolidBrush(RGB(0,0,0));

bool CMainWindow::RegisterAndCreateWindow()
{
    WNDCLASSEX wcx = {sizeof(WNDCLASSEX)}; // Set size and zero out rest.
    wcx.style                  = CS_DBLCLKS;
    wcx.lpfnWndProc            = stWinMsgHandler;
    wcx.hInstance              = hResource;
    const std::wstring clsName = GetWindowClassName();
    wcx.lpszClassName          = clsName.c_str();
    wcx.hIcon                  = LoadIcon(hResource, MAKEINTRESOURCE(IDI_N4D));
    wcx.hbrBackground          = reinterpret_cast<HBRUSH>((COLOR_DESKTOP));
    wcx.hIconSm                = LoadIcon(hResource, MAKEINTRESOURCE(IDI_N4D));
    wcx.hCursor                = LoadCursor(nullptr, static_cast<LPTSTR>(IDC_ARROW)); // for resizing the tree control
    if (RegisterWindow(&wcx))
    {
        // create the window hidden, then after the window is created use the RestoreWindowPos
        // methods of the CIniSettings to show the window and move it to the saved position.
        // RestoreWindowPos uses the API SetWindowPlacement() which ensures the window is automatically
        // shown on a monitor and not outside (e.g. if the window position was saved on an external
        // monitor but that monitor is not connected now).
        if (CreateEx(WS_EX_ACCEPTFILES | WS_EX_NOINHERITLAYOUT, WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN, nullptr))
        {
            int w = static_cast<int>(GetInt64(DEFAULTS_SECTION, L"FileTreeWidth", 200));
            SetFileTreeWidth(w);
            // hide the tab and status bar so they won't show right away when
            // restoring the window: those two show a white background until properly painted.
            // After restoring and showing the main window, ResizeChildControls() is called
            // which will show those controls again.
            ShowWindow(m_statusBar, SW_HIDE);
            ShowWindow(*this, SW_SHOW);
            m_editor.StartupDone();
            PostMessage(m_hwnd, WM_AFTERINIT, 0, 0);
            return true;
        }
    }
    return false;
}

void CMainWindow::HandleEditorContextMenu(LPARAM lParam) 
{
    if (GetAsyncKeyState(VK_MENU) & 0x8000)
        return;

    POINT pt{};
    POINTSTOPOINT(pt, lParam);

    RECT                rcw;
    GetWindowRect(m_editor, &rcw);
    int mw = m_editor.Scintilla().MarginWidthN(SC_MARGE_FOLDER) 
            + m_editor.Scintilla().MarginWidthN(SC_MARGE_SYMBOL) 
            + m_editor.Scintilla().MarginWidthN(SC_MARGE_LINENUMBER); 
    rcw.right = rcw.left + mw;

    int menuIdx = PtInRect(&rcw, pt) || GetAsyncKeyState(VK_CONTROL) & 0x8000 ? 2 : 1;

    HMENU popupMenu = GetSubMenu(LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU)), menuIdx);
    UpdateMenu(popupMenu);
    if (menuIdx == 2)
    {
        CheckMenuItem(popupMenu, cmdEnableD2D, GetInt64(DEFAULTS_SECTION, L"Direct2D") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdLinenumbers, GetInt64(DEFAULTS_SECTION, L"ShowLineNumbers") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdAutoBraces, GetInt64(DEFAULTS_SECTION, L"AutoBrace") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdAutocomplete, GetInt64(DEFAULTS_SECTION, L"AutoComplete") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdAutocMatchcase, GetInt64(DEFAULTS_SECTION, L"AutoCompleteMatchCase") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdFoldingMargin, GetInt64(DEFAULTS_SECTION, L"ShowFoldingMargin") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdHighlightBrace, GetInt64(DEFAULTS_SECTION, L"HighlightBrace") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdFrameCaretLine, GetInt64(DEFAULTS_SECTION, L"CaretLineFrame") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdLineWrap, GetInt64(DEFAULTS_SECTION, L"WrapMode", 0) ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdLineWrapIndent, GetInt64(DEFAULTS_SECTION, L"WrapModeIndent") ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(popupMenu, cmdUseTabs, GetInt64(DEFAULTS_SECTION, L"UseTabs") ? MF_CHECKED : MF_UNCHECKED);
    }
    auto cmd = TrackPopupMenuEx(popupMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, pt.x, pt.y, *this, nullptr);
    DoCommand(cmd, 0);
}
struct ITEMDEF
{
    UINT    cmd;
    LPCWSTR label;
};

static std::vector<ITEMDEF> tabContextMenuItems = {
    {cmdClose, L"Close This Tab"},
    {cmdCloseAll, L"Close All Tabs"},
    {cmdCloseAllButThis, L"Close All But This"},
    {0, L""},
    {cmdCopyDir, L"Copy Directory"},
    {cmdCopyName, L"Copy File Name"},
    {cmdCopyPath, L"Copy Path"},
};

void CMainWindow::HandleTabContextMenu(LPARAM lParam)
{
    POINT pt{};
    POINTSTOPOINT(pt, lParam);
    RECT hoveredItemRect = m_allRects.tabs;
    int  idx             = -1;
    int  left            = hoveredItemRect.left;
    int  x = pt.x, y = pt.y;
    ScreenToClient(*this, &pt);

    for (int i = m_firstVisibleItemIdx; i < m_lastVisibleItemIdx + 1; i++)
    {
        int right             = left + AdjustItemWidth(m_allTabs[i].width);
        hoveredItemRect.left  = left;
        hoveredItemRect.right = right;
        left                  = right;

        if (PtInRect(&hoveredItemRect, pt))
        {
            idx = i;
            break;
        }
    }
    if (idx == -1)
        return;

    HMENU hMenu = CreatePopupMenu();
    UINT  menuItemFlags = MF_STRING;

    for (auto const& mi : tabContextMenuItems)
    {
        if (mi.cmd == 0)
            AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        else
            AppendMenu(hMenu, menuItemFlags, mi.cmd, mi.label);
    }

    auto cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, x, y, *this, nullptr);

    switch(cmd)
    {
        case cmdClose:
            CloseTab(idx);
            break;
        case cmdCloseAllButThis:
            CloseAllButThis(idx);
            break;
        case cmdCopyDir:
        case cmdCopyName:
        case cmdCopyPath:
        {
            const auto& doc = m_docManager.GetDocumentFromID(m_allTabs[idx].id);
            std::wstring str = doc.m_path;
            if (cmd == cmdCopyDir)
                str = CPathUtils::GetParentDirectory(doc.m_path);
            else if (cmd == cmdCopyName)
                str = CPathUtils::GetFileName(doc.m_path);
            
            WriteAsciiStringToClipboard(str.c_str(), *this);

            break;
        }
        default:
            DoCommand(cmd, 0);
    }       
}

void CMainWindow::ShowSystemMenu()
    {
    POINT pt = {m_allRects.system.left, m_allRects.system.bottom};
    ClientToScreen(*this, &pt);
    HMENU pop = GetSubMenu(LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU)), 0);

    auto cmd = TrackPopupMenuEx(pop, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, pt.x, pt.y, *this, nullptr);
    DoCommand(cmd, 0);
}

LRESULT CALLBACK CMainWindow::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 0;
        case WM_SETTEXT:
        {
            InvalidateRect(*this, &m_allRects.text, FALSE);
            break;
        }
        case WM_CREATE:
        {
            HandleCreate(hwnd);
            break;
        }
        case WM_NCCALCSIZE:
        {
            if (!wParam)
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
            UINT dpi = GetDpiForWindow(hwnd);

            int frame_x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
            int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
            int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

            NCCALCSIZE_PARAMS* params                = (NCCALCSIZE_PARAMS*)lParam;
            RECT*              requested_client_rect = params->rgrc;

            requested_client_rect->right -= frame_x + padding;
            requested_client_rect->left += frame_x + padding;
            requested_client_rect->bottom -= frame_y + padding;
            requested_client_rect->top += IsMaximized(hwnd) ? padding + 2 : 0;

            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hwnd, &ps);

            RECT rcPaint = ps.rcPaint;
            GetClientRect(*this, &rcPaint);

            int  width  = WidthOf(rcPaint);
            int  height = HeightOf(rcPaint);
            auto memDC   = ::CreateCompatibleDC(hdc);
            auto hBitmap    = CreateCompatibleBitmap(hdc, width, height);
            SelectObject(memDC, hBitmap);
            HBRUSH hbr    = CreateSolidBrush(CTheme::CurrentTheme().itemHover);
            FillRect(memDC, &rcPaint, hbr);
            DeleteObject(hbr);
            DrawTitlebar(memDC);
            DrawTabs(memDC);
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
            DeleteObject(hBitmap);
            DeleteDC(memDC);
            
            EndPaint(*this, &ps);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_SYSKEYDOWN:
        case WM_NCHITTEST:
        {
            POINT pt             = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            int HIT_SIZING_WIDTH = 5;
            ScreenToClient(hwnd,&pt);
            RECT rc;
            GetClientRect(hwnd,&rc);
            enum {left   = 1,top    = 2,right  = 4,bottom = 8};
            int hit = 0;
            if (pt.x < HIT_SIZING_WIDTH)
                hit |= left;
            if (pt.x > rc.right - HIT_SIZING_WIDTH)
                hit |= right;
            if (pt.y < HIT_SIZING_WIDTH)
                hit |= top;
            if (pt.y > rc.bottom - HIT_SIZING_WIDTH)
                hit |= bottom;

            if (hit & top && hit & left)
                return HTTOPLEFT;
            if (hit & top && hit & right)
                return HTTOPRIGHT;
            if (hit & bottom && hit & left)
                return HTBOTTOMLEFT;
            if (hit & bottom && hit & right)
                return HTBOTTOMRIGHT;
            if (hit & left)
                return HTLEFT;
            if (hit & top)
                return HTTOP;
            if (hit & right)
                return HTRIGHT;
            if (hit & bottom)
                return HTBOTTOM;
            
            if (m_hoveredRect == TitlebarRect::Maximize)
                return HTMAXBUTTON;
            if (pt.y < m_allRects.tabs.bottom)
                return HTCAPTION;

            return HTCLIENT;
            }
        case WM_NCMOUSEMOVE:
        {
            TitlebarRect newHoveredRect = GetHoveredRect();

            if (newHoveredRect != m_hoveredRect)
            {
                m_hoveredRect = newHoveredRect;
                if (IsLeftButtonDown() && (newHoveredRect != m_oldHoveredRect))
                {
                    m_hoveredRect = TitlebarRect::None;
                }
            }

            if (newHoveredRect == TitlebarRect::Tabs)
            {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(*this, &pt);

                m_hoveredItemRect = m_allRects.tabs;

                m_hoveredItemIdx = -1;

                int left = m_hoveredItemRect.left;

                for (int i = m_firstVisibleItemIdx; i < m_lastVisibleItemIdx + 1; i++)
                {
                    int right      = left + AdjustItemWidth(m_allTabs[i].width);
                    m_hoveredItemRect.left = left;
                    m_hoveredItemRect.right = right;
                    left           = right;

                    if (PtInRect(&m_hoveredItemRect, pt))
                    {
                        m_hoveredItemIdx = i;
                        break;
                    }
                }

            }
            else
            {
                m_hoveredItemIdx = -1;
            }

            if (m_hoveredItemIdx == -1 && IsWindowVisible(m_custToolTip))
                m_custToolTip.HideTip();

            if (m_hoveredItemIdx != -1)
            {
                if(!IsWindowVisible(m_custToolTip) || m_hoveredItemIdx != m_tipIdx)
                {
                    std::wstring sTitle = m_allTabs[m_hoveredItemIdx].path;
                    if (sTitle.empty())
                        sTitle = m_allTabs[m_hoveredItemIdx].name;
                    else
                        sTitle += L"\\" + m_allTabs[m_hoveredItemIdx].name;

                    POINT pt = { 0,0};
                    
                    pt.x = m_hoveredItemRect.right;
                    pt.y = m_allRects.total.bottom + 60;
                    ClientToScreen(hwnd, &pt);

                    m_custToolTip.ShowTip(pt, sTitle, nullptr, sTitle);
                    m_tipIdx = m_hoveredItemIdx;
                }
            }

            InvalidateRect(*this, &m_allRects.total, FALSE);

            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        case WM_NCMOUSELEAVE:
        {
            if (m_hoveredRect != TitlebarRect::None)
            {
                m_hoveredRect = TitlebarRect::None;
            }

            m_custToolTip.HideTip();
            m_hoveredItemIdx = -1;
            InvalidateRect(*this, &m_allRects.total, FALSE);

            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        case WM_NCLBUTTONDOWN:
        {
            m_oldHoveredRect = m_hoveredRect;

            if (m_hoveredRect == TitlebarRect::Close)
            {
                PostMessage(*this, WM_CLOSE, 0, 0);
                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::Minimize)
            {
                ShowWindow(*this, SW_MINIMIZE);
                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::Maximize)
            {
                int mode = IsMaximized(*this) ? SW_NORMAL : SW_MAXIMIZE;
                ShowWindow(*this, mode);
                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::LeftRoller)
            {
                int step = m_firstVisibleItemIdx == 0 ? 0 : -1;
                m_firstVisibleItemIdx += step;
                InvalidateRect(*this, &m_allRects.tabs, FALSE);
                InvalidateRect(*this, &m_allRects.leftRoller, FALSE);
                InvalidateRect(*this, &m_allRects.rightRoller, FALSE);

                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::RightRoller)
            {
                int len  = GetItemCount() - 1;
                int step = m_lastVisibleItemIdx == len ? 0 : 1;
                m_firstVisibleItemIdx += step;
                InvalidateRect(*this, &m_allRects.tabs, FALSE);
                InvalidateRect(*this, &m_allRects.leftRoller, FALSE);
                InvalidateRect(*this, &m_allRects.rightRoller, FALSE);
                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::System)
            {
                ShowSystemMenu();
                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::ShowMore)
            {
                DoCommand(cmdSelectTab, 0);
                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::Tabs)
            {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ScreenToClient(*this, &pt);

                if (m_hoveredItemIdx > -1)
                {
                    bool inClose = (pt.x > (m_hoveredItemRect.right - 20)) && (pt.x < m_hoveredItemRect.right);

                    if (inClose)
                    {
                        if (m_hoveredItemIdx == m_selected)
                            SetSelected(m_hoveredItemIdx > 0 ? m_hoveredItemIdx - 1 : m_hoveredItemIdx);

                        CloseTab(m_hoveredItemIdx);
                    }
                    else
                        SetSelected(m_hoveredItemIdx);
                }

                return 0;
            }
            
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        // If the mouse gets into the client area then no title bar buttons are hovered
        // so need to reset the hover state
        case WM_MOUSEMOVE:
        {
            if (m_bDragging) // point.y > tabrc.bottom && m_bDragging)
            {
                if ((static_cast<UINT>(wParam) & MK_LBUTTON) != 0 && (GET_X_LPARAM(lParam) != m_oldPt.x))
                {
                    SetFileTreeWidth(GET_X_LPARAM(lParam));
                    ResizeChildWindows();
                }
            }

            return true;
        }
        case WM_DESTROY:
        {
            findReplaceFinish();
            SaveRecents();
            PostQuitMessage(0);
            return 0;
        }
        case WM_SIZE:
        {
            UpdateTitlebarRects();
            ResizeChildWindows();
            InvalidateRect(*this, &m_allRects.total, FALSE);
            break;
        }
        case WM_COMMAND:
            return DoCommand(wParam, lParam);
        case WM_SYSCOMMAND:
        {
            UINT cmd = GET_SC_WPARAM(wParam);
            switch(cmd)
            {
                case SC_MAXIMIZE:
                case SC_RESTORE: 
                case SC_MINIMIZE:
                case SC_MOVE: 
                case SC_CLOSE:
                case SC_SIZE:
                {
                    if (cmd == SC_CLOSE)
                        SaveRecents();

                    DefWindowProc(hwnd, uMsg, wParam, lParam);
                    InvalidateRect(*this, nullptr, FALSE);

                    return 1;
                }
                case SC_MOUSEMENU:
                case SC_KEYMENU:
                    ShowSystemMenu();
                    return 0;
                default:
                    break;
            }
        }
        break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO& mmi      = *reinterpret_cast<MINMAXINFO*>(lParam);
            mmi.ptMinTrackSize.x = 400;
            mmi.ptMinTrackSize.y = 100;
            return 0;
        }
        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT& dis = *reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis.CtlType == ODT_TAB)
                return ::SendMessage(dis.hwndItem, WM_DRAWITEM, wParam, lParam);
        }
        break;
        case WM_NCRBUTTONDOWN:
        {
            if (m_hoveredRect == TitlebarRect::Text)
            {
                DoCommand(cmdCommandPalette, 0);
            }
            else if (m_hoveredRect == TitlebarRect::Tabs)
            {
                HandleTabContextMenu(lParam);
            }
            else if (m_hoveredRect == TitlebarRect::System)
            {
                POINT pt = {m_allRects.system.left, m_allRects.system.bottom};
                ClientToScreen(*this, &pt);
                HMENU sysMenu = GetSystemMenu(*this, FALSE);
                UINT  flags   = TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL | TPM_RETURNCMD;
                auto  command = TrackPopupMenuEx(sysMenu, flags, pt.x, pt.y, *this, nullptr);
                SendMessage(*this, WM_SYSCOMMAND, command, 0);
            }

            return true;
        }
        case WM_LBUTTONDOWN:
        {
            if (!m_bDragging) 
            {
                SetCapture(*this);
                m_bDragging = true;
            }

            return true;
        }
        case WM_LBUTTONUP:
        {
            if (!m_bDragging)
                return false;

            SetFileTreeWidth(GET_X_LPARAM(lParam));

            SetInt64(DEFAULTS_SECTION, L"FileTreeWidth", m_treeWidth);

            ReleaseCapture();
            m_bDragging = false;

            ResizeChildWindows();

            return true;
        }
        case WM_DROPFILES:
        {
            auto hDrop = reinterpret_cast<HDROP>(wParam);
            OnOutOfScope(
                DragFinish(hDrop););
            HandleDropFiles(hDrop);
        }
        break;
        case WM_COPYDATA:
        {
            if (lParam == 0)
                return 0;
            const COPYDATASTRUCT& cds = *reinterpret_cast<const COPYDATASTRUCT*>(lParam);
            switch (cds.dwData)
            {
                case CD_COMMAND_LINE:
                    HandleCopyDataCommandLine(cds);
                    break;
                case CD_COMMAND_MOVETAB:
                    return static_cast<LRESULT>(HandleCopyDataMoveTab(cds));
                default:
                    break;
            }
        }
        break;
        case WM_MOVETODESKTOP:
            PostMessage(*this, WM_MOVETODESKTOP2, wParam, lParam);
            break;
        case WM_MOVETODESKTOP2:
        {
            IVirtualDesktopManager* pvdm = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pvdm))))
            {
                if (pvdm)
                {
                    GUID guid{};
                    if (SUCCEEDED(pvdm->GetWindowDesktopId(reinterpret_cast<HWND>(lParam), &guid)))
                        pvdm->MoveWindowToDesktop(*this, guid);
                    SetForegroundWindow(*this);
                }
                pvdm->Release();
            }
            return TRUE;
        }
        case WM_AFTERINIT:
            HandleAfterInit();
            break;
        case WM_EDITORCONTEXTMENU:
            HandleEditorContextMenu(lParam);
            break;
        case WM_TABCONTEXTMENU:
            HandleTabContextMenu(lParam);
            break;
        case WM_NOTIFY:
        {
            LPNMHDR pnmHdr = reinterpret_cast<LPNMHDR>(lParam);

            if (pnmHdr == nullptr)
                return 0;
            
            const NMHDR& nmHdr = *pnmHdr;

            if (nmHdr.hwndFrom == m_quickbar)
            {
                if (nmHdr.code == static_cast<UINT>(TBN_GETINFOTIP))
                {
                    LPNMTBGETINFOTIP pInfo = reinterpret_cast<LPNMTBGETINFOTIP>(lParam);
                    auto sRes = LoadResourceWString(g_hRes, pInfo->iItem);
                    auto idx = sRes.find(L"###");
                    bool found = idx != std::wstring::npos;
                    std::wstring tip = found ? sRes.substr(0, idx) : sRes;
                    if (!tip.empty())
                    {
                        POINT pos;
                        GetCursorPos(&pos);
                        pos.y += CTheme::CurrentTheme().titleHeight * 2;
                        m_custToolTip.ShowTip(pos, tip, nullptr, tip);
                    }

                    return 0;
                }

                if (nmHdr.code == static_cast<UINT>(NM_CUSTOMDRAW))
                {
                    LPNMTBCUSTOMDRAW pCustomDraw = (LPNMTBCUSTOMDRAW)pnmHdr;
                    return HandleQuickbarCustomDraw(pCustomDraw);
                }
            }

            if (nmHdr.code == TTN_GETDISPINFO)
            {
                LPNMTTDISPINFO lpNmtdi = reinterpret_cast<LPNMTTDISPINFO>(lParam);
                HandleGetDispInfo(static_cast<int>(nmHdr.idFrom), lpNmtdi);
            }

            if (nmHdr.idFrom == reinterpret_cast<UINT_PTR>(&m_editor) || nmHdr.hwndFrom == m_editor)
            {
                return HandleEditorEvents(nmHdr, wParam, lParam);
            }
            if (nmHdr.idFrom == reinterpret_cast<UINT_PTR>(&m_fileTree) || nmHdr.hwndFrom == m_fileTree)
            {
                return HandleFileTreeEvents(nmHdr, wParam, lParam);
            }
        }
        break;

        case WM_SETFOCUS: // lParam HWND that is losing focus.
        {
            SetFocus(m_editor);
            m_editor.Scintilla().SetFocus(true);
            // the update check can show a dialog. Doing this in the
            // WM_SETFOCUS handler causes problems due to the dialog
            // having its own message queue.
            // See issue #129 https://sourceforge.net/p/bowpad-sk/tickets/129/
            // To avoid these problems, set a timer instead. The timer
            // will fire after all messages related to the focus change have
            // been handled, and then it is save to show a message box dialog.
            SetTimer(*this, TIMER_UPDATECHECK, 200, nullptr);
        }
        break;
        case WM_CLIPBOARDUPDATE:
            HandleClipboardUpdate();
            break;
        case WM_TIMER:
            switch (wParam)
            {
                case TIMER_UPDATECHECK:
                    KillTimer(*this, TIMER_UPDATECHECK);
                    CheckForOutsideChanges();
                    break;
                case TIMER_SELCHANGE:
                    KillTimer(*this, TIMER_SELCHANGE);
                    m_editor.MarkSelectedWord(false, false);
                    break;
                case TIMER_CHECKLINES:
                {
                    KillTimer(*this, TIMER_CHECKLINES);

                    auto activeLexer = static_cast<int>(m_editor.Scintilla().Lexer());
                    auto lexerData   = CLexStyles::Instance().GetLexerDataForLexer(activeLexer);
                    if (!lexerData.annotations.empty())
                    {
                        sptr_t lineSize = 1024;
                        auto   pLine    = std::make_unique<char[]>(lineSize);

                        auto eolBytes = 1;
                        switch (m_editor.Scintilla().EOLMode())
                        {
                            case Scintilla::EndOfLine::CrLf:
                                eolBytes = 2;
                                break;
                            default:
                                eolBytes = 1;
                                break;
                        }
                        auto endLine = m_editor.Scintilla().DocLineFromVisible(m_editor.Scintilla().FirstVisibleLine()) + m_editor.Scintilla().LinesOnScreen();
                        for (sptr_t line = m_lastCheckedLine; line <= endLine; ++line)
                        {
                            auto curLineSize = m_editor.Scintilla().GetLine(line, nullptr);
                            if (curLineSize <= eolBytes)
                                continue;
                            if (curLineSize > lineSize)
                            {
                                lineSize = curLineSize + 1024;
                                pLine    = std::make_unique<char[]>(lineSize);
                            }
                            m_editor.Scintilla().GetLine(line, pLine.get());
                            pLine[curLineSize - eolBytes] = 0;
                            std::string_view sLine(pLine.get(), curLineSize - eolBytes);
                            bool             textSet = false;
                            for (const auto& [sRegex, annotation] : lexerData.annotations)
                            {
                                std::regex rx(sRegex, std::regex_constants::icase);
                                if (std::regex_match(sLine.begin(), sLine.end(), rx))
                                {
                                    m_editor.Scintilla().EOLAnnotationSetText(line, annotation.c_str());
                                    m_editor.Scintilla().EOLAnnotationSetStyle(line, STYLE_FOLDDISPLAYTEXT);
                                    textSet = true;
                                    break;
                                }
                            }
                            if (!textSet)
                                m_editor.Scintilla().EOLAnnotationSetText(line, nullptr);
                        }
                        if (m_lastCheckedLine < endLine)
                            m_lastCheckedLine = endLine;
                    }
                }
                break;
                default:
                    break;
            }
            break;
        case WM_CLOSE:
            CCommandHandler::Instance().OnClose();
            if (CloseAllTabs(true))
                DestroyWindow(m_hwnd);

            break;
        case WM_STATUSBAR_MSG:
            HandleStatusBar(wParam, lParam);
            break;
        case WM_ENTERMENULOOP:
            m_inMenuLoop = true;
            break;
        case WM_EXITMENULOOP:
            m_inMenuLoop = false;
            break;
        case WM_SETCURSOR:
        {
            // Because the tab bar does not set the cursor if the mouse pointer
            // is over its 'extended' area (the area where no tab buttons are shown
            // on the right side up to the point where the scroll buttons would appear),
            // we have to set the cursor for that area here to an arrow.
            // The tab control resizes itself to the width of the area of the buttons!
            DWORD pos = GetMessagePos();
            POINT pt  = {GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};

            RECT rc;
            GetWindowRect(m_statusBar, &rc);
            int  bottom = rc.top;
            GetWindowRect(m_editor, &rc);
            int  top = rc.top;
            bottom  = rc.bottom;
            
            HWND h = ::WindowFromPoint(pt);
            if (pt.y > top && pt.y < bottom && h == *this && GetActiveWindow() == *this)
            {
                SetCursor(LoadCursor(nullptr, static_cast<LPTSTR>(IDC_SIZEWE)));
                return TRUE;
            }

            // Pass the message onto the system so the cursor adapts
            // such as changing to the appropriate sizing cursor when
            // over the window border.
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        case WM_CANHIDECURSOR:
        {
            BOOL* result = reinterpret_cast<BOOL*>(lParam);
            *result      = m_inMenuLoop ? FALSE : TRUE;
        }
        break;
        case WM_SETTINGCHANGE:
        case WM_SYSCOLORCHANGE:
        case WM_DPICHANGED:
            SendMessage(m_editor, uMsg, wParam, lParam);
            CDPIAware::Instance().Invalidate();
            CTheme::Instance().OnSysColorChanged();
            SetTheme(CTheme::Instance().IsDarkTheme());
            if (uMsg == WM_DPICHANGED)
            {
                RECT rc;
                GetWindowRect(*this, &rc);
                const RECT* rect = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(*this, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            break;
        case WM_NCLBUTTONDBLCLK:
        {
            if (m_hoveredRect == TitlebarRect::Tabs)
            {
                if (m_hoveredItemIdx == -1) 
                    OpenNewTab();

                return 0;
            }
            else if (m_hoveredRect == TitlebarRect::Text)
                return DefWindowProc(hwnd, uMsg, wParam, lParam);

            return 0;
        }
        case WM_SCICHAR:
        {
            auto ret = m_autoCompleter.HandleChar(wParam, lParam);

            if (wParam == VK_ESCAPE)
            {
                findReplaceClose();
                return 1;
            }

            return ret ? 1 : 0;
        }

        case WM_WINDOWPOSCHANGED:
        {
            findReplaceResize();
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

LRESULT CMainWindow::HandleEditorEvents(const NMHDR& nmHdr, WPARAM wParam, LPARAM lParam)
{
    if (nmHdr.code == NM_COOLSB_CUSTOMDRAW)
        return m_editor.HandleScrollbarCustomDraw(wParam, reinterpret_cast<NMCSBCUSTOMDRAW*>(lParam));
    if (nmHdr.code == NM_COOLSB_CLICK)
    {
        auto pNmcb = reinterpret_cast<NMCOOLBUTMSG*>(lParam);
        if (pNmcb->uCmdId == 0)
            return DoCommand(cmdGotoLine, 0);

        return DoCommand(pNmcb->uCmdId, 0);
    }
    SCNotification*       pScn = reinterpret_cast<SCNotification*>(lParam);
    const SCNotification& scn  = *pScn;

    m_editor.ReflectEvents(pScn);
    m_autoCompleter.HandleScintillaEvents(pScn);

    CCommandHandler::Instance().ScintillaNotify(pScn);
    switch (scn.nmhdr.code)
    {
        case SCN_SAVEPOINTREACHED:
        case SCN_SAVEPOINTLEFT:
            HandleSavePoint(scn);
            break;
        case SCN_MARGINCLICK:
            m_editor.MarginClick(pScn);
            break;
        case SCN_UPDATEUI:
            HandleUpdateUI(scn);
            break;
        case SCN_CHARADDED:
            HandleAutoIndent(scn);
            break;
        case SCN_MODIFYATTEMPTRO:
            HandleWriteProtectedEdit();
            break;
        case SCN_DWELLSTART:
            m_dwellStartPos = scn.position;
            HandleDwellStart(scn, true);
            break;
        case SCN_DWELLEND:
            if ((scn.position >= 0) && m_editor.Scintilla().CallTipActive())
                HandleDwellStart(scn, false);
            else
            {
                m_editor.Scintilla().CallTipCancel();
                m_custToolTip.HideTip();
                m_dwellStartPos = -1;
            }
            SetTimer(*this, TIMER_DWELLEND, 300, nullptr);
            break;
        case SCN_CALLTIPCLICK:
            OpenUrlAtPos(m_dwellStartPos);
            break;
        case SCN_ZOOM:
            m_editor.UpdateLineNumberWidth();
            UpdateStatusBar(false);
            break;
        case SCN_BP_MOUSEMSG:
            return HandleMouseMsg(scn);
        case SCN_MODIFIED:
        {
            if (pScn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
            {
                m_lastCheckedLine = m_editor.Scintilla().LineFromPosition(pScn->position);
                SetTimer(*this, TIMER_CHECKLINES, 300, nullptr);
            }
        }
        break;
        default:
            break;
    }
    return 0;
}

LRESULT CMainWindow::HandleFileTreeEvents(const NMHDR& nmHdr, WPARAM /*wParam*/, LPARAM lParam)
{
    LPNMTREEVIEW pNmTv = reinterpret_cast<LPNMTREEVIEWW>(lParam);
    switch (nmHdr.code)
    {
        case NM_RETURN:
        {
            bool isDir = false;
            bool isDot = false;
            auto path  = m_fileTree.GetPathForSelItem(&isDir, &isDot);
            if (!path.empty())
            {
                HandleTreePath(path, isDir, isDot);
                return TRUE;
            }
        }
        break;
        case NM_DBLCLK:
        {
            bool isDir = false;
            bool isDot = false;
            auto path  = m_fileTree.GetPathForSelItem(&isDir, &isDot);
            if (!path.empty())
            {
                HandleTreePath(path, isDir, isDot);
                PostMessage(*this, WM_SETFOCUS, TRUE, 0);
            }
        }
        return 0;
        case NM_RCLICK:
        {
            // the tree control does not get the WM_CONTEXTMENU message.
            // see http://support.microsoft.com/kb/222905 for details about why
            // so we have to work around this and handle the NM_RCLICK instead
            // and send the WM_CONTEXTMENU message from here.
            SendMessage(m_fileTree, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(m_hwnd), GetMessagePos());
        }
        break;
        case TVN_ITEMEXPANDING:
        {
            m_fileTree.ExpandItem(pNmTv->itemNew.hItem, (pNmTv->action & TVE_EXPAND) != 0);
        }
        break;
        case NM_CUSTOMDRAW:
        {
            if (CTheme::Instance().IsDarkTheme())
            {
                LPNMTVCUSTOMDRAW lpNMCustomDraw = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);
                // only do custom drawing when in dark theme
                switch (lpNMCustomDraw->nmcd.dwDrawStage)
                {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
                    case CDDS_ITEMPREPAINT:
                    {
                        if (IsWindows8OrGreater())
                        {
                            lpNMCustomDraw->clrText   = CTheme::Instance().GetThemeColor(RGB(0, 0, 0), true);
                            lpNMCustomDraw->clrTextBk = CTheme::Instance().GetThemeColor(RGB(255, 255, 255), true);
                            
                            HTREEITEM hItem = (HTREEITEM)lpNMCustomDraw->nmcd.dwItemSpec;

                            TVITEM item = {};
                            item.mask   = TVIF_PARAM;
                            item.hItem  = hItem;
                            TreeView_GetItem(m_fileTree, &item);
                            FileTreeItem* fi = reinterpret_cast<FileTreeItem*>(item.lParam);

                            if (fi->isDir && fi->isDot)
                                lpNMCustomDraw->clrText = RGB(200, 90, 20);
                            else if (TreeView_GetItemState(m_fileTree, hItem, TVIS_BOLD) & TVIS_BOLD)
                                lpNMCustomDraw->clrText = RGB(100, 250, 180); //TODO : Set marked item text color;
                        }
                        else
                        {
                            if ((lpNMCustomDraw->nmcd.uItemState & CDIS_SELECTED) != 0 &&
                                (lpNMCustomDraw->nmcd.uItemState & CDIS_FOCUS) == 0)
                            {
                                lpNMCustomDraw->clrTextBk = CTheme::Instance().GetThemeColor(RGB(255, 255, 255), true);
                                lpNMCustomDraw->clrText   = RGB(128, 128, 128);
                            }
                        }

                        return CDRF_DODEFAULT;
                    }
                    default:
                        break;
                }
                return CDRF_DODEFAULT;
            }
        }
        break;
        default:
            break;
    }
    return 0;
}

void CMainWindow::HandleTreePath(const std::wstring& path, bool isDir, bool isDot)
{
    if (!isDir)
    {
        bool         control   = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        unsigned int openFlags = OpenFlags::IgnoreIfMissing;
        if (control)
            openFlags |= OpenFlags::OpenIntoActiveTab;
        OpenFile(path, openFlags);
        return;
    }

    if (isDot)// && GetInt64(DEFAULTS_SECTION, L"FileTreeGotoParent", 0) != 0)
        m_fileTree.SetPath(path);
}

std::vector<std::wstring> CMainWindow::GetFileListFromGlobPath(const std::wstring& path)
{
    std::vector<std::wstring> results;
    if (path.find_first_of(L"*?") == std::string::npos)
    {
        results.push_back(path);
        return results;
    }
    CDirFileEnum enumerator(path);
    bool         bIsDir = false;
    std::wstring enumPath;
    while (enumerator.NextFile(enumPath, &bIsDir, false))
    {
        if (!bIsDir)
        {
            results.push_back(enumPath);
        }
    }

    return results;
}

static int  g_marginWidth = -1;
void CMainWindow::HandleStatusBar(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
        case WM_CONTEXTMENU:
        {
            if (lParam == STATUSBAR_TABSPACE && m_editor.Scintilla().UseTabs())
            {
                DoCommand(cmdTabSize, 0);
            }
        }
        break;
        case WM_LBUTTONDOWN:
        {
            switch (lParam)
            {
                case STATUSBAR_UNICODE_TYPE:
                    DoCommand(cmdSelectEncoding, 0);
                    break;
                case STATUSBAR_DOC_TYPE:
                    DoCommand(cmdSelectLexer, 0);
                    break;
                case STATUSBAR_ZOOM:
                    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
                        m_editor.Scintilla().SetZoom(0);
                    else
                        HandleStatusBarZoom();
                    break;
                case STATUSBAR_EOL_FORMAT:
                    HandleStatusBarEOLFormat();
                    break;
                case STATUSBAR_TABSPACE:
                    DoCommand(cmdUseTabs, 0);
                    break;
                case STATUSBAR_R2L:
                {
                    auto biDi = m_editor.Scintilla().Bidirectional();
                    m_editor.SetReadDirection(biDi == Scintilla::Bidirectional::R2L ? Scintilla::Bidirectional::Disabled : Scintilla::Bidirectional::R2L);
                    auto& doc     = m_docManager.GetModDocumentFromID(GetCurrentTabId());
                    doc.m_readDir = m_editor.Scintilla().Bidirectional();
                }
                break;
                case STATUSBAR_TYPING_MODE:
                    m_editor.Scintilla().EditToggleOvertype();
                    break;
                case STATUSBAR_CUR_POS:
                    DoCommand(cmdGotoLine, 0);
                    break;
                default:
                    break;
            }
            UpdateStatusBar(true);
        }
        break;
        default:
            break;
    }
}

void CMainWindow::HandleStatusBarEOLFormat()
{
    DWORD msgPos = GetMessagePos();
    int   xPos   = GET_X_LPARAM(msgPos);
    int   yPos   = GET_Y_LPARAM(msgPos);

    HMENU hPopup = CreatePopupMenu();
    if (!hPopup)
        return;
    OnOutOfScope(DestroyMenu(hPopup););
    auto                    currentEolMode   = m_editor.Scintilla().EOLMode();
    EOLFormat              currentEolFormat = toEolFormat(currentEolMode);
    static const EOLFormat options[]        = {EOLFormat::Win_Format, EOLFormat::Mac_Format, EOLFormat::Unix_Format};
    const size_t           numOptions       = std::size(options);
    for (size_t i = 0; i < numOptions; ++i)
    {
        std::wstring eolName       = getEolFormatDescription(options[i]);
        UINT         menuItemFlags = MF_STRING;
        if (options[i] == currentEolFormat)
            menuItemFlags |= MF_CHECKED | MF_DISABLED;
        AppendMenu(hPopup, menuItemFlags, i + 1, eolName.c_str());
    }
    auto result = TrackPopupMenu(hPopup, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, xPos, yPos, 0, *this, nullptr);
    if (result != FALSE)
    {
        size_t optionIndex       = static_cast<size_t>(result) - 1;
        auto   selectedEolFormat = options[optionIndex];
        if (selectedEolFormat != currentEolFormat)
        {
            auto selectedEolMode = toEolMode(selectedEolFormat);
            m_editor.Scintilla().SetEOLMode(selectedEolMode);
            m_editor.Scintilla().ConvertEOLs(selectedEolMode);
            auto id = GetCurrentTabId();
            if (m_docManager.HasDocumentID(id))
            {
                auto& doc    = m_docManager.GetModDocumentFromID(id);
                doc.m_format = selectedEolFormat;
                UpdateStatusBar(true);
            }
        }
    }
}

void CMainWindow::HandleStatusBarZoom()
{
    DWORD msgPos = GetMessagePos();
    int   xPos   = GET_X_LPARAM(msgPos);
    int   yPos   = GET_Y_LPARAM(msgPos);

    HMENU hPopup = CreatePopupMenu();
    if (hPopup)
    {
        OnOutOfScope(
            DestroyMenu(hPopup););
        AppendMenu(hPopup, MF_STRING, 25, L"25%");
        AppendMenu(hPopup, MF_STRING, 50, L"50%");
        AppendMenu(hPopup, MF_STRING, 75, L"75%");
        AppendMenu(hPopup, MF_STRING, 100, L"100%");
        AppendMenu(hPopup, MF_STRING, 125, L"125%");
        AppendMenu(hPopup, MF_STRING, 150, L"150%");
        AppendMenu(hPopup, MF_STRING, 175, L"175%");
        AppendMenu(hPopup, MF_STRING, 200, L"200%");

        // Note the font point size is a factor in what determines the zoom percentage the user actually gets.
        // So the values above are estimates. However, the status bar shows the true/exact setting that resulted.
        // This might be necessary as the user can adjust the zoom through ctrl + mouse wheel to get finer/other settings
        // than we offer here. We could round and lie to make the status bar and estimates match (meh) or
        // we could show real size in the menu to be consistent (arguably better), but we don't take either approach yet,
        // and opt to show the user nice instantly relate-able percentages that we then don't quite honor precisely in practice.
        auto cmd = TrackPopupMenu(hPopup, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, xPos, yPos, 0, *this, nullptr);
        if (cmd != 0)
            SetZoomPC(cmd);
    }
}

LRESULT CMainWindow::DoCommand(WPARAM wParam, LPARAM /*lParam*/)
{
    auto id = LOWORD(wParam);
    switch (id)
    {
        case cmdAutocMatchcase:
            SetInt64(DEFAULTS_SECTION, L"AutoCompleteMatchCase", GetInt64(DEFAULTS_SECTION, L"AutoCompleteMatchCase") ? 0 : 1);
            break;
        case cmdExit:
            PostMessage(m_hwnd, WM_CLOSE, 0, 0);
            break;
        case cmdNew:
            OpenNewTab();
            InvalidateRect(*this, &m_allRects.tabs, FALSE);
            break;
        case cmdClose:
            CloseTab(GetCurrentTabIndex());
            break;
        case cmdCloseAll:
            CloseAllTabs();
            break;
        case cmdPasteHistory:
            PasteHistory();
            break;
        case cmdAbout:
            About();
            break;
            
        default:
        {
            ICommand* pCmd = CCommandHandler::Instance().GetCommand(id);
            if (pCmd)
                pCmd->Execute();
        }
        break;
    }
    return 1;
}

bool CMainWindow::Initialize()
{
    UpdateTitlebarRects();
    
    m_fileTree.Init(*this);
    
    SendMessage(m_fileTree, WM_SETREDRAW, FALSE, 0);
    OnOutOfScope(SendMessage(m_fileTree, WM_SETREDRAW, TRUE, 0));

    CCommandHandler::Instance().AddCommand(cmdNew);
    CCommandHandler::Instance().AddCommand(cmdClose);
    CCommandHandler::Instance().AddCommand(cmdCloseAll);
    CCommandHandler::Instance().AddCommand(cmdCloseAllButThis);
    CCommandHandler::Instance().AddCommand(cmdPasteHistory);
    CCommandHandler::Instance().AddCommand(cmdAbout);

    m_editor.Init(hResource, *this);
    SendMessage(m_editor, WM_SETREDRAW, FALSE, 0);
    OnOutOfScope(SendMessage(m_editor, WM_SETREDRAW, TRUE, 0));
    m_autoCompleter.Init();
    m_statusBar.Init(*this, true);
    SendMessage(m_statusBar, WM_SETREDRAW, FALSE, 0);
    OnOutOfScope(SendMessage(m_statusBar, WM_SETREDRAW, TRUE, 0));
    m_progressBar.Init(hResource, *this);
    static ResString rsClickToCopy(g_hRes, 0);//IDS_CLICKTOCOPYTOCLIPBOARD);
    m_custToolTip.Init(m_editor, *this, rsClickToCopy);

    // Tell UAC that lower integrity processes are allowed to send WM_COPYDATA messages to this process (or window)
    HMODULE hDll = GetModuleHandle(TEXT("user32.dll"));
    if (hDll)
    {
        // first try ChangeWindowMessageFilterEx, if it's not available (i.e., running on Vista), then
        // try ChangeWindowMessageFilter.
        using MESSAGEFILTERFUNCEX = BOOL(WINAPI*)(HWND hWnd, UINT message, DWORD action, VOID * pChangeFilterStruct);
        MESSAGEFILTERFUNCEX func  = reinterpret_cast<MESSAGEFILTERFUNCEX>(GetProcAddress(hDll, "ChangeWindowMessageFilterEx"));

        constexpr UINT WM_COPYGLOBALDATA = 0x0049;
        if (func)
        {
            // note:
            // this enabled dropping files from e.g. explorer, but only
            // on the main window, status bar, tab bar and the file tree.
            // it does NOT work on the scintilla window because scintilla
            // calls RegisterDragDrop() - and OLE drag'n'drop does not work
            // between different elevation levels! Unfortunately, once
            // a window calls RegisterDragDrop() the WM_DROPFILES message also
            // won't work anymore...
            func(*this, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
            func(*this, WM_COPYGLOBALDATA, MSGFLT_ALLOW, nullptr);
            func(*this, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
        }
        else
        {
            ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
            ChangeWindowMessageFilter(WM_COPYGLOBALDATA, MSGFLT_ADD);
            ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
        }
    }

    SetTheme(CTheme::Instance().IsDarkTheme());
    CTheme::Instance().RegisterThemeChangeCallback(
        [this]() {
            SetTheme(CTheme::Instance().IsDarkTheme());
        });

    CCommandHandler::Instance().Init(this);
    CKeyboardShortcutHandler::Instance();
    CKeyboardShortcutHandler::Instance().UpdateTooltips();

    AddClipboardFormatListener(*this);
    
    constexpr UINT flags       = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER ;//| SWP_SHOWWINDOW;// | SWP_NOCOPYBITS;
    RECT           rect;
    GetClientRect(*this, &rect);
    const int treeWidth      = m_fileTreeVisible ? m_treeWidth : 0;
    const int stHeight       = m_statusBar.GetHeight();
    const int titlebarHeight = HeightOf(m_allRects.text); // GetTitlebarRect());
    const int tabbarHeight   = HeightOf(m_allRects.tabs); // GetTabbarRect());
    const int height         = HeightOf(rect) - tabbarHeight - titlebarHeight - stHeight;
    const int topPos         = rect.top + titlebarHeight + tabbarHeight;

    if (m_fileTreeVisible)
    {
        SendMessage(m_hwnd, WM_SETREDRAW, FALSE, 0);
        SetWindowPos(m_fileTree, nullptr, 0, topPos, treeWidth ? treeWidth - 3 : 0, height, flags);
        SendMessage(m_hwnd, WM_SETREDRAW, TRUE, 0);
    }

    ////Init and setup quickbar
    // Declare and initialize local constants.
    const int numButtons = 12;
    const int bitmapSize = 20;
    const DWORD buttonStyles = TBSTYLE_BUTTON;
    const DWORD tbStyles = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN;
    
    // Create the toolbar.
    m_quickbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, tbStyles, 0, 0, 0, 0, m_hwnd, NULL, g_hInst, NULL);
    m_quickbarImages = ImageList_Create(bitmapSize, bitmapSize, ILC_COLOR32 | ILC_MASK, 20, 0);
    ImageList_AddMasked(m_quickbarImages, LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_QUICKBAR)), RGB(192, 192, 192));
    SendMessage(m_quickbar, TB_SETIMAGELIST, 0, (LPARAM)(m_quickbarImages));

    // Declare and Add buttons to quickbar.
    TBBUTTON tbButtons[numButtons] =
    {
        {0, cmdOpenRecent, 0, buttonStyles, {0}, 0, 0},
        {1, cmdNew, TBSTATE_ENABLED, buttonStyles, {0}, 0, 0},
        {2, cmdOpen, TBSTATE_ENABLED, buttonStyles, {0}, 0, 0},
        {3, cmdOpenFolder, TBSTATE_ENABLED, buttonStyles, {0}, 0, 0},
        {4, cmdSave, TBSTATE_ENABLED, buttonStyles, {0}, 0, 0},
        {5, cmdSaveAs, TBSTATE_ENABLED, buttonStyles, {0}, 0, 0},
        {6, cmdPrint, TBSTATE_ENABLED, buttonStyles, {0}, 0, 0},
        {0, 0, 0, TBSTYLE_SEP, {0}, 0, 0},
        {7, cmdFileTree, TBSTATE_ENABLED, buttonStyles | TBSTYLE_CHECK, {0}, 0, 0},
        {8, cmdToggleTheme, TBSTATE_ENABLED, buttonStyles, {0}, 0, 0},
        {9, cmdConfigStyle, TBSTATE_ENABLED, buttonStyles , {0}, 0, 0},
        {0, 0, 0, TBSTYLE_SEP, {0}, 0, 0},
    };

    SendMessage(m_quickbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(m_quickbar, TB_ADDBUTTONS, (WPARAM)numButtons, (LPARAM)&tbButtons);
    SendMessage(m_quickbar, TB_CHECKBUTTON, cmdFileTree, m_fileTreeVisible);

    //Load recents from registry
    HKEY hKey = nullptr;
    std::wstring valueName = L"Recents";

    if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\n4d", &hKey) == ERROR_SUCCESS)
        m_recents = CAppUtils::GetMultiStringValue(hKey, valueName);
    RegCloseKey(hKey);

    for (auto pItem = m_recents.begin(); pItem < m_recents.end(); pItem++)
    {
        std::wstring item = *pItem;
        if(item.empty())
            m_recents.erase(pItem);
    }
    if(m_recents.size() > 0)
        SendMessage(m_quickbar, TB_SETSTATE, cmdOpenRecent, TBSTATE_ENABLED);

    
    //Refresh titlebar
    RedrawWindow(*this, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASENOW);

    return true;
}

void CMainWindow::SaveRecents()
{
    HKEY hKey = nullptr;
    std::wstring valueName = L"Recents";

    if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\n4d", &hKey) != ERROR_SUCCESS)
    {
        // but the default icon hasn't been set yet: set the default icon now
        if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\n4d", &hKey) == ERROR_SUCCESS)
        {
            CAppUtils::SetMultiStringValue(hKey, valueName, m_recents);
        }
    }else
        CAppUtils::SetMultiStringValue(hKey, valueName, m_recents);

    RegCloseKey(hKey);
}

void CMainWindow::HandleCreate(HWND hwnd)
{
    m_hwnd = hwnd;
    Initialize();
}

void CMainWindow::HandleAfterInit()
{
    ResizeChildWindows();
    UpdateWindow(*this);

    CCommandHandler::Instance().BeforeLoad();

    if ((m_pathsToOpen.size() == 1) && (PathIsDirectory(m_pathsToOpen.begin()->first.c_str())))
    {
        OpenNewTab();
        OpenFolder(m_pathsToOpen.begin()->first);
    }
    else if (m_pathsToOpen.size() > 0)
    {
        unsigned int openFlags = OpenFlags::AskToCreateIfMissing;
        BlockAllUIUpdates(true);
        ShowProgressCtrl(static_cast<UINT>(GetInt64(DEFAULTS_SECTION, L"ProgressDelay", 1000)));
        OnOutOfScope(HideProgressCtrl());

        int fileCounter = 0;
        for (const auto& [path, line] : m_pathsToOpen)
        {
            ++fileCounter;
            SetProgress(fileCounter, static_cast<DWORD>(m_pathsToOpen.size()));
            if (OpenFile(path, openFlags) >= 0)
            {
                if (line != static_cast<size_t>(-1))
                    GoToLine(line);
            }
        }
        BlockAllUIUpdates(false);
        if (IsFileTreeShown())
        {
            m_fileTree.SetPath(CPathUtils::GetParentDirectory(m_pathsToOpen.begin()->first));
            m_fileTree.MarkActiveDocument(true);
        }
    } else
        EnsureAtLeastOneTab();
    
    m_pathsToOpen.clear();
    CCommandHandler::Instance().AfterInit();
    g_marginWidth = m_editor.Scintilla().MarginWidthN(SC_MARGIN_BACK); 
    m_bIsAfterInit = true;
}
/// Utility method to calculate toolbar size in actually.
static auto getQuickbarSize = [](HWND quickbar) -> SIZE {
    int count = static_cast<int>(SendMessage(quickbar, TB_BUTTONCOUNT, 0, 0));

    SIZE   sz;
    ZeroMemory(&sz, sizeof(sz));

    LPARAM lparam = reinterpret_cast<LPARAM>(&sz);
    SendMessage(quickbar, TB_GETMAXSIZE, 0, lparam);

    // This fixes a Windows bug calculating the size when TBSTYLE_DROPDOWN is used.
    int cxMaxSize = 0;
    int cyMaxSize = 0;
    for (int i = 0; i < count; ++i)
    {
        RECT   itemRect;
        WPARAM wparam = static_cast<WPARAM>(i);
        lparam = reinterpret_cast<LPARAM>(&itemRect);
        SendMessage(quickbar, TB_GETITEMRECT, wparam, lparam);
        cxMaxSize += WidthOf(itemRect);
        cyMaxSize += HeightOf(itemRect);
    }

    sz.cx = cxMaxSize;

    return sz;
};

void CMainWindow::ResizeChildWindows()
{
    RECT rect;
    GetClientRect(*this, &rect);
    // if the main window is not visible (yet) or the UI is blocked,
    // then don't resize the child controls.
    // as soon as the UI is unblocked, ResizeChildWindows() is called
    // again.
    if (!IsRectEmpty(&rect) && IsWindowVisible(*this))
    {
        constexpr UINT flags       = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;
        constexpr UINT noShowFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOCOPYBITS ;
        
        const int treeWidth   = m_fileTreeVisible ? m_treeWidth : 0;
        const int stHeight = m_statusBar.GetHeight();
        const int titlebarHeight = HeightOf(m_allRects.text);
        const int tabbarHeight = HeightOf(m_allRects.tabs);
        const int height      = HeightOf(rect) - tabbarHeight - titlebarHeight - stHeight;
        const int topPos         = rect.top + titlebarHeight + tabbarHeight;
        const int width          = rect.right - rect.left;
        const SIZE qbarSize     = getQuickbarSize(m_quickbar);
        int yPos = rect.top + (titlebarHeight - qbarSize.cy) / 2 + 1;
        int xPos = GetTitlebarRects().minimize.left - qbarSize.cx;
        int qbarHeight = titlebarHeight - yPos;

        HDWP hDwp = BeginDeferWindowPos(4);
        DeferWindowPos(hDwp, m_quickbar, nullptr, xPos, yPos, qbarSize.cx, qbarHeight, flags);
        DeferWindowPos(hDwp, m_statusBar, nullptr, rect.left, rect.bottom - stHeight, width, stHeight, flags);
        DeferWindowPos(hDwp, m_editor, nullptr, rect.left + treeWidth, topPos, width - treeWidth, height, flags);
        DeferWindowPos(hDwp, m_fileTree, nullptr, rect.left, topPos, treeWidth ? treeWidth - 3 : 0, height, m_fileTreeVisible ? flags : noShowFlags);
        EndDeferWindowPos(hDwp);
        InvalidateRect(m_hwnd, nullptr, TRUE);
        findReplaceResize();
    }
}

void CMainWindow::EnsureNewLineAtEnd(const CDocument& doc) const
{
    size_t endPos = m_editor.Scintilla().Length();
    char   c      = static_cast<char>(m_editor.Scintilla().CharAt(endPos - 1));
    if ((c != '\r') && (c != '\n'))
    {
        switch (doc.m_format)
        {
            case EOLFormat::Win_Format:
                m_editor.AppendText(2, "\r\n");
                break;
            case EOLFormat::Mac_Format:
                m_editor.AppendText(1, "\r");
                break;
            case EOLFormat::Unix_Format:
            default:
                m_editor.AppendText(1, "\n");
                break;
        }
    }
}

bool CMainWindow::SaveCurrentTab(bool bSaveAs /* = false */)
{
    auto docID = GetCurrentTabId();
    return SaveDoc(docID, bSaveAs);
}

bool CMainWindow::SaveDoc(DocID docID, bool bSaveAs)
{
    if (!docID.IsValid())
        return false;
    if (!m_docManager.HasDocumentID(docID))
        return false;

    auto& doc = m_docManager.GetModDocumentFromID(docID);
    if (!bSaveAs && !doc.m_bIsDirty && !doc.m_bNeedsSaving)
        return false;

    auto isActiveTab    = docID == GetCurrentTabId();
    bool updateFileTree = false;
    if (doc.m_path.empty() || bSaveAs || doc.m_bDoSaveAs)
    {
        bSaveAs = true;
        std::wstring filePath;

        std::wstring title    = GetAppName();
        std::wstring fileName = GetTabTitle(GetIndexFromID(docID));
        title += L" - ";
        title += fileName;
        // Do not change doc.m_path until the user determines to save
        if (doc.m_path.empty())
        {
            if (m_fileTree.GetPath().empty())
                filePath = fileName;
            else
                filePath = m_fileTree.GetPath() + L"\\" + fileName;
        }
        else
            filePath = doc.m_path;

        UINT         extIndex = 0;
        std::wstring ext;
        // if there's a lexer active, get the default extension and default filet type index in filter types
        CLexStyles::Instance().GetDefaultExtensionForLanguage(doc.GetLanguage(), ext, extIndex);

        if (!ShowFileSaveDialog(*this, title, ext, extIndex, filePath))
            return false;
        doc.m_path = filePath;
        if ((isActiveTab && m_fileTree.GetPath().empty()) || bSaveAs)
        {
            updateFileTree = true;
        }
    }
    if (!doc.m_path.empty())
    {
        doc.m_bDoSaveAs = false;
        if (doc.m_bTrimBeforeSave)
        {
            auto cmd = CCommandHandler::Instance().GetCommand(cmdTrim);
            cmd->Execute();
        }

        if (doc.m_bEnsureNewlineAtEnd)
            EnsureNewLineAtEnd(doc);

        bool bTabMoved = false;
        if (!m_docManager.SaveFile(/**this,*/ doc, bTabMoved))
        {
            return false;
        }
        if (doc.m_saveCallback)
            doc.m_saveCallback();

        if (CPathUtils::PathCompare(CIniSettings::Instance().GetIniPath(), doc.m_path) == 0)
            CIniSettings::Instance().Reload();

        doc.m_bIsDirty     = false;
        doc.m_bNeedsSaving = false;
        m_docManager.UpdateFileTime(doc, false);
        if (bSaveAs)
        {
            const auto& lang = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
            if (isActiveTab)
            {
                m_editor.SetupLexerForLang(lang);
                RefreshAnnotations();
            }
            doc.SetLanguage(lang);
        }
        // Update tab so the various states are updated (readonly, modified, ...)
        UpdateTab(docID);
        if (isActiveTab)
        {
            int          idx       = GetIndexFromID(docID);
            std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
            SetTabTitle(idx, sFileName.c_str());
            m_allTabs[idx].name = sFileName;
            m_allTabs[idx].path = CPathUtils::GetParentDirectory(doc.m_path);
            UpdateCaptionBar();
            UpdateStatusBar(true);
            m_editor.Scintilla().SetSavePoint();
            m_editor.EnableChangeHistory();
        }
        if (updateFileTree)
        {
            m_fileTree.SetPath(CPathUtils::GetParentDirectory(doc.m_path), bSaveAs);
        }
        CCommandHandler::Instance().OnDocumentSave(docID, bSaveAs);
    }
    return true;
}

bool CMainWindow::SaveDoc(DocID docID, const std::wstring& path)
{
    if (!docID.IsValid())
        return false;
    if (!m_docManager.HasDocumentID(docID))
        return false;
    if (path.empty())
        return false;

    auto& doc = m_docManager.GetModDocumentFromID(docID);

    if (!m_docManager.SaveFile(/**this,*/ doc, path))
    {
        return false;
    }
    if (doc.m_saveCallback)
        doc.m_saveCallback();
    return true;
}

void CMainWindow::EnsureAtLeastOneTab()
{
    if (GetItemCount() == 0)
        OpenNewTab();

    InvalidateRect(*this, &m_allRects.tabs, FALSE);
}

void CMainWindow::GoToLine(size_t line)
{
    m_editor.GotoLine(static_cast<long>(line));
}

int CMainWindow::GetZoomPC() const
{
    int fontSize   = static_cast<int>(m_editor.Scintilla().StyleGetSize(STYLE_DEFAULT));
    int zoom       = static_cast<int>(m_editor.Scintilla().Zoom());
    int zoomFactor = (fontSize + zoom) * 100 / fontSize;
    if (zoomFactor == 0)
        zoomFactor = 100;
    return zoomFactor;
}

void CMainWindow::SetZoomPC(int zoomPC) const
{
    int fontSize = static_cast<int>(m_editor.Scintilla().StyleGetSize(STYLE_DEFAULT));
    int zoom     = (fontSize * zoomPC / 100) - fontSize;
    m_editor.Scintilla().SetZoom(zoom);
}

void CMainWindow::UpdateStatusBar(bool bEverything)
{
    static ResString rsStatusTTDocSize(g_hRes, IDS_STATUSTTDOCSIZE);         // Length in bytes: %ld\r\nLines: %ld
    static ResString rsStatusTTCurPos(g_hRes, IDS_STATUSTTCURPOS);           // Line : %ld\r\nColumn : %ld\r\nSelection : %Iu | %Iu\r\nMatches: %ld
    static ResString rsStatusTTEOL(g_hRes, IDS_STATUSTTEOL);                 // Line endings: %s
    static ResString rsStatusTTTyping(g_hRes, IDS_STATUSTTTYPING);           // Typing mode: %s
    static ResString rsStatusTTTypingOvl(g_hRes, IDS_STATUSTTTYPINGOVL);     // Overtype
    static ResString rsStatusTTTypingIns(g_hRes, IDS_STATUSTTTYPINGINS);     // Insert
    static ResString rsStatusSelection(g_hRes, IDS_STATUSSELECTION);         // Sel: %Iu chars | %Iu lines | %ld matches.
    static ResString rsStatusSelectionLong(g_hRes, IDS_STATUSSELECTIONLONG); // Selection: %Iu chars | %Iu lines | %ld matches.
    static ResString rsStatusSelectionNone(g_hRes, IDS_STATUSSELECTIONNONE); // no selection
    static ResString rsStatusTTTabSpaces(g_hRes, IDS_STATUSTTTABSPACES);     // Insert Tabs or Spaces
    static ResString rsStatusTTR2L(g_hRes, IDS_STATUSTTR2L);                 // Reading order (left-to-right or right-to-left)
    static ResString rsStatusTTEncoding(g_hRes, IDS_STATUSTTENCODING);       // Encoding: %s
    static ResString rsStatusZoom(g_hRes, IDS_STATUSZOOM);                   // Zoom: %d%%
    static ResString rsStatusCurposLong(g_hRes, IDS_STATUS_CURPOSLONG);      // Line: %ld / %ld   Column: %ld
    static ResString rsStatusCurpos(g_hRes, IDS_STATUS_CURPOS);              // Ln: %ld / %ld    Col: %ld

    auto lineCount = static_cast<long>(m_editor.Scintilla().LineCount());

    sptr_t selByte = 0;
    sptr_t selLine = 0;
    m_editor.GetSelectedCount(selByte, selLine);
    auto selTextMarkerCount = m_editor.GetSelTextMarkerCount();
    auto curPos             = m_editor.Scintilla().CurrentPos();
    long line               = static_cast<long>(m_editor.Scintilla().LineFromPosition(curPos)) + 1;
    long column             = static_cast<long>(m_editor.Scintilla().Column(curPos)) + 1;
    auto lengthInBytes      = m_editor.Scintilla().Length();
    auto bidi               = m_editor.Scintilla().Bidirectional();

    wchar_t readableLength[100] = {0};
    StrFormatByteSize(lengthInBytes, readableLength, _countof(readableLength));

    auto numberColor = 0x600000;
    if (CTheme::Instance().IsHighContrastModeDark())
        numberColor = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT));
    else if (CTheme::Instance().IsHighContrastMode())
        numberColor = CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOWTEXT));

    m_statusBar.SetPart(0, STATUSBAR_CUR_POS,
                        CStringUtils::Format(rsStatusCurposLong, numberColor, line, numberColor, lineCount, numberColor, column),
                        CStringUtils::Format(rsStatusCurpos, numberColor, line, numberColor, lineCount, numberColor, column),
                        CStringUtils::Format(rsStatusTTDocSize, lengthInBytes, readableLength, lineCount),
                        0,
                        0,
                        2,
                        true,true);
    auto sNoSel = CStringUtils::Format(rsStatusSelectionNone, GetSysColor(COLOR_GRAYTEXT));
    m_statusBar.SetPart(0,STATUSBAR_SEL,
                        selByte ? CStringUtils::Format(rsStatusSelectionLong, numberColor, selByte, numberColor, selLine, (selTextMarkerCount ? 0x008000 : numberColor), selTextMarkerCount) : sNoSel,
                        selByte ? CStringUtils::Format(rsStatusSelection, numberColor, selByte, numberColor, selLine, (selTextMarkerCount ? 0x008000 : numberColor), selTextMarkerCount) : sNoSel,
                        CStringUtils::Format(rsStatusTTCurPos, line, column, selByte, selLine, selTextMarkerCount),
                        0,
                        0,
                        0,
                        false, true);
    auto overType = m_editor.Scintilla().Overtype();
    m_statusBar.SetPart(0,STATUSBAR_TYPING_MODE, 
                    overType ? L"OVR" : L"INS", L"", 
                    CStringUtils::Format(rsStatusTTTyping, overType ? rsStatusTTTypingOvl.c_str() : rsStatusTTTypingIns.c_str()),
                    0, 0, 2, true, true);

    bool usingTabs = m_editor.Scintilla().UseTabs() ? true : false;
    int  tabSize   = static_cast<int>(m_editor.Scintilla().TabWidth());
    m_statusBar.SetPart(0,STATUSBAR_TABSPACE, usingTabs ? CStringUtils::Format(L"Tab Width: %d", tabSize) : L"Spaces",
                        L"", rsStatusTTTabSpaces, 0, 0, 2, true,true);
    m_statusBar.SetPart(0,STATUSBAR_R2L, bidi == Scintilla::Bidirectional::R2L ? L"R2L" : L"L2R",
                        L"", rsStatusTTR2L, 0, 0, 2, true,true);

    int  zoomFactor = GetZoomPC();
    auto sZoom      = CStringUtils::Format(rsStatusZoom, numberColor, zoomFactor);
    auto sZoomTT    = CRichStatusBar::GetPlainString(sZoom);
    m_statusBar.SetPart(0,STATUSBAR_ZOOM, sZoomTT,L"", sZoomTT,
        0, 0, 2, true, true);

    if (bEverything)
    {
        const auto& doc   = m_docManager.GetDocumentFromID(GetCurrentTabId());
        auto        sLang = CUnicodeUtils::StdGetUnicode(doc.GetLanguage());
        m_statusBar.SetPart(0,STATUSBAR_DOC_TYPE, L"%b" + sLang, L"", sLang,
         0, 0, 2, true, true);

        auto eolDesc = getEolFormatDescription(doc.m_format);
        m_statusBar.SetPart(0, STATUSBAR_EOL_FORMAT,
                            (doc.m_format == EOLFormat::Win_Format) ? L"CR/LF" : (doc.m_format == EOLFormat::Mac_Format ? L"CR" : L"LF"),
                            L"", CStringUtils::Format(rsStatusTTEOL, eolDesc.c_str()),
                            0, 0, 2, true, true);

        m_statusBar.SetPart(0,STATUSBAR_UNICODE_TYPE, doc.GetEncodingString(), L"",
                            CStringUtils::Format(rsStatusTTEncoding, doc.GetEncodingString().c_str()),
                            0, 0, 2, true, true);
    }
    m_statusBar.CalcWidths();
}

bool CMainWindow::CloseTab(int closingTabIndex, bool force /* = false */, bool quitting)
{
    if (closingTabIndex < 0 || closingTabIndex >= GetItemCount())
    {
        return false;
    }
    
    auto             closingTabId = GetIDFromIndex(closingTabIndex);
    auto             currentTabId = GetCurrentTabId();
    const CDocument& closingDoc   = m_docManager.GetDocumentFromID(closingTabId);
    if (!force && (closingDoc.m_bIsDirty || closingDoc.m_bNeedsSaving))
    {
        if (closingTabId != currentTabId)
            SetSelected(closingTabIndex);
        if (!doCloseAll || !closeAllDoAll)
        {
            auto bc            = UnblockUI();
            responseToCloseTab = AskToCloseTab();
            ReBlockUI(bc);
        }

        if (responseToCloseTab == ResponseToCloseTab::SaveAndClose)
        {
            if (!SaveCurrentTab()) // Save And (fall through to) Close
                return false;
        }
        else if (responseToCloseTab != ResponseToCloseTab::CloseWithoutSaving)
        {
            // Cancel And Stay Open
            // activate the tab: user might have clicked another than
            // the active tab to close: user clicked on that tab so activate that tab now
            
            SetSelected(closingTabIndex);
            return false;
        }
        // If the save successful or closed without saveing, the tab will be closed.
    }
    CCommandHandler::Instance().OnDocumentClose(closingTabId);
    // Prefer to remove the document after the tab has gone as it supports it
    // and deletion causes events that may expect it to be there.
    m_allTabs.erase(m_allTabs.begin() + closingTabIndex);
    // SCI_SETDOCPOINTER is necessary so the reference count of the document
    // is decreased and the memory can be released.
    m_editor.Scintilla().SetDocPointer(nullptr);
    m_docManager.RemoveDocument(closingTabId);

    int tabCount     = GetItemCount();
    int nextTabIndex = (closingTabIndex < tabCount) ? closingTabIndex : tabCount - 1;
    if (closingTabId != currentTabId)
    {
        auto nxtIndex = GetIndexFromID(currentTabId);
        if (nxtIndex >= 0)
            nextTabIndex = nxtIndex;
    }
    else if (tabCount == 0 && !quitting)
    {
        EnsureAtLeastOneTab();
        return true;
    }

    SetSelected(nextTabIndex);

    return true;
}

bool CMainWindow::CloseAllTabs(bool quitting)
{
    BlockAllUIUpdates(true);
    OnOutOfScope(BlockAllUIUpdates(false));

    ShowProgressCtrl(static_cast<UINT>(GetInt64(DEFAULTS_SECTION, L"ProgressDelay", 1000)));
    OnOutOfScope(HideProgressCtrl());

    closeAllDoAll = FALSE;
    doCloseAll    = true;
    OnOutOfScope(closeAllDoAll = FALSE; doCloseAll    = false;);
    auto total = GetItemCount();
    int  count = 0;
    for (;;)
    {
        SetProgress(count++, total);
        if (GetItemCount() == 0)
            break;
        if (!CloseTab(GetCurrentTabIndex(), false, quitting))
            return false;
        if (!quitting && GetItemCount() == 1 &&
            m_editor.Scintilla().TextLength() == 0 &&
            m_editor.Scintilla().Modify() == 0 &&
            m_docManager.GetDocumentFromID(GetIDFromIndex(0)).m_path.empty())
            return false;
    }
    if (quitting)
    {
        m_fileTree.BlockRefresh(true); // when we're quitting, don't let the file tree do a refresh
        return true;
    }

    return GetItemCount() == 0;
}

void CMainWindow::SetFileToOpen(const std::wstring& path, size_t line)
{
    auto list = GetFileListFromGlobPath(path);
    for (const auto p : list)
    {
        m_pathsToOpen[p] = line;
    }
}

void CMainWindow::CloseAllButThis(int idx)
{
    BlockAllUIUpdates(true);
    OnOutOfScope(BlockAllUIUpdates(false));
    int count   = GetItemCount();
    int current = idx == -1 ? GetCurrentTabIndex() : idx;

    ShowProgressCtrl(static_cast<UINT>(GetInt64(DEFAULTS_SECTION, L"ProgressDelay", 1000)));
    OnOutOfScope(HideProgressCtrl());

    closeAllDoAll = FALSE;
    doCloseAll    = true;
    OnOutOfScope(closeAllDoAll = FALSE; doCloseAll    = false;);
    for (int i = count - 1; i >= 0; --i)
    {
        if (i != current)
        {
            CloseTab(i);
            SetProgress(count - i, count - 1);
        }
    }
}
std::wstring CMainWindow::GetTabTitle(int index) const
{
    return m_allTabs[index].name;
}

void CMainWindow::SetTabTitle(int index, std::wstring title)
{
    m_allTabs[index].name = title;
}

void CMainWindow::UpdateCaptionBar()
{
    auto         docID   = GetCurrentTabId();
    std::wstring appName = GetAppName();
    std::wstring elev;
    ResString    rElev(g_hRes, IDS_ELEVATED);
    if (SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated())
        elev = static_cast<LPCWSTR>(rElev);

    if (m_docManager.HasDocumentID(docID))
    {
        const auto& doc = m_docManager.GetDocumentFromID(docID);
        int          idx    = GetIndexFromID(docID);
        std::wstring sTitle = elev;
        if (!elev.empty())
            sTitle += L" : ";
        sTitle += doc.m_path.empty() ? m_allTabs[idx].name : doc.m_path;
        m_titleText = CPathUtils::GetParentDirectory(sTitle); 
        if (doc.m_path.empty()) 
        {
            if (m_allTabs[m_selected].state == UNSAVED_DOC)
                m_titleText = L"~ " + m_allTabs[m_selected].name;
            else
                m_titleText = m_allTabs[m_selected].name;
        }
    }
    else
    {
        if (!elev.empty())
            appName += L" : " + elev;
        m_titleText = appName;
    }

    SendMessage(*this, WM_SETTEXT, 0, 0);
}

void CMainWindow::UpdateTab(DocID docID)
{
    const auto& doc = m_docManager.GetDocumentFromID(docID);
    int         index = GetIndexFromID(docID);

    if (doc.m_bIsReadonly || doc.m_bIsWriteProtected || (m_editor.Scintilla().ReadOnly() != 0))
        m_allTabs[index].state = READONLY_DOC;
    else
        m_allTabs[index].state = doc.m_bIsDirty || doc.m_bNeedsSaving ? UNSAVED_DOC : SAVED_DOC;
    
    UpdateCaptionBar();// TODO : update tab item
    InvalidateRect(*this, &m_allRects.tabs, FALSE);
}

ResponseToCloseTab CMainWindow::AskToCloseTab() const
{
    ResString    rTitle(g_hRes, IDS_HASMODIFICATIONS);
    ResString    rQuestion(g_hRes, IDS_DOYOUWANTOSAVE);
    ResString    rSave(g_hRes, IDS_SAVE);
    ResString    rDontSave(g_hRes, IDS_DONTSAVE);
    std::wstring sQuestion = CStringUtils::Format(rQuestion, GetTabTitle(GetSelected()).c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    aCustomButtons[0].nButtonID         = 100;
    aCustomButtons[0].pszButtonText     = rSave;
    aCustomButtons[1].nButtonID         = 101;
    aCustomButtons[1].pszButtonText     = rDontSave;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100;
    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwCommonButtons    = TDCBF_CANCEL_BUTTON;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    if (doCloseAll)
        tdc.pszVerificationText = MAKEINTRESOURCE(IDS_DOITFORALLFILES);
    int     nClickedBtn = 0;
    HRESULT hr          = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &closeAllDoAll);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    ResponseToCloseTab response = ResponseToCloseTab::StayOpen;
    switch (nClickedBtn)
    {
        case 100:
            response = ResponseToCloseTab::SaveAndClose;
            break;
        case 101:
            response = ResponseToCloseTab::CloseWithoutSaving;
            break;
        default:
            break;
    }
    return response;
}

ResponseToOutsideModifiedFile CMainWindow::AskToReloadOutsideModifiedFile(const CDocument& doc) const
{
    if (!responseToOutsideModifiedFileDoAll || !doModifiedAll)
    {
        bool changed = doc.m_bNeedsSaving || doc.m_bIsDirty;
        if (!changed && GetInt64(DEFAULTS_SECTION, L"AutoReload"))
            return ResponseToOutsideModifiedFile::Reload;
        ResString rTitle(g_hRes, IDS_OUTSIDEMODIFICATIONS);
        ResString rQuestion(g_hRes, changed ? IDS_DOYOUWANTRELOADBUTDIRTY : IDS_DOYOUWANTTORELOAD);
        ResString rSave(g_hRes, IDS_SAVELOSTOUTSIDEMODS);
        ResString rReload(g_hRes, changed ? IDS_RELOADLOSTMODS : IDS_RELOAD);
        ResString rCancel(g_hRes, IDS_NORELOAD);
        // Be specific about what they are re-loading.
        std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

        TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
        TASKDIALOG_BUTTON aCustomButtons[3] = {};
        int               bi                = 0;
        aCustomButtons[bi].nButtonID        = 101;
        aCustomButtons[bi++].pszButtonText  = rReload;
        if (changed)
        {
            aCustomButtons[bi].nButtonID       = 102;
            aCustomButtons[bi++].pszButtonText = rSave;
        }
        aCustomButtons[bi].nButtonID       = 100;
        aCustomButtons[bi++].pszButtonText = rCancel;
        tdc.pButtons                       = aCustomButtons;
        tdc.cButtons                       = bi;
        assert(tdc.cButtons <= _countof(aCustomButtons));
        tdc.nDefaultButton = 100; // default to "Cancel" in case the file is modified

        tdc.hwndParent          = *this;
        tdc.hInstance           = g_hRes;
        tdc.dwFlags             = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
        tdc.pszWindowTitle      = MAKEINTRESOURCE(IDS_APP_TITLE);
        tdc.pszMainIcon         = changed ? TD_WARNING_ICON : TD_INFORMATION_ICON;
        tdc.pszMainInstruction  = rTitle;
        tdc.pszContent          = sQuestion.c_str();
        tdc.pszVerificationText = MAKEINTRESOURCE(IDS_DOITFORALLFILES);
        int     nClickedBtn     = 0;
        HRESULT hr              = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &responseToOutsideModifiedFileDoAll);
        if (CAppUtils::FailedShowMessage(hr))
            nClickedBtn = 0;
        responseToOutsideModifiedFile = ResponseToOutsideModifiedFile::Cancel;
        switch (nClickedBtn)
        {
            case 101:
                responseToOutsideModifiedFile = ResponseToOutsideModifiedFile::Reload;
                break;
            case 102:
                responseToOutsideModifiedFile = ResponseToOutsideModifiedFile::KeepOurChanges;
                break;
            default:
                break;
        }
    }
    return responseToOutsideModifiedFile;
}

bool CMainWindow::AskToReload(const CDocument& doc) const
{
    ResString rTitle(g_hRes, IDS_HASMODIFICATIONS);
    ResString rQuestion(g_hRes, IDS_RELOADREALLY);
    ResString rReload(g_hRes, IDS_DORELOAD);
    ResString rCancel(g_hRes, IDS_DONTRELOAD);
    // Be specific about what they are re-loading.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    aCustomButtons[0].nButtonID         = 101;
    aCustomButtons[0].pszButtonText     = rReload;
    aCustomButtons[1].nButtonID         = 100;
    aCustomButtons[1].pszButtonText     = rCancel;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100; // Default to cancel

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_WARNING_ICON; // Warn because we are going to throw away the document.
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool reload = (nClickedBtn == 101);
    return reload;
}

bool CMainWindow::AskAboutOutsideDeletedFile(const CDocument& doc) const
{
    ResString rTitle(g_hRes, IDS_OUTSIDEREMOVEDHEAD);
    ResString rQuestion(g_hRes, IDS_OUTSIDEREMOVED);
    ResString rKeep(g_hRes, IDS_OUTSIDEREMOVEDKEEP);
    ResString rClose(g_hRes, IDS_OUTSIDEREMOVEDCLOSE);
    // Be specific about what they are removing.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, doc.m_path.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    int               bi                = 0;
    aCustomButtons[bi].nButtonID        = 100;
    aCustomButtons[bi++].pszButtonText  = rKeep;
    aCustomButtons[bi].nButtonID        = 101;
    aCustomButtons[bi].pszButtonText    = rClose;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    tdc.nDefaultButton                  = 100;

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bKeep = true;
    if (nClickedBtn == 101)
        bKeep = false;
    return bKeep;
}

bool CMainWindow::AskToRemoveReadOnlyAttribute() const
{
    ResString rTitle(g_hRes, IDS_FILEISREADONLY);
    ResString rQuestion(g_hRes, IDS_FILEMAKEWRITABLEASK);
    auto      rEditFile = LoadResourceWString(g_hRes, IDS_EDITFILE);
    auto      rCancel   = LoadResourceWString(g_hRes, IDS_CANCEL);
    // We remove the short cut accelerators from these buttons because this
    // dialog pops up automatically and it's too easy to be typing into the editor
    // when that happens and accidentally acknowledge a button.
    SearchRemoveAll(rEditFile, L"&");
    SearchRemoveAll(rCancel, L"&");

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    aCustomButtons[0].nButtonID         = 101;
    aCustomButtons[0].pszButtonText     = rEditFile.c_str();
    aCustomButtons[1].nButtonID         = 100;
    aCustomButtons[1].pszButtonText     = rCancel.c_str();
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = _countof(aCustomButtons);
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 100; // Default to cancel

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_WARNING_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = rQuestion;
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bRemoveReadOnlyAttribute = (nClickedBtn == 101);
    return bRemoveReadOnlyAttribute;
}

// Returns true if file exists or was created.
bool CMainWindow::AskToCreateNonExistingFile(const std::wstring& path) const
{
    ResString rTitle(g_hRes, IDS_FILE_DOESNT_EXIST);
    ResString rQuestion(g_hRes, IDS_FILE_ASK_TO_CREATE);
    ResString rCreate(g_hRes, IDS_FILE_CREATE);
    ResString rCancel(g_hRes, IDS_FILE_CREATE_CANCEL);
    // Show exactly what we are creating.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, path.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    int               bi                = 0;
    aCustomButtons[bi].nButtonID        = 101;
    aCustomButtons[bi++].pszButtonText  = rCreate;
    aCustomButtons[bi].nButtonID        = 100;
    aCustomButtons[bi++].pszButtonText  = rCancel;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = bi;
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 101;

    tdc.hwndParent         = *this;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bCreate = (nClickedBtn == 101);
    return bCreate;
}

void CMainWindow::HandleClipboardUpdate()
{
    CCommandHandler::Instance().OnClipboardChanged();
    std::wstring s;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        CClipboardHelper clipboard;
        if (clipboard.Open(*this))
        {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData)
            {
                LPCWSTR lptStr = static_cast<LPCWSTR>(GlobalLock(hData));
                OnOutOfScope(
                    GlobalUnlock(hData););
                if (lptStr != nullptr)
                    s = lptStr;
            }
        }
    }
    if (!s.empty())
    {
        std::wstring sTrimmed = s;
        CStringUtils::trim(sTrimmed);
        if (!sTrimmed.empty())
        {
            for (auto it = m_clipboardHistory.cbegin(); it != m_clipboardHistory.cend(); ++it)
            {
                if (it->compare(s) == 0)
                {
                    m_clipboardHistory.erase(it);
                    break;
                }
            }
            m_clipboardHistory.push_front(std::move(s));
        }
    }

    size_t maxsize = static_cast<size_t>(GetInt64(DEFAULTS_SECTION, L"MaxClipboardHistorySize", 20));
    if (m_clipboardHistory.size() > maxsize)
        m_clipboardHistory.pop_back();
}

void CMainWindow::PasteHistory()
{
    if (!m_clipboardHistory.empty())
    {
        // create a context menu with all the items in the clipboard history
        HMENU hMenu = CreatePopupMenu();
        if (hMenu)
        {
            OnOutOfScope(
                DestroyMenu(hMenu););
            size_t pos = m_editor.Scintilla().CurrentPos();
            POINT  pt{};
            pt.x = static_cast<LONG>(m_editor.Scintilla().PointXFromPosition(pos));
            pt.y = static_cast<LONG>(m_editor.Scintilla().PointYFromPosition(pos));
            ClientToScreen(m_editor, &pt);
            int    index   = 1;
            size_t maxsize = static_cast<size_t>(GetInt64(DEFAULTS_SECTION, L"MaxClipboardItemLength", 40));
            for (const auto& s : m_clipboardHistory)
            {
                std::wstring sf = s;
                SearchReplace(sf, L"\t", L" ");
                SearchReplace(sf, L"\n", L" ");
                SearchReplace(sf, L"\r", L" ");
                CStringUtils::trim(sf);
                // remove unnecessary whitespace inside the string
                std::wstring::iterator newEnd = std::unique(sf.begin(), sf.end(), [](wchar_t lhs, wchar_t rhs) -> bool {
                    return (lhs == ' ' && rhs == ' ');
                });
                sf.erase(newEnd, sf.end());

                AppendMenu(hMenu, MF_ENABLED, index, sf.substr(0, maxsize).c_str());
                ++index;
            }
            int selIndex = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, m_editor, nullptr);
            if (selIndex > 0)
            {
                index = 1;
                for (const auto& s : m_clipboardHistory)
                {
                    if (index == selIndex)
                    {
                        WriteAsciiStringToClipboard(s.c_str(), *this);
                        m_editor.Scintilla().Paste();
                        break;
                    }
                    ++index;
                }
            }
        }
    }
}

// Show Tool Tips and colors for colors and numbers and their
// conversions to hex octal etc.
// e.g. 0xF0F hex == 3855 decimal == 07417 octal.
void CMainWindow::HandleDwellStart(const SCNotification& scn, bool start)
{
    UNREFERENCED_PARAMETER(start);
    // Note style will be zero if no style or past end of the document.
    if ((scn.position >= 0) && (m_editor.Scintilla().IndicatorValueAt(INDIC_URLHOTSPOT, scn.position)))
    {
        if (GetInt64(DEFAULTS_SECTION, L"URLTooltip") == 0)
            return;
        static int lastDwellPosX = -1;
        static int lastDwellPosY = -1;
        const int  pixelMargin   = CDPIAware::Instance().Scale(*this, 4);
        // an url hotspot
        // find start of url
        auto startPos     = scn.position;
        auto endPos       = scn.position;
        auto lineStartPos = m_editor.Scintilla().PositionFromPoint(0, scn.y);
        if (!m_editor.Scintilla().IndicatorValueAt(INDIC_URLHOTSPOT, startPos))
        {
            startPos     = m_editor.Scintilla().PositionFromPoint(scn.x, scn.y + pixelMargin);
            endPos       = startPos;
            lineStartPos = m_editor.Scintilla().PositionFromPoint(0, scn.y + pixelMargin);
            if (!m_editor.Scintilla().IndicatorValueAt(INDIC_URLHOTSPOT, startPos))
            {
                m_editor.Scintilla().CallTipCancel();
                lastDwellPosX = -1;
                lastDwellPosY = -1;
                return;
            }
        }
        while (startPos && m_editor.Scintilla().IndicatorValueAt(INDIC_URLHOTSPOT, startPos))
            --startPos;
        ++startPos;
        // find end of url
        auto docEnd = m_editor.Scintilla().Length();
        while (endPos < docEnd && m_editor.Scintilla().IndicatorValueAt(INDIC_URLHOTSPOT, endPos))
            ++endPos;
        --endPos;

        ResString          str(g_hRes, IDS_HOWTOOPENURL);
        std::string        strA = CUnicodeUtils::StdGetUTF8(str);
        std::istringstream is(strA);
        std::string        part;
        size_t             lineLength = 0;
        while (getline(is, part, '\n'))
            lineLength = max(lineLength, part.size());
        auto cursorPos = m_editor.Scintilla().PositionFromPoint(scn.x, scn.y);
        auto tipPos    = cursorPos - static_cast<sptr_t>(lineLength) / 2;
        tipPos         = max(lineStartPos, tipPos);
        if (m_editor.Scintilla().CallTipActive() && m_editor.Scintilla().CallTipPosStart() == tipPos)
            return;
        lastDwellPosX = scn.x;
        lastDwellPosY = scn.y;
        m_editor.Scintilla().CallTipShow(tipPos, strA.c_str());
        return;
    }

    if (GetInt64(DEFAULTS_SECTION, L"NumTooltip") == 0)
        return;

    auto getPos = [&]() -> POINT {
        auto  msgPos     = GetMessagePos();
        POINT pt         = {GET_X_LPARAM(msgPos), GET_Y_LPARAM(msgPos)};
        auto  lineHeight = m_editor.Scintilla().TextHeight(0);
        pt.y -= lineHeight;
        return pt;
    };
    // try the users real selection first
    std::string sWord    = m_editor.GetSelectedText(SelectionHandling::None);
    auto        selStart = m_editor.Scintilla().SelectionStart();
    auto        selEnd   = m_editor.Scintilla().SelectionEnd();

    if (sWord.empty() ||
        (scn.position > selEnd) || (scn.position < selStart))
    {
        auto wordCharsBuffer = m_editor.GetWordChars();
        OnOutOfScope(m_editor.Scintilla().SetWordChars(wordCharsBuffer.c_str()));

        m_editor.Scintilla().SetWordChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,#");
        selStart = m_editor.Scintilla().WordStartPosition(scn.position, false);
        selEnd   = m_editor.Scintilla().WordEndPosition(scn.position, false);
        sWord    = m_editor.GetTextRange(static_cast<long>(selStart), static_cast<long>(selEnd));
    }
    if (sWord.empty())
        return;

    // Short form or long form html color e.g. #F0F or #FF00FF
    // Make sure the string is hexadecimal
    if (sWord[0] == '#' && (sWord.size() == 4 || sWord.size() == 7 || sWord.size() == 9) && IsHexDigitString(sWord.c_str() + 1))
    {
        bool     ok    = false;
        COLORREF color = 0;

        // Note: could use std::stoi family of functions but they throw
        // range exceptions etc. and VC reports those in the debugger output
        // window. That's distracting and gives the impression something
        // is drastically wrong when it isn't, so we don't use those.

        std::string strNum = sWord.substr(1); // Drop #
        if (strNum.size() == 3)               // short form .e.g. F0F
        {
            ok = GDIHelpers::ShortHexStringToCOLORREF(strNum, &color);
        }
        else if (strNum.size() == 6) // normal/long form, e.g. FF00FF
        {
            ok = GDIHelpers::HexStringToCOLORREF(strNum, &color);
        }
        else if (strNum.size() == 8) // normal/long form with alpha, e.g. 00FF00FF
        {
            ok = GDIHelpers::HexStringToCOLORREF(strNum, &color);
        }
        if (ok)
        {
            // Don't display hex with # as that means web color hex triplet
            // Show as hex with 0x and show what it was parsed to.
            auto copyTip = CStringUtils::Format(
                L"RGB(%d, %d, %d)\nHex: 0x%06lX",
                GetRValue(color), GetGValue(color), GetBValue(color), static_cast<DWORD>(color));
            auto  sCallTip = copyTip;
            POINT pt       = getPos();
            m_custToolTip.ShowTip(pt, sCallTip, &color, copyTip);
            return;
        }
    }

    // See if the data looks like a pattern matching RGB(r,g,b) where each element
    // can be decimal, hex with leading 0x, or octal with leading 0, like C/C++.
    auto          wWord  = CUnicodeUtils::StdGetUnicode(sWord);
    const wchar_t rgb[]  = {L"RGB"};
    const size_t  rgbLen = wcslen(rgb);
    if (_wcsnicmp(wWord.c_str(), rgb, rgbLen) == 0)
    {
        // get the word up to the closing bracket
        int maxlength = 20;
        while ((static_cast<char>(m_editor.Scintilla().CharAt(++selEnd)) != ')') && --maxlength)
        {
        }
        if (maxlength == 0)
            return;
        sWord = m_editor.GetTextRange(static_cast<long>(selStart), static_cast<long>(selEnd));
        wWord = CUnicodeUtils::StdGetUnicode(sWord);

        // Grab the data the between brackets that follows the word RGB,
        // if there looks to be 3 elements to it, try to parse each r,g,b element
        // as a number in decimal, hex or octal.
        wWord = wWord.substr(rgbLen, (wWord.length() - rgbLen));
        CStringUtils::TrimLeadingAndTrailing(wWord, std::wstring(L"()"));
        int                       r = 0, g = 0, b = 0;
        std::vector<std::wstring> vec;
        stringtok(vec, wWord, true, L",");
        if (vec.size() == 3 &&
            CAppUtils::TryParse(vec[0].c_str(), r, false, 0, 0) &&
            CAppUtils::TryParse(vec[1].c_str(), g, false, 0, 0) &&
            CAppUtils::TryParse(vec[2].c_str(), b, false, 0, 0))
        {
            // Display the item back as RGB(r,g,b) where each is decimal
            // (since they can already see any other element type that might have used,
            // so all decimal might mean more info)
            // and as a hex color triplet which is useful using # to indicate that.
            auto color   = RGB(r, g, b);
            auto copyTip = CStringUtils::Format(
                L"RGB(%d, %d, %d)\n#%02X%02X%02X\nHex: 0x%06lX",
                r, g, b, GetRValue(color), GetGValue(color), GetBValue(color), color);
            auto sCallTip = copyTip;

            POINT pt = getPos();
            m_custToolTip.ShowTip(pt, sCallTip, &color, copyTip);
            return;
        }
    }

    // Assume a number format determined by the string itself
    // and as recognized as a valid format according to strtoll:
    // e.g. 0xF0F hex == 3855 decimal == 07417 octal.
    // Use long long to yield more conversion range than say just long.
    char* ep = nullptr;
    // 0 base means determine base from any format in the string.
    errno            = 0;
    int  radix       = 0;
    auto trimmedWord = CStringUtils::trim(CStringUtils::trim(CStringUtils::trim(sWord, L'('), L','), L')');
    if (trimmedWord.starts_with("0b"))
    {
        trimmedWord = trimmedWord.substr(2);
        radix       = 2;
    }
    long long number = strtoll(trimmedWord.c_str(), &ep, radix);
    // Be forgiving if given 100xyz, show 100, but
    // don't accept xyz100, show nothing.
    // Must convert some digits of string.
    if (errno == 0 && (*ep == 0))
    {
        auto bs       = to_bit_wstring(number, true);
        auto copyTip  = CStringUtils::Format(L"Dec: %lld\nHex: 0x%llX\nOct: %#llo\nBin: %s (%d digits)",
                                             number, number, number, bs.c_str(), static_cast<int>(bs.size()));
        auto sCallTip = copyTip;

        COLORREF  color  = 0;
        COLORREF* pColor = nullptr;
        if (sWord.size() > 7)
        {
            // may be a color: 0xFF205090 or just 0x205090
            if (sWord[0] == '0' && (sWord[1] == 'x' || sWord[1] == 'X'))
            {
                BYTE r = (number >> 16) & 0xFF;
                BYTE g = (number >> 8) & 0xFF;
                BYTE b = number & 0xFF;
                color  = RGB(r, g, b) | (number & 0xFF000000);
                pColor = &color;
            }
        }
        POINT pt = getPos();
        m_custToolTip.ShowTip(pt, sCallTip, pColor, copyTip);
        return;
    }
    int  err       = 0;
    auto exprValue = te_interp(sWord.c_str(), &err);
    if (err)
    {
        auto wordCharsBuffer = m_editor.GetWordChars();
        OnOutOfScope(m_editor.Scintilla().SetWordChars(wordCharsBuffer.c_str()));

        m_editor.Scintilla().SetWordChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-+*/!%~()^_.,");
        selStart = m_editor.Scintilla().WordStartPosition(scn.position, false);
        selEnd   = m_editor.Scintilla().WordEndPosition(scn.position, false);
        sWord    = m_editor.GetTextRange(static_cast<long>(selStart), static_cast<long>(selEnd));
    }
    exprValue = te_interp(sWord.c_str(), &err);
    if (err == 0)
    {
        long long ulongVal = static_cast<long long>(exprValue);
        auto      sCallTip = CStringUtils::Format(L"Expr: %S\n-->\nVal: %f\nDec: %lld\nHex: 0x%llX\nOct: %#llo",
                                                  sWord.c_str(), exprValue, ulongVal, ulongVal, ulongVal);
        auto      copyTip  = CStringUtils::Format(L"Val: %f\nDec: %lld\nHex: 0x%llX\nOct: %#llo",
                                                  exprValue, ulongVal, ulongVal, ulongVal);
        POINT     pt       = getPos();
        m_custToolTip.ShowTip(pt, sCallTip, nullptr, copyTip);
    }
}

void CMainWindow::HandleGetDispInfo(int tab, LPNMTTDISPINFO lpNmtdi)
{
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(*this, &p);
    HWND hWin = RealChildWindowFromPoint(*this, p);
    if (hWin == *this)
    {
        auto docId = GetIDFromIndex(tab);
        if (docId.IsValid())
        {
            if (m_docManager.HasDocumentID(docId))
            {
                const auto& doc = m_docManager.GetDocumentFromID(docId);
                m_tooltipBuffer = std::make_unique<wchar_t[]>(doc.m_path.size() + 1);
                wcscpy_s(m_tooltipBuffer.get(), doc.m_path.size() + 1, doc.m_path.c_str());
                lpNmtdi->lpszText = m_tooltipBuffer.get();
                lpNmtdi->hinst    = nullptr;
            }
        }
    }
}

LPARAM CMainWindow::HandleMouseMsg(const SCNotification& scn)
{
    switch (scn.modificationType)
    {
        case WM_LBUTTONDOWN:
        {
            if (scn.modifiers & SCMOD_CTRL)
            {
                if (m_editor.Scintilla().IndicatorValueAt(INDIC_URLHOTSPOT, scn.position))
                {
                    OpenUrlAtPos(scn.position);
                    return FALSE;
                }
            }
            SendMessage(*this, WM_TIMER, TIMER_DWELLEND, 0);
        }
        break;
        case WM_MOUSEWHEEL:
        {
            if (scn.modifiers == SCMOD_SHIFT)
            {
                SendMessage(m_editor, WM_HSCROLL, GET_WHEEL_DELTA_WPARAM(scn.wParam) < 0 ? SB_LINERIGHT : SB_LINELEFT, 0);
                SendMessage(m_editor, WM_HSCROLL, GET_WHEEL_DELTA_WPARAM(scn.wParam) < 0 ? SB_LINERIGHT : SB_LINELEFT, 0);
                SendMessage(m_editor, WM_HSCROLL, GET_WHEEL_DELTA_WPARAM(scn.wParam) < 0 ? SB_LINERIGHT : SB_LINELEFT, 0);
                return TRUE;
            }
        }
        break;
        default:
            break;
    }

    return TRUE;
}

bool CMainWindow::OpenUrlAtPos(Sci_Position pos)
{
    auto wordCharsBuffer = m_editor.GetWordChars();
    OnOutOfScope(m_editor.Scintilla().SetWordChars(wordCharsBuffer.c_str()));

    m_editor.Scintilla().SetWordChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-+.,:;?&@=/%#()");

    Sci_Position startPos = static_cast<Sci_Position>(m_editor.Scintilla().WordStartPosition(pos, false));
    Sci_Position endPos   = static_cast<Sci_Position>(m_editor.Scintilla().WordEndPosition(pos, false));

    m_editor.Scintilla().SetTargetStart(startPos);
    m_editor.Scintilla().SetTargetEnd(endPos);
    auto originalSearchFlags = m_editor.Scintilla().SearchFlags();
    OnOutOfScope(m_editor.Scintilla().SetSearchFlags(originalSearchFlags));

    m_editor.Scintilla().SetSearchFlags(Scintilla::FindOption::RegExp | Scintilla::FindOption::Cxx11RegEx);

    Sci_Position posFound = static_cast<Sci_Position>(m_editor.Scintilla().SearchInTarget(URL_REG_EXPR_LENGTH, URL_REG_EXPR));
    if (posFound != -1)
    {
        startPos = static_cast<Sci_Position>(m_editor.Scintilla().TargetStart());
        endPos   = static_cast<Sci_Position>(m_editor.Scintilla().TargetEnd());
    }
    else
        return false;
    std::string urlText = m_editor.GetTextRange(startPos, endPos);
    // This treatment would fail on some valid URLs where there's actually supposed to be a comma or parenthesis at the end.
    CStringUtils::TrimLeadingAndTrailing(urlText, std::vector<char>{',', ')', '('});
    std::wstring url = CUnicodeUtils::StdGetUnicode(urlText);
    SearchReplace(url, L"&amp;", L"&");

    ::ShellExecute(*this, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);

    return true;
}

void CMainWindow::HandleSavePoint(const SCNotification& scn)
{
    auto docID = GetCurrentTabId();
    if (m_docManager.HasDocumentID(docID))
    {
        auto& doc      = m_docManager.GetModDocumentFromID(docID);
        doc.m_bIsDirty = scn.nmhdr.code == SCN_SAVEPOINTLEFT;
        UpdateTab(docID);
    }
}

void CMainWindow::HandleWriteProtectedEdit()
{
    // user tried to edit a readonly file: ask whether
    // to make the file writable
    auto docID = GetCurrentTabId();
    if (m_docManager.HasDocumentID(docID))
    {
        auto& doc = m_docManager.GetModDocumentFromID(docID);
        if (!doc.m_bIsWriteProtected && (doc.m_bIsReadonly || (m_editor.Scintilla().ReadOnly() != 0)))
        {
            // If the user really wants to edit despite it being read only, let them.
            if (AskToRemoveReadOnlyAttribute())
            {
                doc.m_bIsReadonly = false;
                UpdateTab(docID);
                m_editor.Scintilla().SetReadOnly(false);
                m_editor.Scintilla().SetSavePoint();
                m_editor.EnableChangeHistory();
            }
        }
    }
}

void CMainWindow::AddHotSpots() const
{
    auto firstVisibleLine = m_editor.Scintilla().FirstVisibleLine();
    auto startPos         = m_editor.Scintilla().PositionFromLine(m_editor.Scintilla().DocLineFromVisible(firstVisibleLine));
    auto linesOnScreen    = m_editor.Scintilla().LinesOnScreen();
    auto lineCount        = m_editor.Scintilla().LineCount();
    auto endPos           = m_editor.Scintilla().PositionFromLine(m_editor.Scintilla().DocLineFromVisible(firstVisibleLine + min(linesOnScreen, lineCount)));

    // to speed up the search, first search for "://" without using the regex engine
    auto fStartPos = startPos;
    auto fEndPos   = endPos;
    m_editor.Scintilla().SetSearchFlags(Scintilla::FindOption::None);
    m_editor.Scintilla().SetTargetStart(fStartPos);
    m_editor.Scintilla().SetTargetEnd(fEndPos);
    LRESULT posFoundColonSlash = m_editor.Scintilla().SearchInTarget(3, "://");
    while (posFoundColonSlash != -1)
    {
        // found a "://"
        auto lineFoundColonSlash = m_editor.Scintilla().LineFromPosition(posFoundColonSlash);
        startPos                 = m_editor.Scintilla().PositionFromLine(lineFoundColonSlash);
        endPos                   = m_editor.Scintilla().LineEndPosition(lineFoundColonSlash);
        fStartPos                = posFoundColonSlash + 1LL;

        m_editor.Scintilla().SetSearchFlags(Scintilla::FindOption::RegExp | Scintilla::FindOption::Cxx11RegEx);

        // 20 chars for the url protocol should be enough
        m_editor.Scintilla().SetTargetStart(max(startPos, posFoundColonSlash - 20));
        // urls longer than 2048 are not handled by browsers
        m_editor.Scintilla().SetTargetEnd(min(endPos, posFoundColonSlash + 2048));

        LRESULT posFound = m_editor.Scintilla().SearchInTarget(URL_REG_EXPR_LENGTH, URL_REG_EXPR);

        if (posFound != -1)
        {
            auto start        = m_editor.Scintilla().TargetStart();
            auto end          = m_editor.Scintilla().TargetEnd();
            auto foundTextLen = end - start;

            // reset indicators
            m_editor.Scintilla().SetIndicatorCurrent(INDIC_URLHOTSPOT);
            m_editor.Scintilla().IndicatorClearRange(start, foundTextLen);
            m_editor.Scintilla().IndicatorClearRange(start, foundTextLen - 1LL);

            m_editor.Scintilla().IndicatorFillRange(start, foundTextLen);
        }
        m_editor.Scintilla().SetTargetStart(fStartPos);
        m_editor.Scintilla().SetTargetEnd(fEndPos);
        m_editor.Scintilla().SetSearchFlags(Scintilla::FindOption::None);
        posFoundColonSlash = static_cast<int>(m_editor.Scintilla().SearchInTarget(3, "://"));
    }
}

void CMainWindow::HandleUpdateUI(const SCNotification& scn)
{
    if (scn.updated & SC_UPDATE_V_SCROLL)
        m_editor.UpdateLineNumberWidth();
    if (scn.updated & SC_UPDATE_SELECTION)
        m_editor.SelectionUpdated();
    constexpr unsigned int uiFlags = SC_UPDATE_SELECTION |
                                     SC_UPDATE_H_SCROLL | SC_UPDATE_V_SCROLL;
    if ((scn.updated & uiFlags) != 0)
    {
        if ((scn.updated & SC_UPDATE_SELECTION) && (m_editor.Scintilla().Length() > 102400))
            SetTimer(*this, TIMER_SELCHANGE, 500, nullptr);
        else
            m_editor.MarkSelectedWord(false, false);
        SetTimer(*this, TIMER_CHECKLINES, 300, nullptr);
    }

    m_editor.MatchBraces(BraceMatch::Braces);
    m_editor.MatchTags();
    AddHotSpots();
    UpdateStatusBar(false);
}

void CMainWindow::IndentToLastLine() const
{
    auto curLine      = m_editor.GetCurrentLineNumber();
    auto lastLine     = curLine - 1;
    int  indentAmount = 0;
    // use the same indentation as the last line
    while (lastLine > 0 && (m_editor.Scintilla().LineEndPosition(lastLine) - m_editor.Scintilla().PositionFromLine(lastLine)) == 0)
        lastLine--;

    indentAmount = static_cast<int>(m_editor.Scintilla().LineIndentation(lastLine));

    if (indentAmount > 0)
    {
        Sci_CharacterRange cRange{};
        cRange.cpMin   = static_cast<Sci_PositionCR>(m_editor.Scintilla().SelectionStart());
        cRange.cpMax   = static_cast<Sci_PositionCR>(m_editor.Scintilla().SelectionEnd());
        auto posBefore = m_editor.Scintilla().LineIndentPosition(curLine);
        m_editor.Scintilla().SetLineIndentation(curLine, indentAmount);
        auto posAfter      = m_editor.Scintilla().LineIndentPosition(curLine);
        auto posDifference = posAfter - posBefore;
        if (posAfter > posBefore)
        {
            // Move selection on
            if (cRange.cpMin >= posBefore)
                cRange.cpMin += static_cast<Sci_PositionCR>(posDifference);
            if (cRange.cpMax >= posBefore)
                cRange.cpMax += static_cast<Sci_PositionCR>(posDifference);
        }
        else if (posAfter < posBefore)
        {
            // Move selection back
            if (cRange.cpMin >= posAfter)
            {
                if (cRange.cpMin >= posBefore)
                    cRange.cpMin += static_cast<Sci_PositionCR>(posDifference);
                else
                    cRange.cpMin = static_cast<Sci_PositionCR>(posAfter);
            }
            if (cRange.cpMax >= posAfter)
            {
                if (cRange.cpMax >= posBefore)
                    cRange.cpMax += static_cast<Sci_PositionCR>(posDifference);
                else
                    cRange.cpMax = static_cast<Sci_PositionCR>(posAfter);
            }
        }
        m_editor.Scintilla().SetSel(cRange.cpMin, cRange.cpMax);
    }
}

void CMainWindow::HandleAutoIndent(const SCNotification& scn) const
{
    if (m_bBlockAutoIndent)
        return;
    int eolMode = static_cast<int>(m_editor.Scintilla().EOLMode());

    if (((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && scn.ch == '\n') ||
        (eolMode == SC_EOL_CR && scn.ch == '\r'))
    {
        IndentToLastLine();
    }
}

void CMainWindow::OpenNewTab()
{
    OnOutOfScope(
        m_insertionIndex = -1;);

    BOOL      preferUTF8 = GetInt64(DEFAULTS_SECTION, L"PreferUTF8") != 0;
    CDocument doc;
    doc.m_document = m_editor.Scintilla().CreateDocument(0, Scintilla::DocumentOption::Default);
    doc.m_bHasBOM  = FALSE;
    doc.m_encoding = preferUTF8 ? CP_UTF8 : GetACP();
    doc.m_format   = EOLFormat::Win_Format;
    auto eolMode   = toEolMode(doc.m_format);
    m_editor.SetEOLType(eolMode);

    doc.SetLanguage("Text");
    std::wstring tabName = GetNewTabName();
    int          index   = -1;
    index = InsertAtEnd(tabName.c_str());
    auto docID = GetIDFromIndex(index) ;
    m_docManager.AddDocumentAtEnd(doc, docID);
    CCommandHandler::Instance().OnDocumentOpen(docID);
   
    m_editor.GotoLine(0);
    m_allTabs[index].id = docID;
    m_editor.EnableChangeHistory();
    HandleTabChange();
}

void CMainWindow::HandleTabChanging()
{
    // document is about to be deactivated
    auto docID = GetCurrentTabId();
    if (m_docManager.HasDocumentID(docID))
    {
        m_editor.MatchBraces(BraceMatch::Clear);
        auto& doc = m_docManager.GetModDocumentFromID(docID);
        m_editor.SaveCurrentPos(doc.m_position);
    }
}

void CMainWindow::HandleTabChange()
{
    auto docID = GetCurrentTabId();
    // Can't do much if no document for this tab.
    if (!m_docManager.HasDocumentID(docID))
        return;
    
    TBHDR tbHdr        = {};
    tbHdr.hdr.hwndFrom = *this;
    tbHdr.hdr.code     = TCN_SELCHANGE;
    tbHdr.hdr.idFrom   = m_selected;
    tbHdr.tabOrigin    = m_selected;
    CCommandHandler::Instance().TabNotify(&tbHdr);

    auto& doc = m_docManager.GetModDocumentFromID(docID);
    m_editor.Scintilla().SetDocPointer(doc.m_document);
    m_editor.SetEOLType(toEolMode(doc.m_format));
    m_editor.SetupLexerForLang(doc.GetLanguage());
    m_editor.RestoreCurrentPos(doc.m_position);
    m_editor.SetTabSettings(doc.m_tabSpace);
    m_editor.SetReadDirection(doc.m_readDir);
    RefreshAnnotations();
    CCommandHandler::Instance().OnStylesSet();
    m_editor.MarkSelectedWord(true, false);
    m_editor.MarkBookmarksInScrollbar();
    m_fileTree.MarkActiveDocument(true);
    UpdateCaptionBar();
    SetFocus(m_editor);
    m_editor.Scintilla().GrabFocus();
    UpdateStatusBar(true);
    auto ds = m_docManager.HasFileChanged(docID);
    if (ds == DocModifiedState::Modified)
    {
        ReloadTab(m_selected, -1, true);
    }
    else if (ds == DocModifiedState::Removed)
    {
        HandleOutsideDeletedFile(m_selected);
    }
}

int CMainWindow::OpenFile(const std::wstring& file, unsigned int openFlags)
{
    int  index                 = -1;
    bool bAskToCreateIfMissing = (openFlags & OpenFlags::AskToCreateIfMissing) != 0;
    bool bCreateIfMissing      = (openFlags & OpenFlags::CreateIfMissing) != 0;
    bool bIgnoreIfMissing      = (openFlags & OpenFlags::IgnoreIfMissing) != 0;
    bool bOpenIntoActiveTab    = (openFlags & OpenFlags::OpenIntoActiveTab) != 0;
    // Ignore no activate flag for now. It causes too many issues.
    bool bActivate      = true; //(openFlags & OpenFlags::NoActivate) == 0;
    bool bCreateTabOnly = (openFlags & OpenFlags::CreateTabOnly) != 0;

    if (bCreateTabOnly)
    {
        auto fileName   = CPathUtils::GetFileName(file);
        BOOL      preferUTF8 = GetInt64(DEFAULTS_SECTION, L"PreferUTF8") != 0;
        CDocument doc;
        doc.m_document  = m_editor.Scintilla().CreateDocument(0, Scintilla::DocumentOption::Default);
        doc.m_bHasBOM   = FALSE;
        doc.m_encoding  = preferUTF8 ? CP_UTF8 : GetACP();
        
        doc.m_bNeedsSaving = true;
        doc.m_bDoSaveAs    = true;
        doc.m_path         = file;
        auto lang          = CLexStyles::Instance().GetLanguageForPath(fileName);
        if (lang.empty())
            lang = "Text";
        doc.SetLanguage(lang);
        index      = InsertAtEnd(CPathUtils::GetLongPathname(file).c_str());
        auto docID = GetIDFromIndex(index);
        m_docManager.AddDocumentAtEnd(doc, docID);
        m_allTabs[index].id = docID;
        UpdateTab(docID);
        UpdateStatusBar(true);
        SetSelected(index);
        
        CCommandHandler::Instance().OnDocumentOpen(docID);

        return index;
    }

    // if we're opening the first file, we have to activate it
    // to ensure all the activation stuff is handled for that first
    // file.

    if (GetItemCount() == 0)
        bActivate = true;

    int          encoding = -1;
    std::wstring filepath = file;
    if (_wcsicmp(CIniSettings::Instance().GetIniPath().c_str(), filepath.c_str()) == 0)
    {
        CIniSettings::Instance().Save();
    }
    auto id = m_docManager.GetIdForPath(filepath);
    if (id.IsValid())
    {
        index = GetIndexFromID(id) ;//m_tabBar.GetIndexFromID(id);
        // document already open.
        if (IsWindowEnabled(*this) && GetCurrentTabIndex() != index) // m_tabBar.GetCurrentTabIndex() != index)
        {
            // only activate the new doc tab if the main window is enabled:
            // if it's disabled, a modal dialog is shown
            // (e.g., the handle-outside-modifications confirmation dialog)
            // and we then must not change the active tab.
            SetSelected(index);
        }
    }
    else
    {
        bool createIfMissing = bCreateIfMissing;

        // Note PathFileExists returns true for existing folders,
        // not just files.
        if (!PathFileExists(file.c_str()))
        {
            if (bAskToCreateIfMissing)
            {
                if (!AskToCreateNonExistingFile(file))
                    return false;
                createIfMissing = true;
            }
            if (bIgnoreIfMissing)
                return false;
        }

        CDocument doc = m_docManager.LoadFile(/**this,*/ filepath, encoding, createIfMissing);
        if (doc.m_document)
        {
            DocID activeTabId;
            if (bOpenIntoActiveTab)
            {
                activeTabId           = GetCurrentTabId(); // m_tabBar.GetCurrentTabId();
                const auto& activeDoc = m_docManager.GetDocumentFromID(activeTabId);
                if (!activeDoc.m_bIsDirty && !activeDoc.m_bNeedsSaving)
                {
                    m_docManager.RemoveDocument(activeTabId);
                }
                else
                    activeTabId = DocID();
            }
            if (!activeTabId.IsValid())
            {
                // check if the only tab is empty and if it is, remove it
                auto docID = GetCurrentTabId(); // m_tabBar.GetCurrentTabId();
                if (docID.IsValid())
                {
                    const auto& existDoc = m_docManager.GetDocumentFromID(docID);
                    if (existDoc.m_path.empty() && (m_editor.Scintilla().Length() == 0) && (m_editor.Scintilla().CanUndo() == 0))
                    {
                        auto curTabIndex = GetCurrentTabIndex();
                        CCommandHandler::Instance().OnDocumentClose(docID);
                        m_insertionIndex = curTabIndex;
                        m_allTabs.erase(m_allTabs.begin() + m_insertionIndex);
                        if (m_insertionIndex)
                            --m_insertionIndex;
                        // Prefer to remove the document after the tab has gone as it supports it
                        // and deletion causes events that may expect it to be there.
                        m_docManager.RemoveDocument(docID);
                    }
                }
            }

            std::wstring sFileName = CPathUtils::GetFileName(filepath);
            if (!activeTabId.IsValid())
            {
                index = InsertAtEnd(filepath.c_str());
                id    = GetIDFromIndex(index) ;
            }
            else
            {
                index = GetCurrentTabIndex() ;//m_tabBar.GetCurrentTabIndex();
                m_allTabs[index].id = activeTabId;
                id = activeTabId;
                SetTabTitle(index,sFileName.c_str());
            }
            doc.SetLanguage(CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor));

            // check if the file is special, i.e. if we need to do something
            // when the file is saved
            auto sExt = CStringUtils::to_lower(CPathUtils::GetFileExtension(doc.m_path));
            if (sExt == L"ini")
            {
                if (doc.m_path == CIniSettings::Instance().GetIniPath())
                {
                    doc.m_saveCallback = []() { CIniSettings::Instance().Reload(); };
                }
            }
            else if (sExt == L"bplex")
            {
                doc.m_saveCallback = []() { CLexStyles::Instance().Reload(); };
            }

            m_docManager.AddDocumentAtEnd(doc, id);
            doc = m_docManager.GetDocumentFromID(id);

            // only activate the new doc tab if the main window is enabled:
            // if it's disabled, a modal dialog is shown
            // (e.g., the handle-outside-modifications confirmation dialog)
            // and we then must not change the active tab.
            if (IsWindowEnabled(*this))
            {
                bool bResize = m_fileTree.GetPath().empty() && !doc.m_path.empty();
                if (bActivate)
                    SetSelected(index);
                else
                    m_editor.Scintilla().SetDocPointer(doc.m_document);

                if (bResize)
                    ResizeChildWindows();
            }
            else
                m_editor.Scintilla().SetDocPointer(doc.m_document);

            if (m_fileTree.GetPath().empty())
            {
                m_fileTree.SetPath(CPathUtils::GetParentDirectory(filepath), false);
                ResizeChildWindows();
            }
            
            UpdateTab(id);
            CCommandHandler::Instance().OnDocumentOpen(id);
        }
        m_editor.EnableChangeHistory();
        AddToRecents(filepath);
    }
    m_insertionIndex = -1;
    
    return index;
}

// TODO: consider continuing merging process,
// how to merge with OpenFileAs with OpenFile
bool CMainWindow::OpenFileAs(const std::wstring& tempPath, const std::wstring& realpath, bool bModified)
{
    // If we can't open it, not much we can do.
    if (OpenFile(tempPath, 0) < 0)
    {
        DeleteFile(tempPath.c_str());
        return false;
    }

    // Get the id for the document we just loaded,
    // it'll currently have a temporary name.
    auto  docID        = m_docManager.GetIdForPath(tempPath);
    auto& doc          = m_docManager.GetModDocumentFromID(docID);
    doc.m_path         = CPathUtils::GetLongPathname(realpath);
    doc.m_bIsDirty     = bModified;
    doc.m_bNeedsSaving = bModified;
    m_docManager.UpdateFileTime(doc, true);
    std::wstring sFileName = CPathUtils::GetFileName(doc.m_path);
    const auto&  lang      = CLexStyles::Instance().GetLanguageForDocument(doc, m_scratchEditor);
    m_editor.Scintilla().SetReadOnly(doc.m_bIsReadonly);
    m_editor.SetupLexerForLang(lang);
    doc.SetLanguage(lang);
    UpdateTab(docID);
    RefreshAnnotations();
    SetTabTitle(GetIndexFromID(docID), sFileName.empty() ? GetNewTabName().c_str() : sFileName.c_str());
    UpdateCaptionBar();
    UpdateStatusBar(true);
    DeleteFile(tempPath.c_str());

    return true;
}

// Called when the user drops a selection of files (or a folder!) onto
// BowPad's main window. The response is to try to load all those files.
// Note: this function is also called from clipboard functions, so we must
// not call DragFinish() on the hDrop object here!
void CMainWindow::HandleDropFiles(HDROP hDrop)
{
    if (!hDrop)
        return;
    int                       filesDropped = DragQueryFile(hDrop, 0xffffffff, nullptr, 0);
    std::vector<std::wstring> files;
    for (int i = 0; i < filesDropped; ++i)
    {
        UINT len     = DragQueryFile(hDrop, i, nullptr, 0);
        auto pathBuf = std::make_unique<wchar_t[]>(len + 2LL);
        DragQueryFile(hDrop, i, pathBuf.get(), len + 1);
        files.push_back(pathBuf.get());
    }

    if (files.size() == 1)
    {
        if (CPathUtils::GetFileExtension(files[0]) == L"bplex")
        {
            // ask whether to install or open the file
            ResString    rTitle(g_hRes, IDS_IMPORTBPLEX_TITLE);
            ResString    rQuestion(g_hRes, IDS_IMPORTBPLEX_QUESTION);
            ResString    rImport(g_hRes, IDS_IMPORTBPLEX_IMPORT);
            ResString    rOpen(g_hRes, IDS_IMPORTBPLEX_OPEN);
            std::wstring sQuestion = CStringUtils::Format(rQuestion, CPathUtils::GetFileName(files[0]).c_str());

            TASKDIALOGCONFIG tdc                = {sizeof(TASKDIALOGCONFIG)};
            tdc.dwFlags                         = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
            TASKDIALOG_BUTTON aCustomButtons[2] = {};
            aCustomButtons[0].nButtonID         = 100;
            aCustomButtons[0].pszButtonText     = rImport;
            aCustomButtons[1].nButtonID         = 101;
            aCustomButtons[1].pszButtonText     = rOpen;
            tdc.pButtons                        = aCustomButtons;
            tdc.cButtons                        = _countof(aCustomButtons);
            assert(tdc.cButtons <= _countof(aCustomButtons));
            tdc.nDefaultButton = 100;

            tdc.hwndParent         = *this;
            tdc.hInstance          = g_hRes;
            tdc.dwCommonButtons    = TDCBF_CANCEL_BUTTON;
            tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
            tdc.pszMainIcon        = TD_INFORMATION_ICON;
            tdc.pszMainInstruction = rTitle;
            tdc.pszContent         = sQuestion.c_str();
            int     nClickedBtn    = 0;
            HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, &closeAllDoAll);
            if (CAppUtils::FailedShowMessage(hr))
                nClickedBtn = 0;
            if (nClickedBtn == 100)
            {
                // import the file
                CopyFile(files[0].c_str(), (CAppUtils::GetDataPath() + L"\\" + CPathUtils::GetFileName(files[0])).c_str(), FALSE);
                return;
            }
        }
    }

    BlockAllUIUpdates(true);
    OnOutOfScope(BlockAllUIUpdates(false););

    ShowProgressCtrl(static_cast<UINT>(GetInt64(DEFAULTS_SECTION, L"ProgressDelay", 1000)));
    OnOutOfScope(HideProgressCtrl());

    constexpr size_t maxFiles    = 100;
    int              fileCounter = 0;
    for (const auto& filename : files)
    {
        ++fileCounter;
        SetProgress(fileCounter, static_cast<DWORD>(files.size()));
        if (PathIsDirectory(filename.c_str()))
        {
            std::vector<std::wstring> recurseFiles;
            CDirFileEnum              enumerator(filename);
            bool                      bIsDir = false;
            std::wstring              path;
            // Collect no more than maxFiles + 1 files. + 1 so we know we have too many.
            while (enumerator.NextFile(path, &bIsDir, false))
            {
                if (!bIsDir)
                {
                    recurseFiles.push_back(std::move(path));
                    if (recurseFiles.size() > maxFiles)
                        break;
                }
            }
            if (recurseFiles.size() <= maxFiles)
            {
                std::sort(recurseFiles.begin(), recurseFiles.end(),
                          [](const std::wstring& lhs, const std::wstring& rhs) {
                              return CPathUtils::PathCompare(lhs, rhs) < 0;
                          });

                for (const auto& f : recurseFiles)
                    OpenFile(f, 0);
            }
        }
        else
            OpenFile(filename, OpenFlags::AddToMRU);
    }
}

void CMainWindow::HandleCopyDataCommandLine(const COPYDATASTRUCT& cds)
{
    CCmdLineParser parser(static_cast<LPCWSTR>(cds.lpData));
    LPCTSTR        path = parser.GetVal(L"path");
    if (path)
    {
        if (PathIsDirectory(path))
        {
            if (!m_fileTree.GetPath().empty()) // File tree not empty: create a new empty tab first.
                OpenNewTab();
            m_fileTree.SetPath(path);
            ShowFileTree(true);
        }
        else
        {
            auto list = GetFileListFromGlobPath(path);
            for (const auto p : list)
            {
                if (OpenFile(p, OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing) >= 0)
                {
                    if (parser.HasVal(L"line"))
                    {
                        GoToLine(static_cast<size_t>(parser.GetLongLongVal(L"line") - 1));
                    }
                }
            }
        }
    }
    else
    {
        // find out if there are paths specified without the key/value pair syntax
        int nArgs = 0;

        LPCWSTR* szArgList = const_cast<LPCWSTR*>(CommandLineToArgvW(static_cast<LPCWSTR>(cds.lpData), &nArgs));
        OnOutOfScope(LocalFree(szArgList););
        if (!szArgList)
            return;

        BlockAllUIUpdates(true);
        OnOutOfScope(BlockAllUIUpdates(false););
        int filesOpened = 0;
        // no need for a progress dialog here:
        // the command line is limited in size, so there can't be too many
        // filePaths passed here
        for (int i = 1; i < nArgs; i++)
        {
            if (szArgList[i][0] != '/')
            {
                if (PathIsDirectory(szArgList[i]))
                {
                    if (!m_fileTree.GetPath().empty()) // File tree not empty: create a new empty tab first.
                        OpenNewTab();
                    m_fileTree.SetPath(szArgList[i]);
                    ShowFileTree(true);
                }
                else
                {
                    auto list = GetFileListFromGlobPath(szArgList[i]);
                    for (const auto p : list)
                    {
                        if (OpenFile(p, OpenFlags::AddToMRU | OpenFlags::AskToCreateIfMissing) >= 0)
                            ++filesOpened;
                    }
                }
            }
        }

        if ((filesOpened == 1) && parser.HasVal(L"line"))
        {
            GoToLine(static_cast<size_t>(parser.GetLongLongVal(L"line") - 1));
        }
    }
}

// FIXME/TODO:
// If both instances have the same document loaded,
// things get tricky:
//
// Currently, if one instance instructs another instance to
// load a document the first instance already has open,
// the receiver will switch to it's own tab and copy of the document
// and silently throw the senders document away.
//
// This may cause the sender to lose work if their document was modified
// or more recent.
// This is what happens currently but is arguably wrong.
//
// If the receiver refuses to load the document, the sender might not actually
// have it open themselves and was just instructing the receiver to load it;
// if the receiver refuses, nothing will happen and that would be also wrong.
//
// The receiver could load the senders document but the receiver might have
// modified their version, in which case we don't know if it's
// safe to replace it and if we silently do they may lose something.
//
// Even if the receivers document isn't modified, there is no
// guarantee that the senders document is newer.
//
// The design probably needs to change so that the receiver asks
// the user what to do if they try to move a document to another
// when it's open in two places.
//
// The sender might also need to indicate to the receiver if it's ok
// for the receiver to switch to an already loaded instance.
//
// For now, continue to do what we've always have, which is to
// just switch to any existing document of the same name and
// throw the senders copy away if there is a clash.
//
// Will revisit this later.

// Returns true of the tab was moved in, else false.
// Callers using SendMessage can check if the receiver
// loaded the tab they sent before they close their tab
// it was sent from.

// TODO!: Moving a tab to another instance means losing
// undo history.
// Consider warning about that or if the undo history
// could be saved and restored.

// Called when a Tab is dropped over another instance of BowPad.
bool CMainWindow::HandleCopyDataMoveTab(const COPYDATASTRUCT& cds)
{
    std::wstring              paths = std::wstring(static_cast<const wchar_t*>(cds.lpData), cds.cbData / sizeof(wchar_t));
    std::vector<std::wstring> dataVec;
    stringtok(dataVec, paths, false, L"*");
    // Be a little untrusting of the clipboard data and if it's
    // a mistake by the caller let them know so they
    // don't throw away something we can't open.
    if (dataVec.size() != 4)
    {
        return false;
    }
    std::wstring realPath = dataVec[0];
    std::wstring tempPath = dataVec[1];
    bool         bMod     = _wtoi(dataVec[2].c_str()) != 0;
    int          line     = _wtoi(dataVec[3].c_str());

    // Unsaved files / New tabs have an empty real path.

    if (!realPath.empty()) // If this is a saved file
    {
        auto docID = m_docManager.GetIdForPath(realPath);
        if (docID.IsValid()) // If it already exists switch to it.
        {
            // TODO: we can lose work here, see notes above.
            // The document we switch to may be the same.
            // better to reject the move and return false or something.

            int tab = GetIndexFromID(docID);
            SetSelected(tab);
            DeleteFile(tempPath.c_str());

            return true;
        }
        // If it doesn't exist, fall through to load it.
    }
    bool opened = OpenFileAs(tempPath, realPath, bMod);
    if (opened)
        GoToLine(line);
    return opened;
}

static bool AskToCopyOrMoveFile(HWND hWnd, const std::wstring& filename, const std::wstring& hitpath, bool bCopy)
{
    ResString rTitle(g_hRes, IDS_FILE_DROP);
    ResString rQuestion(g_hRes, bCopy ? IDS_FILE_DROP_COPY : IDS_FILE_DROP_MOVE);
    ResString rDoIt(g_hRes, bCopy ? IDS_FILE_DROP_DOCOPY : IDS_FILE_DROP_DOMOVE);
    ResString rCancel(g_hRes, IDS_FILE_DROP_CANCEL);
    // Show exactly what we are creating.
    std::wstring sQuestion = CStringUtils::Format(rQuestion, filename.c_str(), hitpath.c_str());

    TASKDIALOGCONFIG  tdc               = {sizeof(TASKDIALOGCONFIG)};
    TASKDIALOG_BUTTON aCustomButtons[2] = {};
    int               bi                = 0;
    aCustomButtons[bi].nButtonID        = 101;
    aCustomButtons[bi++].pszButtonText  = rDoIt;
    aCustomButtons[bi].nButtonID        = 100;
    aCustomButtons[bi++].pszButtonText  = rCancel;
    tdc.pButtons                        = aCustomButtons;
    tdc.cButtons                        = bi;
    assert(tdc.cButtons <= _countof(aCustomButtons));
    tdc.nDefaultButton = 101;

    tdc.hwndParent         = hWnd;
    tdc.hInstance          = g_hRes;
    tdc.dwFlags            = TDF_USE_COMMAND_LINKS | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ALLOW_DIALOG_CANCELLATION;
    tdc.pszWindowTitle     = MAKEINTRESOURCE(IDS_APP_TITLE);
    tdc.pszMainIcon        = TD_INFORMATION_ICON;
    tdc.pszMainInstruction = rTitle;
    tdc.pszContent         = sQuestion.c_str();
    int     nClickedBtn    = 0;
    HRESULT hr             = TaskDialogIndirect(&tdc, &nClickedBtn, nullptr, nullptr);
    if (CAppUtils::FailedShowMessage(hr))
        nClickedBtn = 0;
    bool bDoIt = (nClickedBtn == 101);
    return bDoIt;
}

void CMainWindow::HandleTabDroppedOutside(int tab, POINT pt)
{
    // Start a new instance of BowPad with this dropped tab, or add this tab to
    // the BowPad window the drop was done on. Then close this tab.
    // First save the file to a temp location to ensure all unsaved mods are saved.
    std::wstring tempPath    = CTempFiles::Instance().GetTempFilePath(true);
    auto         docID       = GetIDFromIndex(tab);
    auto&        doc         = m_docManager.GetModDocumentFromID(docID);
    HWND         hDroppedWnd = WindowFromPoint(pt);
    if (hDroppedWnd)
    {
        // If the drop target identifies a specific instance of BowPad found
        // move the tab to that instance OR
        // if not, create a new instance of BowPad and move the tab to that.

        bool isThisInstance;
        HWND hMainWnd = FindAppMainWindow(hDroppedWnd, &isThisInstance);
        // Dropping on our own window shall create an new BowPad instance
        // because the user doesn't want to be moving a tab around within
        // the same instance of BowPad this way.
        if (hMainWnd && !isThisInstance)
        {
            // dropped on another BowPad Window, 'move' this tab to that BowPad Window
            CDocument tempDoc = doc;
            tempDoc.m_path    = tempPath;
            bool bDummy       = false;
            bool bModified    = doc.m_bIsDirty || doc.m_bNeedsSaving;
            m_docManager.SaveFile(/**this, */tempDoc, bDummy);

            COPYDATASTRUCT cpd  = {0};
            cpd.dwData          = CD_COMMAND_MOVETAB;
            std::wstring cpData = doc.m_path + L"*" + tempPath + L"*";
            cpData += bModified ? L"1*" : L"0*";
            cpData += std::to_wstring(m_editor.GetCurrentLineNumber() + 1);
            cpd.lpData = static_cast<PVOID>(const_cast<LPWSTR>(cpData.c_str()));
            cpd.cbData = static_cast<DWORD>(cpData.size() * sizeof(wchar_t));

            // It's an important concept that the receiver can reject/be unable
            // to load / handle the file/tab we are trying to move to them.
            // We don't want the user to lose their work if that happens.
            // So only close this tab if the move was successful.
            bool moved = SendMessage(hMainWnd, WM_COPYDATA, reinterpret_cast<WPARAM>(m_hwnd), reinterpret_cast<LPARAM>(&cpd)) ? true : false;
            if (moved)
            {
                // remove the tab
                CloseTab(tab, true);
            }
            else
            {
                // TODO.
                // On error, in theory the sender or receiver or both can display an error.
                // Until it's clear what set of errors and handling is required just throw
                // up a simple dialog box this side. In theory the receiver may even crash
                // with no error, so probably some minimal message might be useful.
                ::MessageBox(*this, L"Failed to move Tab.", GetAppName().c_str(), MB_OK | MB_ICONERROR);
            }
            return;
        }
        if ((hDroppedWnd == m_fileTree) && (!doc.m_path.empty()))
        {
            // drop over file tree
            auto hitPath = m_fileTree.GetDirPathForHitItem();
            if (!hitPath.empty())
            {
                auto fileName = CPathUtils::GetFileName(doc.m_path);
                auto destPath = CPathUtils::Append(hitPath, fileName);

                if (CPathUtils::PathCompare(doc.m_path, destPath))
                {
                    bool bCopy = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    if (AskToCopyOrMoveFile(*this, fileName, destPath, bCopy))
                    {
                        if (bCopy)
                        {
                            CDocument tempDoc = doc;
                            tempDoc.m_path    = destPath;
                            bool bDummy       = false;
                            m_docManager.SaveFile(/**this,*/ tempDoc, bDummy);
                            OpenFile(destPath, OpenFlags::AddToMRU);
                        }
                        else
                        {
                            if (MoveFile(doc.m_path.c_str(), destPath.c_str()))
                            {
                                doc.m_path = destPath;
                                SetSelected(tab);
                            }
                        }
                    }
                    return;
                }
            }
        }
    }
    CDocument tempDoc = doc;
    tempDoc.m_path    = tempPath;
    bool bDummy       = false;
    bool bModified    = doc.m_bIsDirty || doc.m_bNeedsSaving;
    m_docManager.SaveFile(tempDoc, bDummy);

    // have the plugins save any information for this document
    // before we start the new process!
    CCommandHandler::Instance().OnDocumentClose(docID);

    // Start a new instance and open the tab there.
    std::wstring modPath = CPathUtils::GetModulePath();
    std::wstring cmdLine = CStringUtils::Format(L"/multiple /tabmove /savepath:\"%s\" /path:\"%s\" /line:%Id /title:\"%s\"",
                                                doc.m_path.c_str(), tempPath.c_str(),
                                                m_editor.GetCurrentLineNumber() + 1,
                                                L"");
    if (bModified)
        cmdLine += L" /modified";
    SHELLEXECUTEINFO shExecInfo = {};
    shExecInfo.cbSize           = sizeof(SHELLEXECUTEINFO);

    shExecInfo.hwnd         = *this;
    shExecInfo.lpVerb       = L"open";
    shExecInfo.lpFile       = modPath.c_str();
    shExecInfo.lpParameters = cmdLine.c_str();
    shExecInfo.nShow        = SW_NORMAL;

    if (ShellExecuteEx(&shExecInfo))
        CloseTab(tab, true);
}

bool CMainWindow::ReloadTab(int tab, int encoding, bool dueToOutsideChanges)
{
    auto docID = GetIDFromIndex(tab);
    if (!docID.IsValid()) // No such tab.
        return false;
    if (!m_docManager.HasDocumentID(docID)) // No such document
        return false;
    bool  bReloadCurrentTab = (tab == GetCurrentTabIndex());
    auto& doc               = m_docManager.GetModDocumentFromID(docID);
    // if encoding is set to default, use the current encoding
    if (encoding == -1)
        encoding = doc.m_encoding;

    CScintillaWnd* editor = bReloadCurrentTab ? &m_editor : &m_scratchEditor;
    if (dueToOutsideChanges)
    {
        ResponseToOutsideModifiedFile response = AskToReloadOutsideModifiedFile(doc);

        // Responses: Cancel, Save and Reload, Reload

        if (response == ResponseToOutsideModifiedFile::Reload) // Reload
        {
            // Fall through to reload
        }
        // Save if asked to. Assume we won't have asked them to save if
        // the file isn't dirty or it wasn't appropriate to save.
        else if (response == ResponseToOutsideModifiedFile::KeepOurChanges) // Save
        {
            SaveDoc(docID);
        }
        else // Cancel or failed to ask
        {
            // update the fileTime of the document to avoid this warning
            m_docManager.UpdateFileTime(doc, false);
            // the current content of the tab is possibly different
            // than the content on disk: mark the content as dirty
            // so the user knows he can save the changes.
            doc.m_bIsDirty     = true;
            doc.m_bNeedsSaving = true;
            // the next to calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
            editor->Scintilla().AddUndoAction(0, Scintilla::UndoFlags::None);
            editor->Scintilla().Undo();
            return false;
        }
    }
    else if (doc.m_bIsDirty || doc.m_bNeedsSaving)
    {
        if (!AskToReload(doc)) // User doesn't want to reload.
            return false;
    }

    if (!bReloadCurrentTab)
        editor->Scintilla().SetDocPointer(doc.m_document);

    // LoadFile increases the reference count, so decrease it here first
    editor->Scintilla().ReleaseDocument(doc.m_document);
    CDocument docReload = m_docManager.LoadFile(/**this,*/ doc.m_path, encoding, false);
    if (!docReload.m_document)
    {
        // since we called SCI_RELEASEDOCUMENT but LoadFile did not properly load,
        // we have to increase the reference count again. Otherwise the document
        // might get completely released.
        editor->Scintilla().AddRefDocument(doc.m_document);
        return false;
    }

    if (bReloadCurrentTab)
    {
        editor->SaveCurrentPos(doc.m_position);
        // Apply the new one.
        editor->Scintilla().SetDocPointer(docReload.m_document);
    }

    docReload.m_position          = doc.m_position;
    docReload.m_bIsWriteProtected = doc.m_bIsWriteProtected;
    docReload.m_saveCallback      = doc.m_saveCallback;
    auto lang                     = doc.GetLanguage();
    doc                           = docReload;
    editor->SetupLexerForLang(lang);
    doc.SetLanguage(lang);
    editor->RestoreCurrentPos(docReload.m_position);
    editor->Scintilla().SetReadOnly(docReload.m_bIsWriteProtected);
    editor->EnableChangeHistory();
    RefreshAnnotations();

    //TBHDR tbHdr        = {};
    //tbHdr.hdr.hwndFrom = *this;
    //tbHdr.hdr.code = TCN_;// TCN_RELOAD;
    //tbHdr.hdr.idFrom   = tab;
    //tbHdr.tabOrigin    = tab;
    //CCommandHandler::Instance().TabNotify(&tbHdr);
    CCommandHandler::Instance().OnStylesSet();

    if (bReloadCurrentTab)
        UpdateStatusBar(true);
    UpdateTab(docID);
    if (bReloadCurrentTab)
    {
        editor->Scintilla().SetSavePoint();
        editor->EnableChangeHistory();
    }
    // refresh the file tree
    m_fileTree.SetPath(m_fileTree.GetPath(), !dueToOutsideChanges);

    return true;
}

// Return true if requested to removed document.
bool CMainWindow::HandleOutsideDeletedFile(int tab)
{
    assert(tab == GetCurrentTabIndex());
    auto  docID = GetIDFromIndex(tab);
    auto& doc   = m_docManager.GetModDocumentFromID(docID);
    // file was removed. Options are:
    // * keep the file open
    // * close the tab
    if (!AskAboutOutsideDeletedFile(doc))
    {
        // User wishes to close the tab.
        CloseTab(tab);
        return true;
    }

    // keep the file: mark the file as modified
    doc.m_bNeedsSaving = true;
    // update the fileTime of the document to avoid this warning
    m_docManager.UpdateFileTime(doc, false);
    // the next to calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
    m_editor.Scintilla().AddUndoAction(0, Scintilla::UndoFlags::None);
    m_editor.Scintilla().Undo();
    return false;
}

bool CMainWindow::HasOutsideChangesOccurred() const
{
    int tabCount = GetItemCount();
    for (int i = 0; i < tabCount; ++i)
    {
        auto docID = GetIDFromIndex(i);
        auto ds    = m_docManager.HasFileChanged(docID);
        if (ds == DocModifiedState::Modified || ds == DocModifiedState::Removed)
            return true;
    }
    return false;
}

void CMainWindow::CheckForOutsideChanges()
{
    static bool bInsideCheckForOutsideChanges = false;
    if (bInsideCheckForOutsideChanges)
        return;

    // See if any doc has been changed externally.
    bInsideCheckForOutsideChanges = true;
    OnOutOfScope(bInsideCheckForOutsideChanges = false;);
    bool bChangedTab = false;
    int  activeTab   = GetCurrentTabIndex();
    {
        responseToOutsideModifiedFileDoAll = FALSE;
        doModifiedAll                      = true;
        OnOutOfScope(
            responseToOutsideModifiedFileDoAll = FALSE;
            doModifiedAll                      = false;);
        for (int i = 0; i < GetItemCount(); ++i)
        {
            auto docID = GetIDFromIndex(i);
            auto ds    = m_docManager.HasFileChanged(docID);
            if (ds == DocModifiedState::Modified || ds == DocModifiedState::Removed)
            {
                const auto& doc = m_docManager.GetDocumentFromID(docID);
                if ((ds != DocModifiedState::Removed) && !doc.m_bIsDirty && !doc.m_bNeedsSaving && GetInt64(DEFAULTS_SECTION, L"AutoReload"))
                {
                    ReloadTab(i, -1, true);
                }
                else
                {
                    SetSelected(i);
                    if (i != activeTab)
                        bChangedTab = true;
                }
            }
        }
    }

    if (bChangedTab)
        SetSelected(activeTab);
}

void CMainWindow::ShowFileTree(bool bShow)
{
    m_fileTreeVisible = bShow;
    ShowWindow(m_fileTree, m_fileTreeVisible ? SW_SHOW : SW_HIDE);
    ResizeChildWindows();
    if (bShow)
        InvalidateRect(m_hwnd, nullptr, TRUE);
    SendMessage(m_quickbar, TB_CHECKBUTTON, cmdFileTree, m_fileTreeVisible);
    SetInt64(DEFAULTS_SECTION, L"FileTreeVisible", m_fileTreeVisible);
}

void CMainWindow::OpenFiles(const std::vector<std::wstring>& paths)
{
    if (paths.size() == 1)
    {
        if (!paths[0].empty())
        {
            unsigned int openFlags = OpenFlags::AddToMRU;
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
                openFlags |= OpenFlags::OpenIntoActiveTab;
            OpenFile(paths[0], openFlags);
        }
    }
    else if (paths.size() > 0)
    {
        BlockAllUIUpdates(true);
        OnOutOfScope(BlockAllUIUpdates(false));
        ShowProgressCtrl(static_cast<UINT>(GetInt64(DEFAULTS_SECTION, L"ProgressDelay", 1000)));
        OnOutOfScope(HideProgressCtrl());

        // Open all that was m_selected or at least returned.
        DocID docToActivate;
        int   fileCounter = 0;
        for (const auto& file : paths)
        {
            ++fileCounter;
            SetProgress(fileCounter, static_cast<DWORD32>(paths.size()));
            // Remember whatever we first successfully open in order to return to it.
            if (OpenFile(file, OpenFlags::AddToMRU) >= 0 && !docToActivate.IsValid())
                docToActivate = m_docManager.GetIdForPath(file);
        }

        if (docToActivate.IsValid())
        {
            auto tabToActivate = GetIndexFromID(docToActivate);
            SetSelected(tabToActivate);
        }
    }
}

void CMainWindow::BlockAllUIUpdates(bool block)
{
    if (block)
    {
        if (m_blockCount == 0)
            SendMessage(*this, WM_SETREDRAW, FALSE, 0);
        FileTreeBlockRefresh(block);
        ++m_blockCount;
    }
    else
    {
        --m_blockCount;
        if (m_blockCount == 0)
        {
            // unblock
            SendMessage(*this, WM_SETREDRAW, TRUE, 0);
            // force a redraw
            RedrawWindow(*this, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        // FileTreeBlockRefresh maintains it's own count.
        FileTreeBlockRefresh(block);
        if (m_blockCount == 0)
            ResizeChildWindows();
    }
}

int CMainWindow::UnblockUI()
{
    auto blockCount = m_blockCount;
    // Only unblock if it was blocked.
    if (m_blockCount > 0)
    {
        m_blockCount = 0;
        SendMessage(*this, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(*this, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    return blockCount;
}

void CMainWindow::ReBlockUI(int blockCount)
{
    if (blockCount)
    {
        m_blockCount = blockCount;
        SendMessage(*this, WM_SETREDRAW, FALSE, 0);
    }
}

void CMainWindow::ShowProgressCtrl(UINT delay)
{
    m_progressBar.SetDarkMode(CTheme::Instance().IsDarkTheme(), CTheme::Instance().GetThemeColor(GetSysColor(COLOR_WINDOW)));
    RECT rect;
    GetClientRect(*this, &rect);
    MapWindowPoints(*this, nullptr, reinterpret_cast<LPPOINT>(&rect), 2);
    SetWindowPos(m_progressBar, nullptr, rect.left, rect.bottom - m_statusBar.GetHeight(), rect.right - rect.left, m_statusBar.GetHeight(), SWP_NOACTIVATE | SWP_NOCOPYBITS);

    m_progressBar.ShowWindow(delay);
}

void CMainWindow::HideProgressCtrl()
{
    ShowWindow(m_progressBar, SW_HIDE);
}

void CMainWindow::SetProgress(DWORD32 pos, DWORD32 end)
{
    m_progressBar.SetRange(0, end);
    m_progressBar.SetPos(pos);
    UpdateWindow(m_progressBar);
}

void CMainWindow::SetFileTreeWidth(int width)
{
    m_treeWidth = width;
    m_treeWidth = max(50, m_treeWidth);
    RECT rc;
    GetClientRect(*this, &rc);
    m_treeWidth = min(m_treeWidth, rc.right - rc.left - 200);
}

void CMainWindow::AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words)
{
    m_autoCompleter.AddWords(lang, words);
}

void CMainWindow::AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words)
{
    m_autoCompleter.AddWords(lang, words);
}

void CMainWindow::AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words)
{
    m_autoCompleter.AddWords(docID, words);
}

void CMainWindow::AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words)
{
    m_autoCompleter.AddWords(docID, words);
}

static void SetModeForWindow(HWND hWnd, BOOL darkFlag)
{
    DarkModeHelper::Instance().AllowDarkModeForWindow(hWnd, darkFlag);
    SetWindowTheme(hWnd, L"Explorer", nullptr);
}

void CMainWindow::SetTheme(bool dark)
{
    SetModeForWindow(m_hwnd,dark);
    SetModeForWindow(m_statusBar,dark);
    SetModeForWindow(m_fileTree,dark);
    SetModeForWindow(m_editor,dark);
    
    DarkModeHelper::WINDOWCOMPOSITIONATTRIBDATA data     = {DarkModeHelper::WCA_USEDARKMODECOLORS, &dark, sizeof(dark)};
    DarkModeHelper::Instance().SetWindowCompositionAttribute(*this, &data);
    DarkModeHelper::Instance().FlushMenuThemes();
    DarkModeHelper::Instance().AllowDarkModeForApp(dark);
    DarkModeHelper::Instance().RefreshImmersiveColorPolicyState();

    auto activeTabId = GetCurrentTabId();
    if (activeTabId.IsValid())
    {
        m_editor.Scintilla().ClearDocumentStyle();
        m_editor.Scintilla().Colourise(0, m_editor.Scintilla().PositionFromLine(m_editor.Scintilla().LinesOnScreen() + 1));
        const auto& doc = m_docManager.GetDocumentFromID(activeTabId);
        m_editor.SetupLexerForLang(doc.GetLanguage());
        RefreshAnnotations();
    }
    DarkModeHelper::Instance().RefreshTitleBarThemeColor(*this, dark);
    CCommandHandler::Instance().OnThemeChanged(dark);
    
    //SendMessage(m_quickbar, TB_CHANGEBITMAP, cmdOpenRecent, dark ? 11 : 0);
    RedrawWindow(*this, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE | RDW_INTERNALPAINT | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASENOW);
}

void CMainWindow::RefreshAnnotations()
{
    m_lastCheckedLine = 0;
    SetTimer(*this, TIMER_CHECKLINES, 300, nullptr);
}

TitlebarRect CMainWindow::GetHoveredRect()
{
    POINT cursorPoint;
    GetCursorPos(&cursorPoint);
    ScreenToClient(*this,&cursorPoint);

    TitlebarRect hoveredRect = TitlebarRect::None;
    if (PtInRect(&m_allRects.system, cursorPoint))
    {
        hoveredRect = TitlebarRect::System;
    }
    else if (PtInRect(&m_allRects.close, cursorPoint))
    {
        hoveredRect = TitlebarRect::Close;
    }
    else if (PtInRect(&m_allRects.minimize, cursorPoint))
    {
        hoveredRect = TitlebarRect::Minimize;
    }
    else if (PtInRect(&m_allRects.maximize, cursorPoint))
    {
        hoveredRect = TitlebarRect::Maximize;
    }
    else if (PtInRect(&m_allRects.leftRoller, cursorPoint))
    {
        hoveredRect = TitlebarRect::LeftRoller;
    }
    else if (PtInRect(&m_allRects.rightRoller, cursorPoint))
    {
        hoveredRect = TitlebarRect::RightRoller;
    }
    else if (PtInRect(&m_allRects.text, cursorPoint))
    {
        hoveredRect = TitlebarRect::Text;
    }
    else if (PtInRect(&m_allRects.tabs, cursorPoint))
    {
        hoveredRect = TitlebarRect::Tabs;
    }
    else if (PtInRect(&m_allRects.showMore, cursorPoint))
    {
        hoveredRect = TitlebarRect::ShowMore;
    }

    return hoveredRect;
}

void CMainWindow::DrawTabs(HDC hdc)
{
    THEME theme = CTheme::CurrentTheme();
    
    RECT tabrc = m_allRects.total;
    tabrc.top += theme.titleHeight;

    Gdiplus::Graphics   graphics(hdc);
    Gdiplus::SolidBrush brush(ColorFrom(theme.tabBack));
    graphics.FillRectangle(&brush, 0.0f, tabrc.top * 1.0f, WidthOf(tabrc) * 1.0f, HeightOf(tabrc) * 1.0f);
    brush.SetColor(ColorFrom(theme.selBack));
    graphics.FillRectangle(&brush, 0, tabrc.bottom - 2, WidthOf(tabrc), 2);

    Gdiplus::Font font = FontFrom(theme); 

    if (m_fontName != theme.fontName || m_fontSize != theme.fontSize)
    {
        m_fontName = theme.fontName;
        m_fontSize = theme.fontSize;

        Gdiplus::SizeF layoutSize(FLT_MAX, FLT_MAX);
        Gdiplus::SizeF size;
        for (int i = 0; i < GetItemCount(); i++)
        {
            graphics.MeasureString(m_allTabs[i].name.c_str(), -1, &font, layoutSize, nullptr, &size);
            m_allTabs[i].width  = static_cast<int>(size.Width);
            m_allTabs[i].height = static_cast<int>(size.Height);
        }
    }

    int len  = GetItemCount();
    int left = m_allRects.tabs.left + 4;   

    RECT itemRect = m_allRects.tabs;
    Pen  pen(ColorFrom(theme.selFore));

    for (int i = m_firstVisibleItemIdx; i < len; i++)
    {
        m_lastVisibleItemIdx = i;
        std::wstring text  = m_allTabs[i].name;

        int right = left + AdjustItemWidth(m_allTabs[i].width);
        if (right > m_allRects.tabs.right) 
        {
            m_lastVisibleItemIdx = i - 1;
            break;
        }

        // Cached visible item rects
        itemRect.left   = left;
        itemRect.top    = tabrc.top + 4;
        itemRect.right  = right;
        itemRect.bottom = tabrc.bottom;
        left            = right;

        // Draw item
        Gdiplus::PointF pos((itemRect.left + ITEM_XPADDING) * 1.0f,
                            (itemRect.top + ITEM_YPADDING + (HeightOf(m_allRects.tabs) - m_allTabs[i].height) / 2 - 4) * 1.0f);
        
        pen.SetColor(ColorFrom(i == m_selected || m_hoveredItemIdx == i ? theme.selFore : theme.tabFore));
        auto& doc = m_docManager.GetModDocumentFromID(m_allTabs[i].id);
        if (doc.m_bIsDirty)
            pen.SetColor(Color(255, 0, 0));
        if (m_selected == i || m_hoveredItemIdx == i)
        {
            brush.SetColor(ColorFrom(m_hoveredItemIdx == i ? theme.tabHover : theme.selBack));
            
            Gdiplus::GraphicsPath pPath;
            GetRoundRectPath(&pPath, Rect(itemRect.left, itemRect.top, WidthOf(itemRect), HeightOf(itemRect)), 6);
            graphics.FillPath(&brush, &pPath);

            int leftPos = itemRect.right - 16;
            int topPos  = itemRect.top + (HeightOf(itemRect) - 9) / 2;

            graphics.DrawLine(&pen, Point(leftPos, topPos), Point(leftPos + 9, topPos + 9));
            graphics.DrawLine(&pen, Point(leftPos, topPos + 9), Point(leftPos + 9, topPos));
        }
        
        brush.SetColor(ColorFrom(m_hoveredItemIdx == i || m_selected == i ? theme.selFore : theme.tabFore));
        SetBkMode(hdc, TRANSPARENT);
        if (doc.m_bIsDirty)
            brush.SetColor(Color(255, 0, 0));

        Gdiplus::PointF iconPos(pos.X - 14, pos.Y - 2);
        
        if (m_allTabs[i].state == UNSAVED_DOC)
        {
             std::wstring iconText = L"~";
             iconPos.X -= 5;
             graphics.DrawString(iconText.c_str(), -1, &font, iconPos, &brush);
        }
        else if (m_allTabs[i].state == READONLY_DOC)
        {
            pen.SetWidth(2);
            float middle = iconPos.Y + (m_allTabs[i].height - 8) / 2 - 3;
            graphics.DrawEllipse(&pen, iconPos.X + 2, middle, 8.0f, 8.0f);
            graphics.DrawLine(&pen,iconPos.X + 6, middle + 8, iconPos.X + 6, middle + 18);
            graphics.DrawLine(&pen, iconPos.X + 6, middle + 14, iconPos.X + 11, middle + 14);
            pen.SetWidth(1);
        }
        pos.Y += 1; // Justify text vertical position
        graphics.DrawString(text.c_str(), -1, &font, pos, &brush);
    }

    //Draw tab bar scroll arrow
    int                       mPos = m_allRects.leftRoller.top + HeightOf(tabrc) / 2;
    bool               disableLeft = m_firstVisibleItemIdx == 0;
    bool              disableRight = m_lastVisibleItemIdx >= GetItemCount() - 1;
    RECT                 arrowRect = { 0,0,10,10 };
    RECT                 hoverRect = { 0,0,HeightOf(tabrc) / 2,HeightOf(tabrc) / 2 };
    
    //Draw left roller
    if (GetHoveredRect() == TitlebarRect::LeftRoller)
    {
        CenterRectInRect(&hoverRect, &m_allRects.leftRoller);
        OffsetRect(&hoverRect, 1,0);
        Gdiplus::Rect r(hoverRect.left, hoverRect.top, WidthOf(hoverRect), HeightOf(hoverRect));
        brush.SetColor(ColorFrom(theme.itemHover));
        graphics.FillRectangle(&brush, r);
    }

    CenterRectInRect(&arrowRect, &m_allRects.leftRoller);
    Gdiplus::Point leftArrow[] = { {arrowRect.left, mPos}, {arrowRect.right, arrowRect.top}, {arrowRect.right, arrowRect.bottom} };
    brush.SetColor(ColorFrom(disableLeft ? theme.arrowDisable : theme.arrowFore));
    graphics.FillPolygon(&brush, leftArrow, _countof(leftArrow));

    //Draw right roller
    if (GetHoveredRect() == TitlebarRect::RightRoller)
    {
        CenterRectInRect(&hoverRect, &m_allRects.rightRoller);
        Gdiplus::Rect r(hoverRect.left, hoverRect.top, WidthOf(hoverRect), HeightOf(hoverRect));
        brush.SetColor(ColorFrom(theme.itemHover));
        graphics.FillRectangle(&brush, r);
    }

    CenterRectInRect(&arrowRect, &m_allRects.rightRoller);
    Gdiplus::Point rightArrow[] = { {arrowRect.left, arrowRect.top}, {arrowRect.left, arrowRect.bottom}, {arrowRect.right, mPos} };
    brush.SetColor(ColorFrom(disableRight ? theme.arrowDisable : theme.arrowFore));
    graphics.FillPolygon(&brush, rightArrow, _countof(rightArrow));

    //Draw show more button
    RECT icon_rect = { 0, 0, 14, 14 };
    CenterRectInRect(&icon_rect, &m_allRects.showMore);
    Gdiplus::Point    downArrow[] = { {icon_rect.left + 2, icon_rect.top + 3}, {icon_rect.right - 2, icon_rect.top + 3}, {(WidthOf(icon_rect) - 4) / 2 + icon_rect.left + 2, icon_rect.bottom - 2}};
    if (GetHoveredRect() == TitlebarRect::ShowMore)
    {
        InflateRect(&icon_rect, 4, 4);
        Gdiplus::Rect r(icon_rect.left,icon_rect.top,WidthOf(icon_rect),HeightOf(icon_rect));
        brush.SetColor(ColorFrom(theme.itemHover));
        graphics.FillRectangle(&brush, r);
    }
    brush.SetColor(ColorFrom(theme.arrowFore));
    graphics.FillPolygon(&brush, downArrow, _countof(downArrow));
}

void CMainWindow::DrawTitlebar(HDC hdc) {
    THEME theme  = CTheme::CurrentTheme();
    //// Paint Background
    HBRUSH bg_brush = CreateSolidBrush(theme.winBack);
    RECT   titlebarRect = m_allRects.total;
    titlebarRect.bottom -= theme.tabHeight;
    FillRect(hdc, &titlebarRect, bg_brush);
    HBRUSH      hoverBrush  = CreateSolidBrush(theme.itemHover);

    switch (m_hoveredRect)
    {
        case TitlebarRect::Minimize:
            FillRect(hdc, &m_allRects.minimize, hoverBrush);
            break;
        case TitlebarRect::Maximize:
            FillRect(hdc, &m_allRects.maximize, hoverBrush);
            break;
        case TitlebarRect::Close:
            FillRect(hdc, &m_allRects.close, hoverBrush);
            break;
        case TitlebarRect::System:
            FillRect(hdc, &m_allRects.system, hoverBrush);
            break;
    }
      
    DeleteObject(hoverBrush);

    HPEN pen = CreatePen(PS_SOLID, 1, theme.winFore);
    SelectObject(hdc, pen);
    
    // Draw system menu icon
    int left = m_allRects.system.left + 8;
    int middle = m_allRects.system.top + HeightOf(m_allRects.system) / 2 - 3;
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    Gdiplus::SolidBrush brush(ColorFrom(theme.winFore));
    graphics.FillEllipse(&brush, left + 4, middle, 6,6); left += 10;
    graphics.FillEllipse(&brush, left + 4, middle, 6,6); left += 10;
    graphics.FillEllipse(&brush, left + 4, middle, 6,6);
    
    // Draw close button
    RECT icon_rect = { 0, 0, 12, 12 };
    CenterRectInRect(&icon_rect, &m_allRects.close);
    Gdiplus::Pen gpen(ColorFrom(theme.winFore), 1.0f);
    graphics.DrawLine(&gpen, (INT)icon_rect.left, (INT)icon_rect.top, (INT)icon_rect.right, (INT)icon_rect.bottom);
    graphics.DrawLine(&gpen, (INT)icon_rect.left, (INT)icon_rect.bottom, (INT)icon_rect.right, (INT)icon_rect.top);
    
    // Draw maximize button
    CenterRectInRect(&icon_rect, &m_allRects.maximize);
    SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    if (IsMaximized(m_hwnd))
    {
        OffsetRect(&icon_rect, -1, 1);
        Rectangle(hdc, icon_rect.left + 3, icon_rect.top - 3, icon_rect.right + 3, icon_rect.bottom - 3);
        FillRect(hdc, &icon_rect, bg_brush);
    }
    Rectangle(hdc, icon_rect.left, icon_rect.top, icon_rect.right, icon_rect.bottom);

    // Draw minimized button
    icon_rect = { 0, 0, 12, 1 };
    CenterRectInRect(&icon_rect, &m_allRects.minimize);
    Rectangle(hdc, icon_rect.left, icon_rect.top, icon_rect.right, icon_rect.bottom);

    // Draw title text
    SetBkColor(hdc, theme.winBack);
    SetTextColor(hdc, theme.winFore);
    SIZE size;
    GetTextExtentPoint32(hdc, m_titleText.c_str(), static_cast<int>(m_titleText.size()), &size);
    icon_rect = m_allRects.text;
    icon_rect.top += (HeightOf(icon_rect) - size.cy) / 2;
    DrawText(hdc, m_titleText.c_str(), -1, &icon_rect, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

    DeleteObject(pen);
    DeleteObject(bg_brush);
}

int CMainWindow::GetSelected() const
{
    return m_selected;
}

void CMainWindow::SetSelected(int idx)
{
    if (idx < 0 || idx > GetItemCount() - 1)
        return;

    HandleTabChanging();

    m_selected = idx;
    
    HandleTabChange();

    if (!(m_selected >= m_firstVisibleItemIdx && m_selected <= m_lastVisibleItemIdx))
    {
        int len = WidthOf(m_allRects.tabs);

        if (m_selected > m_lastVisibleItemIdx)
        {
            int w                = 0;
            m_lastVisibleItemIdx = m_selected;
            for (int i = m_selected, k = m_firstVisibleItemIdx; i >= k; i--)
            {
                w = w + AdjustItemWidth(m_allTabs[i].width);
                if (w > len)
                    break;

                m_firstVisibleItemIdx = i;
            }
        }
        else if (m_selected < m_firstVisibleItemIdx)
        {
            int w                 = 0;
            m_firstVisibleItemIdx = m_selected;
            for (int i = m_selected; i < GetItemCount(); i++)
            {
                w = w + AdjustItemWidth(m_allTabs[i].width);
                if (w > len)
                    break;
                m_lastVisibleItemIdx = i;
            }
        }
    }

    InvalidateRect(*this, &m_allRects.tabs, FALSE);
}

int CMainWindow::AdjustItemWidth(int width)
{
    return width + ITEM_XPADDING * 2;
}


DocID CMainWindow::GetIDFromIndex(int index) const
{
    if (index < 0 || GetItemCount() - 1 < index)
        return {};

    return m_allTabs[index].id;
}

int CMainWindow::GetIndexFromID(DocID id) const
{
    if (id.IsValid()) // Only look for an id that's legal to begin with.
    {
        for (int i = 0; i < GetItemCount(); ++i)
        {
            if (GetIDFromIndex(i) == id)
                return i;
        }
    }
    return -1;
}

int CMainWindow::InsertAtEnd(const wchar_t* tabName)
{
    ItemMetrics ii;

    ii.name = CPathUtils::GetFileName(tabName);
    ii.path = CPathUtils::GetParentDirectory(tabName);
    ii.id = DocID(m_tabID++);
    
    THEME             theme = CTheme::CurrentTheme();
    Gdiplus::Graphics graph(GetDC(*this));
    Gdiplus::Font     font = FontFrom(theme); 
    PointF            pos(0.0f, 0.0f);
    RectF             boundRect;

    graph.MeasureString(ii.name.c_str(), -1, &font, pos, &boundRect);
    ii.width  = static_cast<int>(boundRect.Width);
    ii.height = static_cast<int>(boundRect.Height);

    m_allTabs.push_back(ii);

    return GetItemCount() - 1;
}

int CMainWindow::GetCurrentTabIndex() const
{
    return m_selected;
}

DocID CMainWindow::GetCurrentTabId() const
{
    if (GetItemCount() < 1)
        return {};

    return m_allTabs[m_selected].id;
}

int CMainWindow::GetItemCount() const
{
    return static_cast<int>(m_allTabs.size());
}

void CMainWindow::AddToRecents(std::wstring path)
{
    if (std::find(m_recents.begin(), m_recents.end(), path) == m_recents.end())
    {
        if (m_recents.size() > RECENTS_LENGTH - 1)
            m_recents.erase(m_recents.begin());

        m_recents.push_back(path);
    }

    SendMessage(m_quickbar, TB_SETSTATE, cmdOpenRecent, TBSTATE_ENABLED);
}
void CMainWindow::OpenFolder(std::wstring path)
{
    AddToRecents(path);
    m_fileTree.SetPath(path.c_str());
    m_titleText = path;
    SendMessage(*this, WM_SETTEXT, 0, 0);
    ShowFileTree(TRUE);
}

void CMainWindow::ShowAutoComplete()
{
    SCNotification scn{};
    scn.ch         = -1;
    scn.nmhdr.code = SCN_CHARADDED;
    scn.nmhdr.idFrom = cmdShowAutoComplete;
    m_autoCompleter.HandleScintillaEvents(&scn);
}

BOOL CMainWindow::HasActiveDocument() 
{
    return m_docManager.HasDocumentID(GetCurrentTabId());
}

const CDocument& CMainWindow::GetActiveDocument() const
{
    return m_docManager.GetDocumentFromID(GetCurrentTabId());
}

LRESULT CMainWindow::HandleQuickbarCustomDraw(const LPNMTBCUSTOMDRAW pCustomDraw)
{
    HDC hdc = pCustomDraw->nmcd.hdc;
    BOOL isDark = CTheme::Instance().IsDarkTheme();
    THEME theme = CTheme::CurrentTheme();

    if (pCustomDraw->nmcd.dwDrawStage == CDDS_PREPAINT)
    {
        RECT          rect;
        GetClientRect(m_quickbar, &rect);
        HBRUSH brush = CreateSolidBrush(theme.winBack);
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }
    else if (pCustomDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
    {
        RECT rc = pCustomDraw->nmcd.rc;
        UINT  state = pCustomDraw->nmcd.uItemState;
        UINT  item = static_cast<UINT>(pCustomDraw->nmcd.dwItemSpec);

        // Calculate image position.
        int cx = 0;
        int cy = 0;
        HIMAGELIST imageList = m_quickbarImages;// isDark ? m_darkImages : m_lightImages;
        ImageList_GetIconSize(imageList, &cx, &cy);
        int   pressedOffset = (state & CDIS_SELECTED) ? 1 : 0;
        // Calculate the image position without the TBSTYLE_LIST toolbar style.
        int xImage = (rc.right + rc.left - cx) / 2 + pressedOffset;
        int yImage = (rc.bottom + rc.top - cy) / 2;

        // Draw the button image.
        TBBUTTON tbb;
        ZeroMemory(&tbb, sizeof(tbb));
        int button = static_cast<int>(SendMessage(m_quickbar, TB_COMMANDTOINDEX, item, 0));
        WPARAM wparam = static_cast<WPARAM>(button);
        LPARAM lparam = reinterpret_cast<LPARAM>(&tbb);
        SendMessage(m_quickbar, TB_GETBUTTON, wparam, lparam);
        if (((tbb.fsState & TBSTATE_CHECKED) != 0) || ((state & CDIS_HOT) != 0))
        {
            COLORREF clr = theme.tabHover;
            clr = GDIHelpers::Lighter(clr, 1.5);
            HBRUSH hoverBrush = CreateSolidBrush(isDark ? clr : theme.itemHover);
            FillRect(hdc, &rc, hoverBrush);
            DeleteObject(hoverBrush);
        }
        else
        {
            if (IsWindowVisible(m_custToolTip))
                m_custToolTip.HideTip();
        }
        
        ImageList_Draw(imageList, isDark ? tbb.iBitmap + 10 : tbb.iBitmap, hdc, xImage, yImage, ILD_TRANSPARENT);

        return CDRF_SKIPDEFAULT;
    }

    return 0;
}

//void CMainWindow::ShowRecents()
//{
//    auto count = m_recents.size();
//
//    if (count > 0)
//    {
//        auto hMenu = CreatePopupMenu();
//        for (int i = 0; i < count; ++i)
//        {
//            std::wstring folderName = m_recents[i];
//            AppendMenu(hMenu, MF_STRING, i + 1, folderName.c_str());
//            if (PathIsDirectory(folderName.c_str()))
//            {
//                BOOL isDark = CTheme::Instance().IsDarkTheme();
//                HBITMAP bitmap = LoadBitmap(g_hRes, MAKEINTRESOURCE(isDark ? IDB_FOLDER_DARK : IDB_FOLDER_LIGHT));
//
//                SetMenuItemBitmaps(hMenu, i, MF_BYPOSITION, bitmap, NULL);
//            }
//        }
//
//        POINT pt = { m_allRects.minimize.left - getQuickbarSize(m_quickbar).cx, m_allRects.minimize.bottom};
//        ClientToScreen(*this, &pt);
//
//        int idx = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, pt.x, pt.y, *this, nullptr);
//        if (idx > 0)
//        {
//            if (PathIsDirectory(m_recents[idx - 1].c_str()))
//            {
//                m_fileTree.SetPath(m_recents[idx - 1]);
//                m_titleText = m_recents[idx - 1];
//                SendMessage(*this, WM_SETTEXT, 0, 0);
//                ShowFileTree(true);
//            }
//            else
//                OpenFile(m_recents[idx - 1].c_str(), OpenFlags::AskToCreateIfMissing);
//        }
//        DestroyMenu(hMenu);
//    }
//}