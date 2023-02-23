// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2021 - Stefan Kueng
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
#include "ICommand.h"
#include "Theme.h"

class CmdPalData
{
public:
    UINT         cmdId = 0;
    std::wstring command;
    std::wstring description;
    std::wstring shortcut;
};

struct CListItem
{
    CListItem(UINT codepage, bool bom, const std::wstring& txt1, const std::wstring& txt2 = L"", const std::wstring& txt3 = L"")
        : uintVal(codepage)
        , boolVal(bom)
        , text1(txt1)
        , text2(txt2)
        , text3(txt3)
    {
    }

    CListItem() {}

    UINT         uintVal = 0;
    bool         boolVal = false;
    std::wstring text1 = L"";
    std::wstring text2 = L"";
    std::wstring text3 = L"";
};

class CDialogWithFilterableList : public CDialog
    , public ICommand
{
public:
    CDialogWithFilterableList(void* obj);
    CDialogWithFilterableList() = default;

    bool Execute() override
    {
        ShowModeless(g_hRes, IDD_COMMANDPALETTE, GetHwnd(), FALSE);

        constexpr UINT flags = SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOCOPYBITS;
        RECT  rc;
        GetClientRect(GetHwnd(), &rc);
        POINT pt((rc.right - rc.left - 720) / 2, CTheme::CurrentTheme().tabHeight + CTheme::CurrentTheme().titleHeight);
        ClientToScreen(GetHwnd(), &pt);
        SetWindowPos(*this, nullptr, pt.x, pt.y, 720, 400, flags);

        return true;
    }

    virtual UINT GetCmdId() { return 0; };

    void ClearFilterText();

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int code);

    virtual void OnOK() = 0;
    virtual UINT GetFilterCUE() = 0;
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx) = 0;

    virtual bool                    IsFiltered(std::wstring sFilterText, CListItem item);
    virtual void                    InitResultsList() const;
    virtual void                    FillResults(bool force);
    virtual LRESULT                 DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    virtual LRESULT                 GetListItemDispInfo(NMLVDISPINFO* pDispInfo);
    virtual void                    ResizeChildren();
    static LRESULT CALLBACK EditSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK ListViewSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    // private:
    HWND m_hFilter;
    HWND m_hResults;
    std::vector<CListItem> m_results;
    std::vector<CListItem> m_allResults;
    ICommand* m_pCmd;
};

class CCmdCommandPalette : public CDialogWithFilterableList
{
public:
    CCmdCommandPalette(void* obj);
    CCmdCommandPalette() = default;
    UINT GetCmdId() override { return cmdCommandPalette; }

protected:
    LRESULT DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);
    virtual bool IsFiltered(std::wstring sFilterText, CListItem item);
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx);
};

enum class ColorButtonDialogResult
{
    None,
    Cancel,
    Ok
};

class CColorButton
{
public:
    CColorButton();
    virtual ~CColorButton();

    BOOL ConvertToColorButton(HWND hwndParent, UINT uiCtlId);

    void SetColor(COLORREF clr);
    COLORREF GetColor() const { return m_color; }
    COLORREF GetLastColor() const { return m_lastColor; }
    ColorButtonDialogResult GetDialogResult() const { return m_dialogResult; }
    void Reset()
    {
        m_hasLastColor = false;
    }

protected:

private:
    WNDPROC     m_pfnOrigCtlProc = nullptr;
    COLORREF    m_color = 0;
    HWND        m_hwnd = nullptr;
    UINT        m_ctlId = 0;
    bool        m_hasLastColor = false;
    COLORREF    m_lastColor = 0;
    COLORREF    m_hoverColor = 0;
    std::vector<HWND> m_colorEdits;
    ColorButtonDialogResult m_dialogResult = ColorButtonDialogResult::None;

private:
    bool ConvertToColorButton(HWND hwndCtl);
    inline static CColorButton* GetObjectFromWindow(HWND hWnd)
    {
        return reinterpret_cast<CColorButton*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }
    static LRESULT CALLBACK _ColorButtonProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static UINT_PTR CALLBACK CCHookProc(
        _In_ HWND   hDlg,
        _In_ UINT   uiMsg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam
    );
};

class CCmdDelete : public ICommand
{
public:
    CCmdDelete(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdDelete() = default;

    bool Execute() override
    {
        Scintilla().Clear();
        return true;
    }

    UINT GetCmdId() override { return cmdDelete; }
};

class CCmdSelectAll : public ICommand
{
public:
    CCmdSelectAll(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdSelectAll() = default;

    bool Execute() override
    {
        Scintilla().SelectAll();
        return true;
    }

    UINT GetCmdId() override { return cmdSelectAll; }
};

class CCmdGotoBrace : public ICommand
{
public:
    CCmdGotoBrace(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdGotoBrace() = default;

    bool Execute() override
    {
        GotoBrace();
        return true;
    }

    UINT GetCmdId() override { return cmdGotoBrace; }
};

class CCmdToggleTheme : public ICommand
{
public:
    CCmdToggleTheme(void* obj);
    ~CCmdToggleTheme() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdToggleTheme; }
private:
};

class CCmdConfigShortcuts : public ICommand
{
public:
    CCmdConfigShortcuts(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdConfigShortcuts() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdConfigShortcuts; }
};

class CCmdAutoBraces : public ICommand
{
public:
    CCmdAutoBraces(void* obj);

