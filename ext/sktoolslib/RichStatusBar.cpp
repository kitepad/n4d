// sktoolslib - common files for SK tools

// Copyright (C) 2017-2018, 2020-2021 Stefan Kueng

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
#include "RichStatusBar.h"
#include "GDIHelpers.h"
#include "DPIAware.h"
#include <deque>
#include "Theme.h"
//#include "Themes.h"

constexpr int border_width = 8;
#define ICON_WIDTH (GetHeight())

CRichStatusBar::CRichStatusBar(HINSTANCE hInst)
    : CWindow(hInst)
    , m_fonts{nullptr}
    , m_tooltip(nullptr)
    //, m_themeColorFunc(nullptr)
    , m_hoverPart(-1)
    , m_height(0)
    , m_drawGrip(false)
{
}

CRichStatusBar::~CRichStatusBar()
{
    for (auto& font : m_fonts)
        DeleteObject(font);
}

bool CRichStatusBar::Init(HWND hParent, bool drawGrip)
{
    m_drawGrip     = drawGrip;
    WNDCLASSEX wcx = {sizeof(WNDCLASSEX)};

    wcx.lpfnWndProc   = CWindow::stWinMsgHandler;
    wcx.style         = CS_DBLCLKS;
    wcx.hInstance     = hResource;
    wcx.lpszClassName = L"RichStatusBar_{226E35DD-FFAC-4D97-A040-B94AF5BE39EC}";
    wcx.hbrBackground = CreateSolidBrush(CTheme::CurrentTheme().winBack); // darkHBR; // reinterpret_cast<HBRUSH>((COLOR_3DFACE + 1));
    wcx.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(0, WS_CHILD | WS_VISIBLE, hParent))
        {
            NONCLIENTMETRICS ncm;
            ncm.cbSize = sizeof(NONCLIENTMETRICS);
            SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0U);
            ncm.lfStatusFont.lfHeight = 24;
            m_fonts[0]                = CreateFontIndirect(&ncm.lfStatusFont);
            ncm.lfStatusFont.lfItalic = TRUE;
            m_fonts[1]                = CreateFontIndirect(&ncm.lfStatusFont);
            ncm.lfStatusFont.lfItalic = FALSE;
            ncm.lfStatusFont.lfWeight = FW_BOLD;
            m_fonts[2]                = CreateFontIndirect(&ncm.lfStatusFont);
            ncm.lfStatusFont.lfItalic = TRUE;
            ncm.lfStatusFont.lfWeight = FW_BOLD;
            m_fonts[3]                = CreateFontIndirect(&ncm.lfStatusFont);
            ncm.lfStatusFont.lfItalic = FALSE;
            ncm.lfStatusFont.lfWeight = FW_NORMAL;
            ncm.lfStatusFont.lfHeight = 20;
            ncm.lfStatusFont.lfWeight = FW_BOLD;
            //ncm.lfStatusFont.lfFaceName = L"Segoe MDL2 Assets";
            //wcscpy_s(ncm.lfStatusFont.lfFaceName, L"la-solid-900");
            //wcscpy_s(ncm.lfStatusFont.lfFaceName, L"Segoe MDL2 Assets");
            m_fonts[4]                = CreateFontIndirect(&ncm.lfStatusFont);
            // calculate the height of the status bar from the font size
            RECT fr  = {0};
            auto hdc = GetDC(*this);
            DrawText(hdc, L"W", 1, &fr, DT_SINGLELINE | DT_CALCRECT);
            ReleaseDC(*this, hdc);
            m_height = fr.bottom - fr.top;
            m_height += CDPIAware::Instance().Scale(*this, 4);
            m_height = CDPIAware::Instance().Scale(*this, m_height);

            // create the tooltip window
            m_tooltip = CreateWindowEx(0, TOOLTIPS_CLASS, nullptr,
                                       WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT,
                                       *this, nullptr, hResource,
                                       nullptr);
            SendMessage(m_tooltip, TTM_SETMAXTIPWIDTH, 0, 600);
            return true;
        }
    }
    return false;
}

