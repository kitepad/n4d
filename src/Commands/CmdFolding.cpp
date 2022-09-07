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

#include "stdafx.h"
#include "CmdFolding.h"
#include "ResString.h"
#include "StringUtils.h"
#include "UnicodeUtils.h"

#include <functional>

namespace
{
int           g_marginWidth                     = 16;
const wchar_t ShowFoldingMarginSettingSection[] = DEFAULTS_SECTION;
const wchar_t ShowFoldingMarginSettingName[]    = L"ShowFoldingMargin";

class FoldLevelStack
{
public:
    int                  levelCount = 0; // 1-based level number
    Scintilla::FoldLevel levelStack[12]{};

    void push(Scintilla::FoldLevel level)
    {
        while (levelCount != 0 && level <= levelStack[levelCount - 1])
        {
            --levelCount;
        }
        if (levelCount > (_countof(levelStack) - 1))
            return;
        levelStack[levelCount++] = level;
    }
};
} // namespace


CCmdFoldAll::CCmdFoldAll(void* obj)
    : ICommand(obj)
{
}

bool CCmdFoldAll::Execute()
{
    Scintilla().FoldAll(Scintilla::FoldAction::Toggle);
    return true;
}

CCmdFoldCurrent::CCmdFoldCurrent(void* obj)
    : ICommand(obj)
{
}

bool CCmdFoldCurrent::Execute()
{
    Scintilla().ToggleFold(GetCurrentLineNumber());
    return true;
}


CCmdFoldingMargin::CCmdFoldingMargin(void* obj)
    : ICommand(obj)
{
    g_marginWidth = Scintilla().MarginWidthN(SC_MARGIN_BACK);

    if (g_marginWidth == -1 || g_marginWidth == 0)
        g_marginWidth = 16;
}

bool CCmdFoldingMargin::Execute()
{
    bool isOn       = Scintilla().MarginWidthN(SC_MARGIN_BACK) > 0;
    Scintilla().SetMarginWidthN(SC_MARGIN_BACK, isOn ? 0 : g_marginWidth);
    SetInt64(ShowFoldingMarginSettingSection, ShowFoldingMarginSettingName, isOn ? 0 : 1);
    return true;
}