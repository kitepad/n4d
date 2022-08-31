// This file is part of BowPad.
//
// Copyright (C) 2014, 2016-2017, 2021 - Stefan Kueng
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
#include "CmdEol.h"
#include "Document.h"

bool CCmdEOLBase::Execute()
{
    auto lineType = GetLineType();
    Scintilla().SetEOLMode(lineType);
    Scintilla().ConvertEOLs(lineType);
    if (HasActiveDocument())
    {
        auto& doc    = GetModActiveDocument();
        doc.m_format = toEolFormat(lineType);
        UpdateStatusBar(true);
    }

    return true;
}

CCmdEOLWin::CCmdEOLWin(void* obj)
    : CCmdEOLBase(obj)
{
}

CCmdEOLUnix::CCmdEOLUnix(void* obj)
    : CCmdEOLBase(obj)
{
}

CCmdEOLMac::CCmdEOLMac(void* obj)
    : CCmdEOLBase(obj)
{
}

