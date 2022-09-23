// This file is part of BowPad.
//
// Copyright (C) 2013-2022 - Stefan Kueng
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
#include "BaseWindow.h"
#include "Scintilla.h"
#include "Document.h"
#include "ScrollTool.h"
#include "AnimationManager.h"
#include "../ext/scintilla/include/ILexer.h"
#include "../ext/scintilla/include/ScintillaTypes.h"
#include "../ext/scintilla/include/ScintillaCall.h"
#include "coolscroll.h"
#include <map>
#include <vector>
#include <unordered_map>

class CPosData;

#define INDIC_SELECTION_MARK   (INDIC_CONTAINER + 1)
#define INDIC_TAGMATCH         (INDIC_CONTAINER + 2)
#define INDIC_TAGATTR          (INDIC_CONTAINER + 3)
#define INDIC_FINDTEXT_MARK    (INDIC_CONTAINER + 4)
#define INDIC_URLHOTSPOT       (INDIC_CONTAINER + 5)
#define INDIC_BRACEMATCH       (INDIC_CONTAINER + 6)
#define INDIC_MISSPELLED       (INDIC_CONTAINER + 7)
#define INDIC_MISSPELLED_DEL   (INDIC_CONTAINER + 8)
#define INDIC_SNIPPETPOS       (INDIC_CONTAINER + 9)
#define INDIC_REGEXCAPTURE     (INDIC_CONTAINER + 10)
#define INDIC_REGEXCAPTURE_END (INDIC_CONTAINER + 20)

constexpr int SC_MARGE_LINENUMBER = 0;
//constexpr int SC_MARGE_SYMBOL     = 1;
//constexpr int SC_MARGE_FOLDER     = 2;
constexpr int SC_MARGE_HISTORY = 1;
constexpr int SC_MARGE_SYMBOL = 2;
constexpr int SC_MARGE_FOLDER = 4;
constexpr int MARK_BOOKMARK       = 24;
constexpr int SCN_BP_MOUSEMSG     = 4000;
constexpr int DOCSCROLLTYPE_SELTEXT = 1;
constexpr int DOCSCROLLTYPE_BOOKMARK = 2;
constexpr int DOCSCROLLTYPE_SEARCHTEXT = 3;
constexpr int DOCSCROLLTYPE_END = 4;

struct LineColor
{
    inline LineColor(int type, size_t line) : type(type), line(line) {}
    // Sort by type then line.
    inline bool operator<(const LineColor& rhs) const
    {
        if (type != rhs.type)
            return type < rhs.type;
        else
            return line < rhs.line;
    }

    int type;
    size_t line;
};

enum class BraceMatch
{
    Braces,
    Highlight,
    Clear,
};

enum class SelectionHandling
{
    None,
    CurrentWordIfSelectionIsEmpty,
    CurrentWordIfSelectionIsEmptyAndSelect,
};

struct XmlMatchedTagsPos
{
    sptr_t tagOpenStart;
    sptr_t tagNameEnd;
    sptr_t tagOpenEnd;

    sptr_t tagCloseStart;
    sptr_t tagCloseEnd;
};

struct FindResult
{
    sptr_t start;
    sptr_t end;
    bool   success;
};

class LexerData;

class CDocScroll
{
    friend class CScintillaWnd;
public:
    CDocScroll();
    ~CDocScroll();

    void                        InitScintilla(CScintillaWnd* pScintilla);
    LRESULT CALLBACK            HandleCustomDraw(WPARAM wParam, NMCSBCUSTOMDRAW* pCustDraw);
    void                        SetTotalLines(size_t lines);
    void                        Clear(int type);
    void                        AddLineColor(int type, size_t line, COLORREF clr);
    void                        RemoveLine(int type, size_t line);
    void                        SetCurrentPos(size_t visibleLine, COLORREF clr) { m_curPosVisLine = visibleLine; m_curPosColor = clr; }
    void                        VisibleLinesChanged();
    bool                        IsDirty() const { return m_bDirty; }
private:
    void                        CalcLines();
    void                        AnimateFraction(AnimationVariable& animVar, double endVal);


    std::map<size_t, COLORREF>                  m_visibleLineColors[DOCSCROLLTYPE_END];
    std::map<LineColor, COLORREF>                m_lineColors;
    size_t                                      m_visibleLines;
    size_t                                      m_lines;
    size_t                                      m_curPosVisLine;
    COLORREF                                    m_curPosColor;
    CScintillaWnd*                              m_pScintilla;
    bool                                        m_bDirty;
    ULONG_PTR                                   m_gdiplusToken;

    AnimationVariable                           m_animVarHL;
    AnimationVariable                           m_animVarHR;
    AnimationVariable                           m_animVarHT;
    AnimationVariable                           m_animVarVL;
    AnimationVariable                           m_animVarVR;
    AnimationVariable                           m_animVarVT;
    bool                                        m_bHotHL;
    bool                                        m_bHotHR;
    bool                                        m_bHotHT;
    bool                                        m_bHotVL;
    bool                                        m_bHotVR;
    bool                                        m_bHotVT;
};

