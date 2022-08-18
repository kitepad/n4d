﻿// This file is part of BowPad.
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
#include "main.h"
#include "AppUtils.h"
#include "Theme.h"
#include "LexStyles.h"
#include "ResString.h"

#include <strsafe.h>
#include "UnicodeUtils.h"

constexpr POINT PADDINGS = {16, 4};

CCmdToggleTheme::CCmdToggleTheme(void* obj)
    : ICommand(obj)
{
    int dark = static_cast<int>(CIniSettings::Instance().GetInt64(L"View", L"darktheme", 0));
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
    CIniSettings::Instance().SetInt64(L"View", L"autobrace", CIniSettings::Instance().GetInt64(L"View", L"autobrace", 1) ? 0 : 1);
    //InvalidateUICommand(UI_INVALIDATIONS_PROPERTY, &UI_PKEY_BooleanValue);
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
    CIniSettings::Instance().SetInt64(L"View", L"autocomplete", CIniSettings::Instance().GetInt64(L"View", L"autocomplete", 1) ? 0 : 1);
    return true;
}


namespace
{
std::unique_ptr<CCmdSelectLexerDlg>    g_pSelectLexerDlg;
std::unique_ptr<CCmdSelectEncodingDlg> g_pSelectEncodingDlg;
} // namespace

CCmdSelectLexer::~CCmdSelectLexer()
{
    if (g_pSelectLexerDlg != nullptr)
    {
        PostMessage(*g_pSelectLexerDlg, WM_CLOSE, 0, 0);
    }
}

bool CCmdSelectLexer::Execute()
{
    if (g_pSelectLexerDlg == nullptr)
        g_pSelectLexerDlg = std::make_unique<CCmdSelectLexerDlg>(m_pMainWindow);
    
    g_pSelectLexerDlg->ClearFilterText();
    g_pSelectLexerDlg->ShowModeless(g_hRes, IDD_COMMANDPALETTE, GetHwnd(), FALSE);

    constexpr UINT flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;
    RECT  rc;
    GetClientRect(GetHwnd(), &rc);
    POINT pt((rc.right - rc.left - 720) / 2, CTheme::CurrentTheme().tabHeight + CTheme::CurrentTheme().titleHeight);
    ClientToScreen(GetHwnd(), &pt);
    SetWindowPos(*g_pSelectLexerDlg, nullptr, pt.x, pt.y, 720, 400, flags);
    

    return true;
}

CCmdSelectEncoding::~CCmdSelectEncoding()
{
    if (g_pSelectEncodingDlg != nullptr)
    {
        PostMessage(*g_pSelectEncodingDlg, WM_CLOSE, 0, 0);
    }
}

bool CCmdSelectEncoding::Execute()
{
    if (g_pSelectEncodingDlg == nullptr)
        g_pSelectEncodingDlg = std::make_unique<CCmdSelectEncodingDlg>(m_pMainWindow);

    g_pSelectEncodingDlg->ClearFilterText();
    g_pSelectEncodingDlg->ShowModeless(g_hRes, IDD_COMMANDPALETTE, GetHwnd(), FALSE);

    constexpr UINT flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;
    RECT           rc;
    GetClientRect(GetHwnd(), &rc);
    POINT pt((rc.right - rc.left - 720) / 2, CTheme::CurrentTheme().tabHeight + CTheme::CurrentTheme().titleHeight);
    ClientToScreen(GetHwnd(), &pt);
    SetWindowPos(*g_pSelectEncodingDlg, nullptr, pt.x, pt.y, 720, 400, flags);


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

CCmdSelectLexerDlg::CCmdSelectLexerDlg(void* obj)
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

void CCmdSelectLexerDlg::OnOK()
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

UINT CCmdSelectLexerDlg::GetFilterCUE()
{
    return IDS_SELECTLEXER_FILTERCUE;
}

CCmdSelectEncodingDlg::CCmdSelectEncodingDlg(void* obj)
    : CDialogWithFilterableList(obj)
{
    if (codepages.empty())
        EnumSystemCodePages(EnumerateCodePage, CP_INSTALLED);

    m_allResults = codepages;
}

UINT CCmdSelectEncodingDlg::GetFilterCUE()
{
    return IDS_SELECTENCODING_FILTERCUE;
}

void CCmdSelectEncodingDlg::OnOK()
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