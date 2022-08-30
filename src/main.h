// This file is part of BowPad.
//
// Copyright (C) 2013, 2020 - Stefan Kueng
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

#include "resource.h"

extern HINSTANCE g_hRes;
extern HINSTANCE g_hInst;
extern bool firstInstance;
extern IUIImagePtr g_emptyIcon;
extern bool g_useItemIcons;
extern HWND g_hMainWindow;
#define EMPTY_IMAGE (g_useItemIcons ? g_emptyIcon : nullptr)

#define DEFAULTS_SECTION      L"DEFAULTS"
#define BOOKMARKS_SECTION     L"BOOKMARKS"
#define SEARCHREPLACE_SECTION L"SEARCHREPLACE"
#define WINDOWPOS_SECTION     L"WINDOWPOS"

namespace
{
LPCWSTR GetString(LPCWSTR section, LPCWSTR key, LPCWSTR defaultVal = nullptr)
{
    return CIniSettings::Instance().GetString(section, key, defaultVal);
}
__int64 GetInt64(LPCWSTR section, LPCWSTR key, __int64 defaultVal = 1)
{
    return CIniSettings::Instance().GetInt64(section, key, defaultVal);
}

void SetInt64(LPCWSTR section, LPCWSTR key, __int64 value)
{
    CIniSettings::Instance().SetInt64(section, key, value);
}
void SetString(LPCWSTR section, LPCWSTR key, LPCWSTR value)
{
    CIniSettings::Instance().SetString(section, key, value);
}
} // namespace