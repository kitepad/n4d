// This file is part of BowPad.
//
// Copyright (C) 2014-2017, 2021 - Stefan Kueng
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
#include "CmdMisc.h"
#include "AppUtils.h"
#include "Theme.h"
#include "LexStyles.h"
#include "ResString.h"
#include "UnicodeUtils.h"
#include "CommandHandler.h"
#include <strsafe.h>
#include <Commdlg.h>

constexpr POINT PADDINGS = {16, 4};

CCmdToggleTheme::CCmdToggleTheme(void* obj)
    : ICommand(obj)
{
    int dark = static_cast<int>(GetInt64(DEFAULTS_SECTION,L"DarkTheme", 0));
    if (dark)
    {
        CTheme::Instance().SetDarkTheme(dark != 0);
    }
}

bool CCmdToggleTheme::Execute()
{
    if (!HasActiveDocument())
        return false;
    CTheme::Instance().SetDarkTheme(!CTheme::Instance().IsDarkTheme());

    return true;
}


bool CCmdConfigShortcuts::Execute()
{
    std::wstring userFile = CAppUtils::GetDataPath() + L"\\n4d.shortcuts";
    if (!PathFileExists(userFile.c_str()))
    {
        DWORD       resLen    = 0;
        const char* lpResLock = CAppUtils::GetResourceData(L"config", IDR_SHORTCUTS, resLen);
        if (lpResLock)
        {
            const char* lpStart = strstr(lpResLock, "#--");
            if (lpStart)
            {
                const char* lpEnd = strstr(lpStart + 3, "#--");
                if (lpEnd)
                {
                    HANDLE hFile = CreateFile(userFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hFile != INVALID_HANDLE_VALUE)
                    {
                        DWORD dwWritten = 0;
                        WriteFile(hFile, lpStart, static_cast<DWORD>(lpEnd - lpStart), &dwWritten, nullptr);
                        CloseHandle(hFile);
                    }
                }
            }
        }
    }
    return OpenFile(userFile.c_str(), 0) >= 0;
}

CCmdAutoBraces::CCmdAutoBraces(void* obj)
    : ICommand(obj)
{
}

bool CCmdAutoBraces::Execute()
{
    SetInt64(DEFAULTS_SECTION, L"autobrace", GetInt64(DEFAULTS_SECTION, L"AutoBrace") ? 0 : 1);
    return true;
}

CCmdViewFileTree::CCmdViewFileTree(void* obj)
    : ICommand(obj)
{
}

bool CCmdViewFileTree::Execute()
{
    ShowFileTree(!IsFileTreeShown());
    return true;
}

CCmdWriteProtect::CCmdWriteProtect(void* obj)
    : ICommand(obj)
{
}

bool CCmdWriteProtect::Execute()
{
    if (!HasActiveDocument())
        return false;

    auto& doc               = GetModActiveDocument();
    doc.m_bIsWriteProtected = !(doc.m_bIsWriteProtected || doc.m_bIsReadonly);
    if (!doc.m_bIsWriteProtected && doc.m_bIsReadonly)
        doc.m_bIsReadonly = false;
    Scintilla().SetReadOnly(doc.m_bIsWriteProtected);
    UpdateTab(GetActiveTabIndex());

    return true;
}


CCmdAutoComplete::CCmdAutoComplete(void* obj)
    : ICommand(obj)
{
}

bool CCmdAutoComplete::Execute()
{
    SetInt64(DEFAULTS_SECTION, L"AutoComplete", GetInt64(DEFAULTS_SECTION, L"AutoComplete") ? 0 : 1);
    return true;
}

CCmdSelectLexer::~CCmdSelectLexer()
{
    PostMessage(*this, WM_CLOSE, 0, 0);
}

bool CCmdSelectLexer::Execute()
{
    ClearFilterText();
    CDialogWithFilterableList::Execute();
    return true;
}

CCmdSelectEncoding::~CCmdSelectEncoding()
{
    PostMessage(*this, WM_CLOSE, 0, 0);
}

