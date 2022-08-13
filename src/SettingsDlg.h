// This file is part of BowPad.
//
// Copyright (C) 2013, 2016, 2021 - Stefan Kueng
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
#pragma once

#include "MainWindow.h"
#include "main.h"
#include "BaseDialog.h"
#include "Theme.h"
#include "ColorButton.h"
#include "LexStyles.h"
#include "SimpleIni.h"
#include "UnicodeUtils.h"
#include "ResString.h"
#include <windowsx.h>
#include <vector>
#include <IniSettings.h>

COLORREF Rgba(COLORREF c, BYTE alpha = 255)
{
    return (c | (static_cast<DWORD>(alpha) << 24));
}

class CSettingsDlg : public CDialog 
{
public:
    CSettingsDlg(HWND hParent)
        : m_hParent(hParent)
        , m_hForm(0)
        , m_formWidth(0)
        , m_pMainWindow(nullptr)
    {
    }
    ~CSettingsDlg() = default;

protected:

    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
            HANDLE_MSG(hwndDlg, WM_INITDIALOG, OnInitDialog);
            HANDLE_MSG(hwndDlg, WM_COMMAND, OnCommand);
            HANDLE_MSG(hwndDlg, WM_SIZE, OnSize);
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
                return 0;
            }
            default:
                break;
        }

        return FALSE;
    }

