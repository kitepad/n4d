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
#pragma once
#include "ICommand.h"

#include <string>
#include <map>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <unordered_set>

class KshAccelEx
{
public:
    KshAccelEx()
        : fVirt(0)
        , fVirt1(false)
        , key1(0)
        , fVirt2(false)
        , key2(0)
        , cmd(0)
    {
    }
    ~KshAccelEx() {}

    BYTE         fVirt;  // 0x10 = ALT, 0x08 = Ctrl, 0x04 = Shift
    bool         fVirt1; // true if key1 is a virtual key, false if it's a character code
    WORD         key1;
    bool         fVirt2;
    WORD         key2;
    WORD         cmd;  // the command
    std::wstring sCmd; // the command as a string
};

class CCommandHandler
{
public:
    CCommandHandler();
    ~CCommandHandler() = default;

public:
    static CCommandHandler& Instance();
    static void             ShutDown();

    LRESULT CALLBACK TranslateAccelerator(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    std::wstring GetShortCutStringForCommand(UINT cmd) const;
    const std::unordered_map<std::wstring, UINT>& GetVirtKeys() const { return m_virtKeys; }
    const std::vector<KshAccelEx>& GetAccelerators() const { return m_accelerators; }
    static std::wstring MakeShortCutKeyForAccel(const KshAccelEx& accel);

    void        Init(void* obj);
    ICommand*   GetCommand(UINT cmdId);
    std::wstring GetCommandLabel(UINT cmdId);
    std::wstring GetCommandDescription(UINT cmdId);
    const std::unordered_map<std::wstring, UINT>& GetResourceData() const { return m_resourceData; }
    void        ScintillaNotify(SCNotification* pScn);
    void        TabNotify(TBHDR* ptbHdr);
    void        OnClose();
    void        OnDocumentClose(DocID docId);
    void        OnDocumentOpen(DocID docId);
    void        OnDocumentSave(DocID docId, bool bSaveAs);
    void        OnClipboardChanged();
    void        BeforeLoad();
    void        AfterInit();
    void        OnTimer(UINT timerId);
    void        OnThemeChanged(bool bDark);
    void        OnLangChanged();
    void        OnStylesSet();
    void        AddCommand(ICommand* cmd);
    void        AddCommand(UINT cmdId);

    const std::map<UINT, std::unique_ptr<ICommand>>& GetCommands() const
    {
        return m_commands;
    };
    const std::map<UINT, ICommand*>& GetNoDeleteCommands() const
    {
        return m_noDeleteCommands;
    };

private:
    template <typename T, typename... Args>
    T* Add(Args... args)
    {
        // Construct the type we want. We need to get the id out of it.
        // Move it into the map, then return the pointer we got
        // out. We know it must be the type we want because we just created it.
        // We could use shared_ptr here but we control the life time so
        // no point paying the price as if we didn't.
        auto pCmd      = std::make_unique<T>(std::forward<Args>(args)...);
        auto cmdId     = pCmd->GetCmdId();
        m_highestCmdId = max(m_highestCmdId, cmdId);
        auto at        = m_commands.emplace(cmdId, std::move(pCmd));
        assert(at.second); // Verify no command has the same ID as an existing command.
        return static_cast<T*>(at.first->second.get());
    }

    std::map<UINT, std::unique_ptr<ICommand>> m_commands;
    std::map<UINT, ICommand*>                 m_noDeleteCommands;
    UINT                                      m_highestCmdId;
    static std::unique_ptr<CCommandHandler>   m_instance;
    std::unordered_map<std::wstring, UINT>     m_resourceData;
    bool                                      m_bLoaded;
    std::vector<KshAccelEx>                     m_accelerators;
    std::unordered_map<std::wstring, UINT>    m_virtKeys;
    WORD                                      m_lastKey;
    void                                      LoadUIHeaders();
    void                                       LoadUIHeader(const char* resData, DWORD resSize);
    void                                       Load();
    void                                       Load(const CSimpleIni& ini);
};