bool CCmdSelectEncoding::Execute()
{
    ClearFilterText();
    CDialogWithFilterableList::Execute();
    return true;
}

static std::vector<CListItem> codepages;

BOOL CALLBACK EnumerateCodePage(LPTSTR lpCodePageString)
{
    if (codepages.empty())
    {
        // insert the main encodings
        codepages.push_back(CListItem(GetACP(), false, L"ANSI"));
        codepages.push_back(CListItem(CP_UTF8, false, L"UTF-8"));
        codepages.push_back(CListItem(CP_UTF8, true, L"UTF-8 BOM"));
        codepages.push_back(CListItem(1200, true, L"UTF-16 Little Endian"));
        codepages.push_back(CListItem(1201, true, L"UTF-16 Big Endian"));
        codepages.push_back(CListItem(12000, true, L"UTF-32 Little Endian"));
        codepages.push_back(CListItem(12001, true, L"UTF-32 Big Endian"));
    }
    UINT codepage = _wtoi(lpCodePageString);
    switch (codepage)
    {
        case 1200:
        case 1201:
        case 12000:
        case 12001:
        case CP_UTF8:
            break;
        default:
        {
            CPINFOEX cpEx = {0};
            GetCPInfoEx(codepage, 0, &cpEx);
            if (cpEx.CodePageName[0])
            {
                std::wstring name = cpEx.CodePageName;
                name              = name.substr(name.find_first_of(' ') + 1);
                size_t pos        = name.find_first_of('(');
                if (pos != std::wstring::npos)
                    name.erase(pos, 1);
                pos = name.find_last_of(')');
                if (pos != std::wstring::npos)
                    name.erase(pos, 1);
                CStringUtils::trim(name);
                codepages.emplace_back(codepage, false, name);
            }
        }
        break;
    }
    return TRUE;
}

CCmdSelectLexer::CCmdSelectLexer(void* obj)
    : CDialogWithFilterableList(obj)
{
    const auto& lexers = CLexStyles::Instance().GetLanguages();
    for (std::wstring lexer : lexers)
    {
        m_allResults.push_back(CListItem(0, false, lexer));
    }

    std::sort(m_allResults.begin(), m_allResults.end(),
              [](CListItem a, CListItem b) -> bool {
                  return StrCmpLogicalW(a.text1.c_str(), b.text1.c_str()) < 0;
              });
}

void CCmdSelectLexer::OnOK()
{
    auto i = ListView_GetSelectionMark(m_hResults);
    if (i >= 0)
    {
        m_pCmd    = nullptr;
        auto& doc = GetModActiveDocument();

        auto lang = CUnicodeUtils::StdGetUTF8(m_results[i].text1.c_str());
        SetupLexerForLang(lang);
        CLexStyles::Instance().SetLangForPath(doc.m_path, lang);
        // set the language last, so that the OnLanguageChanged events happen last:
        // otherwise the SetLangForPath() invalidates the LanguageData pointers after
        // commands re-evaluated those!
        doc.SetLanguage(lang);
        UpdateStatusBar(true);
    }
}

UINT CCmdSelectLexer::GetFilterCUE()
{
    return IDS_SELECTLEXER_FILTERCUE;
}

CCmdSelectEncoding::CCmdSelectEncoding(void* obj)
    : CDialogWithFilterableList(obj)
{
    if (codepages.empty())
        EnumSystemCodePages(EnumerateCodePage, CP_INSTALLED);

    m_allResults = codepages;
}

UINT CCmdSelectEncoding::GetFilterCUE()
{
    return IDS_SELECTENCODING_FILTERCUE;
}

