﻿// This file is part of BowPad.
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
#include "ICommand.h"
#include "BaseDialog.h"
#include "StringUtils.h"
#include <Shobjidl.h>
#include <VersionHelpers.h>
#include "Registry.h"

class LaunchBase : public ICommand
{
public:
    LaunchBase(void* obj)
        : ICommand(obj)
    {
    }
    ~LaunchBase() = default;

protected:
    bool Launch(const std::wstring& cmdline) const;
};

class CCmdLaunchEdge : public LaunchBase
{
public:
    CCmdLaunchEdge(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchEdge() = default;

    bool Execute() override
    {
        // the Edge browser is not a normal executable but a Windows Store app, so it can't be started like
        // a normal exe file.
        // therefore we first try to find out the App ID of Edge (since I'm not sure if the ID won't change)
        // using the path in Windows\SystemApps. Then we start the browser via the ApplicationActivationMananager
        // object and pass the path of the file.
        // only if all of that fails, we fall back to the "microsoft-edge:" protocol handler - this handler
        // worked fine for all urls in early Win10, but unfortunately won't work in current Win10 versions for
        // file:/// urls anymore due to security reasons.
        std::wstring path = L"Microsoft.MicrosoftEdge_8wekyb3d8bbwe!MicrosoftEdge";
        // first try to get the ID of the Edge browser
        wchar_t pf[MAX_PATH];
        SHGetFolderPath(nullptr, CSIDL_WINDOWS, nullptr, SHGFP_TYPE_CURRENT, pf);
        std::wstring edgePath = pf;
        edgePath += L"\\SystemApps\\Microsoft.MicrosoftEdge*";
        WIN32_FIND_DATA fileData = {0};
        auto            hSearch  = FindFirstFile(edgePath.c_str(), &fileData);
        if (hSearch != INVALID_HANDLE_VALUE)
        {
            path = fileData.cFileName;
            path += L"!MicrosoftEdge";
            FindClose(hSearch);
        }
        IApplicationActivationManager* activationManager;
        auto                           hr        = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&activationManager));
        bool                           succeeded = false;
        if (SUCCEEDED(hr))
        {
            const auto&  doc     = GetActiveDocument();
            std::wstring tabPath = doc.m_path;
            SearchReplace(tabPath, L"\\", L"/");

            DWORD newProcessId;
            hr = activationManager->ActivateApplication(path.c_str(), (L"file:///" + tabPath).c_str(), AO_NONE, &newProcessId);
            if (SUCCEEDED(hr))
                succeeded = true;
        }
        if (!succeeded)
        {
            // get the shell execute command for the new edge chrome
            std::wstring cmd = CRegStdString(L"SOFTWARE\\Classes\\MSEdgeHTM\\shell\\open\\command\\", L"", false, HKEY_LOCAL_MACHINE);
            SearchReplace(cmd, L"%1", L"$(TAB_PATH)");
            return Launch(cmd);
        }
        return true;
    }
    UINT GetCmdId() override
    {
        return cmdLaunchEdge;
    }
};


class CCmdLaunchFirefox : public LaunchBase
{
public:
    CCmdLaunchFirefox(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchFirefox() = default;

    bool Execute() override { return Launch(L"firefox \"$(TAB_PATH)\""); }
    UINT GetCmdId() override { return cmdLaunchFirefox; }
};

class CCmdLaunchChrome : public LaunchBase
{
public:
    CCmdLaunchChrome(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchChrome() = default;

    bool Execute() override { return Launch(L"chrome \"$(TAB_PATH)\""); }
    UINT GetCmdId() override { return cmdLaunchChrome; }
};

class CCmdLaunchConsole : public LaunchBase
{
public:
    CCmdLaunchConsole(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchConsole() = default;

    bool Execute() override
    {
        if (HasActiveDocument() && !GetActiveDocument().m_path.empty())
            return Launch(L"cmd /K cd /d \"$(TAB_DIR)\"");
        else
            return Launch(L"cmd /K");
    }
    UINT GetCmdId() override { return cmdLaunchConsole; }
};

class CCmdLaunchExplorer : public LaunchBase
{
public:
    CCmdLaunchExplorer(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchExplorer() = default;

    bool Execute() override { return Launch(L"explorer \"$(TAB_DIR)\""); }
    UINT GetCmdId() override { return cmdLaunchExplorer; }
};

class CCmdLaunchSciter : public LaunchBase
{
public:
    CCmdLaunchSciter(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchSciter() = default;

    bool Execute() override { return Launch(L"scapp $(TAB_PATH)"); }
    UINT GetCmdId() override { return cmdLaunchSciter; }
};

class CCmdLaunchInspector : public LaunchBase
{
public:
    CCmdLaunchInspector(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchInspector() = default;

    bool Execute() override { return Launch(L"inspector"); }
    UINT GetCmdId() override { return cmdLaunchInspector; }
};

class CCmdLaunchSciterDebug : public LaunchBase
{
public:
    CCmdLaunchSciterDebug(void* obj)
        : LaunchBase(obj)
    {
    }
    ~CCmdLaunchSciterDebug() = default;

    bool Execute() override { return Launch(L"scapp \"$(TAB_PATH)\" --debug"); }
    UINT GetCmdId() override { return cmdLaunchSciterDebug; }
};