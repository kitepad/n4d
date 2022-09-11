// This file is part of BowPad.
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
#include "stdafx.h"
#include "ICommand.h"
#include "MainWindow.h"
#include "LexStyles.h"
#include "CommandHandler.h"

ICommand::ICommand(void* obj)
    : m_pMainWindow(static_cast<CMainWindow*>(obj))
{
}

void ICommand::ScintillaNotify(SCNotification* /*pScn*/)
{
}

void ICommand::TabNotify(TBHDR* /*ptbHdr*/)
{
}

void ICommand::OnClose()
{
}

void ICommand::BeforeLoad()
{
}

void ICommand::AfterInit()
{
}

void ICommand::OnDocumentClose(DocID /*id*/)
{
}

void ICommand::OnDocumentOpen(DocID /*id*/)
{
}

void ICommand::OnDocumentSave(DocID /*id*/, bool /*bSaveAs*/)
{
}

void ICommand::OnClipboardChanged()
{
}

void ICommand::OnThemeChanged(bool /*bDark*/)
{
}

void ICommand::OnLangChanged()
{
}

void ICommand::OnStylesSet()
{
}

void ICommand::OpenFolder(std::wstring path) 
{
    m_pMainWindow->OpenFolder(path);
}

void ICommand::TabActivateAt(int index) const
{
    m_pMainWindow->SetSelected(index);
}

void ICommand::UpdateTab(int index) const
{
    m_pMainWindow->UpdateTab(GetDocIDFromTabIndex(index));
}

int ICommand::GetActiveTabIndex() const
{
    return m_pMainWindow->GetCurrentTabIndex();
}

DocID ICommand::GetDocIdOfCurrentTab() const
{
    return m_pMainWindow->GetCurrentTabId();
}

std::wstring ICommand::GetCurrentTitle() const
{
    return m_pMainWindow->GetTabTitle(m_pMainWindow->GetSelected());
}

std::wstring ICommand::GetTitleForTabIndex(int index) const
{
    return m_pMainWindow->GetTabTitle(index);
}

std::wstring ICommand::GetTitleForDocID(DocID id) const
{
    return GetTitleForTabIndex(GetTabIndexFromDocID(id));
}

void ICommand::SetCurrentTitle(LPCWSTR title) const
{
    m_pMainWindow->SetTabTitle(m_pMainWindow->GetSelected(),title);
}

void ICommand::SetTitleForDocID(DocID id, LPCWSTR title) const
{
    m_pMainWindow->SetTabTitle(m_pMainWindow->GetIndexFromID(id), title);
    m_pMainWindow->UpdateCaptionBar();
}

int ICommand::GetTabCount() const
{
    return m_pMainWindow->GetItemCount();
}

bool ICommand::CloseTab(int index, bool bForce) const
{
    return m_pMainWindow->CloseTab(index, bForce);
}

Scintilla::ScintillaCall& ICommand::Scintilla() const
{
    return m_pMainWindow->m_editor.Scintilla();
}

HWND ICommand::GetHwnd() const
{
    return *m_pMainWindow;
}

HWND ICommand::GetScintillaWnd() const
{
    return m_pMainWindow->m_editor;
}

HWND ICommand::GetStatusbarWnd() const
{
    return m_pMainWindow->m_statusBar;
}

int ICommand::OpenFile(LPCWSTR file, unsigned int openFlags) const
{
    int idx = m_pMainWindow->OpenFile(file, openFlags);
    m_pMainWindow->SetSelected(idx);
    return idx;
}

void ICommand::OpenFiles(const std::vector<std::wstring>& paths) const
{
    return m_pMainWindow->OpenFiles(paths);
}

bool ICommand::ReloadTab(int tab, int encoding) const
{
    return m_pMainWindow->ReloadTab(tab, encoding);
}

bool ICommand::SaveCurrentTab(bool bSaveAs /* = false */) const
{
    return m_pMainWindow->SaveCurrentTab(bSaveAs);
}

bool ICommand::SaveDoc(DocID docID, bool bSaveAs /* = false */) const
{
    return m_pMainWindow->SaveDoc(docID, bSaveAs);
}

bool ICommand::SaveDoc(DocID docID, const std::wstring& path) const
{
    return m_pMainWindow->SaveDoc(docID, path);
}

int ICommand::GetDocumentCount() const
{
    return m_pMainWindow->m_docManager.GetCount();
}

bool ICommand::HasActiveDocument() const
{
    auto id = m_pMainWindow->GetCurrentTabId();
    return m_pMainWindow->m_docManager.HasDocumentID(id);
}

const CDocument& ICommand::GetActiveDocument() const
{
    return m_pMainWindow->m_docManager.GetDocumentFromID(m_pMainWindow->GetCurrentTabId());
}

CDocument& ICommand::GetModActiveDocument() const
{
    return m_pMainWindow->m_docManager.GetModDocumentFromID(m_pMainWindow->GetCurrentTabId());
}