bool CRichStatusBar::SetPart(int index, const CRichStatusBarItem& item, bool redraw, bool replace)
{
    if (index >= static_cast<int>(m_parts.size()))
    {
        for (auto i = static_cast<int>(m_parts.size()); i <= index; ++i)
        {
            m_parts.push_back(std::move(CRichStatusBarItem()));
            m_partWidths.push_back({});
            m_animVars.push_back(Animator::Instance().CreateAnimationVariable(0.0, 0.0));
        }
    }
    if (index < 0)
    {
        m_parts.push_back(item);
        m_partWidths.push_back({});
        m_animVars.push_back(Animator::Instance().CreateAnimationVariable(0.0, 0.0));
        index = static_cast<int>(m_parts.size()) - 1;
    }
    else if (replace)
    {
        m_parts[index] = item;
    }
    else
    {
        m_parts.insert(m_parts.begin() + index - 1, item);
        m_partWidths.insert(m_partWidths.begin() + index - 1, {});
        m_animVars.insert(m_animVars.begin() + index - 1, Animator::Instance().CreateAnimationVariable(0.0, 1.0));
    }
    CalcRequestedWidths(index);

    if (redraw)
    {
        CalcWidths();
        UpdateWindow(*this);
    }

    return true;
}

bool CRichStatusBar::SetPart(int fontIdx, int index, const std::wstring& text, const std::wstring& /*shortText*/, const std::wstring& tooltip, int width, int type, int align, bool fixedWidth, bool hover, HICON icon, HICON collapsedIcon)
{
    CRichStatusBarItem part;
    part.text          = text;
    part.tooltip       = tooltip;
    part.width         = width;
    part.align         = align;
    part.fixedWidth    = fixedWidth;
    part.hoverActive   = hover;
    part.icon          = icon;
    part.collapsedIcon = collapsedIcon;
    part.fontIdx       = fontIdx;
    part.type          = type;

    return SetPart(index, part, false);
}

int CRichStatusBar::GetPartIndexAt(const POINT& pt) const
{
    RECT rect;
    ::GetClientRect(*this, &rect);
    int width = 0;
    for (size_t i = 0; i < m_partWidths.size(); ++i)
    {
        RECT rc  = rect;
        rc.left  = width;
        rc.right = rc.left + m_partWidths[i].calculatedWidth;
        if (PtInRect(&rc, pt))
        {
            return static_cast<int>(i);
        }
        width += m_partWidths[i].calculatedWidth;
    }
    return -1;
}

int CRichStatusBar::GetWidthToLeft(int idx)
{
    RECT rect;
    ::GetClientRect(*this, &rect);
    int width = 0;
    for (size_t i = 0; i < idx; ++i)
    {
        RECT rc  = rect;
        rc.left  = width;
        rc.right = rc.left + m_partWidths[i].calculatedWidth;
        width += m_partWidths[i].calculatedWidth;
    }

    return width;
}

//void CRichStatusBar::DrawSizeGrip(HDC hdc, LPCRECT lpRect)
//{
//    POINT pt = {0};
//    INT   i  = {0};
//
//    const auto oneDpi = CDPIAware::Instance().Scale(*this, 1);
//    pt.x              = lpRect->right - oneDpi;
//    pt.y              = lpRect->bottom - oneDpi;
//
//    HPEN hPenFace = CreatePen(PS_SOLID, 1, m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_3DFACE)) : GetSysColor(COLOR_3DFACE));
//    HPEN hOldPen  = static_cast<HPEN>(SelectObject(hdc, hPenFace));
//    MoveToEx(hdc, pt.x - CDPIAware::Instance().Scale(*this, 12), pt.y, nullptr);
//    LineTo(hdc, pt.x, pt.y);
//    LineTo(hdc, pt.x, pt.y - CDPIAware::Instance().Scale(*this, 13));
//
//    pt.x--;
//    pt.y--;
//
//    HPEN hPenShadow = CreatePen(PS_SOLID, 1, m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_3DSHADOW)) : GetSysColor(COLOR_3DSHADOW));
//    SelectObject(hdc, hPenShadow);
//    for (i = 1; i < CDPIAware::Instance().Scale(*this, 11); i += 4)
//    {
//        MoveToEx(hdc, pt.x - i, pt.y, nullptr);
//        LineTo(hdc, pt.x + 1, pt.y - i - 1);
//
//        MoveToEx(hdc, pt.x - i - 1, pt.y, nullptr);
//        LineTo(hdc, pt.x + 1, pt.y - i - 2);
//    }
//
//    HPEN hPenHighlight = CreatePen(PS_SOLID, 1, m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_3DHIGHLIGHT)) : GetSysColor(COLOR_3DHIGHLIGHT));
//    SelectObject(hdc, hPenHighlight);
//    for (i = 3; i < CDPIAware::Instance().Scale(*this, 13); i += 4)
//    {
//        MoveToEx(hdc, pt.x - i, pt.y, nullptr);
//        LineTo(hdc, pt.x + 1, pt.y - i - 1);
//    }
//
//    SelectObject(hdc, hOldPen);
//    DeleteObject(hPenFace);
//    DeleteObject(hPenShadow);
//    DeleteObject(hPenHighlight);
//}

