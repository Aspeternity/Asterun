#include "UiListView.hpp"

#include "UiMetrics.hpp"
#include "UiTheme.hpp"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <array>

namespace altrun::ui {
namespace {

constexpr wchar_t
    kNextListStateProperty[] =
        L"ALTRunNext.UiListView.State";

constexpr UINT_PTR
    kNextListSubclassId =
        0x1A57;

constexpr UINT_PTR
    kNextHeaderSubclassId =
        0x1A58;

constexpr COLORREF
    kTableHairline =
        RGB(237, 240, 244);

struct NextListState {
    UINT dpi{96};
    HFONT bodyFont{};
    HFONT headerFont{};
    HIMAGELIST rowHeightImageList{};
    HWND header{};
    int hotItem{-1};
    int hotDivider{-1};
    int draggingDivider{-1};
    int resizeGuideX{-1};
};

[[nodiscard]] NextListState*
ListState(
    HWND list) noexcept {

    return reinterpret_cast<
        NextListState*>(
            GetPropW(
                list,
                kNextListStateProperty));
}

void InvalidateListItem(
    HWND list,
    int itemIndex) {

    if (!list ||
        itemIndex < 0) {
        return;
    }

    RECT rect{};

    if (ListView_GetItemRect(
            list,
            itemIndex,
            &rect,
            LVIR_BOUNDS)) {
        InvalidateRect(
            list,
            &rect,
            FALSE);
    }
}

void ReplaceRowHeightImageList(
    HWND list,
    NextListState& state) {

    if (state.rowHeightImageList) {
        ListView_SetImageList(
            list,
            nullptr,
            LVSIL_SMALL);
        ImageList_Destroy(
            state.rowHeightImageList);
        state.rowHeightImageList =
            nullptr;
    }

    const int rowHeight =
        Scale(30, state.dpi);

    state.rowHeightImageList =
        ImageList_Create(
            1,
            rowHeight,
            ILC_COLOR32,
            1,
            1);

    if (!state.rowHeightImageList) {
        return;
    }

    HBITMAP spacer =
        CreateBitmap(
            1,
            rowHeight,
            1,
            32,
            nullptr);

    if (spacer) {
        ImageList_Add(
            state.rowHeightImageList,
            spacer,
            nullptr);
        DeleteObject(
            spacer);
    }

    ListView_SetImageList(
        list,
        state.rowHeightImageList,
        LVSIL_SMALL);
}

void DrawListFrame(
    HWND list) {

    HDC dc =
        GetDC(list);

    if (!dc) {
        return;
    }

    RECT rect{};
    GetClientRect(
        list,
        &rect);

    if (rect.right >
            rect.left &&
        rect.bottom >
            rect.top) {
        rect.right -= 1;
        rect.bottom -= 1;

        HPEN pen =
            CreatePen(
                PS_SOLID,
                1,
                kApplicationPalette
                    .frame);

        HGDIOBJ oldPen =
            SelectObject(
                dc,
                pen);
        HGDIOBJ oldBrush =
            SelectObject(
                dc,
                GetStockObject(
                    NULL_BRUSH));

        const auto* state =
            ListState(list);
        const UINT dpi =
            state
                ? state->dpi
                : 96;

        RoundRect(
            dc,
            rect.left,
            rect.top,
            rect.right + 1,
            rect.bottom + 1,
            Scale(6, dpi),
            Scale(6, dpi));

        SelectObject(
            dc,
            oldBrush);
        SelectObject(
            dc,
            oldPen);
        DeleteObject(
            pen);
    }

    ReleaseDC(
        list,
        dc);
}

[[nodiscard]] int
HeaderDividerX(
    HWND header,
    int divider) {

    if (!header ||
        divider < 0) {
        return -1;
    }

    RECT itemRect{};

    if (!Header_GetItemRect(
            header,
            divider,
            &itemRect)) {
        return -1;
    }

    return itemRect.right;
}

[[nodiscard]] int
DividerNearPoint(
    HWND header,
    POINT point,
    UINT dpi) {

    if (!header) {
        return -1;
    }

    const int count =
        Header_GetItemCount(
            header);
    const int hitRadius =
        Scale(4, dpi);

    int bestDivider = -1;
    int bestDistance =
        hitRadius + 1;

    for (int divider = 0;
         divider < count - 1;
         ++divider) {
        const int x =
            HeaderDividerX(
                header,
                divider);

        if (x < 0) {
            continue;
        }

        const int distance =
            std::abs(
                point.x - x);

        if (distance <=
                hitRadius &&
            distance <
                bestDistance) {
            bestDivider =
                divider;
            bestDistance =
                distance;
        }
    }

    return bestDivider;
}

void InvalidateGuideStrip(
    HWND list,
    const NextListState& state,
    int guideX) {

    if (!list ||
        guideX < 0) {
        return;
    }

    const int halfWidth =
        std::max(
            2,
            Scale(2, state.dpi));

    if (state.header &&
        IsWindow(state.header)) {
        RECT headerRect{
            guideX - halfWidth,
            0,
            guideX + halfWidth + 1,
            0,
        };

        RECT headerClient{};
        GetClientRect(
            state.header,
            &headerClient);
        headerRect.bottom =
            headerClient.bottom;

        InvalidateRect(
            state.header,
            &headerRect,
            FALSE);
    }

    POINT origin{0, 0};

    if (state.header &&
        IsWindow(state.header)) {
        MapWindowPoints(
            state.header,
            list,
            &origin,
            1);
    }

    RECT listClient{};
    GetClientRect(
        list,
        &listClient);

    RECT strip{
        origin.x +
            guideX -
            halfWidth,
        0,
        origin.x +
            guideX +
            halfWidth + 1,
        listClient.bottom,
    };

    InvalidateRect(
        list,
        &strip,
        FALSE);
}

void SetResizeGuide(
    HWND list,
    NextListState& state,
    int divider,
    int guideX) {

    const int oldGuide =
        state.resizeGuideX;

    state.draggingDivider =
        divider;
    state.resizeGuideX =
        guideX;

    InvalidateGuideStrip(
        list,
        state,
        oldGuide);
    InvalidateGuideStrip(
        list,
        state,
        guideX);
}

void DrawResizeGuideOnList(
    HWND list,
    HDC dc,
    const NextListState& state) {

    if (!list ||
        !dc ||
        state.resizeGuideX < 0 ||
        !state.header ||
        !IsWindow(state.header)) {
        return;
    }

    POINT origin{0, 0};
    MapWindowPoints(
        state.header,
        list,
        &origin,
        1);

    RECT headerClient{};
    GetClientRect(
        state.header,
        &headerClient);

    RECT listClient{};
    GetClientRect(
        list,
        &listClient);

    const int x =
        origin.x +
        state.resizeGuideX;
    const int top =
        origin.y +
        headerClient.bottom;

    HPEN guide =
        CreatePen(
            PS_SOLID,
            std::max(
                1,
                Scale(2, state.dpi)),
            RGB(155, 190, 222));
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            guide);

