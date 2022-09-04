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
        , m_hForm(nullptr)
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
        m_hForm = *this;
        RECT rc         = {};
        GetClientRect(hwnd, &rc);

        int cx = rc.right - rc.left;
        int cy = rc.bottom - rc.top;
        CTheme::Instance().SetThemeForDialog(hwnd, CTheme::Instance().IsDarkTheme());

        InitializeLexerConfiguration();

        GetWindowRect(m_pMainWindow->m_hwnd, &rc);
        int x = rc.left + (rc.right - rc.left - cx) / 2;
        int y = rc.top + CTheme::CurrentTheme().tabHeight + CTheme::CurrentTheme().titleHeight;
        SetWindowPos(*this, nullptr, x, y, cx, cy, SWP_SHOWWINDOW);
        return FALSE;
    }
   
    void OnCommand(HWND /*hwnd*/, int id, HWND /*hwndCtl*/, UINT msg)
    {
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
                            LexerFontStyle fontStyle = Fontstyle_Normal;
                            if (Button_GetCheck(GetDlgItem(m_hForm, IDC_BOLDCHECK)) == BST_CHECKED)
                                fontStyle = static_cast<LexerFontStyle>(fontStyle | Fontstyle_Bold);
                            if (Button_GetCheck(GetDlgItem(m_hForm, IDC_ITALICCHECK)) == BST_CHECKED)
                                fontStyle = static_cast<LexerFontStyle>(fontStyle | Fontstyle_Italic);
                            if (Button_GetCheck(GetDlgItem(m_hForm, IDC_UNDERLINECHECK)) == BST_CHECKED)
                                fontStyle = static_cast<LexerFontStyle>(fontStyle | Fontstyle_Underlined);
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
        OnCommand(*this, IDC_LANGCOMBO, nullptr, CBN_SELCHANGE);

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
                OnCommand(*this, IDC_STYLECOMBO, nullptr, CBN_SELCHANGE);
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

    std::vector<std::wstring> m_fonts;
    CColorButton              m_fgColor;
    CColorButton              m_bkColor;

    HWND m_hParent;
    HWND m_hForm;
    int  m_formWidth;
    CMainWindow* m_pMainWindow;
};
