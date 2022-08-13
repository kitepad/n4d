﻿// sktoolslib - common files for SK tools

// Copyright (C) 2019-2021 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "stdafx.h"
#include "DarkModeHelper.h"
//#include "StringUtils.h"
//#include "PathUtils.h"
#include <vector>
#include <Shlobj.h>

DarkModeHelper& DarkModeHelper::Instance()
{
    static DarkModeHelper helper;
    return helper;
}

bool DarkModeHelper::CanHaveDarkMode() const
{
    return m_bCanHaveDarkMode;
}

void DarkModeHelper::AllowDarkModeForApp(BOOL allow) const
{
    if (m_pAllowDarkModeForApp)
        m_pAllowDarkModeForApp(allow ? 1 : 0);
    if (m_pSetPreferredAppMode)
        m_pSetPreferredAppMode(allow ? ForceDark : Default);
}

void DarkModeHelper::AllowDarkModeForWindow(HWND hwnd, BOOL allow) const
{
    if (m_pAllowDarkModeForWindow)
        m_pAllowDarkModeForWindow(hwnd, allow);
}

BOOL DarkModeHelper::ShouldAppsUseDarkMode() const
{
    if (m_pShouldAppsUseDarkMode && m_pAllowDarkModeForApp)
        return m_pShouldAppsUseDarkMode() & 0x01;
    return ShouldSystemUseDarkMode();
}

BOOL DarkModeHelper::IsDarkModeAllowedForWindow(HWND hwnd) const
{
    if (m_pIsDarkModeAllowedForWindow)
        return m_pIsDarkModeAllowedForWindow(hwnd);
    return FALSE;
}

BOOL DarkModeHelper::IsDarkModeAllowedForApp() const
{
    if (m_pIsDarkModeAllowedForApp)
        return m_pIsDarkModeAllowedForApp();
    return FALSE;
}

BOOL DarkModeHelper::ShouldSystemUseDarkMode() const
{
    if (m_pShouldSystemUseDarkMode)
        return m_pShouldSystemUseDarkMode();
    return FALSE;
}

void DarkModeHelper::RefreshImmersiveColorPolicyState() const
{
    if (m_pRefreshImmersiveColorPolicyState)
        m_pRefreshImmersiveColorPolicyState();
}

BOOL DarkModeHelper::GetIsImmersiveColorUsingHighContrast(IMMERSIVE_HC_CACHE_MODE mode) const
{
    if (m_pGetIsImmersiveColorUsingHighContrast)
        return m_pGetIsImmersiveColorUsingHighContrast(mode);
    return FALSE;
}

void DarkModeHelper::FlushMenuThemes() const
{
    if (m_pFlushMenuThemes)
        m_pFlushMenuThemes();
}

BOOL DarkModeHelper::SetWindowCompositionAttribute(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA* data) const
{
    if (m_pSetWindowCompositionAttribute)
        return m_pSetWindowCompositionAttribute(hWnd, data);
    return FALSE;
}

void DarkModeHelper::RefreshTitleBarThemeColor(HWND hWnd, BOOL dark) const
{
    WINDOWCOMPOSITIONATTRIBDATA data = {WCA_USEDARKMODECOLORS, &dark, sizeof(dark)};
    SetWindowCompositionAttribute(hWnd, &data);
}

