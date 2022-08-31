// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017, 2020-2021 - Stefan Kueng
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
#include "CmdOpenselection.h"
#include "main.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"
#include "PathUtils.h"
#include "OnOutOfScope.h"
#include "ResString.h"

extern void findReplaceFindFile(void* mainWnd, const std::wstring& fileName);

CCmdOpenSelection::CCmdOpenSelection(void* obj)
    : ICommand(obj)
{
}

bool CCmdOpenSelection::Execute()
{
    // ensure that the next attempt to show the context menu will
    // re-evaluate both the label and enabled state
    //InvalidateUICommand(cmdOpenSelection, UI_INVALIDATIONS_PROPERTY, &UI_PKEY_Label);
    //InvalidateUICommand(cmdOpenSelection, UI_INVALIDATIONS_STATE, &UI_PKEY_Enabled);

    // If the current selection (or default selection) is a folder
    // then show it in explorer. Else if it's a file then open it in the editor.
    // If it can't be found, then open it in the file finder.
    // If it's a relative path, look for it it relative from the current
    // file's parent older.
    std::wstring path = GetPathUnderCursor();
    if (path.empty())
        return false;

    if (PathIsDirectory(path.c_str()))
    {
        SHELLEXECUTEINFO shi = {};
        shi.cbSize           = sizeof(SHELLEXECUTEINFO);
        shi.fMask            = SEE_MASK_DOENVSUBST | SEE_MASK_UNICODE;
        shi.hwnd             = GetHwnd();
        shi.lpVerb           = L"open";
        shi.lpFile           = nullptr;
        shi.lpParameters     = nullptr;
        shi.lpDirectory      = path.c_str();
        shi.nShow            = SW_SHOW;

        return !!ShellExecuteEx(&shi);
    }
    if (PathFileExists(path.c_str()))
        return OpenFile(path.c_str(), OpenFlags::AddToMRU) >= 0;
    findReplaceFindFile(m_pMainWindow, path);
    return false;
}

std::wstring CCmdOpenSelection::GetPathUnderCursor()
{
    if (!HasActiveDocument())
        return std::wstring();

    auto pathUnderCursor = CUnicodeUtils::StdGetUnicode(GetSelectedText(SelectionHandling::None));
    if (pathUnderCursor.empty())
    {
        auto lineBuffer = GetWordChars();
        OnOutOfScope(Scintilla().SetWordChars(lineBuffer.c_str()));

        Scintilla().SetWordChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.,#/\\");
        size_t pos = Scintilla().CurrentPos();

        std::string sWordA = GetTextRange(Scintilla().WordStartPosition(pos, false),
                                          Scintilla().WordEndPosition(pos, false));
        pathUnderCursor    = CUnicodeUtils::StdGetUnicode(sWordA);
    }

    // Look for the file name under the cursor in either the same directory
    // as the current document or in it's parent directory.
    // Finally try look for it in the current directory.
    if (PathIsRelative(pathUnderCursor.c_str()))
    {
        const auto& doc = GetActiveDocument();
        if (!doc.m_path.empty())
        {
            std::wstring parent = CPathUtils::GetParentDirectory(doc.m_path);
            if (!parent.empty())
            {
                std::wstring path = CPathUtils::Append(parent, pathUnderCursor);
                if (PathFileExists(path.c_str()))
                    return path;
                parent = CPathUtils::GetParentDirectory(parent);
                if (!parent.empty())
                {
                    path = CPathUtils::Append(parent, pathUnderCursor);
                    if (PathFileExists(path.c_str()))
                        return path;
                }
            }
        }
    }
    return pathUnderCursor;
}
