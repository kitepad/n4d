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
#include "CommandHandler.h"
#include "CmdBlanks.h"
#include "CmdBookmarks.h"
#include "CmdClipboard.h"
#include "CmdComment.h"
#include "CmdConvertCase.h"
#include "CmdEditSelection.h"
#include "CmdEOL.h"
#include "CmdFiles.h"
#include "CmdFindReplace.h"
#include "CmdFolding.h"
#include "CmdGotoLine.h"
#include "CmdGotoSymbol.h"
#include "CmdLaunch.h"
#include "CmdLineNumbers.h"
#include "CmdLines.h"
#include "CmdLineWrap.h"
#include "CmdMisc.h"
#include "CmdOpenSelection.h"
#include "CmdPrevNext.h"
#include "CmdPrint.h"
#include "CmdSort.h"
#include "CmdUndo.h"
#include "CmdWhiteSpace.h"
#include "CmdZoom.h"

#include "DirFileEnum.h"
#include "AppUtils.h"
#include "PathUtils.h"
#include "UnicodeUtils.h"
#include "IniSettings.h"
#include "ResString.h"

#include <fstream>

CCommandHandler::CCommandHandler()
    : m_highestCmdId(0)
    , m_bLoaded(false)
    , m_lastKey(0)
{
}

std::unique_ptr<CCommandHandler> CCommandHandler::m_instance = nullptr;

void CCommandHandler::ShutDown()
{
    m_instance.reset(nullptr);
}

ICommand* CCommandHandler::GetCommand(UINT cmdId)
{
    auto c = m_commands.find(cmdId);
    if (c != m_commands.end())
        return c->second.get();
    auto nc = m_noDeleteCommands.find(cmdId);
    if (nc != m_noDeleteCommands.end())
        return nc->second;
    return nullptr;
}

void CCommandHandler::Init(void* obj)
{
    Add<CCmdOpenFolder>(obj);
    Add<CCmdToggleTheme>(obj);
    Add<CCmdOpen>(obj);
    Add<CCmdSave>(obj);
    Add<CCmdSaveAll>(obj);
    Add<CCmdSaveAuto>(obj);
    Add<CCmdSaveAs>(obj);
    Add<CCmdReload>(obj);
    Add<CCmdWriteProtect>(obj);
    Add<CCmdFileDelete>(obj);
    Add<CCmdPrint>(obj);
    Add<CCmdPrintNow>(obj);
    Add<CCmdPageSetup>(obj);
    Add<CCmdUndo>(obj);
    Add<CCmdRedo>(obj);
    Add<CCmdCut>(obj);
    Add<CCmdCutPlain>(obj);
    Add<CCmdCopy>(obj);
    Add<CCmdCopyPlain>(obj);
    Add<CCmdPaste>(obj);
    Add<CCmdPasteHtml>(obj);
    Add<CCmdDelete>(obj);
    Add<CCmdSelectAll>(obj);
    Add<CCmdGotoBrace>(obj);
    Add<CCmdConfigShortcuts>(obj);
    Add<CCmdLineWrap>(obj);
    Add<CCmdLineWrapIndent>(obj);
    Add<CCmdWhiteSpace>(obj);
    Add<CCmdLineNumbers>(obj);
    Add<CCmdUseTabs>(obj);
    Add<CCmdAutoBraces>(obj);
    Add<CCmdAutoComplete>(obj);
    Add<CCmdViewFileTree>(obj);
    Add<CCmdSelectLexer>(obj);
    Add<CCmdSelectEncoding>(obj);
    Add<CCmdEOLWin>(obj);
    Add<CCmdEOLUnix>(obj);
    Add<CCmdEOLMac>(obj);
    Add<CCmdPrevNext>(obj);
    Add<CCmdPrevious>(obj);
    Add<CCmdNext>(obj);
    Add<CCmdFindReplace>(obj);
    Add<CCmdFindNext>(obj);
    Add<CCmdFindPrev>(obj);
    Add<CCmdFindSelectedNext>(obj);
    Add<CCmdFindSelectedPrev>(obj);
    Add<CCmdFindFile>(obj);
    Add<CCmdGotoLine>(obj);
    Add<CCmdGotoSymbol>(obj);
    Add<CCmdBookmarks>(obj);
    Add<CCmdBookmarkToggle>(obj);
    Add<CCmdBookmarkClearAll>(obj);
    Add<CCmdBookmarkNext>(obj);
    Add<CCmdBookmarkPrev>(obj);
    Add<CCmdComment>(obj);
    Add<CCmdUnComment>(obj);
    Add<CCmdConvertUppercase>(obj);
    Add<CCmdConvertLowercase>(obj);
    Add<CCmdConvertTitlecase>(obj);
    Add<CCmdLineDuplicate>(obj);
    Add<CCmdLineSplit>(obj);
    Add<CCmdLineJoin>(obj);
    Add<CCmdLineUp>(obj);
    Add<CCmdLineDown>(obj);
    Add<CCmdSort>(obj);
    Add<CCmdEditSelection>(obj);
    Add<CCmdFoldingMargin>(obj);
    Add<CCmdFoldAll>(obj);
    Add<CCmdFoldCurrent>(obj);
    Add<CCmdTrim>(obj);
    Add<CCmdTabs2Spaces>(obj);
    Add<CCmdSpaces2Tabs>(obj);
    Add<CCmdZoom100>(obj);
    Add<CCmdZoomIn>(obj);
    Add<CCmdZoomOut>(obj);
    Add<CCmdOpenSelection>(obj);
    Add<CCmdEnableD2D>(obj);
    Add<CCmdHighlightBrace>(obj);
    Add<CCmdFrameCaretLine>(obj);
    Add<CCmdLaunchEdge>(obj);
    Add<CCmdLaunchSciter>(obj);
    Add<CCmdLaunchFirefox>(obj);
    Add<CCmdLaunchChrome>(obj);
    Add<CCmdLaunchSciterDebug>(obj);
    Add<CCmdLaunchInspector>(obj);
    Add<CCmdLaunchConsole>(obj);
    Add<CCmdLaunchExplorer>(obj);
    Add<CCmdConfigStyle>(obj);
    Add<CCmdTabSize>(obj);
    Add<CCmdSelectTab>(obj);
    Add<CCmdCommandPalette>(obj);
}