    MoveToEx(
        dc,
        x,
        top,
        nullptr);
    LineTo(
        dc,
        x,
        listClient.bottom);

    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        guide);
}

void DrawHeaderSurface(
    HWND header,
    HDC dc,
    const NextListState& state) {

    RECT client{};
    GetClientRect(
        header,
        &client);

    HBRUSH background =
        CreateSolidBrush(
            kApplicationPalette
                .cardBackground);
    FillRect(
        dc,
        &client,
        background);
    DeleteObject(
        background);

    SetBkMode(
        dc,
        TRANSPARENT);
    SetTextColor(
        dc,
        kApplicationPalette.text);

    HGDIOBJ oldFont =
        nullptr;

    if (state.headerFont) {
        oldFont =
            SelectObject(
                dc,
                state.headerFont);
    }

    const int count =
        Header_GetItemCount(
            header);
    const int padding =
        Scale(
            12,
            state.dpi);

    for (int index = 0;
         index < count;
         ++index) {
        RECT itemRect{};

        if (!Header_GetItemRect(
                header,
                index,
                &itemRect)) {
            continue;
        }

        std::array<wchar_t, 256>
            text{};

        HDITEMW item{};
        item.mask = HDI_TEXT;
        item.pszText =
            text.data();
        item.cchTextMax =
            static_cast<int>(
                text.size());

        if (!Header_GetItem(
                header,
                index,
                &item)) {
            continue;
        }

        RECT textRect =
            itemRect;
        textRect.left += padding;
        textRect.right -=
            padding;

        DrawTextW(
            dc,
            text.data(),
            -1,
            &textRect,
            DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS |
                DT_NOPREFIX);

        if (state.hotDivider ==
                index &&
            state.draggingDivider < 0 &&
            index <
                count - 1) {
            HPEN guide =
                CreatePen(
                    PS_SOLID,
                    std::max(
                        1,
                        Scale(2, state.dpi)),
                    RGB(171, 199, 224));
            HGDIOBJ oldPen =
                SelectObject(
                    dc,
                    guide);

            const int x =
                itemRect.right;

            MoveToEx(
                dc,
                x,
                itemRect.top +
                    Scale(
                        6,
                        state.dpi),
                nullptr);
            LineTo(
                dc,
                x,
                itemRect.bottom -
                    Scale(
                        6,
                        state.dpi));

            SelectObject(
                dc,
                oldPen);
            DeleteObject(
                guide);
        }
    }

    if (state.resizeGuideX >= 0) {
        HPEN guide =
            CreatePen(
                PS_SOLID,
                std::max(
                    1,
                    Scale(2, state.dpi)),
                RGB(155, 190, 222));
        HGDIOBJ oldGuidePen =
            SelectObject(
                dc,
                guide);

        MoveToEx(
            dc,
            state.resizeGuideX,
            client.top +
                Scale(4, state.dpi),
            nullptr);
        LineTo(
            dc,
            state.resizeGuideX,
            client.bottom -
                Scale(1, state.dpi));

        SelectObject(
            dc,
            oldGuidePen);
        DeleteObject(
            guide);
    }

    if (oldFont) {
        SelectObject(
            dc,
            oldFont);
    }

    HPEN bottom =
        CreatePen(
            PS_SOLID,
            1,
            kTableHairline);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            bottom);

    MoveToEx(
        dc,
        client.left,
        client.bottom - 1,
        nullptr);
    LineTo(
        dc,
        client.right,
        client.bottom - 1);

    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        bottom);
}