    ~CCmdAutoBraces() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdAutoBraces; }
};

class CCmdViewFileTree : public ICommand
{
public:
    CCmdViewFileTree(void* obj);

    ~CCmdViewFileTree() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdFileTree; }
};

class CCmdWriteProtect : public ICommand
{
public:
    CCmdWriteProtect(void* obj);
    ~CCmdWriteProtect() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdWriteProtect; }
};

class CCmdAutoComplete : public ICommand
{
public:
    CCmdAutoComplete(void* obj);

    ~CCmdAutoComplete() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdAutocomplete; }
};

class CCmdConfigStyle : public CDialog, public ICommand
{
public:
    CCmdConfigStyle(void* obj);
    ~CCmdConfigStyle() = default;

    bool Execute() override;
    UINT GetCmdId() override { return cmdConfigStyle; }
protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
    void OnSize(HWND hwnd, UINT /*state*/, int cx, int cy);
    BOOL OnInitDialog(HWND hwnd, HWND /*hwndFocus*/, LPARAM /*lParam*/);
    void OnCommand(HWND /*hwnd*/, int id, HWND /*hwndCtl*/, UINT msg);
    void InitializeLexerConfiguration();
    void SelectStyle(int style);
    std::vector<std::wstring> m_fonts;
    CColorButton              m_fgColor;
    CColorButton              m_bkColor;
    HWND m_hForm;
    int  m_formWidth;
};

class CCmdSelectLexer : public CDialogWithFilterableList
{
public:
    CCmdSelectLexer(void* obj);
    ~CCmdSelectLexer();
    bool Execute() override;
    UINT GetCmdId() override { return cmdSelectLexer; }

protected:
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx) 
    {
        DrawText(hdc, m_results[idx].text1.c_str(), -1, rc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
};

class CCmdSelectEncoding : public CDialogWithFilterableList
{
public:
    CCmdSelectEncoding(void* obj);
    ~CCmdSelectEncoding();

    bool Execute() override;
    UINT GetCmdId() override { return cmdSelectEncoding; }

protected:
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx)
    {
        DrawText(hdc, m_results[idx].text1.c_str(), -1, rc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
};

class CCmdSelectTab : public CDialogWithFilterableList
{
public:
    CCmdSelectTab(void* obj);
    ~CCmdSelectTab();

    bool Execute() override;
    UINT GetCmdId() override { return cmdSelectTab; }

protected:
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx)
    {
        DrawText(hdc, m_results[idx].text1.c_str(), -1, rc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
};

class CCmdOpenRecent : public CDialogWithFilterableList
{
public:
    CCmdOpenRecent(void* obj);
    ~CCmdOpenRecent();

    bool Execute() override;
    UINT GetCmdId() override { return cmdOpenRecent; }

protected:
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx)
    {
        COLORREF oldColor = GetTextColor(hdc);
        std::wstring item = m_results[idx].text1;
        if (PathIsDirectory(item.c_str()))
            SetTextColor(hdc, RGB(240, 0, 0));
        DrawText(hdc, m_results[idx].text1.c_str(), -1, rc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        SetTextColor(hdc,oldColor);
    }
};

class CCmdEnableD2D : public ICommand
{
public:
    CCmdEnableD2D(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdEnableD2D() = default;

    bool Execute() override
    {
        auto val = GetInt64(DEFAULTS_SECTION, L"Direct2D") ? 0 : 1;
        SetInt64(DEFAULTS_SECTION, L"Direct2D", val);
        Scintilla().SetTechnology(val ? Scintilla::Technology::DirectWriteRetain : Scintilla::Technology::Default);
        return true;
    }

    UINT GetCmdId() override { return cmdEnableD2D; }
};

class CCmdHighlightBrace : public ICommand
{
public:
    CCmdHighlightBrace(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdHighlightBrace() = default;

    bool Execute() override
    {
        auto val = GetInt64(DEFAULTS_SECTION, L"HighlightBrace") ? 0 : 1;
        SetInt64(DEFAULTS_SECTION, L"HighlighBrace", val);
        return true;
    }

    UINT GetCmdId() override { return cmdHighlightBrace; }
};

class CCmdFrameCaretLine : public ICommand
{
public:
    CCmdFrameCaretLine(void* obj)
        : ICommand(obj)
    {
    }

    ~CCmdFrameCaretLine() = default;

    bool Execute() override
    {
        auto val = GetInt64(DEFAULTS_SECTION, L"CaretLineFrame") ? 0 : 1;
        SetInt64(DEFAULTS_SECTION, L"CaretLineFrame", val);
        CTheme&  theme      = CTheme::Instance();
        COLORREF themeColor = theme.GetThemeColor(RGB(0, 0, 0), true);
        auto     Rgba       = [](COLORREF c, int alpha) { return (c | (static_cast<DWORD>(alpha) << 24)); };

        if (val)
        {
            Scintilla().SetCaretLineFrame(1);
            Scintilla().SetElementColour(Scintilla::Element::CaretLineBack, Rgba(themeColor, 80));
            Scintilla().SetCaretLineLayer(Scintilla::Layer::UnderText);
        }
        else
        {
            if (theme.IsDarkTheme())
            {
                Scintilla().SetElementColour(Scintilla::Element::CaretLineBack, themeColor);
            }
            else
            {
                Scintilla().SetElementColour(Scintilla::Element::CaretLineBack, Rgba(themeColor, 75));
            }
        }

        return true;
    }

    UINT GetCmdId() override { return cmdFrameCaretLine; }
};