void CCommandHandler::ScintillaNotify(SCNotification* pScn)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->ScintillaNotify(pScn);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->ScintillaNotify(pScn);
    }
}

void CCommandHandler::TabNotify(TBHDR* ptbHdr)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->TabNotify(ptbHdr);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->TabNotify(ptbHdr);
    }
}

void CCommandHandler::OnClose()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnClose();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnClose();
    }
}

void CCommandHandler::OnDocumentClose(DocID docId)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnDocumentClose(docId);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnDocumentClose(docId);
    }
}

void CCommandHandler::OnDocumentOpen(DocID docId)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnDocumentOpen(docId);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnDocumentOpen(docId);
    }
}

void CCommandHandler::OnDocumentSave(DocID docId, bool bSaveAs)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnDocumentSave(docId, bSaveAs);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnDocumentSave(docId, bSaveAs);
    }
}

void CCommandHandler::OnClipboardChanged()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnClipboardChanged();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnClipboardChanged();
    }
}

void CCommandHandler::BeforeLoad()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->BeforeLoad();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->BeforeLoad();
    }
}

void CCommandHandler::AfterInit()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->AfterInit();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->AfterInit();
    }
}

void CCommandHandler::OnThemeChanged(bool bDark)
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnThemeChanged(bDark);
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnThemeChanged(bDark);
    }
}

void CCommandHandler::OnLangChanged()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnLangChanged();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnLangChanged();
    }
}

void CCommandHandler::OnStylesSet()
{
    for (auto& [id, cmd] : m_commands)
    {
        cmd->OnStylesSet();
    }
    for (auto& [id, cmd] : m_noDeleteCommands)
    {
        if (cmd)
            cmd->OnStylesSet();
    }
}
void CCommandHandler::AddCommand(ICommand* cmd)
{
    m_highestCmdId     = max(m_highestCmdId, cmd->GetCmdId());
    auto [it, success] = m_noDeleteCommands.emplace(cmd->GetCmdId(), cmd);
    assert(success); // Verify no command has the same ID as an existing command.
}

