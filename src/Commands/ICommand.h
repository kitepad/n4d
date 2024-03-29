﻿// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2021 - Stefan Kueng
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
#include "Scintilla.h"
#include "DocumentManager.h"
#include "Document.h"
#include "../AutoComplete.h"

#include <vector>
#include <string>

struct TBHDR
{
    NMHDR hdr;
    int   tabOrigin;
};

class CMainWindow;

namespace OpenFlags
{
constexpr unsigned int AddToMRU             = 1;
constexpr unsigned int AskToCreateIfMissing = 2;
constexpr unsigned int IgnoreIfMissing      = 4;
constexpr unsigned int OpenIntoActiveTab    = 8;
constexpr unsigned int NoActivate           = 16;
constexpr unsigned int CreateTabOnly        = 32;
constexpr unsigned int CreateIfMissing      = 64;
}; // namespace OpenFlags

class ICommand
{
public:
    ICommand(void* obj);
    virtual ~ICommand() {}

    /// Execute the command
    virtual bool Execute()  = 0;
    virtual UINT GetCmdId() = 0;
    virtual bool IsItemsSourceCommand() { return false; }

    virtual void ScintillaNotify(SCNotification* pScn);
    // note: the 'tabOrigin' member of the TBHDR is only valid for TCN_GETCOLOR, TCN_TABDROPPED, TCN_TABDROPPEDOUTSIDE, TCN_ORDERCHANGED
    virtual void    TabNotify(TBHDR* ptbHdr);
    virtual void    OnClose();
    virtual void    BeforeLoad();
    virtual void    AfterInit();
    virtual void    OnLangChanged();
    virtual void    OnStylesSet();
    virtual void    OnThemeChanged(bool bDark);
    virtual void    OnDocumentClose(DocID id);
    virtual void    OnDocumentOpen(DocID id);
    virtual void    OnDocumentSave(DocID id, bool bSaveAs);
    virtual void    OnClipboardChanged();
    HWND            GetHwnd() const;
    HWND            GetScintillaWnd() const;
    HWND            GetStatusbarWnd() const;

protected:
    void         OpenFolder(std::wstring path);
    void         TabActivateAt(int index) const;
    void         UpdateTab(int index) const;
    DocID        GetDocIdOfCurrentTab() const;
    int          GetActiveTabIndex() const;
    int          GetTabCount() const;
    std::wstring GetCurrentTitle() const;
    std::wstring GetTitleForTabIndex(int index) const;
    std::wstring GetTitleForDocID(DocID id) const;
    void         SetCurrentTitle(LPCWSTR title) const;
    void         SetTitleForDocID(DocID id, LPCWSTR title) const;
    bool         CloseTab(int index, bool bForce) const;
    DocID        GetDocIDFromTabIndex(int tab) const;
    DocID        GetDocIDFromPath(LPCTSTR path) const;
    int          GetTabIndexFromDocID(DocID docID) const;
    void         OpenNewTab() const;
    int          GetWidthToLeft(int idx);
    int              GetDocumentCount() const;
    bool             HasActiveDocument() const;
    const CDocument& GetActiveDocument() const;
    CDocument&       GetModActiveDocument() const;
    bool             HasDocumentID(DocID id) const;
    const CDocument& GetDocumentFromID(DocID id) const;
    CDocument&       GetModDocumentFromID(DocID id) const;
    int              GetStatusBarHeight();
    Scintilla::ScintillaCall& Scintilla() const;
    void         UpdateStatusBar(bool bEverything) const;
    void         SetupLexerForLang(const std::string& lang) const;
    std::string  GetCurrentLanguage() const;
    void         DocScrollClear(int type) const;
    void         DocScrollAddLineColor(int type, size_t line, COLORREF clr) const;
    void         DocScrollUpdate() const;
    void         DocScrollRemoveLine(int type, size_t line) const;
    void         UpdateLineNumberWidth() const;
    void         GotoLine(sptr_t line) const;
    void         Center(sptr_t startPos, sptr_t endPos) const;
    void         GotoBrace() const;
    std::string  GetTextRange(sptr_t startPos, sptr_t endPos) const;
    size_t       FindText(const std::string& toFind, sptr_t startPos, sptr_t endPos) const;
    std::string  GetSelectedText(SelectionHandling handling) const;
    std::string  GetCurrentWord() const;
    std::string  GetCurrentLine() const;
    std::string  GetWordChars() const;
    void         MarkSelectedWord(bool clear, bool edit) const;
    void         ShowFileTree(bool bShow) const;
    bool         IsFileTreeShown() const;
    std::wstring GetFileTreePath() const;
    std::vector<std::wstring> GetRecents() const;
    void AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words) const;
    void AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words) const;
    void AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words) const;
    void AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words) const;

    int            OpenFile(LPCWSTR file, unsigned int openFlags) const;
    void           OpenFiles(const std::vector<std::wstring>& paths) const;
    void           OpenHDROP(HDROP hDrop) const;
    bool           ReloadTab(int tab, int encoding = -1) const; // By default reload encoding
    bool           SaveCurrentTab(bool bSaveAs = false) const;
    bool           SaveDoc(DocID docID, bool bSaveAs = false) const;
    bool           SaveDoc(DocID docID, const std::wstring& path) const;
    sptr_t         GetCurrentLineNumber() const;

protected:
    CMainWindow* m_pMainWindow;
};