LRESULT CALLBACK
NextHeaderSubclassProc(LRESULT CALLBACK
NextHeaderSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR refData) {

    auto* state =
        reinterpret_cast<
            NextListState*>(
                refData);

    switch (message) {
    case HDM_LAYOUT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (state &&
            lParam) {
            auto* layout =
                reinterpret_cast<
                    HDLAYOUT*>(
                        lParam);

            if (layout->pwpos &&
                layout->prc) {
                const int height =
                    Scale(
                        34,
                        state->dpi);

                layout->pwpos->cy =
                    height;
                layout->prc->top =
                    layout->pwpos->y +
                    height;
            }
        }

        return result;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc =
            BeginPaint(
                hwnd,
                &paint);

        if (state) {
            DrawHeaderSurface(
                hwnd,
                dc,
                *state);
        }

        EndPaint(
            hwnd,
            &paint);
        return 0;
    }

    case WM_PRINTCLIENT:
        if (state) {
            DrawHeaderSurface(
                hwnd,
                reinterpret_cast<HDC>(
                    wParam),
                *state);
            return 0;
        }
        break;

    case WM_SETCURSOR:
        if (state) {
            POINT point{};

            if (GetCursorPos(
                    &point) &&
                ScreenToClient(
                    hwnd,
                    &point) &&
                DividerNearPoint(
                    hwnd,
                    point,
                    state->dpi) >= 0) {
                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_MOUSEMOVE:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            const int divider =
                state->draggingDivider >= 0
                    ? state->draggingDivider
                    : DividerNearPoint(
                          hwnd,
                          point,
                          state->dpi);

            if (divider !=
                state->hotDivider) {
                state->hotDivider =
                    divider;
                InvalidateRect(
                    hwnd,
                    nullptr,
                    FALSE);
            }

            TRACKMOUSEEVENT track{
                sizeof(track),
                TME_LEAVE,
                hwnd,
                0,
            };
            TrackMouseEvent(
                &track);
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            const int divider =
                DividerNearPoint(
                    hwnd,
                    point,
                    state->dpi);

            if (divider >= 0) {
                const int dividerX =
                    HeaderDividerX(
                        hwnd,
                        divider);

                state->hotDivider =
                    divider;

                if (message ==
                        WM_LBUTTONDOWN) {
                    state->draggingDivider =
                        divider;

                    SetResizeGuide(
                        GetParent(hwnd),
                        *state,
                        divider,
                        dividerX);
                }

                SetCursor(
                    LoadCursorW(
                        nullptr,
                        IDC_SIZEWE));

                const LPARAM adjusted =
                    MAKELPARAM(
                        std::max(
                            0,
                            dividerX - 1),
                        point.y);

                return DefSubclassProc(
                    hwnd,
                    message,
                    wParam,
                    adjusted);
            }
        }
        break;

    case WM_LBUTTONUP: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (state &&
            state->draggingDivider >= 0) {
            ClearNextListResizeGuide(
                GetParent(hwnd));
        }

        return result;
    }

    case WM_CAPTURECHANGED:
        if (state &&
            state->draggingDivider >= 0) {
            ClearNextListResizeGuide(
                GetParent(hwnd));
        }
        break;

    case WM_MOUSELEAVE:
        if (state &&
            state->hotDivider != -1 &&
            state->draggingDivider < 0) {
            state->hotDivider = -1;
            InvalidateRect(
                hwnd,
                nullptr,
                FALSE);
        }
        break;

    case WM_SETFONT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        if (state) {
            state->headerFont =
                reinterpret_cast<HFONT>(
                    wParam);
        }

        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return result;
    }

    case WM_NCDESTROY:
        if (state &&
            state->header == hwnd) {
            state->header = nullptr;
        }

        RemoveWindowSubclass(
            hwnd,
            NextHeaderSubclassProc,
            subclassId);
        break;

    default:
        break;
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

