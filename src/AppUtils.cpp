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
#include "stdafx.h"
#include "AppUtils.h"
#include "StringUtils.h"
#include "PathUtils.h"
#include "SmartHandle.h"
#include <fstream>

CAppUtils::CAppUtils()
{
}


CAppUtils::~CAppUtils()
{
}

std::wstring CAppUtils::GetDataPath(HMODULE hMod)
{
    static std::wstring dataPath;
    if (dataPath.empty())
    {
        // in case BowPad was installed with the installer, there's a registry
        // entry made by the installer.
        HKEY subKey = nullptr;
        LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\BowPad", 0, KEY_READ, &subKey);
        if (result != ERROR_SUCCESS)
        {
            dataPath = CPathUtils::GetLongPathname(CPathUtils::GetModuleDir(hMod));
        }
        else
        {
            RegCloseKey(subKey);
            // BowPad is installed: we must not store the application data
            // in the same directory as the exe is but in %APPDATA%\BowPad instead
            PWSTR outPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &outPath)))
            {
                dataPath = outPath;
                CoTaskMemFree(outPath);
                dataPath += L"\\BowPad";
                dataPath = CPathUtils::GetLongPathname(dataPath);
                CreateDirectory(dataPath.c_str(), nullptr);
            }
        }
    }
    return dataPath;
}

std::wstring CAppUtils::GetSessionID()
{
    std::wstring t;
    CAutoGeneralHandle token;
    BOOL result = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.GetPointer());
    if(result)
    {
        DWORD len = 0;
        GetTokenInformation(token, TokenStatistics, nullptr, 0, &len);
        if (len >= sizeof (TOKEN_STATISTICS))
        {
            auto data = std::make_unique<BYTE[]>(len);
            GetTokenInformation(token, TokenStatistics, data.get(), len, &len);
            LUID uid = reinterpret_cast<PTOKEN_STATISTICS>(data.get())->AuthenticationId;
            t        = CStringUtils::Format(L"-%08x%08x", uid.HighPart, uid.LowPart);
        }
    }
    return t;
}

bool CAppUtils::FailedShowMessage(HRESULT hr)
{
    if (FAILED(hr))
    {
        _com_error err(hr);
        MessageBox(nullptr, L"BowPad", err.ErrorMessage(), MB_ICONERROR);
        return true;
    }
    return false;
}

bool CAppUtils::TryParse(const wchar_t* s, int& result, bool emptyOk, int def, int base)
{
    // At the time of writing _wtoi doesn't appear to set errno at all,
    // despite what the documentation suggests.
    // Using std::stoi isn't ideal as exceptions aren't really wanted here,
    // and they cloud the output window in VS with often unwanted exception
    // info for exceptions that are handled so aren't of interest.
    // However, for the uses intended so far exceptions shouldn't be thrown
    // so hopefully this will not be a problem.
    if (!*s)
    {
        // Don't even try to convert empty strings.
        // Success or failure is determined by the caller.
        result = def;
        return emptyOk;
    }
    try
    {
        result  = std::stoi(s, nullptr, base);
    }
    catch (const std::invalid_argument& /*ex*/)
    {
        result = def;
        return false;
    }
    return true;
}

bool CAppUtils::TryParse(const wchar_t* s, unsigned long & result, bool emptyOk, unsigned long def, int base)
{
    if (!*s)
    {
        result = def;
        return emptyOk;
    }
    try
    {
        result = std::stoi(s, nullptr, base);
    }
    catch (const std::invalid_argument& /*ex*/)
    {
        result = def;
        return false;
    }
    return true;
}

const char* CAppUtils::GetResourceData(const wchar_t * resName, int id, DWORD& resLen)
{
    resLen = 0;
    auto hResource = FindResource(nullptr, MAKEINTRESOURCE(id), resName);
    if (!hResource)
        return nullptr;
    auto hResourceLoaded = LoadResource(nullptr, hResource);
    if (!hResourceLoaded)
        return nullptr;
    auto lpResLock = static_cast<const char*>(LockResource(hResourceLoaded));
    resLen         = SizeofResource(nullptr, hResource);
    return lpResLock;
}