void CCmdSelectEncoding::OnOK()
{
    auto i = ListView_GetSelectionMark(m_hResults);
    if (i >= 0)
    {
        auto& doc            = GetModActiveDocument();
        m_pCmd               = nullptr;
        doc.m_encoding       = m_results[i].uintVal;
        doc.m_bHasBOM        = m_results[i].boolVal;
        doc.m_encodingSaving = -1;
        doc.m_bHasBOMSaving  = false;
        doc.m_bIsDirty       = true;
        doc.m_bNeedsSaving   = true;
        // the next two calls are only here to trigger SCN_SAVEPOINTLEFT/SCN_SAVEPOINTREACHED messages
        Scintilla().AddUndoAction(0, Scintilla::UndoFlags::MayCoalesce);
        Scintilla().Undo();
        UpdateStatusBar(true);
    }
}

CCmdSelectTab::~CCmdSelectTab()
{
    PostMessage(*this, WM_CLOSE, 0, 0);
}

bool CCmdSelectTab::Execute()
{
    ClearFilterText();
    auto count = GetTabCount();
    m_allResults.clear();
    for (int i = 0; i < count; i++)
    {
        auto& doc = GetDocumentFromID(GetDocIDFromTabIndex(i));
        std::wstring sTitle = L"";
        sTitle += doc.m_path.empty() ? GetTitleForTabIndex(i) : doc.m_path;
        if (doc.m_bNeedsSaving || doc.m_bIsDirty)
            sTitle = L"* " + sTitle;

        m_allResults.push_back(CListItem(0, false, sTitle));
    }
    CDialogWithFilterableList::Execute();
    return true;
}

CCmdSelectTab::CCmdSelectTab(void* obj)
    : CDialogWithFilterableList(obj)
{
}

void CCmdSelectTab::OnOK()
{
    auto i = ListView_GetSelectionMark(m_hResults);
    if (i >= 0)
    {
        m_pCmd = nullptr;
        TabActivateAt(i);
    }
}

UINT CCmdSelectTab::GetFilterCUE()
{
    return IDS_SELECTTAB_FILTERCUE;
}

CCmdConfigStyle::CCmdConfigStyle(void* obj)
    : ICommand(obj)
    , m_hForm(nullptr)
    , m_formWidth(0)
{
}
LRESULT CALLBACK CCmdConfigStyle::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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


void CCmdConfigStyle::OnSize(HWND hwnd, UINT /*state*/, int cx, int cy)
{
    int btnWidth = 60, btnHeight = 22;
    SetWindowPos(GetDlgItem(hwnd, IDOK), HWND_TOP, 24, 8, btnWidth, btnHeight, SWP_SHOWWINDOW);
    SetWindowPos(GetDlgItem(hwnd, IDCANCEL), HWND_TOP, cx - btnWidth - 24, 8, btnWidth, btnHeight, SWP_SHOWWINDOW);
    SetWindowPos(m_hForm, HWND_TOP, (cx - m_formWidth) / 2, 36, m_formWidth, cy - 37, SWP_SHOWWINDOW);
}

BOOL CCmdConfigStyle::OnInitDialog(HWND hwnd, HWND /*hwndFocus*/, LPARAM /*lParam*/)
{
    m_hForm = *this;
    RECT rc = {};
    GetClientRect(hwnd, &rc);

    int cx = rc.right - rc.left;
    int cy = rc.bottom - rc.top;
    CTheme::Instance().SetThemeForDialog(hwnd, CTheme::Instance().IsDarkTheme());

    InitializeLexerConfiguration();

    GetWindowRect(GetHwnd(), &rc);
    int x = rc.left + (rc.right - rc.left - cx) / 2;
    int y = rc.top + CTheme::CurrentTheme().tabHeight + CTheme::CurrentTheme().titleHeight;
    SetWindowPos(*this, nullptr, x, y, cx, cy, SWP_SHOWWINDOW);
    return FALSE;
}