bool ICommand::HasDocumentID(DocID id) const
{
    return m_pMainWindow->m_docManager.HasDocumentID(id);
}

const CDocument& ICommand::GetDocumentFromID(DocID id) const
{
    return m_pMainWindow->m_docManager.GetDocumentFromID(id);
}

CDocument& ICommand::GetModDocumentFromID(DocID id) const
{
    return m_pMainWindow->m_docManager.GetModDocumentFromID(id);
}

void ICommand::UpdateStatusBar(bool bEverything) const
{
    m_pMainWindow->UpdateStatusBar(bEverything);
}

void ICommand::SetupLexerForLang(const std::string& lang) const
{
    return m_pMainWindow->m_editor.SetupLexerForLang(lang);
}

std::string ICommand::GetCurrentLanguage() const
{
    return GetActiveDocument().GetLanguage();
}

void ICommand::DocScrollClear(int type) const
{
    m_pMainWindow->m_editor.DocScrollClear(type);
}

void ICommand::DocScrollAddLineColor(int type, size_t line, COLORREF clr) const
{
    m_pMainWindow->m_editor.DocScrollAddLineColor(type, line, clr);
}

void ICommand::DocScrollRemoveLine(int type, size_t line) const
{
    m_pMainWindow->m_editor.DocScrollRemoveLine(type, line);
}

void ICommand::UpdateLineNumberWidth() const
{
    m_pMainWindow->m_editor.UpdateLineNumberWidth();
}

void ICommand::DocScrollUpdate() const
{
    m_pMainWindow->m_editor.DocScrollUpdate();
}

void ICommand::GotoLine(sptr_t line) const
{
    m_pMainWindow->m_editor.GotoLine(line);
}

void ICommand::Center(sptr_t startPos, sptr_t endPos) const
{
    m_pMainWindow->m_editor.Center(startPos, endPos);
}

void ICommand::GotoBrace() const
{
    m_pMainWindow->m_editor.GotoBrace();
}

DocID ICommand::GetDocIDFromTabIndex(int tab) const
{
    return m_pMainWindow->GetIDFromIndex(tab);
}

int ICommand::GetTabIndexFromDocID(DocID docID) const
{
    return m_pMainWindow->GetIndexFromID(docID);
}

void ICommand::OpenNewTab() const
{
    return m_pMainWindow->OpenNewTab();
}

DocID ICommand::GetDocIDFromPath(LPCTSTR path) const
{
    return m_pMainWindow->m_docManager.GetIdForPath(path);
}

std::string ICommand::GetTextRange(sptr_t startPos, sptr_t endPos) const
{
    return m_pMainWindow->m_editor.GetTextRange(startPos, endPos);
}

size_t ICommand::FindText(const std::string& toFind, sptr_t startPos, sptr_t endPos) const
{
    return m_pMainWindow->m_editor.FindText(toFind, startPos, endPos);
}

std::string ICommand::GetSelectedText(SelectionHandling handling) const
{
    return m_pMainWindow->m_editor.GetSelectedText(handling);
}

std::string ICommand::GetCurrentWord() const
{
    return m_pMainWindow->m_editor.GetCurrentWord();
}

std::string ICommand::GetCurrentLine() const
{
    return m_pMainWindow->m_editor.GetCurrentLine();
}

std::string ICommand::GetWordChars() const
{
    return m_pMainWindow->m_editor.GetWordChars();
}

void ICommand::MarkSelectedWord(bool clear, bool edit) const
{
    return m_pMainWindow->m_editor.MarkSelectedWord(clear, edit);
}

void ICommand::OpenHDROP(HDROP hDrop) const
{
    return m_pMainWindow->HandleDropFiles(hDrop);
}

void ICommand::ShowFileTree(bool bShow) const
{
    return m_pMainWindow->ShowFileTree(bShow);
}

bool ICommand::IsFileTreeShown() const
{
    return m_pMainWindow->IsFileTreeShown();
}

std::wstring ICommand::GetFileTreePath() const
{
    return m_pMainWindow->GetFileTreePath();
}

void ICommand::AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words) const
{
    m_pMainWindow->AddAutoCompleteWords(lang, std::move(words));
}

void ICommand::AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words) const
{
    m_pMainWindow->AddAutoCompleteWords(lang, words);
}

void ICommand::AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words) const
{
    m_pMainWindow->AddAutoCompleteWords(docID, std::move(words));
}

void ICommand::AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words) const
{
    m_pMainWindow->AddAutoCompleteWords(docID, words);
}

sptr_t ICommand::GetCurrentLineNumber() const
{
    return m_pMainWindow->m_editor.GetCurrentLineNumber();
}

int ICommand::GetWidthToLeft(int idx)
{
    return m_pMainWindow->m_statusBar.GetWidthToLeft(idx);
}

int ICommand::GetStatusBarHeight()
{
    return m_pMainWindow->m_statusBar.GetHeight();
}
