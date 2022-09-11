// This file is part of BowPad.
//
// Copyright (C) 2013-2018, 2020-2021 - Stefan Kueng
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

#include "MainWindow.h"
#include "CmdLineParser.h"
#include "BaseDialog.h"
#include "AppUtils.h"
#include "SmartHandle.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "ProgressDlg.h"
#include "SysInfo.h"
#include "OnOutOfScope.h"
#include "CommandHandler.h"
#include "ResString.h"
#include <wrl.h>

HINSTANCE   g_hInst;
HINSTANCE   g_hRes;
bool        firstInstance = false;
HWND        g_hMainWindow  = nullptr;

static void SetIcon()
{
    HKEY hKey = nullptr;
    if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\n4d.exe", &hKey) == ERROR_SUCCESS)
    {
        // registry key exists, which means at least one file type was associated with BowPad by the user
        RegCloseKey(hKey);
        if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\n4d.exe\\DefaultIcon", &hKey) != ERROR_SUCCESS)
        {
            // but the default icon hasn't been set yet: set the default icon now
            if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\n4d.exe\\DefaultIcon", &hKey) == ERROR_SUCCESS)
            {
                OnOutOfScope(RegCloseKey(hKey););
                std::wstring sIconPath = CStringUtils::Format(L"%s,-%d", CPathUtils::GetLongPathname(CPathUtils::GetModulePath()).c_str(), IDI_BOWPAD);
                if (RegSetValue(hKey, nullptr, REG_SZ, sIconPath.c_str(), 0) == ERROR_SUCCESS)
                {
                    // now tell the shell about the changed icon
                    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
                }
            }
        }
        else
        {
            RegCloseKey(hKey);
        }
    }
}

static void SetUserStringKey(LPCWSTR keyName, LPCWSTR subKeyName, const std::wstring& keyValue)
{
    DWORD dwSizeInBytes = static_cast<DWORD>((keyValue.length() + 1) * sizeof(WCHAR));
    auto  status        = SHSetValue(HKEY_CURRENT_USER, keyName, subKeyName, REG_SZ, keyValue.c_str(), dwSizeInBytes);
    if (status != ERROR_SUCCESS)
    {
        std::wstring msg = CStringUtils::Format(L"Registry key '%s' (subkey: '%s') could not be set.",
                                                keyName, subKeyName ? subKeyName : L"(none)");
        MessageBox(nullptr, msg.c_str(), L"N4D", MB_ICONINFORMATION);
    }
}

