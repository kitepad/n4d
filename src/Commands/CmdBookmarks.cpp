// This file is part of BowPad.
//
// Copyright (C) 2014-2016, 2020-2021 - Stefan Kueng
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
#include "CmdBookmarks.h"
#include "ScintillaWnd.h"
#include "StringUtils.h"

namespace
{
constexpr auto g_bmColor = RGB(255, 0, 0);
}

CCmdBookmarks::CCmdBookmarks(void* obj)
    : ICommand(obj)
{
    //auto& settings = CIniSettings::Instance();
    auto maxFiles = GetInt64(BOOKMARKS_SECTION, L"BookmarksMaxFiles", 30);
    m_bookmarks.clear();
    for (decltype(maxFiles) fileIndex = 0; fileIndex < maxFiles; ++fileIndex)
    {
        std::wstring sKey    = CStringUtils::Format(L"file%d", fileIndex);
        std::wstring sBmData = GetString(BOOKMARKS_SECTION, sKey.c_str(), L"");
        if (!sBmData.empty())
        {
            std::vector<std::wstring> tokens;
            stringtok(tokens, sBmData, true, L"*");
            if (tokens.size() >= 1)
            {
                const std::wstring& filepath = tokens[0];
                if (tokens.size() > 1)
                {
                    auto& lines = m_bookmarks[filepath];
                    for (decltype(tokens.size()) li = 1; li < tokens.size(); ++li)
                        lines.push_back(std::stoi(tokens[li]));
                }
            }
        }
    }
}

void CCmdBookmarks::OnDocumentClose(DocID id)
{
    //auto&       settings = CIniSettings::Instance();
    const auto& doc      = GetDocumentFromID(id);
    if (doc.m_path.empty())
        return;

    // look if the path is already in our list
    bool bModified = (m_bookmarks.erase(doc.m_path) > 0);

    // find all bookmarks
    std::vector<sptr_t> bookmarkLines;
    for (sptr_t line = -1;;)
    {
        line = Scintilla().MarkerNext(line + 1, (1 << MARK_BOOKMARK));
        if (line < 0)
            break;
        bookmarkLines.push_back(line);
    }

    if (!bookmarkLines.empty())
    {
        m_bookmarks[doc.m_path] = std::move(bookmarkLines);
        bModified               = true;
    }

    if (bModified)
    {
        // Save the bookmarks to the ini file
        auto maxFiles = GetInt64(BOOKMARKS_SECTION, L"BookmarksMaxFiles", 30);
        int fileNum  = 0;
        if (maxFiles == 0)
            return;
        std::wstring bmValue;
        for (const auto& [path, lines] : m_bookmarks)
        {
            bmValue = path;
            assert(!bmValue.empty());
            for (const auto lineNr : lines)
            {
                bmValue += L'*';
                bmValue += std::to_wstring(lineNr);
            }
            std::wstring sKey = CStringUtils::Format(L"file%d", fileNum);
            SetString(BOOKMARKS_SECTION, sKey.c_str(), bmValue.c_str());
            ++fileNum;
            if (fileNum >= maxFiles)
                break;
        }
        std::wstring sKey = CStringUtils::Format(L"file%d", fileNum);
        SetString(BOOKMARKS_SECTION, sKey.c_str(), L"");
    }
}

void CCmdBookmarks::OnDocumentOpen(DocID id)
{
    const CDocument& doc = GetDocumentFromID(id);
    if (doc.m_path.empty())
        return;

    auto it = m_bookmarks.find(doc.m_path);
    if (it != m_bookmarks.end())
    {
        const auto& lines = it->second;
        for (const auto line : lines)
        {
            Scintilla().MarkerAdd(line, MARK_BOOKMARK);
            DocScrollAddLineColor(DOCSCROLLTYPE_BOOKMARK, line, g_bmColor);
        }
        if (!lines.empty())
            DocScrollUpdate();
    }
}

// CCmdBookmarkToggle

bool CCmdBookmarkToggle::Execute()
{
    auto line = GetCurrentLineNumber();

    LRESULT state = Scintilla().MarkerGet(line);
    if ((state & (1 << MARK_BOOKMARK)) != 0)
    {
        Scintilla().MarkerDelete(line, MARK_BOOKMARK);
        DocScrollRemoveLine(DOCSCROLLTYPE_BOOKMARK, line);
    }
    else
    {
        Scintilla().MarkerAdd(line, MARK_BOOKMARK);
        DocScrollAddLineColor(DOCSCROLLTYPE_BOOKMARK, line, g_bmColor);
    }
    DocScrollUpdate();

    return true;
}

// CCmdBookmarkClearAll

bool CCmdBookmarkClearAll::Execute()
{
    Scintilla().MarkerDeleteAll(MARK_BOOKMARK);
    DocScrollClear(DOCSCROLLTYPE_BOOKMARK);
    return true;
}

// CCmdBookmarkNext

bool CCmdBookmarkNext::Execute()
{
    auto line = GetCurrentLineNumber();
    line      = Scintilla().MarkerNext(line + 1, (1 << MARK_BOOKMARK));
    if (line >= 0)
        Scintilla().GotoLine(line);
    else
    {
        // retry from the start of the document
        line = Scintilla().MarkerNext(0, (1 << MARK_BOOKMARK));
        if (line >= 0)
            Scintilla().GotoLine(line);
    }
    return true;
}

// CCmdBookmarkPrev

bool CCmdBookmarkPrev::Execute()
{
    auto line = GetCurrentLineNumber();
    line      = Scintilla().MarkerPrevious(line - 1, (1 << MARK_BOOKMARK));
    if (line >= 0)
        Scintilla().GotoLine(line);
    else
    {
        // retry from the start of the document
        line = Scintilla().MarkerPrevious(Scintilla().LineCount(), (1 << MARK_BOOKMARK));
        if (line >= 0)
            Scintilla().GotoLine(line);
    }
    return true;
}