LRESULT CALLBACK
NextListSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR refData) {

    auto* state =
        reinterpret_cast<
            NextListState*>(
                refData);

    switch (message) {
    case WM_MOUSEMOVE:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            LVHITTESTINFO hit{};
            hit.pt = point;

            const int hotItem =
                ListView_HitTest(
                    hwnd,
                    &hit);

            if (hotItem !=
                state->hotItem) {
                const int oldHot =
                    state->hotItem;
                state->hotItem =
                    hotItem;

                InvalidateListItem(
                    hwnd,
                    oldHot);
                InvalidateListItem(
                    hwnd,
                    hotItem);
            }

            TRACKMOUSEEVENT track{
                sizeof(track),
                TME_LEAVE,
                hwnd,
                0,
            };
            TrackMouseEvent(
                &track);
        }
        break;

    case WM_MOUSELEAVE:
        if (state &&
            state->hotItem >= 0) {
            const int oldHot =
                state->hotItem;
            state->hotItem = -1;

            InvalidateListItem(
                hwnd,
                oldHot);
        }
        break;

    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        break;

    case WM_PAINT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        DrawListFrame(
            hwnd);

        if (state) {
            HDC dc =
                GetDC(hwnd);

            if (dc) {
                DrawResizeGuideOnList(
                    hwnd,
                    dc,
                    *state);
                ReleaseDC(
                    hwnd,
                    dc);
            }
        }

        return result;
    }

    case WM_PRINTCLIENT: {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wParam,
                lParam);

        DrawListFrame(
            hwnd);
        return result;
    }

    case WM_NCDESTROY:
        if (state) {
            if (state->header &&
                IsWindow(
                    state->header)) {
                RemoveWindowSubclass(
                    state->header,
                    NextHeaderSubclassProc,
                    kNextHeaderSubclassId);
                state->header =
                    nullptr;
            }

            if (state->
                    rowHeightImageList) {
                ListView_SetImageList(
                    hwnd,
                    nullptr,
                    LVSIL_SMALL);
                ImageList_Destroy(
                    state->
                        rowHeightImageList);
                state->
                    rowHeightImageList =
                        nullptr;
            }

            RemovePropW(
                hwnd,
                kNextListStateProperty);
            RemoveWindowSubclass(
                hwnd,
                NextListSubclassProc,
                subclassId);
            delete state;
        }
        break;

    default:
        break;
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace

