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
#include "BaseWindow.h"
#include "RichStatusBar.h"
#include "DocumentManager.h"
#include "ScintillaWnd.h"
#include "FileTree.h"
#include "ProgressBar.h"
#include "CustomTooltip.h"
#include "AutoComplete.h"

#include <list>

constexpr int SAVED_DOC         = 0;
constexpr int UNSAVED_DOC       = 1;
constexpr int READONLY_DOC      = 2;
constexpr int ITEM_XPADDING     = 24;
constexpr int ITEM_YPADDING     = 2;
constexpr int STATUSBAR_DOC_TYPE     = 0;
constexpr int STATUSBAR_UNICODE_TYPE = 1;
constexpr int STATUSBAR_EOL_FORMAT   = 2;
constexpr int STATUSBAR_R2L          = 3;
constexpr int STATUSBAR_TYPING_MODE  = 7;
constexpr int STATUSBAR_TABSPACE     = 8;
constexpr int STATUSBAR_CUR_POS      = 4;
constexpr int STATUSBAR_SEL          = 5;
constexpr int STATUSBAR_ZOOM         = 9;

enum class TitlebarRect
{
    None, 
    System,
    Minimize,
    Maximize,
    Close,
//    Theme,
    LeftRoller,
    RightRoller,
    Tabs,
    Text,
    ShowMore,
};

struct TitlebarRects
{
    RECT total;
    RECT system;
    RECT text;
    RECT minimize;
    RECT maximize;
    RECT close;
    RECT leftRoller;
    RECT rightRoller;
    RECT tabs;
    RECT showMore;
};

enum class ResponseToOutsideModifiedFile
{
    Cancel,
    Reload,
    KeepOurChanges
};

enum class ResponseToCloseTab
{
    StayOpen,
    SaveAndClose,
    CloseWithoutSaving
};

struct ItemMetrics
{
    std::wstring name;
    std::wstring path;
    int     width  = 0;
    int     height = 0;
    int     state = 0;
    DocID   id;
};

struct RECENT
{
    std::wstring path;
    bool         isFolder = false; //true: folder, false: file
};

class CAboutDlg : public CDialog
{
public:
    CAboutDlg(HWND hParent);
    ~CAboutDlg();

protected:
    LRESULT CALLBACK        DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};

class CMainWindow : public CWindow
{
    friend class ICommand;
    friend class CAutoComplete;
    friend class CAutoCompleteConfigDlg;
    friend class CFileTree;

public:
    CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = nullptr);
    virtual ~CMainWindow();

    bool RegisterAndCreateWindow();
    bool Initialize();
    
    TitlebarRects GetTitlebarRects(bool isInvalidate = FALSE);
    TitlebarRect GetHoveredRect();
    int    GetItemCount() const;
    
    std::wstring   GetTabTitle(int index) const;
    void         SetTabTitle(int index, std::wstring title);

    // Load/Reload functions
    int  OpenFile(const std::wstring& file, unsigned int openFlags);
    bool OpenFileAs(const std::wstring& tempPath, const std::wstring& realpath, bool bModified);
    bool ReloadTab(int tab, int encoding, bool dueToOutsideChanges = false);

    bool         SaveCurrentTab(bool bSaveAs = false);
    bool         SaveDoc(DocID docID, bool bSaveAs = false);
    bool         SaveDoc(DocID docID, const std::wstring& path);
    void         EnsureAtLeastOneTab();
    void         GoToLine(size_t line);
    bool         CloseTab(int tab, bool force = false, bool quitting = false);
    bool         CloseAllTabs(bool quitting = false);
    void         SetFileToOpen(const std::wstring& path, size_t line = static_cast<size_t>(-1));
    void         SetInsertionIndex(int index) { m_insertionIndex = index; }
    std::wstring GetNewTabName();
    void         ShowFileTree(bool bShow);
    bool         IsFileTreeShown() const { return m_fileTreeVisible; }
    std::wstring GetFileTreePath() const { return m_fileTree.GetPath(); }
    void         FileTreeBlockRefresh(bool bBlock) { m_fileTree.BlockRefresh(bBlock); }
    void         SetFileTreeWidth(int width);
    void         IndentToLastLine() const;
    
    void AddAutoCompleteWords(const std::string& lang, std::map<std::string, AutoCompleteType>&& words);
    void AddAutoCompleteWords(const std::string& lang, const std::map<std::string, AutoCompleteType>& words);
    void AddAutoCompleteWords(const DocID& docID, std::map<std::string, AutoCompleteType>&& words);
    void AddAutoCompleteWords(const DocID& docID, const std::map<std::string, AutoCompleteType>& words);
    BOOL                HasActiveDocument();
    const CDocument&           GetActiveDocument() const;

protected:
    /// the message handler for this window
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT DoCommand(WPARAM wParam, LPARAM lParam);