static void ForwardToOtherInstance(HWND hBowPadWnd, LPCTSTR lpCmdLine, CCmdLineParser& parser)
{
    if (::IsIconic(hBowPadWnd))
        ::ShowWindow(hBowPadWnd, SW_RESTORE);
    // if the window is not yet visible, we wait a little bit
    // and we don't make the window visible here: the message we send
    // to open the file might get handled before the RegisterAndCreateWindow
    // in MainWindow.cpp hasn't finished yet. Just let that function make
    // the window visible in the right position.
    if (IsWindowVisible(hBowPadWnd))
    {
        // check if it's on the current virtual desktop
        IVirtualDesktopManager* pvdm = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_VirtualDesktopManager,
                                       nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pvdm))))
        {
            BOOL isCurrent = FALSE;
            if (pvdm && SUCCEEDED(pvdm->IsWindowOnCurrentVirtualDesktop(hBowPadWnd, &isCurrent)))
            {
                if (!isCurrent)
                {
                    pvdm->Release();
                    pvdm = nullptr;

                    // move it to the current virtual desktop
                    SendMessage(hBowPadWnd, WM_MOVETODESKTOP, 0, reinterpret_cast<LPARAM>(GetForegroundWindow()));
                }
                else
                    ::SetForegroundWindow(hBowPadWnd);
            }
        }
        else
            ::SetForegroundWindow(hBowPadWnd);
        if (pvdm)
            pvdm->Release();
    }
    else
        Sleep(500);

    size_t cmdLineLen = wcslen(lpCmdLine);
    if (cmdLineLen)
    {
        COPYDATASTRUCT cds = {};
        cds.dwData         = CD_COMMAND_LINE;
        if (!parser.HasVal(L"path"))
        {
            // create our own command line with all paths converted to long/full paths
            // since the CWD of the other instance is most likely different
            int                nArgs;
            std::wstring       sCmdLine;
            const std::wstring commandLine = GetCommandLineW();
            LPWSTR*            szArgList   = CommandLineToArgvW(commandLine.c_str(), &nArgs);
            if (szArgList)
            {
                OnOutOfScope(LocalFree(szArgList););
                bool bOmitNext = false;
                for (int i = 0; i < nArgs; i++)
                {
                    if (bOmitNext)
                    {
                        bOmitNext = false;
                        continue;
                    }
                    if ((szArgList[i][0] != '/') && (szArgList[i][0] != '-'))
                    {
                        std::wstring path = szArgList[i];
                        CPathUtils::NormalizeFolderSeparators(path);
                        path = CPathUtils::GetLongPathname(path);
                        if (!PathFileExists(path.c_str()))
                        {
                            auto pathPos = commandLine.find(szArgList[i]);
                            if (pathPos != std::wstring::npos)
                            {
                                auto tempPath = commandLine.substr(pathPos);
                                if (PathFileExists(tempPath.c_str()))
                                {
                                    CPathUtils::NormalizeFolderSeparators(tempPath);
                                    path = CPathUtils::GetLongPathname(tempPath);
                                    sCmdLine += L"\"" + path + L"\" ";
                                    break;
                                }
                            }
                        }
                        sCmdLine += L"\"" + path + L"\" ";
                    }
                    else
                    {
                        if (wcscmp(&szArgList[i][1], L"z") == 0)
                            bOmitNext = true;
                        else
                        {
                            sCmdLine += szArgList[i];
                            sCmdLine += L" ";
                        }
                    }
                }
            }
            auto ownCmdLine = std::make_unique<wchar_t[]>(sCmdLine.size() + 2);
            wcscpy_s(ownCmdLine.get(), sCmdLine.size() + 2, sCmdLine.c_str());
            cds.cbData = static_cast<DWORD>((sCmdLine.size() + 1) * sizeof(wchar_t));
            cds.lpData = ownCmdLine.get();
            SendMessage(hBowPadWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
        }
        else
        {
            cds.cbData = static_cast<DWORD>((cmdLineLen + 1) * sizeof(wchar_t));
            cds.lpData = static_cast<PVOID>(const_cast<LPWSTR>(lpCmdLine));
            SendMessage(hBowPadWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
        }
    }
}

static HWND FindAndWaitForBowPad()
{
    // don't start another instance: reuse the existing one
    // find the window of the existing instance
    ResString    clsResName(g_hInst, IDC_BOWPAD);
    std::wstring clsName = static_cast<LPCWSTR>(clsResName) + CAppUtils::GetSessionID();

    HWND hBowPadWnd = ::FindWindow(clsName.c_str(), nullptr);
    // if we don't have a window yet, wait a little while
    // to give the other process time to create the window
    for (int i = 0; !hBowPadWnd && i < 20; i++)
    {
        Sleep(100);
        hBowPadWnd = ::FindWindow(clsName.c_str(), nullptr);
    }
    // also wait for the window to become visible first
    for (int i = 0; !IsWindowVisible(hBowPadWnd) && i < 20; i++)
    {
        Sleep(100);
    }
    return hBowPadWnd;
}

static void ShowBowPadCommandLineHelp()
{
    std::wstring sMessage = CStringUtils::Format(L"N4D \nusage: n4d.exe PATH [Options]\nOptions:\n   [/n:number] goto line number\n   [/admin] Run as Administator\n   [/f] Force open a new window");
    MessageBox(nullptr, sMessage.c_str(), L"N4D", MB_ICONINFORMATION);
}

static void ParseCommandLine(CCmdLineParser& parser, CMainWindow* mainWindow)
{
    // find out if there are paths specified without the key/value pair syntax
    int nArgs;

    const std::wstring commandLine = GetCommandLineW();
    LPWSTR*            szArgList   = CommandLineToArgvW(commandLine.c_str(), &nArgs);
    if (szArgList)
    {
        OnOutOfScope(LocalFree(szArgList););
        size_t line = static_cast<size_t>(-1);
        if (parser.HasVal(L"n"))
        {
            line = parser.GetLongLongVal(L"n") - 1LL;
        }

        bool bOmitNext = false;
        for (int i = 1; i < nArgs; i++)
        {
            if (bOmitNext)
            {
                bOmitNext = false;
                continue;
            }
            if ((szArgList[i][0] != '/') && (szArgList[i][0] != '-'))
            {
                auto pathPos = commandLine.find(szArgList[i]);
                if (pathPos != std::wstring::npos)
                {
                    auto tempPath = commandLine.substr(pathPos);
                    if (PathFileExists(tempPath.c_str()))
                    {
                        CPathUtils::NormalizeFolderSeparators(tempPath);
                        auto path = CPathUtils::GetLongPathname(tempPath);
                        mainWindow->SetFileToOpen(path, line);
                        break;
                    }
                }

                std::wstring path = szArgList[i];
                CPathUtils::NormalizeFolderSeparators(path);
                path = CPathUtils::GetLongPathname(path);
                if (!PathFileExists(path.c_str()))
                {
                    pathPos = commandLine.find(szArgList[i]);
                    if (pathPos != std::wstring::npos)
                    {
                        auto tempPath = commandLine.substr(pathPos);
                        if (PathFileExists(tempPath.c_str()))
                        {
                            CPathUtils::NormalizeFolderSeparators(tempPath);
                            path = CPathUtils::GetLongPathname(tempPath);
                            mainWindow->SetFileToOpen(path, line);
                            break;
                        }
                    }
                }
                mainWindow->SetFileToOpen(path, line);
            }
            else
            {
                if (wcscmp(&szArgList[i][1], L"z") == 0)
                    bOmitNext = true;
            }
        }
    }
}

int n4dMain(LPCTSTR lpCmdLine, bool bAlreadyRunning)
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return -1;
    OnOutOfScope(CoUninitialize(););

    auto parser = std::make_unique<CCmdLineParser>(lpCmdLine);

    if (parser->HasKey(L"?") || parser->HasKey(L"help"))
    {
        ShowBowPadCommandLineHelp();
        return 0;
    }

    bool isAdminMode = SysInfo::Instance().IsUACEnabled() && SysInfo::Instance().IsElevated();
    if (parser->HasKey(L"admin") && !isAdminMode)
    {
        std::wstring     modPath    = CPathUtils::GetModulePath();
        SHELLEXECUTEINFO shExecInfo = {sizeof(SHELLEXECUTEINFO)};

        shExecInfo.hwnd         = nullptr;
        shExecInfo.lpVerb       = L"runas";
        shExecInfo.lpFile       = modPath.c_str();
        shExecInfo.lpParameters = parser->getCmdLine();
        shExecInfo.nShow        = SW_NORMAL;

        if (ShellExecuteEx(&shExecInfo))
            return 0;
    }
    if (bAlreadyRunning && !parser->HasKey(L"f"))
    {
        HWND hBowPadWnd = FindAndWaitForBowPad();
        if (hBowPadWnd)
        {
            ForwardToOtherInstance(hBowPadWnd, lpCmdLine, *parser);
            return 0;
        }
    }

    CIniSettings::Instance().SetIniPath(CAppUtils::GetDataPath() + L"\\n4d.settings");
    SetIcon();

    auto        mainWindow = std::make_unique<CMainWindow>(g_hRes);
    if (!mainWindow->RegisterAndCreateWindow())
        return -1;

    ParseCommandLine(*parser, mainWindow.get());

    //// Don't need the parser any more so don't keep it around taking up space.
    parser.reset();

    // force CWD to the install path to avoid the CWD being locked:
    // if BowPad is started from another path (e.g. via double click on a text file in
    // explorer), the CWD is the directory of that file. As long as BowPad runs with the CWD
    // set to that dir, that dir can't be removed or renamed due to the lock.
    ::SetCurrentDirectory(CPathUtils::GetModuleDir().c_str());

    g_hMainWindow = *mainWindow.get();

    // Main message loop:
    MSG   msg = {nullptr};
    auto& kb  = CCommandHandler::Instance();
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!kb.TranslateAccelerator(*mainWindow.get(), msg.message, msg.wParam, msg.lParam) &&
            !CDialog::IsDialogMessage(&msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    CCommandHandler::ShutDown();
    Animator::ShutDown();
    return static_cast<int>(msg.wParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE     hInstance,
                      _In_opt_ HINSTANCE /*hPrevInstance*/,
                      _In_ LPTSTR        lpCmdLine,
                      _In_ int           /*nCmdShow*/)
{
    g_hInst = hInstance;
    g_hRes  = hInstance;

    const std::wstring sID = L"N4D_EFA99E4D-68EB-4EFA-B8CE-4F5B41104540_" + CAppUtils::GetSessionID();
    ::SetLastError(NO_ERROR); // Don't do any work between these 3 statements to spoil the error code.
    HANDLE hAppMutex   = ::CreateMutex(nullptr, false, sID.c_str());
    DWORD  mutexStatus = GetLastError();
    OnOutOfScope(CloseHandle(hAppMutex););
    bool bAlreadyRunning = (mutexStatus == ERROR_ALREADY_EXISTS || mutexStatus == ERROR_ACCESS_DENIED);
    firstInstance        = !bAlreadyRunning;

    auto mainResult = n4dMain(lpCmdLine, bAlreadyRunning);
    Scintilla_ReleaseResources();

    // Be careful shutting down Scintilla's resources here if any
    // global static objects contain things like CScintillaWnd as members
    // as they will destruct AFTER WinMain. That won't be a good thing
    // if we've released Scintilla resources IN WinMain.

    return mainResult;
}
