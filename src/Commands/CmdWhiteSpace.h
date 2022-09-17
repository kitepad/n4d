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

class CCmdWhiteSpace : public ICommand
{
public:

    CCmdWhiteSpace(void * obj);

    ~CCmdWhiteSpace() = default;

    bool Execute() override;

    void AfterInit() override;

    UINT GetCmdId() override { return cmdWhiteSpace; }
};

class CCmdTabSize : public CDialog, public ICommand
{
public:
    CCmdTabSize(void* obj)
        : ICommand(obj)
    {
    }
    virtual ~CCmdTabSize() = default;
    bool Execute() override;

    UINT GetCmdId() override { return cmdTabSize; }
    void AfterInit() override;

protected:
    LRESULT CALLBACK DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    LRESULT          DoCommand(int id, int msg);
};

//class CCmdTabSize : public ICommand
//{
//public:
//
//    CCmdTabSize(void * obj);
//
//    ~CCmdTabSize() = default;
//
//    bool Execute() override;
//
//    UINT GetCmdId() override { return cmdTabSize; }
//    void AfterInit() override;
//};

class CCmdUseTabs : public ICommand
{
public:

    CCmdUseTabs(void * obj);

    ~CCmdUseTabs() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdUseTabs; }
};