private:
    //static BOOL SD_OnInitDialog(HWND hwnd, HWND /*hwndFocus*/, LPARAM /*lParam*/)
    //{
    //    RECT rc = {};
    //    GetClientRect(hwnd, &rc);

    //    const SIZE sz = {rc.right - rc.left, rc.bottom - rc.top};

    //    SCROLLINFO si = {};
    //    si.cbSize     = sizeof(SCROLLINFO);
    //    si.fMask      = SIF_PAGE | SIF_POS | SIF_RANGE;
    //    si.nPos = si.nMin = 1;

    //    si.nMax  = sz.cx;
    //    si.nPage = sz.cx;
    //    SetScrollInfo(hwnd, SB_HORZ, &si, FALSE);

    //    si.nMax  = sz.cy;
    //    si.nPage = sz.cy;
    //    SetScrollInfo(hwnd, SB_VERT, &si, FALSE);
    //    SCROLLBARINFO sbi;
    //    GetScrollBarInfo(hwnd, OBJID_VSCROLL, &sbi);

    //    CTheme::Instance().SetThemeForDialog(hwnd, CTheme::Instance().IsDarkTheme());
    //    return FALSE;
    //}

    //static void SD_ScrollClient(HWND hwnd, int bar, int pos)
    //{
    //    static int s_prevx = 1;
    //    static int s_prevy = 1;

    //    int cx = 0;
    //    int cy = 0;

    //    int& delta = (bar == SB_HORZ ? cx : cy);
    //    int& prev  = (bar == SB_HORZ ? s_prevx : s_prevy);

    //    delta = prev - pos;
    //    prev  = pos;

    //    if (cx || cy)
    //    {
    //        ScrollWindow(hwnd, cx, cy, nullptr, nullptr);
    //    }
    //}

    //static void SD_OnSize(HWND hwnd, UINT state, int cx, int cy)
    //{
    //    if (state != SIZE_RESTORED && state != SIZE_MAXIMIZED)
    //        return;

    //    SCROLLINFO si = {};
    //    si.cbSize     = sizeof(SCROLLINFO);

    //    const int bar[]  = {SB_HORZ, SB_VERT};
    //    const int page[] = {cx, cy};
    //    for (size_t i = 0; i < ARRAYSIZE(bar); ++i)
    //    {
    //        si.fMask = SIF_PAGE;
    //        si.nPage = page[i];
    //        SetScrollInfo(hwnd, bar[i], &si, TRUE);

    //        si.fMask = SIF_RANGE | SIF_POS;
    //        GetScrollInfo(hwnd, bar[i], &si);

    //        const int maxScrollPos = si.nMax - (page[i] - 1);

    //        // Scroll client only if scroll bar is visible and window's
    //        // content is fully scrolled toward right and/or bottom side.
    //        // Also, update window's content on maximize.
    //        const bool needToScroll =
    //            (si.nPos != si.nMin && si.nPos == maxScrollPos) ||
    //            (state == SIZE_MAXIMIZED);

    //        if (needToScroll)
    //        {
    //            SD_ScrollClient(hwnd, bar[i], si.nPos);
    //        }
    //    }
    //}

    //static void SD_OnScroll(HWND hwnd, int bar, UINT code)
    //{
    //    SCROLLINFO si = {};
    //    si.cbSize     = sizeof(SCROLLINFO);
    //    si.fMask      = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_TRACKPOS;
    //    GetScrollInfo(hwnd, bar, &si);

    //    const int minPos = si.nMin;
    //    const int maxPos = si.nMax - (si.nPage - 1);

    //    int scrollPos = -1;

    //    switch (code)
    //    {
    //        case SB_LINEUP /*SB_LINELEFT*/:
    //            scrollPos = max(si.nPos - 1, minPos);
    //            break;

    //        case SB_LINEDOWN /*SB_LINERIGHT*/:
    //            scrollPos = min(si.nPos + 1, maxPos);
    //            break;

    //        case SB_PAGEUP /*SB_PAGELEFT*/:
    //            scrollPos = max(si.nPos - (int)si.nPage, minPos);
    //            break;

    //        case SB_PAGEDOWN /*SB_PAGERIGHT*/:
    //            scrollPos = min(si.nPos + (int)si.nPage, maxPos);
    //            break;

    //        case SB_THUMBPOSITION:
    //            break;

    //        case SB_THUMBTRACK:
    //            scrollPos = si.nTrackPos;
    //            break;

    //        case SB_TOP /*SB_LEFT*/:
    //            scrollPos = minPos;
    //            break;

    //        case SB_BOTTOM /*SB_RIGHT*/:
    //            scrollPos = maxPos;
    //            break;

    //        case SB_ENDSCROLL:
    //            break;
    //    }

    //    if (scrollPos == -1)
    //        return;

    //    SetScrollPos(hwnd, bar, scrollPos, TRUE);
    //    SD_ScrollClient(hwnd, bar, scrollPos);
    //}

    //static INT_PTR CALLBACK SD_DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    //{
    //    switch (uMsg)
    //    {
    //        case WM_INITDIALOG:
    //            SD_OnInitDialog(hwndDlg, (HWND)wParam, lParam);
    //            break;
    //        case WM_SIZE:
    //            SD_OnSize(hwndDlg, (UINT)wParam, LOWORD(lParam), HIWORD(lParam));
    //            break;
    //        case WM_HSCROLL:
    //            SD_OnScroll(hwndDlg, SB_HORZ, LOWORD(wParam));
    //            break;
    //        case WM_MOUSEWHEEL:
    //            SD_OnScroll(hwndDlg, SB_VERT, GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? SB_LINEDOWN : SB_LINEUP);
    //            break;
    //        case WM_VSCROLL:
    //            SD_OnScroll(hwndDlg, SB_VERT, LOWORD(wParam));
    //            break;
    //        case WM_COMMAND:
    //            return SendMessage(GetParent(hwndDlg), uMsg, wParam, lParam);
    //        default:
    //            return FALSE;
    //    }

    //    return TRUE;
    //}

    void OnSize(HWND hwnd, UINT /*state*/, int cx, int cy)
    {
        int btnWidth = 60, btnHeight = 22;
        SetWindowPos(GetDlgItem(hwnd, IDOK), HWND_TOP, 24, 8, btnWidth, btnHeight, SWP_SHOWWINDOW);
        SetWindowPos(GetDlgItem(hwnd, IDCANCEL), HWND_TOP, cx - btnWidth - 24, 8, btnWidth, btnHeight, SWP_SHOWWINDOW);
        SetWindowPos(m_hForm, HWND_TOP, (cx - m_formWidth) / 2, 36, m_formWidth, cy - 37, SWP_SHOWWINDOW);
    }

    BOOL OnInitDialog(HWND hwnd, HWND /*hwndFocus*/, LPARAM /*lParam*/)
    {
        m_pMainWindow = reinterpret_cast<CMainWindow*>(GetWindowLongPtr(m_hParent, GWLP_USERDATA));
        //m_hForm = CreateDialog(g_hRes, MAKEINTRESOURCE(IDD_SETTINGS_FORM), hwnd, SD_DialogProc);
        m_hForm = *this;
        RECT rc         = {};
        //GetWindowRect(m_hForm, &rc);
        //m_formWidth = rc.right - rc.left;

        GetClientRect(hwnd, &rc);

        int cx = rc.right - rc.left;
        int cy = rc.bottom - rc.top;
        //OnSize(hwnd, 0, cx, cy);
        CTheme::Instance().SetThemeForDialog(hwnd, CTheme::Instance().IsDarkTheme());

        //InitializeGeneralSetting();
        //InitializeDefaultEncoding();
        InitializeLexerConfiguration();

        GetWindowRect(m_pMainWindow->m_hwnd, &rc);
        int x = rc.left + (rc.right - rc.left - cx) / 2;
        int y = rc.top + CTheme::CurrentTheme().tabHeight + CTheme::CurrentTheme().titleHeight;
        SetWindowPos(*this, 0, x, y, cx, cy, SWP_SHOWWINDOW);
        return FALSE;
    }
   
    void OnCommand(HWND /*hwnd*/, int id, HWND /*hwndCtl*/, UINT msg)
    {
        //CMainWindow* pMainWindow = reinterpret_cast<CMainWindow*>(GetWindowLongPtr(m_hParent, GWLP_USERDATA));
        switch (id)
        {
            case IDOK:
                if (msg == BN_CLICKED)
                {
                    CLexStyles::Instance().SaveUserData();
                    
                    if (HasActiveDocument())
                    {
                        const auto& doc = GetActiveDocument();
                        m_pMainWindow->m_editor.SetupLexerForLang(doc.GetLanguage());
                    }

                    //SaveGeneralSetting();
                    //SaveDefaultEncoding();
                    EndDialog(*this, 0);
                }
                break;
            case IDCANCEL:
                if (msg == BN_CLICKED)
                {
                    CLexStyles::Instance().ResetUserData();

                    if (HasActiveDocument())
                    {
                        const auto& doc = GetActiveDocument();
                        m_pMainWindow->m_editor.SetupLexerForLang(doc.GetLanguage());
                    }
                    EndDialog(*this, 0);
                }
                break;
            case IDC_LANGCOMBO:
            {
                if (msg == CBN_SELCHANGE)
                {
                    auto hLangCombo = GetDlgItem(m_hForm, IDC_LANGCOMBO);
                    int  langSel    = ComboBox_GetCurSel(hLangCombo);
                    auto languages  = CLexStyles::Instance().GetLanguages();
                    if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
                    {
                        std::wstring currentLang = languages[langSel];
                        const auto&  lexData     = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(currentLang));
                        auto         hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
                        ComboBox_ResetContent(hStyleCombo);
                        for (const auto& [lexId, styleData] : lexData.styles)
                        {
                            int styleSel = ComboBox_AddString(hStyleCombo, styleData.name.c_str());
                            ComboBox_SetItemData(hStyleCombo, styleSel, lexId);
                        }

                        // pMainWindow->m_editor.Scintilla();
                        int style = static_cast<int>(Scintilla().StyleAt(Scintilla().CurrentPos()));
                        SelectStyle(style);
                        std::wstring exts = CLexStyles::Instance().GetUserExtensionsForLanguage(currentLang);
                        SetDlgItemText(m_hForm, IDC_EXTENSIONS, exts.c_str());
                        DialogEnableWindow(IDC_EXTENSIONS, true);
                        CheckDlgButton(m_hForm, IDC_HIDE, CLexStyles::Instance().IsLanguageHidden(currentLang) ? BST_CHECKED : BST_UNCHECKED);
                    }
                    else
                    {
                        SetDlgItemText(m_hForm, IDC_EXTENSIONS, L"");
                        DialogEnableWindow(IDC_EXTENSIONS, false);
                    }
                }
            }
            break;
            case IDC_STYLECOMBO:
            {
                if (msg == CBN_SELCHANGE)
                {
                    auto hLangCombo = GetDlgItem(m_hForm, IDC_LANGCOMBO);
                    int  langSel    = ComboBox_GetCurSel(hLangCombo);
                    auto languages  = CLexStyles::Instance().GetLanguages();
                    if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
                    {
                        auto        currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                        auto        hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
                        int         styleSel    = ComboBox_GetCurSel(hStyleCombo);
                        int         styleKey    = static_cast<int>(ComboBox_GetItemData(hStyleCombo, styleSel));
                        const auto& lexData     = CLexStyles::Instance().GetLexerDataForLang(currentLang);

                        auto foundStyle = lexData.styles.find(styleKey);
                        if (foundStyle != lexData.styles.end())
                        {
                            ComboBox_SelectString(GetDlgItem(m_hForm, IDC_FONTCOMBO), -1, foundStyle->second.fontName.c_str());
                            std::wstring fsize = std::to_wstring(foundStyle->second.fontSize);
                            if (foundStyle->second.fontSize == 0)
                                fsize.clear();
                            ComboBox_SelectString(GetDlgItem(m_hForm, IDC_FONTSIZECOMBO), -1, fsize.c_str());
                            Button_SetCheck(GetDlgItem(m_hForm, IDC_BOLDCHECK),
                                            (foundStyle->second.fontStyle & Fontstyle_Bold) ? BST_CHECKED : BST_UNCHECKED);
                            Button_SetCheck(GetDlgItem(m_hForm, IDC_ITALICCHECK),
                                            (foundStyle->second.fontStyle & Fontstyle_Italic) ? BST_CHECKED : BST_UNCHECKED);
                            Button_SetCheck(GetDlgItem(m_hForm, IDC_UNDERLINECHECK),
                                            (foundStyle->second.fontStyle & Fontstyle_Underlined) ? BST_CHECKED : BST_UNCHECKED);

                            // REVIEW:
                            // If the user changes a style from color A to color B,
                            // they would see B instantly in the editor if a document of that
                            // language/style is open.
                            // However without the code below if they leave this current style
                            // and return to it, the user will see the original uncommited
                            // color A again and not B. This is confusing to the user.
                            // So we try to use the active style data not the original style data,
                            // if possible.
                            COLORREF fgc = foundStyle->second.foregroundColor;
                            COLORREF bgc = foundStyle->second.backgroundColor;
                            m_fgColor.SetColor(fgc);
                            m_bkColor.SetColor(bgc);
                        }
                    }
                }
            }
            break;
            case IDC_FG_BTN:
            case IDC_BK_BTN:
            case IDC_FONTCOMBO:
            case IDC_FONTSIZECOMBO:
            case IDC_BOLDCHECK:
            case IDC_ITALICCHECK:
            case IDC_UNDERLINECHECK:
            case IDC_EXTENSIONS:
            case IDC_HIDE:
            {
                auto hLangCombo = GetDlgItem(m_hForm, IDC_LANGCOMBO);
                int  langSel    = ComboBox_GetCurSel(hLangCombo);
                auto languages  = CLexStyles::Instance().GetLanguages();
                if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
                {
                    auto        currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                    const auto& lexData     = CLexStyles::Instance().GetLexerDataForLang(currentLang);
                    const auto  lexID       = lexData.id;
                    const auto  hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
                    const int   styleSel    = ComboBox_GetCurSel(hStyleCombo);
                    const int   styleKey    = static_cast<int>(ComboBox_GetItemData(hStyleCombo, styleSel));
                    bool        updateView  = false;
                    if (HasActiveDocument())
                    {
                        const auto& doc = GetActiveDocument();
                        if (doc.GetLanguage().compare(currentLang) == 0)
                            updateView = true;
                    }
                    switch (id)
                    {
                        case IDC_HIDE:
                        {
                            CLexStyles::Instance().SetLanguageHidden(languages[langSel], IsDlgButtonChecked(m_hForm, IDC_HIDE) != 0);
                            // NotifyPlugins(L"cmdStyleConfigurator", 1);
                            break;
                        }
                        case IDC_FG_BTN:
                        {
                            auto fgcolor = m_fgColor.GetColor();
                            // When colors are applied by SetupLexer GetThemeColor is applied,
                            // so don't do it again here when storing the color.
                            CLexStyles::Instance().SetUserForeground(lexID, styleKey, fgcolor);
                            if (updateView)
                                Scintilla().StyleSetFore(styleKey, CTheme::Instance().GetThemeColor(fgcolor));
                            break;
                        }
                        case IDC_BK_BTN:
                        {
                            auto bgcolor = m_bkColor.GetColor();
                            CLexStyles::Instance().SetUserBackground(lexID, styleKey, bgcolor);
                            if (updateView)
                                Scintilla().StyleSetBack(styleKey, CTheme::Instance().GetThemeColor(bgcolor));
                            break;
                        }
                        case IDC_FONTCOMBO:
                        {
                            if (msg == CBN_SELCHANGE)
                            {
                                int fontSel = ComboBox_GetCurSel(GetDlgItem(m_hForm, IDC_FONTCOMBO));
                                if (fontSel >= 0 && fontSel < static_cast<int>(m_fonts.size()))
                                {
                                    std::wstring font = m_fonts[fontSel];
                                    CLexStyles::Instance().SetUserFont(lexID, styleKey, font);
                                    if (updateView)
                                    {
                                        if (font.empty())
                                        {
                                            HFONT hFont = CreateFont(0, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, L"Consolas");
                                            if (hFont)
                                            {
                                                DeleteObject(hFont);
                                                Scintilla().StyleSetFont(styleKey, "Consolas");
                                            }
                                            else
                                                Scintilla().StyleSetFont(styleKey, "Courier New");
                                        }
                                        else
                                        {
                                            Scintilla().StyleSetFont(styleKey, CUnicodeUtils::StdGetUTF8(font).c_str());
                                        }
                                    }
                                }
                            }
                        }
                        break;
                        case IDC_FONTSIZECOMBO:
                        {
                            if (msg == CBN_SELCHANGE)
                            {
                                auto hFontSizeCombo = GetDlgItem(m_hForm, IDC_FONTSIZECOMBO);
                                int  fontSizeSel    = ComboBox_GetCurSel(hFontSizeCombo);
                                if (fontSizeSel >= 0)
                                {
                                    int fontSize = static_cast<int>(ComboBox_GetItemData(hFontSizeCombo, fontSizeSel));
                                    CLexStyles::Instance().SetUserFontSize(lexID, styleKey, fontSize);
                                    if (updateView)
                                    {
                                        if (fontSize > 0)
                                            Scintilla().StyleSetSize(styleKey, fontSize);
                                        else
                                            Scintilla().StyleSetSize(styleKey, 10);
                                    }
                                }
                            }
                        }
                        break;
                        case IDC_BOLDCHECK:
                        case IDC_ITALICCHECK:
                        case IDC_UNDERLINECHECK:
                        {
                            ::FontStyle fontStyle = Fontstyle_Normal;
                            if (Button_GetCheck(GetDlgItem(m_hForm, IDC_BOLDCHECK)) == BST_CHECKED)
                                fontStyle = static_cast<::FontStyle>(fontStyle | Fontstyle_Bold);
                            if (Button_GetCheck(GetDlgItem(m_hForm, IDC_ITALICCHECK)) == BST_CHECKED)
                                fontStyle = static_cast<::FontStyle>(fontStyle | Fontstyle_Italic);
                            if (Button_GetCheck(GetDlgItem(m_hForm, IDC_UNDERLINECHECK)) == BST_CHECKED)
                                fontStyle = static_cast<::FontStyle>(fontStyle | Fontstyle_Underlined);
                            CLexStyles::Instance().SetUserFontStyle(lexData.id, styleKey, fontStyle);
                            if (updateView)
                            {
                                Scintilla().StyleSetBold(styleKey, (fontStyle & Fontstyle_Bold) ? 1 : 0);
                                Scintilla().StyleSetItalic(styleKey, (fontStyle & Fontstyle_Italic) ? 1 : 0);
                                Scintilla().StyleSetUnderline(styleKey, (fontStyle & Fontstyle_Underlined) ? 1 : 0);
                            }
                        }
                        break;
                        case IDC_EXTENSIONS:
                        {
                            if (msg == EN_KILLFOCUS)
                            {
                                auto extText = GetDlgItemText(IDC_EXTENSIONS);
                                CLexStyles::Instance().SetUserExt(extText.get(), currentLang);
                            }
                        }
                        break;
                    }
                }
            }
            break;
        }
    }

    static int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*fontType*/, LPARAM lParam)
    {
        // only show the default and ansi fonts
        if (lpelfe->lfCharSet == DEFAULT_CHARSET || lpelfe->lfCharSet == ANSI_CHARSET)
        {
            // filter out fonts that start with a '@'
            if (lpelfe->lfFaceName[0] != '@')
            {
                auto& tempFonts = *reinterpret_cast<std::set<std::wstring>*>(lParam);
                tempFonts.insert(lpelfe->lfFaceName);
            }
        }
        return TRUE;
    }

    void InitializeLexerConfiguration()
    {
        auto languages  = CLexStyles::Instance().GetLanguages();
        auto hLangCombo = GetDlgItem(m_hForm, IDC_LANGCOMBO);
        for (const auto& langName : languages)
            ComboBox_AddString(hLangCombo, langName.c_str());

        std::set<std::wstring> tempFonts;
        // Populates tempFonts.
        {
            HDC     dc       = GetWindowDC(m_hForm);
            LOGFONT lf       = {0};
            lf.lfCharSet     = DEFAULT_CHARSET;
            lf.lfFaceName[0] = 0;
            EnumFontFamiliesEx(dc, &lf, EnumFontFamExProc, reinterpret_cast<LPARAM>(&tempFonts), 0);
            ReleaseDC(m_hForm, dc);
        }
        m_fonts.push_back(L"");
        for (const auto& fontName : tempFonts)
            m_fonts.push_back(fontName);
        tempFonts.clear(); // We don't need them any more.
        auto hFontCombo = GetDlgItem(m_hForm, IDC_FONTCOMBO);
        for (const auto& fontName : m_fonts)
            ComboBox_AddString(hFontCombo, fontName.c_str());

        auto hFontSizeCombo = GetDlgItem(m_hForm, IDC_FONTSIZECOMBO);
        int  index          = ComboBox_AddString(hFontSizeCombo, L"");
        ComboBox_SetItemData(hFontSizeCombo, 0, 0);
        static constexpr int fontSizes[] = {5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28};
        for (auto fontSize : fontSizes)
        {
            std::wstring s = std::to_wstring(fontSize);
            index          = static_cast<int>(ComboBox_AddString(hFontSizeCombo, s.c_str()));
            ComboBox_SetItemData(hFontSizeCombo, index, fontSize);
        }
        m_fgColor.ConvertToColorButton(m_hForm, IDC_FG_BTN);
        m_bkColor.ConvertToColorButton(m_hForm, IDC_BK_BTN);

        ResString sExtTooltip(g_hRes, IDS_EXTENSIONTOOLTIP);

        AddToolTip(IDC_EXTENSIONS, sExtTooltip);

        // Select the current language.
        CMainWindow* pMainWindow = reinterpret_cast<CMainWindow*>(GetWindowLongPtr(m_hParent, GWLP_USERDATA));

        auto tid = pMainWindow->GetCurrentTabId();
        if (pMainWindow->m_docManager.HasDocumentID(tid))
        {
            const auto& doc = pMainWindow->m_docManager.GetDocumentFromID(pMainWindow->GetCurrentTabId());
            ComboBox_SelectString(hLangCombo, -1, CUnicodeUtils::StdGetUnicode(doc.GetLanguage()).c_str());
        }
        OnCommand(*this, IDC_LANGCOMBO, 0, CBN_SELCHANGE);

        int style = static_cast<int>(pMainWindow->m_editor.Scintilla().StyleAt(pMainWindow->m_editor.Scintilla().CurrentPos()));
        SelectStyle(style);
    }

    void SelectStyle(int style)
    {
        auto hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
        int  styleCount  = ComboBox_GetCount(hStyleCombo);
        bool selected    = false;
        for (int i = 0; i < styleCount; ++i)
        {
            int styleCombo = static_cast<int>(ComboBox_GetItemData(hStyleCombo, i));
            if (style == styleCombo)
            {
                ComboBox_SetCurSel(hStyleCombo, i);
                OnCommand(*this, IDC_STYLECOMBO, 0, CBN_SELCHANGE);
                selected = true;
                break;
            }
        }
        if (!selected)
            ComboBox_SetCurSel(hStyleCombo, 0);
    }

    BOOL HasActiveDocument()
    {
        auto tid = m_pMainWindow->GetCurrentTabId();
        return m_pMainWindow->m_docManager.HasDocumentID(tid);
    }

    const CDocument& GetActiveDocument()
    {
        auto tid = m_pMainWindow->GetCurrentTabId();
        return m_pMainWindow->m_docManager.GetDocumentFromID(tid);
    }

    Scintilla::ScintillaCall& Scintilla()
    {
        return m_pMainWindow->m_editor.Scintilla();
    }

    //void InitializeDefaultEncoding()
    //{
    //    UINT      cp         = static_cast<UINT>(CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnew", GetACP()));
    //    bool      bom        = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingnewbom", 0) != 0;
    //    bool      preferUtf8 = CIniSettings::Instance().GetInt64(L"Defaults", L"encodingutf8overansi", 0) != 0;
    //    EOLFormat eol        = static_cast<EOLFormat>(CIniSettings::Instance().GetInt64(L"Defaults", L"lineendingnew", static_cast<int>(EOLFormat::Win_Format)));

    //    if (cp == GetACP())
    //        CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_ANSI);
    //    else if (cp == CP_UTF8)
    //    {
    //        if (bom)
    //            CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_UTF8BOM);
    //        else
    //            CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_UTF8);
    //    }
    //    else if (cp == 1200)
    //        CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_UTF16LE);
    //    else if (cp == 1201)
    //        CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_UTF16BE);
    //    else if (cp == 12000)
    //        CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_UTF32LE);
    //    else if (cp == 12001)
    //        CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_UTF32BE);
    //    else
    //        CheckRadioButton(m_hForm, IDC_R_ANSI, IDC_R_UTF32BE, IDC_R_ANSI);

    //    CheckDlgButton(m_hForm, IDC_LOADASUTF8, preferUtf8 ? BST_CHECKED : BST_UNCHECKED);

    //    switch (eol)
    //    {
    //        default:
    //        case EOLFormat::Win_Format:
    //            CheckRadioButton(m_hForm, IDC_CRLF_RADIO, IDC_CR_RADIO, IDC_CRLF_RADIO);
    //            break;
    //        case EOLFormat::Mac_Format:
    //            CheckRadioButton(m_hForm, IDC_CRLF_RADIO, IDC_CR_RADIO, IDC_CR_RADIO);
    //            break;
    //        case EOLFormat::Unix_Format:
    //            CheckRadioButton(m_hForm, IDC_CRLF_RADIO, IDC_CR_RADIO, IDC_LF_RADIO);
    //            break;
    //    }
    //}

    //void SaveDefaultEncoding()
    //{
    //    UINT cp         = GetACP();
    //    bool bom        = false;
    //    bool preferUtf8 = IsDlgButtonChecked(m_hForm, IDC_LOADASUTF8) == BST_CHECKED;

    //    if (IsDlgButtonChecked(m_hForm, IDC_R_ANSI))
    //        cp = GetACP();
    //    else if (IsDlgButtonChecked(m_hForm, IDC_R_UTF8))
    //        cp = CP_UTF8;
    //    else if (IsDlgButtonChecked(m_hForm, IDC_R_UTF8BOM))
    //    {
    //        cp  = CP_UTF8;
    //        bom = true;
    //    }
    //    else if (IsDlgButtonChecked(m_hForm, IDC_R_UTF16LE))
    //        cp = 1200;
    //    else if (IsDlgButtonChecked(m_hForm, IDC_R_UTF16BE))
    //        cp = 1201;
    //    else if (IsDlgButtonChecked(m_hForm, IDC_R_UTF32LE))
    //        cp = 12000;
    //    else if (IsDlgButtonChecked(m_hForm, IDC_R_UTF32BE))
    //        cp = 12001;

    //    if (IsDlgButtonChecked(m_hForm, IDC_CRLF_RADIO))
    //        CIniSettings::Instance().SetInt64(L"Defaults", L"lineendingnew", static_cast<int>(EOLFormat::Win_Format));
    //    if (IsDlgButtonChecked(m_hForm, IDC_CR_RADIO))
    //        CIniSettings::Instance().SetInt64(L"Defaults", L"lineendingnew", static_cast<int>(EOLFormat::Mac_Format));
    //    if (IsDlgButtonChecked(m_hForm, IDC_LF_RADIO))
    //        CIniSettings::Instance().SetInt64(L"Defaults", L"lineendingnew", static_cast<int>(EOLFormat::Unix_Format));

    //    CIniSettings::Instance().SetInt64(L"Defaults", L"encodingnew", cp);
    //    CIniSettings::Instance().SetInt64(L"Defaults", L"encodingnewbom", bom);
    //    CIniSettings::Instance().SetInt64(L"Defaults", L"encodingutf8overansi", preferUtf8);
    //}

    //void InitializeGeneralSetting()
    //{
    //    bool d2d         = CIniSettings::Instance().GetInt64(L"View", L"d2d", 0) != 0;
    //    bool matchCase = CIniSettings::Instance().GetInt64(L"View", L"autocompleteMatchCase", 0) != 0;
    //    bool frameLine   = CIniSettings::Instance().GetInt64(L"View", L"caretlineframe", 0) != 0;

    //    CheckDlgButton(m_hForm, IDC_HIDE2, frameLine ? BST_CHECKED : BST_UNCHECKED);
    //    CheckDlgButton(m_hForm, IDC_HIDE3, d2d ? BST_CHECKED : BST_UNCHECKED);
    //    CheckDlgButton(m_hForm, IDC_HIDE4, matchCase ? BST_CHECKED : BST_UNCHECKED);

    //    int ve = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"verticaledge", 0));
    //    std::wstring ves = CStringUtils::Format(L"%Id", ve);
    //    SetDlgItemText(m_hForm, IDC_MARGIN, ves.c_str());
    //    Scintilla().SetEdgeColumn(ve);
    //    Scintilla().SetEdgeMode(ve ? Scintilla::EdgeVisualStyle::Line : Scintilla::EdgeVisualStyle::None);

    //    int          val   = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"tabwidth", 4));
    //    std::wstring sLine = CStringUtils::Format(L"%Id", val);
    //    SetDlgItemText(m_hForm, IDC_TABWIDTH, sLine.c_str());
    //}

    //void SaveGeneralSetting()
    //{
    //    bool frameLine = IsDlgButtonChecked(m_hForm, IDC_HIDE2) == BST_CHECKED;
    //    bool d2d = IsDlgButtonChecked(m_hForm, IDC_HIDE3) == BST_CHECKED;
    //    bool matchCase = IsDlgButtonChecked(m_hForm, IDC_HIDE4) == BST_CHECKED;

    //    CIniSettings::Instance().SetInt64(L"View", L"d2d", d2d);
    //    CIniSettings::Instance().SetInt64(L"View", L"autocompleteMatchCase", matchCase);
    //    CIniSettings::Instance().SetInt64(L"View", L"caretlineframe", frameLine);

    //    CTheme&  theme      = CTheme::Instance();
    //    COLORREF themeColor = theme.GetThemeColor(RGB(0, 0, 0), true);

    //    if (frameLine)
    //    {
    //        Scintilla().SetCaretLineFrame(1);
    //        Scintilla().SetElementColour(Scintilla::Element::CaretLineBack, Rgba(themeColor, 80));
    //        Scintilla().SetCaretLineLayer(Scintilla::Layer::UnderText);
    //    }
    //    else
    //    {
    //        if (theme.IsDarkTheme())
    //        {
    //            Scintilla().SetElementColour(Scintilla::Element::CaretLineBack, themeColor);
    //        }
    //        else
    //        {
    //            Scintilla().SetElementColour(Scintilla::Element::CaretLineBack, Rgba(themeColor, 75));
    //        }
    //    }
    //    WCHAR sLine[8];
    //    ::GetDlgItemText(m_hForm, IDC_MARGIN, sLine, 4);
    //    int ve = _wtol(sLine);
    //    CIniSettings::Instance().SetInt64(L"View", L"verticaledge", ve);
    //    Scintilla().SetEdgeColumn(ve);
    //    Scintilla().SetEdgeMode(ve ? Scintilla::EdgeVisualStyle::Line : Scintilla::EdgeVisualStyle::None);

    //     ::GetDlgItemText(m_hForm, IDC_TABWIDTH, sLine, 4);
    //    int w = _wtol(sLine);
    //    Scintilla().SetTabWidth(w);
    //    CIniSettings::Instance().SetInt64(L"View", L"tabwidth", w);
    //    m_pMainWindow->UpdateStatusBar(false);
    //}

    std::vector<std::wstring> m_fonts;
    CColorButton              m_fgColor;
    CColorButton              m_bkColor;

    HWND m_hParent;
    HWND m_hForm;
    int  m_formWidth;
    CMainWindow* m_pMainWindow;
};