private:
    std::wstring        GetWindowClassName() const;
    static std::wstring GetAppName();
    HWND                FindAppMainWindow(HWND hStartWnd, bool* isThisInstance = nullptr) const;
    void                ResizeChildWindows();
    void                UpdateStatusBar(bool bEverything);
    void                AddHotSpots() const;

    bool                             AskToCreateNonExistingFile(const std::wstring& path) const;
    bool                             AskToReload(const CDocument& doc) const;
    ResponseToOutsideModifiedFile    AskToReloadOutsideModifiedFile(const CDocument& doc) const;
    bool                             AskAboutOutsideDeletedFile(const CDocument& doc) const;
    bool                             AskToRemoveReadOnlyAttribute() const;
    ResponseToCloseTab               AskToCloseTab() const;
    void                             UpdateTab(DocID docID);
    void                             CloseAllButThis(int idx = -1);
    void                             EnsureNewLineAtEnd(const CDocument& doc) const;
    void                             OpenNewTab();
    void                             PasteHistory();
    void                             About() const;
    bool                             HasOutsideChangesOccurred() const;
    void                             CheckForOutsideChanges();
    void                             UpdateCaptionBar();
    bool                             HandleOutsideDeletedFile(int docID);
    void                             HandleCreate(HWND hwnd);
    void                             HandleAfterInit();
    void                             HandleDropFiles(HDROP hDrop);
    void                             HandleTabDroppedOutside(int tab, POINT pt);
    void                             HandleTabChange();
    void                             HandleTabChanging();
    void                             HandleClipboardUpdate();
    void                             HandleGetDispInfo(int tab, LPNMTTDISPINFO lpNmtdi);
    void                             HandleTreePath(const std::wstring& path, bool isDir, bool isDot);
    static std::vector<std::wstring> GetFileListFromGlobPath(const std::wstring& path);

    // Scintilla events.
    void        HandleDwellStart(const SCNotification& scn, bool start);
    LPARAM      HandleMouseMsg(const SCNotification& scn);
    bool        OpenUrlAtPos(Sci_Position pos);
    void        HandleCopyDataCommandLine(const COPYDATASTRUCT& cds);
    bool        HandleCopyDataMoveTab(const COPYDATASTRUCT& cds);
    void        HandleWriteProtectedEdit();
    void        HandleSavePoint(const SCNotification& scn);
    void        HandleUpdateUI(const SCNotification& scn);
    void        HandleAutoIndent(const SCNotification& scn) const;
    void        HandleEditorContextMenu(LPARAM lParam);
    void        HandleTabContextMenu(LPARAM lParam);
    void        HandleStatusBarEOLFormat();
    void        HandleStatusBarZoom();
    void        HandleStatusBar(WPARAM wParam, LPARAM lParam);
    LRESULT     HandleEditorEvents(const NMHDR& nmHdr, WPARAM wParam, LPARAM lParam);
    LRESULT     HandleFileTreeEvents(const NMHDR& nmHdr, WPARAM wParam, LPARAM lParam);
    LRESULT     HandleQuickbarCustomDraw(const LPNMTBCUSTOMDRAW pCustomDraw);
    int         GetZoomPC() const;
    void        SetZoomPC(int zoomPC) const;
    void        OpenFiles(const std::vector<std::wstring>& paths);
    void        BlockAllUIUpdates(bool block);
    int         UnblockUI();
    void        ReBlockUI(int blockCount);
    void        ShowProgressCtrl(UINT delay);
    void        HideProgressCtrl();
    void        SetProgress(DWORD32 pos, DWORD32 end);
    void        SetTheme(bool theme);
    void        RefreshAnnotations();
    void        ShowSystemMenu();
    void        UpdateTitlebarRects();
    void        DrawTabs(HDC dc);
    int         AdjustItemWidth(int width);
    void        DrawTitlebar(HDC dc);
    int         GetSelected() const;
    void        SetSelected(int idx);
    DocID       GetIDFromIndex(int index) const;
    int         GetIndexFromID(DocID id) const;
    int         InsertAtEnd(const wchar_t* subTabName);
    int         GetCurrentTabIndex() const;
    DocID       GetCurrentTabId() const;
    void        OpenFolder(std::wstring path);
    void        ShowAutoComplete();
    void        AddToRecents(std::wstring path);
    void        SaveRecents();
    //void        ShowRecents();

private:
    CRichStatusBar                                  m_statusBar;
    CScintillaWnd                                   m_editor;
    CFileTree                                       m_fileTree;
    CProgressBar                                    m_progressBar;
    CCustomToolTip                                  m_custToolTip;
    //CCustomToolTip                                  m_tabTip;
    //CCustomToolTip                                  m_quickbarTip;
    int                                             m_treeWidth;
    bool                                            m_bDragging;
    POINT                                           m_oldPt;
    bool                                            m_fileTreeVisible;
    CDocumentManager                                m_docManager;
    std::unique_ptr<wchar_t[]>                      m_tooltipBuffer;
    std::list<std::wstring>                         m_clipboardHistory;
    std::map<std::wstring, size_t>                  m_pathsToOpen;
    long                                            m_initLine;
    int                                             m_insertionIndex;
    bool                                            m_inMenuLoop;
    CScintillaWnd                                   m_scratchEditor;
    int                                             m_blockCount;
    CAutoComplete                                   m_autoCompleter;
    Sci_Position                                    m_dwellStartPos;
    bool                                            m_bBlockAutoIndent;
    sptr_t                                          m_lastCheckedLine;

    TitlebarRect m_hoveredRect;    // Current hovered button.
    TitlebarRect m_oldHoveredRect; // Hovered button when left mouse button pressed.
    TitlebarRects  m_allRects;

    // Begin for tab bar drawing and handling
    int                         m_newCount;
    int                         m_firstVisibleItemIdx = 0;
    int                         m_lastVisibleItemIdx  = 0;
    int                         m_selected            = 0;
    int                         m_hoveredItemIdx      = -1;
    RECT                        m_hoveredItemRect     = {0,0,0,0};
    int                         m_oldHoveredTabItem   = -1;
    int                         m_tabID               = 0;
    int                         m_fontSize            = 12;
    bool                        m_bIsAfterInit        = false;
    std::wstring                m_fontName            = L"Tahoma";
    std::wstring                m_titleText           = L"";
    std::vector<ItemMetrics>    m_allTabs;
    std::vector<std::wstring>   m_recents;
    int                         m_tipIdx = -1;        
    ULONG_PTR                   m_gdiplusToken;
    HWND       m_quickbar;
    HIMAGELIST m_quickbarImages = nullptr;
};