void CCommandHandler::AddCommand(UINT cmdId)
{
    m_highestCmdId     = max(m_highestCmdId, cmdId);
    auto [it, success] = m_noDeleteCommands.emplace(cmdId, nullptr);
    assert(success); // Verify no command has the same ID as an existing command.
}

std::wstring CCommandHandler::GetCommandLabel(UINT cmd)
{
    auto whereAt = std::find_if(m_resourceData.begin(), m_resourceData.end(),
                                [&](const auto& item) { return (item.second == cmd); });
    if (whereAt != m_resourceData.end())
    {
        auto sID = whereAt->first;

        const auto& ttIDit = m_resourceData.find(sID);

        if (ttIDit != m_resourceData.end())
        {
            auto sRes  = LoadResourceWString(g_hRes, ttIDit->second);
            auto idx   = sRes.find(L"###");
            bool found = idx != std::wstring::npos;
            return found ? sRes.substr(0, idx) : sRes;
        }
    }
    return {};
}

std::wstring CCommandHandler::GetCommandDescription(UINT cmd)
{
    auto whereAt = std::find_if(m_resourceData.begin(), m_resourceData.end(),
                                [&](const auto& item) { return (item.second == cmd); });
    if (whereAt != m_resourceData.end())
    {
        auto sID = whereAt->first;

        const auto& ttIDit = m_resourceData.find(sID);

        if (ttIDit != m_resourceData.end())
        {
            auto sRes  = LoadResourceWString(g_hRes, ttIDit->second);
            auto idx   = sRes.find(L"###");
            bool found = idx != std::wstring::npos;
            return found ? sRes.substr(idx + 3) : sRes;
        }
    }

    return {};
}

std::wstring CCommandHandler::MakeShortCutKeyForAccel(const KshAccelEx& accel)
{
    std::wstring shortCut;
    wchar_t      buf[128] = {};

    auto mapKey = [&](UINT code) {
        LONG sc = MapVirtualKey(code, MAPVK_VK_TO_VSC);
        sc <<= 16;
        int len = GetKeyNameText(sc, buf, _countof(buf));
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    };

    auto scanKey = [&](UINT code) {
        LONG nScanCode = MapVirtualKey(code, MAPVK_VK_TO_VSC);
        switch (code)
        {
            // Keys which are "extended" (except for Return which is Numeric Enter as extended)
            case VK_INSERT:
            case VK_DELETE:
            case VK_HOME:
            case VK_END:
            case VK_NEXT:  // Page down
            case VK_PRIOR: // Page up
            case VK_LEFT:
            case VK_RIGHT:
            case VK_UP:
            case VK_DOWN:
            case VK_SNAPSHOT:
            case VK_OEM_COMMA:
            case VK_OEM_PERIOD:
                nScanCode |= 0x0100; // Add extended bit
                break;
            default:
                break;
        }
        nScanCode <<= 16;
        int len = GetKeyNameText(nScanCode, buf, _countof(buf));
        if (len == 0)
        {
            buf[0] = static_cast<wchar_t>(code);
            buf[1] = 0;
            len    = 1;
        }
        if (!shortCut.empty())
            shortCut += L"+";
        if (len > 0)
            shortCut += buf;
    };

    if (accel.fVirt & 0x08)
    {
        mapKey(VK_CONTROL);
    }
    if (accel.fVirt & 0x10)
    {
        mapKey(VK_MENU);
    }
    if (accel.fVirt & 0x04)
    {
        mapKey(VK_SHIFT);
    }
    if (accel.key1)
    {
        scanKey(accel.key1);
    }

    if (accel.key2)
    {
        scanKey(accel.key2);
    }
    return shortCut;   //L "(" + shortCut + L")";
}

static BYTE GetCtrlKeys()
{
    BYTE ctrlKeys = (GetKeyState(VK_CONTROL) & 0x8000) ? 0x08 : 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        ctrlKeys |= 0x04;
    if (GetKeyState(VK_MENU) & 0x8000)
        ctrlKeys |= 0x10;
    return ctrlKeys;
}

CCommandHandler& CCommandHandler::Instance()
{
    static CCommandHandler instance;
    if (!instance.m_bLoaded)
    {
        instance.LoadUIHeaders();
        instance.Load();
    }
    return instance;
}

