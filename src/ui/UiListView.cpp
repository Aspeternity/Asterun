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
            index <
                count - 1) {
            HPEN guide =
                CreatePen(
                    PS_SOLID,
                    1,
                    RGB(188, 207, 226));
            HGDIOBJ oldPen =
                SelectObject(
                    dc,
                    guide);

            const int x =
                itemRect.right - 1;

            MoveToEx(
                dc,
                x,
                itemRect.top +
                    Scale(
                        8,
                        state.dpi),
                nullptr);
            LineTo(
                dc,
                x,
                itemRect.bottom -
                    Scale(
                        8,
                        state.dpi));

            SelectObject(
                dc,
                oldPen);
            DeleteObject(
                guide);
        }
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

[[nodiscard]] int
DividerAtPoint(
    HWND header,
    POINT point) {

    HDHITTESTINFO hit{};
    hit.pt = point;

    const int item =
        Header_HitTest(
            header,
            &hit);

    if (item < 0) {
        return -1;
    }

    if ((hit.flags &
         (HHT_ONDIVIDER |
          HHT_ONDIVOPEN)) == 0) {
        return -1;
    }

    return item;
}

LRESULT CALLBACK
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

    case WM_MOUSEMOVE:
        if (state) {
            POINT point{
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
            };

            const int divider =
                DividerAtPoint(
                    hwnd,
                    point);

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

    case WM_MOUSELEAVE:
        if (state &&
            state->hotDivider != -1) {
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
