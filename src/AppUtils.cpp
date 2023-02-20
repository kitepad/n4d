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

LSTATUS CAppUtils::SetMultiStringValue(const HKEY hKey,const std::wstring& valueName,const std::vector<std::wstring>& data)
{
    // First, we have to build a double-NUL-terminated multi-string from the input data
    std::vector<wchar_t> multiString;// = BuildMultiString(data);

    // Special case of the empty multi-string
    if (data.empty())
    {
        // Build a vector containing just two NULs
        multiString = std::vector<wchar_t>(2, L'\0');
    }

    // Get the total length in wchar_ts of the multi-string
    size_t totalLen = 0;
    for (const auto& s : data)
    {
        // Add one to current string's length for the terminating NUL
        totalLen += (s.length() + 1);
    }

    // Add one for the last NUL terminator (making the whole structure double-NUL terminated)
    totalLen++;

    // Reserve room in the vector to speed up the following insertion loop
    multiString.reserve(totalLen);

    // Copy the single strings into the multi-string
    for (const auto& s : data)
    {
        if (!s.empty())
        {
            // Copy current string's content
            multiString.insert(multiString.end(), s.begin(), s.end());
        }

        // Don't forget to NUL-terminate the current string
        // (or just insert L'\0' for empty strings)
        multiString.emplace_back(L'\0');
    }

    // Add the last NUL-terminator
    multiString.emplace_back(L'\0');

    // Total size, in bytes, of the whole multi-string structure
    const DWORD dataSize = static_cast<DWORD>(multiString.size() * sizeof(wchar_t));

    LSTATUS retCode = ::RegSetValueExW(
        hKey,
        valueName.c_str(),
        0, // reserved
        REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(multiString.data()),
        dataSize
    );

    return retCode; //ERROR_SUCESS
}

std::vector<std::wstring> CAppUtils::GetMultiStringValue(const HKEY hKey, const std::wstring& valueName)
{
    // Request the size of the multi-string, in bytes
    DWORD dataSize = 0;
    constexpr DWORD flags = RRF_RT_REG_MULTI_SZ;
    LSTATUS retCode = ::RegGetValueW(
        hKey,
        nullptr,    // no subkey
        valueName.c_str(),
        flags,
        nullptr,    // type not required
        nullptr,    // output buffer not needed now
        &dataSize
    );

    std::vector<std::wstring> result;

    if (retCode == ERROR_SUCCESS)
    {
        std::vector<wchar_t> data(dataSize / sizeof(wchar_t), L' ');

        // Read the multi-string from the registry into the vector object
        retCode = ::RegGetValueW(
            hKey,
            nullptr,    // no subkey
            valueName.c_str(),
            flags,
            nullptr,    // no type required
            data.data(),   // output buffer
            &dataSize
        );
        if (retCode == ERROR_SUCCESS)
        {
            // Resize vector to the actual size returned by GetRegValue.
            // Note that the vector is a vector of wchar_ts, instead the size returned by GetRegValue
            // is in bytes, so we have to scale from bytes to wchar_t count.
            data.resize(dataSize / sizeof(wchar_t));

            // Convert the double-null-terminated string structure to a vector<wstring>,
            // and return that back to the caller
            //result = ParseMultiString(data);

            //std::vector<std::wstring> result;

            const wchar_t* currStringPtr = data.data();
            const wchar_t* const endPtr = data.data() + data.size() - 1;

            while (currStringPtr < endPtr)
            {
                // Current string is NUL-terminated, so get its length calling wcslen
                const size_t currStringLength = wcslen(currStringPtr);

                // Add current string to the result vector
                if (currStringLength > 0)
                {
                    result.emplace_back(currStringPtr, currStringLength);
                }
                else
                {
                    // Insert empty strings, as well
                    result.emplace_back(std::wstring{});
                }

                // Move to the next string, skipping the terminating NUL
                currStringPtr += currStringLength + 1;
            }

        }
    }

    return result;
}