void CCmdConfigStyle::OnCommand(HWND /*hwnd*/, int id, HWND /*hwndCtl*/, UINT msg)
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
                SetupLexerForLang(doc.GetLanguage());
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
                SetupLexerForLang(doc.GetLanguage());
            }
            EndDialog(*this, 0);
        }
        break;
    case IDC_LANGCOMBO:
    {
        if (msg == CBN_SELCHANGE)
        {
            auto hLangCombo = GetDlgItem(m_hForm, IDC_LANGCOMBO);
            int  langSel = ComboBox_GetCurSel(hLangCombo);
            auto languages = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
            {
                std::wstring currentLang = languages[langSel];
                const auto& lexData = CLexStyles::Instance().GetLexerDataForLang(CUnicodeUtils::StdGetUTF8(currentLang));
                auto         hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
                ComboBox_ResetContent(hStyleCombo);
                for (const auto& [lexId, styleData] : lexData.styles)
                {
                    int styleSel = ComboBox_AddString(hStyleCombo, styleData.name.c_str());
                    ComboBox_SetItemData(hStyleCombo, styleSel, lexId);
                }

                auto style = Scintilla().StyleAt(Scintilla().CurrentPos());
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
            int  langSel = ComboBox_GetCurSel(hLangCombo);
            auto languages = CLexStyles::Instance().GetLanguages();
            if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
            {
                auto        currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
                auto        hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
                int         styleSel = ComboBox_GetCurSel(hStyleCombo);
                int         styleKey = static_cast<int>(ComboBox_GetItemData(hStyleCombo, styleSel));
                const auto& lexData = CLexStyles::Instance().GetLexerDataForLang(currentLang);

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
        int  langSel = ComboBox_GetCurSel(hLangCombo);
        auto languages = CLexStyles::Instance().GetLanguages();
        if (langSel >= 0 && langSel < static_cast<int>(languages.size()))
        {
            auto        currentLang = CUnicodeUtils::StdGetUTF8(languages[langSel]);
            const auto& lexData = CLexStyles::Instance().GetLexerDataForLang(currentLang);
            const auto  lexID = lexData.id;
            const auto  hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
            const int   styleSel = ComboBox_GetCurSel(hStyleCombo);
            const int   styleKey = static_cast<int>(ComboBox_GetItemData(hStyleCombo, styleSel));
            bool        updateView = false;
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
                    int  fontSizeSel = ComboBox_GetCurSel(hFontSizeCombo);
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

void CCmdConfigStyle::InitializeLexerConfiguration()
{
    auto languages = CLexStyles::Instance().GetLanguages();
    auto hLangCombo = GetDlgItem(m_hForm, IDC_LANGCOMBO);
    for (const auto& langName : languages)
        ComboBox_AddString(hLangCombo, langName.c_str());

    std::set<std::wstring> tempFonts;
    // Populates tempFonts.
    {
        HDC     dc = GetWindowDC(m_hForm);
        LOGFONT lf = { 0 };
        lf.lfCharSet = DEFAULT_CHARSET;
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
    int  index = ComboBox_AddString(hFontSizeCombo, L"");
    ComboBox_SetItemData(hFontSizeCombo, 0, 0);
    static constexpr int fontSizes[] = { 5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28 };
    for (auto fontSize : fontSizes)
    {
        std::wstring s = std::to_wstring(fontSize);
        index = static_cast<int>(ComboBox_AddString(hFontSizeCombo, s.c_str()));
        ComboBox_SetItemData(hFontSizeCombo, index, fontSize);
    }
    m_fgColor.ConvertToColorButton(m_hForm, IDC_FG_BTN);
    m_bkColor.ConvertToColorButton(m_hForm, IDC_BK_BTN);

    ResString sExtTooltip(g_hRes, IDS_EXTENSIONTOOLTIP);

    AddToolTip(IDC_EXTENSIONS, sExtTooltip);

    // Select the current language.
        
    if (HasActiveDocument())
    {
        const auto& doc = GetActiveDocument();
        ComboBox_SelectString(hLangCombo, -1, CUnicodeUtils::StdGetUnicode(doc.GetLanguage()).c_str());
    }
    OnCommand(*this, IDC_LANGCOMBO, nullptr, CBN_SELCHANGE);

    auto style = Scintilla().StyleAt(Scintilla().CurrentPos());
    SelectStyle(style);
}

void CCmdConfigStyle::SelectStyle(int style)
{
    auto hStyleCombo = GetDlgItem(m_hForm, IDC_STYLECOMBO);
    int  styleCount = ComboBox_GetCount(hStyleCombo);
    bool selected = false;
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

bool CCmdConfigStyle::Execute()
{
    DoModal(g_hRes, IDD_STYLECONFIGURATOR, GetHwnd());
    return true;
}

namespace
{
    COLORREF g_acrCustClr[16] = {};

    static inline std::wstring GetWindowText(HWND hwnd)
    {
        wchar_t buf[20]; // We're dealing with color values no longer than three chars.
        ::GetWindowText(hwnd, buf, _countof(buf));
        return buf;
    }

    static BOOL CALLBACK EnumChildWindowProc(HWND hwnd, LPARAM lParam)
    {
        auto& [wndClassName, winVec] = *reinterpret_cast<std::pair<std::wstring, std::vector<HWND>>*>(lParam);

        if (wndClassName.empty())
            winVec.push_back(hwnd);
        else
        {
            wchar_t className[257];
            auto    status = ::GetClassName(hwnd, className, _countof(className));
            if (status > 0)
            {
                if (_wcsicmp(className, wndClassName.c_str()) == 0)
                    winVec.push_back(hwnd);
            }
        }
        return TRUE;
    }

    // Empty classname means match child windows of ANY classname.
    std::vector<HWND> GetChildWindows(HWND hwnd, const std::wstring& classname)
    {
        std::pair<std::wstring, std::vector<HWND>> state;
        state.first = classname;
        EnumChildWindows(hwnd, EnumChildWindowProc, reinterpret_cast<LPARAM>(&state));
        return state.second;
    }
} // namespace

CColorButton::CColorButton()
{
}

CColorButton::~CColorButton()
{
}

bool CColorButton::ConvertToColorButton(HWND hwndCtl)
{
    // Subclass the existing control.
    m_pfnOrigCtlProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwndCtl, GWLP_WNDPROC));
    SetWindowLongPtr(hwndCtl, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(_ColorButtonProc));
    SetWindowLongPtr(hwndCtl, GWLP_USERDATA, reinterpret_cast<LPARAM>(this));
    m_hwnd = hwndCtl;
    return true;
}

BOOL CColorButton::ConvertToColorButton(HWND hwndParent, UINT uiCtlId)
{
    m_ctlId = uiCtlId;
    return ConvertToColorButton(GetDlgItem(hwndParent, uiCtlId));
}

LRESULT CALLBACK CColorButton::_ColorButtonProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    CColorButton* pColorButton = GetObjectFromWindow(hwnd);
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC         hdc = BeginPaint(hwnd, &ps);
        RECT        rc;
        GetClientRect(hwnd, &rc);
        SetBkColor(hdc, pColorButton->m_color);
        ExtTextOut(hdc, rc.left, rc.top, ETO_CLIPPED | ETO_OPAQUE, &rc, L"", 0, nullptr);
        HBRUSH hbr = CreateSolidBrush(pColorButton->m_hoverColor == 0 ? CTheme::CurrentTheme().itemHover : pColorButton->m_hoverColor);
        FrameRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        EndPaint(hwnd, &ps);
        return 0L;
    }
    case WM_MOUSELEAVE:
    {
        pColorButton->m_hoverColor = 0;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        TRACKMOUSEEVENT tme = { 0 };
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);

        pColorButton->m_hoverColor = RGB(255, 0, 0);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_ERASEBKGND:
        return TRUE;
    case WM_KEYUP:
    {
        if (wParam != VK_SPACE)
            break;
    }
    [[fallthrough]];
    case WM_LBUTTONUP:
    {
        CHOOSECOLOR cc = { 0 };
        cc.lStructSize = sizeof(CHOOSECOLOR);
        cc.hwndOwner = hwnd;
        cc.rgbResult = pColorButton->m_color;
        cc.lpCustColors = g_acrCustClr;
        cc.lCustData = reinterpret_cast<LPARAM>(pColorButton);
        cc.lpfnHook = CCHookProc;
        cc.Flags = CC_ANYCOLOR | CC_RGBINIT | CC_ENABLEHOOK | CC_FULLOPEN;

        if (ChooseColor(&cc) != FALSE)
        {
            pColorButton->m_dialogResult = ColorButtonDialogResult::Ok;
        }
        else if (pColorButton->m_hasLastColor)
        {
            pColorButton->SetColor(cc.rgbResult);
            pColorButton->m_dialogResult = ColorButtonDialogResult::Cancel;
            SendMessage(GetParent(hwnd), WM_COMMAND, pColorButton->m_ctlId, reinterpret_cast<LPARAM>(hwnd));
        }
        return 0;
    }
    case WM_DESTROY:
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(pColorButton->m_pfnOrigCtlProc));
        break;
    default:
        break;
    }

    return CallWindowProc(pColorButton->m_pfnOrigCtlProc, hwnd, message, wParam, lParam);
}

void CColorButton::SetColor(COLORREF clr)
{
    m_color = clr;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// This function is attempts to hijack and hack the standard basic Windows
// Color Control into allow repeated colors to be selected without having
// to press OK each time. Each selection initiates a WM_COMMAND.
// If Cancel is chosen, a WM_COMMAND is sent that undoes the previous color set.
// The logic in this function is based on an idea from this article:
// http://stackoverflow.com/questions/16583139/get-color-property-while-colordialog-still-open-before-confirming-the-dialog
// As the article states, it is questionable to do what we do in this function
// as the code i subject to break if the Windows Color Control changes
// in the future.

UINT_PTR CALLBACK CColorButton::CCHookProc(
    _In_ HWND   hDlg,
    _In_ UINT   uiMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    switch (uiMsg)
    {
    case WM_INITDIALOG:
    {
        const CHOOSECOLOR& cc = *reinterpret_cast<CHOOSECOLOR*>(lParam);
        // Crude attempt to assert this data slot isn't used by anybody but us.
        assert(GetWindowLongPtr(hDlg, GWLP_USERDATA) == static_cast<LONG_PTR>(0));
        SetWindowLongPtr(hDlg, GWLP_USERDATA, cc.lCustData);
        CColorButton* pColorBtn = reinterpret_cast<CColorButton*>(cc.lCustData);
        auto          mainWindow = GetAncestor(hDlg, GA_ROOT);
        pColorBtn->m_colorEdits = GetChildWindows(mainWindow, L"Edit");
        assert(pColorBtn->m_colorEdits.size() == 6);
    }
    break;
    case WM_CTLCOLOREDIT:
    {
        // InitDialog should have set this.
        CColorButton* pColorButton = GetObjectFromWindow(hDlg);
        assert(pColorButton != nullptr);
        assert(pColorButton->m_colorEdits.size() == 6);

        // See top of function for what's going on here.
        std::vector<std::wstring> colorText;
        for (auto hwnd : pColorButton->m_colorEdits)
            colorText.push_back(GetWindowText(hwnd));

        COLORREF color = 0;
        try
        {
            BYTE r = static_cast<BYTE>(std::stoi(colorText[3]));
            BYTE g = static_cast<BYTE>(std::stoi(colorText[4]));
            BYTE b = static_cast<BYTE>(std::stoi(colorText[5]));
            color = RGB(r, g, b);
        }
        catch (const std::exception& /*ex*/)
        {
            return 0;
        }
        if (!pColorButton->m_hasLastColor || color != pColorButton->m_lastColor)
        {
            pColorButton->m_lastColor = color;
            pColorButton->m_hasLastColor = true;
            pColorButton->SetColor(color);
            SendMessage(GetParent(hDlg), WM_COMMAND,
                pColorButton->m_ctlId, reinterpret_cast<LPARAM>(GetParent(hDlg)));
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

CDialogWithFilterableList::CDialogWithFilterableList(void* obj)
    : ICommand(obj), m_pCmd(nullptr), m_hFilter(nullptr), m_hResults(nullptr)
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

        InitDialog(hwndDlg, 0/*IDI_BOWPAD*/, false);
        CTheme::Instance().SetThemeForDialog(*this, CTheme::Instance().IsDarkTheme());
        // initialize the controls
        m_hFilter = GetDlgItem(*this, IDC_FILTER);
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
        rc.top = 38;
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
    default:
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
    default:
        break;
    }
    return 1;
}

void CDialogWithFilterableList::InitResultsList() const
{
    SetWindowTheme(m_hResults, L"Explorer", nullptr);
    ListView_SetItemCountEx(m_hResults, 0, 0);

    auto hListHeader = ListView_GetHeader(m_hResults);
    int  c = Header_GetItemCount(hListHeader) - 1;
    while (c >= 0)
        ListView_DeleteColumn(m_hResults, c--);

    DWORD exStyle = LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT;
    ListView_SetExtendedListViewStyle(m_hResults, exStyle);
    RECT rc;
    GetClientRect(m_hResults, &rc);

    LVCOLUMN lvc{};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_IMAGE;
    lvc.fmt = LVCFMT_LEFT | LVCF_IMAGE;
    ListView_InsertColumn(m_hResults, 0, &lvc);
}

LRESULT CDialogWithFilterableList::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    switch (lpNMItemActivate->hdr.code)
    {
    case LVN_GETINFOTIP:
    {
        LPNMLVGETINFOTIP tip = reinterpret_cast<LPNMLVGETINFOTIPW>(lpNMItemActivate);
        auto              itemIndex = tip->iItem;
        if (itemIndex < 0 || itemIndex >= m_results.size())
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
        auto itemIndex = pDispInfo->item.iItem;
        if (itemIndex >= m_results.size())
            return 0;

        std::wstring sTemp;
        const auto& item = m_results[itemIndex];
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
    auto                filterText = GetDlgItemText(IDC_FILTER);
    std::wstring        sFilterText = filterText.get();
    if (force || lastFilterText.empty() || _wcsicmp(sFilterText.c_str(), lastFilterText.c_str()))
    {
        m_results.clear();

        for (int i = 0; i < m_allResults.size(); i++)
        {
            CListItem cmd = m_allResults[i];
            if (IsFiltered(sFilterText, cmd))
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
        COLORREF back = CTheme::CurrentTheme().winBack;
        COLORREF selBack = CTheme::CurrentTheme().itemHover; // RGB(200, 200, 200); // CTheme::CurrentTheme().selBack;

        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        int  width = ps.rcPaint.right - ps.rcPaint.left;
        int  height = ps.rcPaint.bottom - ps.rcPaint.top;
        auto memDC = ::CreateCompatibleDC(ps.hdc);
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
            pThis->DrawItemText(memDC, &rc, i);
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

void CCmdCommandPalette::DrawItemText(HDC hdc, LPRECT rc, int idx)
{
    DrawText(hdc, m_results[idx].text2.c_str(), -1, rc, DT_LEFT | DT_VCENTER);
    rc->left -= PADDINGS.x;
    rc->right -= PADDINGS.x;
    DrawText(hdc, m_results[idx].text3.c_str(), -1, rc, DT_RIGHT | DT_VCENTER);
}

CCmdCommandPalette::CCmdCommandPalette(void* obj)
    : CDialogWithFilterableList(obj)
{
    const auto& commands = CCommandHandler::Instance().GetCommands();
    for (const auto& [cmdId, pCommand] : commands)
    {
        CListItem data = {};
        data.uintVal = cmdId;
        data.text1 = CCommandHandler::Instance().GetCommandLabel(cmdId);
        data.text2 = CCommandHandler::Instance().GetCommandDescription(cmdId);
        data.text2.erase(0, data.text2.find_first_not_of(L" "));
        data.text2.erase(data.text2.find_last_not_of(L" ") + 1);
        data.text3 = CCommandHandler::Instance().GetShortCutStringForCommand(cmdId);
        if (!data.text1.empty())
            m_allResults.push_back(data);
    }
    const auto& noDelCommands = CCommandHandler::Instance().GetNoDeleteCommands();
    for (const auto& [cmdId, pCommand] : noDelCommands)
    {
        CListItem data;
        data.uintVal = cmdId;
        data.text1 = CCommandHandler::Instance().GetCommandLabel(cmdId);
        data.text2 = CCommandHandler::Instance().GetCommandDescription(cmdId);
        data.text2.erase(0, data.text2.find_first_not_of(L" "));
        data.text2.erase(data.text2.find_last_not_of(L" ") + 1);
        data.text3 = CCommandHandler::Instance().GetShortCutStringForCommand(cmdId);
        if (!data.text1.empty())
            m_allResults.push_back(data);
    }

    std::sort(m_allResults.begin(), m_allResults.end(),
        [](const CListItem& a, const CListItem& b) -> bool {
            return StrCmpLogicalW(a.text2.c_str(), b.text2.c_str()) < 0;
        });
}


UINT CCmdCommandPalette::GetFilterCUE()
{
    return IDS_COMMANDPALETTE_FILTERCUE;
}

void CCmdCommandPalette::OnOK()
{
    auto i = ListView_GetSelectionMark(m_hResults);
    if (i >= 0)
    {
        const auto& data = m_results[i];
        m_pCmd = nullptr;
        SendMessage(GetParent(*this), WM_COMMAND, MAKEWPARAM(data.uintVal, 1), 0);
        ShowWindow(*this, SW_HIDE);
    }
}

LRESULT CCmdCommandPalette::DoListNotify(LPNMITEMACTIVATE lpNMItemActivate)
{
    switch (lpNMItemActivate->hdr.code)
    {
    case LVN_GETINFOTIP:
    {
        LPNMLVGETINFOTIP tip = reinterpret_cast<LPNMLVGETINFOTIPW>(lpNMItemActivate);
        auto              itemIndex = tip->iItem;
        if (itemIndex < 0 || itemIndex >= m_results.size())
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
        if (lpNMItemActivate->iItem >= 0 && lpNMItemActivate->iItem < m_results.size())
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

bool CCmdCommandPalette::IsFiltered(std::wstring sFilterText, CListItem item)
{
    return sFilterText.empty() || StrStrIW(item.text1.c_str(), sFilterText.c_str()) || StrStrIW(item.text2.c_str(), sFilterText.c_str());
}

CCmdOpenRecent::~CCmdOpenRecent()
{
    PostMessage(*this, WM_CLOSE, 0, 0);
}

bool CCmdOpenRecent::Execute()
{
    ClearFilterText();
    std::vector<std::wstring> recents = GetRecents();

    auto count = recents.size();
    m_allResults.clear();
    for (int i = 0; i < count; i++)
    {
        m_allResults.push_back(CListItem(0, false, recents[i]));
    }
    CDialogWithFilterableList::Execute();
    return true;
}

CCmdOpenRecent::CCmdOpenRecent(void* obj)
    : CDialogWithFilterableList(obj)
{
}

void CCmdOpenRecent::OnOK()
{
    auto i = ListView_GetSelectionMark(m_hResults);
    if (i >= 0)
    {
        auto buf = std::make_unique<wchar_t[]>(MAX_PATH + 1);
        ListView_GetItemText(m_hResults, i, 0, buf.get(), MAX_PATH);
        auto itemText = buf.get();
        if (PathIsDirectory(itemText))
            OpenFolder(itemText);
        else
            OpenFile(itemText, OpenFlags::AskToCreateIfMissing);

        m_pCmd = nullptr;
    }
}

UINT CCmdOpenRecent::GetFilterCUE()
{
    return IDS_OPENRECENT_FILTERCUE;
}