void CCommandHandler::Load()
{
    DWORD       resSize = 0;
    const char* resData = CAppUtils::GetResourceData(L"config", IDR_SHORTCUTS, resSize);
    if (resData != nullptr)
    {
        CSimpleIni ini;
        ini.SetMultiKey(true);
        ini.LoadFile(resData, resSize);
        Load(ini);
    }

    std::wstring userFile = CAppUtils::GetDataPath() + L"\\n4d.shortcuts";
    CSimpleIni   userIni;
    userIni.SetMultiKey(true);
    userIni.LoadFile(userFile.c_str());
    Load(userIni);

    m_bLoaded = true;
}

void CCommandHandler::Load(const CSimpleIni& ini)
{
    CSimpleIni::TNamesDepend virtKeys;
    ini.GetAllKeys(L"virtkeys", virtKeys);
    for (auto keyName : virtKeys)
    {
        unsigned long  vk            = 0;
        const wchar_t* keyDataString = ini.GetValue(L"virtkeys", keyName);
        if (CAppUtils::TryParse(keyDataString, vk, false, 0, 16))
            m_virtKeys[keyName] = static_cast<UINT>(vk);
        //else
        //    APPVERIFYM(false, L"Invalid Virtual Key ini file. Cannot convert key '%s' to uint.", keyName);
    }

    CSimpleIni::TNamesDepend shortkeys;
    ini.GetAllKeys(L"shortcuts", shortkeys);

    for (auto cmd : shortkeys)
    {
        CSimpleIni::TNamesDepend values;
        ini.GetAllValues(L"shortcuts", cmd, values);
        for (auto v : values)
        {
            std::vector<std::wstring> keyVec;
            stringtok(keyVec, v, false, L",");
            KshAccelEx accel;
            accel.sCmd = cmd;
            accel.cmd  = static_cast<WORD>(_wtoi(cmd));
            if (accel.cmd == 0)
            {
                auto it = m_resourceData.find(cmd);
                if (it != m_resourceData.end())
                    accel.cmd = static_cast<WORD>(it->second);
            }
            for (size_t i = 0; i < keyVec.size(); ++i)
            {
                switch (i)
                {
                    case 0:
                        if (CStringUtils::find_caseinsensitive(keyVec[i], L"alt") != std::wstring::npos)
                            accel.fVirt |= 0x10;
                        if (CStringUtils::find_caseinsensitive(keyVec[i], L"ctrl") != std::wstring::npos)
                            accel.fVirt |= 0x08;
                        if (CStringUtils::find_caseinsensitive(keyVec[i], L"shift") != std::wstring::npos)
                            accel.fVirt |= 0x04;
                        break;
                    case 1:
                    {
                        auto keyVecI = keyVec[i].c_str();
                        auto whereAt = m_virtKeys.cend();
                        for (auto it = m_virtKeys.cbegin(); it != m_virtKeys.cend(); ++it)
                        {
                            if (_wcsicmp(keyVecI, it->first.c_str()) == 0)
                            {
                                whereAt = it;
                                break;
                            }
                        }
                        if (whereAt != m_virtKeys.cend())
                        {
                            accel.fVirt1 = TRUE;
                            accel.key1   = static_cast<WORD>(whereAt->second);
                        }
                        else
                        {
                            if ((keyVec[i].size() > 2) && (keyVec[i][0] == '0') && (keyVec[i][1] == 'x'))
                                accel.key1 = static_cast<WORD>(wcstol(keyVec[i].c_str(), nullptr, 0));
                            else
                                accel.key1 = static_cast<WORD>(::towupper(keyVec[i][0]));
                        }
                    }
                    break;
                    case 2:
                    {
                        auto keyVecI = keyVec[i].c_str();
                        auto whereAt = m_virtKeys.cend();
                        for (auto it = m_virtKeys.cbegin(); it != m_virtKeys.cend(); ++it)
                        {
                            if (_wcsicmp(keyVecI, it->first.c_str()) == 0)
                            {
                                whereAt = it;
                                break;
                            }
                        }
                        if (whereAt != m_virtKeys.cend())
                        {
                            accel.fVirt2 = TRUE;
                            accel.key2   = static_cast<WORD>(whereAt->second);
                        }
                        else
                        {
                            if ((keyVec[i].size() > 2) && (keyVec[i][0] == '0') && (keyVec[i][1] == 'x'))
                                accel.key2 = static_cast<WORD>(wcstol(keyVec[i].c_str(), nullptr, 0));
                            else
                                accel.key2 = static_cast<WORD>(::towupper(keyVec[i][0]));
                        }
                        break;
                    }
                    default:
                        break;
                } // switch
            }
            m_accelerators.push_back(std::move(accel));
        }
    }
}

