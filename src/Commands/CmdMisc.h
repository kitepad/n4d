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
#include "CommandPaletteDlg.h"
#include "Theme.h"

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

class CCmdConfigStyle : public ICommand
{
public:
    CCmdConfigStyle(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdConfigStyle() = default;

    bool Execute() override
    {
        SendMessage(GetHwnd(), WM_COMMAND, GetCmdId(), 0);
        return true;
    }

    UINT GetCmdId() override { return cmdConfigStyle; }
};

class CCmdSelectLexerDlg : public CDialogWithFilterableList
{
public:
    CCmdSelectLexerDlg(void* obj);
    CCmdSelectLexerDlg() = default;
    
    UINT GetCmdId() override { return cmdSelectLexer; }

protected:
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx) 
    {
        DrawText(hdc, m_results[idx].text1.c_str(), -1, rc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
};

class CCmdSelectEncodingDlg : public CDialogWithFilterableList
{
public:
    CCmdSelectEncodingDlg(void* obj);
    CCmdSelectEncodingDlg() = default;

    UINT GetCmdId() override { return cmdSelectEncoding; }

protected:
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx)
    {
        DrawText(hdc, m_results[idx].text1.c_str(), -1, rc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
};

class CCmdSelectLexer : public ICommand
{
public:
    CCmdSelectLexer(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdSelectLexer();

    bool Execute() override;

    UINT GetCmdId() override { return cmdSelectLexer; }
};

class CCmdSelectEncoding : public ICommand
{
public:
    CCmdSelectEncoding(void* obj)
        : ICommand(obj)
    {
    }
    ~CCmdSelectEncoding();

    bool Execute() override;

    UINT GetCmdId() override { return cmdSelectEncoding; }
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
        auto val = CIniSettings::Instance().GetInt64(L"View", L"d2d", 1) ? 0 : 1;
        CIniSettings::Instance().SetInt64(L"View", L"d2d", val);
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
        auto val = CIniSettings::Instance().GetInt64(L"View", L"bracehighlighttext", 1) ? 0 : 1;
        CIniSettings::Instance().SetInt64(L"View", L"bracehighlighttext", val);
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
        auto val = CIniSettings::Instance().GetInt64(L"View", L"caretlineframe", 1) ? 0 : 1;
        CIniSettings::Instance().SetInt64(L"View", L"caretlineframe", val);
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

//class CCmdScrollStyle : public ICommand
//{
//public:
//    CCmdScrollStyle(void* obj)
//        : ICommand(obj)
//    {
//    }
//
//    ~CCmdScrollStyle() = default;
//
//    bool Execute() override
//    {
//        auto val = CIniSettings::Instance().GetInt64(L"View", L"scrollstyle", 1) ? 0 : 1;
//        CIniSettings::Instance().SetInt64(L"View", L"scrollstyle", val);
//        return true;
//    }
//
//    UINT GetCmdId() override { return cmdScrollStyle; }
//};
