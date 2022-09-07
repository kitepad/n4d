// This file is part of BowPad.
//
// Copyright (C) 2020-2021 - Stefan Kueng
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
#include "ICommand.h"
#include <vector>

class CmdPalData
{
public:
    UINT         cmdId           = 0;
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
    std::wstring text1   = L"";
    std::wstring text2   = L"";
    std::wstring text3   = L"";
};

class CDialogWithFilterableList : public CDialog
    , public ICommand
{
public:
    CDialogWithFilterableList(void* obj);
    CDialogWithFilterableList() = default;

    bool Execute() override
    {
        return true;
    }

    virtual UINT GetCmdId() { return 0; };

    void ClearFilterText();

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int code);
        
    virtual void OnOK()                                    = 0;
    virtual UINT GetFilterCUE()                            = 0;
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
    ICommand*              m_pCmd;
};

class CCommandPaletteDlg : public CDialogWithFilterableList
{
public:
    CCommandPaletteDlg(void* obj);
    CCommandPaletteDlg() = default;

    UINT GetCmdId() override { return 0; }

protected:
    LRESULT DoListNotify(LPNMITEMACTIVATE lpNMItemActivate);

    virtual bool IsFiltered(std::wstring sFilterText, CListItem item);
    virtual UINT GetFilterCUE();
    virtual void OnOK();
    virtual void DrawItemText(HDC hdc, LPRECT rc, int idx);
};