LRESULT CALLBACK CCommandHandler::TranslateAccelerator(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (uMsg)
    {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            auto ctrlkeys = GetCtrlKeys();
            int  nonvirt  = MapVirtualKey(static_cast<UINT>(wParam), MAPVK_VK_TO_CHAR);
            for (auto accel = m_accelerators.crbegin(); accel != m_accelerators.crend(); ++accel)
            {
                if (accel->fVirt == ctrlkeys)
                {
                    if ((m_lastKey == 0) &&
                        (((accel->key1 == wParam) && accel->fVirt1) || (nonvirt && (accel->key1 == nonvirt) && !accel->fVirt1)))
                    {
                        if (accel->key2 == 0)
                        {
                            // execute the command
                            if (GetForegroundWindow() == hwnd)
                            {
                                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(accel->cmd, 1), 0);
                                m_lastKey = 0;
                                return TRUE;
                            }
                        }
                        else
                        {
                            m_lastKey = accel->key1;
                            // ignore alt codes
                            if (accel->fVirt == 0x10)
                            {
                                if (accel->key1 >= VK_NUMPAD0 && accel->key1 <= VK_NUMPAD9)
                                    return FALSE;
                            }
                            return TRUE;
                        }
                    }
                    else if ((m_lastKey == accel->key1) &&
                             (((accel->key2 == wParam) && accel->fVirt2) || (nonvirt && (accel->key2 == nonvirt) && !accel->fVirt2)))
                    {
                        // execute the command
                        if (GetForegroundWindow() == hwnd)
                        {
                            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(accel->cmd, 1), 0);
                            m_lastKey = 0;
                            return TRUE;
                        }
                    }
                }
            }
        }
        break;
        case WM_KEYUP:
        {
            auto ctrlKeys = GetCtrlKeys();
            if (ctrlKeys == 0)
                m_lastKey = 0;
        }
        break;
        default:
            break;
    }
    return FALSE;
}

std::wstring CCommandHandler::GetShortCutStringForCommand(UINT cmd) const
{
    auto whereAt = std::find_if(m_accelerators.begin(), m_accelerators.end(),
                                [&](const auto& item) { return (item.cmd == cmd); });
    if (whereAt != m_accelerators.end())
    {
        return MakeShortCutKeyForAccel(*whereAt);
    }
    return {};
}

void CCommandHandler::LoadUIHeaders()
{
    m_resourceData.clear();
    DWORD       resSize = 0;
    const char* resData = CAppUtils::GetResourceData(L"config", IDR_BOWPADUIH, resSize);
    LoadUIHeader(resData, resSize);

    resData = CAppUtils::GetResourceData(L"config", IDR_SCINTILLAH, resSize);
    LoadUIHeader(resData, resSize);
}

void CCommandHandler::LoadUIHeader(const char* resData, DWORD resSize)
{
    if (resData != nullptr)
    {
        // parse the header file
        DWORD lastLineStart = 0;
        for (DWORD ind = 0; ind < resSize; ++ind)
        {
            if (resData[ind] == '\n')
            {
                std::string sLine(resData + lastLineStart, ind - lastLineStart);
                lastLineStart = ind + 1;
                // cut off '#define'
                if (sLine.empty())
                    continue;
                if (sLine[0] == '/')
                    continue;
                if (sLine.find("#define") == std::string::npos)
                    continue;
                auto spacePos = sLine.find(' ');
                if (spacePos != std::string::npos)
                {
                    auto spacePos2 = sLine.find(' ', spacePos + 1);
                    if (spacePos2 != std::string::npos)
                    {
                        std::string sIDString                                   = sLine.substr(spacePos + 1, spacePos2 - spacePos - 1);
                        int         id                                          = atoi(sLine.substr(spacePos2).c_str());
                        m_resourceData[CUnicodeUtils::StdGetUnicode(sIDString)] = id;
                    }
                }
            }
        }
    }
}
