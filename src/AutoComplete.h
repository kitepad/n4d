﻿// This file is part of BowPad.
//
// Copyright (C) 2021 - Stefan Kueng
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
#include "BaseDialog.h"
#include "StringUtils.h"
#include "ScintillaWnd.h"
#include "../ext/scintilla/include/Sci_Position.h"
#include <mutex>

class CMainWindow;
class DocID;
struct SCNotification;

enum class AutoCompleteType : int
{
    None = -1,
    Code = 0,
    Path,
    Snippet,
    Word,
};

class CAutoComplete
{
    friend class CAutoCompleteConfigDlg;

public:
    CAutoComplete(CScintillaWnd* scintilla);
    virtual ~CAutoComplete();

    void Init();
    void HandleScintillaEvents(const SCNotification* scn);
    bool HandleChar(WPARAM wParam, LPARAM lParam);
    void AddWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words);
    void AddWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words);
    void AddWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words);
    void AddWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words);

private:
    //void                 PrepareWordList(std::map<std::string, AutoCompleteType>* wList);
    void                 HandleAutoComplete(const SCNotification* scn);
    void                 ExitSnippetMode();
    void                 MarkSnippetPositions(bool clearOnly);
    void                 PrepareWordList(std::map<std::string, AutoCompleteType>& wordList) const;
    static bool          IsWordChar(int ch);
    std::string          SanitizeSnippetText(const std::string& text) const;
    static void          SetWindowStylesForAutocompletionPopup();
    static BOOL CALLBACK AdjustThemeProc(HWND hwnd, LPARAM lParam);
    
private:
    CScintillaWnd* m_editor;

    // map of [language, [word, AutoCompleteType]]
    std::map<std::string, std::map<std::string, AutoCompleteType, ci_less>> m_langWordList;
    std::map<std::string, std::map<std::string, std::string>>               m_langSnippetList;
    std::map<DocID, std::map<std::string, AutoCompleteType, ci_less>>       m_docWordList;
    std::recursive_mutex                                                    m_mutex;
    bool                                                                    m_insertingSnippet;
    std::string                                                             m_stringToSelect;
    std::map<int, std::vector<Sci_Position>>                                m_snippetPositions;
    int                                                                     m_currentSnippetPos;
};

class CAutoCompleteConfigDlg : public CDialog
{
public:
    CAutoCompleteConfigDlg(CMainWindow* main);
    ~CAutoCompleteConfigDlg() = default;

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);

private:
    CMainWindow*  m_main;
    CScintillaWnd m_scintilla;
};