DarkModeHelper::DarkModeHelper()
{
    INITCOMMONCONTROLSEX used = {
        sizeof(INITCOMMONCONTROLSEX),
        ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_COOL_CLASSES};
    InitCommonControlsEx(&used);

    m_bCanHaveDarkMode   = true;
    //PWSTR        sysPath = nullptr;
    long         micro   = 19043;
    std::wstring dllPath;

    if (dllPath.empty())
        m_hUxthemeLib = LoadLibrary(L"uxtheme.dll");
    else
        m_hUxthemeLib = LoadLibrary(dllPath.c_str());
    if (m_hUxthemeLib && m_bCanHaveDarkMode)
    {
        // Note: these functions are undocumented! Which meas I shouldn't even use them.
        // So, since MS decided to keep this new feature to themselves, I have to use
        // undocumented functions to adjust.
        // Let's just hope they change their minds and document these functions one day...

        // first try with the names, just in case MS decides to properly export these functions
        if (micro < 18362)
            m_pAllowDarkModeForApp = reinterpret_cast<AllowDarkModeForAppFpn>(GetProcAddress(m_hUxthemeLib, "AllowDarkModeForApp"));
        else
            m_pSetPreferredAppMode = reinterpret_cast<SetPreferredAppModeFpn>(GetProcAddress(m_hUxthemeLib, "SetPreferredAppMode"));
        m_pAllowDarkModeForWindow               = reinterpret_cast<AllowDarkModeForWindowFpn>(GetProcAddress(m_hUxthemeLib, "AllowDarkModeForWindow"));
        m_pShouldAppsUseDarkMode                = reinterpret_cast<ShouldAppsUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, "ShouldAppsUseDarkMode"));
        m_pIsDarkModeAllowedForWindow           = reinterpret_cast<IsDarkModeAllowedForWindowFpn>(GetProcAddress(m_hUxthemeLib, "IsDarkModeAllowedForWindow"));
        m_pIsDarkModeAllowedForApp              = reinterpret_cast<IsDarkModeAllowedForAppFpn>(GetProcAddress(m_hUxthemeLib, "IsDarkModeAllowedForApp"));
        m_pShouldSystemUseDarkMode              = reinterpret_cast<ShouldSystemUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, "ShouldSystemUseDarkMode"));
        m_pRefreshImmersiveColorPolicyState     = reinterpret_cast<RefreshImmersiveColorPolicyStateFn>(GetProcAddress(m_hUxthemeLib, "RefreshImmersiveColorPolicyState"));
        m_pGetIsImmersiveColorUsingHighContrast = reinterpret_cast<GetIsImmersiveColorUsingHighContrastFn>(GetProcAddress(m_hUxthemeLib, "GetIsImmersiveColorUsingHighContrast"));
        m_pFlushMenuThemes                      = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(m_hUxthemeLib, "FlushMenuThemes"));
        m_pSetWindowCompositionAttribute        = reinterpret_cast<SetWindowCompositionAttributeFpn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
        if (m_pAllowDarkModeForApp == nullptr && micro < 18362)
            m_pAllowDarkModeForApp = reinterpret_cast<AllowDarkModeForAppFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(135)));
        if (m_pSetPreferredAppMode == nullptr && micro >= 18362)
            m_pSetPreferredAppMode = reinterpret_cast<SetPreferredAppModeFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(135)));
        if (m_pAllowDarkModeForWindow == nullptr)
            m_pAllowDarkModeForWindow = reinterpret_cast<AllowDarkModeForWindowFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(133)));
        if (m_pShouldAppsUseDarkMode == nullptr)
            m_pShouldAppsUseDarkMode = reinterpret_cast<ShouldAppsUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(132)));
        if (m_pIsDarkModeAllowedForWindow == nullptr)
            m_pIsDarkModeAllowedForWindow = reinterpret_cast<IsDarkModeAllowedForWindowFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(137)));
        if (m_pIsDarkModeAllowedForApp == nullptr)
            m_pIsDarkModeAllowedForApp = reinterpret_cast<IsDarkModeAllowedForAppFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(139)));
        if (m_pShouldSystemUseDarkMode == nullptr)
            m_pShouldSystemUseDarkMode = reinterpret_cast<ShouldSystemUseDarkModeFpn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(138)));
        if (m_pRefreshImmersiveColorPolicyState == nullptr)
            m_pRefreshImmersiveColorPolicyState = reinterpret_cast<RefreshImmersiveColorPolicyStateFn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(104)));
        if (m_pGetIsImmersiveColorUsingHighContrast == nullptr)
            m_pGetIsImmersiveColorUsingHighContrast = reinterpret_cast<GetIsImmersiveColorUsingHighContrastFn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(106)));
        if (m_pFlushMenuThemes == nullptr)
            m_pFlushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(m_hUxthemeLib, MAKEINTRESOURCEA(136)));
    }
}

DarkModeHelper::~DarkModeHelper()
{
    FreeLibrary(m_hUxthemeLib);
}
