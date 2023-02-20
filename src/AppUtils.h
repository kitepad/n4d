// This file is part of BowPad.
//
// Copyright (C) 2013-2014, 2016-2017, 2020-2021 - Stefan Kueng
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
#include <string>
#include <memory>           // std::unique_ptr, std::make_unique
#include <string>           // std::wstring
#include <vector>           // std::vector

class CAppUtils
{
public:
    CAppUtils();
    ~CAppUtils();

    static std::vector<std::wstring> GetMultiStringValue(const HKEY hKey, const std::wstring& valueName);
    static LSTATUS SetMultiStringValue(const HKEY hKey, const std::wstring& valueName, const std::vector<std::wstring>& data);
    static std::wstring             GetDataPath(HMODULE hMod = nullptr);
    static std::wstring             GetSessionID();
    static bool                     FailedShowMessage(HRESULT hr);
    static bool                     TryParse(const wchar_t* s, int& result, bool emptyOk = false, int def = 0, int base = 10);
    static bool                     TryParse(const wchar_t* s, unsigned long& result, bool emptyOk = false, unsigned long def = 0, int base = 10);
    static const char*              GetResourceData(const wchar_t * resName, int id, DWORD& resLen);
};
