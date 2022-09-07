// This file is part of BowPad.
//
// Copyright (C) 2013-2017, 2019-2021 - Stefan Kueng
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
#include "CmdLaunch.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "UnicodeUtils.h"
#include "EscapeUtils.h"
#include "Theme.h"
#include "ResString.h"

bool LaunchBase::Launch(const std::wstring& cmdline) const
{
    if (cmdline.empty() || !HasActiveDocument())
        return false;
    std::wstring cmd = cmdline;
    // remove EOLs
    SearchRemoveAll(cmd, L"\n");
    SearchReplace(cmd, L"\r", L" ");
    // replace the macros in the command line
    const auto&  doc     = GetActiveDocument();
    std::wstring tabPath = doc.m_path;
    if ((cmd.find(L"$(TAB_PATH)") != std::wstring::npos) ||
        (cmd.find(L"$(TAB_NAME)") != std::wstring::npos) ||
        (cmd.find(L"$(TAB_EXT)") != std::wstring::npos))
    {
        // save the file first
        SaveCurrentTab();
    }

    auto tabDirPath = CPathUtils::GetParentDirectory(tabPath);
    if (PathFileExists(tabPath.c_str()))
    {
        SearchReplace(cmd, L"$(TAB_PATH)", tabPath);
        SearchReplace(cmd, L"$(TAB_DIR)", tabDirPath);
        SearchReplace(cmd, L"$(TAB_NAME)", CPathUtils::GetFileNameWithoutExtension(tabPath));
        SearchReplace(cmd, L"$(TAB_EXT)", CPathUtils::GetFileExtension(tabPath));
    }
    else
    {
        SearchRemoveAll(cmd, L"$(TAB_PATH)");
        SearchRemoveAll(cmd, L"$(TAB_DIR)");
    }

    SearchReplace(cmd, L"$(LINE)", std::to_wstring(GetCurrentLineNumber()));
    SearchReplace(cmd, L"$(POS)", std::to_wstring(static_cast<int>(Scintilla().CurrentPos())));
    // find selected text or current word
    std::string sSelText = GetSelectedText(SelectionHandling::CurrentWordIfSelectionIsEmpty);
    if (sSelText.empty())
    {
        // get the current word instead
        sSelText = GetCurrentWord();
    }
    std::wstring selText = CUnicodeUtils::StdGetUnicode(sSelText);
    SearchReplace(cmd, L"$(SEL_TEXT)", selText);
    SearchReplace(cmd, L"$(SEL_TEXT_ESCAPED)", CEscapeUtils::EscapeString(selText));

    // split the command line into the command and the parameters
    std::wstring params;
    if (cmd[0] == '"')
    {
        cmd             = cmd.substr(1);
        size_t quotePos = cmd.find_first_of('"');
        if (quotePos == std::wstring::npos)
            return false;
        params = cmd.substr(quotePos + 1);
        cmd    = cmd.substr(0, quotePos);
    }
    else
    {
        size_t spacePos = cmd.find_first_of(' ');
        if (spacePos != std::wstring::npos)
        {
            params = cmd.substr(spacePos + 1);
            cmd    = cmd.substr(0, spacePos);
        }
    }

    SHELLEXECUTEINFO shi = {0};
    shi.cbSize           = sizeof(SHELLEXECUTEINFO);
    shi.fMask            = SEE_MASK_DOENVSUBST | SEE_MASK_UNICODE;
    shi.hwnd             = GetHwnd();
    shi.lpVerb           = L"open";
    shi.lpFile           = cmd.c_str();
    shi.lpParameters     = params.empty() ? nullptr : params.c_str();
    shi.lpDirectory      = tabDirPath.c_str();
    shi.nShow            = SW_SHOW;

    return !!ShellExecuteEx(&shi);
}

