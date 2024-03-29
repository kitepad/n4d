﻿// This file is part of BowPad.
//
// Copyright (C) 2013-2016, 2020-2021 - Stefan Kueng
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

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <windowsx.h>

// C RunTime Header Files
#include <stdlib.h>
#include <memory.h>
#include <comip.h>
#include <comdef.h>
#include "IniSettings.h"
#include <shellapi.h>
#include <Commctrl.h>
#include <Shlobj.h>
#include <Shlwapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// SDKs prior to Win10 don't have the IsWindows10OrGreater API in the versionhelpers header, so
// we define it here just in case:
#include <VersionHelpers.h>
#ifndef _WIN32_WINNT_WIN10
#    define _WIN32_WINNT_WIN10        0x0A00
#    define _WIN32_WINNT_WINTHRESHOLD 0x0A00
#    define IsWindows10OrGreater()    (IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 0))
#endif

#define APP_ID          L"Notepad4Developer.1"
#define APP_ID_ELEVATED L"Notepad4Developer_Elevated.1"

#ifdef _WIN64
#    define LANGPLAT L"x64"
#else
#    define LANGPLAT L"x86"
#endif

// custom id for the WM_COPYDATA message
#define CD_COMMAND_LINE    101
#define CD_COMMAND_MOVETAB 102

#define WM_UPDATEAVAILABLE   (WM_APP + 10)
#define WM_AFTERINIT         (WM_APP + 11)
#define WM_STATUSBAR_MSG     (WM_APP + 12)
#define WM_THREADRESULTREADY (WM_APP + 13)
#define WM_CANHIDECURSOR     (WM_APP + 14)
#define WM_MOVETODESKTOP     (WM_APP + 15)
#define WM_MOVETODESKTOP2    (WM_APP + 16)
#define WM_SCICHAR           (WM_APP + 17)
#define WM_EDITORCONTEXTMENU (WM_APP + 18)
#define WM_TABCONTEXTMENU    (WM_APP + 19)

typedef _com_ptr_t<_com_IIID<IFileSaveDialog, &__uuidof(IFileSaveDialog)>> IFileSaveDialogPtr;
typedef _com_ptr_t<_com_IIID<IFileOpenDialog, &__uuidof(IFileOpenDialog)>> IFileOpenDialogPtr;
typedef _com_ptr_t<_com_IIID<IShellItem, &__uuidof(IShellItem)>> IShellItemPtr;
typedef _com_ptr_t<_com_IIID<IShellItemArray, &__uuidof(IShellItemArray)>> IShellItemArrayPtr;
typedef _com_ptr_t<_com_IIID<IFileOperation, &__uuidof(IFileOperation)>> IFileOperationPtr;
typedef _com_ptr_t<_com_IIID<IContextMenu, &__uuidof(IContextMenu)>> IContextMenuPtr;

#include "main.h"