LRESULT CRichStatusBar::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc = BeginPaint(hwnd, &ps);
            RECT        rect; // = ps.rcPaint;
            //rc.right += 200;
            //RECT        rcWin;
            GetClientRect(hwnd, &rect);
            //GetWindowRect(hwnd, &rc);
            THEME currentTheme = CTheme::CurrentTheme();
            auto hMyMemDC   = ::CreateCompatibleDC(hdc);
            auto hBitmap    = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
            auto hOldBitmap = static_cast<HBITMAP>(SelectObject(hMyMemDC, hBitmap));

            auto foreColor = currentTheme.winFore; // m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_WINDOWTEXT)) : GetSysColor(COLOR_WINDOWTEXT);
            auto backColor = currentTheme.winBack;//m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_3DFACE)) : GetSysColor(COLOR_3DFACE);
            auto hbr       = CreateSolidBrush(backColor);
            FillRect(hMyMemDC, &rect, hbr);
            DeleteObject(hbr);
            hbr = CreateSolidBrush(currentTheme.itemHover);
            RECT r(rect);
            r.bottom = r.top + 1;
            FillRect(hMyMemDC, &r, hbr);
            DeleteObject(hbr);

            //DrawEdge(hMyMemDC, &rect, 1, BF_TOP | BF_FLAT | BF_MONO);
            //if (m_drawGrip)
            //    DrawSizeGrip(hMyMemDC, &rect);

            SetTextColor(hMyMemDC, foreColor);
            SetBkColor(hMyMemDC, backColor);

            const auto oneDpi = CDPIAware::Instance().Scale(*this, 1);
            const auto twoDpi = CDPIAware::Instance().Scale(*this, 2);

            //COLORREF checkedColor = GDIHelpers::Darker(GetSysColor(COLOR_3DFACE), CTheme::Instance().IsDarkTheme() ? 0.4f : 0.8f);

            RECT     partRect     =  rect;
            int  right    = 0;
            for (decltype(m_parts.size()) i = 0; i < m_parts.size(); ++i)
            {
                partRect         = rect;
                const auto& part = m_parts[i];
                partRect.left    = right;
                partRect.right   = partRect.left + m_partWidths[i].calculatedWidth;
                right            = partRect.right;
                auto penColor    = currentTheme.selBack; // RGB(100, 100, 100) ;//m_themeColorFunc ? RGB(0x48, 0x48, 0x48) : GetSysColor(COLOR_GRAYTEXT);
                auto pen         = CreatePen(PS_SOLID, 1, penColor);
                auto oldPen      = SelectObject(hMyMemDC, pen);
                MoveToEx(hMyMemDC, partRect.left, partRect.top, nullptr);
                SelectObject(hMyMemDC, oldPen);
                DeleteObject(pen);
                InflateRect(&partRect, -oneDpi, -oneDpi);
                auto fraction    = Animator::GetValue(m_animVars[i]);
                auto animForeClr = RGB((GetRValue(penColor) - GetRValue(backColor)) * fraction + GetRValue(backColor),
                                       (GetGValue(penColor) - GetGValue(backColor)) * fraction + GetGValue(backColor),
                                       (GetBValue(penColor) - GetBValue(backColor)) * fraction + GetBValue(backColor));
                pen              = CreatePen(PS_SOLID, 1, animForeClr);
                oldPen           = SelectObject(hMyMemDC, pen);

                SelectObject(hMyMemDC, oldPen);
                DeleteObject(pen);
                InflateRect(&partRect, -oneDpi, -oneDpi);
                if (part.isChecked)
                {
                    RECT tempRect = partRect;
                    InflateRect(&tempRect, -twoDpi, -twoDpi);

                    tempRect.top -= 3;
                    tempRect.bottom += 4;
                    tempRect.left -= 3;
                    tempRect.right += 4;

                    //GDIHelpers::FillSolidRect(hMyMemDC, &tempRect, checkedColor);
                    //DrawEdge(hMyMemDC, &tempRect, i == m_hoverPart ? EDGE_ETCHED : BDR_SUNKENOUTER, BF_RECT | BF_MONO | BF_SOFT | BF_ADJUST);
                }
                
                int x = 0;
                if (part.type == 2/*part.icon && !m_partWidths[i].collapsed*/)
                {
                    RECT tempRect = partRect;
                    InflateRect(&tempRect, -twoDpi, -twoDpi);
                    auto cy = tempRect.bottom - tempRect.top;
                    DrawIconEx(hMyMemDC, tempRect.left + 4, tempRect.top+1, part.icon, cy, cy, 0, nullptr, DI_NORMAL);
                    x = twoDpi + cy;
                }
                partRect.left += x;
                RECT tempRect = partRect;
                if (part.type != 2/*!m_partWidths[i].collapsed || !part.collapsedIcon*/)
                {
                    InflateRect(&tempRect, -(border_width - twoDpi), 0);
                    auto text   = /* m_partWidths[i].shortened && !part.shortText.empty() ? part.shortText : */part.text;
                    UINT format = DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
                    if (part.align == 1)
                        format |= DT_CENTER;
                    if (part.align == 2)
                        format |= DT_RIGHT;
                    if (part.align == 0)
                        format |= DT_LEFT;

                    tempRect.left += 1;
                    //tempRect.bottom -= 2;
                    tempRect.top += 1;
                    SetBkColor(hMyMemDC,currentTheme.winBack);
                    DrawRichText(part.fontIdx,hMyMemDC, text, tempRect, format);
                }
            }

            // Copy the off screen bitmap onto the screen.
            BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hMyMemDC, rect.left, rect.top, SRCCOPY);
            //Swap back the original bitmap.
            SelectObject(hMyMemDC, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hMyMemDC);

            EndPaint(hwnd, &ps);
            break;
        }
        case WM_ERASEBKGND:
            break;
        case WM_SIZE:
            CalcWidths();
            break;
        //case WM_NCHITTEST:
        //{
        //    POINT pt;
        //    pt.x = GET_X_LPARAM(lParam);
        //    pt.y = GET_Y_LPARAM(lParam);
        //    ScreenToClient(*this, &pt);
        //    if (m_partWidths[0].calculatedWidth > pt.x)
        //        SendMessage(GetParent(*this), WM_NCLBUTTONDOWN, HTCAPTION, NULL);
        //    return 0;
        //}
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
        case WM_CONTEXTMENU:
        {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            if (uMsg == WM_CONTEXTMENU)
                ScreenToClient(*this, &pt);
            auto index = GetPartIndexAt(pt);
            
            if (index >= 0)
            {
                SendMessage(::GetParent(*this), WM_STATUSBAR_MSG, uMsg, index);
            }
        }
        break;
        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize          = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags         = TME_LEAVE;
            tme.hwndTrack       = *this;
            TrackMouseEvent(&tme);

            POINT pt;
            pt.x           = GET_X_LPARAM(lParam);
            pt.y           = GET_Y_LPARAM(lParam);
            auto oldHover  = m_hoverPart;
            bool oldActive = false;
            if (m_hoverPart >= 0)
                oldActive = m_parts[m_hoverPart].hoverActive;
            m_hoverPart = GetPartIndexAt(pt);
            if ((m_hoverPart != oldHover) || ((m_hoverPart >= 0) && m_parts[m_hoverPart].hoverActive != oldActive))
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if ((m_hoverPart >= 0) && !m_parts[m_hoverPart].hoverActive)
                m_hoverPart = -1;
            if (m_hoverPart != oldHover)
            {
                if ((m_hoverPart >= 0) && m_parts[m_hoverPart].hoverActive)
                {
                    auto transHot   = Animator::Instance().CreateSmoothStopTransition(m_animVars[m_hoverPart], 0.3, 1.0);
                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    if (storyBoard && transHot)
                    {
                        storyBoard->AddTransition(m_animVars[m_hoverPart].m_animVar, transHot);
                        Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                            InvalidateRect(*this, nullptr, false);
                        });
                    }
                    else
                        InvalidateRect(*this, nullptr, false);
                }
                if (oldHover >= 0)
                {
                    auto transHot   = Animator::Instance().CreateSmoothStopTransition(m_animVars[oldHover], 0.3, 0.0);
                    auto storyBoard = Animator::Instance().CreateStoryBoard();
                    if (storyBoard && transHot)
                    {
                        storyBoard->AddTransition(m_animVars[oldHover].m_animVar, transHot);
                        Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                            InvalidateRect(*this, nullptr, false);
                        });
                    }
                    else
                        InvalidateRect(*this, nullptr, false);
                }
            }
        }
        break;
        case WM_MOUSELEAVE:
        {
            TRACKMOUSEEVENT tme = {0};
            tme.cbSize          = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags         = TME_LEAVE | TME_CANCEL;
            tme.hwndTrack       = *this;
            TrackMouseEvent(&tme);
            if (m_hoverPart >= 0)
            {
                auto transHot   = Animator::Instance().CreateSmoothStopTransition(m_animVars[m_hoverPart], 0.3, 0.0);
                auto storyBoard = Animator::Instance().CreateStoryBoard();
                if (storyBoard && transHot)
                {
                    storyBoard->AddTransition(m_animVars[m_hoverPart].m_animVar, transHot);
                    Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                        InvalidateRect(*this, nullptr, false);
                    });
                }
                else
                    InvalidateRect(*this, nullptr, false);
            }
            m_hoverPart = -1;
        }
        break;
    }
    if (prevWndProc)
        return prevWndProc(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CRichStatusBar::CalcRequestedWidths(int index)
{
    auto hdc  = GetDC(*this);
    RECT rect = {0};
    GetClientRect(*this, &rect);
    auto& part = m_parts[index];

    PartWidths w;
    w.calculatedWidth = 0;
    //w.collapsed       = false;
    //w.canCollapse     = part.collapsedIcon != nullptr;
    //w.shortened       = false;
    w.fixed           = part.fixedWidth;

    //if (part.shortWidth > 0)
    //    w.shortWidth = CDPIAware::Instance().Scale(*this, part.shortWidth);
    //else
    //{
        RECT rc = rect;

        DrawRichText(part.fontIdx,hdc, part.text/*part.shortText.empty() ? part.text : part.shortText*/, rc, DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
        //w.shortWidth = rc.right - rc.left;
    //}
    if (part.width > 0)
        w.defaultWidth = CDPIAware::Instance().Scale(*this, part.width);
    else
    {
        //RECT rc = rect;
        DrawRichText(part.fontIdx,hdc, part.text, rc, DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
        w.defaultWidth = rc.right - rc.left;
    }
    if (part.icon)
    {
        if (part.text.empty())
        {
            w.defaultWidth += 12; //ICON_WIDTH;
            //w.shortWidth += 12;   //ICON_WIDTH;
        }
        else
        {
            w.defaultWidth = w.defaultWidth + ICON_WIDTH - 36;
            //w.shortWidth = w.shortWidth + ICON_WIDTH - 36;
        }
    }
    const auto twodpi = CDPIAware::Instance().Scale(*this, 2);
    //w.shortWidth += (twodpi * border_width);
    w.defaultWidth += (twodpi * border_width);
    // add padding
    if (part.width < 0)
        w.defaultWidth -= part.width;
    //if (part.shortWidth < 0)
    //    w.shortWidth -= part.shortWidth;
    m_partWidths[index] = w;
    ReleaseDC(*this, hdc);
}

std::wstring CRichStatusBar::GetPlainString(const std::wstring& text)
{
    std::wstring result;
    size_t       pos     = 0;
    auto         percPos = text.find('%', pos);
    while (percPos != std::wstring::npos)
    {
        if (percPos < text.size() - 1)
        {
            result += text.substr(pos, percPos - pos);
            pos = percPos + 1;
            switch (text[pos])
            {
                case '%':
                {
                    result += L"%";
                    ++pos;
                }
                break;
                case 'i': // italic
                case 'b': // bold
                case 'r': // reset
                    ++pos;
                    break;
                case 'c': // color
                {
                    if (percPos < text.size() - 7)
                    {
                        pos += 7;
                    }
                }
                break;
                default:
                    break;
            }
        }
        else
            break;
        percPos = text.find('%', pos);
    }
    result += text.substr(pos);
    return result;
}

void CRichStatusBar::DrawRichText(int fontIdx, HDC hdc, const std::wstring& text, RECT& rect, UINT flags)
{
    struct TextControls
    {
        int          xPos = 0;
        std::wstring text;
        COLORREF     color   = static_cast<COLORREF>(-1);
        HFONT        font    = nullptr;
        wchar_t      command = '\0';
    };

    std::list<HGDIOBJ> objStack;
    int                font    = fontIdx;//0;
    auto               oldFont = SelectObject(hdc, m_fonts[font]);

    SetTextColor(hdc, CTheme::CurrentTheme().winFore); // m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_WINDOWTEXT)) : GetSysColor(COLOR_WINDOWTEXT));

    SetBkMode(hdc, TRANSPARENT);

    size_t                   pos       = 0;
    auto                     percPos   = text.find('%', pos);
    int                      textWidth = 0;
    std::deque<TextControls> tokens;
    TextControls             textControls;
    while (percPos != std::wstring::npos)
    {
        if (percPos < text.size() - 1)
        {
            RECT tempRc       = rect;
            textControls.text = text.substr(pos, percPos - pos);
            DrawText(hdc, textControls.text.c_str(), -1, &tempRc, flags | DT_CALCRECT);
            textControls.xPos = textWidth;
            textWidth += (tempRc.right - tempRc.left);
            tokens.push_back(textControls);

            pos = percPos + 1;
            switch (text[pos])
            {
                case '%':
                {
                    textControls.command = '\0';
                    textControls.text    = L"%";
                    textControls.xPos    = textWidth;
                    tokens.push_back(textControls);
                    tempRc = rect;
                    DrawText(hdc, L"%", 1, &tempRc, flags | DT_CALCRECT);
                    textWidth += (tempRc.right - tempRc.left);
                    ++pos;
                }
                break;
                case 'i': // italic
                {
                    font |= 1;
                    textControls.font    = m_fonts[font];
                    textControls.command = text[pos];
                    objStack.push_front(SelectObject(hdc, m_fonts[font]));
                    ++pos;
                }
                break;
                case 'b': // bold
                {
                    font |= 2;
                    textControls.font    = m_fonts[font];
                    textControls.command = text[pos];
                    objStack.push_front(SelectObject(hdc, m_fonts[font]));
                    ++pos;
                }
                break;
                case 'c': // color
                {
                    if (percPos < text.size() - 7)
                    {
                        auto sColor = text.substr(percPos + 2, 6);
                        auto color  = wcstoul(sColor.c_str(), nullptr, 16);
                        color       = RGB(GetBValue(color), GetGValue(color), GetRValue(color));
                        //if (m_themeColorFunc)
                        //    color = m_themeColorFunc(color);
                        color = CTheme::Instance().GetThemeColor(color);
                        textControls.color   = color;
                        textControls.command = text[pos];
                        SetTextColor(hdc, color);
                        pos += 7;
                    }
                }
                break;
                case 'r': // reset
                {
                    font = 0;
                    for (auto& obj : objStack)
                        SelectObject(hdc, obj);
                    objStack.clear();
                    SetTextColor(hdc, CTheme::CurrentTheme().winFore); // m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_WINDOWTEXT)) : GetSysColor(COLOR_WINDOWTEXT));
                    //SetBkColor(hdc, m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_3DFACE)) : GetSysColor(COLOR_3DFACE));
                    textControls.font    = nullptr;
                    textControls.color   = static_cast<COLORREF>(-1);
                    textControls.command = text[pos];
                    ++pos;
                }
                break;
            }
        }
        else
            break;
        percPos = text.find('%', pos);
    }
    RECT tempRc       = rect;

    textControls.text = text.substr(pos);
    DrawText(hdc, textControls.text.c_str(), -1, &tempRc, flags | DT_CALCRECT);
    textControls.xPos = textWidth;
    textWidth += (tempRc.right - tempRc.left);
    tokens.push_back(textControls);

    for (auto& obj : objStack)
        SelectObject(hdc, obj);

    if (flags & DT_CALCRECT)
    {
        rect.right = textWidth;
    }
    else
    {
        SetTextColor(hdc, CTheme::CurrentTheme().winFore); // m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_WINDOWTEXT)) : GetSysColor(COLOR_WINDOWTEXT));
        //SetBkColor(hdc, m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_3DFACE)) : GetSysColor(COLOR_3DFACE));
        flags &= ~DT_CALCRECT;
        if (flags & DT_CENTER)
        {
            flags &= ~DT_CENTER;
            rect.left = rect.left + ((rect.right - rect.left) - textWidth) / 2;
        }
        if (flags & DT_RIGHT)
        {
            flags &= ~DT_RIGHT;
            rect.left = rect.right - textWidth;
        }

        for (auto& token : tokens)
        {
            switch (token.command)
            {
                case 'i':
                case 'b':
                    objStack.push_front(SelectObject(hdc, token.font));
                    break;
                case 'c':
                    SetTextColor(hdc, token.color);
                    break;
                case 'r':
                    for (auto& obj : objStack)
                        SelectObject(hdc, obj);
                    objStack.clear();
                    SetTextColor(hdc, CTheme::CurrentTheme().winFore); // m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_WINDOWTEXT)) : GetSysColor(COLOR_WINDOWTEXT));
                    //SetBkColor(hdc, m_themeColorFunc ? m_themeColorFunc(GetSysColor(COLOR_3DFACE)) : GetSysColor(COLOR_3DFACE));
                    break;
            }
            RECT tempRect = rect;
            tempRect.left += token.xPos;

            if (fontIdx == 4) 
            {
                tempRect.top += 2;
                tempRect.bottom -= 3;
                SelectObject(hdc, m_fonts[4]);
            }
    
            DrawText(hdc, token.text.c_str(), -1, &tempRect, flags);
        }
    }

    SelectObject(hdc, oldFont);
}

void CRichStatusBar::CalcWidths()
{
    if (m_partWidths.empty())
        return;

    for (auto& p : m_partWidths)
    {
        p.calculatedWidth = 0;
    }

    RECT rect;
    GetClientRect(*this, &rect);
    int  maxWidth  = rect.right - rect.left;
    int total    = 0;
    int nonFixed = 0;
    for (auto& p : m_partWidths)
    {
        if (p.calculatedWidth == 0)
            p.calculatedWidth = p.defaultWidth;
        total += p.calculatedWidth;
        if (!p.fixed)
            ++nonFixed;
    }
    if ((total < maxWidth) && nonFixed)
    {
        int         ext    = (maxWidth - total) / nonFixed;
        int         tWidth = 0;
        PartWidths* pPart  = nullptr;
        for (auto& p : m_partWidths)
        {
            if (!p.fixed)
            {
                p.calculatedWidth += ext;
                pPart = &p;
            }
            tWidth += p.calculatedWidth;
        }
        if (pPart)
            pPart->calculatedWidth += (maxWidth - tWidth);
    }

    // set the tooltips
    TOOLINFO ti = {sizeof(TOOLINFO)};
    ti.hinst    = hResource;
    ti.uFlags   = TTF_SUBCLASS;
    ti.hwnd     = *this;
    // first remove all tools
    for (decltype(m_parts.size()) i = 0; i < m_parts.size(); ++i)
    {
        ti.uId = i + 1;
        SendMessage(m_tooltip, TTM_DELTOOL, 0, reinterpret_cast<LPARAM>(&ti));
    }
    // now add all tools
    int startX        = 0;
    ti.rect.top       = rect.top;
    ti.rect.bottom    = rect.bottom;
    const auto twoDpi = CDPIAware::Instance().Scale(*this, 2);
    for (decltype(m_parts.size()) i = 0; i < m_parts.size(); ++i)
    {
        ti.uId        = i + 1;
        ti.rect.left  = startX;
        ti.rect.right = startX + m_partWidths[i].calculatedWidth;
        startX        = ti.rect.right;
        InflateRect(&ti.rect, -twoDpi, 0);
        ti.lpszText = const_cast<wchar_t*>(m_parts[i].tooltip.c_str());
        if (ti.lpszText[0])
        {
            SendMessage(m_tooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
            SendMessage(m_tooltip, TTM_ACTIVATE, TRUE, static_cast<LPARAM>(0));
        }
    }
    SetWindowPos(m_tooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    InvalidateRect(*this, nullptr, FALSE);
}
