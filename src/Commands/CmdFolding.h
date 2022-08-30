// This file is part of BowPad.
//
// Copyright (C) 2015-2017, 2020-2021 - Stefan Kueng
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
//#include "commands.h" //#include "BowPadUI.h"

class CCmdFoldAll : public ICommand
{
public:
    CCmdFoldAll(void* obj);
    ~CCmdFoldAll() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdFoldAll; }
    //void AfterInit() override;
};

class CCmdFoldCurrent : public ICommand
{
public:
    CCmdFoldCurrent(void* obj);
    ~CCmdFoldCurrent() = default;

    bool Execute() override;

    UINT GetCmdId() override { return cmdFoldCurrent; }
    // void AfterInit() override;
};

//class CCmdFoldLevel : public ICommand
//{
//public:
//    CCmdFoldLevel(UINT customId, void* obj);
//    ~CCmdFoldLevel() = default;
//
//    bool Execute() override;
//    UINT GetCmdId() override { return m_customCmdId; }
//
//private:
//    UINT m_customId;
//    UINT m_customCmdId;
//};

//class CCmdInitFoldingMargin : public ICommand
//{
//public:
//    CCmdInitFoldingMargin(void* obj);
//    ~CCmdInitFoldingMargin() = default;
//
//    bool Execute() override;
//    UINT GetCmdId() override { return cmdInitFoldingMargin; }
//    void TabNotify(TBHDR* ptbHdr) override;
//    void ScintillaNotify(SCNotification* pScn) override;
//    //void AfterInit() override;
//};

class CCmdFoldingMargin : public ICommand
{
public:
    CCmdFoldingMargin(void* obj);
    ~CCmdFoldingMargin() = default;

    bool Execute() override;
    UINT GetCmdId() override { return cmdFoldingMargin; }
    //void AfterInit() override;
    //HRESULT IUICommandHandlerUpdateProperty(REFPROPERTYKEY key, const PROPVARIANT* /*pPropVarCurrentValue*/, PROPVARIANT* pPropVarNewValue) override;
};