class CScintillaWnd : public CWindow
{
public:
    CScintillaWnd(HINSTANCE hInst);
    ~CScintillaWnd() override;

    bool                      Init(HINSTANCE hInst, HWND hParent, HWND hWndAttachTo = nullptr);
    bool                      InitScratch(HINSTANCE hInst);
    void                      StartupDone() { m_eraseBkgnd = false; }

    Scintilla::ScintillaCall& Scintilla() const { return m_scintilla; }

    void                      UpdateLineNumberWidth() const;
    void                      SaveCurrentPos(CPosData& pos);
    void                      RestoreCurrentPos(const CPosData& pos);
    void                      SetupLexerForLang(const std::string& lang) const;
    void                      MarginClick(SCNotification* pNotification);
    void                      SelectionUpdated() const;
    void                      MarkSelectedWord(bool clear, bool edit);
    void                      MatchBraces(BraceMatch what);
    void                      GotoBrace() const;
    void                      MatchTags() const;
    bool                      GetSelectedCount(sptr_t& selByte, sptr_t& selLine) const;
    void                      DocScrollClear(int type) { m_docScroll.Clear(type); }
    void                      DocScrollAddLineColor(int type, sptr_t line, COLORREF clr) { m_docScroll.AddLineColor(type, line, clr); }
    void                      DocScrollUpdate();
    void                      DocScrollRemoveLine(int type, sptr_t line) { m_docScroll.RemoveLine(type, line); }
    void                      MarkBookmarksInScrollbar();
    void                      GotoLine(sptr_t line);
    void                      Center(sptr_t posStart, sptr_t posEnd);
    void                      SetTabSettings(TabSpace ts) const;
    void                      SetReadDirection(Scintilla::Bidirectional rd) const;
    void                      SetEOLType(Scintilla::EndOfLine eolType) const;
    void                      AppendText(sptr_t len, const char* buf) const;
    std::string               GetLine(sptr_t line) const;
    std::string               GetTextRange(Sci_Position startPos, Sci_Position endPos) const;
    sptr_t                    FindText(const std::string& toFind, sptr_t startPos, sptr_t endPos) const;
    std::string               GetSelectedText(SelectionHandling handling) const;
    std::string               GetCurrentWord(bool select = false) const;
    std::string               GetCurrentLine() const;
    std::string               GetWordChars() const;
    std::string               GetWhitespaceChars() const;
    sptr_t                    GetSelTextMarkerCount() const { return m_selTextMarkerCount; }
    sptr_t                    GetCurrentLineNumber() const;
    void                      VisibleLinesChanged() { m_docScroll.VisibleLinesChanged(); }
    void                      EnableChangeHistory() const;
    static bool               IsXMLWhitespace(int ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

    LRESULT CALLBACK          HandleScrollbarCustomDraw(WPARAM wParam, NMCSBCUSTOMDRAW* pCustomDraw);
    void                      ReflectEvents(SCNotification* pScn);

protected:
    virtual LRESULT CALLBACK               WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    void                                   SetupDefaultStyles() const;
    void                                   SetupFoldingColors(COLORREF fore, COLORREF back, COLORREF backSel) const;

    bool                                   GetXmlMatchedTagsPos(XmlMatchedTagsPos& xmlTags) const;
    FindResult                             FindText(const char* text, sptr_t start, sptr_t end, Scintilla::FindOption flags) const;
    FindResult                             FindOpenTag(const std::string& tagName, sptr_t start, sptr_t end) const;
    sptr_t                                 FindCloseAngle(sptr_t startPosition, sptr_t endPosition) const;
    FindResult                             FindCloseTag(const std::string& tagName, sptr_t start, sptr_t end) const;
    std::vector<std::pair<sptr_t, sptr_t>> GetAttributesPos(sptr_t start, sptr_t end) const;
    bool                                   AutoBraces(WPARAM wParam) const;

    void                                   BookmarkAdd(sptr_t lineNo);
    void                                   BookmarkDelete(sptr_t lineNo);
    bool                                   IsBookmarkPresent(sptr_t lineNo) const;
    void                                   BookmarkToggle(sptr_t lineNo);

private:
    mutable Scintilla::ScintillaCall m_scintilla;
    CDocScroll                       m_docScroll;
    CScrollTool                      m_scrollTool;
    sptr_t                           m_selTextMarkerCount;
    bool                             m_bCursorShown;
    bool                             m_bScratch;
    bool                             m_eraseBkgnd;
    int                              m_cursorTimeout;
    bool                             m_bInFolderMargin;
    bool                             m_hasConsolas;
    sptr_t                           m_lineToScrollToAfterPaint;
    sptr_t                           m_wrapOffsetToScrollToAfterPaint;
    int                              m_lineToScrollToAfterPaintCounter;
    sptr_t                           m_lastMousePos;
    AnimationVariable                m_animVarGrayFore;
    AnimationVariable                m_animVarGrayBack;
    AnimationVariable                m_animVarGraySel;
    AnimationVariable                m_animVarGrayLineNr;
};