void InitializeNextListView(
    HWND list,
    UINT dpi,
    HFONT bodyFont,
    HFONT headerFont) {

    if (!list) {
        return;
    }

    auto* state =
        ListState(list);

    if (!state) {
        state =
            new NextListState();

        if (!SetPropW(
                list,
                kNextListStateProperty,
                reinterpret_cast<HANDLE>(
                    state))) {
            delete state;
            return;
        }

        SetWindowSubclass(
            list,
            NextListSubclassProc,
            kNextListSubclassId,
            reinterpret_cast<DWORD_PTR>(
                state));
    }

    state->dpi =
        dpi != 0
            ? dpi
            : 96;
    state->bodyFont =
        bodyFont;
    state->headerFont =
        headerFont
            ? headerFont
            : bodyFont;

    const DWORD extended =
        static_cast<DWORD>(
            ListView_GetExtendedListViewStyle(
                list));

    ListView_SetExtendedListViewStyle(
        list,
        extended |
            LVS_EX_FULLROWSELECT |
            LVS_EX_DOUBLEBUFFER);

    SetWindowTheme(
        list,
        L"Explorer",
        nullptr);

    HWND header =
        ListView_GetHeader(
            list);

    if (header) {
        if (state->header &&
            state->header != header &&
            IsWindow(
                state->header)) {
            RemoveWindowSubclass(
                state->header,
                NextHeaderSubclassProc,
                kNextHeaderSubclassId);
        }

        state->header =
            header;

        LONG_PTR headerStyle =
            GetWindowLongPtrW(
                header,
                GWL_STYLE);
        headerStyle &=
            ~static_cast<LONG_PTR>(
                HDS_FULLDRAG);
        SetWindowLongPtrW(
            header,
            GWL_STYLE,
            headerStyle);

        // The Header remains the native hit-testing/resizing engine, but its
        // visible surface is fully owned by Next. No classic Header borders or
        // permanent column grid lines are painted.
        SetWindowTheme(
            header,
            L"",
            L"");

        SetWindowSubclass(
            header,
            NextHeaderSubclassProc,
            kNextHeaderSubclassId,
            reinterpret_cast<DWORD_PTR>(
                state));
    }

    ListView_SetBkColor(
        list,
        kApplicationPalette
            .controlBackground);
    ListView_SetTextBkColor(
        list,
        kApplicationPalette
            .controlBackground);
    ListView_SetTextColor(
        list,
        kApplicationPalette.text);

    if (bodyFont) {
        SendMessageW(
            list,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                bodyFont),
            TRUE);
    }

    if (header &&
        state->headerFont) {
        SendMessageW(
            header,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                state->headerFont),
            TRUE);
    }

    ReplaceRowHeightImageList(
        list,
        *state);

    if (header) {
        InvalidateRect(
            header,
            nullptr,
            FALSE);
    }

    RedrawWindow(
        list,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN);
}

void UpdateNextListResizeGuide(
    HWND list,
    int column,
    int proposedWidth) {

    auto* state =
        ListState(list);

    if (!state ||
        !state->header ||
        column < 0 ||
        proposedWidth < 0) {
        return;
    }

    int x = 0;

    for (int index = 0;
         index < column;
         ++index) {
        x +=
            ListView_GetColumnWidth(
                list,
                index);
    }

    x += proposedWidth;

    state->hotDivider =
        column;

    SetResizeGuide(
        list,
        *state,
        column,
        x);
}

void ClearNextListResizeGuide(
    HWND list) {

    auto* state =
        ListState(list);

    if (!state) {
        return;
    }

    const int oldGuide =
        state->resizeGuideX;

    state->resizeGuideX = -1;
    state->draggingDivider = -1;

    InvalidateGuideStrip(
        list,
        *state,
        oldGuide);

    if (state->header &&
        IsWindow(state->header)) {
        InvalidateRect(
            state->header,
            nullptr,
            FALSE);
    }
}

COLORREF NextListRowBackground(
    HWND list,
    int itemIndex,
    bool selected) {

    const auto& palette =
        kApplicationPalette;

    if (selected) {
        return palette.selectionBackground;
    }

    const auto* state =
        ListState(list);

    if (state &&
        state->hotItem ==
            itemIndex) {
        return palette.accentBackground;
    }

    return palette.controlBackground;
}

COLORREF NextListRowText(
    bool selected) noexcept {

    return selected
        ? kApplicationPalette
              .selectionText
        : kApplicationPalette.text;
}

int NextListCellPadding(
    UINT dpi) noexcept {

    return Scale(
        12,
        dpi);
}

void DrawNextListRowSeparator(
    HDC dc,
    const RECT& row) {

    if (!dc) {
        return;
    }

    HPEN separator =
        CreatePen(
            PS_SOLID,
            1,
            kTableHairline);
    HGDIOBJ oldPen =
        SelectObject(
            dc,
            separator);

    MoveToEx(
        dc,
        row.left,
        row.bottom - 1,
        nullptr);
    LineTo(
        dc,
        row.right,
        row.bottom - 1);

    SelectObject(
        dc,
        oldPen);
    DeleteObject(
        separator);
}

} // namespace altrun::ui
